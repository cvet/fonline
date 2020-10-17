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

#include "NetProtocol-Include.h"

class NetBuffer final
{
public:
    static constexpr uint DEFAULT_BUF_SIZE = 4096;
    static constexpr int CRYPT_KEYS_COUNT = 50;
    static constexpr int STRING_LEN_SIZE = sizeof(ushort);

    NetBuffer();
    NetBuffer(const NetBuffer&) = delete;
    NetBuffer(NetBuffer&&) noexcept = default;
    auto operator=(const NetBuffer&) = delete;
    auto operator=(NetBuffer&&) noexcept -> NetBuffer& = default;
    ~NetBuffer() = default;

    [[nodiscard]] auto GetData() -> uchar*;
    [[nodiscard]] auto GetCurData() -> uchar*;
    [[nodiscard]] auto GetLen() const -> uint { return _bufLen; }
    [[nodiscard]] auto GetCurPos() const -> uint { return _bufReadPos; }
    [[nodiscard]] auto GetEndPos() const -> uint { return _bufEndPos; }
    [[nodiscard]] auto IsError() const -> bool { return _isError; }
    [[nodiscard]] auto IsEmpty() const -> bool { return _bufReadPos >= _bufEndPos; }
    [[nodiscard]] auto IsHaveSize(uint size) const -> bool { return _bufReadPos + size <= _bufEndPos; }
    [[nodiscard]] auto NeedProcess() -> bool;

    void SetEncryptKey(uint seed);
    void Refresh();
    void Reset();
    void Push(const void* buf, uint len);
    void Push(const void* buf, uint len, bool no_crypt);
    void Pop(void* buf, uint len);
    void Cut(uint len);
    void GrowBuf(uint len);
    void SetEndPos(uint pos) { _bufEndPos = pos; }
    void MoveReadPos(int val);
    void SetError(bool value) { _isError = value; }
    void SkipMsg(uint msg);

    // Generic specification
    template<typename T>
    auto operator<<(const T& i) -> NetBuffer&
    {
        Push(&i, sizeof(T));
        return *this;
    }

    template<typename T>
    auto operator>>(T& i) -> NetBuffer&
    {
        Pop(&i, sizeof(T));
        return *this;
    }

    // String specification
    auto operator<<(const string& i) -> NetBuffer&
    {
        RUNTIME_ASSERT(i.length() <= 65535);
        auto len = static_cast<ushort>(i.length());
        Push(&len, sizeof(len), false);
        Push(i.c_str(), len, false);
        return *this;
    }

    auto operator>>(string& i) -> NetBuffer&
    {
        ushort len = 0;
        Pop(&len, sizeof(len));
        i.resize(len);
        Pop(&i[0], len);
        return *this;
    }

private:
    auto EncryptKey(int move) -> uchar;
    void CopyBuf(const void* from, void* to, uchar crypt_key, uint len);

    bool _isError {};
    unique_ptr<uchar[]> _bufData {};
    uint _bufLen {};
    uint _bufEndPos {};
    uint _bufReadPos {};
    bool _encryptActive {};
    int _encryptKeyPos {};
    uchar _encryptKeys[CRYPT_KEYS_COUNT] {};
};
