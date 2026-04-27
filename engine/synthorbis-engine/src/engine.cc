// SynthOrbis Engine — 核心实现
//
// RIME C API 正式集成（通过 rime_get_api() 方式）

#include "synthorbis/engine.h"
#include "synthorbis/session.h"
#include "synthorbis/candidates.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cinttypes>
#include <memory>
#include <vector>
#include <string>

// RIME 头文件（公开 C API）
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

#ifdef HAVE_RIME
    RimeApi*       rime;         // RIME API 函数表
    RimeSessionId  rime_session; // 当前输入会话
    bool           rime_inited;  // 是否已完成 RIME 初始化
#endif

    // 当前会话（preedit / 光标位置）
    SynthSession* session;

    // 候选词列表
    SynthCandidateList* candidates;

    // 云端 AI 状态
    int cloud_enabled;
    std::string cloud_api_url;
};

// ─────────────────────────────────────────────────────────
// RIME 通知回调
// ─────────────────────────────────────────────────────────

#ifdef HAVE_RIME
static void rime_notification_handler(void* context_object,
                                      RimeSessionId /*session_id*/,
                                      const char* message_type,
                                      const char* message_value) {
    SynthEngine* engine = static_cast<SynthEngine*>(context_object);
    if (!engine) return;

    if (strcmp(message_type, "deploy") == 0) {
        if (strcmp(message_value, "success") == 0) {
            // 部署成功 → 可以创建会话
            if (engine->callbacks.on_state_changed) {
                engine->callbacks.on_state_changed(
                    SYNTHORBIS_ENGINE_STATE_READY,
                    engine->callbacks.user_data);
            }
        } else if (strcmp(message_value, "failure") == 0) {
            fprintf(stderr, "[SynthOrbis] RIME deploy failure!\n");
        }
    }
}
#endif

// ─────────────────────────────────────────────────────────
// 辅助：从 RIME context 同步 session + candidates
// ─────────────────────────────────────────────────────────

#ifdef HAVE_RIME
static void sync_from_rime(SynthEngine* engine) {
    if (!engine || !engine->rime_inited || engine->rime_session == 0) return;

    RimeApi* rime = engine->rime;

    // ── preedit (组合串) ──
    RIME_STRUCT(RimeContext, ctx);
    if (rime->get_context(engine->rime_session, &ctx)) {
        if (ctx.composition.preedit) {
            synth_session_set_composition(
                engine->session,
                ctx.composition.preedit,
                ctx.composition.cursor_pos);
        } else {
            synth_session_clear(engine->session);
        }

        // ── 候选词 ──
        synth_candidate_list_clear(engine->candidates);
        for (int i = 0; i < ctx.menu.num_candidates; ++i) {
            RimeCandidate* rc = &ctx.menu.candidates[i];
            synth_candidate_list_add(
                engine->candidates,
                rc->text    ? rc->text    : "",
                rc->comment ? rc->comment : "",
                SYNTH_CANDIDATE_SIMPLIFIED,
                /* score */ 1.0f - i * 0.01f);
        }

        rime->free_context(&ctx);
    }

    // ── 通知候选词更新回调 ──
    if (engine->callbacks.on_update_candidates) {
        engine->callbacks.on_update_candidates(
            engine->candidates,
            engine->callbacks.user_data);
    }
}
#endif

// ─────────────────────────────────────────────────────────
// 辅助：初始化配置
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
    engine->session = synth_session_create();
    engine->candidates = synth_candidate_list_create();
    engine->cloud_enabled = 0;

#ifdef HAVE_RIME
    engine->rime         = nullptr;
    engine->rime_session = 0;
    engine->rime_inited  = false;
#endif

    init_config(engine, config);
    init_callbacks(engine, nullptr);

    return engine;
}

void synth_engine_destroy(SynthEngine* engine) {
    if (!engine) return;

#ifdef HAVE_RIME
    if (engine->rime_inited) {
        if (engine->rime_session) {
            engine->rime->destroy_session(engine->rime_session);
        }
        engine->rime->finalize();
    }
#endif

    synth_session_destroy(engine->session);
    synth_candidate_list_destroy(engine->candidates);
    delete engine;
}

int synth_engine_init(SynthEngine* engine) {
    if (!engine) return -1;

#ifdef HAVE_RIME
    // ── 获取 RIME API 函数表 ──
    engine->rime = rime_get_api();
    if (!engine->rime) {
        fprintf(stderr, "[SynthOrbis] rime_get_api() returned NULL\n");
        return -1;
    }

    // ── setup: 注册通知 ──
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir        = engine->config.shared_data_dir
                                    ? engine->config.shared_data_dir
                                    : "/usr/share/rime-data";
    traits.user_data_dir          = engine->config.user_data_dir
                                    ? engine->config.user_data_dir
                                    : "/tmp/synthorbis-user";
    traits.distribution_name      = "SynthOrbis";
    traits.distribution_code_name = "synthorbis";
    traits.distribution_version   = "1.0.0";
    traits.app_name               = "rime.synthorbis";
    traits.min_log_level          = 2;  // ERROR — 减少日志噪声
    traits.log_dir                = "/tmp/synthorbis-log";

    engine->rime->setup(&traits);
    engine->rime->set_notification_handler(rime_notification_handler, engine);

    // ── initialize: 加载方案并 deploy ──
    engine->rime->initialize(&traits);

    // 首次运行需要 deploy，阻塞等待完成
    if (engine->rime->start_maintenance(/* full_check= */ True)) {
        engine->rime->join_maintenance_thread();
    }

    // ── 创建输入会话 ──
    engine->rime_session = engine->rime->create_session();
    if (!engine->rime_session) {
        fprintf(stderr, "[SynthOrbis] Failed to create RIME session\n");
        engine->rime->finalize();
        return -1;
    }

    // ── 选择方案 ──
    const char* schema_id = engine->config.schema_id
                            ? engine->config.schema_id
                            : "luna_pinyin";
    engine->rime->select_schema(engine->rime_session, schema_id);

    engine->rime_inited = true;
    fprintf(stderr, "[SynthOrbis] RIME initialized, session=%lu, schema=%s\n",
            (unsigned long)engine->rime_session, schema_id);
#endif  // HAVE_RIME

    engine->state = SYNTHORBIS_ENGINE_STATE_READY;
    return 0;
}

int synth_engine_start(SynthEngine* engine) {
    if (!engine) return -1;

    if (engine->state == SYNTHORBIS_ENGINE_STATE_STOPPED) {
        int rc = synth_engine_init(engine);
        if (rc != 0) return rc;
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
    if (engine->rime_inited && engine->rime_session) {
        Bool ok = engine->rime->select_schema(engine->rime_session, schema_id);
        return ok ? 0 : -1;
    }
#endif

    return 0;
}

int synth_engine_list_schemas(SynthEngine* engine, char*** schema_ids,
                              char*** schema_names, int* count) {
    if (!engine || !count) return -1;

    *count = 0;
    if (schema_ids)   *schema_ids   = nullptr;
    if (schema_names) *schema_names = nullptr;

#ifdef HAVE_RIME
    if (!engine->rime_inited) return 0;

    RimeSchemaList list;
    if (!engine->rime->get_schema_list(&list)) return -1;

    *count = static_cast<int>(list.size);

    if (schema_ids && schema_names) {
        *schema_ids   = static_cast<char**>(malloc(list.size * sizeof(char*)));
        *schema_names = static_cast<char**>(malloc(list.size * sizeof(char*)));

        for (size_t i = 0; i < list.size; ++i) {
            (*schema_ids)[i]   = strdup(list.list[i].schema_id);
            (*schema_names)[i] = strdup(list.list[i].name);
        }
    }

    engine->rime->free_schema_list(&list);
#endif

    return 0;
}

int synth_engine_process_key(SynthEngine* engine, int keycode, int modifier) {
    if (!engine) return -1;
    if (engine->state != SYNTHORBIS_ENGINE_STATE_ACTIVE) return -1;

#ifdef HAVE_RIME
    if (engine->rime_inited && engine->rime_session) {
        Bool handled = engine->rime->process_key(
            engine->rime_session, keycode, modifier);

        // 同步 preedit + 候选词
        sync_from_rime(engine);

        // 检查是否有提交文本
        RIME_STRUCT(RimeCommit, commit);
        if (engine->rime->get_commit(engine->rime_session, &commit)) {
            if (commit.text && strlen(commit.text) > 0) {
                if (engine->callbacks.on_commit_text) {
                    engine->callbacks.on_commit_text(
                        commit.text, engine->callbacks.user_data);
                }
            }
            engine->rime->free_commit(&commit);
        }

        return handled ? 1 : 0;
    }
#endif

    // stub 模式：无 RIME，直接返回未处理
    return 0;
}

int synth_engine_select_candidate(SynthEngine* engine, int index) {
    if (!engine) return -1;

#ifdef HAVE_RIME
    if (engine->rime_inited && engine->rime_session) {
        Bool ok = engine->rime->select_candidate_on_current_page(
            engine->rime_session, static_cast<size_t>(index));

        sync_from_rime(engine);

        // 提交已选候选词
        RIME_STRUCT(RimeCommit, commit);
        if (engine->rime->get_commit(engine->rime_session, &commit)) {
            if (commit.text && strlen(commit.text) > 0) {
                if (engine->callbacks.on_commit_text) {
                    engine->callbacks.on_commit_text(
                        commit.text, engine->callbacks.user_data);
                }
            }
            engine->rime->free_commit(&commit);
        }

        return ok ? 0 : -1;
    }
#endif

    if (engine->callbacks.on_select_candidate) {
        engine->callbacks.on_select_candidate(index, engine->callbacks.user_data);
    }
    return 0;
}

int synth_engine_commit(SynthEngine* engine) {
    if (!engine) return -1;

#ifdef HAVE_RIME
    if (engine->rime_inited && engine->rime_session) {
        engine->rime->commit_composition(engine->rime_session);

        RIME_STRUCT(RimeCommit, commit);
        if (engine->rime->get_commit(engine->rime_session, &commit)) {
            if (commit.text && strlen(commit.text) > 0) {
                if (engine->callbacks.on_commit_text) {
                    engine->callbacks.on_commit_text(
                        commit.text, engine->callbacks.user_data);
                }
            }
            engine->rime->free_commit(&commit);
        }

        synth_session_clear(engine->session);
        synth_candidate_list_clear(engine->candidates);
        return 0;
    }
#endif

    // stub 路径
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

#ifdef HAVE_RIME
    if (engine->rime_inited && engine->rime_session) {
        engine->rime->clear_composition(engine->rime_session);
    }
#endif

    synth_session_clear(engine->session);
    synth_candidate_list_clear(engine->candidates);
    return 0;
}

void synth_engine_set_cloud_enabled(SynthEngine* engine, int enabled) {
    if (engine) engine->cloud_enabled = enabled;
}

void synth_engine_set_cloud_api(SynthEngine* engine, const char* api_url) {
    if (engine && api_url) engine->cloud_api_url = api_url;
}

int synth_engine_cloud_predict(SynthEngine* engine, const char* context) {
    if (!engine || !context) return -1;
    if (!engine->cloud_enabled) return -1;

    // TODO: 实现云端预测调用
    // 1. 调用 cloud_api_url
    // 2. 解析返回结果
    // 3. 更新候选词列表

    return 0;
}
