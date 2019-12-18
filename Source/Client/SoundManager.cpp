#include "SoundManager.h"
#include "FileUtils.h"
#include "Log.h"
#include "ResourceManager.h"
#include "SDL_audio.h"
#include "StringUtils.h"
#include "Timer.h"
#include "acmstrm.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

class Sound
{
public:
    uchar* BaseBuf;
    uint BaseBufSize;

    bool CvtBuilded;
    SDL_AudioCVT Cvt;
    uchar* ConvertedBuf;
    uint ConvertedBufRealSize;
    uint ConvertedBufSize;
    uint ConvertedBufCur;

    int OriginalFormat;
    int OriginalChannels;
    int OriginalRate;

    bool IsMusic;
    uint NextPlay;
    uint RepeatTime;

    OggVorbis_File* OggStream;

    Sound() :
        BaseBuf {},
        BaseBufSize {},
        CvtBuilded {},
        Cvt {},
        ConvertedBuf {},
        ConvertedBufRealSize {},
        ConvertedBufSize {},
        ConvertedBufCur {},
        OriginalFormat {},
        OriginalChannels {},
        OriginalRate {},
        IsMusic {},
        NextPlay {},
        RepeatTime {},
        OggStream {}
    {
    }

    ~Sound()
    {
        SAFEDELA(BaseBuf);
        SAFEDELA(ConvertedBuf);
        if (OggStream)
            ov_clear(OggStream);
        SAFEDEL(OggStream);
    }
};

SoundManager::SoundManager() :
    isActive {}, deviceID {}, soundSpec {}, streamingPortion {}, soundsActive {}, outputBuf {}, soundsFunc {}
{
    UNUSED_VARIABLE(OV_CALLBACKS_DEFAULT);
    UNUSED_VARIABLE(OV_CALLBACKS_NOCLOSE);
    UNUSED_VARIABLE(OV_CALLBACKS_STREAMONLY);
    UNUSED_VARIABLE(OV_CALLBACKS_STREAMONLY_NOCLOSE);

    // SDL
    bool sound_system_ok = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!sound_system_ok)
    {
        WriteLog("SDL Audio initialization fail, error '{}'.\n", SDL_GetError());
        return;
    }

    // Create audio device
    SDL_AudioSpec desired;
    memzero(&desired, sizeof(desired));
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
    desired.callback = [](void* userdata, Uint8* stream, int len) {
        auto& func = *reinterpret_cast<SoundsFunc*>(userdata);
        func(stream);
    };
    soundsFunc = std::bind(&SoundManager::ProcessSounds, this, std::placeholders::_1);
    desired.userdata = &soundsFunc;

    SDL_AudioSpec sound_spec;
    SDL_AudioDeviceID device_id = SDL_OpenAudioDevice(nullptr, 0, &desired, &sound_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (device_id < 2)
    {
        WriteLog("SDL Open audio device fail, error '{}'.\n", SDL_GetError());
        return;
    }

    outputBuf.resize(sound_spec.size);

    soundSpec = std::make_unique<SDL_AudioSpec>(sound_spec);
    deviceID = std::make_unique<SDL_AudioDeviceID>(device_id);
    isActive = true;

    // Start playing
    SDL_PauseAudioDevice(device_id, 0);
}

SoundManager::~SoundManager()
{
    if (!isActive)
        return;

    StopSounds();
    StopMusic();

    SDL_CloseAudioDevice(*deviceID);
    deviceID = nullptr;

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SoundManager::ProcessSounds(uchar* output)
{
    memset(output, soundSpec->silence, soundSpec->size);

    for (auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if (ProcessSound(sound, &outputBuf[0]))
        {
            int volume =
                CLAMP(sound->IsMusic ? GameOpt.MusicVolume : GameOpt.SoundVolume, 0, 100) * SDL_MIX_MAXVOLUME / 100;
            SDL_MixAudioFormat(output, &outputBuf[0], soundSpec->format, soundSpec->size, volume);
            ++it;
        }
        else
        {
            delete sound;
            it = soundsActive.erase(it);
        }
    }
}

bool SoundManager::ProcessSound(Sound* sound, uchar* output)
{
    // Playing
    uint whole = soundSpec->size;
    if (sound->ConvertedBufCur < sound->ConvertedBufSize)
    {
        if (whole > sound->ConvertedBufSize - sound->ConvertedBufCur)
        {
            // Flush last part of buffer
            uint offset = sound->ConvertedBufSize - sound->ConvertedBufCur;
            memcpy(output, sound->ConvertedBuf + sound->ConvertedBufCur, offset);
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while (offset < whole && sound->OggStream && StreamOGG(sound))
            {
                uint write = sound->ConvertedBufSize - sound->ConvertedBufCur;
                if (offset + write > whole)
                    write = whole - offset;
                memcpy(output + offset, sound->ConvertedBuf + sound->ConvertedBufCur, write);
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if (offset < whole)
                memset(output + offset, soundSpec->silence, whole - offset);
        }
        else
        {
            // Copy
            memcpy(output, sound->ConvertedBuf + sound->ConvertedBufCur, whole);
            sound->ConvertedBufCur += whole;
        }

        if (sound->OggStream && sound->ConvertedBufCur == sound->ConvertedBufSize)
            StreamOGG(sound);

        // Continue processing
        return true;
    }

    // Repeat
    if (sound->RepeatTime)
    {
        if (!sound->NextPlay)
        {
            // Set next playing time
            sound->NextPlay = Timer::GameTick() + (sound->RepeatTime > 1 ? sound->RepeatTime : 0);
        }

        if (Timer::GameTick() >= sound->NextPlay)
        {
            // Set buffer to beginning
            sound->ConvertedBufCur = 0;
            if (sound->OggStream)
                ov_raw_seek(sound->OggStream, 0);

            // Drop timer
            sound->NextPlay = 0;

            // Process without silent
            return ProcessSound(sound, output);
        }

        // Give silent
        memset(output, soundSpec->silence, whole);
        return true;
    }

    // Give silent
    memset(output, soundSpec->silence, whole);
    return false;
}

Sound* SoundManager::Load(const string& fname, bool is_music)
{
    string fixed_fname = fname;
    string ext = _str(fname).getFileExtension();

    // Default ext
    if (ext.empty())
    {
        ext = "acm";
        fixed_fname += "." + ext;
    }

    Sound* sound = new Sound();
    if (!((ext == "wav" && LoadWAV(sound, fixed_fname)) || (ext == "acm" && LoadACM(sound, fixed_fname, is_music)) ||
            (ext == "ogg" && LoadOGG(sound, fixed_fname))))
    {
        delete sound;
        return nullptr;
    }

    SDL_LockAudioDevice(*deviceID);
    soundsActive.push_back(sound);
    SDL_UnlockAudioDevice(*deviceID);
    return sound;
}

bool SoundManager::LoadWAV(Sound* sound, const string& fname)
{
    File fm;
    if (!fm.LoadFile(fname))
        return false;

    uint dw_buf = fm.GetLEUInt();
    if (dw_buf != MAKEUINT('R', 'I', 'F', 'F'))
    {
        WriteLog("'RIFF' not found.\n");
        return false;
    }

    fm.GoForward(4);

    dw_buf = fm.GetLEUInt();
    if (dw_buf != MAKEUINT('W', 'A', 'V', 'E'))
    {
        WriteLog("'WAVE' not found.\n");
        return false;
    }

    dw_buf = fm.GetLEUInt();
    if (dw_buf != MAKEUINT('f', 'm', 't', ' '))
    {
        WriteLog("'fmt ' not found.\n");
        return false;
    }

    dw_buf = fm.GetLEUInt();
    if (!dw_buf)
    {
        WriteLog("Unknown format.\n");
        return false;
    }

    struct // WAVEFORMATEX
    {
        ushort wFormatTag; // Integer identifier of the format
        ushort nChannels; // Number of audio channels
        uint nSamplesPerSec; // Audio sample rate
        uint nAvgBytesPerSec; // Bytes per second (possibly approximate)
        ushort nBlockAlign; // Size in bytes of a sample block (all channels)
        ushort wBitsPerSample; // Size in bits of a single per-channel sample
        ushort cbSize; // Bytes of extra data appended to this struct
    } waveformatex;

    fm.CopyMem(&waveformatex, 16);

    if (waveformatex.wFormatTag != 1)
    {
        WriteLog("Compressed files not supported.\n");
        return false;
    }

    fm.GoForward(dw_buf - 16);

    dw_buf = fm.GetLEUInt();
    if (dw_buf == MAKEUINT('f', 'a', 'c', 't'))
    {
        dw_buf = fm.GetLEUInt();
        fm.GoForward(dw_buf);
        dw_buf = fm.GetLEUInt();
    }

    if (dw_buf != MAKEUINT('d', 'a', 't', 'a'))
    {
        WriteLog("Unknown format2.\n");
        return false;
    }

    dw_buf = fm.GetLEUInt();
    sound->BaseBufSize = dw_buf;

    // Check format
    sound->OriginalChannels = waveformatex.nChannels;
    sound->OriginalRate = waveformatex.nSamplesPerSec;
    switch (waveformatex.wBitsPerSample)
    {
    case 8:
        sound->OriginalFormat = AUDIO_U8;
        break;
    case 16:
        sound->OriginalFormat = AUDIO_S16;
        break;
    default:
        WriteLog("Unknown format.\n");
        return false;
    }

    // Convert
    sound->BaseBuf = new unsigned char[sound->BaseBufSize];
    if (!fm.CopyMem(sound->BaseBuf, sound->BaseBufSize))
    {
        WriteLog("File truncated.\n");
        return false;
    }

    return ConvertData(sound);
}

bool SoundManager::LoadACM(Sound* sound, const string& fname, bool is_music)
{
    File fm;
    if (!fm.LoadFile(fname))
        return false;

    int channels = 0;
    int freq = 0;
    int samples = 0;
    CACMUnpacker* acm = new CACMUnpacker(fm.GetBuf(), (int)fm.GetFsize(), channels, freq, samples);

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = (is_music ? 2 : 1);
    sound->OriginalRate = 22050;
    sound->BaseBufSize = samples * 2;
    sound->BaseBuf = new unsigned char[sound->BaseBufSize];

    int dec_data = acm->readAndDecompress((ushort*)sound->BaseBuf, sound->BaseBufSize);
    delete acm;

    if (dec_data != (int)sound->BaseBufSize)
    {
        WriteLog("Decode Acm error.\n");
        return false;
    }

    return ConvertData(sound);
}

static size_t Ogg_read_func(void* ptr, size_t size, size_t nmemb, void* datasource)
{
    File* fm = (File*)datasource;
    return fm->CopyMem(ptr, (uint)size) ? size : 0;
}

static int Ogg_seek_func(void* datasource, ogg_int64_t offset, int whence)
{
    File* fm = (File*)datasource;
    switch (whence)
    {
    case SEEK_SET:
        fm->SetCurPos((uint)offset);
        break;
    case SEEK_CUR:
        fm->GoForward((uint)offset);
        break;
    case SEEK_END:
        fm->SetCurPos(fm->GetFsize() - (uint)offset);
        break;
    default:
        return -1;
    }
    return 0;
}

static int Ogg_close_func(void* datasource)
{
    File* fm = (File*)datasource;
    delete fm;
    return 0;
}

static long Ogg_tell_func(void* datasource)
{
    File* fm = (File*)datasource;
    return fm->GetCurPos();
}

bool SoundManager::LoadOGG(Sound* sound, const string& fname)
{
    File* fm = new File();
    if (!fm->LoadFile(fname))
    {
        SAFEDEL(fm);
        return false;
    }

    ov_callbacks callbacks;
    callbacks.read_func = &Ogg_read_func;
    callbacks.seek_func = &Ogg_seek_func;
    callbacks.close_func = &Ogg_close_func;
    callbacks.tell_func = &Ogg_tell_func;

    sound->OggStream = new OggVorbis_File();
    int error = ov_open_callbacks(fm, sound->OggStream, nullptr, 0, callbacks);
    if (error)
    {
        WriteLog("Open OGG file '{}' fail, error:\n", fname);
        switch (error)
        {
        case OV_EREAD:
            WriteLog("A read from media returned an error.\n");
            break;
        case OV_ENOTVORBIS:
            WriteLog("Bitstream does not contain any Vorbis data.\n");
            break;
        case OV_EVERSION:
            WriteLog("Vorbis version mismatch.\n");
            break;
        case OV_EBADHEADER:
            WriteLog("Invalid Vorbis bitstream header.\n");
            break;
        case OV_EFAULT:
            WriteLog("Internal logic fault; indicates a bug or heap/stack corruption.\n");
            break;
        default:
            WriteLog("Unknown error code {}.\n", error);
            break;
        }
        return false;
    }

    vorbis_info* vi = ov_info(sound->OggStream, -1);
    if (!vi)
    {
        WriteLog("Ogg info error.\n");
        return false;
    }

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = (int)vi->rate;
    sound->BaseBuf = new unsigned char[streamingPortion];

    int result = 0;
    uint decoded = 0;
    while (true)
    {
        result = (int)ov_read(
            sound->OggStream, (char*)sound->BaseBuf + decoded, streamingPortion - decoded, 0, 2, 1, nullptr);
        if (result <= 0)
            break;

        decoded += result;
        if (decoded >= streamingPortion)
            break;
    }
    if (result < 0)
        return false;

    sound->BaseBufSize = decoded;

    // No need streaming
    if (!result)
    {
        ov_clear(sound->OggStream);
        SAFEDEL(sound->OggStream);
    }

    return ConvertData(sound);
}

bool SoundManager::StreamOGG(Sound* sound)
{
    int result = 0;
    uint decoded = 0;
    while (true)
    {
        result = (int)ov_read(
            sound->OggStream, (char*)sound->BaseBuf + decoded, streamingPortion - decoded, 0, 2, 1, nullptr);
        if (result <= 0)
            break;
        decoded += result;
        if (decoded >= streamingPortion)
            break;
    }
    if (result < 0 || !decoded)
        return false;

    sound->BaseBufSize = decoded;
    return ConvertData(sound);
}

bool SoundManager::ConvertData(Sound* sound)
{
    if (!sound->CvtBuilded)
    {
        sound->CvtBuilded = true;
        if (SDL_BuildAudioCVT(&sound->Cvt, sound->OriginalFormat, sound->OriginalChannels, sound->OriginalRate,
                soundSpec->format, soundSpec->channels, soundSpec->freq) == -1)
        {
            WriteLog("SDL_BuildAudioCVT fail, error '{}'.\n", SDL_GetError());
            return false;
        }
    }

    sound->Cvt.len = sound->BaseBufSize;
    if (sound->Cvt.len * sound->Cvt.len_mult > (int)sound->ConvertedBufRealSize)
    {
        sound->ConvertedBufRealSize = sound->Cvt.len * sound->Cvt.len_mult * 4;
        SAFEDELA(sound->ConvertedBuf);
        sound->ConvertedBuf = new unsigned char[sound->ConvertedBufRealSize];
    }
    sound->Cvt.buf = (Uint8*)sound->ConvertedBuf;
    memcpy(sound->Cvt.buf, sound->BaseBuf, sound->BaseBufSize);

    if (SDL_ConvertAudio(&sound->Cvt))
    {
        WriteLog("SDL_ConvertAudio fail, error '{}'.\n", SDL_GetError());
        return false;
    }

    sound->ConvertedBufCur = 0;
    sound->ConvertedBufSize = sound->Cvt.len_cvt;
    return true;
}

bool SoundManager::PlaySound(const StrMap& sound_names, const string& name)
{
    if (!isActive || !GameOpt.SoundVolume)
        return true;

    // Make 'NAME'
    string sound_name = _str(name).eraseFileExtension().upper();

    // Find base
    auto it = sound_names.find(sound_name);
    if (it != sound_names.end())
        return Load(it->second, false) != nullptr;

    // Check random pattern 'NAME_X'
    uint count = 0;
    while (sound_names.find(_str("{}_{}", sound_name, count + 1)) != sound_names.end())
        count++;
    if (count)
        return Load(sound_names.find(_str("{}_{}", sound_name, Random(1, count)))->second, false) != nullptr;

    return false;
}

bool SoundManager::PlayMusic(const string& fname, uint repeat_time)
{
    if (!isActive)
        return true;

    StopMusic();

    // Load new
    Sound* sound = Load(fname, true);
    if (!sound)
        return false;

    sound->IsMusic = true;
    sound->RepeatTime = repeat_time;
    return true;
}

void SoundManager::StopSounds()
{
    SDL_LockAudioDevice(*deviceID);

    for (auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if (!sound->IsMusic)
        {
            delete *it;
            it = soundsActive.erase(it);
        }
        else
        {
            ++it;
        }
    }

    SDL_UnlockAudioDevice(*deviceID);
}

void SoundManager::StopMusic()
{
    SDL_LockAudioDevice(*deviceID);

    for (auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if (sound->IsMusic)
        {
            delete sound;
            it = soundsActive.erase(it);
        }
        else
        {
            ++it;
        }
    }

    SDL_UnlockAudioDevice(*deviceID);
}
