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

#include "AudioBaker.h"
#include "FileSystem.h"
#include "Settings.h"

#include "ogg/ogg.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisenc.h"
#include "vorbis/vorbisfile.h"

FO_BEGIN_NAMESPACE

struct MemoryOggSource
{
    const_span<uint8_t> Data {};
    size_t Pos {};
};

struct WavFormat
{
    uint16_t Tag {};
    int32_t Channels {};
    int32_t SampleRate {};
    int32_t BlockAlign {};
    int32_t BitsPerSample {};
};

static constexpr uint16_t WAVE_FORMAT_PCM = 1;
static constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT = 3;
static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

static void VerifyVorbisStream(string_view fname, const_span<uint8_t> data);
static auto ReadFourcc(FileReader& reader) -> string;
static auto DecodeWavSample(const_span<uint8_t> bytes, const WavFormat& format) -> int16_t;
static void AppendOggPage(vector<uint8_t>& output, const ogg_page& page);

AudioBaker::AudioBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME)
{
    FO_STACK_TRACE_ENTRY();

    AddLoader(std::bind(&AudioBaker::LoadWav, this, std::placeholders::_1, std::placeholders::_2), {"wav"});
}

void AudioBaker::AddLoader(const LoadFunc& loader, const vector<string>& file_extensions)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& ext : file_extensions) {
        _fileLoaders[ext] = loader;
    }
}

void AudioBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    float32_t quality = _context->Settings->AudioVorbisQuality;
    FO_VERIFY_AND_THROW(quality >= -0.1f && quality <= 1.0f, "Vorbis quality must stay within the encoder range -0.1..1.0", quality);

    vector<File> files_to_bake;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            string ext = strex(file_header.GetPath()).get_file_extension();

            if (!IsBakeableExtension(ext)) {
                continue;
            }
            if (_context->BakeChecker && !_context->BakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
                continue;
            }

            files_to_bake.emplace_back(File::Load(file_header));
        }
    }
    else {
        string ext = strex(target_path).get_file_extension();

        if (!IsBakeableExtension(ext)) {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_context->BakeChecker && !_context->BakeChecker(file.GetPath(), file.GetWriteTime())) {
            return;
        }

        files_to_bake.emplace_back(std::move(file));
    }

    if (files_to_bake.empty()) {
        return;
    }

    vector<std::future<BakedAudioInfo>> file_bakings;
    file_bakings.reserve(files_to_bake.size());

    for (size_t file_index = 0; file_index < files_to_bake.size(); file_index++) {
        string task_name = strex("BakeAudio-{}", files_to_bake[file_index].GetPath()).str();
        file_bakings.emplace_back(run_async(GetAsyncMode(), task_name, [this, &files_to_bake, file_index]() FO_DEFERRED -> BakedAudioInfo { return BakeFile(files_to_bake[file_index]); }));
    }

    size_t errors = 0;
    uint64_t encoded = 0;
    uint64_t passthrough = 0;
    uint64_t input_bytes = 0;
    uint64_t output_bytes = 0;

    for (auto& file_baking : file_bakings) {
        try {
            BakedAudioInfo info = file_baking.get();
            encoded += info.Encoded ? 1 : 0;
            passthrough += info.Encoded ? 0 : 1;
            input_bytes += info.InputBytes;
            output_bytes += info.OutputBytes;
        }
        catch (const std::exception& ex) {
            WriteLog("Audio baking error: {}", ex.what());
            errors++;
        }
    }

    if (errors != 0) {
        throw AudioBakerException("Errors during audio baking", errors);
    }

    if (IsBakingReportEnabled()) {
        AddBakingReportCounter("audioEncoded", encoded);
        AddBakingReportCounter("audioPassthrough", passthrough);
        AddBakingReportCounter("audioInputBytes", input_bytes);
        AddBakingReportCounter("audioOutputBytes", output_bytes);
    }
}

auto AudioBaker::IsBakeableExtension(string_view ext) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ext == NATIVE_EXTENSION || _fileLoaders.contains(string(ext));
}

auto AudioBaker::BakeFile(const File& file) const -> BakedAudioInfo
{
    FO_STACK_TRACE_ENTRY();

    string_view path = file.GetPath();
    string ext = strex(path).get_file_extension();
    BakedAudioInfo info;
    info.InputBytes = file.GetDataSpan().size();

    // A source already in the runtime format is never re-encoded: lossy over lossy only loses
    if (ext == NATIVE_EXTENSION) {
        const_span<uint8_t> data = file.GetDataSpan();
        VerifyVorbisStream(path, data);
        _context->WriteData(path, data);
        info.OutputBytes = data.size();
        return info;
    }

    auto loader_it = _fileLoaders.find(ext);
    FO_VERIFY_AND_THROW(loader_it != _fileLoaders.end(), "No audio loader is registered for the file extension", path, ext);

    PcmAudio pcm = loader_it->second(path, file.GetReader());
    vector<uint8_t> vorbis_data = EncodeVorbis(path, pcm);
    _context->WriteData(path, vorbis_data);
    info.Encoded = true;
    info.OutputBytes = vorbis_data.size();
    return info;
}

auto AudioBaker::LoadWav(string_view fname, FileReader reader) const -> PcmAudio
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t riff_header_size = 12;
    constexpr size_t chunk_header_size = 8;
    constexpr size_t fmt_base_size = 16;
    constexpr size_t fmt_extensible_size = 40;

    size_t file_size = reader.GetSize();
    FO_VERIFY_AND_THROW(file_size >= riff_header_size, "WAV file is shorter than the RIFF header", fname, file_size);

    string riff_tag = ReadFourcc(reader);
    FO_VERIFY_AND_THROW(riff_tag == "RIFF", "WAV file does not start with a RIFF header", fname);
    reader.GoForward(4);
    string wave_tag = ReadFourcc(reader);
    FO_VERIFY_AND_THROW(wave_tag == "WAVE", "RIFF file is not a WAVE form", fname);

    optional<WavFormat> format;
    const_span<uint8_t> pcm_data;

    // Chunks come in any order and metadata chunks are common, so the walk skips whatever it does not know
    while (reader.GetCurPos() + chunk_header_size <= file_size) {
        string chunk_id = ReadFourcc(reader);
        size_t chunk_size = reader.GetLEUInt32();
        size_t chunk_start = reader.GetCurPos();

        // A streamed export leaves a placeholder size in the data chunk, so the file end wins over the header
        size_t chunk_avail = std::min(chunk_size, file_size - chunk_start);

        if (chunk_id == "fmt ") {
            FO_VERIFY_AND_THROW(chunk_avail >= fmt_base_size, "WAV format chunk is truncated", fname, chunk_avail);

            WavFormat wav_format;
            wav_format.Tag = reader.GetLEUInt16();
            wav_format.Channels = reader.GetLEUInt16();
            wav_format.SampleRate = numeric_cast<int32_t>(reader.GetLEUInt32());
            reader.GoForward(4);
            wav_format.BlockAlign = reader.GetLEUInt16();
            wav_format.BitsPerSample = reader.GetLEUInt16();

            // The extensible header repeats the real format tag as the first word of its sub-format GUID
            if (wav_format.Tag == WAVE_FORMAT_EXTENSIBLE) {
                FO_VERIFY_AND_THROW(chunk_avail >= fmt_extensible_size, "WAV extensible format chunk is truncated", fname, chunk_avail);
                reader.GoForward(8);
                wav_format.Tag = reader.GetLEUInt16();
            }

            format = wav_format;
        }
        else if (chunk_id == "data") {
            pcm_data = reader.GetCurDataSpan(chunk_avail);
        }

        size_t next_chunk_pos = chunk_start + chunk_avail + (chunk_size % 2);

        if (next_chunk_pos >= file_size) {
            break;
        }

        reader.SetCurPos(next_chunk_pos);
    }

    FO_VERIFY_AND_THROW(format.has_value(), "WAV file has no format chunk", fname);
    FO_VERIFY_AND_THROW(!pcm_data.empty(), "WAV file has no sample data", fname);
    FO_VERIFY_AND_THROW(format->Tag == WAVE_FORMAT_PCM || format->Tag == WAVE_FORMAT_IEEE_FLOAT, "WAV encoding is not PCM or IEEE float", fname, format->Tag);
    FO_VERIFY_AND_THROW(format->Channels > 0, "WAV channel count is invalid", fname, format->Channels);
    FO_VERIFY_AND_THROW(format->SampleRate > 0, "WAV sample rate is invalid", fname, format->SampleRate);

    bool pcm_width_ok = format->Tag == WAVE_FORMAT_PCM && (format->BitsPerSample == 8 || format->BitsPerSample == 16 || format->BitsPerSample == 24 || format->BitsPerSample == 32);
    bool float_width_ok = format->Tag == WAVE_FORMAT_IEEE_FLOAT && format->BitsPerSample == 32;
    FO_VERIFY_AND_THROW(pcm_width_ok || float_width_ok, "WAV sample width is unsupported", fname, format->Tag, format->BitsPerSample);

    size_t bytes_per_sample = numeric_cast<size_t>(format->BitsPerSample / 8);
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(format->BlockAlign) == bytes_per_sample * numeric_cast<size_t>(format->Channels), "WAV block alignment disagrees with the channel count and sample width", fname, format->BlockAlign, format->Channels, format->BitsPerSample);

    size_t frame_count = pcm_data.size() / numeric_cast<size_t>(format->BlockAlign);
    FO_VERIFY_AND_THROW(frame_count > 0, "WAV data chunk holds no whole frame", fname, pcm_data.size());

    PcmAudio pcm;
    pcm.Channels = format->Channels;
    pcm.SampleRate = format->SampleRate;
    pcm.Samples.resize(frame_count * numeric_cast<size_t>(format->Channels));

    for (size_t i = 0; i < pcm.Samples.size(); i++) {
        pcm.Samples[i] = DecodeWavSample(pcm_data.subspan(i * bytes_per_sample, bytes_per_sample), *format);
    }

    return pcm;
}

auto AudioBaker::EncodeVorbis(string_view fname, const PcmAudio& pcm) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pcm.Channels > 0, "Audio has no channels to encode", fname);
    FO_VERIFY_AND_THROW(pcm.SampleRate > 0, "Audio sample rate is invalid", fname, pcm.SampleRate);

    size_t channels = numeric_cast<size_t>(pcm.Channels);
    FO_VERIFY_AND_THROW(pcm.Samples.size() % channels == 0, "Interleaved sample count is not a whole number of frames", fname, pcm.Samples.size(), pcm.Channels);

    float32_t quality = _context->Settings->AudioVorbisQuality;

    vorbis_info info;
    vorbis_info_init(&info);
    auto clear_info = scope_exit([&info]() noexcept { vorbis_info_clear(&info); });
    int32_t encoder_result = vorbis_encode_init_vbr(&info, pcm.Channels, pcm.SampleRate, quality);
    FO_VERIFY_AND_THROW(encoder_result == 0, "Vorbis encoder rejected the audio format", fname, pcm.Channels, pcm.SampleRate, quality, encoder_result);

    vorbis_comment comment;
    vorbis_comment_init(&comment);
    auto clear_comment = scope_exit([&comment]() noexcept { vorbis_comment_clear(&comment); });

    vorbis_dsp_state dsp;
    int32_t dsp_result = vorbis_analysis_init(&dsp, &info);
    FO_VERIFY_AND_THROW(dsp_result == 0, "Vorbis analysis state failed to initialize", fname, dsp_result);
    auto clear_dsp = scope_exit([&dsp]() noexcept { vorbis_dsp_clear(&dsp); });

    vorbis_block block;
    int32_t block_result = vorbis_block_init(&dsp, &block);
    FO_VERIFY_AND_THROW(block_result == 0, "Vorbis block failed to initialize", fname, block_result);
    auto clear_block = scope_exit([&block]() noexcept { vorbis_block_clear(&block); });

    // A fixed serial number keeps the output byte-identical between bakes of the same source
    ogg_stream_state stream;
    int32_t stream_result = ogg_stream_init(&stream, 0);
    FO_VERIFY_AND_THROW(stream_result == 0, "Ogg stream failed to initialize", fname, stream_result);
    auto clear_stream = scope_exit([&stream]() noexcept { ogg_stream_clear(&stream); });

    vector<uint8_t> output;
    ogg_page page;

    ogg_packet header_packet;
    ogg_packet comment_packet;
    ogg_packet codebook_packet;
    int32_t header_result = vorbis_analysis_headerout(&dsp, &comment, &header_packet, &comment_packet, &codebook_packet);
    FO_VERIFY_AND_THROW(header_result == 0, "Vorbis header packets failed to generate", fname, header_result);
    ogg_stream_packetin(&stream, &header_packet);
    ogg_stream_packetin(&stream, &comment_packet);
    ogg_stream_packetin(&stream, &codebook_packet);

    // The headers are flushed onto their own pages so the audio data starts page-aligned, as decoders expect
    while (ogg_stream_flush(&stream, &page) != 0) {
        AppendOggPage(output, page);
    }

    auto drain_encoder = [&dsp, &block, &stream, &page, &output]() {
        while (vorbis_analysis_blockout(&dsp, &block) == 1) {
            vorbis_analysis(&block, nullptr);
            vorbis_bitrate_addblock(&block);

            ogg_packet packet;

            while (vorbis_bitrate_flushpacket(&dsp, &packet) == 1) {
                ogg_stream_packetin(&stream, &packet);

                while (ogg_stream_pageout(&stream, &page) != 0) {
                    AppendOggPage(output, page);
                }
            }
        }
    };

    constexpr size_t frames_per_block = 1024;
    size_t frame_count = pcm.Samples.size() / channels;

    for (size_t frame = 0; frame < frame_count; frame += frames_per_block) {
        size_t block_frames = std::min(frames_per_block, frame_count - frame);
        auto channel_buffers = make_ptr(vorbis_analysis_buffer(&dsp, numeric_cast<int32_t>(block_frames)));

        for (size_t channel = 0; channel < channels; channel++) {
            span<float32_t> channel_buffer = make_span(make_ptr(channel_buffers[channel]), block_frames);

            for (size_t i = 0; i < block_frames; i++) {
                channel_buffer[i] = numeric_cast<float32_t>(pcm.Samples[(frame + i) * channels + channel]) / 32768.0f;
            }
        }

        vorbis_analysis_wrote(&dsp, numeric_cast<int32_t>(block_frames));
        drain_encoder();
    }

    vorbis_analysis_wrote(&dsp, 0);
    drain_encoder();

    while (ogg_stream_flush(&stream, &page) != 0) {
        AppendOggPage(output, page);
    }

    return output;
}

static void VerifyVorbisStream(string_view fname, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!data.empty(), "Ogg file is empty", fname);

    MemoryOggSource source {.Data = data};

    ov_callbacks callbacks;

    callbacks.read_func = [](void* output_buf, size_t size, size_t count, void* datasource) -> size_t {
        auto memory = cast_from_void<MemoryOggSource*>(datasource);
        FO_VERIFY_AND_THROW(memory, "Missing Ogg memory source");
        size_t bytes_read = std::min(memory->Data.size() - memory->Pos, size * count);

        if (bytes_read > 0) {
            FO_VERIFY_AND_THROW(output_buf != nullptr, "Ogg read output buffer is null");
            span<uint8_t> output = make_span(output_buf, bytes_read);
            const_span<uint8_t> input = memory->Data.subspan(memory->Pos, bytes_read);
            std::ranges::copy(input, output.begin());
            memory->Pos += bytes_read;
        }

        return bytes_read;
    };

    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int32_t whence) -> int32_t {
        auto memory = cast_from_void<MemoryOggSource*>(datasource);
        FO_VERIFY_AND_THROW(memory, "Missing Ogg memory source");
        int64_t base = 0;

        switch (whence) {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = numeric_cast<int64_t>(memory->Pos);
            break;
        case SEEK_END:
            base = numeric_cast<int64_t>(memory->Data.size());
            break;
        default:
            return -1;
        }

        int64_t target = base + offset;

        if (target < 0 || target > numeric_cast<int64_t>(memory->Data.size())) {
            return -1;
        }

        memory->Pos = numeric_cast<size_t>(target);
        return 0;
    };

    callbacks.close_func = nullptr;

    callbacks.tell_func = [](void* datasource) -> long {
        auto memory = cast_from_void<const MemoryOggSource*>(datasource);
        FO_VERIFY_AND_THROW(memory, "Missing Ogg memory source");
        return numeric_cast<long>(memory->Pos);
    };

    OggVorbis_File vorbis_file;
    int32_t test_result = ov_test_callbacks(make_ptr(&source).void_cast(), &vorbis_file, nullptr, 0, callbacks);

    // A failed open releases its own state, so only a successful one is ours to clear
    if (test_result == 0) {
        ov_clear(&vorbis_file);
    }

    FO_VERIFY_AND_THROW(test_result == 0, "Ogg file is not a Vorbis stream", fname, test_result);
}

static auto ReadFourcc(FileReader& reader) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    std::array<uint8_t, 4> raw {};
    reader.ReadBytes(span<uint8_t> {raw.data(), raw.size()});

    string fourcc;
    fourcc.reserve(raw.size());

    for (uint8_t byte : raw) {
        fourcc.push_back(std::bit_cast<char>(byte));
    }

    return fourcc;
}

static auto DecodeWavSample(const_span<uint8_t> bytes, const WavFormat& format) -> int16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (format.Tag == WAVE_FORMAT_IEEE_FLOAT) {
        uint32_t raw = bytes[0] | (numeric_cast<uint32_t>(bytes[1]) << 8) | (numeric_cast<uint32_t>(bytes[2]) << 16) | (numeric_cast<uint32_t>(bytes[3]) << 24);
        float32_t value = std::clamp(std::bit_cast<float32_t>(raw), -1.0f, 1.0f);
        return iround<int16_t>(value * 32767.0f);
    }

    switch (format.BitsPerSample) {
    case 8:
        return numeric_cast<int16_t>((numeric_cast<int32_t>(bytes[0]) - 128) << 8);
    case 16:
        return std::bit_cast<int16_t>(numeric_cast<uint16_t>(bytes[0] | (numeric_cast<uint32_t>(bytes[1]) << 8)));
    case 24:
        return std::bit_cast<int16_t>(numeric_cast<uint16_t>(bytes[1] | (numeric_cast<uint32_t>(bytes[2]) << 8)));
    case 32:
        return std::bit_cast<int16_t>(numeric_cast<uint16_t>(bytes[2] | (numeric_cast<uint32_t>(bytes[3]) << 8)));
    default:
        throw AudioBakerException("WAV sample width is unsupported", format.BitsPerSample);
    }
}

static void AppendOggPage(vector<uint8_t>& output, const ogg_page& page)
{
    FO_NO_STACK_TRACE_ENTRY();

    const_span<uint8_t> header = make_span(page.header, numeric_cast<size_t>(page.header_len));
    const_span<uint8_t> body = make_span(page.body, numeric_cast<size_t>(page.body_len));
    output.insert(output.end(), header.begin(), header.end());
    output.insert(output.end(), body.begin(), body.end());
}

FO_END_NAMESPACE
