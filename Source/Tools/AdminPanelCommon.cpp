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

#include "AdminPanelCommon.h"

FO_BEGIN_NAMESPACE

void AdminChannelCipher::Init(string_view password) noexcept
{
    if (password.empty()) {
        Reset();
        return;
    }

    _state = static_cast<uint32_t>(hashing_ex::hash(password.data(), password.size()) & 0xFFFFFFFFULL);

    if (_state == 0) {
        _state = 0xA341316CU;
    }

    _enabled = true;
}

void AdminChannelCipher::Reset() noexcept
{
    _enabled = false;
    _state = 0;
}

void AdminChannelCipher::Apply(void* data, size_t len) noexcept
{
    if (!_enabled || data == nullptr || len == 0) {
        return;
    }

    uint8_t* bytes = static_cast<uint8_t*>(data);

    for (size_t i = 0; i < len; i++) {
        _state ^= _state << 13;
        _state ^= _state >> 17;
        _state ^= _state << 5;
        bytes[i] ^= numeric_cast<uint8_t>(_state & 0xFF);
    }
}

FO_END_NAMESPACE
