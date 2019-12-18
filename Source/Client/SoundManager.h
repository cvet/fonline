#pragma once

#include "Common.h"

class Sound;
using SoundVec = vector<Sound*>;
using Uint32 = uint32_t;
using SDL_AudioDeviceID = Uint32;
struct SDL_AudioSpec;

class SoundManager
{
public:
    SoundManager();
    ~SoundManager();
    bool PlaySound(const StrMap& sound_names, const string& name);
    bool PlayMusic(const string& fname, uint repeat_time);
    void StopSounds();
    void StopMusic();

private:
    using SoundsFunc = std::function<void(uchar*)>;

    void ProcessSounds(uchar* output);
    bool ProcessSound(Sound* sound, uchar* output);
    Sound* Load(const string& fname, bool is_music);
    bool LoadWAV(Sound* sound, const string& fname);
    bool LoadACM(Sound* sound, const string& fname, bool is_music);
    bool LoadOGG(Sound* sound, const string& fname);
    bool StreamOGG(Sound* sound);
    bool ConvertData(Sound* sound);

    bool isActive;
    unique_ptr<SDL_AudioDeviceID> deviceID;
    unique_ptr<SDL_AudioSpec> soundSpec;
    uint streamingPortion;
    SoundVec soundsActive;
    UCharVec outputBuf;
    SoundsFunc soundsFunc;
};
