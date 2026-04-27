// SynthOrbis Candidates — 候选词列表实现

#include "synthorbis/candidates.h"

#include <algorithm>
#include <cstring>
#include <vector>

// ─────────────────────────────────────────────────────────
// 内部节点（持有字符串所有权）
// ─────────────────────────────────────────────────────────

struct CandidateNode {
    std::string text;
    std::string comment;
    std::string extra_data;
    SynthCandidateType type;
    float score;
    int index;
    int is_selected;

    // 转为 SynthCandidate（引用本 node 的字符串，需在 node 生命周期内使用）
    SynthCandidate to_c() const {
        SynthCandidate c;
        c.text        = text.c_str();
        c.comment     = comment.c_str();
        c.extra_data  = extra_data.empty() ? nullptr : extra_data.c_str();
        c.type        = type;
        c.score       = score;
        c.index       = index;
        c.is_selected = is_selected;
        return c;
    }
};

// 实际列表容器，嵌入 SynthCandidateList 的 candidates 字段后方
// 通过在 SynthCandidateList 内存末尾附加方式存储（或独立结构）
// 这里使用独立 Impl 指针通过 void* 附着在 SynthCandidateList 上

struct CandidateListImpl {
    std::vector<CandidateNode> nodes;
    // 缓存最后一次 get() 返回的 SynthCandidate（避免重复构造）
    mutable SynthCandidate last_get;
    int page_start = 0;
    int page_size  = 9;
    std::string input_text;
};

// ─────────────────────────────────────────────────────────
// 辅助：从 SynthCandidateList 中取/存 impl
// ─────────────────────────────────────────────────────────

// 我们把 impl 指针藏在 SynthCandidateList::candidates 字段里
// （candidates 指针平时为 nullptr，我们用 0x1 bit 标记为 "impl 指针"
//  太繁琐。改为把 CandidateListImpl* 存在 input_text 同位置后的隐藏字段。）
//
// 最简方案：直接扩展 SynthCandidateList。
// 但 candidates.h 中结构体已对外公开，不能随意修改字段布局。
//
// ✅ 最终方案：用 candidates 字段低位不可能出现的哨兵地址存 impl*
// 即：将 impl 分配出去，再把指针强转存在 candidates 字段，
// 在所有公开 API 入口做 "如果 candidates == IMPL_MAGIC 则取 impl" 的判断。
//
// 更清晰的方式：在 SynthCandidateList 实际分配时多分配 sizeof(void*)，
// 并用我们自己的 wrapper 管理。
//
// ─── 简化实现 ─────────────────────────────────────────────
// 使用全局 map<SynthCandidateList*, CandidateListImpl*>，
// 生命期与 list 绑定。
//
// 这在单线程场景完全可用，测试阶段足够。

#include <unordered_map>
static std::unordered_map<SynthCandidateList*, CandidateListImpl*> g_impl_map;

static CandidateListImpl* get_impl(SynthCandidateList* list) {
    auto it = g_impl_map.find(list);
    return it != g_impl_map.end() ? it->second : nullptr;
}

static const CandidateListImpl* get_impl(const SynthCandidateList* list) {
    auto it = g_impl_map.find(const_cast<SynthCandidateList*>(list));
    return it != g_impl_map.end() ? it->second : nullptr;
}

// ─────────────────────────────────────────────────────────
// 同步 SynthCandidateList 的公开 count/page_start/page_size
// ─────────────────────────────────────────────────────────

static void sync_public(SynthCandidateList* list, CandidateListImpl* impl) {
    list->count      = static_cast<int>(impl->nodes.size());
    list->page_start = impl->page_start;
    list->page_size  = impl->page_size;
}

// ─────────────────────────────────────────────────────────
// C API 实现
// ─────────────────────────────────────────────────────────

SynthCandidateList* synth_candidate_list_create(void) {
    SynthCandidateList* list = new SynthCandidateList;
    list->candidates   = nullptr;
    list->count        = 0;
    list->page_start   = 0;
    list->page_size    = 9;
    list->input_text   = nullptr;

    CandidateListImpl* impl = new CandidateListImpl;
    g_impl_map[list] = impl;

    return list;
}

void synth_candidate_list_destroy(SynthCandidateList* list) {
    if (!list) return;
    auto it = g_impl_map.find(list);
    if (it != g_impl_map.end()) {
        delete it->second;
        g_impl_map.erase(it);
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
    CandidateListImpl* impl = get_impl(list);
    if (!impl) return;

    CandidateNode node;
    node.text        = text;
    node.comment     = comment ? comment : "";
    node.extra_data  = "";
    node.type        = type;
    node.score       = score;
    node.index       = static_cast<int>(impl->nodes.size());
    node.is_selected = 0;

    impl->nodes.push_back(std::move(node));
    sync_public(list, impl);
}

void synth_candidate_list_clear(SynthCandidateList* list) {
    if (!list) return;
    CandidateListImpl* impl = get_impl(list);
    if (!impl) return;
    impl->nodes.clear();
    impl->page_start = 0;
    sync_public(list, impl);
    list->candidates = nullptr;  // 旧缓存指针失效
}

int synth_candidate_list_size(const SynthCandidateList* list) {
    return list ? list->count : 0;
}

const SynthCandidate* synth_candidate_list_get(
    const SynthCandidateList* list,
    int index) {

    if (!list || index < 0 || index >= list->count) return nullptr;
    const CandidateListImpl* impl = get_impl(list);
    if (!impl || index >= static_cast<int>(impl->nodes.size())) return nullptr;

    // 填充缓存并返回指向缓存的指针
    impl->last_get = impl->nodes[index].to_c();
    return &impl->last_get;
}

void synth_candidate_list_set_page(
    SynthCandidateList* list,
    int page_start,
    int page_size) {

    if (!list) return;
    CandidateListImpl* impl = get_impl(list);
    if (!impl) return;
    impl->page_start  = page_start;
    impl->page_size   = page_size > 0 ? page_size : 9;
    sync_public(list, impl);
}

void synth_candidate_list_sort(SynthCandidateList* list) {
    if (!list) return;
    CandidateListImpl* impl = get_impl(list);
    if (!impl) return;

    std::stable_sort(impl->nodes.begin(), impl->nodes.end(),
        [](const CandidateNode& a, const CandidateNode& b) {
            return a.score > b.score;
        });

    // 重新编号
    for (int i = 0; i < static_cast<int>(impl->nodes.size()); ++i) {
        impl->nodes[i].index = i;
    }
}

void synth_candidate_list_filter_type(
    SynthCandidateList* list,
    SynthCandidateType type) {

    if (!list) return;
    CandidateListImpl* impl = get_impl(list);
    if (!impl) return;

    // SYNTH_CANDIDATE_SIMPLIFIED 作为"保留所有"的通配
    if (type != SYNTH_CANDIDATE_SIMPLIFIED) {
        auto& nodes = impl->nodes;
        nodes.erase(
            std::remove_if(nodes.begin(), nodes.end(),
                [type](const CandidateNode& n) { return n.type != type; }),
            nodes.end());

        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            nodes[i].index = i;
        }
    }

    sync_public(list, impl);
}

int synth_candidate_list_get_page(
    const SynthCandidateList* list,
    SynthCandidate** page_candidates,
    int* page_count) {

    if (!list || !page_candidates || !page_count) return -1;
    const CandidateListImpl* impl = get_impl(list);
    if (!impl) return -1;

    int start = impl->page_start;
    int end   = std::min(start + impl->page_size,
                         static_cast<int>(impl->nodes.size()));

    *page_count       = end - start;
    *page_candidates  = nullptr;  // 调用者不得直接持有此指针跨调用使用
    // 按约定返回 nullptr 并提供 count，调用者逐个用 synth_candidate_list_get 取

    return 0;
}

int synth_candidate_list_page_count(const SynthCandidateList* list) {
    if (!list || list->page_size <= 0) return 0;
    return (list->count + list->page_size - 1) / list->page_size;
}

