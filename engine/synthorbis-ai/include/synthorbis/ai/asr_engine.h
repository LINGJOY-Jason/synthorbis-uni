/**
 * @file asr_engine.h
 * @brief ASR Engine 统一接口
 */

#pragma once

#include <string>
#include <vector>
#include <memory>

namespace synthorbis {
namespace ai {

/**
 * @brief 音频数据格式
 */
struct AudioData {
    const float* data;     // 音频采样点
    int sample_rate;       // 采样率 (默认 16000)
    int samples;           // 采样点数量
};

/**
 * @brief ASR 识别结果
 */
struct AsrResult {
    std::string text;      // 识别文本
    float confidence;      // 置信度 0.0-1.0
    double process_time;   // 处理耗时 (秒)
};

/**
 * @brief ASR 引擎类型
 */
enum class AsrEngineType {
    Local,                 // 本地 ONNX 模型
    Cloud                  // 云端 API
};

/**
 * @brief ONNX 模型精度类型
 *
 * 对应量化工具产出的三种模型文件：
 *   - Float32 : 原始全精度模型（如 model.onnx）
 *   - Float16 : FP16 转换模型（如 model_fp16.onnx），约减小 50%
 *   - Int8Dynamic : INT8 动态量化（如 model_int8_dynamic.onnx），约减小 75%
 *   - Int8Static  : INT8 静态量化（如 model_int8_static.onnx），精度最优
 *
 * 在 AsrConfig::precision 中设置后，OnnxAsrEngine 会自动调整
 * SessionOptions 和输入张量精度。
 */
enum class ModelPrecision {
    Float32     = 0,    ///< 全精度 FP32（默认）
    Float16     = 1,    ///< FP16（权重 + 激活）
    Int8Dynamic = 2,    ///< INT8 动态量化
    Int8Static  = 3,    ///< INT8 静态量化（与动态量化加载方式相同，区别在模型文件）
    Auto        = 4,    ///< 自动检测：根据 model_path 后缀推断
};

/**
 * @brief ASR 引擎配置
 */
struct AsrConfig {
    AsrEngineType type = AsrEngineType::Local;

    // ---- 本地模型配置 ----
    std::string model_path;         ///< ONNX 模型路径
    std::string tokens_path;        ///< 词表文件路径（token id → 字符/子词）
    int num_threads = 4;            ///< 推理 CPU 线程数

    // ---- 量化/精度配置 ----
    ModelPrecision precision = ModelPrecision::Auto;
                                    ///< 模型精度类型；Auto 时根据文件名后缀自动推断
    bool enable_memory_pattern = true;
                                    ///< ORT: 开启内存模式复用（减少推理内存分配，建议开启）
    bool enable_cpu_mem_arena  = true;
                                    ///< ORT: 开启 CPU 内存池（提升吞吐量）
    int  graph_opt_level = 99;      ///< ORT graph 优化级别：0=关闭 1=基础 2=扩展 99=全部

    // ---- 云端配置 ----
    std::string api_endpoint;       ///< 云端 API 地址
    std::string api_key;            ///< 云端 API 密钥

    // ---- 语言/文本 ----
    int  language           = 0;    ///< 语言代码（0=自动检测；SenseVoice: 0=zh 1=en 2=ja）
    bool text_normalization = true; ///< 文本归一化（标点/数字规范化）
};

/**
 * @brief ASR 引擎接口
 */
class IAsrEngine {
public:
    virtual ~IAsrEngine() = default;
    
    /**
     * @brief 初始化引擎
     * @param config 引擎配置
     * @return 0 成功，<0 失败
     */
    virtual int initialize(const AsrConfig& config) = 0;
    
    /**
     * @brief 执行语音识别
     * @param audio 音频数据
     * @return 识别结果
     */
    virtual AsrResult recognize(const AudioData& audio) = 0;
    
    /**
     * @brief 批量识别
     * @param audios 音频列表
     * @return 识别结果列表
     */
    virtual std::vector<AsrResult> recognize_batch(
        const std::vector<AudioData>& audios) = 0;
    
    /**
     * @brief 获取引擎类型
     */
    virtual AsrEngineType get_type() const = 0;
    
    /**
     * @brief 获取引擎名称
     */
    virtual std::string get_name() const = 0;
};

/**
 * @brief 创建 ASR 引擎
 * @param type 引擎类型
 * @return 引擎智能指针
 */
std::unique_ptr<IAsrEngine> create_asr_engine(AsrEngineType type);

} // namespace ai
} // namespace synthorbis
