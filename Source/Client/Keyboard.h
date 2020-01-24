//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "KeyCodes_Include.h"
#include "Settings.h"

// Keyboard input flags
#define KIF_NO_SPEC_SYMBOLS (1) // Ignore \n \r \t
#define KIF_ONLY_NUMBERS (2) // Only 0..9
#define KIF_FILE_NAME (4) // Ignore \/:*?\"<>|

class SpriteManager;

class Keyboard : public NonCopyable
{
public:
    Keyboard(InputSettings& sett, SpriteManager& spr_mngr);
    ~Keyboard();
    void Lost();
    void GetChar(uchar dik, const string& dik_text, string& str, uint* position, uint max, int flags);
    void EraseInvalidChars(string& str, int flags);
    uchar MapKey(ushort code);
    ushort UnmapKey(uchar key);

    bool ShiftDwn = false;
    bool CtrlDwn = false;
    bool AltDwn = false;
    bool KeyPressed[0x100] = {};

private:
    bool IsInvalidChar(const char* str, uint flags, uint& length);

    InputSettings& settings;
    SpriteManager& sprMngr;
    uchar KeysMap[0x200] = {};
    ushort KeysMapRevert[0x100] = {};
    uchar KeysMapUser[0x100] = {};
};
