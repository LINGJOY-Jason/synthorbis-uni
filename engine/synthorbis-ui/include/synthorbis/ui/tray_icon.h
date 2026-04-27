// SynthOrbis Tray Icon — 托盘图标
// 系统托盘图标 + 右键菜单

#ifndef SYNTHORBIS_UI_TRAY_ICON_H_
#define SYNTHORBIS_UI_TRAY_ICON_H_

#include <string>
#include <functional>

#ifdef SYNTHORBIS_UI_QT6
#include <QtCore>
#include <QtWidgets>
#else
#include <QtGui>
#include <QtWidgets>
#endif

namespace synthorbis {
namespace ui {

// 托盘菜单项
struct TrayMenuItem {
    std::string id;       // 菜单项 ID
    std::string label;    // 显示文本
    bool        checkable; // 是否可选中
    bool        checked;    // 是否已选中
    bool        separator;  // 是否是分隔线
    std::string shortcut;  // 快捷键（可选）
};

// 托盘图标
class TrayIcon {
public:
    explicit TrayIcon();
    ~TrayIcon();

    // ── 生命周期 ────────────────────────────────────────

    bool Create(const std::string& icon_path = "");
    void Destroy();

    // ── 状态 ───────────────────────────────────────────

    // 设置工具提示
    void SetToolTip(const std::string& tooltip);

    // 设置当前输入法方案名
    void SetCurrentSchema(const std::string& schema_name);

    // 设置图标状态（普通 / 输入中）
    enum class State { Normal, Active, Error };
    void SetState(State state);

    // ── 菜单 ────────────────────────────────────────────

    // 设置菜单项
    void SetMenu(const std::vector<TrayMenuItem>& items);

    // ── 通知 ────────────────────────────────────────────

    // 显示气泡通知
    void ShowNotification(const std::string& title, const std::string& message);

    // ── 回调 ────────────────────────────────────────────

    using OnMenuItemClicked = std::function<void(const std::string& menu_id)>;
    using OnDoubleClicked    = std::function<void()>;

    void SetOnMenuItemClicked(OnMenuItemClicked cb);
    void SetOnDoubleClicked(OnDoubleClicked cb);

    // ── 内部 QSystemTrayIcon 访问 ───────────────────────
    QSystemTrayIcon* GetQSystemTrayIcon();

private:
    struct Impl;
    Impl* impl_;
    OnMenuItemClicked on_menu_item_clicked_;
    OnDoubleClicked   on_double_clicked_;
};

}  // namespace ui
}  // namespace synthorbis

#endif  // SYNTHORBIS_UI_TRAY_ICON_H_
