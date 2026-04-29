#pragma once

#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <array>
#include <vector>

class QAudioSink;

class AudioEngine : public QIODevice
{
    Q_OBJECT

public:
    enum ClickType : int
    {
        Accent = 0,
        Quarter,
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
    void set_beats_per_measure(int n);
    void set_mono_mode(bool on);

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char* data, qint64 max_len) override;
    qint64 writeData(const char* data, qint64 len) override;

private:
    static constexpr int subs_per_quarter_ = 60;

    struct ActiveClick
    {
        qint64 start_in_buffer;
        size_t pos_in_click;
        float gain;
        int type;
    };

    void rebuild_click_samples();
    void recompute_sps_locked();

    QAudioSink* sink_ = nullptr;
    QAudioFormat format_;
    int sample_rate_ = 44100;

    std::array<std::vector<float>, NumTypes> click_samples_;
    std::vector<float> mono_sample_;
    std::vector<ActiveClick> active_clicks_;
    std::vector<float> scratch_;

    mutable QMutex mutex_;
    int bpm_ = 60;
    int beats_per_measure_ = 4;
    double samples_per_subtick_ = 0.0;
    qint64 position_samples_ = 0;
    qint64 anchor_position_ = 0;
    qint64 subticks_since_anchor_ = 0;
    float master_volume_ = 1.0f;
    bool mono_mode_ = true;
    std::array<float, NumTypes> volumes_ = { { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
};
