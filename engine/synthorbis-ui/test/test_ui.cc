// SynthOrbis UI — 简单测试
// 验证候选词窗口可实例化并显示

#include <QApplication>
#include <QDebug>
#include "synthorbis/ui.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SynthOrbis UI Test");

    qDebug() << "=== SynthOrbis UI Test ===";

    // ── 候选词窗口测试 ──
    {
        synthorbis::ui::CandidateWindow win;
        if (!win.Create()) {
            qCritical() << "FAIL: CandidateWindow Create()";
            return 1;
        }
        qDebug() << "OK: CandidateWindow created";

        // 模拟候选词数据
        std::vector<synthorbis::ui::CandidateItem> candidates;
        for (int i = 0; i < 9; ++i) {
            synthorbis::ui::CandidateItem item;
            item.index = i;
            item.text = QString("word%1").arg(i + 1).toUtf8().constData();
            item.comment = (i % 3 == 0) ? "common" : "";
            item.is_ai = (i == 4);
            candidates.push_back(item);
        }

        win.UpdatePreedit("ce shi", 4);
        win.UpdateCandidates(candidates);
        win.UpdatePage(0, 1);

        qDebug() << "OK: Candidates updated";
        qDebug() << "OK: Preedit displayed";
    }

    // ── 配置中心测试 ──
    {
        synthorbis::ui::ConfigDialog dialog;
        if (!dialog.Create()) {
            qCritical() << "FAIL: ConfigDialog Create()";
            return 1;
        }
        qDebug() << "OK: ConfigDialog created";

        // 模拟方案列表
        std::vector<synthorbis::ui::SchemaInfo> schemas;
        {
            synthorbis::ui::SchemaInfo s;
            s.id = "luna_pinyin";
            s.name = "Double Pinyin";
            s.is_active = false;
            schemas.push_back(s);
        }
        {
            synthorbis::ui::SchemaInfo s;
            s.id = "wubi86";
            s.name = "Wubi 86";
            s.is_active = false;
            schemas.push_back(s);
        }
        {
            synthorbis::ui::SchemaInfo s;
            s.id = "cangjie5";
            s.name = "Cangjie 5";
            s.is_active = true;
            schemas.push_back(s);
        }
        dialog.SetSchemas(schemas);

        // 模拟云端 AI 配置
        synthorbis::ui::CloudAIConfig ai_config;
        ai_config.enabled = true;
        ai_config.api_url = "https://api.example.com";
        ai_config.api_key = "sk-test123456";
        ai_config.timeout_ms = 5000;
        dialog.SetCloudAIConfig(ai_config);

        qDebug() << "OK: ConfigDialog schemas and AI config set";
    }

    // ── 托盘图标测试 ──
    {
        synthorbis::ui::TrayIcon tray;
        if (!tray.Create("")) {
            qCritical() << "FAIL: TrayIcon Create()";
            return 1;
        }
        qDebug() << "OK: TrayIcon created";

        tray.SetToolTip("SynthOrbis");
        tray.SetState(synthorbis::ui::TrayIcon::State::Normal);
        qDebug() << "OK: TrayIcon state set";

        std::vector<synthorbis::ui::TrayMenuItem> menu_items;
        {
            synthorbis::ui::TrayMenuItem m;
            m.id = "open_config";
            m.label = "Settings...";
            menu_items.push_back(m);
        }
        {
            synthorbis::ui::TrayMenuItem m;
            m.id = "switch_schema";
            m.label = "Switch Schema";
            menu_items.push_back(m);
        }
        {
            synthorbis::ui::TrayMenuItem m;
            m.separator = true;
            menu_items.push_back(m);
        }
        {
            synthorbis::ui::TrayMenuItem m;
            m.id = "about";
            m.label = "About";
            menu_items.push_back(m);
        }
        {
            synthorbis::ui::TrayMenuItem m;
            m.id = "quit";
            m.label = "Quit";
            m.shortcut = "Ctrl+Q";
            menu_items.push_back(m);
        }
        tray.SetMenu(menu_items);
        qDebug() << "OK: TrayIcon menu set";
    }

    qDebug() << "\n=== All UI Tests PASSED ===";
    return 0;
}
