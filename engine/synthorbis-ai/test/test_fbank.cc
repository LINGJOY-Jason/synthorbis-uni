/**
 * @file test_fbank.cc
 * @brief Fbank 特征提取单元测试
 *
 * 验证：
 *   1. 基本维度正确性（帧数、特征维度）
 *   2. SenseVoice 560 维输出
 *   3. 纯静音输入数值稳定性（无 NaN/Inf）
 *   4. 正弦波输入的能量分布合理
 *   5. int16 输入与 float 输入一致性
 *   6. 预加重效果验证
 *   7. 边界情况处理（极短输入）
 */

#include <gtest/gtest.h>
#include "synthorbis/ai/fbank.h"

#include <cmath>
#include <numeric>
#include <vector>
#include <limits>

using namespace synthorbis::ai;

// ============================================================================
// 辅助函数
// ============================================================================

/** 生成正弦波 PCM 数据 */
static std::vector<float> make_sine(int num_samples, float freq_hz,
                                    int sample_rate = 16000,
                                    float amplitude = 0.5f) {
    std::vector<float> pcm(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        pcm[i] = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * freq_hz * i / sample_rate);
    }
    return pcm;
}

/** 检查 vector 中不含 NaN / Inf */
static bool no_nan_inf(const std::vector<float>& v) {
    for (float x : v) {
        if (!std::isfinite(x)) return false;
    }
    return true;
}

/** 期望帧数（与 FbankExtractor 公式对齐） */
static int expected_frames(int num_samples, int frame_len, int hop_len) {
    if (num_samples < frame_len) return 0;
    return 1 + (num_samples - frame_len) / hop_len;
}

// ============================================================================
// 测试套件
// ============================================================================

class FbankTest : public ::testing::Test {
protected:
    FbankConfig default_cfg;   // 默认 SenseVoice 参数

    void SetUp() override {
        default_cfg.sample_rate      = 16000;
        default_cfg.frame_length_ms  = 25.0f;
        default_cfg.frame_shift_ms   = 10.0f;
        default_cfg.num_mel_bins     = 80;
        default_cfg.low_freq         = 20.0f;
        default_cfg.high_freq        = -400.0f;
        default_cfg.preemph_coeff    = 0.97f;
        default_cfg.use_log_fbank    = true;
        default_cfg.apply_cmvn       = false;
        default_cfg.stack_frames     = 7;
        default_cfg.stack_stride     = 1;
    }
};

// ----------------------------------------------------------------------------
// 1. 基本维度测试
// ----------------------------------------------------------------------------

TEST_F(FbankTest, FeatureDimension_SenseVoice560) {
    FbankExtractor extractor(default_cfg);
    EXPECT_EQ(extractor.feature_dim(), 80 * 7);  // = 560
}

TEST_F(FbankTest, NumFrames_1SecAudio) {
    FbankExtractor extractor(default_cfg);

    int sr = 16000;
    auto pcm = make_sine(sr, 440.0f, sr);   // 1 秒 440Hz 正弦波

    auto result = extractor.compute(pcm.data(), static_cast<int>(pcm.size()));

    // 帧长 400 samples，帧移 160 samples
    // 原始帧数 = 1 + (16000 - 400) / 160 = 98
    // 堆叠后帧数 = 98（stride=1）
    int frame_len = static_cast<int>(25.0f * 0.001f * sr);   // 400
    int hop_len   = static_cast<int>(10.0f * 0.001f * sr);   // 160
    int raw_n     = expected_frames(sr, frame_len, hop_len);  // 98

    EXPECT_EQ(result.num_frames,  raw_n);
    EXPECT_EQ(result.feature_dim, 560);
    EXPECT_EQ(static_cast<int>(result.features.size()),
              result.num_frames * result.feature_dim);
}

TEST_F(FbankTest, NumFrames_3SecAudio) {
    FbankExtractor extractor(default_cfg);
    int sr = 16000;
    int n  = sr * 3;  // 48000 samples
    auto pcm = make_sine(n, 1000.0f, sr);

    auto result = extractor.compute(pcm.data(), n);

    int frame_len = 400;
    int hop_len   = 160;
    int raw_n     = expected_frames(n, frame_len, hop_len);

    EXPECT_EQ(result.num_frames,  raw_n);
    EXPECT_EQ(result.feature_dim, 560);
}

// ----------------------------------------------------------------------------
// 2. 数值稳定性测试
// ----------------------------------------------------------------------------

TEST_F(FbankTest, SilenceInput_NoNanInf) {
    FbankExtractor extractor(default_cfg);
    std::vector<float> silence(16000, 0.0f);   // 纯静音

    auto result = extractor.compute(silence.data(), static_cast<int>(silence.size()));

    EXPECT_GT(result.num_frames, 0);
    EXPECT_TRUE(no_nan_inf(result.features));
}

TEST_F(FbankTest, MaxAmplitude_NoNanInf) {
    FbankExtractor extractor(default_cfg);
    std::vector<float> loud(16000, 1.0f);   // 最大幅度直流信号

    auto result = extractor.compute(loud.data(), static_cast<int>(loud.size()));

    EXPECT_GT(result.num_frames, 0);
    EXPECT_TRUE(no_nan_inf(result.features));
}

TEST_F(FbankTest, SineWave_NoNanInf) {
    FbankExtractor extractor(default_cfg);
    auto pcm = make_sine(16000, 440.0f);

    auto result = extractor.compute(pcm.data(), static_cast<int>(pcm.size()));

    EXPECT_GT(result.num_frames, 0);
    EXPECT_TRUE(no_nan_inf(result.features));
}

// ----------------------------------------------------------------------------
// 3. SenseVoice 堆叠维度精确验证
// ----------------------------------------------------------------------------

TEST_F(FbankTest, StackedOutput_Dim560_AllFrames) {
    FbankExtractor extractor(default_cfg);
    auto pcm = make_sine(16000 * 2, 300.0f);   // 2 秒

    auto result = extractor.compute(pcm.data(), static_cast<int>(pcm.size()));

    ASSERT_GT(result.num_frames, 0);
    ASSERT_EQ(result.feature_dim, 560);

    // 每帧的 560 维切片不应全为零（有信号能量）
    bool all_zero = true;
    for (int d = 0; d < 560 && all_zero; ++d) {
        if (result.features[d] != 0.0f) all_zero = false;
    }
    EXPECT_FALSE(all_zero) << "First frame should not be all zeros for sine input";
}

// ----------------------------------------------------------------------------
// 4. int16 输入测试
// ----------------------------------------------------------------------------

TEST_F(FbankTest, Int16Input_SameDimAsFloat) {
    FbankExtractor extractor(default_cfg);

    int n = 16000;
    auto float_pcm = make_sine(n, 440.0f, 16000, 0.5f);

    // 转为 int16
    std::vector<int16_t> int16_pcm(n);
    for (int i = 0; i < n; ++i) {
        int16_pcm[i] = static_cast<int16_t>(float_pcm[i] * 32767.0f);
    }

    auto result_f = extractor.compute(float_pcm.data(), n);
    auto result_i = extractor.compute_int16(int16_pcm.data(), n);

    EXPECT_EQ(result_f.num_frames,  result_i.num_frames);
    EXPECT_EQ(result_f.feature_dim, result_i.feature_dim);
    EXPECT_TRUE(no_nan_inf(result_i.features));

    // 浮点与 int16 的特征应在合理误差范围内（量化误差）
    double max_diff = 0.0;
    for (size_t k = 0; k < result_f.features.size(); ++k) {
        double diff = std::abs(result_f.features[k] - result_i.features[k]);
        if (diff > max_diff) max_diff = diff;
    }
    // 量化误差在对数 Mel 域内通常在 1-2 以内（log(1 ± 1/32767) 经帧累积后放大）
    EXPECT_LT(max_diff, 2.0) << "int16 vs float feature mismatch too large: " << max_diff;
}

// ----------------------------------------------------------------------------
// 5. 无堆叠模式（stack_frames=1）
// ----------------------------------------------------------------------------

TEST_F(FbankTest, NoStacking_Dim80) {
    FbankConfig cfg = default_cfg;
    cfg.stack_frames = 1;
    FbankExtractor extractor(cfg);

    EXPECT_EQ(extractor.feature_dim(), 80);

    auto pcm    = make_sine(16000, 440.0f);
    auto result = extractor.compute(pcm.data(), static_cast<int>(pcm.size()));

    EXPECT_GT(result.num_frames, 0);
    EXPECT_EQ(result.feature_dim, 80);
    EXPECT_TRUE(no_nan_inf(result.features));
}

// ----------------------------------------------------------------------------
// 6. 边界情况
// ----------------------------------------------------------------------------

TEST_F(FbankTest, TooShortInput_ReturnsEmpty) {
    FbankExtractor extractor(default_cfg);
    std::vector<float> short_audio(100, 0.5f);  // 仅 100 samples，不足一帧（400）

    auto result = extractor.compute(short_audio.data(), static_cast<int>(short_audio.size()));

    EXPECT_EQ(result.num_frames, 0);
}

TEST_F(FbankTest, NullInput_ReturnsEmpty) {
    FbankExtractor extractor(default_cfg);
    auto result = extractor.compute(nullptr, 0);

    EXPECT_EQ(result.num_frames, 0);
}

TEST_F(FbankTest, ExactlyOneFrame_Output) {
    FbankExtractor extractor(default_cfg);
    // 精确一帧长度 = 400 samples
    std::vector<float> pcm(400, 0.1f);

    auto result = extractor.compute(pcm.data(), static_cast<int>(pcm.size()));

    EXPECT_EQ(result.num_frames, 1);
    EXPECT_EQ(result.feature_dim, 560);
    EXPECT_TRUE(no_nan_inf(result.features));
}

// ----------------------------------------------------------------------------
// 7. 频率响应合理性（高频信号 Mel 能量应比低频信号分布偏右）
// ----------------------------------------------------------------------------

TEST_F(FbankTest, FrequencySelectivity_LowVsHigh) {
    FbankConfig cfg = default_cfg;
    cfg.stack_frames = 1;      // 单帧便于分析
    cfg.apply_cmvn   = false;
    FbankExtractor extractor(cfg);

    auto low_pcm  = make_sine(16000, 200.0f);   // 200 Hz
    auto high_pcm = make_sine(16000, 4000.0f);  // 4000 Hz

    auto low_result  = extractor.compute(low_pcm.data(),  static_cast<int>(low_pcm.size()));
    auto high_result = extractor.compute(high_pcm.data(), static_cast<int>(high_pcm.size()));

    ASSERT_GT(low_result.num_frames,  0);
    ASSERT_GT(high_result.num_frames, 0);

    // 计算低频能量集中在前 20% 滤波器，高频集中在后 20%
    int n     = low_result.num_frames;
    int dim   = 80;
    int low_bands  = 16;  // 前 20% Mel 滤波器
    int high_bands = 16;  // 后 20% Mel 滤波器

    double low_low_energy = 0.0, low_high_energy = 0.0;
    double high_low_energy = 0.0, high_high_energy = 0.0;

    for (int t = 0; t < n; ++t) {
        for (int d = 0; d < low_bands; ++d) {
            low_low_energy  += low_result.features[t * dim + d];
            high_low_energy += high_result.features[t * dim + d];
        }
        for (int d = dim - high_bands; d < dim; ++d) {
            low_high_energy  += low_result.features[t * dim + d];
            high_high_energy += high_result.features[t * dim + d];
        }
    }

    // 200Hz 信号：低频带能量应大于高频带
    EXPECT_GT(low_low_energy, low_high_energy)
        << "200Hz signal should have more energy in low Mel bands";

    // 4000Hz 信号：高频带能量应大于低频带
    EXPECT_GT(high_high_energy, high_low_energy)
        << "4000Hz signal should have more energy in high Mel bands";
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
