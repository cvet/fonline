// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

class IAppAudio;

class AudioManager final
{
public:
    AudioManager() = delete;
    AudioManager(ptr<AudioSettings> settings, ptr<FileSystem> resources, ptr<IAppAudio> audio);
    AudioManager(const AudioManager&) = delete;
    AudioManager(AudioManager&&) noexcept = delete;
    auto operator=(const AudioManager&) = delete;
    auto operator=(AudioManager&&) noexcept = delete;
    ~AudioManager();

    [[nodiscard]] auto GetSoundNames() const noexcept -> const_span<string> { return _soundNames; }
    [[nodiscard]] auto GetMusicVolume() const noexcept -> int32_t { return _musicVolume; }
    [[nodiscard]] auto GetSoundVolume() const noexcept -> int32_t { return _soundVolume; }

    void IndexFiles();
    void SetMusicVolume(int32_t volume) noexcept { _musicVolume = volume; }
    void SetSoundVolume(int32_t volume) noexcept { _soundVolume = volume; }
    auto PlaySound(string_view name) -> uint32_t;
    auto PlaySound(string_view name, float32_t attenuation, float32_t pan) -> uint32_t;
    auto UpdateSound(uint32_t sound_id, float32_t attenuation, float32_t pan) -> bool;
    auto PlayMusic(string_view fname, timespan repeat_time) -> bool;
    void StopSounds();
    void StopMusic();

    // Leans an interleaved S16 stereo buffer to one side, in place
    static void ApplyPan(span<uint8_t> buf, float32_t pan);

private:
    struct Sound;

    // Returns the handle of the loaded sound, or zero when the resource could not be played
    auto Load(string_view fname, bool is_music, timespan repeat_time, float32_t attenuation, float32_t pan) -> uint32_t;
    void ProcessSounds(uint8_t silence, span<uint8_t> output);
    auto ProcessSound(ptr<Sound> sound, uint8_t silence, span<uint8_t> output) -> bool;
    auto StreamOgg(ptr<Sound> sound) -> bool;
    auto ConvertData(ptr<Sound> sound) -> bool;

    ptr<AudioSettings> _settings;
    ptr<FileSystem> _resources;
    ptr<IAppAudio> _audio;
    int32_t _musicVolume;
    int32_t _soundVolume;
    bool _isActive {};
    int32_t _streamingPortion {};
    uint32_t _soundIdCounter {};
    vector<string> _soundNames {};
    vector<unique_ptr<Sound>> _playingSounds {};
    vector<uint8_t> _outputBuf {};
};

FO_END_NAMESPACE
