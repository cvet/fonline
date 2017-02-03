#include "Common.h"
#include "SoundManager.h"
#include "ResourceManager.h"
#include "Text.h"
#include "FileManager.h"
#include <functional>

// Manager instance
SoundManager SndMngr;

// ACM
#include "Acm/acmstrm.h"

// OGG
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

static SDL_AudioDeviceID DeviceID = 0;
static SDL_AudioSpec     SoundSpec;

// Sound structure
class Sound
{
public:
    uchar*          BaseBuf;
    uint            BaseBufSize;

    bool            CvtBuilded;
    SDL_AudioCVT    Cvt;
    uchar*          ConvertedBuf;
    uint            ConvertedBufRealSize;
    uint            ConvertedBufSize;
    uint            ConvertedBufCur;

    int             OriginalFormat;
    int             OriginalChannels;
    int             OriginalRate;

    bool            IsMusic;
    uint            NextPlay;
    uint            RepeatTime;

    OggVorbis_File* OggStream;

    Sound(): BaseBuf( nullptr ), BaseBufSize( 0 ), CvtBuilded( false ), ConvertedBuf( nullptr ),
             ConvertedBufRealSize( 0 ), ConvertedBufSize( 0 ), ConvertedBufCur( 0 ),
             OriginalFormat( 0 ), OriginalChannels( 0 ), OriginalRate( 0 ),
             IsMusic( false ), NextPlay( 0 ), RepeatTime( 0 ),
             OggStream( nullptr )
    {}
    ~Sound()
    {
        SAFEDELA( BaseBuf );
        SAFEDELA( ConvertedBuf );
        if( OggStream )
            ov_clear( OggStream );
        SAFEDEL( OggStream );
    }
};

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
        WriteLog( "SDL Audio initialization fail, error '{}'.\n", SDL_GetError() );
        return false;
    }

    // Create audio device
    SDL_AudioSpec desired;
    memzero( &desired, sizeof( desired ) );
    #ifdef FO_WEB
    desired.format = AUDIO_F32;
    desired.freq = 48000;
    desired.channels = 2;
    streamingPortion = 0x20000; // 128kb
    #else
    desired.format = AUDIO_S16;
    desired.freq = 44100;
    streamingPortion = 0x10000; // 64kb
    #endif
    desired.callback = [] ( void* userdata, Uint8 * stream, int len )
    {
        auto& func = *( std::function< void(const SoundManager&, uchar*) >* )userdata;
        func( SndMngr, stream );
    };
    desired.userdata = new std::function< void(SoundManager&, uchar*) >( &SoundManager::ProcessSounds );

    DeviceID = SDL_OpenAudioDevice( nullptr, 0, &desired, &SoundSpec, SDL_AUDIO_ALLOW_ANY_CHANGE );
    if( DeviceID < 2 )
    {
        WriteLog( "SDL Open audio device fail, error '{}'.\n", SDL_GetError() );
        return false;
    }

    outputBuf.resize( SoundSpec.size );

    // Start playing
    SDL_PauseAudioDevice( DeviceID, 0 );

    isActive = true;
    WriteLog( "Sound manager initialization complete.\n" );
    return true;
}

void SoundManager::Finish()
{
    WriteLog( "Sound manager finish.\n" );

    StopSounds();
    StopMusic();

    SDL_CloseAudioDevice( DeviceID );
    DeviceID = 0;

    isActive = false;
    WriteLog( "Sound manager finish complete.\n" );
}

void SoundManager::ProcessSounds( uchar* output )
{
    memset( output, SoundSpec.silence, SoundSpec.size );

    for( auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if( SndMngr.ProcessSound( sound, &outputBuf[ 0 ] ) )
        {
            int volume = CLAMP( sound->IsMusic ? GameOpt.MusicVolume : GameOpt.SoundVolume, 0, 100 ) * SDL_MIX_MAXVOLUME / 100;
            SDL_MixAudioFormat( output, &outputBuf[ 0 ], SoundSpec.format, SoundSpec.size, volume );
            ++it;
        }
        else
        {
            delete sound;
            it = soundsActive.erase( it );
        }
    }
}

bool SoundManager::ProcessSound( Sound* sound, uchar* output )
{
    // Playing
    uint whole = SoundSpec.size;
    if( sound->ConvertedBufCur < sound->ConvertedBufSize )
    {
        if( whole > sound->ConvertedBufSize - sound->ConvertedBufCur )
        {
            // Flush last part of buffer
            uint offset = sound->ConvertedBufSize - sound->ConvertedBufCur;
            memcpy( output, sound->ConvertedBuf + sound->ConvertedBufCur, offset );
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while( offset < whole && sound->OggStream && StreamOGG( sound ) )
            {
                uint write = sound->ConvertedBufSize - sound->ConvertedBufCur;
                if( offset + write > whole )
                    write = whole - offset;
                memcpy( output + offset, sound->ConvertedBuf + sound->ConvertedBufCur, write );
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if( offset < whole )
                memset( output + offset, SoundSpec.silence, whole - offset );
        }
        else
        {
            // Copy
            memcpy( output, sound->ConvertedBuf + sound->ConvertedBufCur, whole );
            sound->ConvertedBufCur += whole;
        }

        if( sound->OggStream && sound->ConvertedBufCur == sound->ConvertedBufSize )
            StreamOGG( sound );

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
            sound->ConvertedBufCur = 0;
            if( sound->OggStream )
                ov_raw_seek( sound->OggStream, 0 );

            // Drop timer
            sound->NextPlay = 0;

            // Process without silent
            return ProcessSound( sound, output );
        }

        // Give silent
        memset( output, SoundSpec.silence, whole );
        return true;
    }

    // Give silent
    memset( output, SoundSpec.silence, whole );
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
        Str::Append( fname_, ".acm" );
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
    if( !fm.LoadFile( fname ) )
        return false;

    uint dw_buf = fm.GetLEUInt();
    if( dw_buf != MAKEUINT( 'R', 'I', 'F', 'F' ) )
    {
        WriteLog( "'RIFF' not found.\n" );
        return false;
    }

    fm.GoForward( 4 );

    dw_buf = fm.GetLEUInt();
    if( dw_buf != MAKEUINT( 'W', 'A', 'V', 'E' ) )
    {
        WriteLog( "'WAVE' not found.\n" );
        return false;
    }

    dw_buf = fm.GetLEUInt();
    if( dw_buf != MAKEUINT( 'f', 'm', 't', ' ' ) )
    {
        WriteLog( "'fmt ' not found.\n" );
        return false;
    }

    dw_buf = fm.GetLEUInt();
    if( !dw_buf )
    {
        WriteLog( "Unknown format.\n" );
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
        WriteLog( "Compressed files not supported.\n" );
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
        WriteLog( "Unknown format2.\n" );
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
        WriteLog( "Unknown format.\n" );
        return false;
    }

    // Convert
    sound->BaseBuf = new unsigned char[ sound->BaseBufSize ];
    if( !fm.CopyMem( sound->BaseBuf, sound->BaseBufSize ) )
    {
        WriteLog( "File truncated.\n" );
        return false;
    }

    return ConvertData( sound );
}

bool SoundManager::LoadACM( Sound* sound, const char* fname, bool is_music )
{
    FileManager fm;
    if( !fm.LoadFile( fname ) )
        return false;

    int                     channels = 0;
    int                     freq = 0;
    int                     samples = 0;
    AutoPtr< CACMUnpacker > acm( new CACMUnpacker( fm.GetBuf(), (int) fm.GetFsize(), channels, freq, samples ) );
    if( !acm.IsValid() )
    {
        WriteLog( "ACMUnpacker init fail.\n" );
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
        WriteLog( "Decode Acm error.\n" );
        return false;
    }

    return ConvertData( sound );
}

static size_t Ogg_read_func( void* ptr, size_t size, size_t nmemb, void* datasource )
{
    FileManager* fm = (FileManager*) datasource;
    return fm->CopyMem( ptr, (uint) size ) ? size : 0;
}

static int Ogg_seek_func( void* datasource, ogg_int64_t offset, int whence )
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

static int Ogg_close_func( void* datasource )
{
    FileManager* fm = (FileManager*) datasource;
    delete fm;
    return 0;
}

static long Ogg_tell_func( void* datasource )
{
    FileManager* fm = (FileManager*) datasource;
    return fm->GetCurPos();
}

bool SoundManager::LoadOGG( Sound* sound, const char* fname )
{
    FileManager* fm = new FileManager();
    if( !fm->LoadFile( fname ) )
    {
        SAFEDEL( fm );
        return false;
    }

    ov_callbacks callbacks;
    callbacks.read_func = &Ogg_read_func;
    callbacks.seek_func = &Ogg_seek_func;
    callbacks.close_func = &Ogg_close_func;
    callbacks.tell_func = &Ogg_tell_func;

    sound->OggStream = new OggVorbis_File();
    int error = ov_open_callbacks( fm, sound->OggStream, nullptr, 0, callbacks );
    if( error )
    {
        WriteLog( "Open OGG file '{}' fail, error:\n", fname );
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
            WriteLog( "Unknown error code {}.\n", error );
            break;
        }
        return false;
    }

    vorbis_info* vi = ov_info( sound->OggStream, -1 );
    if( !vi )
    {
        WriteLog( "Ogg info error.\n" );
        return false;
    }

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = (int) vi->rate;
    sound->BaseBuf = new unsigned char[ streamingPortion ];

    int  result = 0;
    uint decoded = 0;
    while( true )
    {
        result = (int) ov_read( sound->OggStream, (char*) sound->BaseBuf + decoded, streamingPortion - decoded, 0, 2, 1, nullptr );
        if( result <= 0 )
            break;
        decoded += result;
        if( decoded >= streamingPortion )
            break;
    }
    if( result < 0 )
        return false;

    sound->BaseBufSize = decoded;

    // No need streaming
    if( !result )
    {
        ov_clear( sound->OggStream );
        SAFEDEL( sound->OggStream );
    }

    return ConvertData( sound );
}

bool SoundManager::StreamOGG( Sound* sound )
{
    int  result = 0;
    uint decoded = 0;
    while( true )
    {
        result = (int) ov_read( sound->OggStream, (char*) sound->BaseBuf + decoded, streamingPortion - decoded, 0, 2, 1, nullptr );
        if( result <= 0 )
            break;
        decoded += result;
        if( decoded >= streamingPortion )
            break;
    }
    if( result < 0 || !decoded )
        return false;

    sound->BaseBufSize = decoded;
    return ConvertData( sound );
}

bool SoundManager::ConvertData( Sound* sound )
{
    if( !sound->CvtBuilded )
    {
        sound->CvtBuilded = true;
        if( SDL_BuildAudioCVT( &sound->Cvt, sound->OriginalFormat, sound->OriginalChannels, sound->OriginalRate,
                               SoundSpec.format, SoundSpec.channels, SoundSpec.freq ) == -1 )
        {
            WriteLog( "SDL_BuildAudioCVT fail, error '{}'.\n", SDL_GetError() );
            return false;
        }
    }

    sound->Cvt.len = sound->BaseBufSize;
    if( sound->Cvt.len * sound->Cvt.len_mult > (int) sound->ConvertedBufRealSize )
    {
        sound->ConvertedBufRealSize = sound->Cvt.len * sound->Cvt.len_mult * 4;
        SAFEDELA( sound->ConvertedBuf );
        sound->ConvertedBuf = new unsigned char[ sound->ConvertedBufRealSize ];
    }
    sound->Cvt.buf = (Uint8*) sound->ConvertedBuf;
    memcpy( sound->Cvt.buf, sound->BaseBuf, sound->BaseBufSize );

    if( SDL_ConvertAudio( &sound->Cvt ) )
    {
        WriteLog( "SDL_ConvertAudio fail, error '{}'.\n", SDL_GetError() );
        return false;
    }

    sound->ConvertedBufCur = 0;
    sound->ConvertedBufSize = sound->Cvt.len_cvt;

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

bool SoundManager::PlayMusic( const char* fname, uint repeat_time )
{
    if( !isActive )
        return true;

    StopMusic();

    // Load new
    Sound* sound = Load( fname, true );
    if( !sound )
        return false;

    sound->IsMusic = true;
    sound->RepeatTime = repeat_time;
    return true;
}

void SoundManager::StopSounds()
{
    SDL_LockAudioDevice( DeviceID );
    for( auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if( !sound->IsMusic )
        {
            delete *it;
            it = soundsActive.erase( it );
        }
        else
        {
            ++it;
        }
    }
    SDL_UnlockAudioDevice( DeviceID );
}

void SoundManager::StopMusic()
{
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
