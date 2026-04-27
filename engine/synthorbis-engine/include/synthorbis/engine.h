// SynthOrbis Engine — RIME 集成层核心接口
//
// 提供 SynthOrbis UNI 输入法引擎的统一接口
// 桥接 RIME 核心与平台抽象层

#ifndef SYNTHORBIS_ENGINE_H_
#define SYNTHORBIS_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "synthorbis/session.h"
#include "synthorbis/input_adapter.h"
#include "synthorbis/candidates.h"

#ifdef __cplusplus
extern "C" {
#endif

// 引擎状态
typedef enum {
    SYNTHORBIS_ENGINE_STATE_STOPPED = 0,
    SYNTHORBIS_ENGINE_STATE_READY = 1,
    SYNTHORBIS_ENGINE_STATE_ACTIVE = 2,
    SYNTHORBIS_ENGINE_STATE_ERROR = -1
} SynthEngineState;

// 引擎配置
typedef struct {
    const char* user_data_dir;      // 用户数据目录
    const char* shared_data_dir;     // 共享数据目录
    const char* schema_id;          // 默认输入法方案
    int max_candidates;             // 最大候选词数
    int page_size;                  // 每页显示候选词数
    int cloud_api_enabled;          // 是否启用云端 AI
} SynthEngineConfig;

// 引擎实例
typedef struct SynthEngine SynthEngine;

// 回调函数类型
typedef void (*OnCommitText)(const char* text, void* user_data);
typedef void (*OnUpdateCandidates)(SynthCandidateList* candidates, void* user_data);
typedef void (*OnSelectCandidate)(int index, void* user_data);
typedef void (*OnStateChanged)(SynthEngineState state, void* user_data);

// 回调配置
typedef struct {
    OnCommitText on_commit_text;
    OnUpdateCandidates on_update_candidates;
    OnSelectCandidate on_select_candidate;
    OnStateChanged on_state_changed;
    void* user_data;
} SynthEngineCallbacks;

// ─────────────────────────────────────────────────────────
// C API - 引擎生命周期
// ─────────────────────────────────────────────────────────

// 创建引擎实例
SYNTHORBIS_API SynthEngine* synth_engine_create(const SynthEngineConfig* config);

// 销毁引擎实例
SYNTHORBIS_API void synth_engine_destroy(SynthEngine* engine);

// 初始化引擎
SYNTHORBIS_API int synth_engine_init(SynthEngine* engine);

// 启动引擎
SYNTHORBIS_API int synth_engine_start(SynthEngine* engine);

// 停止引擎
SYNTHORBIS_API int synth_engine_stop(SynthEngine* engine);

// 获取引擎状态
SYNTHORBIS_API SynthEngineState synth_engine_get_state(SynthEngine* engine);

// ─────────────────────────────────────────────────────────
// C API - 引擎配置
// ─────────────────────────────────────────────────────────

// 设置回调函数
SYNTHORBIS_API void synth_engine_set_callbacks(
    SynthEngine* engine,
    const SynthEngineCallbacks* callbacks);

// 切换输入法方案
SYNTHORBIS_API int synth_engine_select_schema(
    SynthEngine* engine,
    const char* schema_id);

// 获取可用方案列表
SYNTHORBIS_API int synth_engine_list_schemas(
    SynthEngine* engine,
    char*** schema_ids,
    char*** schema_names,
    int* count);

// ─────────────────────────────────────────────────────────
// C API - 输入处理
// ─────────────────────────────────────────────────────────

// 处理按键事件
SYNTHORBIS_API int synth_engine_process_key(
    SynthEngine* engine,
    int keycode,
    int modifier);

// 处理候选词选择
SYNTHORBIS_API int synth_engine_select_candidate(
    SynthEngine* engine,
    int index);

// 提交当前输入
SYNTHORBIS_API int synth_engine_commit(SynthEngine* engine);

// 清空当前输入
SYNTHORBIS_API int synth_engine_clear(SynthEngine* engine);

// ─────────────────────────────────────────────────────────
// C API - 云端 AI 集成
// ─────────────────────────────────────────────────────────

// 启用/禁用云端 AI
SYNTHORBIS_API void synth_engine_set_cloud_enabled(
    SynthEngine* engine,
    int enabled);

// 设置云端 API 地址
SYNTHORBIS_API void synth_engine_set_cloud_api(
    SynthEngine* engine,
    const char* api_url);

// 请求云端预测
SYNTHORBIS_API int synth_engine_cloud_predict(
    SynthEngine* engine,
    const char* context);

#ifdef __cplusplus
}
#endif

// ─────────────────────────────────────────────────────────
// C++ API - 高级接口
// ─────────────────────────────────────────────────────────

#ifdef __cplusplus

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace synthorbis {

// C++ 引擎状态枚举（与 C API 对应）
enum class EngineState {
    Stopped = SYNTHORBIS_ENGINE_STATE_STOPPED,
    Ready   = SYNTHORBIS_ENGINE_STATE_READY,
    Active  = SYNTHORBIS_ENGINE_STATE_ACTIVE,
    Error   = SYNTHORBIS_ENGINE_STATE_ERROR
};

// C++ 回调类型别名
using CommitTextCallback       = std::function<void(const std::string&)>;
using UpdateCandidatesCallback = std::function<void(const std::vector<std::string>&)>;
using SelectCandidateCallback  = std::function<void(int)>;
using StateChangedCallback     = std::function<void(EngineState)>;

class Engine {
public:
    Engine();
    ~Engine();

    // 初始化与生命周期
    bool Initialize(const std::string& user_data_dir,
                    const std::string& shared_data_dir);
    bool Start();
    bool Stop();
    EngineState GetState() const;

    // 配置
    void SetSchema(const std::string& schema_id);
    std::vector<std::pair<std::string, std::string>> ListSchemas() const;

    // C++ 回调设置
    void OnCommitText(CommitTextCallback callback);
    void OnUpdateCandidates(UpdateCandidatesCallback callback);
    void OnSelectCandidate(SelectCandidateCallback callback);
    void OnStateChanged(StateChangedCallback callback);

    // 输入处理
    bool ProcessKey(int keycode, int modifier);
    bool SelectCandidate(int index);
    bool Commit();
    bool Clear();

    // 云端 AI
    void SetCloudEnabled(bool enabled);
    void SetCloudApi(const std::string& api_url);
    bool CloudPredict(const std::string& context);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace synthorbis

#endif  // __cplusplus

#endif  // SYNTHORBIS_ENGINE_H_
