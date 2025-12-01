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

#include "Keyboard.h"

FO_BEGIN_NAMESPACE();

Keyboard::Keyboard(InputSettings& settings, SpriteManager& spr_mngr) :
    _settings {&settings},
    _sprMngr {&spr_mngr}
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(_settings);
}

void Keyboard::Lost()
{
    FO_STACK_TRACE_ENTRY();

    CtrlDwn = false;
    AltDwn = false;
    ShiftDwn = false;
}

void Keyboard::FillChar(KeyCode dik, string_view dik_text, string& str, int32* position, uint32 flags) const
{
    FO_STACK_TRACE_ENTRY();

    if (AltDwn) {
        return;
    }

    const auto ctrl_shift = CtrlDwn || ShiftDwn;
    const auto dik_text_len_utf8 = strex(dik_text).length_utf8();
    const auto str_len = numeric_cast<int32>(str.length());

    int32 position_dummy = str_len;
    int32& pos = position != nullptr ? *position : position_dummy;
    pos = std::min(pos, str_len);

    if (dik == KeyCode::Right && !ctrl_shift) {
        if (pos < str_len) {
            pos++;

            while (pos < str_len && (str[pos] & 0xC0) == 0x80) {
                pos++;
            }
        }
    }
    else if (dik == KeyCode::Left && !ctrl_shift) {
        if (pos > 0) {
            pos--;

            while (pos != 0u && (str[pos] & 0xC0) == 0x80) {
                pos--;
            }
        }
    }
    else if (dik == KeyCode::Back && !ctrl_shift) {
        if (pos > 0) {
            int32 letter_len = 1;
            pos--;

            while (pos != 0 && (str[pos] & 0xC0) == 0x80) {
                pos--;
                letter_len++;
            }

            str.erase(pos, letter_len);
        }
    }
    else if (dik == KeyCode::Delete && !ctrl_shift) {
        if (pos < str_len) {
            int32 letter_len = 1;
            auto pos_ = pos + 1;

            while (pos_ < str_len && (str[pos_] & 0xC0) == 0x80) {
                pos_++;
                letter_len++;
            }

            str.erase(pos, letter_len);
        }
    }
    else if (dik == KeyCode::Home && !ctrl_shift) {
        pos = 0;
    }
    else if (dik == KeyCode::End && !ctrl_shift) {
        pos = str_len;
    }
    else if (CtrlDwn && !ShiftDwn && str_len > 0 && (dik == KeyCode::C || dik == KeyCode::X)) {
        App->Input.SetClipboardText(str);

        if (dik == KeyCode::X) {
            str.clear();
            pos = 0;
        }
    }
    else if (CtrlDwn && !ShiftDwn && dik == KeyCode::V) {
        const auto cb_text = App->Input.GetClipboardText();
        App->Input.PushEvent(InputEvent {InputEvent::KeyDownEvent({KeyCode::Text, cb_text})});
        App->Input.PushEvent(InputEvent {InputEvent::KeyUpEvent({KeyCode::Text})});
    }
    else if (dik == KeyCode::Text) {
        auto text = string(dik_text);
        RemoveInvalidChars(text, flags);

        if (!text.empty()) {
            str.insert(pos, text);
            pos += numeric_cast<int32>(text.length());
        }
    }
    else {
        if (dik_text_len_utf8 == 0) {
            return;
        }
        if (CtrlDwn) {
            return;
        }

        for (size_t i = 0; i < dik_text.length();) {
            int32 length = 0;

            if (IsInvalidChar(dik_text.data() + i, flags, length)) {
                return;
            }

            i += length;
        }

        str.insert(pos, dik_text);
        pos += numeric_cast<int32>(dik_text.length());
    }
}

void Keyboard::RemoveInvalidChars(string& str, uint32 flags) const
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < str.length();) {
        int32 length = 0;

        if (IsInvalidChar(str.c_str() + i, flags, length)) {
            str.erase(i, length);
        }
        else {
            i += length;
        }
    }
}

auto Keyboard::IsInvalidChar(const char* str, uint32 flags, int32& length) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    size_t decode_length = utf8::DecodeStrNtLen(str);
    const auto ucs = utf8::Decode(str, decode_length);

    if (!utf8::IsValid(ucs)) {
        return true;
    }

    length = numeric_cast<int32>(decode_length);

    if (length == 1) {
        if ((flags & KIF_NO_SPEC_SYMBOLS) != 0u && (*str == '\n' || *str == '\r' || *str == '\t')) {
            return true;
        }
        if ((flags & KIF_ONLY_NUMBERS) != 0u && (*str < '0' || *str > '9')) {
            return true;
        }

        if ((flags & KIF_FILE_NAME) != 0u) {
            switch (*str) {
            case '\\':
            case '/':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
            case '\n':
            case '\r':
            case '\t':
                return true;
            default:
                break;
            }
        }
    }

    return !_sprMngr->HaveLetter(-1, ucs);
}

FO_END_NAMESPACE();
