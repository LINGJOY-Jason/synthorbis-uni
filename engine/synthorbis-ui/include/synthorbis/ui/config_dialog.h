// SynthOrbis Config Dialog — 配置中心
// 输入法设置面板：方案切换、云端 AI 开关、快捷键等

#ifndef SYNTHORBIS_UI_CONFIG_DIALOG_H_
#define SYNTHORBIS_UI_CONFIG_DIALOG_H_

#include <string>
#include <vector>
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

// 输入法方案信息
struct SchemaInfo {
    std::string id;       // schema_id
    std::string name;     // 显示名称
    std::string icon;     // 图标路径（可选）
    bool        is_active; // 是否当前选中
};

// 云端 AI 配置
struct CloudAIConfig {
    bool   enabled;        // 是否启用
    std::string api_url;   // API 地址
    std::string api_key;   // API Key（密文显示）
    int    timeout_ms;     // 超时（毫秒）
};

// 快捷键配置
struct HotkeyConfig {
    std::string action;    // 操作名
    std::string key_seq;   // 按键序列（如 "Ctrl+Shift+P"）
};

// 配置对话框
class ConfigDialog {
public:
    explicit ConfigDialog();
    ~ConfigDialog();

    // ── 生命周期 ────────────────────────────────────────

    bool Create();
    void Destroy();

    // 显示模态对话框
    void ShowModal();

    // ── 配置读写 ────────────────────────────────────────

    // 加载当前配置
    void LoadConfig();

    // 保存配置
    bool SaveConfig();

    // ── 方案列表 ────────────────────────────────────────

    // 设置可用方案列表
    void SetSchemas(const std::vector<SchemaInfo>& schemas);

    // ── 云端 AI ────────────────────────────────────────

    void SetCloudAIConfig(const CloudAIConfig& config);
    CloudAIConfig GetCloudAIConfig() const;

    // ── 快捷键 ─────────────────────────────────────────

    void SetHotkeys(const std::vector<HotkeyConfig>& hotkeys);

    // ── 回调 ────────────────────────────────────────────

    using OnSchemaChanged  = std::function<void(const std::string& schema_id)>;
    using OnCloudAIChanged = std::function<void(const CloudAIConfig& config)>;
    using OnHotkeyChanged  = std::function<void(const std::string& action, const std::string& key_seq)>;

    void SetOnSchemaChanged(OnSchemaChanged cb);
    void SetOnCloudAIChanged(OnCloudAIChanged cb);
    void SetOnHotkeyChanged(OnHotkeyChanged cb);

    // ── 内部 QWidget 访问 ────────────────────────────────
    QWidget* GetQWidget();

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace ui
}  // namespace synthorbis

#endif  // SYNTHORBIS_UI_CONFIG_DIALOG_H_
