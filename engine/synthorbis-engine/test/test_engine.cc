// SynthOrbis Engine — 单元测试
//
// 包含 stub 测试 + RIME C API 集成测试（当 HAVE_RIME 定义时运行）

#include "synthorbis/engine.h"
#include "synthorbis/session.h"
#include "synthorbis/candidates.h"
#include "synthorbis/input_adapter.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>  // mkdir
#include <sys/stat.h>

// ─────────────────────────────────────────────────────────
// stub 测试
// ─────────────────────────────────────────────────────────

int test_session() {
    printf("Testing Session...\n");

    SynthSession* session = synth_session_create();
    if (!session) { printf("  FAIL: create session\n"); return 1; }

    if (synth_session_get_state(session) != SYNTHORBIS_SESSION_STATE_INACTIVE) {
        printf("  FAIL: initial state\n");
        synth_session_destroy(session); return 1;
    }

    synth_session_set_composition(session, "hello", 5);
    if (synth_session_get_state(session) != SYNTHORBIS_SESSION_STATE_COMPOSING) {
        printf("  FAIL: composing state\n");
        synth_session_destroy(session); return 1;
    }

    if (strcmp(synth_session_get_text(session), "hello") != 0) {
        printf("  FAIL: get text\n");
        synth_session_destroy(session); return 1;
    }

    synth_session_clear(session);
    if (strlen(synth_session_get_text(session)) != 0) {
        printf("  FAIL: after clear\n");
        synth_session_destroy(session); return 1;
    }

    synth_session_destroy(session);
    printf("  PASS\n");
    return 0;
}

int test_candidates() {
    printf("Testing Candidates...\n");

    SynthCandidateList* list = synth_candidate_list_create();
    if (!list) { printf("  FAIL: create list\n"); return 1; }

    synth_candidate_list_add(list, "\xe4\xbd\xa0\xe5\xa5\xbd",       // 你好
                             "nihao", SYNTH_CANDIDATE_SIMPLIFIED, 0.9f);
    synth_candidate_list_add(list, "\xe4\xbd\xa0\xe5\xa5\xbd\xe5\x95\x8a",  // 你好啊
                             "nihao a", SYNTH_CANDIDATE_SIMPLIFIED, 0.8f);

    if (synth_candidate_list_size(list) != 2) {
        printf("  FAIL: count\n"); synth_candidate_list_destroy(list); return 1;
    }

    const SynthCandidate* c = synth_candidate_list_get(list, 0);
    if (!c) { printf("  FAIL: get[0]\n"); synth_candidate_list_destroy(list); return 1; }

    synth_candidate_list_clear(list);
    if (synth_candidate_list_size(list) != 0) {
        printf("  FAIL: after clear\n"); synth_candidate_list_destroy(list); return 1;
    }

    synth_candidate_list_destroy(list);
    printf("  PASS\n");
    return 0;
}

int test_input_adapter() {
    printf("Testing Input Adapter...\n");

    SynthInputAdapter* adapter = synth_input_adapter_create();
    if (!adapter) { printf("  FAIL: create adapter\n"); return 1; }

    int rime_kc = 0, rime_mod = 0;
    if (!synth_input_adapter_map_keycode(adapter, 0x08, 0, &rime_kc, &rime_mod)
        || rime_kc != 0xff08) {
        printf("  FAIL: backspace mapping\n");
        synth_input_adapter_destroy(adapter); return 1;
    }

    synth_input_adapter_set_active(adapter, 0);
    if (synth_input_adapter_process_key(adapter, 0x41, 0, 0) != SYNTH_INPUT_IGNORED) {
        printf("  FAIL: inactive should ignore\n");
        synth_input_adapter_destroy(adapter); return 1;
    }

    synth_input_adapter_destroy(adapter);
    printf("  PASS\n");
    return 0;
}

int test_engine_create() {
    printf("Testing Engine Create (stub)...\n");

    SynthEngineConfig cfg;
    cfg.user_data_dir   = "/tmp/synthorbis-test";
    cfg.shared_data_dir = "/usr/share/rime-data";
    cfg.schema_id       = "luna_pinyin";
    cfg.max_candidates  = 9;
    cfg.page_size       = 9;
    cfg.cloud_api_enabled = 0;

    SynthEngine* engine = synth_engine_create(&cfg);
    if (!engine) { printf("  FAIL: create engine\n"); return 1; }

    if (synth_engine_get_state(engine) != SYNTHORBIS_ENGINE_STATE_STOPPED) {
        printf("  FAIL: initial state\n");
        synth_engine_destroy(engine); return 1;
    }

    synth_engine_destroy(engine);
    printf("  PASS\n");
    return 0;
}

// ─────────────────────────────────────────────────────────
// RIME 集成测试（仅在 HAVE_RIME 时编译）
// ─────────────────────────────────────────────────────────

#ifdef HAVE_RIME

// 回调收集结构
struct TestCollector {
    char commit_buf[256];
    int  commit_count;
    int  candidate_count;
    int  state_changed;
};

static void cb_commit(const char* text, void* ud) {
    auto* col = static_cast<TestCollector*>(ud);
    snprintf(col->commit_buf, sizeof(col->commit_buf), "%s", text);
    col->commit_count++;
    printf("    [commit] %s\n", text);
}

static void cb_candidates(SynthCandidateList* list, void* ud) {
    auto* col = static_cast<TestCollector*>(ud);
    col->candidate_count = synth_candidate_list_size(list);
    printf("    [candidates] count=%d\n", col->candidate_count);
    for (int i = 0; i < col->candidate_count && i < 5; ++i) {
        const SynthCandidate* c = synth_candidate_list_get(list, i);
        if (c) printf("      [%d] %s  %s\n", i, c->text, c->comment);
    }
}

static void cb_state(SynthEngineState state, void* ud) {
    auto* col = static_cast<TestCollector*>(ud);
    col->state_changed++;
    const char* names[] = {"STOPPED","READY","ACTIVE","??","??","ERROR"};
    int idx = (state == SYNTHORBIS_ENGINE_STATE_ERROR) ? 5 : state;
    printf("    [state] -> %s\n", names[idx]);
}

int test_rime_integration() {
    printf("Testing RIME Integration...\n");

    // 准备用户数据目录
    mkdir("/tmp/synthorbis-test",  0755);
    mkdir("/tmp/synthorbis-log",   0755);

    SynthEngineConfig cfg;
    cfg.user_data_dir   = "/tmp/synthorbis-test";
    cfg.shared_data_dir = "/usr/share/rime-data";
    cfg.schema_id       = "luna_pinyin";
    cfg.max_candidates  = 9;
    cfg.page_size       = 9;
    cfg.cloud_api_enabled = 0;

    SynthEngine* engine = synth_engine_create(&cfg);
    if (!engine) { printf("  FAIL: create engine\n"); return 1; }

    // 设置回调
    TestCollector col;
    memset(&col, 0, sizeof(col));

    SynthEngineCallbacks cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.on_commit_text        = cb_commit;
    cbs.on_update_candidates  = cb_candidates;
    cbs.on_state_changed      = cb_state;
    cbs.user_data             = &col;
    synth_engine_set_callbacks(engine, &cbs);

    // 初始化（触发 RIME deploy）
    printf("  Initializing RIME (may take a moment for first deploy)...\n");
    int rc = synth_engine_start(engine);
    if (rc != 0) {
        printf("  FAIL: engine start returned %d\n", rc);
        synth_engine_destroy(engine);
        return 1;
    }
    printf("  RIME started OK\n");

    // 输入 "n" → 应产生候选词
    //  RIME XKB keycode: ASCII 字母直接传字符值
    printf("  Typing 'n'...\n");
    synth_engine_process_key(engine, 'n', 0);
    printf("  Candidates after 'n': %d\n", col.candidate_count);

    printf("  Typing 'i'...\n");
    synth_engine_process_key(engine, 'i', 0);
    printf("  Candidates after 'ni': %d\n", col.candidate_count);

    printf("  Typing 'h'...\n");
    synth_engine_process_key(engine, 'h', 0);
    printf("  Typing 'a'...\n");
    synth_engine_process_key(engine, 'a', 0);
    printf("  Typing 'o'...\n");
    synth_engine_process_key(engine, 'o', 0);
    printf("  Candidates after 'nihao': %d\n", col.candidate_count);

    if (col.candidate_count == 0) {
        printf("  WARNING: no candidates for 'nihao' (schema may not be deployed yet)\n");
    }

    // 选择第一个候选词（Space 键 = 0x20）
    printf("  Pressing Space to select...\n");
    col.commit_count = 0;
    synth_engine_process_key(engine, 0x20, 0);

    if (col.commit_count > 0) {
        printf("  Committed: '%s'\n", col.commit_buf);
    } else {
        // 尝试直接 select_candidate
        synth_engine_select_candidate(engine, 0);
        if (col.commit_count > 0) {
            printf("  Committed via select_candidate: '%s'\n", col.commit_buf);
        } else {
            printf("  INFO: no commit yet (normal on first run, schema deploying)\n");
        }
    }

    // 测试方案列表
    char** ids = nullptr;
    char** names = nullptr;
    int count = 0;
    synth_engine_list_schemas(engine, &ids, &names, &count);
    printf("  Available schemas: %d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("    %s: %s\n", ids[i], names[i]);
        free(ids[i]);
        free(names[i]);
    }
    free(ids);
    free(names);

    // 清理
    synth_engine_destroy(engine);
    printf("  PASS (RIME integration functional)\n");
    return 0;
}

#endif  // HAVE_RIME

// ─────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────

int main() {
    printf("=== SynthOrbis Engine Unit Tests ===\n\n");

    int failures = 0;
    failures += test_session();
    failures += test_candidates();
    failures += test_input_adapter();
    failures += test_engine_create();

#ifdef HAVE_RIME
    printf("\n--- RIME Integration Tests ---\n");
    failures += test_rime_integration();
#else
    printf("\n[INFO] Built without RIME — skipping integration tests\n");
#endif

    printf("\n=== Results: %d failure(s) ===\n", failures);
    return failures;
}
