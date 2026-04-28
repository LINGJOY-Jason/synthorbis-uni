/**
 * @file ctc_decoder.cc
 * @brief CTC 解码器实现
 *
 * 包含：
 *   - CtcDecoder::greedy_decode  : 贪婪解码 O(T*V)
 *   - CtcDecoder::beam_search    : CTC Prefix Beam Search（无 LM）
 *   - CtcDecoder::beam_search_lm : CTC Prefix Beam Search + ARPA LM 浅融合
 *   - ArpaLanguageModel          : 纯 C++ ARPA n-gram LM
 *
 * Beam Search 算法参考：
 *   Hannun et al. (2014) "First-Pass Large Vocabulary Continuous Speech
 *   Recognition using Sequence-to-Sequence Models"
 *   https://arxiv.org/abs/1408.2873
 */

#include "synthorbis/ai/ctc_decoder.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace synthorbis {
namespace ai {

// ============================================================
// CtcDecoder
// ============================================================

CtcDecoder::CtcDecoder(const CtcDecoderConfig& config)
    : config_(config),
      lm_(std::make_shared<NullLanguageModel>()) {}

void CtcDecoder::set_vocab(const std::vector<std::string>& vocab) {
    vocab_ = vocab;
}

void CtcDecoder::set_lm(std::shared_ptr<ILanguageModel> lm) {
    if (lm) lm_ = lm;
}

// ---- 主入口 ----

std::vector<CtcHypothesis> CtcDecoder::decode(const float* log_probs,
                                               int time_steps,
                                               int vocab_size) const {
    switch (config_.mode) {
        case CtcDecodeMode::Greedy:
            return {greedy_decode(log_probs, time_steps, vocab_size)};
        case CtcDecodeMode::BeamSearch:
            return beam_search(log_probs, time_steps, vocab_size);
        case CtcDecodeMode::BeamSearchLM:
            return beam_search_lm(log_probs, time_steps, vocab_size);
    }
    return {greedy_decode(log_probs, time_steps, vocab_size)};
}

// ============================================================
// 1. 贪婪解码
// ============================================================

CtcHypothesis CtcDecoder::greedy_decode(const float* log_probs,
                                         int time_steps,
                                         int vocab_size) const {
    CtcHypothesis hyp;
    hyp.ctc_score = 0.0f;
    hyp.lm_score  = 0.0f;
    hyp.length    = 0;

    std::vector<int> token_ids;
    int prev_token = -1;

    for (int t = 0; t < time_steps; ++t) {
        const float* row = log_probs + t * vocab_size;

        // argmax
        int best = 0;
        float best_logp = row[0];
        for (int v = 1; v < vocab_size; ++v) {
            if (row[v] > best_logp) {
                best_logp = row[v];
                best      = v;
            }
        }

        hyp.ctc_score += best_logp;

        // CTC 去重 + 去 blank
        if (best != prev_token && best != config_.blank_id) {
            token_ids.push_back(best);
        }
        prev_token = best;
    }

    hyp.text        = ids_to_text(token_ids);
    hyp.total_score = hyp.ctc_score;
    hyp.length      = static_cast<int>(token_ids.size());
    return hyp;
}

// ============================================================
// 2. CTC Prefix Beam Search（无 LM）
// ============================================================

/**
 * 每条 Beam 携带两个 log 概率：
 *   p_nb : 前缀以"非 blank"结尾的概率
 *   p_b  : 前缀以"blank"结尾的概率
 * 合并概率 p_total = log_sum_exp(p_nb, p_b)
 */

namespace {

struct BeamEntry {
    std::vector<int> prefix;  ///< prefix token id 序列（不含 blank）
    float p_nb;               ///< log P(prefix 以非 blank 结尾)
    float p_b;                ///< log P(prefix 以 blank 结尾)
    float lm_score;           ///< LM 累计分（仅 LM 模式使用）

    float total_log_prob() const {
        // log(exp(p_nb) + exp(p_b))
        if (p_nb == -std::numeric_limits<float>::infinity()) return p_b;
        if (p_b  == -std::numeric_limits<float>::infinity()) return p_nb;
        float m = std::max(p_nb, p_b);
        return m + std::log(std::exp(p_nb - m) + std::exp(p_b - m));
    }
};

using BeamMap = std::map<std::vector<int>, BeamEntry>;

}  // namespace

std::vector<CtcHypothesis> CtcDecoder::beam_search(const float* log_probs,
                                                    int time_steps,
                                                    int vocab_size) const {
    static constexpr float kNegInf = -std::numeric_limits<float>::infinity();

    const int blank_id  = config_.blank_id;
    const int beam_size = config_.beam_size;

    // 初始 beam：空前缀，以 blank 结尾概率 = 0（log=0），以非 blank 结尾 = -inf
    BeamMap beams;
    beams[{}] = BeamEntry{{}, kNegInf, 0.0f, 0.0f};

    for (int t = 0; t < time_steps; ++t) {
        const float* row = log_probs + t * vocab_size;

        // --- 剪枝：只处理 top-N 概率的 token ---
        std::vector<int> top_tokens(vocab_size);
        std::iota(top_tokens.begin(), top_tokens.end(), 0);

        int cutoff_n = std::min(config_.cutoff_top_n, vocab_size);
        std::partial_sort(top_tokens.begin(),
                          top_tokens.begin() + cutoff_n,
                          top_tokens.end(),
                          [&](int a, int b){ return row[a] > row[b]; });
        top_tokens.resize(cutoff_n);

        // 累积概率剪枝
        if (config_.cutoff_prob < 1.0f) {
            float log_cutoff = std::log(config_.cutoff_prob);
            float log_sum    = kNegInf;
            std::vector<int> pruned;
            for (int v : top_tokens) {
                pruned.push_back(v);
                log_sum = log_sum_exp(log_sum, row[v]);
                if (log_sum >= log_cutoff) break;
            }
            top_tokens = std::move(pruned);
        }

        BeamMap new_beams;

        for (auto& [prefix, beam] : beams) {
            float p_total = beam.total_log_prob();

            for (int v : top_tokens) {
                float logp = row[v];

                if (v == blank_id) {
                    // blank -> 延续当前前缀（只更新 p_b）
                    auto& nb = new_beams[prefix];
                    nb.prefix  = prefix;
                    nb.lm_score = beam.lm_score;
                    nb.p_b  = log_sum_exp(nb.p_b,  p_total + logp);
                    nb.p_nb = (nb.p_nb == 0.0f && nb.prefix.empty()
                                ? kNegInf : nb.p_nb);
                    continue;
                }

                // 扩展前缀
                bool same_as_last = (!prefix.empty() && prefix.back() == v);

                // Case A：如果 v == 上一个 token，只能通过 blank 跳转
                //   new_prefix = prefix, p_nb += p_b * logp
                if (same_as_last) {
                    auto& nb = new_beams[prefix];
                    nb.prefix   = prefix;
                    nb.lm_score = beam.lm_score;
                    nb.p_nb = log_sum_exp(nb.p_nb, beam.p_b + logp);
                    nb.p_b  = (nb.p_b == 0.0f && nb.p_nb == kNegInf
                                ? kNegInf : nb.p_b);
                }

                // Case B：新扩展前缀
                std::vector<int> new_prefix = prefix;
                new_prefix.push_back(v);

                auto& nb_new = new_beams[new_prefix];
                nb_new.prefix = new_prefix;
                nb_new.lm_score = beam.lm_score;  // LM 模式再更新

                // 如果 v != last，可通过 p_nb + p_b；如果 v == last，只通过 p_nb
                float extend_score = same_as_last ? beam.p_b : p_total;
                nb_new.p_nb = log_sum_exp(nb_new.p_nb, extend_score + logp);
            }
        }

        // --- 修正：确保所有 entry 的 p_b 初始化 ---
        for (auto& [k, entry] : new_beams) {
            if (entry.p_b == 0.0f && entry.p_nb == kNegInf) {
                // p_b 未被设置过，表示这条新前缀没有以 blank 结尾路径
                entry.p_b = kNegInf;
            }
        }

        // --- 剪枝：保留 top-beam_size 条 ---
        if (static_cast<int>(new_beams.size()) > beam_size) {
            std::vector<BeamMap::iterator> iters;
            iters.reserve(new_beams.size());
            for (auto it = new_beams.begin(); it != new_beams.end(); ++it)
                iters.push_back(it);

            std::partial_sort(iters.begin(),
                              iters.begin() + beam_size,
                              iters.end(),
                              [](const BeamMap::iterator& a,
                                 const BeamMap::iterator& b) {
                                  return a->second.total_log_prob()
                                       > b->second.total_log_prob();
                              });

            BeamMap pruned;
            for (int i = 0; i < beam_size; ++i)
                pruned[iters[i]->first] = std::move(iters[i]->second);
            beams = std::move(pruned);
        } else {
            beams = std::move(new_beams);
        }
    }

    // --- 整理输出 ---
    int nbest = std::min(config_.nbest, static_cast<int>(beams.size()));
    std::vector<BeamMap::iterator> iters;
    iters.reserve(beams.size());
    for (auto it = beams.begin(); it != beams.end(); ++it)
        iters.push_back(it);

    std::sort(iters.begin(), iters.end(),
              [](const BeamMap::iterator& a, const BeamMap::iterator& b) {
                  return a->second.total_log_prob() > b->second.total_log_prob();
              });

    std::vector<CtcHypothesis> results;
    results.reserve(nbest);
    for (int i = 0; i < nbest; ++i) {
        const auto& b = iters[i]->second;
        CtcHypothesis hyp;
        hyp.text        = ids_to_text(b.prefix);
        hyp.ctc_score   = b.total_log_prob();
        hyp.lm_score    = 0.0f;
        hyp.total_score = hyp.ctc_score;
        hyp.length      = static_cast<int>(b.prefix.size());
        results.push_back(std::move(hyp));
    }
    return results;
}

// ============================================================
// 3. CTC Prefix Beam Search + LM Shallow Fusion
// ============================================================

std::vector<CtcHypothesis> CtcDecoder::beam_search_lm(const float* log_probs,
                                                       int time_steps,
                                                       int vocab_size) const {
    static constexpr float kNegInf = -std::numeric_limits<float>::infinity();

    const int   blank_id  = config_.blank_id;
    const int   beam_size = config_.beam_size;
    const float lm_w      = config_.lm_weight;
    const float word_bon  = config_.word_bonus;

    // 初始 beam
    BeamMap beams;
    beams[{}] = BeamEntry{{}, kNegInf, 0.0f, 0.0f};

    for (int t = 0; t < time_steps; ++t) {
        const float* row = log_probs + t * vocab_size;

        // top-N 剪枝
        std::vector<int> top_tokens(vocab_size);
        std::iota(top_tokens.begin(), top_tokens.end(), 0);
        int cutoff_n = std::min(config_.cutoff_top_n, vocab_size);
        std::partial_sort(top_tokens.begin(),
                          top_tokens.begin() + cutoff_n,
                          top_tokens.end(),
                          [&](int a, int b){ return row[a] > row[b]; });
        top_tokens.resize(cutoff_n);

        BeamMap new_beams;

        for (auto& [prefix, beam] : beams) {
            float p_total = beam.total_log_prob();

            for (int v : top_tokens) {
                float logp = row[v];

                if (v == blank_id) {
                    auto& nb   = new_beams[prefix];
                    nb.prefix  = prefix;
                    nb.lm_score = beam.lm_score;
                    nb.p_b  = log_sum_exp(nb.p_b, p_total + logp);
                    if (nb.p_nb == 0.0f && nb.prefix.empty()) nb.p_nb = kNegInf;
                    continue;
                }

                bool same_as_last = (!prefix.empty() && prefix.back() == v);

                // Case A：相同 token 通过 blank
                if (same_as_last) {
                    auto& nb   = new_beams[prefix];
                    nb.prefix  = prefix;
                    nb.lm_score = beam.lm_score;
                    nb.p_nb = log_sum_exp(nb.p_nb, beam.p_b + logp);
                }

                // Case B：扩展新前缀，加 LM 分
                std::vector<int> new_prefix = prefix;
                new_prefix.push_back(v);

                // LM 评分
                float lm_logp = lm_->score(prefix, v);   // log P(v | prefix)

                // 词边界奖励
                float bonus = 0.0f;
                if (word_bon != 0.0f && lm_->is_word_boundary(v)) {
                    bonus = word_bon;
                }

                auto& nb_new    = new_beams[new_prefix];
                nb_new.prefix   = new_prefix;
                nb_new.lm_score = beam.lm_score + lm_logp + bonus;

                float extend_score = same_as_last ? beam.p_b : p_total;
                // CTC 分 + LM 分（融合进 p_nb）
                float combined = extend_score + logp + lm_w * (lm_logp + bonus);
                nb_new.p_nb = log_sum_exp(nb_new.p_nb, combined);
            }
        }

        // 修正未初始化的 p_b
        for (auto& [k, entry] : new_beams) {
            if (entry.p_b == 0.0f && entry.p_nb == kNegInf) {
                entry.p_b = kNegInf;
            }
        }

        // beam 剪枝（按 total_log_prob 排序，LM 已融入 p_nb）
        if (static_cast<int>(new_beams.size()) > beam_size) {
            std::vector<BeamMap::iterator> iters;
            iters.reserve(new_beams.size());
            for (auto it = new_beams.begin(); it != new_beams.end(); ++it)
                iters.push_back(it);

            std::partial_sort(iters.begin(),
                              iters.begin() + beam_size,
                              iters.end(),
                              [](const BeamMap::iterator& a,
                                 const BeamMap::iterator& b) {
                                  return a->second.total_log_prob()
                                       > b->second.total_log_prob();
                              });

            BeamMap pruned;
            for (int i = 0; i < beam_size; ++i)
                pruned[iters[i]->first] = std::move(iters[i]->second);
            beams = std::move(pruned);
        } else {
            beams = std::move(new_beams);
        }
    }

    // 整理 n-best 输出
    int nbest = std::min(config_.nbest, static_cast<int>(beams.size()));
    std::vector<BeamMap::iterator> iters;
    iters.reserve(beams.size());
    for (auto it = beams.begin(); it != beams.end(); ++it)
        iters.push_back(it);

    std::sort(iters.begin(), iters.end(),
              [](const BeamMap::iterator& a, const BeamMap::iterator& b) {
                  return a->second.total_log_prob() > b->second.total_log_prob();
              });

    std::vector<CtcHypothesis> results;
    results.reserve(nbest);
    for (int i = 0; i < nbest; ++i) {
        const auto& b = iters[i]->second;
        CtcHypothesis hyp;
        hyp.text        = ids_to_text(b.prefix);
        hyp.ctc_score   = b.total_log_prob();
        hyp.lm_score    = b.lm_score;
        hyp.total_score = hyp.ctc_score + lm_w * hyp.lm_score;
        hyp.length      = static_cast<int>(b.prefix.size());
        results.push_back(std::move(hyp));
    }
    return results;
}

// ============================================================
// 辅助：token ids -> string
// ============================================================

std::string CtcDecoder::ids_to_text(const std::vector<int>& ids) const {
    if (vocab_.empty()) {
        // 无词表：用 id 拼接（调试用）
        std::string s;
        for (int id : ids) {
            if (!s.empty()) s += ' ';
            s += std::to_string(id);
        }
        return s;
    }
    std::string text;
    for (int id : ids) {
        if (id >= 0 && id < static_cast<int>(vocab_.size())) {
            const std::string& tok = vocab_[id];
            // SentencePiece / BPE 前缀 '▁'（U+2581）表示词边界（空格）
            if (!tok.empty() && tok[0] == '\xe2' &&
                tok.size() >= 3 && tok[1] == '\x96' && tok[2] == '\x81') {
                // ▁ 开头：替换为空格
                if (!text.empty()) text += ' ';
                text += tok.substr(3);  // 去掉 ▁
            } else if (tok == "<blank>" || tok == "<blk>" || tok == "<eps>") {
                // 跳过 blank / eps
            } else if (tok.size() >= 2 && tok[0] == '<' && tok.back() == '>') {
                // 其它特殊 token 跳过（<sos>, <eos> 等）
            } else {
                text += tok;
            }
        }
    }
    return text;
}

// ============================================================
// ArpaLanguageModel 实现
// ============================================================

// ---- hash_ngram ----

std::size_t ArpaLanguageModel::hash_ngram(const std::vector<int>& ids) {
    // FNV-1a hash
    std::size_t h = 14695981039346656037ULL;
    for (int id : ids) {
        h ^= static_cast<std::size_t>(id);
        h *= 1099511628211ULL;
    }
    return h;
}

// ---- load ----

int ArpaLanguageModel::load(const std::string& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) return -1;

    enum class Section { None, Header, NGram };
    Section section   = Section::None;
    int     cur_order = 0;

    std::string line;

    // 第一遍：收集 vocab（unigram 段的词）
    // 直接单遍解析：先读 \data\ 段确定阶数和大小
    std::vector<int> counts;  // counts[n] = n+1 gram 数量

    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line == "\\data\\") {
            section = Section::Header;
            continue;
        }
        if (section == Section::Header) {
            if (line.rfind("ngram ", 0) == 0) {
                // "ngram N=M"
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    int n  = std::stoi(line.substr(6, eq - 6));
                    int m  = std::stoi(line.substr(eq + 1));
                    if (n > static_cast<int>(counts.size()))
                        counts.resize(n, 0);
                    counts[n - 1] = m;
                    if (n > order_) order_ = n;
                }
            }
        }
        if (line == "\\end\\") break;
    }

    if (order_ == 0) return -2;

    ngrams_.resize(order_);
    for (int i = 0; i < order_; ++i) {
        if (i < static_cast<int>(counts.size()))
            ngrams_[i].reserve(counts[i]);
    }

    // 第二遍：重新从头读取
    fin.clear();
    fin.seekg(0);
    section = Section::None;

    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;

        // 检查是否是 \N-grams: 段
        if (line[0] == '\\') {
            if (line == "\\end\\") break;
            if (line == "\\data\\") { section = Section::Header; continue; }
            // "\N-grams:"
            auto dash = line.find('-');
            if (dash != std::string::npos) {
                cur_order = std::stoi(line.substr(1, dash - 1));
                section   = Section::NGram;
            }
            continue;
        }

        if (section == Section::Header) continue;

        if (section == Section::NGram && cur_order >= 1) {
            // 格式：log_prob  w1 [w2 ...] [backoff]
            std::istringstream ss(line);
            float log_prob = 0.0f;
            ss >> log_prob;

            std::vector<std::string> words;
            std::string w;
            while (ss >> w) words.push_back(w);

            // 最后一列可能是 backoff
            float backoff = 0.0f;
            bool has_backoff = false;
            if (static_cast<int>(words.size()) > cur_order) {
                // 尝试解析最后一个词为浮点
                try {
                    backoff = std::stof(words.back());
                    has_backoff = true;
                    words.pop_back();
                } catch (...) {}
            }

            if (static_cast<int>(words.size()) != cur_order) continue;

            // 注册 unigram 词表
            if (cur_order == 1) {
                int id = static_cast<int>(vocab_.size());
                vocab_.push_back(words[0]);
                word2id_[words[0]] = id;
            }

            // 构建 key
            std::vector<int> ngram_ids;
            ngram_ids.reserve(cur_order);
            for (const auto& word : words) {
                auto it = word2id_.find(word);
                if (it == word2id_.end()) {
                    // 词不在词表（unigram 未见），用 -1 占位
                    ngram_ids.push_back(-1);
                } else {
                    ngram_ids.push_back(it->second);
                }
            }

            NGramEntry entry;
            entry.log_prob = log_prob;
            entry.backoff  = has_backoff ? backoff : 0.0f;

            ngrams_[cur_order - 1][hash_ngram(ngram_ids)] = entry;
        }
    }

    return 0;
}

// ---- score ----

float ArpaLanguageModel::score(const std::vector<int>& context,
                                int token_id) const {
    if (order_ == 0) return 0.0f;
    return score_recursive(context, token_id, order_);
}

float ArpaLanguageModel::score_recursive(const std::vector<int>& context,
                                          int token_id,
                                          int order) const {
    // 取最近 order-1 个词作为 context
    int ctx_len = order - 1;
    std::vector<int> ids;
    ids.reserve(order);

    if (static_cast<int>(context.size()) >= ctx_len) {
        ids.assign(context.end() - ctx_len, context.end());
    } else {
        ids = context;
    }
    ids.push_back(token_id);

    // 查找 n-gram
    auto& map = ngrams_[static_cast<size_t>(ids.size()) - 1];
    auto  it  = map.find(hash_ngram(ids));
    if (it != map.end()) {
        // 找到：返回 log10 * log(10) 转为 log_e
        return it->second.log_prob * 2.302585f;  // log10 -> loge
    }

    // 未找到：back-off
    if (ids.size() <= 1) {
        // unigram 未找到：返回 UNK 概率（近似为 -inf 的安全值）
        return -20.0f * 2.302585f;
    }

    // 查 context 的 back-off 权重
    std::vector<int> ctx_ids(ids.begin(), ids.end() - 1);
    float backoff = 0.0f;
    if (ctx_ids.size() >= 1) {
        auto& ctx_map = ngrams_[ctx_ids.size() - 1];
        auto  ctx_it  = ctx_map.find(hash_ngram(ctx_ids));
        if (ctx_it != ctx_map.end()) {
            backoff = ctx_it->second.backoff * 2.302585f;
        }
    }

    // 递归低阶
    std::vector<int> shorter_ctx(ids.begin() + 1, ids.end() - 1);
    return backoff + score_recursive(shorter_ctx, token_id,
                                     static_cast<int>(ids.size()) - 1);
}

// ---- is_word_boundary ----

bool ArpaLanguageModel::is_word_boundary(int token_id) const {
    if (token_id < 0 || token_id >= static_cast<int>(vocab_.size()))
        return false;
    const auto& tok = vocab_[token_id];
    // SentencePiece: ▁ 开头表示新词
    if (tok.size() >= 3 && tok[0] == '\xe2' &&
        tok[1] == '\x96' && tok[2] == '\x81')
        return true;
    // 传统词表：以空格 " " 结尾（罕见，但兼容）
    return (!tok.empty() && tok.back() == ' ');
}

// ---- id2word / word2id ----

const std::string& ArpaLanguageModel::id2word(int id) const {
    static const std::string kUnk = "<unk>";
    if (id < 0 || id >= static_cast<int>(vocab_.size())) return kUnk;
    return vocab_[id];
}

int ArpaLanguageModel::word2id(const std::string& word) const {
    auto it = word2id_.find(word);
    return (it == word2id_.end()) ? -1 : it->second;
}

} // namespace ai
} // namespace synthorbis
