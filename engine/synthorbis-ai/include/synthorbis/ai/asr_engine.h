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
 * @brief ASR 引擎配置
 */
struct AsrConfig {
    AsrEngineType type = AsrEngineType::Local;
    
    // 本地模型配置
    std::string model_path;        // ONNX 模型路径
    std::string tokens_path;       // 词表文件
    int num_threads = 4;            // CPU 线程数
    
    // 云端配置
    std::string api_endpoint;       // API 地址
    std::string api_key;           // API 密钥
    
    // 语言设置 (0=自动检测)
    int language = 0;
    
    // 文本归一化
    bool text_normalization = true;
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
