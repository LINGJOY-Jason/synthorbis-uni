/**
 * @file test_ctc_decoder.cc
 * @brief CtcDecoder 单元测试
 *
 * 覆盖：
 *   1. Greedy 解码基本正确性
 *   2. Greedy 处理 blank 和重复 token
 *   3. BeamSearch 输出数量和排序
 *   4. BeamSearch 与 Greedy 结果一致性（beam=1 简单情形）
 *   5. n-best 结果数量正确
 *   6. 无词表时 ids_to_text 退化
 *   7. 全 blank 输入 -> 空字符串
 *   8. 单帧输入稳定性
 *   9. 大词表/长序列不崩溃（压力测试）
 *  10. LM（NullLM）融合模式不崩溃
 *  11. NullLanguageModel score = 0
 *  12. CtcDecoderConfig 默认值
 *  13. BeamSearchLM 与 BeamSearch 在 NullLM 下结果一致
 */

#include <gtest/gtest.h>
#include "synthorbis/ai/ctc_decoder.h"

#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <numeric>

using namespace synthorbis::ai;

// ============================================================
// 工具函数
// ============================================================

/**
 * 生成 log-softmax 输出。
 * 第 t 帧的最大 token = peak_tokens[t]，其余接近 -inf。
 */
static std::vector<float> make_log_probs(
    const std::vector<int>& peak_tokens,
    int vocab_size,
    float peak_logp = -0.01f,
    float other_logp = -10.0f)
{
    int T = static_cast<int>(peak_tokens.size());
    std::vector<float> lp(T * vocab_size, other_logp);
    for (int t = 0; t < T; ++t) {
        lp[t * vocab_size + peak_tokens[t]] = peak_logp;
    }
    return lp;
}

/**
 * 均匀分布 log-softmax（用于测试 beam 不崩溃）
 */
static std::vector<float> make_uniform_log_probs(int T, int V) {
    float log_val = -std::log(static_cast<float>(V));
    return std::vector<float>(T * V, log_val);
}

// ============================================================
// 测试组
// ============================================================

class CtcDecoderTest : public ::testing::Test {
protected:
    // 简单词表：0=blank, 1='a', 2='b', 3='c', 4='d'
    std::vector<std::string> vocab5 = {"<blank>", "a", "b", "c", "d"};

    // 贪婪解码器
    CtcDecoder make_greedy() {
        CtcDecoderConfig cfg;
        cfg.mode = CtcDecodeMode::Greedy;
        CtcDecoder dec(cfg);
        dec.set_vocab(vocab5);
        return dec;
    }

    // BeamSearch 解码器，beam=5
    CtcDecoder make_beam(int beam = 5, int nbest = 1) {
        CtcDecoderConfig cfg;
        cfg.mode      = CtcDecodeMode::BeamSearch;
        cfg.beam_size = beam;
        cfg.nbest     = nbest;
        CtcDecoder dec(cfg);
        dec.set_vocab(vocab5);
        return dec;
    }

    // BeamSearchLM 解码器（NullLM）
    CtcDecoder make_beam_lm(int beam = 5, int nbest = 1) {
        CtcDecoderConfig cfg;
        cfg.mode      = CtcDecodeMode::BeamSearchLM;
        cfg.beam_size = beam;
        cfg.nbest     = nbest;
        cfg.lm_weight = 0.3f;
        CtcDecoder dec(cfg);
        dec.set_vocab(vocab5);
        dec.set_lm(std::make_shared<NullLanguageModel>());
        return dec;
    }
};

// ============================================================
// 1. Greedy 解码基本正确性
// ============================================================
TEST_F(CtcDecoderTest, Greedy_BasicCorrectness) {
    // 序列：a b c  -> "abc"
    auto lp  = make_log_probs({1, 2, 3}, 5);
    auto dec = make_greedy();
    auto hyp = dec.greedy_decode(lp.data(), 3, 5);
    EXPECT_EQ(hyp.text, "abc");
    EXPECT_EQ(hyp.length, 3);
}

// ============================================================
// 2. Greedy 去除 blank 和重复
// ============================================================
TEST_F(CtcDecoderTest, Greedy_RemoveBlanksAndRepeat) {
    // 序列：a a blank a b blank b c
    // CTC 折叠：aa -> a, blank 分割 ab -> a b, bb -> b, bc -> c
    // 结果: "aabc" 经 CTC: a blank a b blank b c
    // a,a (合并) -> a; blank; a -> a(再出); b; blank; b (合并) -> b; c
    // 正确: "aabbc" 根据 CTC 规则：
    //   a a -> a (重复)
    //   blank
    //   a -> a (新 token，不重复 blank 后)
    //   b -> b
    //   blank
    //   b b -> b
    //   c -> c
    // 结果 "aabc" ... 让我们精确验证：
    // last=-1: a(1) != -1, != blank -> emit a; last=1
    // last=1:  a(1) == last -> skip; last=1
    // last=1:  blank(0) != 1, == blank -> skip; last=0
    // last=0:  a(1) != 0, != blank -> emit a; last=1
    // last=1:  b(2) != 1, != blank -> emit b; last=2
    // last=2:  blank(0) -> skip; last=0
    // last=0:  b(2) != 0, != blank -> emit b; last=2
    // last=2:  c(3) != 2, != blank -> emit c; last=3
    // 结果: "aabbc"
    auto lp  = make_log_probs({1, 1, 0, 1, 2, 0, 2, 3}, 5);
    auto dec = make_greedy();
    auto hyp = dec.greedy_decode(lp.data(), 8, 5);
    EXPECT_EQ(hyp.text, "aabbc");
}

// ============================================================
// 3. Greedy 全 blank -> 空字符串
// ============================================================
TEST_F(CtcDecoderTest, Greedy_AllBlank_EmptyText) {
    auto lp  = make_log_probs({0, 0, 0, 0}, 5);
    auto dec = make_greedy();
    auto hyp = dec.greedy_decode(lp.data(), 4, 5);
    EXPECT_EQ(hyp.text, "");
    EXPECT_EQ(hyp.length, 0);
}

// ============================================================
// 4. Greedy 单帧
// ============================================================
TEST_F(CtcDecoderTest, Greedy_SingleFrame) {
    auto lp  = make_log_probs({3}, 5);
    auto dec = make_greedy();
    auto hyp = dec.greedy_decode(lp.data(), 1, 5);
    EXPECT_EQ(hyp.text, "c");
    EXPECT_EQ(hyp.length, 1);
}

// ============================================================
// 5. BeamSearch 输出数量和排序
// ============================================================
TEST_F(CtcDecoderTest, BeamSearch_NBest_CountAndOrdering) {
    auto lp   = make_uniform_log_probs(5, 5);
    auto dec  = make_beam(5, 3);
    auto hyps = dec.decode(lp.data(), 5, 5);
    EXPECT_GE(static_cast<int>(hyps.size()), 1);
    EXPECT_LE(static_cast<int>(hyps.size()), 3);

    // 验证降序排列
    for (int i = 1; i < static_cast<int>(hyps.size()); ++i) {
        EXPECT_GE(hyps[i-1].total_score, hyps[i].total_score)
            << "n-best 结果未按 score 降序排列";
    }
}

// ============================================================
// 6. BeamSearch 确定性序列
// ============================================================
TEST_F(CtcDecoderTest, BeamSearch_DeterministicSequence) {
    // 强峰序列：a b c -> beam search 应找到 "abc"
    auto lp   = make_log_probs({1, 2, 3}, 5, -0.001f, -20.0f);
    auto dec  = make_beam(5, 1);
    auto hyps = dec.decode(lp.data(), 3, 5);
    ASSERT_FALSE(hyps.empty());
    EXPECT_EQ(hyps[0].text, "abc");
}

// ============================================================
// 7. BeamSearch 全 blank
// ============================================================
TEST_F(CtcDecoderTest, BeamSearch_AllBlank_EmptyText) {
    auto lp   = make_log_probs({0, 0, 0, 0, 0}, 5);
    auto dec  = make_beam(5, 1);
    auto hyps = dec.decode(lp.data(), 5, 5);
    ASSERT_FALSE(hyps.empty());
    EXPECT_EQ(hyps[0].text, "");
}

// ============================================================
// 8. Greedy vs BeamSearch beam=1 一致性（简单强峰）
// ============================================================
TEST_F(CtcDecoderTest, BeamSearch_Beam1_MatchGreedy_SimplePeak) {
    // 使用更大的峰值差异确保 beam=1 与 greedy 一致
    auto lp    = make_log_probs({1, 2, 1, 3}, 5, -0.001f, -30.0f);

    CtcDecoderConfig greedy_cfg;
    greedy_cfg.mode = CtcDecodeMode::Greedy;
    CtcDecoder greedy_dec(greedy_cfg);
    greedy_dec.set_vocab(vocab5);

    CtcDecoderConfig beam_cfg;
    beam_cfg.mode      = CtcDecodeMode::BeamSearch;
    beam_cfg.beam_size = 1;
    beam_cfg.nbest     = 1;
    CtcDecoder beam_dec(beam_cfg);
    beam_dec.set_vocab(vocab5);

    auto g_hyp  = greedy_dec.greedy_decode(lp.data(), 4, 5);
    auto b_hyps = beam_dec.decode(lp.data(), 4, 5);

    ASSERT_FALSE(b_hyps.empty());
    EXPECT_EQ(g_hyp.text, b_hyps[0].text)
        << "beam=1 时与 greedy 结果不一致（强峰序列）";
}

// ============================================================
// 9. NullLanguageModel score = 0
// ============================================================
TEST_F(CtcDecoderTest, NullLM_ScoreIsZero) {
    NullLanguageModel lm;
    EXPECT_FLOAT_EQ(lm.score({}, 1), 0.0f);
    EXPECT_FLOAT_EQ(lm.score({1, 2, 3}, 4), 0.0f);
}

// ============================================================
// 10. BeamSearchLM (NullLM) 不崩溃，结果合理
// ============================================================
TEST_F(CtcDecoderTest, BeamSearchLM_NullLM_NoCrash) {
    auto lp   = make_log_probs({1, 2, 3}, 5, -0.01f, -10.0f);
    auto dec  = make_beam_lm(5, 1);
    EXPECT_NO_THROW({
        auto hyps = dec.decode(lp.data(), 3, 5);
        ASSERT_FALSE(hyps.empty());
        EXPECT_EQ(hyps[0].text, "abc");
    });
}

// ============================================================
// 11. BeamSearchLM (NullLM) vs BeamSearch 结果一致
// ============================================================
TEST_F(CtcDecoderTest, BeamSearchLM_SameAsBeamSearch_UnderNullLM) {
    auto lp = make_log_probs({1, 0, 2, 0, 3}, 5, -0.001f, -20.0f);

    auto bs_dec  = make_beam(10, 1);
    auto bslm_dec = make_beam_lm(10, 1);

    auto bs_hyps   = bs_dec.decode(lp.data(), 5, 5);
    auto bslm_hyps = bslm_dec.decode(lp.data(), 5, 5);

    ASSERT_FALSE(bs_hyps.empty());
    ASSERT_FALSE(bslm_hyps.empty());
    EXPECT_EQ(bs_hyps[0].text, bslm_hyps[0].text)
        << "NullLM 下 BeamSearchLM 应与 BeamSearch 文本结果一致";
}

// ============================================================
// 12. 无词表时 decode 不崩溃
// ============================================================
TEST_F(CtcDecoderTest, NoVocab_DecodeNoCrash) {
    CtcDecoderConfig cfg;
    cfg.mode = CtcDecodeMode::BeamSearch;
    CtcDecoder dec(cfg);  // 不 set_vocab

    auto lp = make_log_probs({1, 2, 3}, 5);
    EXPECT_NO_THROW({
        auto hyps = dec.decode(lp.data(), 3, 5);
        ASSERT_FALSE(hyps.empty());
        // 无词表时文本为 "1 2 3"
        EXPECT_EQ(hyps[0].text, "1 2 3");
    });
}

// ============================================================
// 13. 压力测试：大词表 + 长序列不崩溃
// ============================================================
TEST_F(CtcDecoderTest, StressTest_LargeVocab_LongSequence) {
    const int V = 25055;  // SenseVoice 词表大小
    const int T = 100;
    const int beam = 5;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-10.0f, 0.0f);

    std::vector<float> lp(T * V);
    for (auto& v : lp) v = dist(rng);

    // log-softmax 归一化（近似）
    for (int t = 0; t < T; ++t) {
        float max_v = *std::max_element(lp.begin() + t*V, lp.begin() + (t+1)*V);
        float sum = 0.0f;
        for (int v = 0; v < V; ++v) sum += std::exp(lp[t*V + v] - max_v);
        float log_sum = max_v + std::log(sum);
        for (int v = 0; v < V; ++v) lp[t*V + v] -= log_sum;
    }

    CtcDecoderConfig cfg;
    cfg.mode         = CtcDecodeMode::BeamSearch;
    cfg.beam_size    = beam;
    cfg.cutoff_top_n = 40;
    cfg.nbest        = 3;
    CtcDecoder dec(cfg);

    EXPECT_NO_THROW({
        auto hyps = dec.decode(lp.data(), T, V);
        EXPECT_LE(static_cast<int>(hyps.size()), 3);
    });
}

// ============================================================
// 14. CtcDecoderConfig 默认值验证
// ============================================================
TEST_F(CtcDecoderTest, DefaultConfig_Sanity) {
    CtcDecoderConfig cfg;
    EXPECT_EQ(cfg.mode,       CtcDecodeMode::BeamSearch);
    EXPECT_EQ(cfg.blank_id,   0);
    EXPECT_EQ(cfg.beam_size,  10);
    EXPECT_FLOAT_EQ(cfg.lm_weight, 0.3f);
    EXPECT_EQ(cfg.nbest,      1);
}

// ============================================================
// 15. BeamSearch 处理 SentencePiece ▁ 前缀词表
// ============================================================
TEST(CtcDecoderSPMTest, SentencePiece_WordBoundary) {
    // 词表：0=blank, 1=▁你, 2=好, 3=▁我, 4=爱
    std::vector<std::string> spm_vocab = {
        "<blank>",
        "\xe2\x96\x81\xe4\xbd\xa0",  // ▁你
        "\xe5\xa5\xbd",              // 好
        "\xe2\x96\x81\xe6\x88\x91",  // ▁我
        "\xe7\x88\xb1",              // 爱
    };

    CtcDecoderConfig cfg;
    cfg.mode      = CtcDecodeMode::BeamSearch;
    cfg.beam_size = 5;
    cfg.nbest     = 1;
    CtcDecoder dec(cfg);
    dec.set_vocab(spm_vocab);

    // 序列：▁你 好 ▁我 爱 -> "你好 我爱"
    auto lp   = make_log_probs({1, 2, 3, 4}, 5, -0.001f, -20.0f);
    auto hyps = dec.decode(lp.data(), 4, 5);
    ASSERT_FALSE(hyps.empty());
    // ▁你 -> "你"（首词无空格），好 -> "好"，▁我 -> " 我"，爱 -> "爱"
    // ids_to_text: 首词 ▁ 去掉，非首词 ▁ 替换为空格
    EXPECT_EQ(hyps[0].text, "你好 我爱");
}
