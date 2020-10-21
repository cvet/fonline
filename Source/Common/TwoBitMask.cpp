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

#include "TwoBitMask.h"

TwoBitMask::TwoBitMask(uint width, uint height, uchar* ptr)
{
    if (width == 0u) {
        width = 1;
    }
    if (height == 0u) {
        height = 1;
    }

    _width = width;
    _height = height;
    _widthBytes = _width / 4;

    if (_width % 4 != 0u) {
        _widthBytes++;
    }

    if (ptr != nullptr) {
        _isAlloc = false;
        _data = ptr;
    }
    else {
        _isAlloc = true;
        _data = new uchar[static_cast<size_t>(_widthBytes) * _height];
        Fill(0);
    }
}

TwoBitMask::~TwoBitMask()
{
    if (_isAlloc) {
        delete[] _data;
    }
}

void TwoBitMask::Set2Bit(uint x, uint y, int val) const
{
    if (x >= _width || y >= _height) {
        return;
    }

    auto& b = _data[y * _widthBytes + x / 4];
    const auto bit = static_cast<int>(x) % 4 * 2;

    UnsetBit(b, static_cast<uchar>(3 << bit));
    SetBit(b, static_cast<uchar>((val & 3) << bit));
}

auto TwoBitMask::Get2Bit(uint x, uint y) const -> int
{
    if (x >= _width || y >= _height) {
        return 0;
    }

    return _data[y * _widthBytes + x / 4] >> x % 4 * 2 & 3;
}

void TwoBitMask::Fill(int fill) const
{
    std::memset(_data, fill, static_cast<size_t>(_widthBytes) * _height);
}

auto TwoBitMask::GetData() -> uchar*
{
    NON_CONST_METHOD_HINT();

    return _data;
}
