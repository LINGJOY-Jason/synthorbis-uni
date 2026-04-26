// SynthOrbis Engine — 单元测试

#include "synthorbis/engine.h"
#include "synthorbis/session.h"
#include "synthorbis/candidates.h"
#include "synthorbis/input_adapter.h"

#include <cstdio>
#include <cstring>

// 测试会话管理
int test_session() {
    printf("Testing Session...\n");

    SynthSession* session = synth_session_create();
    if (!session) {
        printf("  FAIL: Failed to create session\n");
        return 1;
    }

    // 测试状态
    SynthSessionState state = synth_session_get_state(session);
    if (state != SYNTHORBIS_SESSION_STATE_INACTIVE) {
        printf("  FAIL: Initial state should be INACTIVE\n");
        synth_session_destroy(session);
        return 1;
    }

    // 测试设置组合文本
    synth_session_set_composition(session, "hello", 5);
    state = synth_session_get_state(session);
    if (state != SYNTHORBIS_SESSION_STATE_COMPOSING) {
        printf("  FAIL: State should be COMPOSING after set_composition\n");
        synth_session_destroy(session);
        return 1;
    }

    // 测试获取文本
    const char* text = synth_session_get_text(session);
    if (strcmp(text, "hello") != 0) {
        printf("  FAIL: Text should be 'hello'\n");
        synth_session_destroy(session);
        return 1;
    }

    // 测试清空
    synth_session_clear(session);
    text = synth_session_get_text(session);
    if (strlen(text) != 0) {
        printf("  FAIL: Text should be empty after clear\n");
        synth_session_destroy(session);
        return 1;
    }

    synth_session_destroy(session);
    printf("  PASS\n");
    return 0;
}

// 测试候选词列表
int test_candidates() {
    printf("Testing Candidates...\n");

    SynthCandidateList* list = synth_candidate_list_create();
    if (!list) {
        printf("  FAIL: Failed to create candidate list\n");
        return 1;
    }

    // 测试添加候选词
    synth_candidate_list_add(list, "你好", "nihao",
                             SYNTH_CANDIDATE_SIMPLIFIED, 0.9f);
    synth_candidate_list_add(list, "你好啊", "nihao a",
                             SYNTH_CANDIDATE_SIMPLIFIED, 0.8f);

    if (synth_candidate_list_size(list) != 2) {
        printf("  FAIL: Candidate count should be 2\n");
        synth_candidate_list_destroy(list);
        return 1;
    }

    // 测试获取候选词
    const SynthCandidate* cand = synth_candidate_list_get(list, 0);
    if (!cand || strcmp(cand->text, "你好") != 0) {
        printf("  FAIL: First candidate should be '你好'\n");
        synth_candidate_list_destroy(list);
        return 1;
    }

    // 测试清空
    synth_candidate_list_clear(list);
    if (synth_candidate_list_size(list) != 0) {
        printf("  FAIL: Candidate list should be empty after clear\n");
        synth_candidate_list_destroy(list);
        return 1;
    }

    synth_candidate_list_destroy(list);
    printf("  PASS\n");
    return 0;
}

// 测试输入适配器
int test_input_adapter() {
    printf("Testing Input Adapter...\n");

    SynthInputAdapter* adapter = synth_input_adapter_create();
    if (!adapter) {
        printf("  FAIL: Failed to create input adapter\n");
        return 1;
    }

    // 测试键码映射
    int rime_keycode = 0;
    int rime_modifier = 0;
    int result = synth_input_adapter_map_keycode(adapter, 0x08, 0,
                                                  &rime_keycode, &rime_modifier);
    if (!result || rime_keycode != 0xff08) {
        printf("  FAIL: Backspace should map to 0xff08\n");
        synth_input_adapter_destroy(adapter);
        return 1;
    }

    // 测试非激活状态
    synth_input_adapter_set_active(adapter, 0);
    SynthInputResult input_result = synth_input_adapter_process_key(
        adapter, 0x41, 0, 0);
    if (input_result != SYNTH_INPUT_IGNORED) {
        printf("  FAIL: Inactive adapter should ignore input\n");
        synth_input_adapter_destroy(adapter);
        return 1;
    }

    // 测试激活状态
    synth_input_adapter_set_active(adapter, 1);
    input_result = synth_input_adapter_process_key(adapter, 0x41, 0, 0);
    // 注意：没有 RIME 引擎时返回 NOT_HANDLED 是正常的

    synth_input_adapter_destroy(adapter);
    printf("  PASS\n");
    return 0;
}

// 测试引擎创建
int test_engine_create() {
    printf("Testing Engine Create...\n");

    SynthEngineConfig config;
    config.user_data_dir = "/tmp/synthorbis";
    config.shared_data_dir = "/usr/share/synthorbis";
    config.schema_id = "luna_pinyin";
    config.max_candidates = 9;
    config.page_size = 9;
    config.cloud_api_enabled = 0;

    SynthEngine* engine = synth_engine_create(&config);
    if (!engine) {
        printf("  FAIL: Failed to create engine\n");
        return 1;
    }

    // 测试状态
    SynthEngineState state = synth_engine_get_state(engine);
    if (state != SYNTHORBIS_ENGINE_STATE_STOPPED) {
        printf("  FAIL: Initial state should be STOPPED\n");
        synth_engine_destroy(engine);
        return 1;
    }

    synth_engine_destroy(engine);
    printf("  PASS\n");
    return 0;
}

int main() {
    printf("=== SynthOrbis Engine Unit Tests ===\n\n");

    int failures = 0;

    failures += test_session();
    failures += test_candidates();
    failures += test_input_adapter();
    failures += test_engine_create();

    printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
