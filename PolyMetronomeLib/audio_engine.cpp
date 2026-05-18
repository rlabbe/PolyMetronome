// Tick scheduling design
// ──────────────────────
// Audio generation runs on a dedicated producer thread (produce_audio) which
// walks the sequence in 1/60-of-a-beat sub-ticks, mixes click samples, and
// writes formatted PCM into a lock-free ring buffer. readData() — called by
// QAudioSink — just copies bytes out of that ring buffer (a fast memcpy).
//
// The audio scheduling cursor (anchor_position_, subticks_in_measure_,
// seq_measure_idx_) advances in the producer thread, generating audio ~2 s
// ahead of what the sink has consumed.
//
// The widget needs sample-accurate beat onsets for LEDs and the needle.
// Driving them off the producer loop fails: it runs ahead of real time,
// so multiple beats would become "available" at the same instant and the
// visual updates would clump.
//
// Solution: a separate tick-only scheduler (schedule_ticks_to_locked) with
// its own cursor (tick_anchor_position_, tick_subticks_in_measure_,
// tick_seq_measure_idx_). It pushes {play_sample, measure, beat} entries
// into pending_ticks_ for ~2 s past the current producer position on
// every chunk, and the queue is pre-filled with 2 s at start().
//
// processed_samples() reports the sink's actual playback position. The raw
// sink_->processedUSecs() value updates in chunks (per device buffer commit),
// so we linearly extrapolate between updates using a wall clock anchored at
// sink_->start(). The result advances smoothly at real time AND tracks the
// device's true output position, so the widget's LED/needle transitions
// land at the same instant the corresponding click is heard.
//
// The widget polls processed_samples() and pop_ticks_through() returns the
// oldest queued tick whose play_sample <= played. Because the queue is
// always populated 2 s ahead of audio, each tick crosses the threshold at
// its true playback instant rather than at a buffer-pull boundary.
//
// The QAudioSink is destroyed on stop() and recreated on start() so that
// no stray data from a previous playback session leaks into the next one.

#include "audio_engine.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QDebug>
#include <QMediaDevices>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <thread>

AudioEngine::AudioEngine(QObject* parent)
    : QIODevice(parent)
{
    active_clicks_.reserve(64);
    ring_.resize(ring_capacity_);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull())
        qWarning() << "AudioEngine: no default audio output device available";

    format_ = device.preferredFormat();
    sample_rate_ = format_.sampleRate();

    rebuild_click_samples();
    recompute_sps_locked();
}

AudioEngine::~AudioEngine()
{
    stop();
}

void AudioEngine::create_sink()
{
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    sink_ = new QAudioSink(device, format_, this);
    // A massive buffer (like 192k / 4 seconds) causes priming glitches on many drivers.
    // We set a 200ms buffer which is standard for low-latency interactive audio.
    sink_->setBufferSize(sample_rate_ * 0.2 * format_.bytesPerFrame());
}

void AudioEngine::destroy_sink()
{
    if (!sink_)
        return;
    sink_->reset();
    sink_->stop();
    delete sink_;
    sink_ = nullptr;
}

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
            click_samples_[t][i] = std::sin(2.0f * std::numbers::pi_v<float> *freqs[t] * ts) * env;
        }
    }
    mono_sample_.resize(n);
    for (int i = 0; i < n; ++i) {
        float ts = static_cast<float>(i) / sample_rate_;
        float env = std::exp(-ts * decay);
        mono_sample_[i] = std::sin(2.0f * std::numbers::pi_v<float> *mono_freq * ts) * env;
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
    int note_value = current_measure_locked().note_value;
    if (note_value <= 0)
        note_value = 4;
    // Sub-tick is 1/60 of a beat. A beat is 1/note_value of a whole note.
    // BPM = quarter notes per minute (always). So time per beat = (60/bpm)*(4/note_value) sec.
    // Samples per subtick = (SR * 60/BPM * 4/note_value) / 60
    samples_per_subtick_ = (sample_rate_ * 4.0) / static_cast<double>(bpm_ * note_value);
}

void AudioEngine::recompute_tick_sps_locked()
{
    if (bpm_ <= 0)
        return;
    int note_value = 4;
    if (auto* m = sequence_.at_absolute(tick_seq_measure_idx_))
        note_value = m->note_value > 0 ? m->note_value : 4;
    tick_samples_per_subtick_ = (sample_rate_ * 4.0) / static_cast<double>(bpm_ * note_value);
}

void AudioEngine::schedule_ticks_to_locked(qint64 target_sample)
{
    int total_ci_subs = count_in_beats_ * subs_per_beat_;
    while (tick_count_in_subtick_ < total_ci_subs) {
        size_t spos = tick_count_in_anchor_ + static_cast<size_t>(std::round(tick_count_in_subtick_ * tick_count_in_sps_));
        if (static_cast<qint64>(spos) > target_sample)
            return;
        ++tick_count_in_subtick_;
    }

    while (true) {
        const MeasureSpec* mp = sequence_.at_absolute(tick_seq_measure_idx_);
        int curr_beats = (mp && mp->beats > 0) ? mp->beats : 4;
        size_t subs_in_curr_measure = curr_beats * subs_per_beat_;

        size_t spos = tick_anchor_position_ + static_cast<size_t>(std::round(tick_subticks_in_measure_ * tick_samples_per_subtick_));
        if (static_cast<qint64>(spos) > target_sample)
            return;

        int sub_in_beat = static_cast<int>(tick_subticks_in_measure_ % subs_per_beat_);
        int beat_in_measure = static_cast<int>(tick_subticks_in_measure_ / subs_per_beat_);
        if (sub_in_beat == 0)
            pending_ticks_.push_back({ static_cast<qint64>(spos), tick_seq_measure_idx_, beat_in_measure });

        ++tick_subticks_in_measure_;

        if (tick_subticks_in_measure_ >= subs_in_curr_measure) {
            tick_anchor_position_ = tick_anchor_position_ + static_cast<size_t>(std::round(subs_in_curr_measure * tick_samples_per_subtick_));
            tick_subticks_in_measure_ = 0;
            int total = sequence_.total_measures();
            if (total <= 0)
                total = 1;
            tick_seq_measure_idx_ = (tick_seq_measure_idx_ + 1) % total;
            recompute_tick_sps_locked();
        }
    }
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

// Producer: generates formatted audio into the ring buffer on a background
// thread. Runs continuously while producing_ is true, sleeping briefly when
// the ring buffer is full enough (~2 seconds ahead).
void AudioEngine::produce_audio()
{
    int channels = format_.channelCount();
    int bytes_per_sample = format_.bytesPerSample();
    if (channels <= 0 || bytes_per_sample <= 0)
        return;
    int bytes_per_frame = channels * bytes_per_sample;

    // Generate in chunks of ~10ms
    size_t chunk_frames = static_cast<size_t>(sample_rate_ * 0.01);
    std::vector<float> local_scratch(chunk_frames);
    std::vector<char> local_out(chunk_frames * bytes_per_frame);

    while (producing_.load(std::memory_order_relaxed)) {
        // How much space is available in the ring?
        size_t w = ring_write_.load(std::memory_order_relaxed);
        size_t r = ring_read_.load(std::memory_order_acquire);
        size_t used = w - r;  // works because unsigned wrapping
        size_t avail = ring_capacity_ - used;

        size_t chunk_bytes = chunk_frames * bytes_per_frame;
        // Keep ~2 seconds ahead, wait if ring is full enough.
        // set_bpm/set_sequence signal producer_wake_ after rewinding the
        // write cursor, so we wake immediately on tempo changes.
        size_t target_ahead = static_cast<size_t>(sample_rate_ * 2) * bytes_per_frame;
        if (used >= target_ahead || avail < chunk_bytes) {
            QMutexLocker wl(&producer_wake_mutex_);
            producer_wake_.wait(&producer_wake_mutex_, 5);
            continue;
        }

        // Generate one chunk
        std::fill(local_scratch.begin(), local_scratch.end(), 0.0f);

        {
            QMutexLocker lock(&mutex_);

            schedule_ticks_to_locked(static_cast<qint64>(position_samples_ + chunk_frames + 2 * sample_rate_));

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

            size_t buffer_end = position_samples_ + chunk_frames;

            // Count-in
            {
                int total_ci_subs = count_in_beats_ * subs_per_beat_;
                while (count_in_subtick_ < total_ci_subs) {
                    size_t spos = count_in_anchor_ + static_cast<size_t>(std::round(count_in_subtick_ * count_in_sps_));
                    if (spos >= buffer_end)
                        break;
                    if (spos >= position_samples_ && count_in_subtick_ % subs_per_beat_ == 0)
                        schedule(spos, Beat);
                    ++count_in_subtick_;
                }
            }

            // Main sequence
            while (true) {
                const MeasureSpec& m = current_measure_locked();
                int curr_beats = m.beats > 0 ? m.beats : 4;
                size_t subs_in_curr_measure = curr_beats * subs_per_beat_;

                size_t spos = anchor_position_ + static_cast<size_t>(std::round(subticks_in_measure_ * samples_per_subtick_));
                if (spos >= buffer_end)
                    break;

                if (spos >= position_samples_) {
                    int sub_in_beat = static_cast<int>(subticks_in_measure_ % subs_per_beat_);
                    int beat_in_measure = static_cast<int>(subticks_in_measure_ / subs_per_beat_);
                    // 60 sub-ticks per beat. Eighth/Sixteenth subdivision intervals
                    // depend on the measure's note value: a beat is one 1/note_value
                    // note, so the spacing of an eighth note in subticks is
                    // 60 * note_value / 8 (and similarly for sixteenths). When that
                    // spacing is >= 60 the subdivision coincides with (or is coarser
                    // than) the beat itself and is suppressed.
                    // Triplet/Quintuplet are always 3 / 5 even hits per beat.
                    int nv = m.note_value > 0 ? m.note_value : 4;
                    int eighth_step = 60 * nv / 8;
                    int sixteenth_step = 60 * nv / 16;
                    bool is_eighth_offbeat = (eighth_step > 0 && eighth_step < 60
                                              && sub_in_beat % eighth_step == 0);
                    bool is_sixteenth_offbeat = (sixteenth_step > 0 && sixteenth_step < 60
                                                 && sub_in_beat % sixteenth_step == 0);

                    if (sub_in_beat == 0) {
                        schedule(spos, Beat);
                        if (beat_in_measure == 0)
                            schedule(spos, Accent);
                        else if (is_group_boundary_locked(beat_in_measure))
                            schedule(spos, Accent, 0.5f);
                    }
                    else if (is_eighth_offbeat)
                        schedule(spos, Eighth);
                    else if (is_sixteenth_offbeat)
                        schedule(spos, Sixteenth);
                    else if (sub_in_beat == 20 || sub_in_beat == 40)
                        schedule(spos, Triplet);
                    else if (sub_in_beat == 12 || sub_in_beat == 24 || sub_in_beat == 36 || sub_in_beat == 48)
                        schedule(spos, Quintuplet);
                }

                ++subticks_in_measure_;

                if (subticks_in_measure_ >= subs_in_curr_measure) {
                    anchor_position_ = anchor_position_ + static_cast<size_t>(std::round(subs_in_curr_measure * samples_per_subtick_));
                    subticks_in_measure_ = 0;
                    int total = sequence_.total_measures();
                    if (total <= 0)
                        total = 1;
                    seq_measure_idx_ = (seq_measure_idx_ + 1) % total;
                    recompute_sps_locked();
                }
            }

            // Mixing
            for (auto& click : active_clicks_) {
                const std::vector<float>& sample_buf = (mono_mode_ && click.type != Accent) ? mono_sample_ : click_samples_[click.type];
                size_t click_remaining = sample_buf.size() - click.pos_in_click;
                size_t buffer_remaining = chunk_frames - click.start_in_buffer;
                size_t to_mix = std::min(click_remaining, buffer_remaining);
                if (to_mix > 0) {
                    for (size_t i = 0; i < to_mix; ++i)
                        local_scratch[click.start_in_buffer + i] += sample_buf[click.pos_in_click + i] * click.gain;
                    click.pos_in_click += to_mix;
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
            const double phase_step = 2.0 * std::numbers::pi * keepalive_freq / static_cast<double>(sample_rate_);
            constexpr double two_pi = 2.0 * std::numbers::pi;

            for (size_t i = 0; i < chunk_frames; ++i) {
                float carrier = static_cast<float>(std::sin(keepalive_phase_)) * keepalive_amp;
                keepalive_phase_ += phase_step;
                if (keepalive_phase_ >= two_pi)
                    keepalive_phase_ -= two_pi;

                dither_state_ = dither_state_ * 1664525u + 1013904223u;
                float n = (static_cast<float>(static_cast<int32_t>(dither_state_)) / 2147483648.0f) * keepalive_noise_amp;

                float v = local_scratch[i] * master + carrier + n;
                if (v > 1.0f)
                    v = 1.0f;
                else if (v < -1.0f)
                    v = -1.0f;
                local_scratch[i] = v;
            }

            position_samples_ += chunk_frames;
        } // mutex released

        // Format conversion into local_out
        auto fmt = format_.sampleFormat();
        if (fmt == QAudioFormat::Int16) {
            int16_t* out = reinterpret_cast<int16_t*>(local_out.data());
            for (size_t i = 0; i < chunk_frames; ++i) {
                int16_t s = static_cast<int16_t>(local_scratch[i] * 32767.0f);
                for (int c = 0; c < channels; ++c)
                    *out++ = s;
            }
        }
        else if (fmt == QAudioFormat::Float) {
            float* out = reinterpret_cast<float*>(local_out.data());
            for (size_t i = 0; i < chunk_frames; ++i) {
                float s = local_scratch[i];
                for (int c = 0; c < channels; ++c)
                    *out++ = s;
            }
        }
        else if (fmt == QAudioFormat::Int32) {
            int32_t* out = reinterpret_cast<int32_t*>(local_out.data());
            for (size_t i = 0; i < chunk_frames; ++i) {
                int32_t s = static_cast<int32_t>(local_scratch[i] * 2147483647.0f);
                for (int c = 0; c < channels; ++c)
                    *out++ = s;
            }
        }
        else if (fmt == QAudioFormat::UInt8) {
            uint8_t* out = reinterpret_cast<uint8_t*>(local_out.data());
            for (size_t i = 0; i < chunk_frames; ++i) {
                uint8_t s = static_cast<uint8_t>(local_scratch[i] * 127.0f + 128.0f);
                for (int c = 0; c < channels; ++c)
                    *out++ = s;
            }
        }
        else {
            std::memset(local_out.data(), 0, chunk_bytes);
        }

        // Write into ring buffer (may wrap around)
        size_t write_pos = w % ring_capacity_;
        size_t first = std::min(chunk_bytes, ring_capacity_ - write_pos);
        std::memcpy(ring_.data() + write_pos, local_out.data(), first);
        if (first < chunk_bytes)
            std::memcpy(ring_.data(), local_out.data() + first, chunk_bytes - first);
        ring_write_.store(w + chunk_bytes, std::memory_order_release);
    }
}

void AudioEngine::start()
{
    // Stop previous playback completely (destroys the sink)
    stop();

    {
        QMutexLocker lock(&mutex_);
        active_clicks_.clear();
        pending_ticks_.clear();
        position_samples_ = 0;

        // Delay the first beat enough that the audio backend's pull pipeline
        // is fully primed before any click is scheduled. 250 ms is comfortably
        // past any typical sink buffer and double-pull window.
        int initial_offset = sample_rate_ / 4;
        // Count-in uses quarter-note beats. Samples per subtick (1/60th of a quarter note).
        count_in_sps_ = static_cast<double>(sample_rate_) / static_cast<double>(bpm_);
        count_in_anchor_ = initial_offset;
        count_in_subtick_ = 0;
        anchor_position_ = initial_offset + static_cast<size_t>(std::round(count_in_beats_ * subs_per_beat_ * count_in_sps_));
        seq_measure_idx_ = 0;
        subticks_in_measure_ = 0;
        keepalive_phase_ = 0.0;
        dither_state_ = 0x12345678u;
        if (sequence_.empty())
            sequence_ = MeterSequence::default_4_4();
        recompute_sps_locked();

        tick_seq_measure_idx_ = 0;
        tick_subticks_in_measure_ = 0;
        tick_anchor_position_ = anchor_position_;
        tick_count_in_subtick_ = 0;
        tick_count_in_sps_ = count_in_sps_;
        tick_count_in_anchor_ = count_in_anchor_;
        recompute_tick_sps_locked();
        schedule_ticks_to_locked(static_cast<qint64>(initial_offset) + 2 * sample_rate_);
    }

    // Reset ring buffer
    ring_write_.store(0, std::memory_order_relaxed);
    ring_read_.store(0, std::memory_order_relaxed);

    // Start producer thread
    producing_.store(true, std::memory_order_release);
    producer_thread_ = QThread::create([this]() { produce_audio(); });
    producer_thread_->start(QThread::TimeCriticalPriority);

    // Let the producer fill ~200ms before starting the sink, so readData
    // never starves on the first pull.
    size_t prime_bytes = static_cast<size_t>(sample_rate_ * 0.2) * format_.bytesPerFrame();
    for (int i = 0; i < 200; ++i) {
        size_t w = ring_write_.load(std::memory_order_acquire);
        size_t r = ring_read_.load(std::memory_order_relaxed);
        if (w - r >= prime_bytes)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Fresh sink every start
    create_sink();
    open(QIODevice::ReadOnly);
    wall_clock_.start();
    last_processed_us_ = -1;
    last_wall_ns_ = wall_clock_.nsecsElapsed();
    sink_->start(this);
}

void AudioEngine::stop()
{
    // Stop producer first
    producing_.store(false, std::memory_order_release);
    producer_wake_.wakeOne();
    if (producer_thread_) {
        producer_thread_->wait();
        delete producer_thread_;
        producer_thread_ = nullptr;
    }

    // Destroy the sink — a fresh one is created on next start()
    destroy_sink();
    close();

    QMutexLocker lock(&mutex_);
    active_clicks_.clear();
    pending_ticks_.clear();
    position_samples_ = 0;
    seq_measure_idx_ = 0;
    subticks_in_measure_ = 0;
    count_in_subtick_ = 0;
    anchor_position_ = 0;
    count_in_anchor_ = 0;
    tick_seq_measure_idx_ = 0;
    tick_subticks_in_measure_ = 0;
    tick_count_in_subtick_ = 0;
    tick_anchor_position_ = 0;
    tick_count_in_anchor_ = 0;
    last_processed_us_ = -1;
    last_wall_ns_ = 0;
}

// Anchor on the sink's reported playback position (microseconds of audio
// actually delivered to the device). That value updates in chunks (one
// per buffer commit), so between updates we linearly extrapolate using
// the wall clock. This keeps the widget pacing tied to what's actually
// audible rather than to when sink_->start() was called.
qint64 AudioEngine::processed_samples() const
{
    if (!sink_ || !wall_clock_.isValid() || !is_running())
        return 0;

    qint64 pu = sink_->processedUSecs();
    if (last_processed_us_ == -1 || pu < last_processed_us_) {
        last_processed_us_ = pu;
        last_wall_ns_ = wall_clock_.nsecsElapsed();
        return 0;
    }

    qint64 wall_ns = wall_clock_.nsecsElapsed();
    if (pu != last_processed_us_) {
        last_processed_us_ = pu;
        last_wall_ns_ = wall_ns;
    }
    qint64 interp_us = pu + (wall_ns - last_wall_ns_) / 1000;
    return interp_us * static_cast<qint64>(sample_rate_) / 1'000'000;
}

std::optional<AudioEngine::TickInfo> AudioEngine::pop_ticks_through(qint64 sample_index)
{
    QMutexLocker lock(&mutex_);
    if (pending_ticks_.empty() || pending_ticks_.front().play_sample > sample_index)
        return std::nullopt;
    TickInfo t{ pending_ticks_.front().measure, pending_ticks_.front().beat };
    pending_ticks_.erase(pending_ticks_.begin());
    return t;
}

bool AudioEngine::is_running() const
{
    return sink_ && (sink_->state() == QAudio::ActiveState || sink_->state() == QAudio::IdleState);
}

void AudioEngine::set_bpm(int bpm)
{
    if (bpm <= 0)
        return;
    qint64 played = processed_samples();
    QMutexLocker lock(&mutex_);
    if (bpm == bpm_)
        return;

    // Figure out which beat the listener is on by unwinding the producer's
    // lead over the playback position.
    double old_sps = samples_per_subtick_;
    int64_t ahead = static_cast<int64_t>(position_samples_) - played;
    int64_t ahead_sub = (old_sps > 0) ? static_cast<int64_t>(std::round(ahead / old_sps)) : 0;
    int64_t play_sub = static_cast<int64_t>(subticks_in_measure_) - ahead_sub;
    const MeasureSpec& m = current_measure_locked();
    int curr_beats = m.beats > 0 ? m.beats : 4;
    int64_t subs_in_meas = curr_beats * subs_per_beat_;
    play_sub = ((play_sub % subs_in_meas) + subs_in_meas) % subs_in_meas;
    int64_t beat = play_sub / subs_per_beat_;
    // Start one subtick past the beat boundary so we don't re-trigger a
    // click the listener already heard.
    size_t beat_sub = static_cast<size_t>(beat * subs_per_beat_ + 1);

    bpm_ = bpm;
    recompute_sps_locked();

    // Rewind producer to playback position, preserving the beat.
    position_samples_ = static_cast<size_t>(std::max<qint64>(0, played));
    subticks_in_measure_ = beat_sub;
    int64_t new_anchor = static_cast<int64_t>(position_samples_)
                       - static_cast<int64_t>(std::round(beat_sub * samples_per_subtick_));
    anchor_position_ = static_cast<size_t>(std::max<int64_t>(0, new_anchor));

    // Sync tick scheduler and regenerate.
    pending_ticks_.clear();
    tick_seq_measure_idx_ = seq_measure_idx_;
    tick_subticks_in_measure_ = subticks_in_measure_;
    recompute_tick_sps_locked();
    int64_t tick_anchor = static_cast<int64_t>(position_samples_)
                        - static_cast<int64_t>(std::round(beat_sub * tick_samples_per_subtick_));
    tick_anchor_position_ = static_cast<size_t>(std::max<int64_t>(0, tick_anchor));
    schedule_ticks_to_locked(static_cast<qint64>(position_samples_) + 2 * sample_rate_);

    // Move the write cursor back to the read cursor so the producer
    // overwrites the unplayed old-tempo data with new-tempo data.
    // readData never sees an empty buffer — it transitions seamlessly.
    ring_write_.store(ring_read_.load(std::memory_order_relaxed), std::memory_order_release);
    producer_wake_.wakeOne();
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
    qint64 played = processed_samples();
    QMutexLocker lock(&mutex_);
    sequence_ = seq;
    if (sequence_.empty())
        sequence_ = MeterSequence::default_4_4();
    seq_measure_idx_ = 0;
    subticks_in_measure_ = 0;
    position_samples_ = static_cast<size_t>(played);
    anchor_position_ = position_samples_;
    recompute_sps_locked();
    tick_seq_measure_idx_ = 0;
    tick_subticks_in_measure_ = 0;
    tick_anchor_position_ = anchor_position_;
    tick_count_in_subtick_ = 0;
    pending_ticks_.clear();
    recompute_tick_sps_locked();
    ring_write_.store(ring_read_.load(std::memory_order_relaxed), std::memory_order_release);
    producer_wake_.wakeOne();
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

// readData: called by QAudioSink's internal thread. Just copies pre-generated
// bytes from the ring buffer. If not enough data is ready, fills with silence.
qint64 AudioEngine::readData(char* data, qint64 max_len)
{
    size_t w = ring_write_.load(std::memory_order_acquire);
    size_t r = ring_read_.load(std::memory_order_relaxed);
    size_t available = w - r;
    size_t to_copy = std::min(available, static_cast<size_t>(max_len));

    if (to_copy == 0) {
        std::memset(data, 0, static_cast<size_t>(max_len));
        return max_len;
    }

    // Copy from ring (may wrap)
    size_t read_pos = r % ring_capacity_;
    size_t first = std::min(to_copy, ring_capacity_ - read_pos);
    std::memcpy(data, ring_.data() + read_pos, first);
    if (first < to_copy)
        std::memcpy(data + first, ring_.data(), to_copy - first);

    ring_read_.store(r + to_copy, std::memory_order_release);

    // If we didn't have enough, zero-fill the remainder
    if (to_copy < static_cast<size_t>(max_len))
    {
        qWarning() << "AudioEngine: underrun, filled" << (max_len - to_copy) << "bytes of silence";
        std::memset(data + to_copy, 0, static_cast<size_t>(max_len) - to_copy);
    }

    return max_len;
}

qint64 AudioEngine::writeData(const char*, qint64)
{
    return 0;
}

qint64 AudioEngine::bytesAvailable() const
{
    return std::numeric_limits<qint64>::max();
}
