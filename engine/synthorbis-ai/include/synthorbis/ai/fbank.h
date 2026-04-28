/**
 * @file fbank.h
 * @brief Fbank（滤波器组）特征提取
 *
 * 实现符合 Kaldi/ESPnet/FunASR 标准的 Fbank 特征提取：
 *   - 预加重 (Pre-emphasis)
 *   - 加窗 (Hamming window)
 *   - FFT 功率谱
 *   - 三角形 Mel 滤波器组
 *   - 对数压缩
 *
 * SenseVoice 模型输入规格：
 *   - 采样率: 16000 Hz
 *   - 帧长: 25ms (400 samples)
 *   - 帧移: 10ms (160 samples)
 *   - Mel 滤波器数: 80
 *   - 最终特征维度: 560 (80 * 7 子帧拼接)
 */

#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace synthorbis {
namespace ai {

/**
 * @brief Fbank 特征提取参数
 */
struct FbankConfig {
    // --- 基本参数 ---
    int sample_rate       = 16000;   // 采样率 (Hz)
    float frame_length_ms = 25.0f;   // 帧长 (ms)
    float frame_shift_ms  = 10.0f;   // 帧移 (ms)

    // --- Mel 滤波器组参数 ---
    int num_mel_bins      = 80;      // Mel 滤波器数量
    float low_freq        = 20.0f;   // 最低频率 (Hz)
    float high_freq       = -400.0f; // 最高频率 (Hz)，负值表示 sample_rate/2 + high_freq
    bool use_energy       = false;   // 是否使用能量特征替代第一个维度
    bool use_log_fbank    = true;    // 是否使用对数 Fbank
    bool remove_dc_offset = true;    // 是否去除直流偏移

    // --- 预加重 ---
    float preemph_coeff   = 0.97f;   // 预加重系数 (0 = 不预加重)

    // --- 归一化 ---
    bool apply_cmvn       = true;    // 是否应用全局 CMVN 归一化
    float mean_norm       = 0.0f;    // 全局均值
    float std_norm        = 1.0f;    // 全局标准差

    // --- SenseVoice 专用 ---
    int stack_frames      = 7;       // 子帧堆叠数量（7 * 80 = 560 维）
    int stack_stride      = 1;       // 堆叠步长
};

/**
 * @brief Fbank 特征提取结果
 */
struct FbankResult {
    std::vector<float> features;    // 特征数据 [num_frames, feature_dim]
    int num_frames   = 0;           // 帧数
    int feature_dim  = 0;           // 特征维度
};

/**
 * @brief Fbank 特征提取器
 *
 * 线程安全（内部无可变全局状态），实例可复用。
 */
class FbankExtractor {
public:
    /**
     * @brief 构造函数
     * @param config Fbank 参数，默认配置适配 SenseVoice-small
     */
    explicit FbankExtractor(const FbankConfig& config = FbankConfig{});

    /**
     * @brief 从原始 PCM 浮点数据提取 Fbank 特征
     *
     * @param pcm      PCM 数据，范围 [-1.0, 1.0]
     * @param num_samples 采样点数
     * @return 特征结果
     */
    FbankResult compute(const float* pcm, int num_samples) const;

    /**
     * @brief 从 PCM int16 数据提取特征（自动归一化为 float）
     */
    FbankResult compute_int16(const int16_t* pcm, int num_samples) const;

    /**
     * @brief 返回每帧的特征维度（含堆叠后）
     */
    int feature_dim() const { return config_.num_mel_bins * config_.stack_frames; }

    const FbankConfig& config() const { return config_; }

private:
    FbankConfig config_;

    // 预计算参数（构造时初始化）
    int frame_len_;       // 帧长（采样点数）
    int hop_len_;         // 帧移（采样点数）
    int fft_size_;        // FFT 点数（frame_len 向上取 2 的幂）
    float high_freq_;     // 真实高频上限

    std::vector<float>              hamming_window_;       // Hamming 窗
    std::vector<std::vector<float>> mel_filter_bank_;      // Mel 滤波器组 [num_mel, fft_size/2+1]

    // 内部方法
    void precompute_window();
    void precompute_mel_filters();

    static int next_pow2(int n);
    static float hz_to_mel(float hz);
    static float mel_to_hz(float mel);

    /**
     * @brief 对单帧做 FFT，返回功率谱 [fft_size/2+1]
     */
    std::vector<float> compute_power_spectrum(const std::vector<float>& frame) const;

    /**
     * @brief 应用 Mel 滤波器组，返回对数 Mel 能量 [num_mel_bins]
     */
    std::vector<float> apply_mel_filters(const std::vector<float>& power_spec) const;

    /**
     * @brief 堆叠相邻子帧
     * @param mel_feats [num_raw_frames, num_mel_bins]
     * @return [num_stacked_frames, num_mel_bins * stack_frames]
     */
    std::vector<std::vector<float>> stack_subframes(
        const std::vector<std::vector<float>>& mel_feats) const;

    /**
     * @brief Cooley-Tukey 原地 FFT（实数输入，复数输出）
     * @param re 实部（输入信号，原地替换为实部输出）
     * @param im 虚部（初始全零，原地替换为虚部输出）
     */
    static void fft(std::vector<float>& re, std::vector<float>& im);
};

} // namespace ai
} // namespace synthorbis
