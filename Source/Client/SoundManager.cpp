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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

struct SoundManager::Sound
{
    vector<uint8_t> BaseBuf {};
    size_t BaseBufLen {};
    vector<uint8_t> ConvertedBuf {};
    size_t ConvertedBufCur {};
    int32_t OriginalFormat {};
    int32_t OriginalChannels {};
    int32_t OriginalRate {};
    bool IsMusic {};
    nanotime NextPlayTime {};
    timespan RepeatTime {};
    unique_del_nptr<OggVorbis_File> OggStream {};
};

struct OggFileContext
{
    File Holder;
    FileReader Reader;
};

static constexpr auto MakeUInt(uint8_t ch0, uint8_t ch1, uint8_t ch2, uint8_t ch3) -> uint32_t
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

SoundManager::SoundManager(ptr<AudioSettings> settings, ptr<FileSystem> resources, ptr<IAppAudio> audio) :
    _settings {settings},
    _resources {resources},
    _audio {audio}
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(OV_CALLBACKS_DEFAULT);
    ignore_unused(OV_CALLBACKS_NOCLOSE);
    ignore_unused(OV_CALLBACKS_STREAMONLY);
    ignore_unused(OV_CALLBACKS_STREAMONLY_NOCLOSE);

    if (!_audio->IsEnabled()) {
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

    _audio->SetSource([this](uint8_t silence, span<uint8_t> output) FO_DEFERRED { ProcessSounds(silence, output); });
    _isActive = true;
}

SoundManager::~SoundManager()
{
    FO_STACK_TRACE_ENTRY();

    if (_isActive) {
        _audio->SetSource(nullptr);

        _audio->LockDevice();
        _playingSounds.clear();
        _audio->UnlockDevice();
    }
}

void SoundManager::ProcessSounds(uint8_t silence, span<uint8_t> output)
{
    FO_STACK_TRACE_ENTRY();

    if (output.size() > _outputBuf.size()) {
        _outputBuf.resize(output.size());
    }

    for (auto it = _playingSounds.begin(); it != _playingSounds.end();) {
        auto sound = it->as_ptr();
        span<uint8_t> mix_buffer = span<uint8_t> {_outputBuf.data(), output.size()};

        if (ProcessSound(sound, silence, mix_buffer)) {
            const auto volume = sound->IsMusic ? _settings->MusicVolume : _settings->SoundVolume;
            _audio->MixAudio(output, mix_buffer, numeric_cast<int32_t>(volume));
            ++it;
        }
        else {
            it = _playingSounds.erase(it);
        }
    }
}

auto SoundManager::ProcessSound(ptr<Sound> sound, uint8_t silence, span<uint8_t> output) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Playing
    if (sound->ConvertedBufCur < sound->ConvertedBuf.size()) {
        if (output.size() > sound->ConvertedBuf.size() - sound->ConvertedBufCur) {
            // Flush last part of buffer
            auto offset = sound->ConvertedBuf.size() - sound->ConvertedBufCur;
            auto target = ptr<uint8_t> {output.data()};
            auto source = ptr<const uint8_t> {sound->ConvertedBuf.data()}.offset(sound->ConvertedBufCur);
            MemCopy(target.get(), source.get(), offset);
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while (offset < output.size() && sound->OggStream && StreamOgg(sound)) {
                auto write = sound->ConvertedBuf.size() - sound->ConvertedBufCur;

                if (offset + write > output.size()) {
                    write = output.size() - offset;
                }

                auto stream_target = ptr<uint8_t> {output.data()}.offset(offset);
                auto stream_source = ptr<const uint8_t> {sound->ConvertedBuf.data()}.offset(sound->ConvertedBufCur);
                MemCopy(stream_target.get(), stream_source.get(), write);
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if (offset < output.size()) {
                auto silence_target = ptr<uint8_t> {output.data()}.offset(offset);
                MemFill(silence_target.get(), silence, output.size() - offset);
            }
        }
        else {
            // Copy
            if (!output.empty()) {
                auto target = ptr<uint8_t> {output.data()};
                auto source = ptr<const uint8_t> {sound->ConvertedBuf.data()}.offset(sound->ConvertedBufCur);
                MemCopy(target.get(), source.get(), output.size());
            }
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
                auto ogg_stream = sound->OggStream.as_ptr();
                ov_raw_seek(ogg_stream.get(), 0);
            }

            // Drop timer
            sound->NextPlayTime = nanotime::zero;

            // Process without silent
            return ProcessSound(sound, silence, output);
        }

        // Give silent
        if (!output.empty()) {
            auto silence_target = ptr<uint8_t> {output.data()};
            MemFill(silence_target.get(), silence, output.size());
        }
        return true;
    }

    // Give silent
    if (!output.empty()) {
        auto silence_target = ptr<uint8_t> {output.data()};
        MemFill(silence_target.get(), silence, output.size());
    }

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

    unique_ptr<Sound> sound = SafeAlloc::MakeUnique<Sound>();

    if (ext == "wav" && !LoadWav(sound.as_ptr(), fixed_fname)) {
        return false;
    }
    if (ext == "acm" && !LoadAcm(sound.as_ptr(), fixed_fname, is_music)) {
        return false;
    }
    if (ext == "ogg" && !LoadOgg(sound.as_ptr(), fixed_fname)) {
        return false;
    }

    sound->IsMusic = is_music;
    sound->RepeatTime = repeat_time;

    _audio->LockDevice();
    _playingSounds.emplace_back(std::move(sound));
    _audio->UnlockDevice();

    return true;
}

auto SoundManager::LoadWav(ptr<Sound> sound, string_view fname) -> bool
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
        uint16_t WFormatTag; // Integer identifier of the format
        uint16_t NChannels; // Number of audio channels
        uint32_t NSamplesPerSec; // Audio sample rate
        uint32_t NAvgBytesPerSec; // Bytes per second (possibly approximate)
        uint16_t NBlockAlign; // Size in bytes of a sample block (all channels)
        uint16_t WBitsPerSample; // Size in bits of a single per-channel sample
        uint16_t CbSize; // Bytes of extra data appended to this struct
    } waveformatex {};

    constexpr size_t wave_format_base_size = 16;

    if (dw_buf < wave_format_base_size) {
        WriteLog("Unknown format");
        return false;
    }

    span<uint8_t> wave_format_bytes = span<uint8_t> {ptr<WaveFormatEx> {&waveformatex}.reinterpret_as<uint8_t>().get(), sizeof(WaveFormatEx)}.first(wave_format_base_size);
    reader.ReadBytes(wave_format_bytes);

    if (waveformatex.WFormatTag != 1) {
        WriteLog("Compressed files not supported");
        return false;
    }

    reader.GoForward(numeric_cast<size_t>(dw_buf) - wave_format_base_size);

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
    sound->OriginalChannels = numeric_cast<int32_t>(waveformatex.NChannels);
    sound->OriginalRate = numeric_cast<int32_t>(waveformatex.NSamplesPerSec);

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
    span<uint8_t> base_buf = span<uint8_t> {sound->BaseBuf.data(), sound->BaseBufLen};
    reader.ReadBytes(base_buf);

    return ConvertData(sound);
}

auto SoundManager::LoadAcm(ptr<Sound> sound, string_view fname, bool is_music) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto file = _resources->ReadFile(fname);

    if (!file) {
        return false;
    }

    vector<uint8_t> acm_data = file.GetData();

    auto channels = 0;
    auto freq = 0;
    auto samples = 0;
    nptr<uint8_t> acm_data_ptr = acm_data.data();
    FO_VERIFY_AND_THROW(acm_data.empty() || !!acm_data_ptr, "Non-empty ACM data has a null pointer");
    unique_ptr<CACMUnpacker> acm = SafeAlloc::MakeUnique<CACMUnpacker>(acm_data_ptr.get(), numeric_cast<int32_t>(acm_data.size()), channels, freq, samples);
    const auto buf_size = samples * 2;

    sound->OriginalFormat = AppAudio::AUDIO_FORMAT_S16;
    sound->OriginalChannels = is_music ? 2 : 1;
    sound->OriginalRate = 22050;
    sound->BaseBuf.resize(buf_size);
    sound->BaseBufLen = sound->BaseBuf.size();

    span<uint8_t> base_buf = span<uint8_t> {sound->BaseBuf.data(), sound->BaseBuf.size()};
    FO_STRONG_ASSERT(!base_buf.empty(), "Sound data is empty");
    FO_STRONG_ASSERT(base_buf.size() % sizeof(uint16_t) == 0, "Sound data size is not 16-bit aligned");
    ptr<uint8_t> base_buf_bytes = base_buf.data();
    auto buf = base_buf_bytes.reinterpret_as<uint16_t>();
    const auto dec_data = acm->readAndDecompress(buf.get(), buf_size);

    if (dec_data != buf_size) {
        WriteLog("Decode Acm error");
        return false;
    }

    return ConvertData(sound);
}

auto SoundManager::LoadOgg(ptr<Sound> sound, string_view fname) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto file = _resources->ReadFile(fname);

    if (!file) {
        return false;
    }

    ov_callbacks callbacks;

    callbacks.read_func = [](void* output_buf, size_t size, size_t count, void* datasource) -> size_t {
        nptr<OggFileContext> nullable_file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(nullable_file_context, "Missing Ogg file context");
        auto file_context = nullable_file_context.as_ptr();
        const size_t bytes_read = std::min(file_context->Reader.GetSize() - file_context->Reader.GetCurPos(), size * count);

        if (bytes_read > 0) {
            FO_VERIFY_AND_THROW(output_buf != nullptr, "Ogg read output buffer is null");
            file_context->Reader.CopyData(make_span(output_buf, bytes_read));
        }

        return bytes_read;
    };

    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int32_t whence) -> int32_t {
        nptr<OggFileContext> nullable_file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(nullable_file_context, "Missing Ogg file context");
        auto file_context = nullable_file_context.as_ptr();

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

    callbacks.close_func = [](void* datasource) -> int32_t {
        nptr<OggFileContext> nullable_file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(nullable_file_context, "Missing Ogg file context");
        auto file_context = nullable_file_context.as_ptr();
        auto owned_file_context = adopt_unique_ptr(file_context);
        ignore_unused(owned_file_context);
        return 0;
    };

    callbacks.tell_func = [](void* datasource) -> long {
        nptr<const OggFileContext> nullable_file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(nullable_file_context, "Missing Ogg file context");
        auto file_context = nullable_file_context.as_ptr();
        return numeric_cast<long>(file_context->Reader.GetCurPos());
    };

    unique_ptr<OggVorbis_File> ogg_stream_owner = SafeAlloc::MakeUnique<OggVorbis_File>();
    ptr<OggVorbis_File> released_ogg_stream = std::move(ogg_stream_owner).release();
    sound->OggStream = make_unique_del_ptr(released_ogg_stream, [](OggVorbis_File* raw_vf) noexcept {
        ptr<OggVorbis_File> vf = raw_vf;
        auto owned_vf = adopt_unique_ptr(vf);
        ov_clear(owned_vf.get());
    });
    auto ogg_stream = sound->OggStream.as_ptr();

    FileReader reader = file.GetReader();
    unique_ptr<OggFileContext> file_context = SafeAlloc::MakeUnique<OggFileContext>(OggFileContext {std::move(file), std::move(reader)});
    auto file_context_ptr = file_context.as_ptr();
    ptr<void> file_context_userdata = cast_to_void(file_context_ptr.get());
    const auto error = ov_open_callbacks(file_context_userdata.get(), ogg_stream.get(), nullptr, 0, callbacks);

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

    ptr<OggFileContext> released_file_context = std::move(file_context).release();
    ignore_unused(released_file_context);

    nptr<const vorbis_info> vi = ov_info(ogg_stream.get(), -1);
    FO_VERIFY_AND_THROW(vi, "Vorbis info is null");

    sound->OriginalFormat = AppAudio::AUDIO_FORMAT_S16;
    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = numeric_cast<int32_t>(vi->rate);
    sound->BaseBuf.resize(_streamingPortion);
    sound->BaseBufLen = _streamingPortion;

    int32_t result;
    int32_t decoded = 0;
    span<uint8_t> base_buf = span<uint8_t> {sound->BaseBuf.data(), sound->BaseBuf.size()};

    while (true) {
        auto output = (ptr<uint8_t> {base_buf.data()}.offset(numeric_cast<size_t>(decoded))).reinterpret_as<char>();
        const int32_t read_size = numeric_cast<int32_t>(_streamingPortion - decoded);
        result = numeric_cast<int32_t>(ov_read(ogg_stream.get(), output.get(), read_size, 0, 2, 1, nullptr));

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
        sound->OggStream.reset();
    }

    return ConvertData(sound);
}

auto SoundManager::StreamOgg(ptr<Sound> sound) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(sound->OggStream, "Sound has no Ogg stream to read");
    auto ogg_stream = sound->OggStream.as_ptr();
    long result;
    int32_t decoded = 0;
    span<uint8_t> base_buf = span<uint8_t> {sound->BaseBuf.data(), sound->BaseBuf.size()};

    while (true) {
        auto output = (ptr<uint8_t> {base_buf.data()}.offset(numeric_cast<size_t>(decoded))).reinterpret_as<char>();
        const int32_t read_size = numeric_cast<int32_t>(_streamingPortion - decoded);
        result = ov_read(ogg_stream.get(), output.get(), read_size, 0, 2, 1, nullptr);

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

auto SoundManager::ConvertData(ptr<Sound> sound) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    sound->ConvertedBuf = sound->BaseBuf;
    sound->ConvertedBuf.resize(sound->BaseBufLen);

    if (!_audio->ConvertAudio(sound->OriginalFormat, sound->OriginalChannels, sound->OriginalRate, sound->ConvertedBuf)) {
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
    int32_t count = 0;

    while (sound_names.find(strex("{}_{}", sound_name, count + 1).str()) != sound_names.end()) {
        count++;
    }

    if (count != 0u) {
        const int32_t random_index = std::uniform_int_distribution<int32_t> {1, count}(_randomGenerator);
        return Load(sound_names.find(strex("{}_{}", sound_name, random_index).str())->second, false, timespan::zero);
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

    _audio->LockDevice();
    std::erase_if(_playingSounds, [](auto&& s) { return !s->IsMusic; });
    _audio->UnlockDevice();
}

void SoundManager::StopMusic()
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive) {
        return;
    }

    _audio->LockDevice();
    std::erase_if(_playingSounds, [](auto&& s) { return s->IsMusic; });
    _audio->UnlockDevice();
}

FO_END_NAMESPACE
