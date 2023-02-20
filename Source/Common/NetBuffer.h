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
    static constexpr size_t CRYPT_KEYS_COUNT = 50;
    static constexpr size_t STRING_LEN_SIZE = sizeof(uint16);
    static constexpr size_t ARRAY_LEN_SIZE = sizeof(uint16);

    explicit NetBuffer(size_t buf_len);
    NetBuffer(const NetBuffer&) = delete;
    NetBuffer(NetBuffer&&) noexcept = default;
    auto operator=(const NetBuffer&) = delete;
    auto operator=(NetBuffer&&) noexcept -> NetBuffer& = default;
    virtual ~NetBuffer() = default;

    [[nodiscard]] auto IsError() const -> bool;
    [[nodiscard]] auto GetData() -> uint8*;
    [[nodiscard]] auto GetEndPos() const -> size_t;

    void SetError(bool value);
    static auto GenerateEncryptKey() -> uint;
    void SetEncryptKey(uint seed);
    virtual void ResetBuf();
    void GrowBuf(size_t len);

protected:
    auto EncryptKey(int move) -> uint8;
    void CopyBuf(const void* from, void* to, uint8 crypt_key, size_t len);

    bool _isError {};
    unique_ptr<uint8[]> _bufData {};
    size_t _defaultBufLen {};
    size_t _bufLen {};
    size_t _bufEndPos {};
    bool _encryptActive {};
    int _encryptKeyPos {};
    uint8 _encryptKeys[CRYPT_KEYS_COUNT] {};
    bool _nonConstHelper {};
};

class NetOutBuffer final : public NetBuffer
{
public:
    explicit NetOutBuffer(size_t buf_len) :
        NetBuffer(buf_len)
    {
    }
    NetOutBuffer(const NetOutBuffer&) = delete;
    NetOutBuffer(NetOutBuffer&&) noexcept = default;
    auto operator=(const NetOutBuffer&) = delete;
    auto operator=(NetOutBuffer&&) noexcept -> NetOutBuffer& = default;
    ~NetOutBuffer() override = default;

    [[nodiscard]] auto IsEmpty() const -> bool { return _bufEndPos == 0u; }

    void Push(const void* buf, size_t len);
    void Cut(size_t len);

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    auto operator<<(const T& i) -> NetBuffer&
    {
        Push(&i, sizeof(T));
        return *this;
    }

    template<typename T, std::enable_if_t<is_strong_type<T>::value, int> = 0>
    auto operator<<(const T& i) -> NetBuffer&
    {
        Push(&i.underlying_value(), sizeof(typename T::underlying_type));
        return *this;
    }

    auto operator<<(string_view i) -> NetBuffer&
    {
        RUNTIME_ASSERT(i.length() <= std::numeric_limits<uint16>::max());
        const auto len = static_cast<uint16>(i.length());
        Push(&len, sizeof(len));
        Push(i.data(), len);
        return *this;
    }

    auto operator<<(hstring i) -> NetBuffer&
    {
        *this << i.as_hash();
        return *this;
    }

    void StartMsg(uint msg);
    void EndMsg();

private:
    bool _msgStarted {};
    uint _startedMsg {};
    size_t _startedBufPos {};
};

class NetInBuffer final : public NetBuffer
{
public:
    explicit NetInBuffer(size_t buf_len) :
        NetBuffer(buf_len)
    {
    }
    NetInBuffer(const NetInBuffer&) = delete;
    NetInBuffer(NetInBuffer&&) noexcept = default;
    auto operator=(const NetInBuffer&) = delete;
    auto operator=(NetInBuffer&&) noexcept -> NetInBuffer& = default;
    ~NetInBuffer() override = default;

    [[nodiscard]] auto GetReadPos() const -> size_t { return _bufReadPos; }
    [[nodiscard]] auto GetAvailLen() const -> size_t { return _bufLen - _bufEndPos; }
    [[nodiscard]] auto NeedProcess() -> bool;

    void AddData(const void* buf, size_t len);
    void SetEndPos(size_t pos);
    void SkipMsg(uint msg);
    void ShrinkReadBuf();
    void Pop(void* buf, size_t len);
    void ResetBuf() override;

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    auto operator>>(T& i) -> NetBuffer&
    {
        Pop(&i, sizeof(T));
        return *this;
    }

    template<typename T, std::enable_if_t<is_strong_type<T>::value, int> = 0>
    auto operator>>(T& i) -> NetBuffer&
    {
        Pop(&i.underlying_value(), sizeof(typename T::underlying_type));
        return *this;
    }

    auto operator>>(string& i) -> NetBuffer&
    {
        uint16 len = 0;
        Pop(&len, sizeof(len));
        i.resize(len);
        Pop(i.data(), len);
        return *this;
    }

    auto operator>>(hstring& i) -> NetBuffer& = delete;
    [[nodiscard]] auto ReadHashedString(const NameResolver& name_resolver) -> hstring;

private:
    size_t _bufReadPos {};
};
