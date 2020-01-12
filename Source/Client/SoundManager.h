#pragma once

#include "Common.h"

class ISoundManager;
using SoundManager = shared_ptr<ISoundManager>;

class ISoundManager : public NonCopyable
{
public:
    static SoundManager Create();
    virtual bool PlaySound(const StrMap& sound_names, const string& name) = 0;
    virtual bool PlayMusic(const string& fname, uint repeat_time) = 0;
    virtual void StopSounds() = 0;
    virtual void StopMusic() = 0;
    virtual ~ISoundManager() = default;
};
