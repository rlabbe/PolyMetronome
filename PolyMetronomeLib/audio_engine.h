#pragma once

#include "meter.h"

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <array>
#include <atomic>
#include <optional>
#include <vector>

class QAudioSink;

class AudioEngine : public QIODevice
{
    Q_OBJECT

public:
    enum ClickType : int
    {
        Accent = 0,
        Beat,
        Eighth,
        Sixteenth,
        Triplet,
        Quintuplet,
        NumTypes
    };

    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    void start();
    void stop();
    bool is_running() const;

    void set_bpm(int bpm);
    void set_master_volume(float vol);
    void set_volume(ClickType type, float vol);
    void set_sequence(const MeterSequence& seq);
    void set_mono_mode(bool on);
    void set_count_in(int beats);

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

    struct TickInfo { int measure; int beat; };
    qint64 processed_samples() const;
    std::optional<TickInfo> pop_ticks_through(qint64 sample_index);

protected:
    qint64 readData(char* data, qint64 max_len) override;
    qint64 writeData(const char* data, qint64 len) override;

private:
    static constexpr int subs_per_beat_ = 60;
    static constexpr size_t ring_capacity_ = 256 * 1024;

    struct ActiveClick
    {
        size_t start_in_buffer;
        size_t pos_in_click;
        float gain;
        int type;
    };

    void rebuild_click_samples();
    void recompute_sps_locked();
    void recompute_tick_sps_locked();
    void schedule_ticks_to_locked(qint64 target_sample);
    const MeasureSpec& current_measure_locked() const;
    bool is_group_boundary_locked(int beat_in_measure) const;
    void create_sink();
    void destroy_sink();
    void produce_audio();

    QAudioFormat format_;
    int sample_rate_ = 44100;
    QAudioSink* sink_ = nullptr;

    std::array<std::vector<float>, NumTypes> click_samples_;
    std::vector<float> mono_sample_;
    std::vector<ActiveClick> active_clicks_;
    std::vector<float> scratch_;

    // Ring buffer: producer writes formatted bytes, readData copies them out.
    // Single-producer single-consumer with atomic cursors — no lock needed on
    // the data path.
    std::vector<char> ring_;
    std::atomic<size_t> ring_write_{0};
    std::atomic<size_t> ring_read_{0};

    // Producer thread
    QThread* producer_thread_ = nullptr;
    std::atomic<bool> producing_{false};
    QMutex producer_wake_mutex_;
    QWaitCondition producer_wake_;

    mutable QMutex mutex_;
    int bpm_ = 60;
    MeterSequence sequence_ = MeterSequence::default_4_4();
    int seq_measure_idx_ = 0;
    size_t subticks_in_measure_ = 0;
    double samples_per_subtick_ = 0.0;
    size_t position_samples_ = 0;
    size_t anchor_position_ = 0;
    float master_volume_ = 1.0f;
    bool mono_mode_ = true;
    std::array<float, NumTypes> volumes_ = { { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
    int count_in_beats_ = 0;
    int count_in_subtick_ = 0;
    size_t count_in_anchor_ = 0;
    double count_in_sps_ = 0.0;
    uint32_t dither_state_ = 0x12345678u;
    double keepalive_phase_ = 0.0;

    struct ScheduledTick { qint64 play_sample; int measure; int beat; };
    std::vector<ScheduledTick> pending_ticks_;

    int tick_seq_measure_idx_ = 0;
    size_t tick_subticks_in_measure_ = 0;
    size_t tick_anchor_position_ = 0;
    double tick_samples_per_subtick_ = 0.0;
    int tick_count_in_subtick_ = 0;
    double tick_count_in_sps_ = 0.0;
    size_t tick_count_in_anchor_ = 0;

    QElapsedTimer wall_clock_;
    mutable qint64 last_processed_us_ = 0;
    mutable qint64 last_wall_ns_ = 0;
};
