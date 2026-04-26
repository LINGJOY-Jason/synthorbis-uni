// SynthOrbis Candidates — 候选词列表管理
//
// 处理输入法候选词生成、排序和显示

#ifndef SYNTHORBIS_CANDIDATES_H_
#define SYNTHORBIS_CANDIDATES_H_

#include "synthorbis/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 候选词类型
typedef enum {
    SYNTH_CANDIDATE_SIMPLIFIED = 0,     // 简体
    SYNTH_CANDIDATE_TRADITIONAL = 1,    // 繁体
    SYNTH_CANDIDATE_STROKE = 2,         // 笔画
    SYNTH_CANDIDATE_PHONETIC = 3,       // 拼音/注音
    SYNTH_CANDIDATE_SYMBOL = 4,         // 符号
    SYNTH_CANDIDATE_PHRASE = 5,         // 短语
    SYNTH_CANDIDATE_CLOUD = 6           // 云端预测
} SynthCandidateType;

// 候选词
typedef struct {
    const char* text;                   // 候选文本
    const char* comment;                 // 注释（拼音、词频等）
    const char* extra_data;             // 额外数据（JSON 格式）
    SynthCandidateType type;            // 类型
    float score;                        // 得分/词频
    int index;                          // 索引
    int is_selected;                    // 是否选中
} SynthCandidate;

// 候选词列表
typedef struct {
    SynthCandidate* candidates;         // 候选词数组
    int count;                          // 候选词数量
    int page_start;                     // 当前页起始索引
    int page_size;                      // 每页大小
    const char* input_text;             // 输入文本
} SynthCandidateList;

// 候选词操作函数

// 创建候选词列表
SYNTHORBIS_API SynthCandidateList* synth_candidate_list_create(void);

// 销毁候选词列表
SYNTHORBIS_API void synth_candidate_list_destroy(SynthCandidateList* list);

// 添加候选词
SYNTHORBIS_API void synth_candidate_list_add(
    SynthCandidateList* list,
    const char* text,
    const char* comment,
    SynthCandidateType type,
    float score);

// 清空候选词列表
SYNTHORBIS_API void synth_candidate_list_clear(SynthCandidateList* list);

// 获取候选词数量
SYNTHORBIS_API int synth_candidate_list_size(const SynthCandidateList* list);

// 获取指定索引的候选词
SYNTHORBIS_API const SynthCandidate* synth_candidate_list_get(
    const SynthCandidateList* list,
    int index);

// 设置分页信息
SYNTHORBIS_API void synth_candidate_list_set_page(
    SynthCandidateList* list,
    int page_start,
    int page_size);

// 排序候选词（按 score 降序）
SYNTHORBIS_API void synth_candidate_list_sort(SynthCandidateList* list);

// 筛选特定类型的候选词
SYNTHORBIS_API void synth_candidate_list_filter_type(
    SynthCandidateList* list,
    SynthCandidateType type);

// 获取当前页的候选词
SYNTHORBIS_API int synth_candidate_list_get_page(
    const SynthCandidateList* list,
    SynthCandidate** page_candidates,
    int* page_count);

// 获取总页数
SYNTHORBIS_API int synth_candidate_list_page_count(const SynthCandidateList* list);

#ifdef __cplusplus
}
#endif

// ─────────────────────────────────────────────────────────
// C++ 扩展
// ─────────────────────────────────────────────────────────

#ifdef __cplusplus

#include <string>
#include <vector>
#include <memory>

namespace synthorbis {

enum class CandidateType {
    Simplified,
    Traditional,
    Stroke,
    Phonetic,
    Symbol,
    Phrase,
    Cloud
};

struct Candidate {
    std::string text;
    std::string comment;
    std::string extra_data;
    CandidateType type;
    float score;
    int index;
    bool is_selected;

    Candidate() : score(0.0f), index(0), is_selected(false) {}
};

class CandidateList {
public:
    CandidateList();
    ~CandidateList();

    void Add(const std::string& text,
             const std::string& comment = "",
             CandidateType type = CandidateType::Simplified,
             float score = 0.0f);
    void Clear();
    size_t Size() const;
    const Candidate& Get(size_t index) const;
    void Sort();

    // 分页
    void SetPageSize(size_t page_size);
    size_t PageCount() const;
    std::vector<Candidate> GetPage(size_t page);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace synthorbis

#endif  // __cplusplus

#endif  // SYNTHORBIS_CANDIDATES_H_