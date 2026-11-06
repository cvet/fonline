// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(VideoClipException);

class VideoClip
{
public:
    explicit VideoClip(vector<uint8_t> video_data);
    VideoClip(const VideoClip&) = delete;
    VideoClip(VideoClip&&) noexcept;
    auto operator=(const VideoClip&) = delete;
    auto operator=(VideoClip&&) noexcept = delete;
    ~VideoClip();

    [[nodiscard]] auto IsPlaying() const noexcept -> bool;
    [[nodiscard]] auto IsStopped() const noexcept -> bool;
    [[nodiscard]] auto IsPaused() const noexcept -> bool;
    [[nodiscard]] auto IsLooped() const noexcept -> bool;
    [[nodiscard]] auto GetTime() const -> timespan;
    [[nodiscard]] auto GetSize() const -> isize32;

    void Stop();
    void Pause();
    void Resume();
    void SetLooped(bool enabled);
    void SetTime(timespan time);
    auto RenderFrame() -> const vector<ucolor>&;

private:
    struct Impl;
    auto DecodePacket() -> int32_t;

    unique_nptr<Impl> _impl {};
};

FO_END_NAMESPACE
