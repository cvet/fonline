#pragma once

#include "Common.h"
#include "SDL_audio.h"

#ifdef FO_WINDOWS
# undef PlaySound
#endif

class Sound;
using SoundVec = vector< Sound* >;

class SoundManager
{
public:
    SoundManager();
    ~SoundManager();
    bool PlaySound( const StrMap& sound_names, const string& name );
    bool PlayMusic( const string& fname, uint repeat_time );
    void StopSounds();
    void StopMusic();

private:
    using SoundsFunc = std::function< void(uchar*) >;

    void   ProcessSounds( uchar* output );
    bool   ProcessSound( Sound* sound, uchar* output );
    Sound* Load( const string& fname, bool is_music );
    bool   LoadWAV( Sound* sound, const string& fname );
    bool   LoadACM( Sound* sound, const string& fname, bool is_music );
    bool   LoadOGG( Sound* sound, const string& fname );
    bool   StreamOGG( Sound* sound );
    bool   ConvertData( Sound* sound );

    bool              isActive;
    SDL_AudioDeviceID deviceID;
    SDL_AudioSpec     soundSpec;
    uint              streamingPortion;
    SoundVec          soundsActive;
    UCharVec          outputBuf;
    SoundsFunc        soundsFunc;
};
