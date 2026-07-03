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

#include "VideoClip.h"

#include "theora/theoradec.h"

FO_BEGIN_NAMESPACE

using TheoraInfo = scoped_init_clear<th_info, th_info_init, th_info_clear>;
using TheoraComment = scoped_init_clear<th_comment, th_comment_init, th_comment_clear>;
using OggSyncState = scoped_init_clear<ogg_sync_state, ogg_sync_init, ogg_sync_clear>;

struct VideoClip::Impl
{
    struct StreamStates
    {
        static constexpr size_t COUNT = 10;

        vector<ogg_stream_state> Streams {};
        vector<bool> StreamsState {};
        int32_t MainIndex {-1};
    };

    bool Stopped {};
    bool Paused {};
    bool Looped {};
    vector<uint8_t> RawVideoData {};
    size_t ReadPos {};
    unique_del_nptr<th_dec_ctx> DecoderContext {};
    TheoraInfo VideoInfo {};
    TheoraComment Comment {};
    unique_del_nptr<th_setup_info> SetupInfo {};
    th_ycbcr_buffer ColorBuffer {};
    OggSyncState SyncState {};
    ogg_packet Packet {};
    StreamStates Streams {};
    vector<ucolor> RenderedTextureData {};
    int32_t CurFrame {};
    timespan AverageRenderTime {};
    nanotime StartTime {};
    nanotime RenderTime {};
    nanotime PauseTime {};
};

VideoClip::VideoClip(VideoClip&&) noexcept = default;

VideoClip::VideoClip(vector<uint8_t> video_data) :
    _impl {SafeAlloc::MakeUnique<Impl>()}
{
    FO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();
    impl->SetupInfo = make_unique_del_ptr(nptr<th_setup_info> {}, [](th_setup_info* raw_setup_info) noexcept {
        if (raw_setup_info != nullptr) {
            ptr<th_setup_info> owned_setup_info = raw_setup_info;
            th_setup_free(owned_setup_info.get());
        }
    });
    impl->RawVideoData = std::move(video_data);

    impl->Streams.Streams.resize(Impl::StreamStates::COUNT);
    impl->Streams.StreamsState.resize(Impl::StreamStates::COUNT);

    // Decode header
    while (true) {
        int32_t stream_index = DecodePacket();

        if (stream_index < 0) {
            throw VideoClipException("Decode header packet failed");
        }

        th_setup_info* setup_info_raw = impl->SetupInfo.release();
        const int32_t r = th_decode_headerin(&impl->VideoInfo.Value, &impl->Comment.Value, &setup_info_raw, &impl->Packet);
        impl->SetupInfo.reset(setup_info_raw);

        if (r == 0) {
            if (stream_index != impl->Streams.MainIndex) {
                while (true) {
                    stream_index = DecodePacket();

                    if (stream_index == impl->Streams.MainIndex) {
                        break;
                    }
                    if (stream_index < 0) {
                        throw VideoClipException("Seek first data packet failed");
                    }
                }
            }

            break;
        }
        else {
            impl->Streams.MainIndex = stream_index;
        }
    }

    auto setup_info = impl->SetupInfo.as_ptr();
    nptr<th_dec_ctx> decoder_context = th_decode_alloc(&impl->VideoInfo.Value, setup_info.get());
    FO_VERIFY_AND_THROW(decoder_context, "Theora decoder context allocation failed");
    impl->DecoderContext = make_unique_del_ptr(decoder_context.as_ptr(), [](th_dec_ctx* raw_decoder_context) noexcept {
        if (raw_decoder_context != nullptr) {
            ptr<th_dec_ctx> owned_decoder_context = raw_decoder_context;
            th_decode_free(owned_decoder_context.get());
        }
    });
    impl->RenderedTextureData.resize(numeric_cast<size_t>(impl->VideoInfo.Value.pic_width) * impl->VideoInfo.Value.pic_height);
    impl->StartTime = nanotime::now();
}

VideoClip::~VideoClip()
{
    FO_STACK_TRACE_ENTRY();
}

auto VideoClip::IsPlaying() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();
    return !impl->Stopped && !impl->Paused;
}

auto VideoClip::IsStopped() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl.as_ptr()->Stopped;
}

auto VideoClip::IsPaused() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl.as_ptr()->Paused;
}

auto VideoClip::IsLooped() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl.as_ptr()->Looped;
}

auto VideoClip::GetTime() const -> timespan
{
    FO_NO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();

    if (impl->Stopped) {
        return std::chrono::milliseconds {0};
    }
    else if (impl->Paused) {
        return impl->PauseTime - impl->StartTime;
    }
    else {
        return impl->RenderTime - impl->StartTime;
    }
}

auto VideoClip::GetSize() const -> isize32
{
    FO_NO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();
    return {numeric_cast<int32_t>(impl->VideoInfo.Value.pic_width), numeric_cast<int32_t>(impl->VideoInfo.Value.pic_height)};
}

void VideoClip::Stop()
{
    FO_STACK_TRACE_ENTRY();

    _impl.as_ptr()->Stopped = true;
}

void VideoClip::Pause()
{
    FO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();
    impl->Paused = true;
    impl->PauseTime = nanotime::now();
}

void VideoClip::Resume()
{
    FO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();

    if (impl->Stopped) {
        impl->StartTime = nanotime::now();
    }
    else if (impl->Paused) {
        impl->StartTime = nanotime::now() - GetTime();
    }

    impl->Stopped = false;
    impl->Paused = false;
}

void VideoClip::SetLooped(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    _impl.as_ptr()->Looped = enabled;
}

void VideoClip::SetTime(timespan time)
{
    FO_STACK_TRACE_ENTRY();

    _impl.as_ptr()->StartTime = nanotime::now() - time;
}

auto VideoClip::RenderFrame() -> const vector<ucolor>&
{
    FO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();

    if (impl->Stopped) {
        return impl->RenderedTextureData;
    }

    const auto start_frame_render = nanotime::now();

    if (impl->Paused) {
        impl->RenderTime = impl->PauseTime;
    }
    else {
        impl->RenderTime = start_frame_render;
    }

    // Calculate next frame
    const float64_t cur_second = (impl->RenderTime - impl->StartTime + impl->AverageRenderTime).to_ms<float64_t>() / 1000.0;
    const int32_t new_frame = iround<int32_t>(cur_second * numeric_cast<float64_t>(impl->VideoInfo.Value.fps_numerator) / numeric_cast<float64_t>(impl->VideoInfo.Value.fps_denominator));
    const int32_t next_frame_diff = new_frame - impl->CurFrame;

    if (next_frame_diff <= 0) {
        return impl->RenderedTextureData;
    }

    bool last_frame = false;

    impl->CurFrame = new_frame;
    auto decoder_context = impl->DecoderContext.as_ptr();

    for (int32_t i = 0; i < next_frame_diff; i++) {
        // Decode frame
        int32_t r = th_decode_packetin(decoder_context.get(), &impl->Packet, nullptr);

        if (r != TH_DUPFRAME) {
            if (r != 0) {
                WriteLog("Frame does not contain encoded video data, error {}", r);
                Stop();
                return impl->RenderedTextureData;
            }

            // Decode color
            r = th_decode_ycbcr_out(decoder_context.get(), impl->ColorBuffer);

            if (r != 0) {
                WriteLog("th_decode_ycbcr_out() failed, error {}", r);
                Stop();
                return impl->RenderedTextureData;
            }
        }

        // Seek next packet
        do {
            r = DecodePacket();

            if (r == -2) {
                last_frame = true;
                break;
            }
        } while (r != impl->Streams.MainIndex);

        if (last_frame) {
            break;
        }
    }

    // Data offsets
    int32_t di;
    int32_t dj;

    switch (impl->VideoInfo.Value.pixel_fmt) {
    case TH_PF_420:
        di = 2;
        dj = 2;
        break;
    case TH_PF_422:
        di = 2;
        dj = 1;
        break;
    case TH_PF_444:
        di = 1;
        dj = 1;
        break;
    default:
        WriteLog("Wrong pixel format {}", impl->VideoInfo.Value.pixel_fmt);
        Stop();
        return impl->RenderedTextureData;
    }

    // Fill texture data
    const auto w = numeric_cast<int32_t>(impl->VideoInfo.Value.pic_width);
    const auto h = numeric_cast<int32_t>(impl->VideoInfo.Value.pic_height);

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const th_ycbcr_buffer& cbuf = impl->ColorBuffer;
            const uint8_t cy = cbuf[0].data[y * cbuf[0].stride + x];
            const uint8_t cu = cbuf[1].data[y / dj * cbuf[1].stride + x / di];
            const uint8_t cv = cbuf[2].data[y / dj * cbuf[2].stride + x / di];

            // YUV to RGB
            const float32_t cr = numeric_cast<float32_t>(cy) + 1.402f * numeric_cast<float32_t>(cv - 127);
            const float32_t cg = numeric_cast<float32_t>(cy) - 0.344f * numeric_cast<float32_t>(cu - 127) - 0.714f * numeric_cast<float32_t>(cv - 127);
            const float32_t cb = numeric_cast<float32_t>(cy) + 1.722f * numeric_cast<float32_t>(cu - 127);

            ucolor& pixel = impl->RenderedTextureData[numeric_cast<size_t>(y) * numeric_cast<size_t>(w) + numeric_cast<size_t>(x)];
            pixel.comp.r = iround<uint8_t>(std::clamp(cr, 0.0f, 255.0f));
            pixel.comp.g = iround<uint8_t>(std::clamp(cg, 0.0f, 255.0f));
            pixel.comp.b = iround<uint8_t>(std::clamp(cb, 0.0f, 255.0f));
            pixel.comp.a = 0xFF;
        }
    }

    // Store render time
    const auto frame_render_duration = nanotime::now() - start_frame_render;

    if (impl->AverageRenderTime > std::chrono::milliseconds(0)) {
        impl->AverageRenderTime = std::chrono::milliseconds(iround<uint64_t>((impl->AverageRenderTime + frame_render_duration).to_ms<float64_t>() / 2.0));
    }
    else {
        impl->AverageRenderTime = frame_render_duration;
    }

    // End cycle
    if (last_frame) {
        Stop();

        if (impl->Looped) {
            Resume();
        }
    }

    return impl->RenderedTextureData;
}

int32_t VideoClip::DecodePacket()
{
    FO_STACK_TRACE_ENTRY();

    auto impl = _impl.as_ptr();
    int32_t b = 0;
    int32_t rv = 0;

    for (int32_t i = 0; i < numeric_cast<int32_t>(impl->Streams.StreamsState.size()) && impl->Streams.StreamsState[i]; i++) {
        const int32_t a = ogg_stream_packetout(&impl->Streams.Streams[i], &impl->Packet);

        switch (a) {
        case 1:
            b = i + 1;
            [[fallthrough]];
        case 0:
        case -1:
            break;
        default:
            break;
        }
    }

    if (b != 0) {
        return rv = b - 1;
    }

    do {
        ogg_page op;

        while (ogg_sync_pageout(&impl->SyncState.Value, &op) != 1) {
            auto read_bytes = numeric_cast<int32_t>(impl->RawVideoData.size() - impl->ReadPos);
            read_bytes = std::min(1024, read_bytes);

            if (read_bytes == 0) {
                return -2;
            }

            ptr<char> dest_buf = ogg_sync_buffer(&impl->SyncState.Value, read_bytes);
            ptr<const uint8_t> source = ptr<const uint8_t> {impl->RawVideoData.data()}.offset(impl->ReadPos);
            MemCopy(dest_buf, source, read_bytes);
            impl->ReadPos += read_bytes;
            ogg_sync_wrote(&impl->SyncState.Value, read_bytes);
        }

        if (ogg_page_bos(&op) != 0 && rv != 1) {
            int32_t i = 0;

            while (i < numeric_cast<int32_t>(impl->Streams.StreamsState.size()) && impl->Streams.StreamsState[i]) {
                i++;
            }

            if (!impl->Streams.StreamsState[i]) {
                const int32_t a = ogg_stream_init(&impl->Streams.Streams[i], ogg_page_serialno(&op));
                impl->Streams.StreamsState[i] = true;

                if (a != 0) {
                    return -1;
                }
                else {
                    rv = 1;
                }
            }
        }

        for (int32_t i = 0; i < numeric_cast<int32_t>(impl->Streams.StreamsState.size()) && impl->Streams.StreamsState[i]; i++) {
            ogg_stream_pagein(&impl->Streams.Streams[i], &op);
            const int32_t a = ogg_stream_packetout(&impl->Streams.Streams[i], &impl->Packet);

            switch (a) {
            case 1:
                rv = i;
                b = i + 1;
                [[fallthrough]];
            case 0:
            case -1:
                break;
            default:
                break;
            }
        }
    } while (b == 0);

    return rv;
}

FO_END_NAMESPACE
