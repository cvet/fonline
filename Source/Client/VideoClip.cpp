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

#include "VideoClip.h"

#include "theora/theoradec.h"

FO_BEGIN_NAMESPACE();

struct VideoClip::Impl
{
    struct StreamStates
    {
        static constexpr size_t COUNT = 10;

        vector<ogg_stream_state> Streams {};
        vector<bool> StreamsState {};
        int32 MainIndex {-1};
    };

    bool Stopped {};
    bool Paused {};
    bool Looped {};
    vector<uint8> RawVideoData {};
    size_t ReadPos {};
    raw_ptr<th_dec_ctx> DecoderContext {};
    th_info VideoInfo {};
    th_comment Comment {};
    raw_ptr<th_setup_info> SetupInfo {};
    th_ycbcr_buffer ColorBuffer {};
    ogg_sync_state SyncState {};
    ogg_packet Packet {};
    StreamStates Streams {};
    vector<ucolor> RenderedTextureData {};
    int32 CurFrame {};
    timespan AverageRenderTime {};
    nanotime StartTime {};
    nanotime RenderTime {};
    nanotime PauseTime {};
};

VideoClip::VideoClip(vector<uint8> video_data) :
    _impl {SafeAlloc::MakeUnique<Impl>()}
{
    FO_STACK_TRACE_ENTRY();

    _impl->RawVideoData = std::move(video_data);

    _impl->Streams.Streams.resize(Impl::StreamStates::COUNT);
    _impl->Streams.StreamsState.resize(Impl::StreamStates::COUNT);

    th_info_init(&_impl->VideoInfo);
    th_comment_init(&_impl->Comment);
    ogg_sync_init(&_impl->SyncState);

    // Decode header
    while (true) {
        int32 stream_index = DecodePacket();

        if (stream_index < 0) {
            throw VideoClipException("Decode header packet failed");
        }

        const int32 r = th_decode_headerin(&_impl->VideoInfo, &_impl->Comment, _impl->SetupInfo.get_pp(), &_impl->Packet);

        if (r == 0) {
            if (stream_index != _impl->Streams.MainIndex) {
                while (true) {
                    stream_index = DecodePacket();

                    if (stream_index == _impl->Streams.MainIndex) {
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
            _impl->Streams.MainIndex = stream_index;
        }
    }

    _impl->DecoderContext = th_decode_alloc(&_impl->VideoInfo, _impl->SetupInfo.get());
    _impl->RenderedTextureData.resize(numeric_cast<size_t>(_impl->VideoInfo.pic_width) * _impl->VideoInfo.pic_height);
    _impl->StartTime = nanotime::now();
}

VideoClip::~VideoClip()
{
    FO_STACK_TRACE_ENTRY();

    if (_impl) {
        th_info_clear(&_impl->VideoInfo);
        th_comment_clear(&_impl->Comment);
        ogg_sync_clear(&_impl->SyncState);
        th_setup_free(_impl->SetupInfo.get());
        th_decode_free(_impl->DecoderContext.get());
    }
}

auto VideoClip::IsPlaying() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_impl->Stopped && !_impl->Paused;
}

auto VideoClip::IsStopped() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->Stopped;
}

auto VideoClip::IsPaused() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->Paused;
}

auto VideoClip::IsLooped() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->Looped;
}

auto VideoClip::GetTime() const -> timespan
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_impl->Stopped) {
        return std::chrono::milliseconds {0};
    }
    else if (_impl->Paused) {
        return _impl->PauseTime - _impl->StartTime;
    }
    else {
        return _impl->RenderTime - _impl->StartTime;
    }
}

auto VideoClip::GetSize() const -> isize32
{
    FO_NO_STACK_TRACE_ENTRY();

    return {numeric_cast<int32>(_impl->VideoInfo.pic_width), numeric_cast<int32>(_impl->VideoInfo.pic_height)};
}

void VideoClip::Stop()
{
    FO_STACK_TRACE_ENTRY();

    _impl->Stopped = true;
}

void VideoClip::Pause()
{
    FO_STACK_TRACE_ENTRY();

    _impl->Paused = true;
    _impl->PauseTime = nanotime::now();
}

void VideoClip::Resume()
{
    FO_STACK_TRACE_ENTRY();

    if (_impl->Stopped) {
        _impl->StartTime = nanotime::now();
    }
    else if (_impl->Paused) {
        _impl->StartTime = nanotime::now() - GetTime();
    }

    _impl->Stopped = false;
    _impl->Paused = false;
}

void VideoClip::SetLooped(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    _impl->Looped = enabled;
}

void VideoClip::SetTime(timespan time)
{
    FO_STACK_TRACE_ENTRY();

    _impl->StartTime = nanotime::now() - time;
}

auto VideoClip::RenderFrame() -> const vector<ucolor>&
{
    FO_STACK_TRACE_ENTRY();

    if (_impl->Stopped) {
        return _impl->RenderedTextureData;
    }

    const auto start_frame_render = nanotime::now();

    if (_impl->Paused) {
        _impl->RenderTime = _impl->PauseTime;
    }
    else {
        _impl->RenderTime = start_frame_render;
    }

    // Calculate next frame
    const float64 cur_second = (_impl->RenderTime - _impl->StartTime + _impl->AverageRenderTime).to_ms<float64>() / 1000.0;
    const int32 new_frame = iround<int32>(cur_second * numeric_cast<float64>(_impl->VideoInfo.fps_numerator) / numeric_cast<float64>(_impl->VideoInfo.fps_denominator));
    const int32 next_frame_diff = new_frame - _impl->CurFrame;

    if (next_frame_diff <= 0) {
        return _impl->RenderedTextureData;
    }

    bool last_frame = false;

    _impl->CurFrame = new_frame;

    for (int32 i = 0; i < next_frame_diff; i++) {
        // Decode frame
        int32 r = th_decode_packetin(_impl->DecoderContext.get(), &_impl->Packet, nullptr);

        if (r != TH_DUPFRAME) {
            if (r != 0) {
                WriteLog("Frame does not contain encoded video data, error {}", r);
                Stop();
                return _impl->RenderedTextureData;
            }

            // Decode color
            r = th_decode_ycbcr_out(_impl->DecoderContext.get(), _impl->ColorBuffer);

            if (r != 0) {
                WriteLog("th_decode_ycbcr_out() failed, error {}", r);
                Stop();
                return _impl->RenderedTextureData;
            }
        }

        // Seek next packet
        do {
            r = DecodePacket();

            if (r == -2) {
                last_frame = true;
                break;
            }
        } while (r != _impl->Streams.MainIndex);

        if (last_frame) {
            break;
        }
    }

    // Data offsets
    int32 di;
    int32 dj;

    switch (_impl->VideoInfo.pixel_fmt) {
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
        WriteLog("Wrong pixel format {}", _impl->VideoInfo.pixel_fmt);
        Stop();
        return _impl->RenderedTextureData;
    }

    // Fill texture data
    const auto w = numeric_cast<int32>(_impl->VideoInfo.pic_width);
    const auto h = numeric_cast<int32>(_impl->VideoInfo.pic_height);

    for (int32 y = 0; y < h; y++) {
        for (int32 x = 0; x < w; x++) {
            const th_ycbcr_buffer& cbuf = _impl->ColorBuffer;
            const uint8 cy = cbuf[0].data[y * cbuf[0].stride + x];
            const uint8 cu = cbuf[1].data[y / dj * cbuf[1].stride + x / di];
            const uint8 cv = cbuf[2].data[y / dj * cbuf[2].stride + x / di];

            // YUV to RGB
            const float32 cr = numeric_cast<float32>(cy) + 1.402f * numeric_cast<float32>(cv - 127);
            const float32 cg = numeric_cast<float32>(cy) - 0.344f * numeric_cast<float32>(cu - 127) - 0.714f * numeric_cast<float32>(cv - 127);
            const float32 cb = numeric_cast<float32>(cy) + 1.722f * numeric_cast<float32>(cu - 127);

            auto* data = reinterpret_cast<uint8*>(_impl->RenderedTextureData.data()) + (y * w * 4 + x * 4);
            data[0] = iround<uint8>(cr);
            data[1] = iround<uint8>(cg);
            data[2] = iround<uint8>(cb);
            data[3] = 0xFF;
        }
    }

    // Store render time
    const auto frame_render_duration = nanotime::now() - start_frame_render;

    if (_impl->AverageRenderTime > std::chrono::milliseconds(0)) {
        _impl->AverageRenderTime = std::chrono::milliseconds(iround<uint64>((_impl->AverageRenderTime + frame_render_duration).to_ms<float64>() / 2.0));
    }
    else {
        _impl->AverageRenderTime = frame_render_duration;
    }

    // End cycle
    if (last_frame) {
        Stop();

        if (_impl->Looped) {
            Resume();
        }
    }

    return _impl->RenderedTextureData;
}

int32 VideoClip::DecodePacket()
{
    FO_STACK_TRACE_ENTRY();

    int32 b = 0;
    int32 rv = 0;

    for (int32 i = 0; i < numeric_cast<int32>(_impl->Streams.StreamsState.size()) && _impl->Streams.StreamsState[i]; i++) {
        const int32 a = ogg_stream_packetout(&_impl->Streams.Streams[i], &_impl->Packet);

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

        while (ogg_sync_pageout(&_impl->SyncState, &op) != 1) {
            auto read_bytes = numeric_cast<int32>(_impl->RawVideoData.size() - _impl->ReadPos);
            read_bytes = std::min(1024, read_bytes);

            if (read_bytes == 0) {
                return -2;
            }

            auto* dest_buf = ogg_sync_buffer(&_impl->SyncState, read_bytes);
            MemCopy(dest_buf, _impl->RawVideoData.data() + _impl->ReadPos, read_bytes);
            _impl->ReadPos += read_bytes;
            ogg_sync_wrote(&_impl->SyncState, read_bytes);
        }

        if (ogg_page_bos(&op) != 0 && rv != 1) {
            int32 i = 0;

            while (i < numeric_cast<int32>(_impl->Streams.StreamsState.size()) && _impl->Streams.StreamsState[i]) {
                i++;
            }

            if (!_impl->Streams.StreamsState[i]) {
                const int32 a = ogg_stream_init(&_impl->Streams.Streams[i], ogg_page_serialno(&op));
                _impl->Streams.StreamsState[i] = true;

                if (a != 0) {
                    return -1;
                }
                else {
                    rv = 1;
                }
            }
        }

        for (int32 i = 0; i < numeric_cast<int32>(_impl->Streams.StreamsState.size()) && _impl->Streams.StreamsState[i]; i++) {
            ogg_stream_pagein(&_impl->Streams.Streams[i], &op);
            const int32 a = ogg_stream_packetout(&_impl->Streams.Streams[i], &_impl->Packet);

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

FO_END_NAMESPACE();
