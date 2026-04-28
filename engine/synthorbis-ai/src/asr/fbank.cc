/**
 * @file fbank.cc
 * @brief Fbank 特征提取实现
 *
 * 算法参考：
 *   - Kaldi compute-fbank-feats
 *   - ESPnet fbank_default.json 参数
 *   - FunASR SenseVoice 预处理规格
 */

#include "synthorbis/ai/fbank.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace synthorbis {
namespace ai {

// ============================================================================
// 构造 & 预计算
// ============================================================================

FbankExtractor::FbankExtractor(const FbankConfig& config)
    : config_(config)
{
    if (config_.sample_rate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (config_.num_mel_bins <= 0) {
        throw std::invalid_argument("num_mel_bins must be positive");
    }

    frame_len_ = static_cast<int>(config_.frame_length_ms * 0.001f * config_.sample_rate);
    hop_len_   = static_cast<int>(config_.frame_shift_ms  * 0.001f * config_.sample_rate);

    if (frame_len_ <= 0 || hop_len_ <= 0) {
        throw std::invalid_argument("Frame length and shift must result in positive sample counts");
    }

    fft_size_ = next_pow2(frame_len_);

    // 真实高频：负数视为 sample_rate/2 + high_freq
    float nyquist = static_cast<float>(config_.sample_rate) / 2.0f;
    high_freq_    = (config_.high_freq <= 0.0f)
                        ? nyquist + config_.high_freq
                        : config_.high_freq;
    if (high_freq_ > nyquist) high_freq_ = nyquist;

    precompute_window();
    precompute_mel_filters();
}

// ---------------------------------------------------------------------------

void FbankExtractor::precompute_window() {
    hamming_window_.resize(frame_len_);
    // Hamming: w(n) = 0.54 - 0.46 * cos(2πn / (N-1))
    for (int n = 0; n < frame_len_; ++n) {
        hamming_window_[n] = 0.54f
            - 0.46f * std::cos(2.0f * static_cast<float>(M_PI) * n / (frame_len_ - 1));
    }
}

// ---------------------------------------------------------------------------

void FbankExtractor::precompute_mel_filters() {
    int num_bins     = config_.num_mel_bins;
    int spec_bins    = fft_size_ / 2 + 1;
    float mel_low    = hz_to_mel(config_.low_freq);
    float mel_high   = hz_to_mel(high_freq_);

    // 均匀划分 Mel 空间的 num_bins+2 个中心点
    std::vector<float> mel_centers(num_bins + 2);
    for (int i = 0; i < num_bins + 2; ++i) {
        mel_centers[i] = mel_low + (mel_high - mel_low) * i / (num_bins + 1);
    }

    // 将 Mel 中心点转换为 Hz，再映射到 FFT bin 下标
    std::vector<float> hz_centers(num_bins + 2);
    std::vector<float> bin_centers(num_bins + 2);
    float freq_resolution = static_cast<float>(config_.sample_rate) / fft_size_;
    for (int i = 0; i < num_bins + 2; ++i) {
        hz_centers[i]  = mel_to_hz(mel_centers[i]);
        bin_centers[i] = hz_centers[i] / freq_resolution;
    }

    // 构建三角滤波器
    mel_filter_bank_.assign(num_bins, std::vector<float>(spec_bins, 0.0f));
    for (int m = 0; m < num_bins; ++m) {
        float left   = bin_centers[m];
        float center = bin_centers[m + 1];
        float right  = bin_centers[m + 2];

        for (int k = 0; k < spec_bins; ++k) {
            float fk = static_cast<float>(k);
            if (fk >= left && fk <= center) {
                mel_filter_bank_[m][k] = (fk - left) / (center - left);
            } else if (fk > center && fk <= right) {
                mel_filter_bank_[m][k] = (right - fk) / (right - center);
            }
        }
    }
}

// ============================================================================
// 公开接口
// ============================================================================

FbankResult FbankExtractor::compute(const float* pcm, int num_samples) const {
    if (!pcm || num_samples <= 0) {
        return {};
    }

    // ---- 1. 预加重 ----
    std::vector<float> signal(pcm, pcm + num_samples);
    if (config_.preemph_coeff > 0.0f) {
        for (int i = num_samples - 1; i >= 1; --i) {
            signal[i] -= config_.preemph_coeff * signal[i - 1];
        }
        signal[0] *= (1.0f - config_.preemph_coeff);
    }

    // ---- 2. 去除直流偏移 ----
    if (config_.remove_dc_offset) {
        float mean = 0.0f;
        for (float s : signal) mean += s;
        mean /= static_cast<float>(num_samples);
        for (float& s : signal) s -= mean;
    }

    // ---- 3. 分帧 ----
    int num_frames = 1 + (num_samples - frame_len_) / hop_len_;
    if (num_frames <= 0) {
        return {};
    }

    std::vector<std::vector<float>> mel_feats_raw;
    mel_feats_raw.reserve(num_frames);

    for (int i = 0; i < num_frames; ++i) {
        int offset = i * hop_len_;

        // 截取帧并加窗
        std::vector<float> frame(frame_len_);
        for (int j = 0; j < frame_len_; ++j) {
            frame[j] = signal[offset + j] * hamming_window_[j];
        }

        // ---- 4. FFT 功率谱 ----
        auto power_spec = compute_power_spectrum(frame);

        // ---- 5. Mel 滤波 + log ----
        mel_feats_raw.push_back(apply_mel_filters(power_spec));
    }

    // ---- 6. 子帧堆叠 ----
    auto stacked = stack_subframes(mel_feats_raw);

    int out_frames = static_cast<int>(stacked.size());
    int feat_dim   = (out_frames > 0) ? static_cast<int>(stacked[0].size()) : 0;

    // ---- 7. CMVN 归一化 (全局简单版本) ----
    FbankResult result;
    result.num_frames  = out_frames;
    result.feature_dim = feat_dim;
    result.features.resize(static_cast<size_t>(out_frames) * feat_dim);

    for (int t = 0; t < out_frames; ++t) {
        for (int d = 0; d < feat_dim; ++d) {
            float val = stacked[t][d];
            if (config_.apply_cmvn) {
                val = (val - config_.mean_norm) / config_.std_norm;
            }
            result.features[static_cast<size_t>(t) * feat_dim + d] = val;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------

FbankResult FbankExtractor::compute_int16(const int16_t* pcm, int num_samples) const {
    if (!pcm || num_samples <= 0) return {};

    constexpr float kScale = 1.0f / 32768.0f;
    std::vector<float> float_pcm(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float_pcm[i] = static_cast<float>(pcm[i]) * kScale;
    }
    return compute(float_pcm.data(), num_samples);
}

// ============================================================================
// 内部实现
// ============================================================================

std::vector<float> FbankExtractor::compute_power_spectrum(
    const std::vector<float>& frame) const
{
    // 零填充到 fft_size_
    std::vector<float> re(fft_size_, 0.0f);
    std::vector<float> im(fft_size_, 0.0f);
    int copy_len = std::min(static_cast<int>(frame.size()), fft_size_);
    std::copy(frame.begin(), frame.begin() + copy_len, re.begin());

    fft(re, im);

    // 功率谱 |X[k]|^2 / fft_size（取单侧谱，直流和奈奎斯特频率不重复）
    int spec_bins = fft_size_ / 2 + 1;
    std::vector<float> power(spec_bins);
    float norm = static_cast<float>(fft_size_);
    for (int k = 0; k < spec_bins; ++k) {
        power[k] = (re[k] * re[k] + im[k] * im[k]) / norm;
    }
    return power;
}

// ---------------------------------------------------------------------------

std::vector<float> FbankExtractor::apply_mel_filters(
    const std::vector<float>& power_spec) const
{
    int num_bins = config_.num_mel_bins;
    std::vector<float> mel_energy(num_bins);

    for (int m = 0; m < num_bins; ++m) {
        float energy = 0.0f;
        const auto& filter = mel_filter_bank_[m];
        int spec_bins = static_cast<int>(power_spec.size());
        int filter_len = static_cast<int>(filter.size());
        int n = std::min(spec_bins, filter_len);
        for (int k = 0; k < n; ++k) {
            energy += filter[k] * power_spec[k];
        }

        if (config_.use_log_fbank) {
            // 取对数；防止 log(0)，加最小正值
            constexpr float kEps = 1e-10f;
            mel_energy[m] = std::log(std::max(energy, kEps));
        } else {
            mel_energy[m] = energy;
        }
    }
    return mel_energy;
}

// ---------------------------------------------------------------------------

std::vector<std::vector<float>> FbankExtractor::stack_subframes(
    const std::vector<std::vector<float>>& mel_feats) const
{
    int n          = static_cast<int>(mel_feats.size());
    int stack      = config_.stack_frames;
    int stride     = config_.stack_stride;
    int mel_dim    = (n > 0) ? static_cast<int>(mel_feats[0].size()) : 0;
    int out_dim    = mel_dim * stack;

    if (stack <= 1) {
        // 不堆叠，直接返回
        return mel_feats;
    }

    // 输出帧数：每隔 stride 取一帧，窗口覆盖 stack 个子帧
    // 与 FunASR/SenseVoice 一致：输入帧两侧用边界帧填充
    // 实际输出帧数 = ceil((n) / stride)，这里 stride=1 时 out_n = n
    int out_n = (n + stride - 1) / stride;

    std::vector<std::vector<float>> result(out_n, std::vector<float>(out_dim, 0.0f));

    // 中心对齐堆叠：对于输出帧 i，堆叠原始帧 [i*stride - stack/2, ..., i*stride + stack/2]
    int half = stack / 2;
    for (int i = 0; i < out_n; ++i) {
        int center = i * stride;
        for (int s = 0; s < stack; ++s) {
            // 用边界值填充（clamp）
            int src = center - half + s;
            src = std::max(0, std::min(n - 1, src));
            const auto& src_frame = mel_feats[src];
            float* dst = result[i].data() + s * mel_dim;
            std::memcpy(dst, src_frame.data(), mel_dim * sizeof(float));
        }
    }
    return result;
}

// ============================================================================
// 辅助工具
// ============================================================================

int FbankExtractor::next_pow2(int n) {
    if (n <= 0) return 1;
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

float FbankExtractor::hz_to_mel(float hz) {
    return 2595.0f * std::log10(1.0f + hz / 700.0f);
}

float FbankExtractor::mel_to_hz(float mel) {
    return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
}

// ============================================================================
// Cooley-Tukey 原地 FFT（Radix-2，DIT）
// ============================================================================

void FbankExtractor::fft(std::vector<float>& re, std::vector<float>& im) {
    const int n = static_cast<int>(re.size());
    assert((n & (n - 1)) == 0 && "FFT size must be power of 2");
    im.assign(n, 0.0f);  // 确保虚部为零

    // ---- 位反转置换 ----
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            // im 全零，无需交换
        }
    }

    // ---- Cooley-Tukey 蝶形运算 ----
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * static_cast<float>(M_PI) / len;
        float wre = std::cos(ang);
        float wim = std::sin(ang);

        for (int i = 0; i < n; i += len) {
            float cur_re = 1.0f;
            float cur_im = 0.0f;

            for (int j = 0; j < len / 2; ++j) {
                float u_re = re[i + j];
                float u_im = im[i + j];
                float v_re = re[i + j + len / 2] * cur_re - im[i + j + len / 2] * cur_im;
                float v_im = re[i + j + len / 2] * cur_im + im[i + j + len / 2] * cur_re;

                re[i + j]             = u_re + v_re;
                im[i + j]             = u_im + v_im;
                re[i + j + len / 2]   = u_re - v_re;
                im[i + j + len / 2]   = u_im - v_im;

                // 旋转因子迭代：cur *= w
                float tmp = cur_re * wre - cur_im * wim;
                cur_im    = cur_re * wim + cur_im * wre;
                cur_re    = tmp;
            }
        }
    }
}

} // namespace ai
} // namespace synthorbis
