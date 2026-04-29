/**
 * @file cloud_asr_engine.h
 * @brief 云端 ASR 引擎接口
 *
 * 支持两个云端 ASR 服务：
 *   1. 智谱 GLM-ASR  (BigModel / zhipuai.cn)
 *      - 接口: POST https://open.bigmodel.cn/api/paas/v4/audio/transcriptions
 *      - 鉴权: Authorization: Bearer {api_key}
 *      - 格式: multipart/form-data，字段 file + model
 *
 *   2. 火山引擎豆包 ASR (volcengine.com)
 *      - 接口: POST https://openspeech.bytedance.com/api/v1/asr
 *      - 鉴权: Authorization: Bearer {api_key} + appid header
 *      - 格式: application/json，PCM base64 编码
 *
 * 降级逻辑：
 *   端侧 OnnxAsrEngine 失败时，自动路由到 CloudAsrEngine。
 *   通过 HybridAsrEngine 组合两者。
 *
 * 依赖：
 *   - 优先使用 libcurl（跨平台）
 *   - Windows 可选退化为 WinHTTP（无需额外依赖）
 *   - 由 CMake 宏 SYNTHORBIS_AI_HAS_CURL 控制
 */

#pragma once

#include "synthorbis/ai/asr_engine.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

namespace synthorbis {
namespace ai {

// ============================================================
// 云端 ASR 服务提供商枚举
// ============================================================

enum class CloudProvider {
    ZhipuGLM,        ///< 智谱 GLM-ASR (bigmodel.cn)
    VolcengineDouBao, ///< 火山引擎豆包 ASR (volcengine.com)
    Custom           ///< 自定义 endpoint（通用 OpenAI-compatible）
};

// ============================================================
// 云端 ASR 配置
// ============================================================

struct CloudAsrConfig {
    CloudProvider provider = CloudProvider::ZhipuGLM;

    // ---- 通用认证 ----
    std::string api_key;           ///< API Key
    std::string app_id;            ///< App ID（豆包必填）
    std::string endpoint;          ///< 自定义 endpoint（Custom 模式必填，其他可覆盖默认）

    // ---- 请求参数 ----
    std::string model;             ///< 模型名称（智谱: "glm-4-voice-flash"; 豆包: "bigmodel"）
    std::string language;          ///< 语言（"zh"/"en"/"auto"，默认 "zh"）
    int         timeout_ms = 10000;///< HTTP 请求超时（毫秒），默认 10 秒
    int         max_retries = 2;   ///< 最大重试次数

    // ---- 音频参数（豆包需要显式指定）----
    int         sample_rate = 16000;  ///< 采样率
    int         bits        = 16;     ///< 位深（8/16）
    int         channels    = 1;      ///< 声道数

    // ---- 代理 ----
    std::string proxy;             ///< HTTP 代理（如 "http://127.0.0.1:7890"）

    // ---- 降级控制 ----
    bool        enable_fallback = true;  ///< 请求失败时是否允许调用端重试/降级
};

// ============================================================
// HTTP 响应（内部用，也暴露给测试）
// ============================================================

struct HttpResponse {
    int         status_code = 0;    ///< HTTP 状态码
    std::string body;               ///< 响应体（JSON 字符串）
    std::string error_msg;          ///< curl/系统错误信息
    double      elapsed_ms = 0.0;   ///< 请求耗时（毫秒）
};

// ============================================================
// HTTP 客户端抽象（便于 Mock 测试）
// ============================================================

/**
 * @brief HTTP POST 客户端接口
 *
 * 生产实现 = CurlHttpClient / WinHttpClient
 * 测试实现 = MockHttpClient
 */
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /**
     * @brief 发送 multipart/form-data POST 请求
     * @param url         请求 URL
     * @param headers     HTTP Header 列表（"Key: Value" 格式）
     * @param fields      form 字段（key, value 对）
     * @param file_field  文件字段名（如 "file"）
     * @param file_data   文件内容（raw bytes）
     * @param filename    文件名（如 "audio.wav"）
     * @param timeout_ms  超时毫秒
     */
    virtual HttpResponse post_multipart(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::vector<std::pair<std::string, std::string>>& fields,
        const std::string& file_field,
        const std::vector<uint8_t>& file_data,
        const std::string& filename,
        int timeout_ms) = 0;

    /**
     * @brief 发送 application/json POST 请求
     * @param url        请求 URL
     * @param headers    HTTP Header 列表
     * @param json_body  JSON 字符串
     * @param timeout_ms 超时毫秒
     */
    virtual HttpResponse post_json(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& json_body,
        int timeout_ms) = 0;
};

// ============================================================
// 云端 ASR 引擎
// ============================================================

/**
 * @brief 云端 ASR 引擎
 *
 * 实现 IAsrEngine 接口，通过 HTTP REST 调用智谱/豆包 ASR 服务。
 *
 * 用法：
 * @code
 *   CloudAsrConfig cfg;
 *   cfg.provider = CloudProvider::ZhipuGLM;
 *   cfg.api_key  = "your_api_key";
 *   cfg.language = "zh";
 *
 *   AsrConfig base_cfg;
 *   base_cfg.type         = AsrEngineType::Cloud;
 *   base_cfg.api_key      = cfg.api_key;
 *   base_cfg.api_endpoint = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions";
 *
 *   auto engine = std::make_unique<CloudAsrEngine>();
 *   engine->initialize(base_cfg);
 *   engine->set_cloud_config(cfg);
 *
 *   // 识别
 *   AudioData audio{pcm_data.data(), 16000, static_cast<int>(pcm_data.size())};
 *   auto result = engine->recognize(audio);
 *   printf("Text: %s\n", result.text.c_str());
 * @endcode
 */
class CloudAsrEngine : public IAsrEngine {
public:
    CloudAsrEngine();
    explicit CloudAsrEngine(std::shared_ptr<IHttpClient> http_client);
    ~CloudAsrEngine() override;

    // ---- IAsrEngine 接口 ----
    int initialize(const AsrConfig& config) override;
    AsrResult recognize(const AudioData& audio) override;
    std::vector<AsrResult> recognize_batch(
        const std::vector<AudioData>& audios) override;
    AsrEngineType get_type() const override { return AsrEngineType::Cloud; }
    std::string   get_name() const override;

    // ---- 云端专属配置 ----

    /** @brief 设置云端详细配置（在 initialize() 之后调用） */
    void set_cloud_config(const CloudAsrConfig& cfg);

    /** @brief 获取当前云端配置 */
    const CloudAsrConfig& cloud_config() const { return cloud_cfg_; }

    /** @brief 注入自定义 HTTP 客户端（用于测试 / 企业代理） */
    void set_http_client(std::shared_ptr<IHttpClient> client);

    /** @brief 获取累计请求次数（线程安全） */
    int request_count() const { return request_count_.load(); }

    /** @brief 获取累计失败次数 */
    int failure_count() const { return failure_count_.load(); }

    /** @brief 重置统计计数器 */
    void reset_stats();

private:
    // ---- 智谱 GLM-ASR 实现 ----
    /**
     * @brief 调用智谱 GLM-ASR API
     * @param audio  音频数据（float PCM）
     * @return HTTP 响应（JSON body 含 text 字段）
     */
    HttpResponse call_zhipu(const AudioData& audio);

    /**
     * @brief 解析智谱响应 JSON
     * @param json_body  如: {"text":"...","duration":1.23}
     * @return 识别文本（失败返回空字符串）
     */
    std::string parse_zhipu_response(const std::string& json_body);

    // ---- 豆包 ASR 实现 ----
    /**
     * @brief 调用豆包 ASR API
     * @param audio  音频数据（float PCM）
     * @return HTTP 响应
     */
    HttpResponse call_doubao(const AudioData& audio);

    /**
     * @brief 解析豆包响应 JSON
     * @param json_body  如: {"code":0,"result":{"text":"..."}}
     */
    std::string parse_doubao_response(const std::string& json_body);

    // ---- 通用工具 ----
    /**
     * @brief float PCM → 16-bit WAV 字节（含 WAV header）
     * @param audio 音频数据
     * @return WAV 文件字节流（可直接用于 multipart 上传）
     */
    static std::vector<uint8_t> pcm_to_wav(const AudioData& audio);

    /**
     * @brief float PCM → int16 raw bytes（豆包 JSON 模式）
     */
    static std::vector<uint8_t> pcm_to_raw_int16(const AudioData& audio);

    /**
     * @brief base64 编码
     */
    static std::string base64_encode(const std::vector<uint8_t>& data);

    /**
     * @brief 极简 JSON 字段提取（key: "value"）
     * 不引入外部 JSON 库，只解析简单字符串字段
     */
    static std::string extract_json_string(const std::string& json,
                                           const std::string& key);

    /**
     * @brief 带重试的请求包装
     */
    AsrResult recognize_with_retry(const AudioData& audio);

    AsrConfig       base_cfg_;
    CloudAsrConfig  cloud_cfg_;
    bool            initialized_ = false;

    std::shared_ptr<IHttpClient> http_client_;

    // 统计（原子，线程安全）
    std::atomic<int> request_count_{0};
    std::atomic<int> failure_count_{0};
};

// ============================================================
// 混合引擎：端侧优先，云端降级
// ============================================================

/**
 * @brief 混合 ASR 引擎（端侧 + 云端自动降级）
 *
 * 策略：
 *   1. 优先调用 local_engine（OnnxAsrEngine）
 *   2. 若 local 返回空文本 / 置信度 < threshold / 耗时超时，
 *      自动 fallback 到 cloud_engine（CloudAsrEngine）
 *   3. 两者均失败则返回空结果
 *
 * 用法：
 * @code
 *   auto local  = create_asr_engine(AsrEngineType::Local);
 *   auto cloud  = create_asr_engine(AsrEngineType::Cloud);
 *   local->initialize(local_cfg);
 *   cloud->initialize(cloud_cfg);
 *
 *   HybridAsrEngine hybrid(std::move(local), std::move(cloud));
 *   hybrid.set_fallback_threshold(0.5f);  // 置信度低于 0.5 触发降级
 *
 *   auto result = hybrid.recognize(audio);
 * @endcode
 */
class HybridAsrEngine : public IAsrEngine {
public:
    HybridAsrEngine() = default;
    HybridAsrEngine(std::unique_ptr<IAsrEngine> local,
                    std::unique_ptr<IAsrEngine> cloud);
    ~HybridAsrEngine() override = default;

    // ---- IAsrEngine 接口 ----
    int initialize(const AsrConfig& config) override;
    AsrResult recognize(const AudioData& audio) override;
    std::vector<AsrResult> recognize_batch(
        const std::vector<AudioData>& audios) override;
    AsrEngineType get_type() const override { return AsrEngineType::Local; }
    std::string   get_name() const override { return "HybridAsrEngine"; }

    // ---- 降级策略配置 ----

    /** @brief 置信度阈值，低于此值触发云端降级（默认 0.4） */
    void set_fallback_threshold(float threshold) { fallback_threshold_ = threshold; }

    /** @brief 最大端侧耗时（秒），超过则降级（默认 5.0s） */
    void set_local_timeout(double timeout_sec) { local_timeout_ = timeout_sec; }

    /** @brief 设置端侧引擎 */
    void set_local_engine(std::unique_ptr<IAsrEngine> engine);

    /** @brief 设置云端引擎 */
    void set_cloud_engine(std::unique_ptr<IAsrEngine> engine);

    /** @brief 是否有端侧引擎 */
    bool has_local() const { return local_engine_ != nullptr; }

    /** @brief 是否有云端引擎 */
    bool has_cloud() const { return cloud_engine_ != nullptr; }

private:
    /** @brief 判断端侧结果是否需要降级 */
    bool should_fallback(const AsrResult& local_result) const;

    std::unique_ptr<IAsrEngine> local_engine_;
    std::unique_ptr<IAsrEngine> cloud_engine_;

    float  fallback_threshold_ = 0.4f;  ///< 置信度低于此值降级
    double local_timeout_      = 5.0;   ///< 端侧超时阈值（秒）
};

} // namespace ai
} // namespace synthorbis
