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

#pragma once

#include "Common.h"

DECLARE_EXCEPTION(VideoClipException);

class VideoClip
{
public:
    explicit VideoClip(vector<uint8> video_data);
    VideoClip(const VideoClip&) = delete;
    VideoClip(VideoClip&&) noexcept = default;
    auto operator=(const VideoClip&) = delete;
    auto operator=(VideoClip&&) noexcept = delete;
    ~VideoClip();

    [[nodiscard]] auto IsPlaying() const noexcept -> bool;
    [[nodiscard]] auto IsStopped() const noexcept -> bool;
    [[nodiscard]] auto IsPaused() const noexcept -> bool;
    [[nodiscard]] auto IsLooped() const noexcept -> bool;
    [[nodiscard]] auto GetTime() const -> timespan;
    [[nodiscard]] auto GetSize() const -> isize;

    void Stop();
    void Pause();
    void Resume();
    void SetLooped(bool enabled);
    void SetTime(timespan time);
    auto RenderFrame() -> const vector<ucolor>&;

private:
    struct Impl;

    auto DecodePacket() -> int;

    unique_ptr<Impl> _impl;
};
