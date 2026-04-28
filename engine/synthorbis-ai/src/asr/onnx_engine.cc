/**
 * @file onnx_engine.cc
 * @brief ONNX Runtime ASR 引擎实现
 */

#include "synthorbis/ai/onnx_engine.h"

#if defined(SYNTHORBIS_AI_HAS_ONNXRUNTIME) && SYNTHORBIS_AI_HAS_ONNXRUNTIME

#include "synthorbis/ai/fbank.h"
#include "synthorbis/ai/ctc_decoder.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>

namespace synthorbis {
namespace ai {

OnnxAsrEngine::OnnxAsrEngine() = default;

OnnxAsrEngine::~OnnxAsrEngine() = default;

int OnnxAsrEngine::initialize(const AsrConfig& config) {
    config_ = config;
    
    // 初始化 ONNX Runtime
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING);
    session_options_ = std::make_unique<Ort::SessionOptions>();
    
    // 设置线程数
    session_options_->SetIntraOpNumThreads(config_.num_threads);
    session_options_->SetInterOpNumThreads(config_.num_threads);
    
    // 创建会话
    try {
        session_ = std::make_unique<Ort::Session>(*env_, 
            config_.model_path.c_str(), *session_options_);
    } catch (const Ort::Exception& e) {
        return -1;
    }
    
    // 获取输入输出名称
    Ort::AllocatorWithDefaultOptions allocator;
    auto input_count  = session_->GetInputCount();
    auto output_count = session_->GetOutputCount();

    for (size_t i = 0; i < input_count; ++i) {
        auto name_ptr = session_->GetInputNameAllocated(i, allocator);
        input_names_.push_back(name_ptr.get());
    }

    for (size_t i = 0; i < output_count; ++i) {
        auto output_name_ptr = session_->GetOutputNameAllocated(i, allocator);
        output_names_.push_back(output_name_ptr.get());
    }
    
    // 加载词表
    if (!config_.tokens_path.empty()) {
        std::ifstream fin(config_.tokens_path);
        if (fin.is_open()) {
            std::string line;
            while (std::getline(fin, line)) {
                if (!line.empty()) vocab_.push_back(line);
            }
        }
    }

    // 同步词表到解码器
    decoder_.set_vocab(vocab_);

    return 0;
}

void OnnxAsrEngine::set_decoder_config(const CtcDecoderConfig& cfg) {
    decoder_ = CtcDecoder(cfg);
    decoder_.set_vocab(vocab_);
}

void OnnxAsrEngine::set_language_model(std::shared_ptr<ILanguageModel> lm) {
    decoder_.set_lm(lm);
}

std::vector<float> OnnxAsrEngine::extract_features(const AudioData& audio) {
    // 使用标准 Fbank 特征提取（符合 FunASR/SenseVoice 规格）
    //   帧长 25ms / 帧移 10ms / 80 Mel bins / 7 帧堆叠 -> 560 维
    FbankConfig fb_cfg;
    fb_cfg.sample_rate       = audio.sample_rate;
    fb_cfg.frame_length_ms   = 25.0f;
    fb_cfg.frame_shift_ms    = 10.0f;
    fb_cfg.num_mel_bins      = 80;
    fb_cfg.low_freq          = 20.0f;
    fb_cfg.high_freq         = -400.0f;   // nyquist - 400 Hz
    fb_cfg.preemph_coeff     = 0.97f;
    fb_cfg.remove_dc_offset  = true;
    fb_cfg.use_log_fbank     = true;
    fb_cfg.apply_cmvn        = false;     // 模型内部已做 CMVN
    fb_cfg.stack_frames      = 7;
    fb_cfg.stack_stride      = 1;

    FbankExtractor extractor(fb_cfg);
    auto result = extractor.compute(audio.data, audio.samples);

    return result.features;  // [num_frames, 560] 展开为一维
}

AsrResult OnnxAsrEngine::recognize(const AudioData& audio) {
    AsrResult result;
    result.text = "";
    result.confidence = 0.0f;

    auto start_time = std::chrono::high_resolution_clock::now();

    // ---- 1. Fbank 特征提取 ----
    auto features  = extract_features(audio);
    int num_frames = static_cast<int>(features.size()) / 560;
    if (num_frames <= 0) {
        result.process_time = 0.0;
        return result;
    }

    // ---- 2. 构建输入张量 ----
    // SenseVoice 输入：speech [B,T,560], speech_lengths [B], language [B], textnorm [B]
    std::array<int64_t, 3> speech_shape  = {1, static_cast<int64_t>(num_frames), 560};
    std::array<int64_t, 1> scalar_shape  = {1};

    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    // 数据缓冲区（必须在 CreateTensor 调用期间保持有效）
    std::vector<float>   speech_data(features);
    std::vector<int64_t> lengths_data   = {static_cast<int64_t>(num_frames)};
    std::vector<int64_t> language_data  = {static_cast<int64_t>(config_.language)};
    std::vector<int64_t> textnorm_data  = {config_.text_normalization ? 1LL : 0LL};

    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(
        Ort::Value::CreateTensor<float>(
            mem_info, speech_data.data(), speech_data.size(),
            speech_shape.data(), speech_shape.size()));
    input_tensors.push_back(
        Ort::Value::CreateTensor<int64_t>(
            mem_info, lengths_data.data(), lengths_data.size(),
            scalar_shape.data(), scalar_shape.size()));
    input_tensors.push_back(
        Ort::Value::CreateTensor<int64_t>(
            mem_info, language_data.data(), language_data.size(),
            scalar_shape.data(), scalar_shape.size()));
    input_tensors.push_back(
        Ort::Value::CreateTensor<int64_t>(
            mem_info, textnorm_data.data(), textnorm_data.size(),
            scalar_shape.data(), scalar_shape.size()));

    // ---- 3. 构建输入/输出名称指针数组 ----
    // 需与 ONNX 模型实际输入顺序匹配
    // SenseVoice-small: speech, speech_lengths, language, textnorm
    std::vector<const char*> input_names_c;
    if (!input_names_.empty()) {
        for (const auto& n : input_names_) input_names_c.push_back(n.c_str());
    } else {
        // fallback：按 SenseVoice 标准顺序
        static const char* kDefaultInputs[] = {
            "speech", "speech_lengths", "language", "textnorm"};
        for (int i = 0; i < 4; ++i) input_names_c.push_back(kDefaultInputs[i]);
    }

    std::vector<const char*> output_names_c;
    for (const auto& n : output_names_) output_names_c.push_back(n.c_str());

    // ---- 4. 运行推理 ----
    std::vector<Ort::Value> outputs;
    try {
        outputs = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_c.data(),  input_tensors.size(),
            output_names_c.data(), output_names_c.size());
    } catch (const Ort::Exception& e) {
        result.process_time = 0.0;
        return result;
    }

    // ---- 5. CTC 解码（Beam Search 或 Greedy）----
    if (!outputs.empty()) {
        const float* logits    = outputs[0].GetTensorData<float>();
        auto         out_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        // out_shape: [batch, time, vocab]
        int time_steps = static_cast<int>(out_shape.size() > 1 ? out_shape[1] : 0);
        int vocab_size = static_cast<int>(out_shape.size() > 2 ? out_shape[2] : 0);

        if (time_steps > 0 && vocab_size > 0) {
            auto hyps = decoder_.decode(logits, time_steps, vocab_size);
            if (!hyps.empty()) {
                result.text       = hyps[0].text;
                // 将 CTC log-prob 转换为近似置信度（sigmoid 映射到 [0,1]）
                float norm_score  = hyps[0].ctc_score / static_cast<float>(time_steps);
                result.confidence = 1.0f / (1.0f + std::exp(-norm_score - 2.0f));
            }
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    result.process_time =
        std::chrono::duration<double>(end_time - start_time).count();

    return result;
}

std::vector<AsrResult> OnnxAsrEngine::recognize_batch(
    const std::vector<AudioData>& audios) {
    std::vector<AsrResult> results;
    for (const auto& audio : audios) {
        results.push_back(recognize(audio));
    }
    return results;
}

} // namespace ai
} // namespace synthorbis

#endif // SYNTHORBIS_AI_HAS_ONNXRUNTIME

