// SynthOrbis Session — 会话管理实现

#include "synthorbis/session.h"

#include <cstring>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────
// 内部数据结构
// ─────────────────────────────────────────────────────────

struct SynthSession {
    std::string composing_text;
    int cursor_pos;
    int selection_start;
    int selection_end;
    SynthSessionState state;
};

// ─────────────────────────────────────────────────────────
// C API 实现
// ─────────────────────────────────────────────────────────

SynthSession* synth_session_create(void) {
    SynthSession* session = new SynthSession;
    session->cursor_pos = 0;
    session->selection_start = 0;
    session->selection_end = 0;
    session->state = SYNTHORBIS_SESSION_STATE_INACTIVE;
    return session;
}

void synth_session_destroy(SynthSession* session) {
    delete session;
}

void synth_session_reset(SynthSession* session) {
    if (!session) return;
    session->composing_text.clear();
    session->cursor_pos = 0;
    session->selection_start = 0;
    session->selection_end = 0;
    session->state = SYNTHORBIS_SESSION_STATE_INACTIVE;
}

SynthSessionState synth_session_get_state(SynthSession* session) {
    return session ? session->state : SYNTHORBIS_SESSION_STATE_INACTIVE;
}

void synth_session_get_context(SynthSession* session, SynthInputContext* context) {
    if (!session || !context) return;

    context->composing_text = session->composing_text.c_str();
    context->cursor_pos = session->cursor_pos;
    context->selection_start = session->selection_start;
    context->selection_end = session->selection_end;
    context->is_composing = !session->composing_text.empty();
}

void synth_session_set_composition(SynthSession* session, const char* text, int cursor_pos) {
    if (!session || !text) return;

    session->composing_text = text;
    session->cursor_pos = cursor_pos;
    session->state = SYNTHORBIS_SESSION_STATE_COMPOSING;
}

void synth_session_clear(SynthSession* session) {
    if (!session) return;
    session->composing_text.clear();
    session->cursor_pos = 0;
    session->selection_start = 0;
    session->selection_end = 0;
    session->state = SYNTHORBIS_SESSION_STATE_INACTIVE;
}

int synth_session_length(SynthSession* session) {
    return session ? static_cast<int>(session->composing_text.length()) : 0;
}

const char* synth_session_get_text(SynthSession* session) {
    return session ? session->composing_text.c_str() : "";
}

int synth_session_delete(SynthSession* session, int pos, int length) {
    if (!session || pos < 0 || length <= 0) return -1;

    int text_len = static_cast<int>(session->composing_text.length());
    if (pos >= text_len) return -1;

    int actual_length = (pos + length > text_len) ? (text_len - pos) : length;
    session->composing_text.erase(pos, actual_length);

    if (session->cursor_pos > pos) {
        session->cursor_pos = (session->cursor_pos > pos + actual_length) ?
            session->cursor_pos - actual_length : pos;
    }

    if (session->composing_text.empty()) {
        session->state = SYNTHORBIS_SESSION_STATE_INACTIVE;
    }

    return 0;
}
