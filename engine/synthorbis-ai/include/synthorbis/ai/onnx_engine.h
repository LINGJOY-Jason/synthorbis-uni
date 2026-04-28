/**
 * @file onnx_engine.h
 * @brief ONNX Runtime ASR 引擎实现
 */

#pragma once

#include "synthorbis/ai/asr_engine.h"

// CMake 通过 target_compile_definitions 传入：
//   SYNTHORBIS_AI_HAS_ONNXRUNTIME=1  (找到 ORT)
//   SYNTHORBIS_AI_HAS_ONNXRUNTIME=0  (未找到 ORT / stub 模式)
#if defined(SYNTHORBIS_AI_HAS_ONNXRUNTIME) && SYNTHORBIS_AI_HAS_ONNXRUNTIME

#include <onnxruntime_cxx_api.h>
#include <vector>
#include <memory>

namespace synthorbis {
namespace ai {

class OnnxAsrEngine : public IAsrEngine {
public:
    OnnxAsrEngine();
    ~OnnxAsrEngine() override;

    int initialize(const AsrConfig& config) override;
    AsrResult recognize(const AudioData& audio) override;
    std::vector<AsrResult> recognize_batch(
        const std::vector<AudioData>& audios) override;

    AsrEngineType get_type() const override { return AsrEngineType::Local; }
    std::string get_name() const override { return "OnnxAsrEngine"; }

private:
    /** 提取 Fbank 特征（560 维，7 帧堆叠） */
    std::vector<float> extract_features(const AudioData& audio);

    /** CTC 贪婪解码 */
    std::string greedy_decode(const float* logits, int time_steps, int vocab_size);

    AsrConfig config_;
    std::unique_ptr<Ort::Env>            env_;
    std::unique_ptr<Ort::Session>        session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;

    // 输入输出名称（SenseVoice 有 4 个输入）
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;

    // 词表
    std::vector<std::string> vocab_;
};

} // namespace ai
} // namespace synthorbis

#else // stub 模式：ONNX Runtime 不可用

#include <vector>
#include <memory>

namespace synthorbis {
namespace ai {

/** Stub 实现，ONNX Runtime 不可用时使用 */
class OnnxAsrEngine : public IAsrEngine {
public:
    OnnxAsrEngine()           = default;
    ~OnnxAsrEngine() override = default;

    int initialize(const AsrConfig&) override { return -1; }
    AsrResult recognize(const AudioData&) override { return {}; }
    std::vector<AsrResult> recognize_batch(
        const std::vector<AudioData>&) override { return {}; }

    AsrEngineType get_type() const override { return AsrEngineType::Local; }
    std::string get_name() const override { return "OnnxAsrEngine"; }
};

} // namespace ai
} // namespace synthorbis

#endif // SYNTHORBIS_AI_HAS_ONNXRUNTIME
