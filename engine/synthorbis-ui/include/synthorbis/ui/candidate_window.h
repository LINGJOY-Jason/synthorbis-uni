// SynthOrbis Candidate Window — 候选词窗口
// 浮动窗口，显示 preedit + 候选词列表
//
// Qt 版本: Qt5 / Qt6 兼容
// 集成: synthorbis::Engine (通过 engine.h)

#ifndef SYNTHORBIS_UI_CANDIDATE_WINDOW_H_
#define SYNTHORBIS_UI_CANDIDATE_WINDOW_H_

#include <string>
#include <vector>
#include <functional>

// Qt 头文件（AUTOUI 需要完整的类型，非前向声明）
#ifdef SYNTHORBIS_UI_QT6
#include <QtCore>
#include <QtWidgets>
#else
#include <QtGui>
#include <QtWidgets>
#endif

namespace synthorbis {
namespace ui {

// 候选词项
struct CandidateItem {
    int    index;          // 候选词索引（0-based）
    std::string text;     // 候选词文本
    std::string comment;  // 候选词备注（可选，如词频、词性）
    bool   is_ai;         // 是否由 AI 生成
};

// 候选词窗口样式
struct CandidateWindowStyle {
    int    page_size     = 9;   // 每页候选词数
    int    font_size     = 14;  // 候选词字体大小
    int    window_width  = 400; // 窗口宽度
    int    window_height = 200; // 窗口高度
    bool   show_comment  = true; // 显示备注
    bool   highlight_ai  = true; // AI 候选词高亮
};

// 候选词窗口类
class CandidateWindow {
public:
    explicit CandidateWindow();
    ~CandidateWindow();

    // ── 生命周期 ────────────────────────────────────────

    // 创建窗口（需在 QApplication 初始化后调用）
    bool Create();

    // 销毁窗口
    void Destroy();

    // ── 显示控制 ────────────────────────────────────────

    // 显示窗口（相对屏幕位置）
    void Show(int x, int y);

    // 隐藏窗口
    void Hide();

    // 移动窗口
    void Move(int x, int y);

    // 窗口是否可见
    bool IsVisible() const;

    // ── 内容更新 ────────────────────────────────────────

    // 更新 preedit（用户输入中的拼音/编码）
    void UpdatePreedit(const std::string& preedit_text, int cursor_pos);

    // 更新候选词列表
    void UpdateCandidates(const std::vector<CandidateItem>& candidates);

    // 更新分页信息
    void UpdatePage(int current_page, int total_pages);

    // 清空所有内容
    void Clear();

    // ── 样式配置 ────────────────────────────────────────

    // 设置样式
    void SetStyle(const CandidateWindowStyle& style);

    // 获取样式
    CandidateWindowStyle GetStyle() const { return style_; }

    // ── 回调设置 ────────────────────────────────────────

    // 候选词点击回调: index = 点击的候选词索引
    using OnCandidateSelected = std::function<void(int index)>;
    // 分页回调: page = 要切换到的页码
    using OnPageChanged = std::function<void(int page)>;

    void SetOnCandidateSelected(OnCandidateSelected cb);
    void SetOnPageChanged(OnPageChanged cb);

    // ── 内部 QWidget 访问（供平台 IME 注入）──────────────
    // 返回底层 Qt 窗口，IME 平台层用它做父窗口
    QWidget* GetQWidget();

private:
    struct Impl;
    Impl* impl_;
    CandidateWindowStyle style_;
    OnCandidateSelected on_candidate_selected_;
    OnPageChanged on_page_changed_;
};

}  // namespace ui
}  // namespace synthorbis

#endif  // SYNTHORBIS_UI_CANDIDATE_WINDOW_H_
