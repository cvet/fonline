//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "acmstrm.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

struct SoundManager::Sound
{
    vector<uint8> BaseBuf {};
    size_t BaseBufLen {};
    vector<uint8> ConvertedBuf {};
    size_t ConvertedBufCur {};
    int OriginalFormat {};
    int OriginalChannels {};
    int OriginalRate {};
    bool IsMusic {};
    time_point_t NextPlayTime {};
    time_duration_t RepeatTime {};
    unique_del_ptr<OggVorbis_File> OggStream {};
};

static constexpr auto MAKEUINT(uint8 ch0, uint8 ch1, uint8 ch2, uint8 ch3) -> uint
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

SoundManager::SoundManager(AudioSettings& settings, FileSystem& resources) :
    _settings {settings},
    _resources {resources}
{
    STACK_TRACE_ENTRY();

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

    App->Audio.SetSource([this](uint8 silence, span<uint8> output) { ProcessSounds(silence, output); });
    _isActive = true;
}

SoundManager::~SoundManager()
{
    STACK_TRACE_ENTRY();

    if (_isActive) {
        App->Audio.SetSource(nullptr);

        App->Audio.LockDevice();
        _playingSounds.clear();
        App->Audio.UnlockDevice();
    }
}

void SoundManager::ProcessSounds(uint8 silence, span<uint8> output)
{
    STACK_TRACE_ENTRY();

    if (output.size() > _outputBuf.size()) {
        _outputBuf.resize(output.size());
    }

    for (auto it = _playingSounds.begin(); it != _playingSounds.end();) {
        auto& sound = *it;

        if (ProcessSound(sound.get(), silence, {_outputBuf.data(), output.size()})) {
            const auto volume = sound->IsMusic ? _settings.MusicVolume : _settings.SoundVolume;
            App->Audio.MixAudio(output.data(), _outputBuf.data(), output.size(), static_cast<int>(volume));
            ++it;
        }
        else {
            it = _playingSounds.erase(it);
        }
    }
}

auto SoundManager::ProcessSound(Sound* sound, uint8 silence, span<uint8> output) -> bool
{
    STACK_TRACE_ENTRY();

    // Playing
    if (sound->ConvertedBufCur < sound->ConvertedBuf.size()) {
        if (output.size() > sound->ConvertedBuf.size() - sound->ConvertedBufCur) {
            // Flush last part of buffer
            auto offset = sound->ConvertedBuf.size() - sound->ConvertedBufCur;
            MemCopy(output.data(), &sound->ConvertedBuf[sound->ConvertedBufCur], offset);
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while (offset < output.size() && sound->OggStream && StreamOgg(sound)) {
                auto write = sound->ConvertedBuf.size() - sound->ConvertedBufCur;

                if (offset + write > output.size()) {
                    write = output.size() - offset;
                }

                MemCopy(output.data() + offset, &sound->ConvertedBuf[sound->ConvertedBufCur], write);
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if (offset < output.size()) {
                MemFill(output.data() + offset, silence, output.size() - offset);
            }
        }
        else {
            // Copy
            MemCopy(output.data(), &sound->ConvertedBuf[sound->ConvertedBufCur], output.size());
            sound->ConvertedBufCur += output.size();
        }

        if (sound->OggStream && sound->ConvertedBufCur == sound->ConvertedBuf.size()) {
            StreamOgg(sound);
        }

        // Continue processing
        return true;
    }

    // Repeat
    if (sound->RepeatTime) {
        if (!sound->NextPlayTime) {
            sound->NextPlayTime = time_point_t::now() + (sound->RepeatTime > std::chrono::milliseconds {1} ? sound->RepeatTime : time_duration_t::zero);
        }

        if (time_point_t::now() >= sound->NextPlayTime) {
            // Set buffer to beginning
            sound->ConvertedBufCur = 0;

            if (sound->OggStream) {
                ov_raw_seek(sound->OggStream.get(), 0);
            }

            // Drop timer
            sound->NextPlayTime = time_point_t::zero;

            // Process without silent
            return ProcessSound(sound, silence, output);
        }

        // Give silent
        MemFill(output.data(), silence, output.size());
        return true;
    }

    // Give silent
    MemFill(output.data(), silence, output.size());

    return false;
}

auto SoundManager::Load(string_view fname, bool is_music, time_duration_t repeat_time) -> bool
{
    STACK_TRACE_ENTRY();

    auto fixed_fname = string(fname);
    string ext = strex(fname).getFileExtension();

    // Default ext
    if (ext.empty()) {
        ext = "acm";
        fixed_fname += "." + ext;
    }

    auto sound = SafeAlloc::MakeUnique<Sound>();

    if (ext == "wav" && !LoadWav(sound.get(), fixed_fname)) {
        return false;
    }
    if (ext == "acm" && !LoadAcm(sound.get(), fixed_fname, is_music)) {
        return false;
    }
    if (ext == "ogg" && !LoadOgg(sound.get(), fixed_fname)) {
        return false;
    }

    sound->IsMusic = is_music;
    sound->RepeatTime = repeat_time;

    App->Audio.LockDevice();
    _playingSounds.emplace_back(std::move(sound));
    App->Audio.UnlockDevice();

    return true;
}

auto SoundManager::LoadWav(Sound* sound, string_view fname) -> bool
{
    STACK_TRACE_ENTRY();

    auto file = _resources.ReadFile(fname);

    if (!file) {
        return false;
    }

    auto dw_buf = file.GetLEUInt();

    if (dw_buf != MAKEUINT('R', 'I', 'F', 'F')) {
        WriteLog("'RIFF' not found");
        return false;
    }

    file.GoForward(4);

    dw_buf = file.GetLEUInt();

    if (dw_buf != MAKEUINT('W', 'A', 'V', 'E')) {
        WriteLog("'WAVE' not found");
        return false;
    }

    dw_buf = file.GetLEUInt();

    if (dw_buf != MAKEUINT('f', 'm', 't', ' ')) {
        WriteLog("'fmt ' not found");
        return false;
    }

    dw_buf = file.GetLEUInt();

    if (dw_buf == 0) {
        WriteLog("Unknown format");
        return false;
    }

    struct WaveFormatEx
    {
        uint16 WFormatTag; // Integer identifier of the format
        uint16 NChannels; // Number of audio channels
        uint NSamplesPerSec; // Audio sample rate
        uint NAvgBytesPerSec; // Bytes per second (possibly approximate)
        uint16 NBlockAlign; // Size in bytes of a sample block (all channels)
        uint16 WBitsPerSample; // Size in bits of a single per-channel sample
        uint16 CbSize; // Bytes of extra data appended to this struct
    } waveformatex {};

    file.CopyData(&waveformatex, 16);

    if (waveformatex.WFormatTag != 1) {
        WriteLog("Compressed files not supported");
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
        WriteLog("Unknown format2");
        return false;
    }

    dw_buf = file.GetLEUInt();
    sound->BaseBuf.resize(dw_buf);
    sound->BaseBufLen = dw_buf;

    // Check format
    sound->OriginalChannels = static_cast<int>(waveformatex.NChannels);
    sound->OriginalRate = static_cast<int>(waveformatex.NSamplesPerSec);

    switch (waveformatex.WBitsPerSample) {
    case 8:
        sound->OriginalFormat = AppAudio::AUDIO_FORMAT_U8;
        break;
    case 16:
        sound->OriginalFormat = AppAudio::AUDIO_FORMAT_S16;
        break;
    default:
        WriteLog("Unknown format");
        return false;
    }

    // Convert
    file.CopyData(sound->BaseBuf.data(), sound->BaseBufLen);

    return ConvertData(sound);
}

auto SoundManager::LoadAcm(Sound* sound, string_view fname, bool is_music) -> bool
{
    STACK_TRACE_ENTRY();

    const auto file = _resources.ReadFile(fname);

    if (!file) {
        return false;
    }

    auto channels = 0;
    auto freq = 0;
    auto samples = 0;
    auto acm = SafeAlloc::MakeUnique<CACMUnpacker>(const_cast<uint8*>(file.GetBuf()), static_cast<int>(file.GetSize()), channels, freq, samples);
    const auto buf_size = samples * 2;

    sound->OriginalFormat = AppAudio::AUDIO_FORMAT_S16;
    sound->OriginalChannels = is_music ? 2 : 1;
    sound->OriginalRate = 22050;
    sound->BaseBuf.resize(buf_size);
    sound->BaseBufLen = sound->BaseBuf.size();

    auto* buf = reinterpret_cast<uint16*>(sound->BaseBuf.data());
    const auto dec_data = acm->readAndDecompress(buf, buf_size);

    if (dec_data != buf_size) {
        WriteLog("Decode Acm error");
        return false;
    }

    return ConvertData(sound);
}

auto SoundManager::LoadOgg(Sound* sound, string_view fname) -> bool
{
    STACK_TRACE_ENTRY();

    auto file = _resources.ReadFile(fname);

    if (!file) {
        return false;
    }

    ov_callbacks callbacks;

    callbacks.read_func = [](void* ptr, size_t size, size_t count, void* datasource) -> size_t {
        auto* file2 = static_cast<File*>(datasource);
        const auto bytes_read = std::min(file2->GetSize() - file2->GetCurPos(), size * count);

        if (bytes_read > 0) {
            file2->CopyData(ptr, bytes_read);
        }

        return bytes_read;
    };

    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int whence) -> int {
        auto* file2 = static_cast<File*>(datasource);

        switch (whence) {
        case SEEK_SET:
            file2->SetCurPos(static_cast<uint>(offset));
            break;
        case SEEK_CUR:
            if (offset >= 0) {
                file2->GoForward(static_cast<uint>(offset));
            }
            else {
                file2->GoBack(static_cast<uint>(-offset));
            }
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
        const auto* file2 = static_cast<File*>(datasource);
        delete file2;
        return 0;
    };

    callbacks.tell_func = [](void* datasource) -> long {
        const auto* file2 = static_cast<File*>(datasource);
        return static_cast<long>(file2->GetCurPos());
    };

    sound->OggStream = unique_del_ptr<OggVorbis_File>(SafeAlloc::MakeRaw<OggVorbis_File>(), [](auto* vf) {
        ov_clear(vf);
        delete vf;
    });

    const auto error = ov_open_callbacks(SafeAlloc::MakeRaw<File>(std::move(file)), sound->OggStream.get(), nullptr, 0, callbacks);

    if (error != 0) {
        WriteLog("Open OGG file '{}' fail, error:", fname);
        switch (error) {
        case OV_EREAD:
            WriteLog("A read from media returned an error");
            break;
        case OV_ENOTVORBIS:
            WriteLog("Bitstream does not contain any Vorbis data");
            break;
        case OV_EVERSION:
            WriteLog("Vorbis version mismatch");
            break;
        case OV_EBADHEADER:
            WriteLog("Invalid Vorbis bitstream header");
            break;
        case OV_EFAULT:
            WriteLog("Internal logic fault; indicates a bug or heap/stack corruption");
            break;
        default:
            WriteLog("Unknown error code {}", error);
            break;
        }
        return false;
    }

    const auto* vi = ov_info(sound->OggStream.get(), -1);
    RUNTIME_ASSERT(vi != nullptr);

    sound->OriginalFormat = AppAudio::AUDIO_FORMAT_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = static_cast<int>(vi->rate);
    sound->BaseBuf.resize(_streamingPortion);
    sound->BaseBufLen = _streamingPortion;

    int result;
    uint decoded = 0;

    while (true) {
        auto* buf = reinterpret_cast<char*>(sound->BaseBuf.data());
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
    STACK_TRACE_ENTRY();

    long result;
    uint decoded = 0;

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

    if (result < 0 || decoded == 0) {
        return false;
    }

    sound->BaseBufLen = decoded;

    return ConvertData(sound);
}

auto SoundManager::ConvertData(Sound* sound) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

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
    STACK_TRACE_ENTRY();

    if (!_isActive || _settings.SoundVolume == 0) {
        return true;
    }

    // Make 'NAME'
    const string sound_name = strex(name).eraseFileExtension().lower();

    // Find base
    const auto it = sound_names.find(sound_name);

    if (it != sound_names.end()) {
        return Load(it->second, false, time_duration_t::zero);
    }

    // Check random pattern 'NAME_X'
    uint count = 0;

    while (sound_names.find(strex("{}_{}", sound_name, count + 1).str()) != sound_names.end()) {
        count++;
    }

    if (count != 0u) {
        return Load(sound_names.find(strex("{}_{}", sound_name, GenericUtils::Random(1u, count)).str())->second, false, time_duration_t::zero);
    }

    return false;
}

auto SoundManager::PlayMusic(string_view fname, time_duration_t repeat_time) -> bool
{
    STACK_TRACE_ENTRY();

    if (!_isActive) {
        return true;
    }

    StopMusic();

    return Load(fname, true, repeat_time);
}

void SoundManager::StopSounds()
{
    STACK_TRACE_ENTRY();

    if (!_isActive) {
        return;
    }

    App->Audio.LockDevice();
    _playingSounds.erase(std::remove_if(_playingSounds.begin(), _playingSounds.end(), [](auto&& s) { return !s->IsMusic; }), _playingSounds.end());
    App->Audio.UnlockDevice();
}

void SoundManager::StopMusic()
{
    STACK_TRACE_ENTRY();

    if (!_isActive) {
        return;
    }

    App->Audio.LockDevice();
    _playingSounds.erase(std::remove_if(_playingSounds.begin(), _playingSounds.end(), [](auto&& s) { return s->IsMusic; }), _playingSounds.end());
    App->Audio.UnlockDevice();
}
