// SynthOrbis Tray Icon — 托盘图标实现

#include "synthorbis/ui/tray_icon.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QApplication>

#ifdef SYNTHORBIS_UI_QT6
#include <QtCore>
#include <QtWidgets>
#else
#include <QtGui>
#include <QtWidgets>
#endif

// ─────────────────────────────────────────────────────────
// 内部托盘图标
// ─────────────────────────────────────────────────────────

class TrayIconWidget : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit TrayIconWidget(QObject* parent = nullptr)
        : QSystemTrayIcon(parent)
    {
        // 创建默认图标（一个简单的输入法图标）
        SetDefaultIcon();

        // 默认提示
        setToolTip("SynthOrbis 输入法");

        // 默认菜单
        BuildMenu({});
    }

    void SetToolTipText(const QString& text) {
        setToolTip(text);
    }

    void SetState(synthorbis::ui::TrayIcon::State state) {
        QColor color;
        switch (state) {
            case synthorbis::ui::TrayIcon::State::Normal: color = Qt::darkGray; break;
            case synthorbis::ui::TrayIcon::State::Active: color = QColor(74, 144, 217); break;
            case synthorbis::ui::TrayIcon::State::Error:   color = Qt::red; break;
        }
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(4, 4, 24, 24);
        p.setBrush(Qt::white);
        p.drawRect(10, 14, 12, 2);
        p.drawRect(10, 18, 12, 2);
        p.drawRect(10, 22, 8, 2);
        setIcon(QIcon(pix));
    }

    void SetMenu(const std::vector<synthorbis::ui::TrayMenuItem>& items) {
        BuildMenu(items);
    }

    void SetOnMenuItemClicked(std::function<void(const std::string&)> cb) {
        on_menu_item_clicked_ = std::move(cb);
    }

    void SetOnDoubleClicked(std::function<void()> cb) {
        on_double_clicked_ = std::move(cb);
        connect(this, &QSystemTrayIcon::activated,
                this, [this](QSystemTrayIcon::ActivationReason reason) {
                    if (reason == QSystemTrayIcon::DoubleClick) {
                        if (on_double_clicked_) on_double_clicked_();
                    }
                });
    }

public slots:
    void OnMenuAction() {
        auto* action = qobject_cast<QAction*>(sender());
        if (action && on_menu_item_clicked_) {
            QString id = action->data().toString();
            on_menu_item_clicked_(id.toUtf8().constData());
        }
    }

private:
    void SetDefaultIcon() {
        // 画一个简单的输入法图标：灰色圆形 + 输入符号
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(100, 100, 100));
        p.setPen(Qt::NoPen);
        p.drawEllipse(4, 4, 24, 24);
        p.setBrush(Qt::white);
        p.drawRect(10, 14, 12, 2);
        p.drawRect(10, 18, 12, 2);
        p.drawRect(10, 22, 8, 2);
        setIcon(QIcon(pix));
    }

    void BuildMenu(const std::vector<synthorbis::ui::TrayMenuItem>& items) {
        auto* menu = new QMenu();

        // 如果没有传入菜单项，使用默认菜单
        if (items.empty()) {
            auto* schema_action = menu->addAction("当前方案: 双拼");
            schema_action->setEnabled(false);

            menu->addSeparator();

            auto* config_action = menu->addAction("配置中心...");
            config_action->setData(QString("open_config"));
            connect(config_action, &QAction::triggered, this, &TrayIconWidget::OnMenuAction);

            auto* switch_action = menu->addAction("切换输入法方案");
            switch_action->setData(QString("switch_schema"));
            connect(switch_action, &QAction::triggered, this, &TrayIconWidget::OnMenuAction);

            menu->addSeparator();

            auto* about_action = menu->addAction("关于");
            about_action->setData(QString("about"));
            connect(about_action, &QAction::triggered, this, &TrayIconWidget::OnMenuAction);

            menu->addSeparator();

            auto* quit_action = menu->addAction("退出");
            quit_action->setData(QString("quit"));
            connect(quit_action, &QAction::triggered, this, &TrayIconWidget::OnMenuAction);
        } else {
            for (const auto& item : items) {
                if (item.separator) {
                    menu->addSeparator();
                    continue;
                }
                auto* action = menu->addAction(QString::fromUtf8(item.label.c_str()));
                action->setData(QString::fromUtf8(item.id.c_str()));
                action->setCheckable(item.checkable);
                action->setChecked(item.checked);
                if (!item.shortcut.empty()) {
                    action->setShortcut(
                        QKeySequence(QString::fromUtf8(item.shortcut.c_str())));
                }
                connect(action, &QAction::triggered, this, &TrayIconWidget::OnMenuAction);
            }
        }

        setContextMenu(menu);
    }

    std::function<void(const std::string&)> on_menu_item_clicked_;
    std::function<void()> on_double_clicked_;
};

// ─────────────────────────────────────────────────────────
// TrayIcon 公共 API
// ─────────────────────────────────────────────────────────

struct synthorbis::ui::TrayIcon::Impl {
    TrayIconWidget* widget = nullptr;
};

synthorbis::ui::TrayIcon::TrayIcon()
    : impl_(new Impl())
    , on_menu_item_clicked_(nullptr)
    , on_double_clicked_(nullptr)
{}

synthorbis::ui::TrayIcon::~TrayIcon() {
    delete impl_;
}

bool synthorbis::ui::TrayIcon::Create(const std::string& icon_path) {
    if (impl_->widget) return true;
    impl_->widget = new TrayIconWidget();

    if (!icon_path.empty()) {
        impl_->widget->setIcon(QIcon(QString::fromUtf8(icon_path.c_str())));
    }

    if (on_menu_item_clicked_) {
        impl_->widget->SetOnMenuItemClicked(
            [this](const std::string& id) { if (on_menu_item_clicked_) on_menu_item_clicked_(id); });
    }
    if (on_double_clicked_) {
        impl_->widget->SetOnDoubleClicked(
            [this]() { if (on_double_clicked_) on_double_clicked_(); });
    }

    return true;
}

void synthorbis::ui::TrayIcon::Destroy() {
    if (impl_->widget) {
        impl_->widget->hide();
        delete impl_->widget;
        impl_->widget = nullptr;
    }
}

void synthorbis::ui::TrayIcon::SetToolTip(const std::string& tooltip) {
    if (impl_->widget) {
        impl_->widget->SetToolTipText(QString::fromUtf8(tooltip.c_str()));
    }
}

void synthorbis::ui::TrayIcon::SetCurrentSchema(const std::string& schema_name) {
    if (impl_->widget) {
        impl_->widget->SetToolTipText(
            QString("SynthOrbis — %1").arg(QString::fromUtf8(schema_name.c_str())));
    }
}

void synthorbis::ui::TrayIcon::SetState(State state) {
    if (impl_->widget) {
        impl_->widget->SetState(state);
    }
}

void synthorbis::ui::TrayIcon::SetMenu(const std::vector<TrayMenuItem>& items) {
    if (impl_->widget) {
        impl_->widget->SetMenu(items);
    }
}

void synthorbis::ui::TrayIcon::ShowNotification(const std::string& title,
                                                  const std::string& message) {
    if (impl_->widget) {
        impl_->widget->showMessage(
            QString::fromUtf8(title.c_str()),
            QString::fromUtf8(message.c_str()),
            QSystemTrayIcon::Information,
            3000);
    }
}

void synthorbis::ui::TrayIcon::SetOnMenuItemClicked(OnMenuItemClicked cb) {
    on_menu_item_clicked_ = std::move(cb);
    if (impl_->widget) {
        impl_->widget->SetOnMenuItemClicked(
            [this](const std::string& id) { if (on_menu_item_clicked_) on_menu_item_clicked_(id); });
    }
}

void synthorbis::ui::TrayIcon::SetOnDoubleClicked(OnDoubleClicked cb) {
    on_double_clicked_ = std::move(cb);
    if (impl_->widget) {
        impl_->widget->SetOnDoubleClicked(
            [this]() { if (on_double_clicked_) on_double_clicked_(); });
    }
}

QSystemTrayIcon* synthorbis::ui::TrayIcon::GetQSystemTrayIcon() {
    return impl_->widget;
}

// moc 文件（AUTOMOC 在同一目录查找）
#include "tray_icon.moc"
