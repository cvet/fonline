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

#include "acmstrm.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

FO_BEGIN_NAMESPACE();

struct SoundManager::Sound
{
    vector<uint8> BaseBuf {};
    size_t BaseBufLen {};
    vector<uint8> ConvertedBuf {};
    size_t ConvertedBufCur {};
    int32 OriginalFormat {};
    int32 OriginalChannels {};
    int32 OriginalRate {};
    bool IsMusic {};
    nanotime NextPlayTime {};
    timespan RepeatTime {};
    unique_del_ptr<OggVorbis_File> OggStream {};
};

static constexpr auto MakeUInt(uint8 ch0, uint8 ch1, uint8 ch2, uint8 ch3) -> uint32
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

SoundManager::SoundManager(AudioSettings& settings, FileSystem& resources) :
    _settings {&settings},
    _resources {&resources}
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(OV_CALLBACKS_DEFAULT);
    ignore_unused(OV_CALLBACKS_NOCLOSE);
    ignore_unused(OV_CALLBACKS_STREAMONLY);
    ignore_unused(OV_CALLBACKS_STREAMONLY_NOCLOSE);

    if (!App->Audio.IsEnabled()) {
        return;
    }
    if (_settings->DisableAudio) {
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
    FO_STACK_TRACE_ENTRY();

    if (_isActive) {
        App->Audio.SetSource(nullptr);

        App->Audio.LockDevice();
        _playingSounds.clear();
        App->Audio.UnlockDevice();
    }
}

void SoundManager::ProcessSounds(uint8 silence, span<uint8> output)
{
    FO_STACK_TRACE_ENTRY();

    if (output.size() > _outputBuf.size()) {
        _outputBuf.resize(output.size());
    }

    for (auto it = _playingSounds.begin(); it != _playingSounds.end();) {
        auto& sound = *it;

        if (ProcessSound(sound.get(), silence, {_outputBuf.data(), output.size()})) {
            const auto volume = sound->IsMusic ? _settings->MusicVolume : _settings->SoundVolume;
            App->Audio.MixAudio(output.data(), _outputBuf.data(), output.size(), numeric_cast<int32>(volume));
            ++it;
        }
        else {
            it = _playingSounds.erase(it);
        }
    }
}

auto SoundManager::ProcessSound(Sound* sound, uint8 silence, span<uint8> output) -> bool
{
    FO_STACK_TRACE_ENTRY();

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
            sound->NextPlayTime = nanotime::now() + (sound->RepeatTime > std::chrono::milliseconds {1} ? sound->RepeatTime : timespan::zero);
        }

        if (nanotime::now() >= sound->NextPlayTime) {
            // Set buffer to beginning
            sound->ConvertedBufCur = 0;

            if (sound->OggStream) {
                ov_raw_seek(sound->OggStream.get(), 0);
            }

            // Drop timer
            sound->NextPlayTime = nanotime::zero;

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

auto SoundManager::Load(string_view fname, bool is_music, timespan repeat_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto fixed_fname = string(fname);
    string ext = strex(fname).get_file_extension();

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
    FO_STACK_TRACE_ENTRY();

    const auto file = _resources->ReadFile(fname);

    if (!file) {
        return false;
    }

    auto reader = file.GetReader();

    auto dw_buf = reader.GetLEUInt32();

    if (dw_buf != MakeUInt('R', 'I', 'F', 'F')) {
        WriteLog("'RIFF' not found");
        return false;
    }

    reader.GoForward(4);

    dw_buf = reader.GetLEUInt32();

    if (dw_buf != MakeUInt('W', 'A', 'V', 'E')) {
        WriteLog("'WAVE' not found");
        return false;
    }

    dw_buf = reader.GetLEUInt32();

    if (dw_buf != MakeUInt('f', 'm', 't', ' ')) {
        WriteLog("'fmt ' not found");
        return false;
    }

    dw_buf = reader.GetLEUInt32();

    if (dw_buf == 0) {
        WriteLog("Unknown format");
        return false;
    }

    struct WaveFormatEx
    {
        uint16 WFormatTag; // Integer identifier of the format
        uint16 NChannels; // Number of audio channels
        uint32 NSamplesPerSec; // Audio sample rate
        uint32 NAvgBytesPerSec; // Bytes per second (possibly approximate)
        uint16 NBlockAlign; // Size in bytes of a sample block (all channels)
        uint16 WBitsPerSample; // Size in bits of a single per-channel sample
        uint16 CbSize; // Bytes of extra data appended to this struct
    } waveformatex {};

    reader.CopyData(&waveformatex, 16);

    if (waveformatex.WFormatTag != 1) {
        WriteLog("Compressed files not supported");
        return false;
    }

    reader.GoForward(dw_buf - 16);

    dw_buf = reader.GetLEUInt32();

    if (dw_buf == MakeUInt('f', 'a', 'c', 't')) {
        dw_buf = reader.GetLEUInt32();
        reader.GoForward(dw_buf);
        dw_buf = reader.GetLEUInt32();
    }

    if (dw_buf != MakeUInt('d', 'a', 't', 'a')) {
        WriteLog("Unknown format2");
        return false;
    }

    dw_buf = reader.GetLEUInt32();
    sound->BaseBuf.resize(dw_buf);
    sound->BaseBufLen = dw_buf;

    // Check format
    sound->OriginalChannels = numeric_cast<int32>(waveformatex.NChannels);
    sound->OriginalRate = numeric_cast<int32>(waveformatex.NSamplesPerSec);

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
    reader.CopyData(sound->BaseBuf.data(), sound->BaseBufLen);

    return ConvertData(sound);
}

auto SoundManager::LoadAcm(Sound* sound, string_view fname, bool is_music) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto file = _resources->ReadFile(fname);

    if (!file) {
        return false;
    }

    auto channels = 0;
    auto freq = 0;
    auto samples = 0;
    auto acm = SafeAlloc::MakeUnique<CACMUnpacker>(const_cast<uint8*>(file.GetBuf()), numeric_cast<int32>(file.GetSize()), channels, freq, samples);
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
    FO_STACK_TRACE_ENTRY();

    auto file = _resources->ReadFile(fname);

    if (!file) {
        return false;
    }

    struct OggFileContext
    {
        explicit OggFileContext(File&& file) :
            Holder {std::move(file)},
            Reader {Holder.GetReader()}
        {
        }
        File Holder;
        FileReader Reader;
    };

    ov_callbacks callbacks;

    callbacks.read_func = [](void* ptr, size_t size, size_t count, void* datasource) -> size_t {
        auto* file_context = static_cast<OggFileContext*>(datasource);
        const auto bytes_read = std::min(file_context->Reader.GetSize() - file_context->Reader.GetCurPos(), size * count);

        if (bytes_read > 0) {
            file_context->Reader.CopyData(ptr, bytes_read);
        }

        return bytes_read;
    };

    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int32 whence) -> int32 {
        auto* file_context = static_cast<OggFileContext*>(datasource);

        switch (whence) {
        case SEEK_SET:
            file_context->Reader.SetCurPos(numeric_cast<size_t>(offset));
            break;
        case SEEK_CUR:
            if (offset >= 0) {
                file_context->Reader.GoForward(numeric_cast<size_t>(offset));
            }
            else {
                file_context->Reader.GoBack(numeric_cast<size_t>(-offset));
            }
            break;
        case SEEK_END:
            file_context->Reader.SetCurPos(file_context->Reader.GetSize() - numeric_cast<size_t>(offset));
            break;
        default:
            return -1;
        }

        return 0;
    };

    callbacks.close_func = [](void* datasource) -> int32 {
        const auto* file_context = static_cast<OggFileContext*>(datasource);
        delete file_context;
        return 0;
    };

    callbacks.tell_func = [](void* datasource) -> long {
        const auto* file_context = static_cast<OggFileContext*>(datasource);
        return numeric_cast<long>(file_context->Reader.GetCurPos());
    };

    sound->OggStream = unique_del_ptr<OggVorbis_File>(SafeAlloc::MakeRaw<OggVorbis_File>(), [](auto* vf) {
        ov_clear(vf);
        delete vf;
    });

    auto file_context = SafeAlloc::MakeUnique<OggFileContext>(std::move(file));
    const auto error = ov_open_callbacks(file_context.release(), sound->OggStream.get(), nullptr, 0, callbacks);

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
    FO_RUNTIME_ASSERT(vi != nullptr);

    sound->OriginalFormat = AppAudio::AUDIO_FORMAT_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = numeric_cast<int32>(vi->rate);
    sound->BaseBuf.resize(_streamingPortion);
    sound->BaseBufLen = _streamingPortion;

    int32 result;
    int32 decoded = 0;

    while (true) {
        auto* buf = reinterpret_cast<char*>(sound->BaseBuf.data());
        result = numeric_cast<int32>(ov_read(sound->OggStream.get(), buf + decoded, numeric_cast<int32>(_streamingPortion - decoded), 0, 2, 1, nullptr));

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
    FO_STACK_TRACE_ENTRY();

    long result;
    int32 decoded = 0;

    while (true) {
        auto* buf = reinterpret_cast<char*>(&sound->BaseBuf[decoded]);
        result = ov_read(sound->OggStream.get(), buf, numeric_cast<int32>(_streamingPortion - decoded), 0, 2, 1, nullptr);

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
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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
    FO_STACK_TRACE_ENTRY();

    if (!_isActive || _settings->SoundVolume == 0) {
        return true;
    }

    // Make 'NAME'
    const string sound_name = strex(name).erase_file_extension().lower();

    // Find base
    const auto it = sound_names.find(sound_name);

    if (it != sound_names.end()) {
        return Load(it->second, false, timespan::zero);
    }

    // Check random pattern 'NAME_X'
    int32 count = 0;

    while (sound_names.find(strex("{}_{}", sound_name, count + 1).str()) != sound_names.end()) {
        count++;
    }

    if (count != 0u) {
        return Load(sound_names.find(strex("{}_{}", sound_name, GenericUtils::Random(1u, count)).str())->second, false, timespan::zero);
    }

    return false;
}

auto SoundManager::PlayMusic(string_view fname, timespan repeat_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive) {
        return true;
    }

    StopMusic();

    return Load(fname, true, repeat_time);
}

void SoundManager::StopSounds()
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive) {
        return;
    }

    App->Audio.LockDevice();
    std::erase_if(_playingSounds, [](auto&& s) { return !s->IsMusic; });
    App->Audio.UnlockDevice();
}

void SoundManager::StopMusic()
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive) {
        return;
    }

    App->Audio.LockDevice();
    std::erase_if(_playingSounds, [](auto&& s) { return s->IsMusic; });
    App->Audio.UnlockDevice();
}

FO_END_NAMESPACE();
