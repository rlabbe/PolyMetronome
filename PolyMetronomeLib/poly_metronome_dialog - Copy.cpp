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


PolyMetronomeDialog::PolyMetronomeDialog(QWidget* parent)
    : QDialog(parent)
    , metronome_(new PolyMetronome(this))
{
    setWindowTitle("PolyMetronome");

    // Main layout with 1px margin to show the border
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(1, 1, 1, 1);
    main->setSpacing(0);

    // Provide the 1px border and a high-contrast close button
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

    // Content container to provide 12px padding inside the border
    auto* content_area = new QWidget(this);
    auto* content_layout = new QVBoxLayout(content_area);
    content_layout->setContentsMargins(12, 0, 12, 12);
    main->addWidget(content_area);

    meter_widget_ = new MeterSequenceWidget(this);
    count_in_ = new CountInCard(this);
    meter_widget_->set_prefix_widget(count_in_);
    content_layout->addWidget(meter_widget_);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 8, 0, 0);

    bpm_dial_ = new BpmDial(this);
    bpm_dial_->setRange(10, 240);
    bpm_dial_->setValue(60);
    bpm_dial_->setNotchesVisible(true);
    bpm_dial_->setFixedSize(160, 160);
    bpm_label_ = new QLabel("60", this);
    bpm_label_->setAlignment(Qt::AlignHCenter);

    QFont bpm_label_font = bpm_label_->font();
    bpm_label_font.setPointSize(bpm_label_font.pointSize() + 6);
    bpm_label_font.setBold(true);
    bpm_label_->setFont(bpm_label_font);

    auto* bpm_dec = new QPushButton(QStringLiteral("−"), this);
    auto* bpm_inc = new QPushButton(QStringLiteral("+"), this);
    bpm_dec->setAutoRepeat(true);
    bpm_inc->setAutoRepeat(true);
    bpm_dec->setFixedSize(48, 48);
    bpm_inc->setFixedSize(48, 48);
    QFont bpm_btn_font = bpm_dec->font();
    bpm_btn_font.setPointSize(bpm_btn_font.pointSize() + 4);
    bpm_btn_font.setBold(true);
    bpm_dec->setFont(bpm_btn_font);
    bpm_inc->setFont(bpm_btn_font);
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
    bpm_v->addWidget(bpm_label_);
    auto* dial_row = new QHBoxLayout;
    dial_row->addStretch();
    dial_row->addWidget(bpm_dec);
    dial_row->addWidget(bpm_dial_);
    dial_row->addWidget(bpm_inc);
    dial_row->addStretch();
    bpm_v->addLayout(dial_row);

    beat_meter_ = new BeatMeterWidget(this);
    beat_meter_->set_bpm(bpm_dial_->value());

    auto* bpm_section = new QHBoxLayout;
    bpm_section->setSpacing(8);
    bpm_section->addWidget(bpm_row);
    bpm_section->addWidget(beat_meter_);
    form->addRow(bpm_section);

    auto make_slider = [this](int default_pct) {
        auto* s = new QSlider(Qt::Horizontal, this);
        s->setRange(0, 100);
        s->setValue(default_pct);
        return s;
    };

    beat_volume_ = make_slider(100);
    eighth_volume_ = make_slider(0);
    sixteenth_volume_ = make_slider(0);
    triplet_volume_ = make_slider(0);
    quintuplet_volume_ = make_slider(0);
    accent_volume_ = make_slider(100);
    master_volume_ = make_slider(100);

    form->addRow("Beat", beat_volume_);
    form->addRow("Eighth", eighth_volume_);
    form->addRow("Sixteenth", sixteenth_volume_);
    form->addRow("Triplet", triplet_volume_);
    form->addRow("Quintuplet", quintuplet_volume_);
    form->addRow("Accent", accent_volume_);
    form->addRow("Master", master_volume_);

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

    content_layout->addLayout(form);

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
    setFixedHeight(height());

    // Focus prevention and frameless flags
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);

    for (QWidget* w : findChildren<QWidget*>()) {
        w->setFocusPolicy(Qt::NoFocus);
        w->setAttribute(Qt::WA_Hover, false);
    }

    installEventFilter(this);
    for (QWidget* w : findChildren<QWidget*>())
        w->installEventFilter(this);

    // Apply Windows-specific WS_EX_NOACTIVATE
    HWND hwnd = (HWND)winId();
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_NOACTIVATE);
}


bool PolyMetronomeDialog::eventFilter(QObject* obj, QEvent* event)
{
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
    metronome_->set_eighth_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_sixteenth_volume_changed(int v)
{
    metronome_->set_sixteenth_volume(v / 100.0f);
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
    emit state_changed();
}

void PolyMetronomeDialog::on_sound_mode_toggled(bool checked)
{
    metronome_->set_mono_mode(checked);
    sound_mode_button_->setText(checked ? "Single Pitch" : "Multi Pitch");
    emit state_changed();
}


void PolyMetronomeDialog::mousePressEvent(QMouseEvent* event)
{
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
    if (event->buttons() & Qt::LeftButton && !drag_position_.isNull()) {
        move(event->globalPosition().toPoint() - drag_position_);
        event->accept();
    }
}

void PolyMetronomeDialog::mouseReleaseEvent(QMouseEvent* event)
{
    drag_position_ = QPoint();
    QDialog::mouseReleaseEvent(event);
}