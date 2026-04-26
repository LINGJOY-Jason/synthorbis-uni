// SynthOrbis Input Adapter — 输入事件适配器
//
// 统一处理来自不同平台的输入事件（键盘、触摸、语音）

#ifndef SYNTHORBIS_INPUT_ADAPTER_H_
#define SYNTHORBIS_INPUT_ADAPTER_H_

#include "synthorbis/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 按键修饰符
#define SYNTH_KEY_MOD_SHIFT   (1 << 0)
#define SYNTH_KEY_MOD_CONTROL (1 << 1)
#define SYNTH_KEY_MOD_ALT     (1 << 2)
#define SYNTH_KEY_MOD_META    (1 << 3)
#define SYNTH_KEY_MOD_CAPS    (1 << 4)

// 特殊键码
#define SYNTH_KEY_BACKSPACE  0x08
#define SYNTH_KEY_TAB       0x09
#define SYNTH_KEY_ENTER     0x0D
#define SYNTH_KEY_ESCAPE    0x1B
#define SYNTH_KEY_SPACE     0x20
#define SYNTH_KEY_PAGEUP    0x21
#define SYNTH_KEY_PAGEDOWN  0x22
#define SYNTH_KEY_END       0x23
#define SYNTH_KEY_HOME      0x24
#define SYNTH_KEY_LEFT      0x25
#define SYNTH_KEY_UP        0x26
#define SYNTH_KEY_RIGHT     0x27
#define SYNTH_KEY_DOWN      0x28
#define SYNTH_KEY_DELETE    0x7F

// 输入事件类型
typedef enum {
    SYNTH_EVENT_KEY_DOWN = 0,
    SYNTH_EVENT_KEY_UP = 1,
    SYNTH_EVENT_TEXT = 2,
    SYNTH_EVENT_VOICE = 3,
    SYNTH_EVENT_GESTURE = 4
} SynthEventType;

// 输入事件
typedef struct {
    SynthEventType type;
    int keycode;
    int modifier;
    const char* text;       // 用于 SYNTH_EVENT_TEXT
    float confidence;      // 用于 SYNTH_EVENT_VOICE
    void* gesture_data;    // 用于 SYNTH_EVENT_GESTURE
} SynthInputEvent;

// 输入适配器
typedef struct SynthInputAdapter SynthInputAdapter;

// 输入处理结果
typedef enum {
    SYNTH_INPUT_PROCESSED = 0,       // 事件已处理
    SYNTH_INPUT_IGNORED = 1,         // 事件被忽略
    SYNTH_INPUT_NOT_HANDLED = 2      // 事件未被处理
} SynthInputResult;

// 创建输入适配器
SYNTHORBIS_API SynthInputAdapter* synth_input_adapter_create(void);

// 销毁输入适配器
SYNTHORBIS_API void synth_input_adapter_destroy(SynthInputAdapter* adapter);

// 处理按键事件
SYNTHORBIS_API SynthInputResult synth_input_adapter_process_key(
    SynthInputAdapter* adapter,
    int keycode,
    int modifier,
    int is_release);

// 处理文本输入
SYNTHORBIS_API SynthInputResult synth_input_adapter_process_text(
    SynthInputAdapter* adapter,
    const char* text);

// 处理语音输入
SYNTHORBIS_API SynthInputResult synth_input_adapter_process_voice(
    SynthInputAdapter* adapter,
    const char* text,
    float confidence);

// 获取平台原始键码到 RIME 键码的映射
SYNTHORBIS_API int synth_input_adapter_map_keycode(
    SynthInputAdapter* adapter,
    int platform_keycode,
    int platform_modifier,
    int* rime_keycode,
    int* rime_modifier);

// 设置输入法激活状态
SYNTHORBIS_API void synth_input_adapter_set_active(
    SynthInputAdapter* adapter,
    int active);

#ifdef __cplusplus
}
#endif

// ─────────────────────────────────────────────────────────
// C++ 扩展接口
// ─────────────────────────────────────────────────────────

#ifdef __cplusplus

#include <functional>

namespace synthorbis {

class InputAdapter {
public:
    using KeyHandler = std::function<InputResult(int keycode, int modifier, bool is_release)>;
    using TextHandler = std::function<InputResult(const std::string& text)>;
    using VoiceHandler = std::function<InputResult(const std::string& text, float confidence)>;

    enum class InputResult {
        Processed,
        Ignored,
        NotHandled
    };

    InputAdapter();
    ~InputAdapter();

    // 设置处理回调
    void OnKey(KeyHandler handler);
    void OnText(TextHandler handler);
    void OnVoice(VoiceHandler handler);

    // 便捷方法
    InputResult HandleKey(int keycode, int modifier, bool is_release = false);
    InputResult HandleText(const std::string& text);
    InputResult HandleVoice(const std::string& text, float confidence = 1.0f);

    // 激活状态
    void SetActive(bool active);
    bool IsActive() const;

private:
    struct Impl;
    class Impl* impl_;
};

}  // namespace synthorbis

#endif  // __cplusplus

#endif  // SYNTHORBIS_INPUT_ADAPTER_H_
