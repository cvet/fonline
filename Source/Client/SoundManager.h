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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "FileSystem.h"
#include "Settings.h"

class SoundManager final
{
public:
    SoundManager() = delete;
    SoundManager(AudioSettings& settings, FileSystem& resources);
    SoundManager(const SoundManager&) = delete;
    SoundManager(SoundManager&&) noexcept = delete;
    auto operator=(const SoundManager&) = delete;
    auto operator=(SoundManager&&) noexcept = delete;
    ~SoundManager();

    auto PlaySound(const map<string, string>& sound_names, string_view name) -> bool;
    auto PlayMusic(string_view fname, uint repeat_time) -> bool;
    void StopSounds();
    void StopMusic();

private:
    struct Sound;
    using SoundsFunc = std::function<void(uchar*)>;
    using SoundVec = vector<Sound*>;

    [[nodiscard]] auto ProcessSound(Sound* sound, uchar* output) -> bool;
    [[nodiscard]] auto Load(string_view fname, bool is_music) -> Sound*;
    [[nodiscard]] auto LoadWav(Sound* sound, string_view fname) -> bool;
    [[nodiscard]] auto LoadAcm(Sound* sound, string_view fname, bool is_music) -> bool;
    [[nodiscard]] auto LoadOgg(Sound* sound, string_view fname) -> bool;
    [[nodiscard]] auto ConvertData(Sound* sound) -> bool;

    void ProcessSounds(uchar* output);
    auto StreamOgg(Sound* sound) -> bool;

    AudioSettings& _settings;
    FileSystem& _resources;
    bool _isActive {};
    uint _streamingPortion {};
    SoundVec _soundsActive {};
    vector<uchar> _outputBuf {};
    SoundsFunc _soundsFunc {};
    bool _nonConstHelper {};
};
