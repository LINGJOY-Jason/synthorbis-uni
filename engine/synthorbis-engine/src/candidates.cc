// SynthOrbis Candidates — 候选词列表实现

#include "synthorbis/candidates.h"

#include <algorithm>
#include <cstring>
#include <vector>

// ─────────────────────────────────────────────────────────
// 内部数据结构
// ─────────────────────────────────────────────────────────

struct CandidateNode {
    std::string text;
    std::string comment;
    std::string extra_data;
    SynthCandidateType type;
    float score;
    int index;

    bool is_selected;
};

struct SynthCandidateListImpl {
    std::vector<CandidateNode> candidates;
    std::string input_text;
    int page_start;
    int page_size;
};

// ─────────────────────────────────────────────────────────
// C API 实现
// ─────────────────────────────────────────────────────────

SynthCandidateList* synth_candidate_list_create(void) {
    SynthCandidateList* list = new SynthCandidateList;
    list->candidates = nullptr;
    list->count = 0;
    list->page_start = 0;
    list->page_size = 9;
    list->input_text = nullptr;
    return list;
}

void synth_candidate_list_destroy(SynthCandidateList* list) {
    if (!list) return;
    if (list->candidates) {
        delete[] list->candidates;
    }
    delete list;
}

void synth_candidate_list_add(
    SynthCandidateList* list,
    const char* text,
    const char* comment,
    SynthCandidateType type,
    float score) {

    if (!list || !text) return;

    CandidateNode node;
    node.text = text;
    node.comment = comment ? comment : "";
    node.type = type;
    node.score = score;
    node.index = list->count;
    node.is_selected = false;

    // 转换为内部格式
    SynthCandidate cand;
    cand.text = node.text.c_str();
    cand.comment = node.comment.c_str();
    cand.type = type;
    cand.score = score;
    cand.index = node.index;

    // 重新分配数组
    auto old = list->candidates;
    list->candidates = new SynthCandidate[list->count + 1];
    for (int i = 0; i < list->count; i++) {
        list->candidates[i] = old[i];
    }
    list->candidates[list->count] = cand;
    list->count++;

    if (old) delete[] old;
}

void synth_candidate_list_clear(SynthCandidateList* list) {
    if (!list) return;
    if (list->candidates) {
        delete[] list->candidates;
        list->candidates = nullptr;
    }
    list->count = 0;
    list->page_start = 0;
}

int synth_candidate_list_size(const SynthCandidateList* list) {
    return list ? list->count : 0;
}

const SynthCandidate* synth_candidate_list_get(
    const SynthCandidateList* list,
    int index) {

    if (!list || index < 0 || index >= list->count) {
        return nullptr;
    }
    return &list->candidates[index];
}

void synth_candidate_list_set_page(
    SynthCandidateList* list,
    int page_start,
    int page_size) {

    if (!list) return;
    list->page_start = page_start;
    list->page_size = page_size > 0 ? page_size : 9;
}

void synth_candidate_list_sort(SynthCandidateList* list) {
    if (!list || !list->candidates) return;

    // 使用简单的选择排序
    for (int i = 0; i < list->count - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < list->count; j++) {
            if (list->candidates[j].score > list->candidates[max_idx].score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            std::swap(list->candidates[i], list->candidates[max_idx]);
        }
    }
}

void synth_candidate_list_filter_type(
    SynthCandidateList* list,
    SynthCandidateType type) {

    if (!list || !list->candidates) return;

    // 简单实现：创建新数组并复制符合条件的项
    SynthCandidate* filtered = new SynthCandidate[list->count];
    int new_count = 0;

    for (int i = 0; i < list->count; i++) {
        if (list->candidates[i].type == type ||
            type == SYNTH_CANDIDATE_SIMPLIFIED) {  // 简化：所有类型都保留
            filtered[new_count++] = list->candidates[i];
        }
    }

    delete[] list->candidates;
    list->candidates = filtered;
    list->count = new_count;
}

int synth_candidate_list_get_page(
    const SynthCandidateList* list,
    SynthCandidate** page_candidates,
    int* page_count) {

    if (!list || !page_candidates || !page_count) return -1;

    int start = list->page_start;
    int end = std::min(start + list->page_size, list->count);

    *page_count = end - start;
    *page_candidates = const_cast<SynthCandidate*>(&list->candidates[start]);

    return 0;
}

int synth_candidate_list_page_count(const SynthCandidateList* list) {
    if (!list || list->page_size <= 0) return 0;
    return (list->count + list->page_size - 1) / list->page_size;
}
