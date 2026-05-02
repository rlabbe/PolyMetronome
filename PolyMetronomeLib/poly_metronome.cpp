#include "poly_metronome.h"

#include "audio_engine.h"

QJsonObject PolyMetronomeState::to_json() const
{
    QJsonObject obj;
    obj["bpm"] = bpm;
    obj["master_volume"] = master_volume;
    obj["accent_volume"] = accent_volume;
    obj["beat_volume"] = beat_volume;
    obj["eighth_volume"] = eighth_volume;
    obj["sixteenth_volume"] = sixteenth_volume;
    obj["triplet_volume"] = triplet_volume;
    obj["quintuplet_volume"] = quintuplet_volume;
    obj["mono_mode"] = mono_mode;
    obj["count_in"] = count_in;
    obj["sequence"] = sequence.to_json();
    return obj;
}

PolyMetronomeState PolyMetronomeState::from_json(const QJsonObject& obj)
{
    PolyMetronomeState s;
    s.bpm = obj.value("bpm").toInt(60);
    s.master_volume = static_cast<float>(obj.value("master_volume").toDouble(0.8));
    s.accent_volume = static_cast<float>(obj.value("accent_volume").toDouble(0.0));
    if (obj.contains("beat_volume"))
        s.beat_volume = static_cast<float>(obj.value("beat_volume").toDouble(1.0));
    else
        s.beat_volume = static_cast<float>(obj.value("quarter_volume").toDouble(1.0));
    s.eighth_volume = static_cast<float>(obj.value("eighth_volume").toDouble(0.0));
    s.sixteenth_volume = static_cast<float>(obj.value("sixteenth_volume").toDouble(0.0));
    s.triplet_volume = static_cast<float>(obj.value("triplet_volume").toDouble(0.0));
    s.quintuplet_volume = static_cast<float>(obj.value("quintuplet_volume").toDouble(0.0));
    s.mono_mode = obj.value("mono_mode").toBool(true);
    s.count_in = obj.value("count_in").toInt(0);
    if (obj.contains("sequence"))
        s.sequence = MeterSequence::from_json(obj.value("sequence").toArray());
    return s;
}

PolyMetronome::PolyMetronome(QObject* parent)
    : QObject(parent)
    , audio_(new AudioEngine(this))
{
}

PolyMetronome::~PolyMetronome() = default;

void PolyMetronome::start()
{
    audio_->start();
}

void PolyMetronome::stop()
{
    audio_->stop();
}

bool PolyMetronome::is_running() const
{
    return audio_->is_running();
}

void PolyMetronome::set_bpm(int bpm)
{
    audio_->set_bpm(bpm);
}

void PolyMetronome::set_master_volume(float v)
{
    audio_->set_master_volume(v);
}

void PolyMetronome::set_beat_volume(float v)
{
    audio_->set_volume(AudioEngine::Beat, v);
}

void PolyMetronome::set_eighth_volume(float v)
{
    audio_->set_volume(AudioEngine::Eighth, v);
}

void PolyMetronome::set_sixteenth_volume(float v)
{
    audio_->set_volume(AudioEngine::Sixteenth, v);
}

void PolyMetronome::set_triplet_volume(float v)
{
    audio_->set_volume(AudioEngine::Triplet, v);
}

void PolyMetronome::set_quintuplet_volume(float v)
{
    audio_->set_volume(AudioEngine::Quintuplet, v);
}

void PolyMetronome::set_accent_volume(float v)
{
    audio_->set_volume(AudioEngine::Accent, v);
}

void PolyMetronome::set_sequence(const MeterSequence& seq)
{
    audio_->set_sequence(seq);
}

void PolyMetronome::set_mono_mode(bool on)
{
    audio_->set_mono_mode(on);
}

void PolyMetronome::set_count_in(int beats)
{
    audio_->set_count_in(beats);
}
