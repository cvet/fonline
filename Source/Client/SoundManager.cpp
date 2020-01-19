#include "SoundManager.h"
#include "FileSystem.h"
#include "Log.h"
#include "ResourceManager.h"
#include "StringUtils.h"
#include "Timer.h"

#include "SDL_audio.h"
#include "acmstrm.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

struct SoundManager::Impl
{
    SDL_AudioDeviceID DeviceId {};
    SDL_AudioSpec SoundSpec {};
};

struct SoundManager::Sound
{
    vector<uchar> BaseBuf {};
    size_t BaseBufLen {};
    unique_ptr<SDL_AudioCVT> Cvt {};
    vector<uchar> ConvertedBuf {};
    size_t ConvertedBufLen {};
    size_t ConvertedBufCur {};
    int OriginalFormat {};
    int OriginalChannels {};
    int OriginalRate {};
    bool IsMusic {};
    uint NextPlay {};
    uint RepeatTime {};
    unique_ptr<OggVorbis_File, std::function<void(OggVorbis_File*)>> OggStream {};
};

SoundManager::SoundManager(FileManager& file_mngr) : fileMngr {file_mngr}
{
    UNUSED_VARIABLE(OV_CALLBACKS_DEFAULT);
    UNUSED_VARIABLE(OV_CALLBACKS_NOCLOSE);
    UNUSED_VARIABLE(OV_CALLBACKS_STREAMONLY);
    UNUSED_VARIABLE(OV_CALLBACKS_STREAMONLY_NOCLOSE);

    // SDL
    isAudioInited = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!isAudioInited)
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

    pImpl = std::make_unique<Impl>();
    pImpl->DeviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &pImpl->SoundSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (pImpl->DeviceId < 2)
    {
        WriteLog("SDL Open audio device fail, error '{}'.\n", SDL_GetError());
        return;
    }

    outputBuf.resize(pImpl->SoundSpec.size);
    isActive = true;

    // Start playing
    SDL_PauseAudioDevice(pImpl->DeviceId, 0);
}

SoundManager::~SoundManager()
{
    if (isActive)
    {
        StopSounds();
        StopMusic();
    }

    if (pImpl && pImpl->DeviceId >= 2)
        SDL_CloseAudioDevice(pImpl->DeviceId);

    if (isAudioInited)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SoundManager::ProcessSounds(uchar* output)
{
    memset(output, pImpl->SoundSpec.silence, pImpl->SoundSpec.size);

    for (auto it = soundsActive.begin(); it != soundsActive.end();)
    {
        Sound* sound = *it;
        if (ProcessSound(sound, &outputBuf[0]))
        {
            int volume = (sound->IsMusic ? GameOpt.MusicVolume : GameOpt.SoundVolume);
            volume = std::clamp(volume, 0, 100) * SDL_MIX_MAXVOLUME / 100;
            SDL_MixAudioFormat(output, &outputBuf[0], pImpl->SoundSpec.format, pImpl->SoundSpec.size, volume);
            ++it;
        }
        else
        {
            it = soundsActive.erase(it);
        }
    }
}

bool SoundManager::ProcessSound(Sound* sound, uchar* output)
{
    // Playing
    size_t whole = pImpl->SoundSpec.size;
    if (sound->ConvertedBufCur < sound->ConvertedBufLen)
    {
        if (whole > sound->ConvertedBufLen - sound->ConvertedBufCur)
        {
            // Flush last part of buffer
            size_t offset = sound->ConvertedBufLen - sound->ConvertedBufCur;
            memcpy(output, &sound->ConvertedBuf[sound->ConvertedBufCur], offset);
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while (offset < whole && sound->OggStream && StreamOGG(sound))
            {
                size_t write = sound->ConvertedBufLen - sound->ConvertedBufCur;
                if (offset + write > whole)
                    write = whole - offset;

                memcpy(output + offset, &sound->ConvertedBuf[sound->ConvertedBufCur], write);
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if (offset < whole)
                memset(output + offset, pImpl->SoundSpec.silence, whole - offset);
        }
        else
        {
            // Copy
            memcpy(output, &sound->ConvertedBuf[sound->ConvertedBufCur], whole);
            sound->ConvertedBufCur += whole;
        }

        if (sound->OggStream && sound->ConvertedBufCur == sound->ConvertedBufLen)
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
                ov_raw_seek(sound->OggStream.get(), 0);

            // Drop timer
            sound->NextPlay = 0;

            // Process without silent
            return ProcessSound(sound, output);
        }

        // Give silent
        memset(output, pImpl->SoundSpec.silence, whole);
        return true;
    }

    // Give silent
    memset(output, pImpl->SoundSpec.silence, whole);
    return false;
}

SoundManager::Sound* SoundManager::Load(const string& fname, bool is_music)
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
    if (ext == "wav" && !LoadWAV(sound, fixed_fname))
        return nullptr;
    if (ext == "acm" && !LoadACM(sound, fixed_fname, is_music))
        return nullptr;
    if (ext == "ogg" && !LoadOGG(sound, fixed_fname))
        return nullptr;

    SDL_LockAudioDevice(pImpl->DeviceId);
    soundsActive.push_back(sound);
    SDL_UnlockAudioDevice(pImpl->DeviceId);
    return sound;
}

bool SoundManager::LoadWAV(Sound* sound, const string& fname)
{
    File file = fileMngr.ReadFile(fname);
    if (!file)
        return false;

    uint dw_buf = file.GetLEUInt();
    if (dw_buf != MAKEUINT('R', 'I', 'F', 'F'))
    {
        WriteLog("'RIFF' not found.\n");
        return false;
    }

    file.GoForward(4);

    dw_buf = file.GetLEUInt();
    if (dw_buf != MAKEUINT('W', 'A', 'V', 'E'))
    {
        WriteLog("'WAVE' not found.\n");
        return false;
    }

    dw_buf = file.GetLEUInt();
    if (dw_buf != MAKEUINT('f', 'm', 't', ' '))
    {
        WriteLog("'fmt ' not found.\n");
        return false;
    }

    dw_buf = file.GetLEUInt();
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

    file.CopyMem(&waveformatex, 16);

    if (waveformatex.wFormatTag != 1)
    {
        WriteLog("Compressed files not supported.\n");
        return false;
    }

    file.GoForward(dw_buf - 16);

    dw_buf = file.GetLEUInt();
    if (dw_buf == MAKEUINT('f', 'a', 'c', 't'))
    {
        dw_buf = file.GetLEUInt();
        file.GoForward(dw_buf);
        dw_buf = file.GetLEUInt();
    }

    if (dw_buf != MAKEUINT('d', 'a', 't', 'a'))
    {
        WriteLog("Unknown format2.\n");
        return false;
    }

    dw_buf = file.GetLEUInt();
    sound->BaseBuf.resize(dw_buf);
    sound->BaseBufLen = dw_buf;

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
    file.CopyMem(&sound->BaseBuf[0], static_cast<uint>(sound->BaseBufLen));

    return ConvertData(sound);
}

bool SoundManager::LoadACM(Sound* sound, const string& fname, bool is_music)
{
    File file = fileMngr.ReadFile(fname);
    if (!file)
        return false;

    int channels = 0;
    int freq = 0;
    int samples = 0;
    auto acm = std::make_unique<CACMUnpacker>(file.GetBuf(), file.GetFsize(), channels, freq, samples);
    int buf_size = samples * 2;

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = (is_music ? 2 : 1);
    sound->OriginalRate = 22050;
    sound->BaseBuf.resize(buf_size);
    sound->BaseBufLen = sound->BaseBuf.size();

    auto* buf = reinterpret_cast<unsigned short*>(&sound->BaseBuf[0]);
    int dec_data = acm->readAndDecompress(buf, buf_size);
    if (dec_data != buf_size)
    {
        WriteLog("Decode Acm error.\n");
        return false;
    }

    return ConvertData(sound);
}

bool SoundManager::LoadOGG(Sound* sound, const string& fname)
{
    File file = fileMngr.ReadFile(fname);
    if (!file)
        return false;

    ov_callbacks callbacks;
    callbacks.read_func = [](void* ptr, size_t size, size_t nmemb, void* datasource) -> size_t {
        File* file = (File*)datasource;
        file->CopyMem(ptr, (uint)size);
        return size;
    };
    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int whence) -> int {
        File* file = (File*)datasource;
        switch (whence)
        {
        case SEEK_SET:
            file->SetCurPos((uint)offset);
            break;
        case SEEK_CUR:
            file->GoForward((uint)offset);
            break;
        case SEEK_END:
            file->SetCurPos(file->GetFsize() - (uint)offset);
            break;
        default:
            return -1;
        }
        return 0;
    };
    callbacks.close_func = [](void* datasource) -> int {
        File* file = (File*)datasource;
        delete file;
        return 0;
    };
    callbacks.tell_func = [](void* datasource) -> long {
        File* file = (File*)datasource;
        return file->GetCurPos();
    };

    sound->OggStream =
        unique_ptr<OggVorbis_File, std::function<void(OggVorbis_File*)>>(new OggVorbis_File(), [](auto* vf) {
            ov_clear(vf);
            delete vf;
        });

    int error = ov_open_callbacks(new File {std::move(file)}, sound->OggStream.get(), nullptr, 0, callbacks);
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

    vorbis_info* vi = ov_info(sound->OggStream.get(), -1);
    if (!vi)
    {
        WriteLog("Ogg info error.\n");
        return false;
    }

    sound->OriginalFormat = AUDIO_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = (int)vi->rate;
    sound->BaseBuf.resize(streamingPortion);
    sound->BaseBufLen = streamingPortion;

    int result = 0;
    uint decoded = 0;
    while (true)
    {
        char* buf = reinterpret_cast<char*>(&sound->BaseBuf[0]);
        result = (int)ov_read(sound->OggStream.get(), buf + decoded, streamingPortion - decoded, 0, 2, 1, nullptr);
        if (result <= 0)
            break;

        decoded += result;
        if (decoded >= streamingPortion)
            break;
    }
    if (result < 0)
        return false;

    sound->BaseBufLen = decoded;

    // No need streaming
    if (!result)
        sound->OggStream = nullptr;

    return ConvertData(sound);
}

bool SoundManager::StreamOGG(Sound* sound)
{
    long result = 0;
    uint decoded = 0;
    while (true)
    {
        char* buf = reinterpret_cast<char*>(&sound->BaseBuf[decoded]);
        result = ov_read(sound->OggStream.get(), buf, streamingPortion - decoded, 0, 2, 1, nullptr);
        if (result <= 0)
            break;

        decoded += result;
        if (decoded >= streamingPortion)
            break;
    }
    if (result < 0 || !decoded)
        return false;

    sound->BaseBufLen = decoded;
    return ConvertData(sound);
}

bool SoundManager::ConvertData(Sound* sound)
{
    if (!sound->Cvt)
    {
        sound->Cvt = std::make_unique<SDL_AudioCVT>();

        if (SDL_BuildAudioCVT(sound->Cvt.get(), sound->OriginalFormat, sound->OriginalChannels, sound->OriginalRate,
                pImpl->SoundSpec.format, pImpl->SoundSpec.channels, pImpl->SoundSpec.freq) == -1)
        {
            WriteLog("SDL_BuildAudioCVT fail, error '{}'.\n", SDL_GetError());
            return false;
        }
    }

    sound->Cvt->len = static_cast<int>(sound->BaseBufLen);
    sound->ConvertedBuf.resize(size_t(1) * sound->Cvt->len * sound->Cvt->len_mult);
    sound->Cvt->buf = reinterpret_cast<Uint8*>(&sound->ConvertedBuf[0]);
    memcpy(sound->Cvt->buf, &sound->BaseBuf[0], sound->BaseBufLen);

    if (SDL_ConvertAudio(sound->Cvt.get()))
    {
        WriteLog("SDL_ConvertAudio fail, error '{}'.\n", SDL_GetError());
        return false;
    }

    sound->ConvertedBufCur = 0;
    sound->ConvertedBufLen = sound->Cvt->len_cvt;
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

    Sound* sound = Load(fname, true);
    if (!sound)
        return false;

    sound->IsMusic = true;
    sound->RepeatTime = repeat_time;
    return true;
}

void SoundManager::StopSounds()
{
    SDL_LockAudioDevice(pImpl->DeviceId);
    soundsActive.erase(std::remove_if(soundsActive.begin(), soundsActive.end(), [](auto s) { return !s->IsMusic; }),
        soundsActive.end());
    SDL_UnlockAudioDevice(pImpl->DeviceId);
}

void SoundManager::StopMusic()
{
    SDL_LockAudioDevice(pImpl->DeviceId);
    soundsActive.erase(std::remove_if(soundsActive.begin(), soundsActive.end(), [](auto s) { return s->IsMusic; }),
        soundsActive.end());
    SDL_UnlockAudioDevice(pImpl->DeviceId);
}
