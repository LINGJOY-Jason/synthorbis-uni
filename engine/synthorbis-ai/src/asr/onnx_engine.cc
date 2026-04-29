#include "synthorbis/ai/onnx_engine.h"

#if defined(SYNTHORBIS_AI_HAS_ONNXRUNTIME) && SYNTHORBIS_AI_HAS_ONNXRUNTIME

#include "synthorbis/ai/fbank.h"
#include "synthorbis/ai/ctc_decoder.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>
#include <string>

namespace synthorbis {
namespace ai {

// ============================================================
// 辅助：从文件名推断模型精度
// ============================================================

/*static*/
ModelPrecision OnnxAsrEngine::infer_precision(const std::string& model_path) {
    // 转小写后缀匹配（简单字符串搜索）
    auto contains = [&](const char* sub) -> bool {
        return model_path.find(sub) != std::string::npos;
    };

    if (contains("_fp16") || contains("_float16"))
        return ModelPrecision::Float16;
    if (contains("_int8_static"))
        return ModelPrecision::Int8Static;
    if (contains("_int8_dynamic") || contains("_int8"))
        return ModelPrecision::Int8Dynamic;
    return ModelPrecision::Float32;
}

// ============================================================
// 辅助：按精度配置 SessionOptions
// ============================================================

void OnnxAsrEngine::apply_session_options(Ort::SessionOptions& opts,
                                           ModelPrecision prec) const {
    const auto& cfg = config_;

    // --- 线程 ---
    opts.SetIntraOpNumThreads(cfg.num_threads);
    opts.SetInterOpNumThreads(cfg.num_threads);

    // --- 内存优化 ---
    if (cfg.enable_memory_pattern)
        opts.EnableMemPattern();
    else
        opts.DisableMemPattern();

    if (cfg.enable_cpu_mem_arena)
        opts.EnableCpuMemArena();
    else
        opts.DisableCpuMemArena();

    // --- Graph 优化 ---
    using OGL = GraphOptimizationLevel;
    OGL level = OGL::ORT_ENABLE_ALL;
    if      (cfg.graph_opt_level == 0)  level = OGL::ORT_DISABLE_ALL;
    else if (cfg.graph_opt_level == 1)  level = OGL::ORT_ENABLE_BASIC;
    else if (cfg.graph_opt_level == 2)  level = OGL::ORT_ENABLE_EXTENDED;
    opts.SetGraphOptimizationLevel(level);

    // --- 精度特定配置 ---
    switch (prec) {
        case ModelPrecision::Float16:
            // FP16 模型：确保全量优化开启，无需额外设置
            opts.SetGraphOptimizationLevel(OGL::ORT_ENABLE_ALL);
            break;

        case ModelPrecision::Int8Dynamic:
        case ModelPrecision::Int8Static:
            // INT8 模型：使用 SEQUENTIAL 执行模式，减少 QDQ 节点融合冲突
            opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            // INT8 模型内存访问模式与 FP32 不同，关闭内存 arena 预分配
            // （ORT 会按需分配，避免 INT8 节点内存对齐问题）
            opts.DisableCpuMemArena();
            opts.SetGraphOptimizationLevel(OGL::ORT_ENABLE_EXTENDED);
            break;

        default:
            break;
    }
}

// ============================================================
// OnnxAsrEngine 实现
// ============================================================

OnnxAsrEngine::OnnxAsrEngine() = default;
OnnxAsrEngine::~OnnxAsrEngine() = default;

int OnnxAsrEngine::initialize(const AsrConfig& config) {
    config_ = config;

    // ---- 推断/确定模型精度 ----
    if (config_.precision == ModelPrecision::Auto) {
        precision_ = infer_precision(config_.model_path);
    } else {
        precision_ = config_.precision;
    }

    // ---- 初始化 ONNX Runtime ----
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING);
    session_options_ = std::make_unique<Ort::SessionOptions>();

    // 根据精度配置 SessionOptions
    apply_session_options(*session_options_, precision_);

    // ---- 创建会话 ----
    try {
#ifdef _WIN32
        // Windows 上 model_path 可能包含中文路径，使用宽字符 API
        std::wstring wpath(config_.model_path.begin(), config_.model_path.end());
        session_ = std::make_unique<Ort::Session>(*env_,
            wpath.c_str(), *session_options_);
#else
        session_ = std::make_unique<Ort::Session>(*env_,
            config_.model_path.c_str(), *session_options_);
#endif
    } catch (const Ort::Exception& e) {
        (void)e;
        return -1;
    }

    // ---- 获取输入输出名称 ----
    Ort::AllocatorWithDefaultOptions allocator;
    auto input_count  = session_->GetInputCount();
    auto output_count = session_->GetOutputCount();

    for (size_t i = 0; i < input_count; ++i) {
        auto name_ptr = session_->GetInputNameAllocated(i, allocator);
        input_names_.push_back(name_ptr.get());
    }
    for (size_t i = 0; i < output_count; ++i) {
        auto name_ptr = session_->GetOutputNameAllocated(i, allocator);
        output_names_.push_back(name_ptr.get());
    }

    // ---- 加载词表 ----
    if (!config_.tokens_path.empty()) {
        std::ifstream fin(config_.tokens_path);
        if (fin.is_open()) {
            std::string line;
            while (std::getline(fin, line)) {
                if (!line.empty()) {
                    // 词表格式可能是 "token\tscore"（SentencePiece 格式）
                    // 只取 tab 之前的部分作为 token
                    auto tab_pos = line.find('\t');
                    if (tab_pos != std::string::npos) {
                        vocab_.push_back(line.substr(0, tab_pos));
                    } else {
                        vocab_.push_back(line);
                    }
                }
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

// ============================================================
// 特征提取
// ============================================================

std::vector<float> OnnxAsrEngine::extract_features(const AudioData& audio) {
    FbankConfig fb_cfg;
    fb_cfg.sample_rate       = audio.sample_rate;
    fb_cfg.frame_length_ms   = 25.0f;
    fb_cfg.frame_shift_ms    = 10.0f;
    fb_cfg.num_mel_bins      = 80;
    fb_cfg.low_freq          = 20.0f;
    fb_cfg.high_freq         = -400.0f;  // nyquist - 400 Hz
    fb_cfg.preemph_coeff     = 0.97f;
    fb_cfg.remove_dc_offset  = true;
    fb_cfg.use_log_fbank     = true;
    fb_cfg.apply_cmvn        = false;    // 模型内部已做 CMVN
    fb_cfg.stack_frames      = 7;
    fb_cfg.stack_stride      = 1;

    FbankExtractor extractor(fb_cfg);
    auto result = extractor.compute(audio.data, audio.samples);
    return result.features;  // [num_frames × 560] 一维展开
}

// ============================================================
// 推理：recognize
// ============================================================

AsrResult OnnxAsrEngine::recognize(const AudioData& audio) {
    AsrResult result;
    result.text        = "";
    result.confidence  = 0.0f;
    result.process_time = 0.0;

    auto t_start = std::chrono::high_resolution_clock::now();

    // ---- 1. Fbank 特征提取 ----
    auto features  = extract_features(audio);
    int num_frames = static_cast<int>(features.size()) / 560;
    if (num_frames <= 0) return result;

    // ---- 2. 构建输入张量 ----
    // SenseVoice 输入：speech [B,T,560] float32, speech_lengths [B] int32/int64,
    //                  language [B] int32, textnorm [B] int32
    std::array<int64_t, 3> speech_shape = {1, static_cast<int64_t>(num_frames), 560};
    std::array<int64_t, 1> scalar_shape = {1};

    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    // FP16 模型：speech 输入需要转为 float16（ORT 会检查类型）
    // 当前实现：如果模型输入类型是 float16，创建 FP16 张量；否则 FP32。
    // 通过检查 session input type 来动态决定
    bool need_fp16_speech = false;
    if (precision_ == ModelPrecision::Float16 && !input_names_.empty()) {
        try {
            auto type_info = session_->GetInputTypeInfo(0);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            need_fp16_speech =
                (tensor_info.GetElementType() == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16);
        } catch (...) {}
    }

    std::vector<Ort::Value> input_tensors;
    std::vector<uint16_t>   speech_fp16;   // FP16 分支数据（外部作用域，保证生命周期）
    std::vector<float>      speech_fp32;   // FP32 分支数据（外部作用域，保证生命周期）

    if (need_fp16_speech) {
        // FP32 → FP16 转换（简单截断，实际部署建议使用 onnxconverter 保持 keep_io_types）
        speech_fp16.resize(features.size());
        for (size_t i = 0; i < features.size(); ++i) {
            // 简易 float → float16 bit-cast（精度足够）
            float f = features[i];
            uint32_t bits;
            std::memcpy(&bits, &f, sizeof(bits));
            uint16_t h = static_cast<uint16_t>(
                ((bits >> 16) & 0x8000) |                      // sign
                (std::min(std::max((int)((bits >> 23) & 0xFF) - 127 + 15, 0), 31) << 10) |
                ((bits >> 13) & 0x3FF)                         // mantissa
            );
            speech_fp16[i] = h;
        }
        input_tensors.push_back(
            Ort::Value::CreateTensor<uint16_t>(
                mem_info, speech_fp16.data(), speech_fp16.size(),
                speech_shape.data(), speech_shape.size()));
    } else {
        speech_fp32 = features;  // 数据生命周期与 input_tensors 对齐
        input_tensors.push_back(
            Ort::Value::CreateTensor<float>(
                mem_info, speech_fp32.data(), speech_fp32.size(),
                speech_shape.data(), speech_shape.size()));
    }

    // SenseVoice 模型要求 int32 输入（speech_lengths / language / textnorm）
    std::vector<int32_t> lengths_data  = {static_cast<int32_t>(num_frames)};
    std::vector<int32_t> language_data = {static_cast<int32_t>(config_.language)};
    std::vector<int32_t> textnorm_data = {config_.text_normalization ? 1 : 0};

    input_tensors.push_back(
        Ort::Value::CreateTensor<int32_t>(
            mem_info, lengths_data.data(), lengths_data.size(),
            scalar_shape.data(), scalar_shape.size()));
    input_tensors.push_back(
        Ort::Value::CreateTensor<int32_t>(
            mem_info, language_data.data(), language_data.size(),
            scalar_shape.data(), scalar_shape.size()));
    input_tensors.push_back(
        Ort::Value::CreateTensor<int32_t>(
            mem_info, textnorm_data.data(), textnorm_data.size(),
            scalar_shape.data(), scalar_shape.size()));

    // ---- 3. 输入/输出名称指针 ----
    std::vector<const char*> input_names_c;
    if (!input_names_.empty()) {
        for (const auto& n : input_names_) input_names_c.push_back(n.c_str());
    } else {
        static const char* kDefaultInputs[] = {
            "speech", "speech_lengths", "language", "textnorm"};
        for (int i = 0; i < 4; ++i) input_names_c.push_back(kDefaultInputs[i]);
    }
    std::vector<const char*> output_names_c;
    for (const auto& n : output_names_) output_names_c.push_back(n.c_str());

    // ---- 4. 推理 ----
    // ORT 1.17+ Run() 要求: input_names (char* const*), input_values (const Value*),
    //   input_count, output_names (char* const*), output_count
    std::vector<Ort::Value> outputs;
    try {
        Ort::RunOptions run_opts{nullptr};
        outputs = session_->Run(
            run_opts,
            input_names_c.data(),
            input_tensors.data(),
            input_tensors.size(),
            output_names_c.data(),
            output_names_c.size());
    } catch (const Ort::Exception&) {
        auto t_end = std::chrono::high_resolution_clock::now();
        result.process_time =
            std::chrono::duration<double>(t_end - t_start).count();
        return result;
    }

    // ---- 5. CTC 解码 ----
    // INT8/FP16 模型输出通常仍为 float32（ORT 在内部做了反量化）
    if (!outputs.empty()) {
        const float* logits    = outputs[0].GetTensorData<float>();
        auto         out_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int time_steps = static_cast<int>(out_shape.size() > 1 ? out_shape[1] : 0);
        int vocab_size = static_cast<int>(out_shape.size() > 2 ? out_shape[2] : 0);

        if (time_steps > 0 && vocab_size > 0) {
            auto hyps = decoder_.decode(logits, time_steps, vocab_size);
            if (!hyps.empty()) {
                result.text = hyps[0].text;
                float norm_score  = hyps[0].ctc_score / static_cast<float>(time_steps);
                result.confidence = 1.0f / (1.0f + std::exp(-norm_score - 2.0f));
            }
        }
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    result.process_time =
        std::chrono::duration<double>(t_end - t_start).count();
    return result;
}

// ============================================================
// 批量推理
// ============================================================

std::vector<AsrResult> OnnxAsrEngine::recognize_batch(
    const std::vector<AudioData>& audios) {
    std::vector<AsrResult> results;
    results.reserve(audios.size());
    for (const auto& audio : audios) {
        results.push_back(recognize(audio));
    }
    return results;
}

} // namespace ai
} // namespace synthorbis

#endif // SYNTHORBIS_AI_HAS_ONNXRUNTIME


