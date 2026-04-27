// SynthOrbis Engine — 核心实现
//
// SynthOrbis 输入法引擎 C 接口实现

#include "synthorbis/engine.h"
#include "synthorbis/session.h"
#include "synthorbis/candidates.h"

#include <cstring>
#include <memory>
#include <vector>
#include <string>

// RIME 头文件（仅使用公开 C API）
#ifdef HAVE_RIME
#include <rime_api.h>
#endif

// ─────────────────────────────────────────────────────────
// 内部数据结构
// ─────────────────────────────────────────────────────────

struct SynthEngine {
    SynthEngineConfig config;
    SynthEngineState state;
    SynthEngineCallbacks callbacks;

    // RIME 引擎实例
    void* rime_engine;

    // 当前会话
    SynthSession* session;

    // 候选词列表
    SynthCandidateList* candidates;

    // 云端 AI 状态
    int cloud_enabled;
    std::string cloud_api_url;
};

// ─────────────────────────────────────────────────────────
// 辅助函数
// ─────────────────────────────────────────────────────────

static void init_config(SynthEngine* engine, const SynthEngineConfig* config) {
    if (config) {
        engine->config = *config;
    } else {
        memset(&engine->config, 0, sizeof(SynthEngineConfig));
        engine->config.max_candidates = 9;
        engine->config.page_size = 9;
    }
}

static void init_callbacks(SynthEngine* engine, const SynthEngineCallbacks* callbacks) {
    if (callbacks) {
        engine->callbacks = *callbacks;
    } else {
        memset(&engine->callbacks, 0, sizeof(SynthEngineCallbacks));
    }
}

// ─────────────────────────────────────────────────────────
// C API 实现
// ─────────────────────────────────────────────────────────

SynthEngine* synth_engine_create(const SynthEngineConfig* config) {
    SynthEngine* engine = new SynthEngine;

    engine->state = SYNTHORBIS_ENGINE_STATE_STOPPED;
    engine->rime_engine = nullptr;
    engine->session = synth_session_create();
    engine->candidates = synth_candidate_list_create();
    engine->cloud_enabled = 0;

    init_config(engine, config);
    init_callbacks(engine, nullptr);

    return engine;
}

void synth_engine_destroy(SynthEngine* engine) {
    if (!engine) return;

    if (engine->session) {
        synth_session_destroy(engine->session);
    }
    if (engine->candidates) {
        synth_candidate_list_destroy(engine->candidates);
    }

    delete engine;
}

int synth_engine_init(SynthEngine* engine) {
    if (!engine) return -1;

#ifdef HAVE_RIME
    // TODO: 通过标准 RIME C API 初始化引擎
    // RimeApi* rime = rime_get_api();
    // rime->setup(&traits);
    // rime->initialize(nullptr);
    (void)engine;  // suppress unused warning
#endif

    engine->state = SYNTHORBIS_ENGINE_STATE_READY;
    return 0;
}

int synth_engine_start(SynthEngine* engine) {
    if (!engine) return -1;

    if (engine->state == SYNTHORBIS_ENGINE_STATE_STOPPED) {
        synth_engine_init(engine);
    }

    engine->state = SYNTHORBIS_ENGINE_STATE_ACTIVE;

    if (engine->callbacks.on_state_changed) {
        engine->callbacks.on_state_changed(engine->state, engine->callbacks.user_data);
    }

    return 0;
}

int synth_engine_stop(SynthEngine* engine) {
    if (!engine) return -1;

    engine->state = SYNTHORBIS_ENGINE_STATE_STOPPED;

    if (engine->callbacks.on_state_changed) {
        engine->callbacks.on_state_changed(engine->state, engine->callbacks.user_data);
    }

    return 0;
}

SynthEngineState synth_engine_get_state(SynthEngine* engine) {
    return engine ? engine->state : SYNTHORBIS_ENGINE_STATE_ERROR;
}

void synth_engine_set_callbacks(SynthEngine* engine, const SynthEngineCallbacks* callbacks) {
    if (engine && callbacks) {
        engine->callbacks = *callbacks;
    }
}

int synth_engine_select_schema(SynthEngine* engine, const char* schema_id) {
    if (!engine || !schema_id) return -1;

#ifdef HAVE_RIME
    // TODO: rime->select_schema(session_id, schema_id);
    (void)schema_id;
#endif

    return 0;
}

int synth_engine_list_schemas(SynthEngine* engine, char*** schema_ids,
                              char*** schema_names, int* count) {
    if (!engine || !count) return -1;

    *count = 0;
    if (schema_ids) *schema_ids = nullptr;
    if (schema_names) *schema_names = nullptr;

#ifdef HAVE_RIME
    // TODO: 通过 RIME C API 获取方案列表
    // RimeSchemaList list; rime->get_schema_list(&list); ...
#endif

    return 0;
}

int synth_engine_process_key(SynthEngine* engine, int keycode, int modifier) {
    if (!engine) return -1;

    if (engine->state != SYNTHORBIS_ENGINE_STATE_ACTIVE) {
        return -1;
    }

#ifdef HAVE_RIME
    // TODO: 通过 RIME C API 处理按键
    // RimeSessionId sid = ...; rime->process_key(sid, keycode, modifier);
    (void)keycode; (void)modifier;
#endif

    return 1;
}

int synth_engine_select_candidate(SynthEngine* engine, int index) {
    if (!engine) return -1;

    if (engine->callbacks.on_select_candidate) {
        engine->callbacks.on_select_candidate(index, engine->callbacks.user_data);
    }

    return 0;
}

int synth_engine_commit(SynthEngine* engine) {
    if (!engine) return -1;

    const char* commit_text = synth_session_get_text(engine->session);
    if (commit_text && strlen(commit_text) > 0) {
        if (engine->callbacks.on_commit_text) {
            engine->callbacks.on_commit_text(commit_text, engine->callbacks.user_data);
        }
        synth_session_clear(engine->session);
    }

    return 0;
}

int synth_engine_clear(SynthEngine* engine) {
    if (!engine) return -1;
    synth_session_clear(engine->session);
    synth_candidate_list_clear(engine->candidates);
    return 0;
}

void synth_engine_set_cloud_enabled(SynthEngine* engine, int enabled) {
    if (engine) {
        engine->cloud_enabled = enabled;
    }
}

void synth_engine_set_cloud_api(SynthEngine* engine, const char* api_url) {
    if (engine && api_url) {
        engine->cloud_api_url = api_url;
    }
}

int synth_engine_cloud_predict(SynthEngine* engine, const char* context) {
    if (!engine || !context) return -1;

    if (!engine->cloud_enabled) {
        return -1;
    }

    // TODO: 实现云端预测调用
    // 1. 调用 cloud_api_url
    // 2. 解析返回结果
    // 3. 更新候选词列表

    return 0;
}
