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

// Todo: merge Keyboard into App::Input and Client/Mapper

#pragma once

#include "Common.h"

#include "Application.h"
#include "Settings.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

// Keyboard input flags
static constexpr uint32 KIF_NO_SPEC_SYMBOLS = 1; // Ignore \n \r \t
static constexpr uint32 KIF_ONLY_NUMBERS = 2; // Only 0..9
static constexpr uint32 KIF_FILE_NAME = 4; // Ignore \/:*?\"<>|

class Keyboard final
{
public:
    Keyboard() = delete;
    explicit Keyboard(InputSettings& settings, SpriteManager& spr_mngr);
    Keyboard(const Keyboard&) = delete;
    Keyboard(Keyboard&&) noexcept = delete;
    auto operator=(const Keyboard&) = delete;
    auto operator=(Keyboard&&) noexcept = delete;
    ~Keyboard() = default;

    void Lost();
    void FillChar(KeyCode dik, string_view dik_text, string& str, int32* position, uint32 flags) const;
    void RemoveInvalidChars(string& str, uint32 flags) const;

    bool ShiftDwn {};
    bool CtrlDwn {};
    bool AltDwn {};
    bool KeyPressed[0x100] {};

private:
    [[nodiscard]] auto IsInvalidChar(const char* str, uint32 flags, int32& length) const -> bool;

    raw_ptr<InputSettings> _settings;
    raw_ptr<SpriteManager> _sprMngr;
};

FO_END_NAMESPACE();
