// SynthOrbis Config Dialog — 配置中心实现

#include "synthorbis/ui/config_dialog.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>

#ifdef SYNTHORBIS_UI_QT6
#include <QtCore>
#include <QtWidgets>
#else
#include <QtGui>
#include <QtWidgets>
#endif

// ─────────────────────────────────────────────────────────
// 内部 QDialog
// ─────────────────────────────────────────────────────────

class ConfigDialogWidget : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialogWidget(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("SynthOrbis 输入法配置中心");
        setMinimumSize(600, 450);
        InitUI();
    }

    void LoadFromConfig(
        const std::vector<synthorbis::ui::SchemaInfo>& schemas,
        const synthorbis::ui::CloudAIConfig& cloud_ai,
        const std::vector<synthorbis::ui::HotkeyConfig>& hotkeys)
    {
        // 填充方案列表
        schema_combo_->clear();
        for (const auto& s : schemas) {
            schema_combo_->addItem(QString::fromUtf8(s.name.c_str()),
                                    QString::fromUtf8(s.id.c_str()));
            if (s.is_active) {
                schema_combo_->setCurrentText(QString::fromUtf8(s.name.c_str()));
            }
        }

        // 云端 AI
        cloud_enabled_cb_->setChecked(cloud_ai.enabled);
        api_url_edit_->setText(QString::fromUtf8(cloud_ai.api_url.c_str()));
        api_key_edit_->setText(QString::fromUtf8(cloud_ai.api_key.c_str()));
        api_key_edit_->setEchoMode(QLineEdit::Password);
        timeout_spin_->setValue(cloud_ai.timeout_ms);

        // 快捷键
        hotkey_table_->setRowCount(static_cast<int>(hotkeys.size()));
        for (int i = 0; i < static_cast<int>(hotkeys.size()); ++i) {
            hotkey_table_->setItem(i, 0, new QTableWidgetItem(
                QString::fromUtf8(hotkeys[i].action.c_str())));
            hotkey_table_->setItem(i, 1, new QTableWidgetItem(
                QString::fromUtf8(hotkeys[i].key_seq.c_str())));
        }
    }

    void GetCloudAIConfig(synthorbis::ui::CloudAIConfig* out) {
        out->enabled = cloud_enabled_cb_->isChecked();
        out->api_url = api_url_edit_->text().toUtf8().constData();
        out->api_key = api_key_edit_->text().toUtf8().constData();
        out->timeout_ms = timeout_spin_->value();
    }

    QString GetSelectedSchemaId() const {
        return schema_combo_->currentData().toString();
    }

private slots:
    void OnSchemaChanged(const QString& text) {
        Q_UNUSED(text);
        // 触发外部回调（如果有）
    }

private:
    void InitUI() {
        QVBoxLayout* main = new QVBoxLayout(this);

        QTabWidget* tabs = new QTabWidget(this);

        // ── Tab 1: 基本设置 ──
        QWidget* basic_tab = new QWidget();
        QFormLayout* basic_layout = new QFormLayout(basic_tab);

        schema_combo_ = new QComboBox(this);
        basic_layout->addRow("输入法方案:", schema_combo_);

        QLabel* schema_hint = new QLabel("切换输入法方案后实时生效，无需重启");
        schema_hint->setStyleSheet("color: #888; font-size: 12px;");
        basic_layout->addRow("", schema_hint);

        basic_layout->addRow(new QLabel(""));  // spacer

        // 候选窗样式
        QGroupBox* style_group = new QGroupBox("候选窗样式", this);
        QFormLayout* style_layout = new QFormLayout(style_group);

        show_comment_cb_ = new QCheckBox("显示候选词备注");
        show_comment_cb_->setChecked(true);
        style_layout->addRow("", show_comment_cb_);

        highlight_ai_cb_ = new QCheckBox("AI 候选词高亮显示");
        highlight_ai_cb_->setChecked(true);
        style_layout->addRow("", highlight_ai_cb_);

        page_size_spin_ = new QSpinBox(this);
        page_size_spin_->setRange(5, 15);
        page_size_spin_->setValue(9);
        style_layout->addRow("每页候选词数:", page_size_spin_);

        basic_layout->addRow(style_group);

        tabs->addTab(basic_tab, "基本设置");

        // ── Tab 2: 云端 AI ──
        QWidget* ai_tab = new QWidget();
        QVBoxLayout* ai_layout = new QVBoxLayout(ai_tab);

        cloud_enabled_cb_ = new QCheckBox("启用云端 AI 预测");
        ai_layout->addWidget(cloud_enabled_cb_);

        QGroupBox* api_group = new QGroupBox("API 配置", this);
        QFormLayout* api_layout = new QFormLayout(api_group);

        api_url_edit_ = new QLineEdit(this);
        api_url_edit_->setPlaceholderText("https://api.example.com/v1/predict");
        api_layout->addRow("API 地址:", api_url_edit_);

        api_key_edit_ = new QLineEdit(this);
        api_key_edit_->setPlaceholderText("sk-xxxxxxxx");
        api_layout->addRow("API Key:", api_key_edit_);

        timeout_spin_ = new QSpinBox(this);
        timeout_spin_->setRange(500, 30000);
        timeout_spin_->setSingleStep(500);
        timeout_spin_->setSuffix(" ms");
        timeout_spin_->setValue(5000);
        api_layout->addRow("超时时间:", timeout_spin_);

        ai_layout->addWidget(api_group);
        ai_layout->addStretch();

        tabs->addTab(ai_tab, "云端 AI");

        // ── Tab 3: 快捷键 ──
        QWidget* hotkey_tab = new QWidget();
        QVBoxLayout* hotkey_layout = new QVBoxLayout(hotkey_tab);

        hotkey_table_ = new QTableWidget(4, 2, this);
        hotkey_table_->setHorizontalHeaderLabels(QStringList() << "操作" << "快捷键");
        hotkey_table_->horizontalHeader()->setStretchLastSection(true);
        hotkey_table_->setItem(0, 0, new QTableWidgetItem("切换输入法"));
        hotkey_table_->setItem(0, 1, new QTableWidgetItem("Ctrl+Shift+P"));
        hotkey_table_->setItem(1, 0, new QTableWidgetItem("中英文切换"));
        hotkey_table_->setItem(1, 1, new QTableWidgetItem("Shift+Space"));
        hotkey_table_->setItem(2, 0, new QTableWidgetItem("候选词翻页"));
        hotkey_table_->setItem(2, 1, new QTableWidgetItem("-[ / ]-"));
        hotkey_table_->setItem(3, 0, new QTableWidgetItem("打开配置中心"));
        hotkey_table_->setItem(3, 1, new QTableWidgetItem("Ctrl+Shift+S"));

        hotkey_layout->addWidget(hotkey_table_);
        tabs->addTab(hotkey_tab, "快捷键");

        // ── Tab 4: 关于 ──
        QWidget* about_tab = new QWidget();
        QVBoxLayout* about_layout = new QVBoxLayout(about_tab);
        about_layout->addWidget(new QLabel("SynthOrbis UNI v1.0"));
        about_layout->addWidget(new QLabel("全平台 AI 输入法引擎"));
        about_layout->addWidget(new QLabel("基于 RIME + FunASR + 云端大模型"));
        about_layout->addStretch();
        tabs->addTab(about_tab, "关于");

        main->addWidget(tabs);

        // 按钮栏
        QDialogButtonBox* btn_box = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        main->addWidget(btn_box);
    }

    QComboBox*  schema_combo_ = nullptr;
    QCheckBox*  show_comment_cb_ = nullptr;
    QCheckBox*  highlight_ai_cb_ = nullptr;
    QSpinBox*   page_size_spin_ = nullptr;
    QCheckBox*  cloud_enabled_cb_ = nullptr;
    QLineEdit*  api_url_edit_ = nullptr;
    QLineEdit*  api_key_edit_ = nullptr;
    QSpinBox*   timeout_spin_ = nullptr;
    QTableWidget* hotkey_table_ = nullptr;
};

// ─────────────────────────────────────────────────────────
// ConfigDialog 公共 API
// ─────────────────────────────────────────────────────────

struct synthorbis::ui::ConfigDialog::Impl {
    ConfigDialogWidget* widget = nullptr;
    CloudAIConfig       cloud_ai;
};

synthorbis::ui::ConfigDialog::ConfigDialog()
    : impl_(new Impl())
{}

synthorbis::ui::ConfigDialog::~ConfigDialog() {
    delete impl_;
}

bool synthorbis::ui::ConfigDialog::Create() {
    if (impl_->widget) return true;
    impl_->widget = new ConfigDialogWidget();
    return true;
}

void synthorbis::ui::ConfigDialog::Destroy() {
    if (impl_->widget) {
        impl_->widget->deleteLater();
        impl_->widget = nullptr;
    }
}

void synthorbis::ui::ConfigDialog::ShowModal() {
    if (impl_->widget) {
        impl_->widget->exec();
    }
}

void synthorbis::ui::ConfigDialog::LoadConfig() {
    if (!impl_->widget) return;
    std::vector<SchemaInfo> schemas;
    std::vector<HotkeyConfig> hotkeys;
    impl_->widget->LoadFromConfig(schemas, impl_->cloud_ai, hotkeys);
}

bool synthorbis::ui::ConfigDialog::SaveConfig() {
    if (!impl_->widget) return false;
    impl_->widget->GetCloudAIConfig(&impl_->cloud_ai);
    return true;
}

void synthorbis::ui::ConfigDialog::SetSchemas(const std::vector<SchemaInfo>& schemas) {
    if (!impl_->widget) return;
    impl_->widget->LoadFromConfig(schemas, impl_->cloud_ai, {});
}

void synthorbis::ui::ConfigDialog::SetCloudAIConfig(const CloudAIConfig& config) {
    impl_->cloud_ai = config;
}

synthorbis::ui::CloudAIConfig synthorbis::ui::ConfigDialog::GetCloudAIConfig() const {
    return impl_->cloud_ai;
}

void synthorbis::ui::ConfigDialog::SetHotkeys(const std::vector<HotkeyConfig>& hotkeys) {
    if (!impl_->widget) return;
    impl_->widget->LoadFromConfig({}, impl_->cloud_ai, hotkeys);
}

void synthorbis::ui::ConfigDialog::SetOnSchemaChanged(OnSchemaChanged cb) {
    Q_UNUSED(cb);
}

void synthorbis::ui::ConfigDialog::SetOnCloudAIChanged(OnCloudAIChanged cb) {
    Q_UNUSED(cb);
}

void synthorbis::ui::ConfigDialog::SetOnHotkeyChanged(OnHotkeyChanged cb) {
    Q_UNUSED(cb);
}

QWidget* synthorbis::ui::ConfigDialog::GetQWidget() {
    return impl_->widget;
}

// moc 文件（AUTOMOC 在同一目录查找）
#include "config_dialog.moc"
