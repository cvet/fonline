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

#include "NetProtocol-Include.h"

class NetBuffer
{
public:
    static constexpr uint DEFAULT_BUF_SIZE = 4096;
    static constexpr int CRYPT_KEYS_COUNT = 50;
    static constexpr uint STRING_LEN_SIZE = sizeof(ushort);

    NetBuffer();
    NetBuffer(const NetBuffer&) = delete;
    NetBuffer(NetBuffer&&) noexcept = default;
    auto operator=(const NetBuffer&) = delete;
    auto operator=(NetBuffer&&) noexcept -> NetBuffer& = default;
    virtual ~NetBuffer() = default;

    [[nodiscard]] auto IsError() const -> bool { return _isError; }
    [[nodiscard]] auto GetData() -> uchar*;
    [[nodiscard]] auto GetEndPos() const -> uint { return _bufEndPos; }

    void SetError(bool value) { _isError = value; }
    static auto GenerateEncryptKey() -> uint;
    void SetEncryptKey(uint seed);
    virtual void ResetBuf();
    void GrowBuf(uint len);

protected:
    auto EncryptKey(int move) -> uchar;
    void CopyBuf(const void* from, void* to, uchar crypt_key, uint len);

    bool _isError {};
    unique_ptr<uchar[]> _bufData {};
    uint _bufLen {};
    uint _bufEndPos {};
    bool _encryptActive {};
    int _encryptKeyPos {};
    uchar _encryptKeys[CRYPT_KEYS_COUNT] {};
    bool _nonConstHelper {};
};

class NetOutBuffer final : public NetBuffer
{
public:
    NetOutBuffer() = default;
    NetOutBuffer(const NetOutBuffer&) = delete;
    NetOutBuffer(NetOutBuffer&&) noexcept = default;
    auto operator=(const NetOutBuffer&) = delete;
    auto operator=(NetOutBuffer&&) noexcept -> NetOutBuffer& = default;
    ~NetOutBuffer() override = default;

    [[nodiscard]] auto IsEmpty() const -> bool { return _bufEndPos == 0u; }

    void Push(const void* buf, uint len);
    void Cut(uint len);

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    auto operator<<(const T& i) -> NetBuffer&
    {
        Push(&i, sizeof(T));
        return *this;
    }

    auto operator<<(string_view i) -> NetBuffer&
    {
        RUNTIME_ASSERT(i.length() <= std::numeric_limits<ushort>::max());
        const auto len = static_cast<ushort>(i.length());
        Push(&len, sizeof(len));
        Push(i.data(), len);
        return *this;
    }

    auto operator<<(hstring i) -> NetBuffer&
    {
        *this << i.as_hash();
        return *this;
    }
};

class NetInBuffer final : public NetBuffer
{
public:
    NetInBuffer() = default;
    NetInBuffer(const NetInBuffer&) = delete;
    NetInBuffer(NetInBuffer&&) noexcept = default;
    auto operator=(const NetInBuffer&) = delete;
    auto operator=(NetInBuffer&&) noexcept -> NetInBuffer& = default;
    ~NetInBuffer() override = default;

    [[nodiscard]] auto GetReadPos() const -> uint { return _bufReadPos; }
    [[nodiscard]] auto GetAvailLen() const -> uint { return _bufLen - _bufEndPos; }
    [[nodiscard]] auto NeedProcess() -> bool;

    void AddData(const void* buf, uint len);
    void SetEndPos(uint pos) { _bufEndPos = pos; }
    void SkipMsg(uint msg);
    void ShrinkReadBuf();
    void Pop(void* buf, uint len);
    void ResetBuf() override;

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    auto operator>>(T& i) -> NetBuffer&
    {
        Pop(&i, sizeof(T));
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

    auto operator>>(hstring& i) -> NetBuffer& = delete;
    [[nodiscard]] auto ReadHashedString(NameResolver& name_resolver) -> hstring;

private:
    uint _bufReadPos {};
};
