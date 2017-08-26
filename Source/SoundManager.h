#ifndef __SOUND_MANAGER__
#define __SOUND_MANAGER__

#include "Common.h"

class Sound;
typedef vector< Sound* > SoundVec;

class SoundManager
{
public:
    SoundManager(): isActive( false ) {}
    bool Init();
    void Finish();

    bool PlaySound( const string& name );
    bool PlayMusic( const string& fname, uint repeat_time );
    void StopSounds();
    void StopMusic();

private:
    void   ProcessSounds( uchar* output );
    bool   ProcessSound( Sound* sound, uchar* output );
    Sound* Load( const string& fname, bool is_music );
    bool   LoadWAV( Sound* sound, const string& fname );
    bool   LoadACM( Sound* sound, const string& fname, bool is_music );
    bool   LoadOGG( Sound* sound, const string& fname );
    bool   StreamOGG( Sound* sound );
    bool   ConvertData( Sound* sound );

    bool     isActive;
    uint     streamingPortion;
    SoundVec soundsActive;
    UCharVec outputBuf;
};

extern SoundManager SndMngr;

#endif // __SOUND_MANAGER__
