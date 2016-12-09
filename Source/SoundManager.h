#ifndef __SOUND_MANAGER__
#define __SOUND_MANAGER__

#include "Common.h"

#define SOUND_DEFAULT_EXT    ".acm"
#define MUSIC_REPEAT_TIME    ( Random( 240, 360 ) * 1000 )   // 4-6 minutes
#define STREAMING_PORTION    ( 0x10000 )

class Sound;
typedef vector< Sound* > SoundVec;

class SoundManager
{
public:
    SoundManager(): isActive( false ) {}

    bool Init();
    void Finish();
    void ClearSounds();

    bool PlaySound( const char* name );
    bool PlayMusic( const char* fname, uint pos = 0, uint repeat = MUSIC_REPEAT_TIME );
    void StopMusic();

private:
    void   ProcessSounds( uchar* output );
    bool   ProcessSound( Sound* sound );
    Sound* Load( const char* fname, bool is_music );
    bool   LoadWAV( Sound* sound, const char* fname );
    bool   LoadACM( Sound* sound, const char* fname, bool is_music );
    bool   LoadOGG( Sound* sound, const char* fname );
    bool   Streaming( Sound* sound );
    bool   StreamingWAV( Sound* sound );
    bool   StreamingACM( Sound* sound );
    bool   StreamingOGG( Sound* sound );
    bool   ConvertData( Sound* sound );

    bool     isActive;
    SoundVec soundsActive;
};

extern SoundManager SndMngr;

#endif // __SOUND_MANAGER__
