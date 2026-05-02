#include "audio_engine.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QDebug>
#include <QMediaDevices>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>

AudioEngine::AudioEngine(QObject* parent)
    : QIODevice(parent)
{
    QAudioFormat desired;
    desired.setSampleRate(sample_rate_);
    desired.setChannelCount(1);
    desired.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    qInfo() << "AudioEngine: default output device:" << device.description();
    if (device.isNull())
        qWarning() << "AudioEngine: no default audio output device available";

    format_ = desired;
    if (!device.isFormatSupported(format_)) {
        qWarning() << "AudioEngine: Int16 mono 44100 not supported, using preferred";
        format_ = device.preferredFormat();
    }
    sample_rate_ = format_.sampleRate();
    qInfo() << "AudioEngine: format rate=" << format_.sampleRate()
            << "channels=" << format_.channelCount()
            << "fmt=" << format_.sampleFormat();

    rebuild_click_samples();
    recompute_sps_locked();

    sink_ = new QAudioSink(device, format_, this);
    connect(sink_, &QAudioSink::stateChanged, this, [this](QAudio::State s) {
        qInfo() << "AudioEngine: sink state ->" << s << "error=" << sink_->error();
    });
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::rebuild_click_samples()
{
    static constexpr float freqs[NumTypes] = { 2500.0f, 1500.0f, 1200.0f, 950.0f, 750.0f, 500.0f };
    constexpr float mono_freq = 1500.0f;
    constexpr int duration_ms = 30;
    constexpr float decay = 80.0f;
    int n = sample_rate_ * duration_ms / 1000;
    for (int t = 0; t < NumTypes; ++t) {
        click_samples_[t].resize(n);
        for (int i = 0; i < n; ++i) {
            float ts = static_cast<float>(i) / sample_rate_;
            float env = std::exp(-ts * decay);
            click_samples_[t][i] = std::sin(2.0f * std::numbers::pi_v<float> * freqs[t] * ts) * env;
        }
    }
    mono_sample_.resize(n);
    for (int i = 0; i < n; ++i) {
        float ts = static_cast<float>(i) / sample_rate_;
        float env = std::exp(-ts * decay);
        mono_sample_[i] = std::sin(2.0f * std::numbers::pi_v<float> * mono_freq * ts) * env;
    }
}

const MeasureSpec& AudioEngine::current_measure_locked() const
{
    static const MeasureSpec fallback{ 4, 4, 1, {} };
    if (sequence_.empty())
        return fallback;
    auto* m = sequence_.at_absolute(seq_measure_idx_);
    return m ? *m : fallback;
}

void AudioEngine::recompute_sps_locked()
{
    if (bpm_ <= 0)
        return;
    int denom = current_measure_locked().denominator;
    if (denom <= 0)
        denom = 4;
    // Sub-tick is 1/60 of a beat. A beat is 1/denominator of a whole note.
    // BPM = quarter notes per minute (always). So time per beat = (60/bpm)*(4/denom) sec.
    samples_per_subtick_ = static_cast<double>(sample_rate_) * 4.0 / (static_cast<double>(bpm_) * static_cast<double>(denom));
}

bool AudioEngine::is_group_boundary_locked(int beat_in_measure) const
{
    if (beat_in_measure <= 0)
        return false;
    const MeasureSpec& m = current_measure_locked();
    if (m.grouping.is_empty())
        return false;
    int cum = 0;
    for (size_t i = 0; i < m.grouping.sizes.size(); ++i) {
        cum += m.grouping.sizes[i];
        if (cum == beat_in_measure && i + 1 < m.grouping.sizes.size())
            return true;
    }
    return false;
}

void AudioEngine::start()
{
    {
        QMutexLocker lock(&mutex_);
        active_clicks_.clear();
        position_samples_ = 0;
        // Delay the first beat by ~50ms so the audio backend's pull pipeline
        // is fully primed before any click is scheduled. Without this, on
        // first start (after dialog open) WASAPI's startup buffering can
        // produce a duplicate first click.
        qint64 initial_offset = sample_rate_ / 20;
        count_in_sps_ = static_cast<double>(sample_rate_) * 60.0 / (static_cast<double>(bpm_) * subs_per_beat_);
        count_in_anchor_ = initial_offset;
        count_in_subtick_ = 0;
        anchor_position_ = initial_offset + static_cast<qint64>(std::round(count_in_beats_ * subs_per_beat_ * count_in_sps_));
        seq_measure_idx_ = 0;
        subticks_in_measure_ = 0;
        keepalive_phase_ = 0.0;
        if (sequence_.empty())
            sequence_ = MeterSequence::default_4_4();
        recompute_sps_locked();
    }
    open(QIODevice::ReadOnly);
    sink_->start(this);
    qInfo() << "AudioEngine::start: sink state=" << sink_->state()
            << "error=" << sink_->error()
            << "bufferSize=" << sink_->bufferSize();
}

void AudioEngine::stop()
{
    sink_->stop();
    close();
    QMutexLocker lock(&mutex_);
    active_clicks_.clear();
}

bool AudioEngine::is_running() const
{
    auto s = sink_->state();
    return s == QAudio::ActiveState || s == QAudio::IdleState;
}

void AudioEngine::set_bpm(int bpm)
{
    if (bpm <= 0)
        return;
    QMutexLocker lock(&mutex_);
    if (bpm == bpm_)
        return;
    bpm_ = bpm;
    recompute_sps_locked();
    anchor_position_ = position_samples_;
    subticks_in_measure_ = 0;
}

void AudioEngine::set_master_volume(float vol)
{
    QMutexLocker lock(&mutex_);
    master_volume_ = vol;
}

void AudioEngine::set_volume(ClickType type, float vol)
{
    if (type < 0 || type >= NumTypes)
        return;
    QMutexLocker lock(&mutex_);
    volumes_[type] = vol;
}

void AudioEngine::set_sequence(const MeterSequence& seq)
{
    QMutexLocker lock(&mutex_);
    sequence_ = seq;
    if (sequence_.empty())
        sequence_ = MeterSequence::default_4_4();
    seq_measure_idx_ = 0;
    subticks_in_measure_ = 0;
    anchor_position_ = position_samples_;
    recompute_sps_locked();
}

void AudioEngine::set_mono_mode(bool on)
{
    QMutexLocker lock(&mutex_);
    mono_mode_ = on;
}

void AudioEngine::set_count_in(int beats)
{
    QMutexLocker lock(&mutex_);
    count_in_beats_ = std::max(0, beats);
}

qint64 AudioEngine::readData(char* data, qint64 max_len)
{
    int channels = format_.channelCount();
    int bytes_per_sample = format_.bytesPerSample();
    if (channels <= 0 || bytes_per_sample <= 0)
        return 0;

    int bytes_per_frame = channels * bytes_per_sample;
    qint64 n_frames = max_len / bytes_per_frame;
    if (n_frames <= 0)
        return 0;

    scratch_.assign(static_cast<size_t>(n_frames), 0.0f);

    QMutexLocker lock(&mutex_);

    auto schedule = [&](qint64 spos, ClickType t, float gain_mult = 1.0f) {
        float v = volumes_[t] * gain_mult;
        if (v <= 0.0f)
            return;
        ActiveClick c;
        c.start_in_buffer = spos - position_samples_;
        c.pos_in_click = 0;
        c.gain = v;
        c.type = t;
        active_clicks_.push_back(c);
    };

    qint64 buffer_end = position_samples_ + n_frames;

    {
        int total_ci_subs = count_in_beats_ * subs_per_beat_;
        while (count_in_subtick_ < total_ci_subs) {
            qint64 spos = count_in_anchor_ + static_cast<qint64>(std::round(count_in_subtick_ * count_in_sps_));
            if (spos >= buffer_end)
                break;
            if (spos >= position_samples_ && count_in_subtick_ % subs_per_beat_ == 0)
                schedule(spos, Beat);
            ++count_in_subtick_;
        }
    }

    while (true) {
        const MeasureSpec& m = current_measure_locked();
        int curr_numer = m.numerator > 0 ? m.numerator : 4;
        qint64 subs_in_curr_measure = static_cast<qint64>(curr_numer) * subs_per_beat_;

        qint64 spos = anchor_position_ + static_cast<qint64>(std::round(static_cast<double>(subticks_in_measure_) * samples_per_subtick_));
        if (spos >= buffer_end)
            break;

        if (spos >= position_samples_) {
            int sub_in_beat = static_cast<int>(subticks_in_measure_ % subs_per_beat_);
            int beat_in_measure = static_cast<int>(subticks_in_measure_ / subs_per_beat_);
            // 60 sub-ticks per beat. Subdivisions are beat-relative:
            //   0           : the beat (Beat click; Accent on beat 0; SubAccent on group starts)
            //   30          : eighth offbeat (2 per beat)
            //   15, 45      : sixteenth offbeats (4 per beat)
            //   20, 40      : triplet offbeats (3 per beat)
            //   12,24,36,48 : quintuplet offbeats (5 per beat)
            if (sub_in_beat == 0) {
                emit beat_tick(seq_measure_idx_, beat_in_measure);
                schedule(spos, Beat);
                if (beat_in_measure == 0)
                    schedule(spos, Accent);
                else if (is_group_boundary_locked(beat_in_measure))
                    schedule(spos, Accent, 0.5f);
            }
            else if (sub_in_beat == 30)
                schedule(spos, Eighth);
            else if (sub_in_beat == 15 || sub_in_beat == 45)
                schedule(spos, Sixteenth);
            else if (sub_in_beat == 20 || sub_in_beat == 40)
                schedule(spos, Triplet);
            else if (sub_in_beat == 12 || sub_in_beat == 24 || sub_in_beat == 36 || sub_in_beat == 48)
                schedule(spos, Quintuplet);
        }

        ++subticks_in_measure_;

        if (subticks_in_measure_ >= subs_in_curr_measure) {
            anchor_position_ = anchor_position_ + static_cast<qint64>(std::round(static_cast<double>(subs_in_curr_measure) * samples_per_subtick_));
            subticks_in_measure_ = 0;
            int total = sequence_.total_measures();
            if (total <= 0)
                total = 1;
            seq_measure_idx_ = (seq_measure_idx_ + 1) % total;
            recompute_sps_locked();
        }
    }

    for (auto& click : active_clicks_) {
        const std::vector<float>& sample_buf = (mono_mode_ && click.type != Accent) ? mono_sample_ : click_samples_[click.type];
        qint64 click_remaining = static_cast<qint64>(sample_buf.size()) - static_cast<qint64>(click.pos_in_click);
        qint64 buffer_remaining = n_frames - click.start_in_buffer;
        qint64 to_mix = std::min(click_remaining, buffer_remaining);
        if (to_mix > 0) {
            for (qint64 i = 0; i < to_mix; ++i)
                scratch_[static_cast<size_t>(click.start_in_buffer + i)] += sample_buf[click.pos_in_click + static_cast<size_t>(i)] * click.gain;
            click.pos_in_click += static_cast<size_t>(to_mix);
        }
        click.start_in_buffer = 0;
    }
    std::erase_if(active_clicks_, [this](const ActiveClick& c) {
        const std::vector<float>& sample_buf = (mono_mode_ && c.type != Accent) ? mono_sample_ : click_samples_[c.type];
        return c.pos_in_click >= sample_buf.size();
    });

    float master = master_volume_;
    // Continuous keep-alive signal so the audio output line is never quiet
    // enough for power-managed drivers (laptop codecs, USB DACs, Bluetooth,
    // some onboard chipsets) to mute it. The un-mute latency on those drivers
    // is long enough to eat the start of a 30ms click that follows a long
    // silence, which manifests as missing/dropped clicks at low BPMs.
    //
    // Two components, both inaudible on normal playback hardware:
    //   - 20Hz sine at -50 dBFS: below human hearing threshold at this freq
    //     (~80 dB SPL minimum vs ~0 dB SPL at 1kHz) and below most speakers'
    //     reproduction range. Provides clear AC signal that any threshold-
    //     based silence detector will see as audio.
    //   - White noise at -66 dBFS: broadband energy so spectral detectors
    //     (e.g., those applying a HPF before measuring) still see signal.
    constexpr double keepalive_freq = 20.0;
    constexpr float keepalive_amp = 0.00316f;
    constexpr float keepalive_noise_amp = 0.0005f;
    const double phase_step = 2.0 * std::numbers::pi_v<double> * keepalive_freq / static_cast<double>(sample_rate_);
    const double two_pi = 2.0 * std::numbers::pi_v<double>;

    for (qint64 i = 0; i < n_frames; ++i) {
        float carrier = static_cast<float>(std::sin(keepalive_phase_)) * keepalive_amp;
        keepalive_phase_ += phase_step;
        if (keepalive_phase_ >= two_pi)
            keepalive_phase_ -= two_pi;

        dither_state_ = dither_state_ * 1664525u + 1013904223u;
        float n = (static_cast<float>(static_cast<int32_t>(dither_state_)) / 2147483648.0f) * keepalive_noise_amp;

        float v = scratch_[static_cast<size_t>(i)] * master + carrier + n;
        if (v > 1.0f)
            v = 1.0f;
        else if (v < -1.0f)
            v = -1.0f;
        scratch_[static_cast<size_t>(i)] = v;
    }

    position_samples_ += n_frames;

    auto fmt = format_.sampleFormat();
    if (fmt == QAudioFormat::Int16) {
        int16_t* out = reinterpret_cast<int16_t*>(data);
        for (qint64 i = 0; i < n_frames; ++i) {
            int16_t s = static_cast<int16_t>(scratch_[static_cast<size_t>(i)] * 32767.0f);
            for (int c = 0; c < channels; ++c)
                *out++ = s;
        }
    }
    else if (fmt == QAudioFormat::Float) {
        float* out = reinterpret_cast<float*>(data);
        for (qint64 i = 0; i < n_frames; ++i) {
            float s = scratch_[static_cast<size_t>(i)];
            for (int c = 0; c < channels; ++c)
                *out++ = s;
        }
    }
    else if (fmt == QAudioFormat::Int32) {
        int32_t* out = reinterpret_cast<int32_t*>(data);
        for (qint64 i = 0; i < n_frames; ++i) {
            int32_t s = static_cast<int32_t>(scratch_[static_cast<size_t>(i)] * 2147483647.0f);
            for (int c = 0; c < channels; ++c)
                *out++ = s;
        }
    }
    else if (fmt == QAudioFormat::UInt8) {
        uint8_t* out = reinterpret_cast<uint8_t*>(data);
        for (qint64 i = 0; i < n_frames; ++i) {
            uint8_t s = static_cast<uint8_t>(scratch_[static_cast<size_t>(i)] * 127.0f + 128.0f);
            for (int c = 0; c < channels; ++c)
                *out++ = s;
        }
    }
    else {
        std::memset(data, 0, static_cast<size_t>(n_frames * bytes_per_frame));
    }

    return n_frames * bytes_per_frame;
}

qint64 AudioEngine::writeData(const char*, qint64)
{
    return 0;
}

qint64 AudioEngine::bytesAvailable() const
{
    return std::numeric_limits<qint64>::max();
}
