#include "poly_metronome.h"

#include "audio_engine.h"

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

void PolyMetronome::set_quarter_volume(float v)
{
    audio_->set_volume(AudioEngine::Quarter, v);
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

void PolyMetronome::set_beats_per_measure(int n)
{
    audio_->set_beats_per_measure(n);
}

void PolyMetronome::set_mono_mode(bool on)
{
    audio_->set_mono_mode(on);
}
