#pragma once

#include "Common.h"

class SoundManager : public NonCopyable
{
public:
    SoundManager();
    ~SoundManager();
    bool PlaySound(const StrMap& sound_names, const string& name);
    bool PlayMusic(const string& fname, uint repeat_time);
    void StopSounds();
    void StopMusic();

private:
    struct DeviceData;
    struct Sound;
    using SoundsFunc = std::function<void(uchar*)>;
    using SoundVec = vector<shared_ptr<Sound>>;

    void ProcessSounds(uchar* output);
    bool ProcessSound(shared_ptr<Sound> sound, uchar* output);
    shared_ptr<Sound> Load(const string& fname, bool is_music);
    bool LoadWAV(shared_ptr<Sound> sound, const string& fname);
    bool LoadACM(shared_ptr<Sound> sound, const string& fname, bool is_music);
    bool LoadOGG(shared_ptr<Sound> sound, const string& fname);
    bool StreamOGG(shared_ptr<Sound> sound);
    bool ConvertData(shared_ptr<Sound> sound);

    bool isActive {};
    bool isAudioInited {};
    unique_ptr<DeviceData> deviceData {};
    uint streamingPortion {};
    SoundVec soundsActive {};
    UCharVec outputBuf {};
    SoundsFunc soundsFunc {};
};
