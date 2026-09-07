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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "AudioManager.h"
#include "Application.h"
#include "FileSystem.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

FO_BEGIN_NAMESPACE

struct AudioManager::Sound
{
    vector<uint8_t> BaseBuf {};
    size_t BaseBufLen {};
    vector<uint8_t> ConvertedBuf {};
    size_t ConvertedBufCur {};
    int32_t OriginalChannels {};
    int32_t OriginalRate {};
    // Fixed for the life of the sound: a one-shot effect ends long before the listener moves far enough
    // for a recomputation to be audible, and the pan is baked into the converted stereo buffer anyway
    float32_t Attenuation {1.0f};
    float32_t Pan {};
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

AudioManager::AudioManager(ptr<AudioSettings> settings, ptr<FileSystem> resources, ptr<IAppAudio> audio) :
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

AudioManager::~AudioManager()
{
    FO_STACK_TRACE_ENTRY();

    if (_isActive) {
        _audio->SetSource(nullptr);

        _audio->LockDevice();
        _playingSounds.clear();
        _audio->UnlockDevice();
    }
}

void AudioManager::ProcessSounds(uint8_t silence, span<uint8_t> output)
{
    FO_STACK_TRACE_ENTRY();

    if (output.size() > _outputBuf.size()) {
        _outputBuf.resize(output.size());
    }

    for (auto it = _playingSounds.begin(); it != _playingSounds.end();) {
        auto sound = it->as_ptr();
        span<uint8_t> mix_buffer = span<uint8_t> {_outputBuf.data(), output.size()};

        if (ProcessSound(sound, silence, mix_buffer)) {
            int32_t volume = sound->IsMusic ? _settings->MusicVolume : _settings->SoundVolume;
            volume = numeric_cast<int32_t>(std::lround(numeric_cast<float32_t>(volume) * sound->Attenuation));
            _audio->MixAudio(output, mix_buffer, volume);
            ++it;
        }
        else {
            it = _playingSounds.erase(it);
        }
    }
}

auto AudioManager::ProcessSound(ptr<Sound> sound, uint8_t silence, span<uint8_t> output) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Playing
    if (sound->ConvertedBufCur < sound->ConvertedBuf.size()) {
        if (output.size() > sound->ConvertedBuf.size() - sound->ConvertedBufCur) {
            // Flush last part of buffer
            auto offset = sound->ConvertedBuf.size() - sound->ConvertedBufCur;
            auto target = make_ptr(output.data());
            auto source = make_ptr(sound->ConvertedBuf.data()).offset(sound->ConvertedBufCur);
            MemCopy(target, source, offset);
            sound->ConvertedBufCur += offset;

            // Stream new parts
            while (offset < output.size() && sound->OggStream && StreamOgg(sound)) {
                auto write = sound->ConvertedBuf.size() - sound->ConvertedBufCur;

                if (offset + write > output.size()) {
                    write = output.size() - offset;
                }

                auto stream_target = make_ptr(output.data()).offset(offset);
                auto stream_source = make_ptr(sound->ConvertedBuf.data()).offset(sound->ConvertedBufCur);
                MemCopy(stream_target, stream_source, write);
                sound->ConvertedBufCur += write;
                offset += write;
            }

            // Cut off end
            if (offset < output.size()) {
                auto silence_target = make_ptr(output.data()).offset(offset);
                MemFill(silence_target, silence, output.size() - offset);
            }
        }
        else {
            // Copy
            if (!output.empty()) {
                auto target = make_ptr(output.data());
                auto source = make_ptr(sound->ConvertedBuf.data()).offset(sound->ConvertedBufCur);
                MemCopy(target, source, output.size());
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
                auto ogg_stream = sound->OggStream.as_nptr();
                FO_VERIFY_AND_THROW(ogg_stream, "Ogg stream is null");
                ov_raw_seek(ogg_stream.get(), 0);
            }

            // Drop timer
            sound->NextPlayTime = nanotime::zero;

            // Process without silent
            return ProcessSound(sound, silence, output);
        }

        // Give silent
        if (!output.empty()) {
            auto silence_target = make_ptr(output.data());
            MemFill(silence_target, silence, output.size());
        }
        return true;
    }

    // Give silent
    if (!output.empty()) {
        auto silence_target = make_ptr(output.data());
        MemFill(silence_target, silence, output.size());
    }

    return false;
}

auto AudioManager::Load(string_view fname, bool is_music, timespan repeat_time, float32_t attenuation, float32_t pan) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Every audio resource is baked to Ogg Vorbis, so the authored extension names the source format only
    auto file = _resources->ReadFile(fname);

    if (!file) {
        return false;
    }

    auto sound_owner = SafeAlloc::MakeUnique<Sound>();
    auto sound = sound_owner.as_ptr();
    sound->Attenuation = attenuation;
    sound->Pan = pan;

    ov_callbacks callbacks;

    callbacks.read_func = [](void* output_buf, size_t size, size_t count, void* datasource) -> size_t {
        auto file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(file_context, "Missing Ogg file context");
        size_t bytes_read = std::min(file_context->Reader.GetSize() - file_context->Reader.GetCurPos(), size * count);

        if (bytes_read > 0) {
            FO_VERIFY_AND_THROW(output_buf != nullptr, "Ogg read output buffer is null");
            file_context->Reader.ReadBytes(make_span(output_buf, bytes_read));
        }

        return bytes_read;
    };

    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int32_t whence) -> int32_t {
        auto file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(file_context, "Missing Ogg file context");

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
        auto file_context = cast_from_void<OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(file_context, "Missing Ogg file context");
        auto owned_file_context = adopt_unique_ptr(file_context);
        ignore_unused(owned_file_context);
        return 0;
    };

    callbacks.tell_func = [](void* datasource) -> long {
        auto file_context = cast_from_void<const OggFileContext*>(datasource);
        FO_VERIFY_AND_THROW(file_context, "Missing Ogg file context");
        return numeric_cast<long>(file_context->Reader.GetCurPos());
    };

    auto ogg_stream_owner = SafeAlloc::MakeUnique<OggVorbis_File>();
    auto released_ogg_stream = ogg_stream_owner.release();
    sound->OggStream = make_unique_del_ptr(released_ogg_stream, [](OggVorbis_File* raw_vf) noexcept {
        auto vf = make_ptr(raw_vf);
        auto owned_vf = adopt_unique_ptr(vf);
        ov_clear(owned_vf.get());
    });
    auto ogg_stream = sound->OggStream.as_nptr();
    FO_VERIFY_AND_THROW(ogg_stream, "Ogg stream is null");

    FileReader reader = file.GetReader();
    auto file_context = SafeAlloc::MakeUnique<OggFileContext>(OggFileContext {std::move(file), std::move(reader)});
    int32_t error = ov_open_callbacks(make_nptr(file_context.get()).void_cast(), ogg_stream.get(), nullptr, 0, callbacks);

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

    auto released_file_context = file_context.release();
    ignore_unused(released_file_context);

    auto vi = make_nptr(ov_info(ogg_stream.get(), -1));
    FO_VERIFY_AND_THROW(vi, "Vorbis info is null");

    sound->OriginalChannels = vi->channels;
    sound->OriginalRate = numeric_cast<int32_t>(vi->rate);
    sound->BaseBuf.resize(_streamingPortion);
    sound->BaseBufLen = _streamingPortion;

    int32_t result;
    int32_t decoded = 0;
    span<uint8_t> base_buf = span<uint8_t> {sound->BaseBuf.data(), sound->BaseBuf.size()};

    while (true) {
        auto output = make_ptr(base_buf.data()).offset(numeric_cast<size_t>(decoded)).reinterpret_as<char>();
        int32_t read_size = numeric_cast<int32_t>(_streamingPortion - decoded);
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

    if (!ConvertData(sound)) {
        return false;
    }

    sound->IsMusic = is_music;
    sound->RepeatTime = repeat_time;

    _audio->LockDevice();
    _playingSounds.emplace_back(std::move(sound_owner));
    _audio->UnlockDevice();

    return true;
}

auto AudioManager::StreamOgg(ptr<Sound> sound) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(sound->OggStream, "Sound has no Ogg stream to read");
    auto ogg_stream = sound->OggStream.as_nptr();
    FO_VERIFY_AND_THROW(ogg_stream, "Ogg stream is null");
    long result;
    int32_t decoded = 0;
    span<uint8_t> base_buf = span<uint8_t> {sound->BaseBuf.data(), sound->BaseBuf.size()};

    while (true) {
        auto output = make_ptr(base_buf.data()).offset(numeric_cast<size_t>(decoded)).reinterpret_as<char>();
        int32_t read_size = numeric_cast<int32_t>(_streamingPortion - decoded);
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

auto AudioManager::ConvertData(ptr<Sound> sound) -> bool
{
    FO_STACK_TRACE_ENTRY();

    sound->ConvertedBuf = sound->BaseBuf;
    sound->ConvertedBuf.resize(sound->BaseBufLen);

    if (!_audio->ConvertAudio(AppAudio::AUDIO_FORMAT_S16, sound->OriginalChannels, sound->OriginalRate, sound->ConvertedBuf)) {
        return false;
    }

    // Safe after conversion because the mixing format is the engine's own S16 stereo, whatever the device runs
    if (sound->Pan != 0.0f) {
        ApplyPan(sound->ConvertedBuf, sound->Pan);
    }

    sound->ConvertedBufCur = 0;

    return true;
}

void AudioManager::ApplyPan(vector<uint8_t>& buf, float32_t pan)
{
    FO_STACK_TRACE_ENTRY();

    // A stream can decode to nothing, and an empty buffer has no data pointer to walk
    if (buf.empty()) {
        return;
    }

    // A balance rather than constant power: lifting the near channel above unity would clip a loud sample,
    // which is a worse artefact than the three decibels this gives up at full deflection
    float32_t left_gain = pan > 0.0f ? 1.0f - pan : 1.0f;
    float32_t right_gain = pan < 0.0f ? 1.0f + pan : 1.0f;

    size_t frame_count = buf.size() / (sizeof(int16_t) * 2);
    auto samples = make_ptr(buf.data()).reinterpret_as<int16_t>();

    for (size_t i = 0; i < frame_count; i++) {
        *samples.offset(i * 2) = numeric_cast<int16_t>(std::lround(numeric_cast<float32_t>(*samples.offset(i * 2)) * left_gain));
        *samples.offset(i * 2 + 1) = numeric_cast<int16_t>(std::lround(numeric_cast<float32_t>(*samples.offset(i * 2 + 1)) * right_gain));
    }
}

void AudioManager::IndexFiles()
{
    FO_STACK_TRACE_ENTRY();

    for (const string& sound_ext : _settings->SoundFileExtensions) {
        for (const auto& file_header : _resources->FilterFiles(sound_ext)) {
            _soundNames.emplace_back(file_header.GetPath());
        }
    }
}

auto AudioManager::PlaySound(string_view name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return PlaySound(name, 1.0f, 0.0f);
}

auto AudioManager::PlaySound(string_view name, float32_t attenuation, float32_t pan) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive || _settings->SoundVolume == 0) {
        return true;
    }

    // Out of earshot: return before the read so a distant event costs neither a file read nor a decode
    if (attenuation <= 0.0f) {
        return true;
    }

    // The name is a resource path the caller already resolved: which file a game concept maps to, and how a
    // set of numbered variants is picked among, is naming convention rather than mixing
    return Load(name, false, timespan::zero, attenuation, pan);
}

auto AudioManager::PlayMusic(string_view fname, timespan repeat_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive) {
        return true;
    }

    StopMusic();

    return Load(fname, true, repeat_time, 1.0f, 0.0f);
}

void AudioManager::StopSounds()
{
    FO_STACK_TRACE_ENTRY();

    if (!_isActive) {
        return;
    }

    _audio->LockDevice();
    std::erase_if(_playingSounds, [](auto&& s) { return !s->IsMusic; });
    _audio->UnlockDevice();
}

void AudioManager::StopMusic()
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
