// SynthOrbis Candidate Window — 候选词窗口实现
// 浮动窗口: 显示 preedit + 候选词列表，支持翻页/点击选择

#include "synthorbis/ui/candidate_window.h"

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QFontMetrics>
#include <algorithm>

#ifdef SYNTHORBIS_UI_QT6
#include <QtCore>
#include <QtWidgets>
#else
#include <QtGui>
#include <QtWidgets>
#endif

// ─────────────────────────────────────────────────────────
// 内部 QWidget：真正的候选词窗口
// ─────────────────────────────────────────────────────────

class CandidateWindowWidget : public QWidget {
    Q_OBJECT

public:
    explicit CandidateWindowWidget(QWidget* parent = nullptr)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
        , preedit_text_()
        , candidates_()
        , current_page_(0)
        , page_size_(9)
        , on_candidate_selected_(nullptr)
        , on_page_changed_(nullptr)
    {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_OpaquePaintEvent, true);

        // 固定宽度，高度随内容
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

        InitUI();
    }

    void SetOnCandidateSelected(std::function<void(int)> cb) { on_candidate_selected_ = std::move(cb); }
    void SetOnPageChanged(std::function<void(int)> cb) { on_page_changed_ = std::move(cb); }

    void UpdatePreedit(const std::string& text, int cursor_pos) {
        preedit_text_ = QString::fromUtf8(text.c_str());
        cursor_pos_ = cursor_pos;
        update();
    }

    void UpdateCandidates(const std::vector<synthorbis::ui::CandidateItem>& candidates) {
        candidates_ = candidates;
        PopulateList();
    }

    void UpdatePage(int current, int total) {
        current_page_ = current;
        total_pages_ = total;
        page_label_->setText(QString(" %1/%2 ").arg(current + 1).arg(total));
        prev_btn_->setEnabled(current > 0);
        next_btn_->setEnabled(current < total - 1);
    }

    void Clear() {
        preedit_text_.clear();
        cursor_pos_ = 0;
        candidates_.clear();
        current_page_ = 0;
        total_pages_ = 1;
        list_widget_->clear();
        page_label_->setText(" 1/1 ");
        prev_btn_->setEnabled(false);
        next_btn_->setEnabled(false);
        update();
    }

    void ShowAt(int screen_x, int screen_y) {
        // 确保窗口在屏幕内
        QScreen* screen = QGuiApplication::screenAt(QPoint(screen_x, screen_y));
        if (!screen) screen = QGuiApplication::primaryScreen();
        QRect geom = screen->geometry();

        int final_x = std::clamp(screen_x, 0, geom.right() - width());
        int final_y = std::clamp(screen_y, 0, geom.bottom() - height());

        move(final_x, final_y);
        show();
    }

    // 样式
    void SetFontSize(int size) { font_size_ = size; }
    void SetPageSize(int size) { page_size_ = size; }
    void SetShowComment(bool show) { show_comment_ = show; }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 背景
        p.setBrush(QColor(250, 250, 250));
        p.setPen(QColor(200, 200, 200));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        // Preedit 区域
        if (!preedit_text_.isEmpty()) {
            QFont preedit_font = p.font();
            preedit_font.setPointSize(font_size_);
            p.setFont(preedit_font);
            p.setPen(QColor(80, 80, 80));
            p.drawText(10, 24, preedit_text_);
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        // 窗口点击不处理，透传给父窗口
        event->ignore();
    }

    bool event(QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            int key = ke->key();
            if (key == Qt::Key_Escape) {
                hide();
                return true;
            }
        }
        return QWidget::event(event);
    }

private slots:
    void OnItemClicked(QListWidgetItem* item) {
        int row = list_widget_->row(item);
        int global_index = current_page_ * page_size_ + row;
        if (on_candidate_selected_ && row >= 0) {
            on_candidate_selected_(global_index);
        }
    }

    void OnPrevPage() {
        if (current_page_ > 0 && on_page_changed_) {
            on_page_changed_(current_page_ - 1);
        }
    }

    void OnNextPage() {
        if (current_page_ < total_pages_ - 1 && on_page_changed_) {
            on_page_changed_(current_page_ + 1);
        }
    }

private:
    void InitUI() {
        // 主垂直布局
        QVBoxLayout* main_layout = new QVBoxLayout(this);
        main_layout->setContentsMargins(4, 4, 4, 4);
        main_layout->setSpacing(2);

        // 候选词列表
        list_widget_ = new QListWidget(this);
        list_widget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_widget_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_widget_->setFont(QFont("Microsoft YaHei", font_size_));
        list_widget_->setStyleSheet(R"(
            QListWidget {
                background: transparent;
                border: none;
                outline: none;
            }
            QListWidget::item {
                padding: 4px 8px;
                border-radius: 3px;
            }
            QListWidget::item:selected {
                background: #4A90D9;
                color: white;
            }
            QListWidget::item:hover {
                background: #E8F0FE;
            }
        )");
        connect(list_widget_, &QListWidget::itemClicked, this, &CandidateWindowWidget::OnItemClicked);
        main_layout->addWidget(list_widget_);

        // 分页控制栏
        QHBoxLayout* page_layout = new QHBoxLayout();
        page_layout->setSpacing(4);

        prev_btn_ = new QPushButton("<", this);
        prev_btn_->setFixedWidth(30);
        prev_btn_->setEnabled(false);
        connect(prev_btn_, &QPushButton::clicked, this, &CandidateWindowWidget::OnPrevPage);

        page_label_ = new QLabel(" 1/1 ", this);
        page_label_->setAlignment(Qt::AlignCenter);
        page_label_->setStyleSheet("color: #888; font-size: 11px;");

        next_btn_ = new QPushButton(">", this);
        next_btn_->setFixedWidth(30);
        next_btn_->setEnabled(false);
        connect(next_btn_, &QPushButton::clicked, this, &CandidateWindowWidget::OnNextPage);

        page_layout->addWidget(prev_btn_);
        page_layout->addStretch();
        page_layout->addWidget(page_label_);
        page_layout->addStretch();
        page_layout->addWidget(next_btn_);

        main_layout->addLayout(page_layout);

        // 设置初始大小
        resize(400, 180);
    }

    void PopulateList() {
        list_widget_->clear();
        int start = current_page_ * page_size_;
        int end = std::min(start + page_size_, static_cast<int>(candidates_.size()));

        for (int i = start; i < end; ++i) {
            const auto& cand = candidates_[i];
            QString text = QString::fromUtf8(cand.text.c_str());
            if (show_comment_ && !cand.comment.empty()) {
                text += QString("  %1").arg(QString::fromUtf8(cand.comment.c_str()));
            }
            if (cand.is_ai) {
                text += " [AI]";
            }
            auto* item = new QListWidgetItem(QString(" %1. %2").arg(cand.index).arg(text));
            if (cand.is_ai) {
                item->setForeground(QColor(100, 149, 237));  // 蓝色标注 AI 候选词
            }
            list_widget_->addItem(item);
        }

        // 调整窗口高度
        int item_count = list_widget_->count();
        int row_h = list_widget_->sizeHintForRow(0);
        if (row_h <= 0) row_h = 28;
        int list_h = item_count * row_h + 4;
        int page_h = 30;
        resize(width(), list_h + page_h + 8);
    }

    QString  preedit_text_;
    int      cursor_pos_ = 0;
    std::vector<synthorbis::ui::CandidateItem> candidates_;
    int      current_page_ = 0;
    int      total_pages_ = 1;
    int      page_size_ = 9;
    int      font_size_ = 14;
    bool     show_comment_ = true;

    QListWidget* list_widget_ = nullptr;
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QLabel*      page_label_ = nullptr;

    std::function<void(int)> on_candidate_selected_;
    std::function<void(int)> on_page_changed_;
};

// ─────────────────────────────────────────────────────────
// CandidateWindow 公共 API 实现
// ─────────────────────────────────────────────────────────

struct synthorbis::ui::CandidateWindow::Impl {
    CandidateWindowWidget* widget = nullptr;
};

synthorbis::ui::CandidateWindow::CandidateWindow()
    : impl_(new Impl())
{}

synthorbis::ui::CandidateWindow::~CandidateWindow() {
    delete impl_;
}

bool synthorbis::ui::CandidateWindow::Create() {
    if (impl_->widget) return true;  // 已创建
    impl_->widget = new CandidateWindowWidget();
    impl_->widget->SetFontSize(style_.font_size);
    impl_->widget->SetPageSize(style_.page_size);
    impl_->widget->SetShowComment(style_.show_comment);

    if (on_candidate_selected_) {
        impl_->widget->SetOnCandidateSelected(
            [this](int idx) { on_candidate_selected_(idx); });
    }
    if (on_page_changed_) {
        impl_->widget->SetOnPageChanged(
            [this](int page) { on_page_changed_(page); });
    }

    return true;
}

void synthorbis::ui::CandidateWindow::Destroy() {
    if (impl_->widget) {
        impl_->widget->deleteLater();
        impl_->widget = nullptr;
    }
}

void synthorbis::ui::CandidateWindow::Show(int x, int y) {
    if (impl_->widget) {
        impl_->widget->ShowAt(x, y);
    }
}

void synthorbis::ui::CandidateWindow::Hide() {
    if (impl_->widget) {
        impl_->widget->hide();
    }
}

void synthorbis::ui::CandidateWindow::Move(int x, int y) {
    if (impl_->widget) {
        impl_->widget->move(x, y);
    }
}

bool synthorbis::ui::CandidateWindow::IsVisible() const {
    return impl_->widget && impl_->widget->isVisible();
}

void synthorbis::ui::CandidateWindow::UpdatePreedit(const std::string& text, int cursor_pos) {
    if (impl_->widget) {
        impl_->widget->UpdatePreedit(text, cursor_pos);
    }
}

void synthorbis::ui::CandidateWindow::UpdateCandidates(const std::vector<CandidateItem>& candidates) {
    if (impl_->widget) {
        impl_->widget->UpdateCandidates(candidates);
    }
}

void synthorbis::ui::CandidateWindow::UpdatePage(int current_page, int total_pages) {
    if (impl_->widget) {
        impl_->widget->UpdatePage(current_page, total_pages);
    }
}

void synthorbis::ui::CandidateWindow::Clear() {
    if (impl_->widget) {
        impl_->widget->Clear();
    }
}

void synthorbis::ui::CandidateWindow::SetStyle(const CandidateWindowStyle& style) {
    style_ = style;
    if (impl_->widget) {
        impl_->widget->SetFontSize(style.font_size);
        impl_->widget->SetPageSize(style.page_size);
        impl_->widget->SetShowComment(style.show_comment);
    }
}

void synthorbis::ui::CandidateWindow::SetOnCandidateSelected(OnCandidateSelected cb) {
    on_candidate_selected_ = std::move(cb);
    if (impl_->widget) {
        impl_->widget->SetOnCandidateSelected(
            [this](int idx) { if (on_candidate_selected_) on_candidate_selected_(idx); });
    }
}

void synthorbis::ui::CandidateWindow::SetOnPageChanged(OnPageChanged cb) {
    on_page_changed_ = std::move(cb);
    if (impl_->widget) {
        impl_->widget->SetOnPageChanged(
            [this](int page) { if (on_page_changed_) on_page_changed_(page); });
    }
}

QWidget* synthorbis::ui::CandidateWindow::GetQWidget() {
    return impl_->widget;
}

#include "candidate_window.moc"
