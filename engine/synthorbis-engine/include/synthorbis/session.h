// SynthOrbis Session — 输入会话管理
//
// 管理单个输入会话的生命周期和状态

#ifndef SYNTHORBIS_SESSION_H_
#define SYNTHORBIS_SESSION_H_

#include "synthorbis/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 会话状态
typedef enum {
    SYNTHORBIS_SESSION_STATE_INACTIVE = 0,
    SYNTHORBIS_SESSION_STATE_COMPOSING = 1,
    SYNTHORBIS_SESSION_STATE_SELECTING = 2,
    SYNTHORBIS_SESSION_STATE_PREDICTING = 3
} SynthSessionState;

// 输入上下文
typedef struct {
    const char* composing_text;       // 当前正在输入的文本
    int cursor_pos;                   // 光标位置
    int selection_start;              // 选择起始位置
    int selection_end;                // 选择结束位置
    int is_composing;                 // 是否正在输入
} SynthInputContext;

// 会话实例
typedef struct SynthSession SynthSession;

// 创建会话
SYNTHORBIS_API SynthSession* synth_session_create(void);

// 销毁会话
SYNTHORBIS_API void synth_session_destroy(SynthSession* session);

// 重置会话
SYNTHORBIS_API void synth_session_reset(SynthSession* session);

// 获取当前状态
SYNTHORBIS_API SynthSessionState synth_session_get_state(SynthSession* session);

// 获取输入上下文
SYNTHORBIS_API void synth_session_get_context(
    SynthSession* session,
    SynthInputContext* context);

// 设置输入文本
SYNTHORBIS_API void synth_session_set_composition(
    SynthSession* session,
    const char* text,
    int cursor_pos);

// 清空输入
SYNTHORBIS_API void synth_session_clear(SynthSession* session);

// 获取已输入的字符数
SYNTHORBIS_API int synth_session_length(SynthSession* session);

// 获取当前文本
SYNTHORBIS_API const char* synth_session_get_text(SynthSession* session);

// 删除指定位置的字符
SYNTHORBIS_API int synth_session_delete(
    SynthSession* session,
    int pos,
    int length);

#ifdef __cplusplus
}
#endif

#endif  // SYNTHORBIS_SESSION_H_
