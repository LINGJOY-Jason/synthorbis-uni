/**
 * @file test_asr_e2e.cc
 * @brief SynthOrbis AI — 端到端 ASR C++ 集成测试
 *
 * 需要环境变量（或编译时宏）指定模型路径：
 *   SYNTHORBIS_MODEL_PATH   — ONNX 模型路径
 *   SYNTHORBIS_TOKENS_PATH  — 词表路径
 *   SYNTHORBIS_TEST_WAV     — 测试音频路径（16kHz mono WAV, optional）
 */

#include <gtest/gtest.h>
#include "synthorbis/ai/asr_engine.h"
#include "synthorbis/ai/onnx_engine.h"
#include "synthorbis/ai/fbank.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
#include <chrono>
#include <cmath>

using namespace synthorbis::ai;

// ============================================================
// 辅助：读取 WAV 文件（简单 PCM，仅支持 16kHz/16bit/mono）
// ============================================================

static bool read_wav(const std::string& path,
                     std::vector<float>& samples,
                     int& sample_rate) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    // 跳过 WAV header（44 字节），简化处理
    char header[44];
    f.read(header, 44);
    if (!f) return false;

    // 从 header 读取 sample rate
    uint32_t sr = 0;
    std::memcpy(&sr, header + 24, 4);
    sample_rate = static_cast<int>(sr);

    // 读取 16-bit PCM 数据
    std::vector<int16_t> pcm;
    int16_t s;
    while (f.read(reinterpret_cast<char*>(&s), 2)) {
        pcm.push_back(s);
    }

    // 归一化到 [-1.0, 1.0]
    samples.resize(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i) {
        samples[i] = static_cast<float>(pcm[i]) / 32768.0f;
    }
    return !samples.empty();
}

// ============================================================
// 辅助：生成 1秒 440Hz 正弦波测试音频
// ============================================================

static std::vector<float> make_sine_wave(float freq, int sr, float duration) {
    int n = static_cast<int>(sr * duration);
    std::vector<float> buf(n);
    for (int i = 0; i < n; ++i) {
        buf[i] = 0.5f * std::sin(2.0f * 3.14159265f * freq * i / sr);
    }
    return buf;
}

// ============================================================
// 集成测试
// ============================================================

class AsrE2ETest : public ::testing::Test {
protected:
    std::string model_path;
    std::string tokens_path;
    std::string test_wav;

    void SetUp() override {
        // 优先用环境变量，其次用默认路径
        auto get_env = [](const char* key, const char* def) -> std::string {
            const char* v = std::getenv(key);
            return v ? std::string(v) : std::string(def);
        };
        model_path  = get_env("SYNTHORBIS_MODEL_PATH",
            "/mnt/c/Users/Administrator/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx/model.onnx");
        tokens_path = get_env("SYNTHORBIS_TOKENS_PATH",
            "/mnt/c/Users/Administrator/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx/tokens.txt");
        test_wav    = get_env("SYNTHORBIS_TEST_WAV",
            "/mnt/c/Users/Administrator/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx/0.wav");
    }

    bool model_available() const {
        std::ifstream f(model_path);
        return f.is_open();
    }
};

// ---- 测试1：Fbank 特征提取（不依赖 ORT）----
TEST_F(AsrE2ETest, FbankFeatureExtraction) {
    auto samples = make_sine_wave(440.0f, 16000, 1.0f);

    AudioData audio;
    audio.data        = samples.data();
    audio.samples     = static_cast<int>(samples.size());
    audio.sample_rate = 16000;

    FbankConfig cfg;
    cfg.sample_rate     = 16000;
    cfg.frame_length_ms = 25.0f;
    cfg.frame_shift_ms  = 10.0f;
    cfg.num_mel_bins    = 80;
    cfg.stack_frames    = 7;
    cfg.stack_stride    = 1;

    FbankExtractor ext(cfg);
    auto result = ext.compute(audio.data, audio.samples);

    EXPECT_GT(result.num_frames, 0);
    EXPECT_EQ(result.feature_dim, 560);
    EXPECT_EQ(static_cast<int>(result.features.size()),
              result.num_frames * result.feature_dim);

    printf("Fbank: %d frames × %d dim = %zu floats\n",
           result.num_frames, result.feature_dim, result.features.size());
}

// ---- 测试2：ORT 引擎创建 ----
TEST_F(AsrE2ETest, EngineCreation) {
    auto engine = create_asr_engine(AsrEngineType::Local);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->get_type(), AsrEngineType::Local);
    EXPECT_EQ(engine->get_name(), "OnnxAsrEngine");
}

// ---- 测试3：引擎加载（需要 ORT + 模型文件）----
TEST_F(AsrE2ETest, EngineInitialize) {
#if !SYNTHORBIS_AI_HAS_ONNXRUNTIME
    GTEST_SKIP() << "ONNX Runtime not available (stub mode)";
#endif
    if (!model_available()) {
        GTEST_SKIP() << "Model not found: " << model_path;
    }

    auto engine = create_asr_engine(AsrEngineType::Local);
    ASSERT_NE(engine, nullptr);

    AsrConfig cfg;
    cfg.type         = AsrEngineType::Local;
    cfg.model_path   = model_path;
    cfg.tokens_path  = tokens_path;
    cfg.num_threads  = 4;
    cfg.language     = 0;  // auto
    cfg.text_normalization = false;

    auto t0 = std::chrono::high_resolution_clock::now();
    int ret = engine->initialize(cfg);
    auto t1 = std::chrono::high_resolution_clock::now();

    double load_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("Model load time: %.1f ms\n", load_ms);

    EXPECT_EQ(ret, 0) << "Engine initialize failed";
}

// ---- 测试4：静音输入不崩溃 ----
TEST_F(AsrE2ETest, SilenceInput_NoCrash) {
#if !SYNTHORBIS_AI_HAS_ONNXRUNTIME
    GTEST_SKIP() << "ONNX Runtime not available";
#endif
    if (!model_available()) {
        GTEST_SKIP() << "Model not found: " << model_path;
    }

    auto engine = create_asr_engine(AsrEngineType::Local);
    AsrConfig cfg;
    cfg.model_path  = model_path;
    cfg.tokens_path = tokens_path;
    cfg.num_threads = 4;
    ASSERT_EQ(engine->initialize(cfg), 0);

    // 1秒静音
    std::vector<float> silence(16000, 0.0f);
    AudioData audio;
    audio.data        = silence.data();
    audio.samples     = 16000;
    audio.sample_rate = 16000;

    AsrResult result = engine->recognize(audio);
    // 不崩溃即可，输出可能为空
    printf("Silence recognition result: '%s' (conf=%.3f, time=%.3fms)\n",
           result.text.c_str(), result.confidence, result.process_time * 1000.0);
    SUCCEED();
}

// ---- 测试5：真实音频端到端（最核心测试）----
TEST_F(AsrE2ETest, RealAudio_E2E_Inference) {
#if !SYNTHORBIS_AI_HAS_ONNXRUNTIME
    GTEST_SKIP() << "ONNX Runtime not available";
#endif
    if (!model_available()) {
        GTEST_SKIP() << "Model not found: " << model_path;
    }

    // 加载测试音频
    std::vector<float> samples;
    int sr = 16000;
    bool wav_ok = false;
    if (!test_wav.empty()) {
        wav_ok = read_wav(test_wav, samples, sr);
        if (wav_ok) {
            printf("Loaded WAV: %s (%d samples @ %d Hz = %.2f s)\n",
                   test_wav.c_str(), (int)samples.size(), sr,
                   (float)samples.size() / sr);
        }
    }
    if (!wav_ok) {
        // 回退：生成 3秒测试音频
        samples = make_sine_wave(440.0f, 16000, 3.0f);
        sr = 16000;
        printf("Using synthetic 3s sine wave\n");
    }

    // 初始化引擎
    auto engine = create_asr_engine(AsrEngineType::Local);
    AsrConfig cfg;
    cfg.model_path   = model_path;
    cfg.tokens_path  = tokens_path;
    cfg.num_threads  = 4;
    cfg.language     = 0;
    cfg.text_normalization = false;
    ASSERT_EQ(engine->initialize(cfg), 0);

    AudioData audio;
    audio.data        = samples.data();
    audio.samples     = static_cast<int>(samples.size());
    audio.sample_rate = sr;

    // 推理
    auto t0 = std::chrono::high_resolution_clock::now();
    AsrResult result = engine->recognize(audio);
    auto t1 = std::chrono::high_resolution_clock::now();

    double total_ms  = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double audio_dur = static_cast<double>(samples.size()) / sr;
    double rtf       = total_ms / 1000.0 / audio_dur;

    printf("\n========== C++ ASR E2E 测试结果 ==========\n");
    printf("音频时长: %.2f s\n", audio_dur);
    printf("推理耗时: %.1f ms\n", total_ms);
    printf("RTF:      %.3f\n", rtf);
    printf("置信度:   %.3f\n", result.confidence);
    printf("识别文本: '%s'\n", result.text.c_str());
    printf("==========================================\n\n");

    // RTF < 1.0 表示实时能力
    EXPECT_LT(rtf, 1.0) << "RTF >= 1.0, 无法实时识别";
    // 推理过程不崩溃
    SUCCEED();
}
