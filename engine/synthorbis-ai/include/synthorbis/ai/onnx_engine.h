/**
 * @file onnx_engine.h
 * @brief ONNX Runtime ASR 引擎实现
 */

#pragma once

#include "synthorbis/ai/asr_engine.h"
#include "synthorbis/ai/ctc_decoder.h"

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

    /** 设置解码器配置（在 initialize 之后调用） */
    void set_decoder_config(const CtcDecoderConfig& cfg);

    /** 设置 LM（可选，用于 BeamSearchLM 模式） */
    void set_language_model(std::shared_ptr<ILanguageModel> lm);

    /** 返回当前实际使用的模型精度 */
    ModelPrecision effective_precision() const { return precision_; }

private:
    /** 提取 Fbank 特征（560 维，7 帧堆叠） */
    std::vector<float> extract_features(const AudioData& audio);

    /**
     * @brief 从 model_path 文件名推断模型精度
     *
     * 规则（与 quantize_onnx.py 产出文件名对应）：
     *   *_fp16.onnx           → Float16
     *   *_int8_dynamic.onnx   → Int8Dynamic
     *   *_int8_static.onnx    → Int8Static
     *   其他                  → Float32
     */
    static ModelPrecision infer_precision(const std::string& model_path);

    /**
     * @brief 根据精度配置 SessionOptions
     *
     * - Float32    : 标准全精度，开启 graph 优化
     * - Float16    : 与 Float32 相同（FP16 算子由模型本身保证）；
     *                开启 ORT_ENABLE_ALL + enable_mem_pattern
     * - Int8Dynamic/Static: 关闭量化节点优化跳过（ORT 自动识别 QDQ 节点）；
     *                开启 SetExecutionMode(ORT_SEQUENTIAL)
     */
    void apply_session_options(Ort::SessionOptions& opts,
                               ModelPrecision prec) const;

    AsrConfig config_;
    ModelPrecision precision_ = ModelPrecision::Float32; ///< 实际使用的精度

    std::unique_ptr<Ort::Env>            env_;
    std::unique_ptr<Ort::Session>        session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;

    // 输入输出名称（SenseVoice 有 4 个输入）
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;

    // 词表
    std::vector<std::string> vocab_;

    // CTC 解码器
    CtcDecoder decoder_;
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
