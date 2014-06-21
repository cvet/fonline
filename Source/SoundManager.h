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
    bool PlaySoundType( uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext );
    bool PlayMusic( const char* fname, uint pos = 0, uint repeat = MUSIC_REPEAT_TIME );
    void StopMusic();
    void PlayAmbient( const char* str );

    void ProcessSounds( uchar* output );

private:
    bool   ProcessSound( Sound* sound );
    Sound* Load( const char* fname, int path_type );
    bool   LoadWAV( Sound* sound, const char* fname, int path_type );
    bool   LoadACM( Sound* sound, const char* fname, int path_type );
    bool   LoadOGG( Sound* sound, const char* fname, int path_type );
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
