/**
 * @file ctc_decoder.h
 * @brief CTC 解码器接口
 *
 * 支持三种解码策略：
 *   1. Greedy   - 贪婪解码（最快，用于实时/低延迟场景）
 *   2. BeamSearch - CTC Prefix Beam Search（最常用，平衡准确率与速度）
 *   3. BeamSearchLM - Beam Search + n-gram LM 浅融合（最准，用于离线高质量识别）
 *
 * LM 融合方式（Shallow Fusion）：
 *   score = ctc_log_prob + lm_weight * lm_log_prob
 *
 * 参考：
 *   - Graves et al. "Connectionist Temporal Classification" (2006)
 *   - Hannun et al. "First-Pass Large Vocabulary Continuous Speech Recognition" (2014)
 *   - Hwang et al. "Character-level Incremental Speech Recognition" (2016)
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <limits>

namespace synthorbis {
namespace ai {

// ============================================================
// 基础类型
// ============================================================

/** @brief CTC 解码策略 */
enum class CtcDecodeMode {
    Greedy,         ///< 贪婪解码
    BeamSearch,     ///< CTC Prefix Beam Search（无 LM）
    BeamSearchLM,   ///< CTC Prefix Beam Search + n-gram LM 融合
};

/** @brief 单条解码假设 */
struct CtcHypothesis {
    std::string text;           ///< 解码文本
    float       ctc_score;      ///< CTC 对数概率之和
    float       lm_score;       ///< LM 对数概率之和（无 LM 时为 0）
    float       total_score;    ///< ctc_score + lm_weight * lm_score
    int         length;         ///< token 数量
};

/** @brief Beam Search 解码配置 */
struct CtcDecoderConfig {
    // ----- 通用 -----
    CtcDecodeMode mode          = CtcDecodeMode::BeamSearch;
    int           blank_id      = 0;    ///< CTC blank token id（SenseVoice 为 0）
    int           beam_size     = 10;   ///< Beam 宽度
    float         cutoff_prob   = 0.99f;///< 每帧只保留累积概率 >= cutoff 的 token
    int           cutoff_top_n  = 40;   ///< 每帧最多考虑 top-N 个 token

    // ----- LM 融合 -----
    float         lm_weight     = 0.3f; ///< LM 融合权重 (λ)
    float         word_bonus    = 0.0f; ///< 词完成奖励（鼓励输出完整词）
    bool          word_level    = false;///< true=词级别 LM；false=字符/BPE 级别 LM

    // ----- 输出 -----
    int           nbest         = 1;    ///< 返回 n-best 结果数量
};

// ============================================================
// n-gram LM 接口（可插拔，支持 ARPA / KenLM 实现）
// ============================================================

/**
 * @brief n-gram 语言模型抽象接口
 *
 * 使用者实现此接口后传给 CtcDecoder，
 * 也可使用内置的 UniformLM（所有 n-gram 概率为 0，退化为无 LM）。
 */
class ILanguageModel {
public:
    virtual ~ILanguageModel() = default;

    /**
     * @brief 给定 context（前缀），计算 token 的 log p(token | context)
     * @param context  前缀 token 序列（从 bos 开始）
     * @param token_id 当前 token id
     * @return 对数概率（以 e 为底）
     */
    virtual float score(const std::vector<int>& context, int token_id) const = 0;

    /** @brief 是否为词边界（用于 word_bonus） */
    virtual bool is_word_boundary(int token_id) const { return false; }
};

/**
 * @brief 虚 LM：所有概率为 0（退化为纯 CTC Beam Search）
 */
class NullLanguageModel : public ILanguageModel {
public:
    float score(const std::vector<int>&, int) const override { return 0.0f; }
};

/**
 * @brief 内置 ARPA n-gram LM（使用 back-off 的标准 ARPA 格式）
 *
 * 支持 2-gram / 3-gram / 4-gram；词表与 CTC 词表对齐（token id = word id）。
 * 不依赖 KenLM，纯 C++ 实现，适合嵌入端侧。
 */
class ArpaLanguageModel : public ILanguageModel {
public:
    ArpaLanguageModel() = default;

    /**
     * @brief 从 ARPA 格式文件加载
     * @param path  .arpa 文件路径（可以是 UTF-8 路径）
     * @return 0=成功，<0=失败
     */
    int load(const std::string& path);

    float score(const std::vector<int>& context, int token_id) const override;
    bool is_word_boundary(int token_id) const override;

    /** @brief LM 阶数 (1~4) */
    int order() const { return order_; }

    /** @brief 词表大小 */
    size_t vocab_size() const { return vocab_.size(); }

    /** @brief token -> word string */
    const std::string& id2word(int id) const;

    /** @brief word string -> token id，不存在返回 -1 */
    int word2id(const std::string& word) const;

private:
    struct NGramEntry {
        float log_prob;    ///< log10 概率
        float backoff;     ///< log10 back-off 权重（若无则为 0）
    };

    // key: context_ids + [token_id]，序列化为 uint64_t hash
    using NGramMap = std::unordered_map<std::size_t, NGramEntry>;

    std::vector<NGramMap> ngrams_;  ///< ngrams_[0]=unigram, [1]=bigram...
    std::vector<std::string> vocab_;///< id -> word
    std::unordered_map<std::string, int> word2id_; ///< word -> id
    int order_ = 0;

    // 内部：计算 hash key
    static std::size_t hash_ngram(const std::vector<int>& ids);

    // 内部：带 back-off 的递归查分
    float score_recursive(const std::vector<int>& context, int token_id,
                          int order) const;
};

// ============================================================
// CTC Prefix Beam Search 解码器
// ============================================================

/**
 * @brief CTC 解码器
 *
 * 用法：
 * @code
 *   CtcDecoderConfig cfg;
 *   cfg.mode = CtcDecodeMode::BeamSearch;
 *   cfg.beam_size = 10;
 *
 *   CtcDecoder decoder(cfg);
 *   decoder.set_vocab(vocab);   // vector<string>，token 0 = blank
 *
 *   auto results = decoder.decode(logits, time_steps, vocab_size);
 *   std::cout << results[0].text << "\n";
 * @endcode
 */
class CtcDecoder {
public:
    explicit CtcDecoder(const CtcDecoderConfig& config = {});
    ~CtcDecoder() = default;

    /** @brief 设置词表（token id → string） */
    void set_vocab(const std::vector<std::string>& vocab);

    /** @brief 设置 LM（可选，不设则使用 NullLanguageModel） */
    void set_lm(std::shared_ptr<ILanguageModel> lm);

    /**
     * @brief 解码
     * @param log_probs  CTC log-softmax 输出，shape [T, V]，按行优先展开
     * @param time_steps 帧数 T
     * @param vocab_size 词表大小 V
     * @return n-best 假设列表（按 total_score 降序）
     */
    std::vector<CtcHypothesis> decode(const float* log_probs,
                                      int time_steps,
                                      int vocab_size) const;

    /**
     * @brief 贪婪解码（最快，O(T*V) 时间复杂度）
     */
    CtcHypothesis greedy_decode(const float* log_probs,
                                int time_steps,
                                int vocab_size) const;

    /**
     * @brief Prefix Beam Search（无 LM）
     */
    std::vector<CtcHypothesis> beam_search(const float* log_probs,
                                           int time_steps,
                                           int vocab_size) const;

    /**
     * @brief Prefix Beam Search + LM Shallow Fusion
     */
    std::vector<CtcHypothesis> beam_search_lm(const float* log_probs,
                                              int time_steps,
                                              int vocab_size) const;

    const CtcDecoderConfig& config() const { return config_; }

private:
    CtcDecoderConfig config_;
    std::vector<std::string> vocab_;
    std::shared_ptr<ILanguageModel> lm_;

    // 内部：将 token id 序列转成字符串
    std::string ids_to_text(const std::vector<int>& ids) const;

    // 内部：log_sum_exp(a, b) = log(exp(a) + exp(b))
    static inline float log_sum_exp(float a, float b) {
        if (a == -std::numeric_limits<float>::infinity()) return b;
        if (b == -std::numeric_limits<float>::infinity()) return a;
        float max_ab = (a > b) ? a : b;
        return max_ab + std::log(std::exp(a - max_ab) + std::exp(b - max_ab));
    }

    static constexpr float kNegInf = -std::numeric_limits<float>::infinity();
};

} // namespace ai
} // namespace synthorbis
