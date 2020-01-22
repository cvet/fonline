#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "Settings.h"

class SoundManager : public NonCopyable
{
public:
    SoundManager(SoundSettings& sett, FileManager& file_mngr);
    ~SoundManager();
    bool PlaySound(const StrMap& sound_names, const string& name);
    bool PlayMusic(const string& fname, uint repeat_time);
    void StopSounds();
    void StopMusic();

private:
    struct Impl;
    struct Sound;
    using SoundsFunc = std::function<void(uchar*)>;
    using SoundVec = vector<Sound*>;

    void ProcessSounds(uchar* output);
    bool ProcessSound(Sound* sound, uchar* output);
    Sound* Load(const string& fname, bool is_music);
    bool LoadWAV(Sound* sound, const string& fname);
    bool LoadACM(Sound* sound, const string& fname, bool is_music);
    bool LoadOGG(Sound* sound, const string& fname);
    bool StreamOGG(Sound* sound);
    bool ConvertData(Sound* sound);

    SoundSettings& settings;
    FileManager& fileMngr;
    bool isActive {};
    bool isAudioInited {};
    unique_ptr<Impl> pImpl {};
    uint streamingPortion {};
    SoundVec soundsActive {};
    UCharVec outputBuf {};
    SoundsFunc soundsFunc {};
};
