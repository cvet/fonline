//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "SoundManager.h"
#include "Application.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "Timer.h"

#include "acmstrm.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

struct SoundManager::Sound
{
    vector<uchar> BaseBuf {};
    size_t BaseBufLen {};
    vector<uchar> ConvertedBuf {};
    size_t ConvertedBufCur {};
    int OriginalFormat {};
    int OriginalChannels {};
    int OriginalRate {};
    bool IsMusic {};
    uint NextPlay {};
    uint RepeatTime {};
    unique_ptr<OggVorbis_File, std::function<void(OggVorbis_File*)>> OggStream {};
};

static constexpr auto MAKEUINT(uchar ch0, uchar ch1, uchar ch2, uchar ch3) -> uint
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

SoundManager::SoundManager(AudioSettings& settings, FileSystem& file_sys) : _settings {settings}, _fileSys {file_sys}
{
    UNUSED_VARIABLE(OV_CALLBACKS_DEFAULT);
    UNUSED_VARIABLE(OV_CALLBACKS_NOCLOSE);
    UNUSED_VARIABLE(OV_CALLBACKS_STREAMONLY);
    UNUSED_VARIABLE(OV_CALLBACKS_STREAMONLY_NOCLOSE);

    if (!App->Audio.IsEnabled()) {
        return;
    }
    if (_settings.DisableAudio) {
        return;
    }

#if FO_WEB
    _streamingPortion = 0x20000; // 128kb
#else
    _streamingPortion = 0x10000; // 64kb
#endif

    _outputBuf.resize(App->Audio.GetStreamSize());
    App->Audio.SetSource(std::bind(&SoundManager::ProcessSounds, this, std::placeholders::_1));
    _isActive = true;
}

SoundManager::~SoundManager()
{
    if (_isActive) {
        App->Audio.SetSource(nullptr);
        StopSounds();
        StopMusic();
    }
}

void SoundManager::ProcessSounds(uchar* output)
{
    for (auto it = _soundsActive.begin(); it != _soundsActive.end();) {
        auto* sound = *it;
        if (ProcessSound(sound, &_outputBuf[0])) {
            const auto volume = sound->IsMusic ? _settings.MusicVolume : _settings.SoundVolume;
            App->Audio.MixAudio(output, &_outputBuf[0], volume);
            ++it;
        }
        else {
            it = _soundsActive.erase(it);
        }
    }
}

auto SoundManager::ProcessSound(Sound* sound, uchar* output) -> bool
{
    // Playing
    const auto whole = _outputBuf.size();
    if (sound->ConvertedBufCur < sound->ConvertedBuf.size()) {
        if (whole > sound->ConvertedBuf.size() - sound->ConvertedBufCur) {
            // Flush last part of buffer
            auto offset = sound->ConvertedBuf.size() - sound->ConvertedBufCur;
            std::memcpy(output, &sound->ConvertedBuf[sound->ConvertedBufCur], offset);
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while (offset < whole && sound->OggStream && StreamOgg(sound)) {
                auto write = sound->ConvertedBuf.size() - sound->ConvertedBufCur;
                if (offset + write > whole) {
                    write = whole - offset;
                }

                std::memcpy(output + offset, &sound->ConvertedBuf[sound->ConvertedBufCur], write);
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if (offset < whole) {
                std::memset(output + offset, App->Audio.GetSilence(), whole - offset);
            }
        }
        else {
            // Copy
            std::memcpy(output, &sound->ConvertedBuf[sound->ConvertedBufCur], whole);
            sound->ConvertedBufCur += whole;
        }

        if (sound->OggStream && sound->ConvertedBufCur == sound->ConvertedBuf.size()) {
            StreamOgg(sound);
        }

        // Continue processing
        return true;
    }

    // Repeat
    if (sound->RepeatTime != 0u) {
        if (sound->NextPlay == 0u) {
            // Set next playing time
            sound->NextPlay = static_cast<uint>(Timer::RealtimeTick()) + (sound->RepeatTime > 1 ? sound->RepeatTime : 0);
        }

        if (static_cast<uint>(Timer::RealtimeTick()) >= sound->NextPlay) {
            // Set buffer to beginning
            sound->ConvertedBufCur = 0;
            if (sound->OggStream) {
                ov_raw_seek(sound->OggStream.get(), 0);
            }

            // Drop timer
            sound->NextPlay = 0;

            // Process without silent
            return ProcessSound(sound, output);
        }

        // Give silent
        std::memset(output, App->Audio.GetSilence(), whole);
        return true;
    }

    // Give silent
    std::memset(output, App->Audio.GetSilence(), whole);
    return false;
}

auto SoundManager::Load(string_view fname, bool is_music) -> Sound*
{
    auto fixed_fname = string(fname);
    string ext = _str(fname).getFileExtension();

    // Default ext
    if (ext.empty()) {
        ext = "acm";
        fixed_fname += "." + ext;
    }

    auto* sound = new Sound();
    if (ext == "wav" && !LoadWav(sound, fixed_fname)) {
        return nullptr;
    }
    if (ext == "acm" && !LoadAcm(sound, fixed_fname, is_music)) {
        return nullptr;
    }
    if (ext == "ogg" && !LoadOgg(sound, fixed_fname)) {
        return nullptr;
    }

    App->Audio.LockDevice();
    _soundsActive.push_back(sound);
    App->Audio.UnlockDevice();
    return sound;
}

auto SoundManager::LoadWav(Sound* sound, string_view fname) -> bool
{
    NON_CONST_METHOD_HINT();

    auto file = _fileSys.ReadFile(fname);
    if (!file) {
        return false;
    }

    auto dw_buf = file.GetLEUInt();
    if (dw_buf != MAKEUINT('R', 'I', 'F', 'F')) {
        WriteLog("'RIFF' not found.\n");
        return false;
    }

    file.GoForward(4);

    dw_buf = file.GetLEUInt();
    if (dw_buf != MAKEUINT('W', 'A', 'V', 'E')) {
        WriteLog("'WAVE' not found.\n");
        return false;
    }

    dw_buf = file.GetLEUInt();
    if (dw_buf != MAKEUINT('f', 'm', 't', ' ')) {
        WriteLog("'fmt ' not found.\n");
        return false;
    }

    dw_buf = file.GetLEUInt();
    if (dw_buf == 0u) {
        WriteLog("Unknown format.\n");
        return false;
    }

    struct WaveFormatEx
    {
        ushort WFormatTag; // Integer identifier of the format
        ushort NChannels; // Number of audio channels
        uint NSamplesPerSec; // Audio sample rate
        uint NAvgBytesPerSec; // Bytes per second (possibly approximate)
        ushort NBlockAlign; // Size in bytes of a sample block (all channels)
        ushort WBitsPerSample; // Size in bits of a single per-channel sample
        ushort CbSize; // Bytes of extra data appended to this struct
    } waveformatex {};

    file.CopyMem(&waveformatex, 16);

    if (waveformatex.WFormatTag != 1) {
        WriteLog("Compressed files not supported.\n");
        return false;
    }

    file.GoForward(dw_buf - 16);

    dw_buf = file.GetLEUInt();
    if (dw_buf == MAKEUINT('f', 'a', 'c', 't')) {
        dw_buf = file.GetLEUInt();
        file.GoForward(dw_buf);
        dw_buf = file.GetLEUInt();
    }

    if (dw_buf != MAKEUINT('d', 'a', 't', 'a')) {
        WriteLog("Unknown format2.\n");
        return false;
    }

    dw_buf = file.GetLEUInt();
    sound->BaseBuf.resize(dw_buf);
    sound->BaseBufLen = dw_buf;

    // Check format
    sound->OriginalChannels = waveformatex.NChannels;
    sound->OriginalRate = waveformatex.NSamplesPerSec;
    switch (waveformatex.WBitsPerSample) {
    case 8:
        sound->OriginalFormat = App->Audio.AUDIO_FORMAT_U8;
        break;
    case 16:
        sound->OriginalFormat = App->Audio.AUDIO_FORMAT_S16;
        break;
    default:
        WriteLog("Unknown format.\n");
        return false;
    }

    // Convert
    file.CopyMem(&sound->BaseBuf[0], static_cast<uint>(sound->BaseBufLen));

    return ConvertData(sound);
}

auto SoundManager::LoadAcm(Sound* sound, string_view fname, bool is_music) -> bool
{
    NON_CONST_METHOD_HINT();

    const auto file = _fileSys.ReadFile(fname);
    if (!file) {
        return false;
    }

    auto channels = 0;
    auto freq = 0;
    auto samples = 0;
    auto acm = std::make_unique<CACMUnpacker>(const_cast<uchar*>(file.GetBuf()), file.GetSize(), channels, freq, samples);
    const auto buf_size = samples * 2;

    sound->OriginalFormat = App->Audio.AUDIO_FORMAT_S16;
    sound->OriginalChannels = is_music ? 2 : 1;
    sound->OriginalRate = 22050;
    sound->BaseBuf.resize(buf_size);
    sound->BaseBufLen = sound->BaseBuf.size();

    auto* buf = reinterpret_cast<unsigned short*>(&sound->BaseBuf[0]);
    const auto dec_data = acm->readAndDecompress(buf, buf_size);
    if (dec_data != buf_size) {
        WriteLog("Decode Acm error.\n");
        return false;
    }

    return ConvertData(sound);
}

auto SoundManager::LoadOgg(Sound* sound, string_view fname) -> bool
{
    auto file = _fileSys.ReadFile(fname);
    if (!file) {
        return false;
    }

    ov_callbacks callbacks;
    callbacks.read_func = [](void* ptr, size_t size, size_t, void* datasource) -> size_t {
        auto* file2 = static_cast<File*>(datasource);
        file2->CopyMem(ptr, static_cast<uint>(size));
        return size;
    };
    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int whence) -> int {
        auto* file2 = static_cast<File*>(datasource);
        switch (whence) {
        case SEEK_SET:
            file2->SetCurPos(static_cast<uint>(offset));
            break;
        case SEEK_CUR:
            file2->GoForward(static_cast<uint>(offset));
            break;
        case SEEK_END:
            file2->SetCurPos(file2->GetSize() - static_cast<uint>(offset));
            break;
        default:
            return -1;
        }
        return 0;
    };
    callbacks.close_func = [](void* datasource) -> int {
        auto* file2 = static_cast<File*>(datasource);
        delete file2;
        return 0;
    };
    callbacks.tell_func = [](void* datasource) -> long {
        auto* file2 = static_cast<File*>(datasource);
        return file2->GetCurPos();
    };

    sound->OggStream = unique_ptr<OggVorbis_File, std::function<void(OggVorbis_File*)>>(new OggVorbis_File(), [](auto* vf) {
        ov_clear(vf);
        delete vf;
    });

    const auto error = ov_open_callbacks(new File {std::move(file)}, sound->OggStream.get(), nullptr, 0, callbacks);
    if (error != 0) {
        WriteLog("Open OGG file '{}' fail, error:\n", fname);
        switch (error) {
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

    auto* vi = ov_info(sound->OggStream.get(), -1);
    if (vi == nullptr) {
        WriteLog("Ogg info error.\n");
        return false;
    }

    sound->OriginalFormat = App->Audio.AUDIO_FORMAT_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = static_cast<int>(vi->rate);
    sound->BaseBuf.resize(_streamingPortion);
    sound->BaseBufLen = _streamingPortion;

    int result;
    auto decoded = 0u;

    while (true) {
        auto* buf = reinterpret_cast<char*>(&sound->BaseBuf[0]);
        result = static_cast<int>(ov_read(sound->OggStream.get(), buf + decoded, static_cast<int>(_streamingPortion - decoded), 0, 2, 1, nullptr));
        if (result <= 0) {
            break;
        }

        decoded += result;
        if (decoded >= _streamingPortion) {
            break;
        }
    }

    if (result < 0) {
        return false;
    }

    sound->BaseBufLen = decoded;

    // No need streaming
    if (result == 0) {
        sound->OggStream = nullptr;
    }

    return ConvertData(sound);
}

auto SoundManager::StreamOgg(Sound* sound) -> bool
{
    NON_CONST_METHOD_HINT();

    long result;
    auto decoded = 0u;

    while (true) {
        auto* buf = reinterpret_cast<char*>(&sound->BaseBuf[decoded]);
        result = ov_read(sound->OggStream.get(), buf, static_cast<int>(_streamingPortion - decoded), 0, 2, 1, nullptr);
        if (result <= 0) {
            break;
        }

        decoded += result;
        if (decoded >= _streamingPortion) {
            break;
        }
    }

    if (result < 0 || decoded == 0u) {
        return false;
    }

    sound->BaseBufLen = decoded;
    return ConvertData(sound);
}

auto SoundManager::ConvertData(Sound* sound) -> bool
{
    sound->ConvertedBuf = sound->BaseBuf;
    sound->ConvertedBuf.resize(sound->BaseBufLen);

    if (!App->Audio.ConvertAudio(sound->OriginalFormat, sound->OriginalChannels, sound->OriginalRate, sound->ConvertedBuf)) {
        return false;
    }

    sound->ConvertedBufCur = 0;
    return true;
}

auto SoundManager::PlaySound(const map<string, string>& sound_names, string_view name) -> bool
{
    if (!_isActive || _settings.SoundVolume == 0) {
        return true;
    }

    // Make 'NAME'
    const string sound_name = _str(name).eraseFileExtension().upper();

    // Find base
    const auto it = sound_names.find(sound_name);
    if (it != sound_names.end()) {
        return Load(it->second, false) != nullptr;
    }

    // Check random pattern 'NAME_X'
    uint count = 0;
    while (sound_names.find(_str("{}_{}", sound_name, count + 1)) != sound_names.end()) {
        count++;
    }
    if (count != 0u) {
        return !(Load(sound_names.find(_str("{}_{}", sound_name, GenericUtils::Random(1u, count)))->second, false) == nullptr);
    }

    return false;
}

auto SoundManager::PlayMusic(string_view fname, uint repeat_time) -> bool
{
    if (!_isActive) {
        return true;
    }

    StopMusic();

    auto* sound = Load(fname, true);
    if (sound == nullptr) {
        return false;
    }

    sound->IsMusic = true;
    sound->RepeatTime = repeat_time;
    return true;
}

void SoundManager::StopSounds()
{
    App->Audio.LockDevice();
    _soundsActive.erase(std::remove_if(_soundsActive.begin(), _soundsActive.end(), [](auto s) { return !s->IsMusic; }), _soundsActive.end());
    App->Audio.UnlockDevice();
}

void SoundManager::StopMusic()
{
    App->Audio.LockDevice();
    _soundsActive.erase(std::remove_if(_soundsActive.begin(), _soundsActive.end(), [](auto s) { return s->IsMusic; }), _soundsActive.end());
    App->Audio.UnlockDevice();
}
