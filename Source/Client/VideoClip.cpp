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
#include "Log.h"

#include "theora/theoradec.h"

FO_BEGIN_NAMESPACE();

struct VideoClip::Impl
{
    struct StreamStates
    {
        static constexpr size_t COUNT = 10;

        vector<ogg_stream_state> Streams {};
        vector<bool> StreamsState {};
        int MainIndex {-1};
    };

    bool Stopped {};
    bool Paused {};
    bool Looped {};
    vector<uint8> RawVideoData {};
    size_t ReadPos {};
    th_dec_ctx* DecoderContext {};
    th_info VideoInfo {};
    th_comment Comment {};
    th_setup_info* SetupInfo {};
    th_ycbcr_buffer ColorBuffer {};
    ogg_sync_state SyncState {};
    ogg_packet Packet {};
    StreamStates Streams {};
    vector<ucolor> RenderedTextureData {};
    int CurFrame {};
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
        int stream_index = DecodePacket();

        if (stream_index < 0) {
            throw VideoClipException("Decode header packet failed");
        }

        const int r = th_decode_headerin(&_impl->VideoInfo, &_impl->Comment, &_impl->SetupInfo, &_impl->Packet);
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

    _impl->DecoderContext = th_decode_alloc(&_impl->VideoInfo, _impl->SetupInfo);

    _impl->RenderedTextureData.resize(static_cast<size_t>(_impl->VideoInfo.pic_width) * _impl->VideoInfo.pic_height);

    _impl->StartTime = nanotime::now();
}

VideoClip::~VideoClip()
{
    FO_STACK_TRACE_ENTRY();

    if (_impl) {
        th_info_clear(&_impl->VideoInfo);
        th_comment_clear(&_impl->Comment);
        ogg_sync_clear(&_impl->SyncState);
        th_setup_free(_impl->SetupInfo);
        th_decode_free(_impl->DecoderContext);
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

auto VideoClip::GetSize() const -> isize
{
    FO_NO_STACK_TRACE_ENTRY();

    return {static_cast<int>(_impl->VideoInfo.pic_width), static_cast<int>(_impl->VideoInfo.pic_height)};
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
    const double cur_second = (_impl->RenderTime - _impl->StartTime + _impl->AverageRenderTime).to_ms<double>() / 1000.0;
    const int new_frame = iround(cur_second * static_cast<double>(_impl->VideoInfo.fps_numerator) / static_cast<double>(_impl->VideoInfo.fps_denominator));
    const int next_frame_diff = new_frame - _impl->CurFrame;

    if (next_frame_diff <= 0) {
        return _impl->RenderedTextureData;
    }

    bool last_frame = false;

    _impl->CurFrame = new_frame;

    for (int i = 0; i < next_frame_diff; i++) {
        // Decode frame
        int r = th_decode_packetin(_impl->DecoderContext, &_impl->Packet, nullptr);
        if (r != TH_DUPFRAME) {
            if (r != 0) {
                WriteLog("Frame does not contain encoded video data, error {}", r);
                Stop();
                return _impl->RenderedTextureData;
            }

            // Decode color
            r = th_decode_ycbcr_out(_impl->DecoderContext, _impl->ColorBuffer);
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
    int di;
    int dj;

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
    const uint w = _impl->VideoInfo.pic_width;
    const uint h = _impl->VideoInfo.pic_height;

    for (uint y = 0; y < h; y++) {
        for (uint x = 0; x < w; x++) {
            const th_ycbcr_buffer& cbuf = _impl->ColorBuffer;
            const uint8 cy = cbuf[0].data[y * cbuf[0].stride + x];
            const uint8 cu = cbuf[1].data[y / dj * cbuf[1].stride + x / di];
            const uint8 cv = cbuf[2].data[y / dj * cbuf[2].stride + x / di];

            // YUV to RGB
            const float cr = static_cast<float>(cy) + 1.402f * static_cast<float>(cv - 127);
            const float cg = static_cast<float>(cy) - 0.344f * static_cast<float>(cu - 127) - 0.714f * static_cast<float>(cv - 127);
            const float cb = static_cast<float>(cy) + 1.722f * static_cast<float>(cu - 127);

            auto* data = reinterpret_cast<uint8*>(_impl->RenderedTextureData.data()) + (y * w * 4 + x * 4);
            data[0] = static_cast<uint8>(cr);
            data[1] = static_cast<uint8>(cg);
            data[2] = static_cast<uint8>(cb);
            data[3] = 0xFF;
        }
    }

    // Store render time
    const auto frame_render_duration = nanotime::now() - start_frame_render;

    if (_impl->AverageRenderTime > std::chrono::milliseconds {0}) {
        _impl->AverageRenderTime = std::chrono::milliseconds {static_cast<uint64>((_impl->AverageRenderTime + frame_render_duration).to_ms<double>() / 2.0)};
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

int VideoClip::DecodePacket()
{
    FO_STACK_TRACE_ENTRY();

    int b = 0;
    int rv = 0;
    for (int i = 0; i < static_cast<int>(_impl->Streams.StreamsState.size()) && _impl->Streams.StreamsState[i]; i++) {
        const int a = ogg_stream_packetout(&_impl->Streams.Streams[i], &_impl->Packet);
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
            auto read_bytes = static_cast<int>(_impl->RawVideoData.size() - _impl->ReadPos);
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
            int i = 0;
            while (i < static_cast<int>(_impl->Streams.StreamsState.size()) && _impl->Streams.StreamsState[i]) {
                i++;
            }

            if (!_impl->Streams.StreamsState[i]) {
                const int a = ogg_stream_init(&_impl->Streams.Streams[i], ogg_page_serialno(&op));
                _impl->Streams.StreamsState[i] = true;
                if (a != 0) {
                    return -1;
                }
                else {
                    rv = 1;
                }
            }
        }

        for (int i = 0; i < static_cast<int>(_impl->Streams.StreamsState.size()) && _impl->Streams.StreamsState[i]; i++) {
            ogg_stream_pagein(&_impl->Streams.Streams[i], &op);
            const int a = ogg_stream_packetout(&_impl->Streams.Streams[i], &_impl->Packet);
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
