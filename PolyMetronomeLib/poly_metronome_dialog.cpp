#include "poly_metronome_dialog.h"
#include <windows.h>

#include "beat_meter_widget.h"
#include "count_in_card.h"
#include "meter_sequence_widget.h"
#include "poly_metronome.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <Qstyle>

// The design here might seem 'weird', but this is intended to be used
// by external apps that don't want the metronome to ever take focus.
// So we have to create our own title bar because the default one
// takes focus. Without that we have no frame, so we make a layout so that
// the dialog still has a border, and add mouse commands to allow
// dragging.


PolyMetronomeDialog::PolyMetronomeDialog(QWidget* parent, bool no_focus)
    : QDialog(parent)
    , metronome_(new PolyMetronome(this))
    , no_focus_(no_focus)
{
    setWindowTitle("PolyMetronome");

    auto* main = new QVBoxLayout(this);

    // Where child widgets get added: into the main layout for the standalone
    // app (regular OS frame), or into a padded content area inside a custom
    // titled / bordered shell for the no-focus embedding case.
    QVBoxLayout* widget_container = main;

    if (no_focus_) {
        // 1px outer margin so the styled border is visible
        main->setContentsMargins(1, 1, 1, 1);
        main->setSpacing(0);

        // 1px border and high-contrast custom close button
        setStyleSheet(
            "PolyMetronomeDialog { border: 1px solid #333; background-color: #222; }"
            "QPushButton#CloseButton { "
            "  border: none; color: #bbb; font-size: 11pt; font-family: 'Segoe UI'; "
            "  background: transparent;"
            "} "
            "QPushButton#CloseButton:hover { background-color: #e81123; color: white; }"
        );

        auto* title_layout = new QHBoxLayout();
        title_layout->setContentsMargins(0, 0, 0, 0);
        title_layout->addStretch();

        auto* close_btn = new QPushButton("✕", this);
        close_btn->setObjectName("CloseButton");
        // Scales based on system-defined title bar height (DPI aware)
        int btnHeight = style()->pixelMetric(QStyle::PM_TitleBarHeight);
        close_btn->setFixedSize(static_cast<int>(btnHeight * 1.5), btnHeight);

        connect(close_btn, &QPushButton::clicked, this, &PolyMetronomeDialog::close);
        title_layout->addWidget(close_btn);
        main->addLayout(title_layout);

        // Content container provides 12px padding inside the border
        auto* content_area = new QWidget(this);
        auto* content_layout = new QVBoxLayout(content_area);
        content_layout->setContentsMargins(12, 0, 12, 12);
        main->addWidget(content_area);

        widget_container = content_layout;
    }

    meter_widget_ = new MeterSequenceWidget(this);
    count_in_ = new CountInCard(this);
    meter_widget_->set_prefix_widget(count_in_);
    widget_container->addWidget(meter_widget_);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 8, 0, 0);

    bpm_dial_ = new BpmDial(this);
    bpm_dial_->setRange(10, 240);
    bpm_dial_->setValue(60);
    bpm_dial_->setNotchesVisible(true);
    bpm_dial_->setFixedSize(200, 200);
    bpm_label_ = new QLabel("60", this);
    bpm_label_->setAlignment(Qt::AlignHCenter);

    QFont bpm_label_font = bpm_label_->font();
    bpm_label_font.setPointSize(bpm_label_font.pointSize() + 6);
    bpm_label_font.setBold(true);
    bpm_label_->setFont(bpm_label_font);
    bpm_label_->setFixedWidth(bpm_label_->fontMetrics().horizontalAdvance("240") + 6);

    auto* bpm_dec = new QPushButton(QStringLiteral("−"), this);
    auto* bpm_inc = new QPushButton(QStringLiteral("+"), this);
    bpm_dec->setAutoRepeat(true);
    bpm_inc->setAutoRepeat(true);
    bpm_dec->setFixedSize(28, 28);
    bpm_inc->setFixedSize(28, 28);
    QFont bpm_btn_font = bpm_dec->font();
    bpm_btn_font.setPointSize(bpm_btn_font.pointSize() + 2);
    bpm_btn_font.setBold(true);
    bpm_dec->setFont(bpm_btn_font);
    bpm_inc->setFont(bpm_btn_font);
    const char* card_btn_qss =
        "QPushButton { background-color: rgb(45,48,55); color: rgb(230,230,235);"
        "  border: 1.5px solid rgb(80,85,95); border-radius: 5px; outline: none; padding: 0; }"
        "QPushButton:hover { background-color: rgb(60,65,75); border-color: rgb(120,160,200); }"
        "QPushButton:focus { outline: none; border-color: rgb(80,85,95); }";
    bpm_dec->setStyleSheet(card_btn_qss);
    bpm_inc->setStyleSheet(card_btn_qss);
    connect(bpm_dec, &QPushButton::clicked, this, [this]() {
        bpm_dial_->setValue(bpm_dial_->value() - 1);
        on_bpm_committed(bpm_dial_->value());
    });
    connect(bpm_inc, &QPushButton::clicked, this, [this]() {
        bpm_dial_->setValue(bpm_dial_->value() + 1);
        on_bpm_committed(bpm_dial_->value());
    });

    auto* bpm_row = new QWidget(this);
    auto* bpm_v = new QVBoxLayout(bpm_row);
    bpm_v->setContentsMargins(0, 0, 0, 0);
    auto* label_row = new QHBoxLayout;
    label_row->setSpacing(8);
    label_row->addStretch();
    label_row->addWidget(bpm_dec);
    label_row->addWidget(bpm_label_);
    label_row->addWidget(bpm_inc);
    label_row->addStretch();
    bpm_v->addLayout(label_row);
    auto* dial_row = new QHBoxLayout;
    dial_row->addStretch();
    dial_row->addWidget(bpm_dial_);
    dial_row->addStretch();
    bpm_v->addLayout(dial_row);

    beat_meter_ = new BeatMeterWidget(this);
    beat_meter_->set_bpm(bpm_dial_->value());

    auto* bpm_section = new QHBoxLayout;
    bpm_section->setSpacing(8);
    bpm_section->addWidget(bpm_row);
    bpm_section->addWidget(beat_meter_);
    form->addRow(bpm_section);

    static const char* slider_qss =
        "QSlider::groove:vertical {"
        "  background: #141414;"
        "  width: 6px;"
        "  border: 1px solid #050505;"
        "  border-radius: 3px;"
        "}"
        "QSlider::sub-page:vertical, QSlider::add-page:vertical {"
        "  background: transparent;"
        "}"
        "QSlider::handle:vertical {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "      stop:0 #888888,"
        "      stop:0.45 #b0b0b0,"
        "      stop:0.48 #1a1a1a,"
        "      stop:0.52 #1a1a1a,"
        "      stop:0.55 #b0b0b0,"
        "      stop:1 #555555);"
        "  border: 1px solid #0a0a0a;"
        "  height: 26px;"
        "  width: 34px;"
        "  margin: 0 -14px;"
        "  border-radius: 4px;"
        "}"
        "QSlider::handle:vertical:hover {"
        "  border: 1px solid #d0a040;"
        "}"
        "QSlider::handle:vertical:disabled {"
        "  background: #303030;"
        "  border: 1px solid #0a0a0a;"
        "}";

    auto make_slider = [this](int default_pct) {
        auto* s = new QSlider(Qt::Vertical, this);
        s->setRange(0, 100);
        s->setValue(default_pct);
        s->setFixedSize(36, 110);
        s->setStyleSheet(slider_qss);
        return s;
    };

    beat_volume_ = make_slider(100);
    eighth_volume_ = make_slider(0);
    sixteenth_volume_ = make_slider(0);
    triplet_volume_ = make_slider(0);
    quintuplet_volume_ = make_slider(0);
    accent_volume_ = make_slider(100);
    master_volume_ = make_slider(100);

    auto* sliders_row = new QHBoxLayout;
    sliders_row->setContentsMargins(0, 8, 0, 0);
    sliders_row->setSpacing(4);

    auto add_slider_column = [&](QSlider* s, const QString& text, QLabel** out_label = nullptr) {
        auto* col = new QVBoxLayout;
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(2);
        col->setAlignment(Qt::AlignHCenter);
        auto* sl_wrap = new QHBoxLayout;
        sl_wrap->setContentsMargins(0, 0, 0, 0);
        sl_wrap->addStretch();
        sl_wrap->addWidget(s);
        sl_wrap->addStretch();
        col->addLayout(sl_wrap);
        auto* label = new QLabel(text, this);
        label->setAlignment(Qt::AlignHCenter);
        col->addWidget(label);
        if (out_label)
            *out_label = label;
        sliders_row->addLayout(col, 1);
    };

    add_slider_column(beat_volume_, "Beat");
    add_slider_column(eighth_volume_, "Eighth", &eighth_label_);
    add_slider_column(sixteenth_volume_, "Sixteenth", &sixteenth_label_);
    add_slider_column(triplet_volume_, "Triplet");
    add_slider_column(quintuplet_volume_, "Quintuplet");
    add_slider_column(accent_volume_, "Accent");
    add_slider_column(master_volume_, "Master");

    form->addRow(sliders_row);

    sound_mode_button_ = new QPushButton(this);
    sound_mode_button_->setCheckable(true);
    sound_mode_button_->setChecked(true);
    sound_mode_button_->hide();

    start_stop_ = new QPushButton("Start", this);
    start_stop_->setFixedSize(64, 64);

    auto* start_row = new QHBoxLayout;
    start_row->addStretch();
    start_row->addWidget(start_stop_);
    form->addRow(start_row);

    widget_container->addLayout(form);

    connect(meter_widget_, &MeterSequenceWidget::sequence_changed, this, &PolyMetronomeDialog::on_sequence_changed);
    connect(bpm_dial_, &QDial::valueChanged, this, &PolyMetronomeDialog::on_bpm_changed);
    connect(bpm_dial_, &BpmDial::value_committed, this, &PolyMetronomeDialog::on_bpm_committed);
    connect(master_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_master_volume_changed);
    connect(beat_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_beat_volume_changed);
    connect(eighth_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_eighth_volume_changed);
    connect(sixteenth_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_sixteenth_volume_changed);
    connect(triplet_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_triplet_volume_changed);
    connect(quintuplet_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_quintuplet_volume_changed);
    connect(accent_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_accent_volume_changed);
    connect(sound_mode_button_, &QPushButton::toggled, this, &PolyMetronomeDialog::on_sound_mode_toggled);
    connect(start_stop_, &QPushButton::clicked, this, &PolyMetronomeDialog::on_start_stop_clicked);
    connect(count_in_, &CountInCard::value_changed, this, [this](int) { emit state_changed(); });
    beat_meter_->set_metronome(metronome_);

    on_sequence_changed(meter_widget_->sequence());
    on_bpm_changed(bpm_dial_->value());
    on_bpm_committed(bpm_dial_->value());
    on_master_volume_changed(master_volume_->value());
    on_beat_volume_changed(beat_volume_->value());
    on_eighth_volume_changed(eighth_volume_->value());
    on_sixteenth_volume_changed(sixteenth_volume_->value());
    on_triplet_volume_changed(triplet_volume_->value());
    on_quintuplet_volume_changed(quintuplet_volume_->value());
    on_accent_volume_changed(accent_volume_->value());
    on_sound_mode_toggled(sound_mode_button_->isChecked());

    adjustSize();
    int min_w = 490;
    if (width() < min_w)
        resize(min_w, height());
    setMinimumWidth(min_w);
    setFixedHeight(height());

    if (no_focus_) {
        // Frameless + never-activating tool palette flags
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);

        for (QWidget* w : findChildren<QWidget*>()) {
            w->setFocusPolicy(Qt::NoFocus);
            w->setAttribute(Qt::WA_Hover, false);
        }

        installEventFilter(this);
        for (QWidget* w : findChildren<QWidget*>())
            w->installEventFilter(this);

        // Windows-specific WS_EX_NOACTIVATE — must come after winId() forces
        // the native HWND to exist so we can modify its extended style.
        HWND hwnd = (HWND)winId();
        LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        ex = (ex | WS_EX_NOACTIVATE) & ~WS_EX_TOPMOST;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
    }
}


bool PolyMetronomeDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (!no_focus_)
        return QDialog::eventFilter(obj, event);
    Q_UNUSED(obj);
    switch (event->type()) {
    case QEvent::FocusIn:
    case QEvent::FocusAboutToChange:
    case QEvent::WindowActivate:
        return true; // swallow — never let focus / activation land here
    default:
        return false;
    }
}


PolyMetronomeDialog::~PolyMetronomeDialog()
{
    if (metronome_->is_running())
        metronome_->stop();
}

PolyMetronomeState PolyMetronomeDialog::state() const
{
    PolyMetronomeState s;
    s.bpm = bpm_dial_->value();
    s.master_volume = master_volume_->value() / 100.0f;
    s.accent_volume = accent_volume_->value() / 100.0f;
    s.beat_volume = beat_volume_->value() / 100.0f;
    s.eighth_volume = eighth_volume_->value() / 100.0f;
    s.sixteenth_volume = sixteenth_volume_->value() / 100.0f;
    s.triplet_volume = triplet_volume_->value() / 100.0f;
    s.quintuplet_volume = quintuplet_volume_->value() / 100.0f;
    s.mono_mode = sound_mode_button_->isChecked();
    s.count_in = count_in_->value();
    s.sequence = meter_widget_->sequence();
    return s;
}

void PolyMetronomeDialog::apply_state(const PolyMetronomeState& s)
{
    {
        QSignalBlocker b1(bpm_dial_);
        QSignalBlocker b2(master_volume_);
        QSignalBlocker b3(accent_volume_);
        QSignalBlocker b4(beat_volume_);
        QSignalBlocker b5(eighth_volume_);
        QSignalBlocker b6(sixteenth_volume_);
        QSignalBlocker b7(triplet_volume_);
        QSignalBlocker b8(quintuplet_volume_);
        QSignalBlocker b9(sound_mode_button_);
        QSignalBlocker b10(meter_widget_);
        QSignalBlocker b11(count_in_);

        bpm_dial_->setValue(s.bpm);
        master_volume_->setValue(static_cast<int>(s.master_volume * 100.0f));
        accent_volume_->setValue(static_cast<int>(s.accent_volume * 100.0f));
        beat_volume_->setValue(static_cast<int>(s.beat_volume * 100.0f));
        eighth_volume_->setValue(static_cast<int>(s.eighth_volume * 100.0f));
        sixteenth_volume_->setValue(static_cast<int>(s.sixteenth_volume * 100.0f));
        triplet_volume_->setValue(static_cast<int>(s.triplet_volume * 100.0f));
        quintuplet_volume_->setValue(static_cast<int>(s.quintuplet_volume * 100.0f));
        sound_mode_button_->setChecked(s.mono_mode);
        count_in_->set_value(s.count_in);
        meter_widget_->set_sequence(s.sequence);
    }

    bpm_label_->setText(QString::number(s.bpm));
    sound_mode_button_->setText(s.mono_mode ? "Single Pitch" : "Multi Pitch");

    metronome_->set_bpm(s.bpm);
    metronome_->set_master_volume(s.master_volume);
    metronome_->set_accent_volume(s.accent_volume);
    metronome_->set_beat_volume(s.beat_volume);
    metronome_->set_eighth_volume(s.eighth_volume);
    metronome_->set_sixteenth_volume(s.sixteenth_volume);
    metronome_->set_triplet_volume(s.triplet_volume);
    metronome_->set_quintuplet_volume(s.quintuplet_volume);
    metronome_->set_mono_mode(s.mono_mode);
    metronome_->set_sequence(s.sequence);
    update_subdivision_slider_states();
}

void PolyMetronomeDialog::closeEvent(QCloseEvent* event)
{
    if (metronome_->is_running()) {
        metronome_->stop();
        start_stop_->setText("Start");
        beat_meter_->set_running(false);
    }
    QDialog::closeEvent(event);
}

void PolyMetronomeDialog::hideEvent(QHideEvent* event)
{
    if (metronome_->is_running()) {
        metronome_->stop();
        start_stop_->setText("Start");
        beat_meter_->set_running(false);
    }
    QDialog::hideEvent(event);
}

void PolyMetronomeDialog::on_start_stop_clicked()
{
    if (metronome_->is_running()) {
        metronome_->stop();
        start_stop_->setText("Start");
        beat_meter_->set_running(false);
    }
    else {
        beat_meter_->set_running(true);
        metronome_->set_count_in(count_in_->value());
        metronome_->start();
        start_stop_->setText("Stop");
    }
}

void PolyMetronomeDialog::on_bpm_changed(int bpm)
{
    bpm_label_->setText(QString::number(bpm));
    beat_meter_->set_bpm(bpm);
}

void PolyMetronomeDialog::on_bpm_committed(int bpm)
{
    metronome_->set_bpm(bpm);
    emit state_changed();
}

void PolyMetronomeDialog::set_bpm(int bpm)
{
    bpm_dial_->setValue(bpm);
    on_bpm_committed(bpm_dial_->value());
}

void PolyMetronomeDialog::toggle_start_stop()
{
    on_start_stop_clicked();
}

void PolyMetronomeDialog::on_master_volume_changed(int v)
{
    metronome_->set_master_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_beat_volume_changed(int v)
{
    metronome_->set_beat_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_eighth_volume_changed(int v)
{
    metronome_->set_eighth_volume(eighth_volume_->isEnabled() ? v / 100.0f : 0.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_sixteenth_volume_changed(int v)
{
    metronome_->set_sixteenth_volume(sixteenth_volume_->isEnabled() ? v / 100.0f : 0.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_triplet_volume_changed(int v)
{
    metronome_->set_triplet_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_quintuplet_volume_changed(int v)
{
    metronome_->set_quintuplet_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_accent_volume_changed(int v)
{
    metronome_->set_accent_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_sequence_changed(const MeterSequence& seq)
{
    metronome_->set_sequence(seq);
    beat_meter_->set_sequence(seq);
    update_subdivision_slider_states();
    emit state_changed();
}

void PolyMetronomeDialog::update_subdivision_slider_states()
{
    const MeterSequence seq = meter_widget_->sequence();
    bool disable_eighth = !seq.measures.empty();
    bool disable_sixteenth = !seq.measures.empty();
    for (const MeasureSpec& m : seq.measures) {
        int nv = m.note_value > 0 ? m.note_value : 4;
        if (nv < 8)
            disable_eighth = false;
        if (nv < 16)
            disable_sixteenth = false;
    }

    auto apply = [](QSlider* s, QLabel* label, bool disable) {
        s->setEnabled(!disable);
        if (label)
            label->setEnabled(!disable);
    };
    apply(eighth_volume_, eighth_label_, disable_eighth);
    apply(sixteenth_volume_, sixteenth_label_, disable_sixteenth);

    metronome_->set_eighth_volume(eighth_volume_->isEnabled() ? eighth_volume_->value() / 100.0f : 0.0f);
    metronome_->set_sixteenth_volume(sixteenth_volume_->isEnabled() ? sixteenth_volume_->value() / 100.0f : 0.0f);
}

void PolyMetronomeDialog::on_sound_mode_toggled(bool checked)
{
    metronome_->set_mono_mode(checked);
    sound_mode_button_->setText(checked ? "Single Pitch" : "Multi Pitch");
    emit state_changed();
}


void PolyMetronomeDialog::mousePressEvent(QMouseEvent* event)
{
    if (!no_focus_) {
        QDialog::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        // Dragging is restricted to the top area defined by the system title bar height
        int titleHeight = style()->pixelMetric(QStyle::PM_TitleBarHeight);
        if (event->position().y() < titleHeight) {
            drag_position_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
}

void PolyMetronomeDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (!no_focus_) {
        QDialog::mouseMoveEvent(event);
        return;
    }
    if (event->buttons() & Qt::LeftButton && !drag_position_.isNull()) {
        move(event->globalPosition().toPoint() - drag_position_);
        event->accept();
    }
}

void PolyMetronomeDialog::mouseReleaseEvent(QMouseEvent* event)
{
    if (!no_focus_) {
        QDialog::mouseReleaseEvent(event);
        return;
    }
    drag_position_ = QPoint();
    QDialog::mouseReleaseEvent(event);
}

bool PolyMetronomeDialog::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (no_focus_ && eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            const int border = 6;
            int screen_x = (short)(msg->lParam & 0xFFFF);
            RECT r;
            GetWindowRect(msg->hwnd, &r);
            int x = screen_x - r.left;
            int w = r.right - r.left;

            if (x < border) {
                *result = HTLEFT;
                return true;
            }
            if (x >= w - border) {
                *result = HTRIGHT;
                return true;
            }
        }
    }
    return QDialog::nativeEvent(eventType, message, result);
}