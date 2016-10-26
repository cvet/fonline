#include "Common.h"
#include "SoundManager.h"
#include "ResourceManager.h"
#include "Text.h"
#include "FileManager.h"

// Manager instance
SoundManager SndMngr;

// ACM
#include "Acm/acmstrm.h"

// OGG
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

SDL_AudioDeviceID DeviceID = 0;
SDL_AudioSpec     SoundSpec;

// Sound structure
class Sound
{
public:
    uchar* BaseBuf;
    uint   BaseBufSize;

    uchar* ConvertedBuf;
    uint   ConvertedBufSize;
    uint   ConvertedBufCur;

    uchar* OutputBuf;

    int    OriginalFormat;
    int    OriginalChannels;
    int    OriginalRate;

    bool   IsMusic;
    uint   NextPlay;
    uint   RepeatTime;

    bool   Streamable;
    enum { WAV, ACM, OGG } StreamType;

    OggVorbis_File OggDescriptor;

    Sound(): BaseBuf( nullptr ), BaseBufSize( 0 ), ConvertedBuf( nullptr ),
             ConvertedBufSize( 0 ), ConvertedBufCur( 0 ), OutputBuf( nullptr ),
             OriginalFormat( 0 ), OriginalChannels( 0 ), OriginalRate( 0 ),
             IsMusic( false ), NextPlay( 0 ), RepeatTime( 0 ),
             Streamable( false ), StreamType( WAV )
    {
        OutputBuf = new uchar[ SoundSpec.size ];
    }
    ~Sound()
    {
        if( ConvertedBuf != BaseBuf )
            SAFEDELA( ConvertedBuf );
        SAFEDELA( BaseBuf );
        SAFEDELA( OutputBuf );
        if( Streamable && StreamType == OGG )
            ov_clear( &OggDescriptor );
    }
};

void AudioCallback( void* userdata, Uint8* stream, int len )
{
    SndMngr.ProcessSounds( stream );
}

// SoundManager
bool SoundManager::Init()
{
    UNUSED_VARIABLE( OV_CALLBACKS_DEFAULT );
    UNUSED_VARIABLE( OV_CALLBACKS_NOCLOSE );
    UNUSED_VARIABLE( OV_CALLBACKS_STREAMONLY );
    UNUSED_VARIABLE( OV_CALLBACKS_STREAMONLY_NOCLOSE );

    if( isActive )
        return true;

    WriteLog( "Sound manager initialization...\n" );

    // SDL
    if( SDL_InitSubSystem( SDL_INIT_AUDIO ) )
    {
        WriteLogF( _FUNC_, " - SDL Audio initialization fail, error '%s'.\n", SDL_GetError() );
        return false;
    }

    // Create audio device
    SDL_AudioSpec desired;
    desired.freq = 22050;              // DSP frequency -- samples per second
    desired.format = AUDIO_S16;        // Audio data format
    desired.channels = 2;              // Number of channels: 1 mono, 2 stereo
    desired.silence = 0;               // Audio buffer silence value (calculated)
    desired.samples = 0;               // Audio buffer size in samples (power of 2)
    desired.padding = 0;               // Necessary for some compile environments
    desired.size = 0;                  // Audio buffer size in bytes (calculated)
    desired.callback = AudioCallback;
    desired.userdata = nullptr;
    DeviceID = SDL_OpenAudioDevice( nullptr, 0, &desired, &SoundSpec, SDL_AUDIO_ALLOW_ANY_CHANGE );
    if( DeviceID < 2 )
    {
        WriteLog( "SDL Open audio device fail, error '%s'.\n", SDL_GetError() );
        return false;
    }

    // Start playing
    SDL_PauseAudioDevice( DeviceID, 0 );

    isActive = true;
    WriteLog( "Sound manager initialization complete.\n" );
    return true;
}

void SoundManager::Finish()
{
    WriteLog( "Sound manager finish.\n" );

    ClearSounds();
    SDL_CloseAudioDevice( DeviceID );
    DeviceID = 0;

    isActive = false;
    WriteLog( "Sound manager finish complete.\n" );
}

void SoundManager::ClearSounds()
{
    SDL_LockAudioDevice( DeviceID );
    for( auto it = soundsActive.begin(); it != soundsActive.end(); ++it )
        delete *it;
    soundsActive.clear();
    SDL_UnlockAudioDevice( DeviceID );
}

void SoundManager::ProcessSounds( uchar* output )
{
    memzero( output, SoundSpec.size );
    for( auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if( SndMngr.ProcessSound( sound ) )
        {
            int volume = CLAMP( sound->IsMusic ? GameOpt.MusicVolume : GameOpt.SoundVolume, 0, 100 ) * SDL_MIX_MAXVOLUME / 100;
            SDL_MixAudioFormat( output, sound->OutputBuf, SoundSpec.format, SoundSpec.size, volume );
            ++it;
        }
        else
        {
            delete sound;
            it = soundsActive.erase( it );
        }
    }
}

bool SoundManager::ProcessSound( Sound* sound )
{
    // Playing
    uint samples = SoundSpec.samples;
    uint sample_size = SoundSpec.size / samples / SoundSpec.channels;
    uint channels = SoundSpec.channels;
    if( sound->ConvertedBufCur < sound->ConvertedBufSize )
    {
        uint whole = samples * sample_size * channels;
        if( whole > sound->ConvertedBufSize - sound->ConvertedBufCur )
        {
            // Flush last part of buffer
            uint offset = sound->ConvertedBufSize - sound->ConvertedBufCur;
            memcpy( sound->OutputBuf, sound->ConvertedBuf + sound->ConvertedBufCur, offset );
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while( offset < whole && sound->Streamable && Streaming( sound ) )
            {
                uint write = sound->ConvertedBufSize - sound->ConvertedBufCur;
                if( offset + write > whole )
                    write = whole - offset;
                memcpy( sound->OutputBuf + offset, sound->ConvertedBuf + sound->ConvertedBufCur, write );
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if( offset < whole )
                memzero( sound->OutputBuf + offset, whole - offset );
        }
        else
        {
            // Copy
            memcpy( sound->OutputBuf, sound->ConvertedBuf + sound->ConvertedBufCur, whole );
            sound->ConvertedBufCur += whole;
        }

        // Stream empty buffer
        if( sound->ConvertedBufCur == sound->ConvertedBufSize && sound->Streamable )
            return Streaming( sound );

        // Continue processing
        return true;
    }

    // Repeat
    if( sound->RepeatTime )
    {
        if( !sound->NextPlay )
        {
            // Set next playing time
            sound->NextPlay = Timer::GameTick() + ( sound->RepeatTime > 1 ? sound->RepeatTime : 0 );
        }

        if( Timer::GameTick() >= sound->NextPlay )
        {
            // Set buffer to beginning
            if( sound->Streamable && sound->StreamType == Sound::OGG )
                ov_raw_seek( &sound->OggDescriptor, 0 );

            // Stream
            if( sound->Streamable )
                Streaming( sound );

            // Drop timer
            sound->NextPlay = 0;

            // Process without silent
            return ProcessSound( sound );
        }

        // Give silent
        memzero( sound->OutputBuf, samples * sample_size * channels );
        return true;
    }

    // Give silent
    memzero( sound->OutputBuf, samples * sample_size * channels );
    return false;
}

Sound* SoundManager::Load( const char* fname, bool is_music )
{
    char fname_[ MAX_FOPATH ];
    Str::Copy( fname_, fname );

    const char* ext = FileManager::GetExtension( fname );
    if( !ext )
    {
        // Default ext
        ext = fname_ + Str::Length( fname_ );
        Str::Append( fname_, SOUND_DEFAULT_EXT );
    }
    else
    {
        --ext;
    }

    Sound* sound = new Sound();
    if( !( ( Str::CompareCase( ext, ".wav" ) && LoadWAV( sound, fname_ ) ) ||
           ( Str::CompareCase( ext, ".acm" ) && LoadACM( sound, fname_, is_music ) ) ||
           ( Str::CompareCase( ext, ".ogg" ) && LoadOGG( sound, fname_ ) ) ) )
    {
        delete sound;
        return nullptr;
    }

    SDL_LockAudioDevice( DeviceID );
    soundsActive.push_back( sound );
    SDL_UnlockAudioDevice( DeviceID );
    return sound;
}

bool SoundManager::LoadWAV( Sound* sound, const char* fname )
{
    FileManager fm;
    if( !fm.LoadFile( fname, true ) )
        return false;

    uint dw_buf = fm.GetLEUInt();
    if( dw_buf != MAKEUINT( 'R', 'I', 'F', 'F' ) )
    {
        WriteLogF( _FUNC_, " - 'RIFF' not found.\n" );
        return false;
    }

    fm.GoForward( 4 );

    dw_buf = fm.GetLEUInt();
    if( dw_buf != MAKEUINT( 'W', 'A', 'V', 'E' ) )
    {
        WriteLogF( _FUNC_, " - 'WAVE' not found.\n" );
        return false;
    }

    dw_buf = fm.GetLEUInt();
    if( dw_buf != MAKEUINT( 'f', 'm', 't', ' ' ) )
    {
        WriteLogF( _FUNC_, " - 'fmt ' not found.\n" );
        return false;
    }

    dw_buf = fm.GetLEUInt();
    if( !dw_buf )
    {
        WriteLogF( _FUNC_, " - Unknown format.\n" );
        return false;
    }

    struct                      // WAVEFORMATEX
    {
        ushort wFormatTag;      // Integer identifier of the format
        ushort nChannels;       // Number of audio channels
        uint   nSamplesPerSec;  // Audio sample rate
        uint   nAvgBytesPerSec; // Bytes per second (possibly approximate)
        ushort nBlockAlign;     // Size in bytes of a sample block (all channels)
        ushort wBitsPerSample;  // Size in bits of a single per-channel sample
        ushort cbSize;          // Bytes of extra data appended to this struct
    } waveformatex;

    fm.CopyMem( &waveformatex, 16 );

    if( waveformatex.wFormatTag != 1 )
    {
        WriteLogF( _FUNC_, " - Compressed files not supported.\n" );
        return false;
    }

    fm.GoForward( dw_buf - 16 );

    dw_buf = fm.GetLEUInt();
    if( dw_buf == MAKEUINT( 'f', 'a', 'c', 't' ) )
    {
        dw_buf = fm.GetLEUInt();
        fm.GoForward( dw_buf );
        dw_buf = fm.GetLEUInt();
    }

    if( dw_buf != MAKEUINT( 'd', 'a', 't', 'a' ) )
    {
        WriteLogF( _FUNC_, " - Unknown format2.\n" );
        return false;
    }

    dw_buf = fm.GetLEUInt();
    sound->BaseBufSize = dw_buf;

    // Check format
    sound->OriginalChannels = waveformatex.nChannels;
    sound->OriginalRate = waveformatex.nSamplesPerSec;
    switch( waveformatex.wBitsPerSample )
    {
    case 8:
        sound->OriginalFormat = AUDIO_U8;
        break;
    case 16:
        sound->OriginalFormat = AUDIO_S16;
        break;
    default:
        WriteLogF( _FUNC_, " - Unknown format.\n" );
        return false;
    }

    // Convert
    sound->BaseBuf = new unsigned char[ sound->BaseBufSize ];
    if( !fm.CopyMem( sound->BaseBuf, sound->BaseBufSize ) )
    {
        WriteLogF( _FUNC_, " - File truncated.\n" );
        return false;
    }

    return ConvertData( sound );
}

bool SoundManager::LoadACM( Sound* sound, const char* fname, bool is_music )
{
    FileManager fm;
    if( !fm.LoadFile( fname, true ) )
        return false;

    int                     channels = 0;
    int                     freq = 0;
    int                     samples = 0;
    AutoPtr< CACMUnpacker > acm( new CACMUnpacker( fm.GetBuf(), (int) fm.GetFsize(), channels, freq, samples ) );
    if( !acm.IsValid() )
    {
        WriteLogF( _FUNC_, " - ACMUnpacker init fail.\n" );
        return false;
    }

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = ( is_music ? 2 : 1 );
    sound->OriginalRate = 22050;

    sound->BaseBufSize = samples * 2;
    sound->BaseBuf = new unsigned char[ sound->BaseBufSize ];
    int dec_data = acm->readAndDecompress( (ushort*) sound->BaseBuf, sound->BaseBufSize );
    if( dec_data != (int) sound->BaseBufSize )
    {
        WriteLogF( _FUNC_, " - Decode Acm error.\n" );
        return false;
    }

    return ConvertData( sound );
}

size_t Ogg_read_func( void* ptr, size_t size, size_t nmemb, void* datasource )
{
    FileManager* fm = (FileManager*) datasource;
    return fm->CopyMem( ptr, (uint) size ) ? size : 0;
}

int Ogg_seek_func( void* datasource, ogg_int64_t offset, int whence )
{
    FileManager* fm = (FileManager*) datasource;
    switch( whence )
    {
    case SEEK_SET:
        fm->SetCurPos( (uint) offset );
        break;
    case SEEK_CUR:
        fm->GoForward( (uint) offset );
        break;
    case SEEK_END:
        fm->SetCurPos( fm->GetFsize() - (uint) offset );
        break;
    default:
        return -1;
    }
    return 0;
}

int Ogg_close_func( void* datasource )
{
    FileManager* fm = (FileManager*) datasource;
    delete fm;
    return 0;
}

long Ogg_tell_func( void* datasource )
{
    FileManager* fm = (FileManager*) datasource;
    return fm->GetCurPos();
}

bool SoundManager::LoadOGG( Sound* sound, const char* fname )
{
    FileManager* fm = new FileManager();
    if( !fm || !fm->LoadFile( fname, true ) )
    {
        SAFEDEL( fm );
        return false;
    }

    ov_callbacks callbacks;
    callbacks.read_func = &Ogg_read_func;
    callbacks.seek_func = &Ogg_seek_func;
    callbacks.close_func = &Ogg_close_func;
    callbacks.tell_func = &Ogg_tell_func;

    int error = ov_open_callbacks( fm, &sound->OggDescriptor, nullptr, 0, callbacks );
    if( error )
    {
        WriteLogF( _FUNC_, " - Open OGG file '%s' fail, error:\n", fname );
        switch( error )
        {
        case OV_EREAD:
            WriteLog( "A read from media returned an error.\n" );
            break;
        case OV_ENOTVORBIS:
            WriteLog( "Bitstream does not contain any Vorbis data.\n" );
            break;
        case OV_EVERSION:
            WriteLog( "Vorbis version mismatch.\n" );
            break;
        case OV_EBADHEADER:
            WriteLog( "Invalid Vorbis bitstream header.\n" );
            break;
        case OV_EFAULT:
            WriteLog( "Internal logic fault; indicates a bug or heap/stack corruption.\n" );
            break;
        default:
            WriteLog( "Unknown error code %d.\n", error );
            break;
        }
        return false;
    }

    vorbis_info* vi = ov_info( &sound->OggDescriptor, -1 );
    if( !vi )
    {
        WriteLogF( _FUNC_, " - ov_info error.\n" );
        ov_clear( &sound->OggDescriptor );
        return false;
    }

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = (int) vi->rate;

    sound->BaseBuf = new unsigned char[ STREAMING_PORTION ];
    if( !sound->BaseBuf )
        return false;

    int  result = 0;
    uint decoded = 0;
    while( true )
    {
        int portion = MIN( (uint) 4096, STREAMING_PORTION - decoded );
        result = (int) ov_read( &sound->OggDescriptor, (char*) sound->BaseBuf + decoded, portion, 0, 2, 1, nullptr );
        if( result <= 0 )
            break;
        decoded += result;
        if( decoded >= STREAMING_PORTION )
            break;
    }
    if( result < 0 )
        return false;

    sound->BaseBufSize = decoded;

    if( !result )
    {
        sound->Streamable = false;
        ov_clear( &sound->OggDescriptor );
    }
    else
    {
        sound->Streamable = true;
        sound->StreamType = Sound::OGG;
    }
    return ConvertData( sound );
}

bool SoundManager::Streaming( Sound* sound )
{
    if( !( ( sound->StreamType == Sound::WAV && StreamingWAV( sound ) ) ||
           ( sound->StreamType == Sound::ACM && StreamingACM( sound ) ) ||
           ( sound->StreamType == Sound::OGG && StreamingOGG( sound ) ) ) )
        return false;
    return true;
}

bool SoundManager::StreamingWAV( Sound* sound )
{
    return false;
}

bool SoundManager::StreamingACM( Sound* sound )
{
    return false;
}

bool SoundManager::StreamingOGG( Sound* sound )
{
    int  result = 0;
    uint decoded = 0;
    while( true )
    {
        int portion = MIN( (uint) 4096, STREAMING_PORTION - decoded );
        result = (int) ov_read( &sound->OggDescriptor, (char*) sound->BaseBuf + decoded, portion, 0, 2, 1, nullptr );
        if( result <= 0 )
            break;
        decoded += result;
        if( decoded >= STREAMING_PORTION )
            break;
    }
    if( result < 0 || !decoded )
        return false;

    sound->BaseBufSize = decoded;
    return ConvertData( sound );
}

bool SoundManager::ConvertData( Sound* sound )
{
    SDL_AudioCVT cvt;
    int          r = SDL_BuildAudioCVT( &cvt, sound->OriginalFormat, sound->OriginalChannels, sound->OriginalRate,
                                        SoundSpec.format, SoundSpec.channels, SoundSpec.freq );
    if( r == -1 )
    {
        WriteLogF( _FUNC_, " - SDL_BuildAudioCVT fail, error '%s'.\n", SDL_GetError() );
        return false;
    }
    if( r == 1 )
    {
        cvt.len = sound->BaseBufSize;
        cvt.buf = (Uint8*) SDL_malloc( cvt.len * cvt.len_mult );
        memcpy( cvt.buf, sound->BaseBuf, sound->BaseBufSize );
        if( SDL_ConvertAudio( &cvt ) )
        {
            WriteLogF( _FUNC_, " - SDL_ConvertAudio fail, error '%s'.\n", SDL_GetError() );
            return false;
        }
        sound->ConvertedBufCur = 0;
        sound->ConvertedBufSize = cvt.len_cvt;
        sound->ConvertedBuf = new unsigned char[ sound->ConvertedBufSize ];
        memcpy( sound->ConvertedBuf, cvt.buf, sound->ConvertedBufSize );
        SDL_free( cvt.buf );
    }
    else
    {
        sound->ConvertedBufCur = 0;
        sound->ConvertedBufSize = sound->BaseBufSize;
        sound->ConvertedBuf = sound->BaseBuf;
    }
    return true;
}

bool SoundManager::PlaySound( const char* name )
{
    if( !isActive || !GameOpt.SoundVolume )
        return true;

    // Make 'NAME'
    char name_[ MAX_FOPATH ];
    Str::Copy( name_, name );
    FileManager::EraseExtension( name_ );
    Str::Upper( name_ );

    // Find base
    StrMap& names = ResMngr.GetSoundNames();
    auto    it = names.find( name_ );
    if( it != names.end() )
        return Load( it->second.c_str(), false ) != nullptr;

    // Check random pattern 'NAME_X'
    uint count = 0;
    char buf[ MAX_FOPATH ];
    while( names.find( Str::Format( buf, "%s_%d", name_, count + 1 ) ) != names.end() )
        count++;
    if( count )
        return Load( names.find( Str::Format( buf, "%s_%d", name_, Random( 1, count ) ) )->second.c_str(), false ) != nullptr;

    return false;
}

bool SoundManager::PlayMusic( const char* fname, uint pos, uint repeat )
{
    if( !isActive )
        return true;

    StopMusic();

    // Load new
    Sound* sound = Load( fname, true );
    if( !sound )
        return false;

    sound->IsMusic = true;
    sound->RepeatTime = repeat;
    return true;
}

void SoundManager::StopMusic()
{
    // Find and erase old music
    SDL_LockAudioDevice( DeviceID );
    for( auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if( sound->IsMusic )
        {
            delete sound;
            it = soundsActive.erase( it );
        }
        else
        {
            ++it;
        }
    }
    SDL_UnlockAudioDevice( DeviceID );
}

void SoundManager::PlayAmbient( const char* str )
{
    if( !isActive || !GameOpt.SoundVolume )
        return;

    int  rnd = Random( 1, 100 );

    char name[ MAX_FOPATH ];
    char num[ 64 ];

    for( int i = 0; *str; ++i, ++str )
    {
        // Read name
        name[ i ] = *str;

        if( *str == ':' )
        {
            if( !i )
                return;
            name[ i ] = '\0';
            str++;

            // Read number
            int j;
            for( j = 0; *str && *str != ','; ++j, ++str )
                num[ j ] = *str;
            if( !j )
                return;
            num[ j ] = '\0';

            // Check
            int k = Str::AtoI( num );
            if( rnd <= k )
            {
                if( !Str::CompareCase( name, "blank" ) )
                    PlaySound( name );
                return;
            }

            rnd -= k;
            i = -1;

            while( *str == ' ' )
                str++;
            if( *str != ',' )
                return;
            while( *str == ' ' )
                str++;
            str++;
        }
    }
}
