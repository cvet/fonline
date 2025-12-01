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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(NetBufferException);
FO_DECLARE_EXCEPTION_EXT(UnknownMessageException, NetBufferException);

class NetBuffer
{
public:
    static constexpr size_t CRYPT_KEYS_COUNT = 50;
    static constexpr uint32 NETMSG_SIGNATURE = 0x011E9422;
    static constexpr hstring::hash_t DEBUG_HASH_VALUE = static_cast<hstring::hash_t>(~0);

    explicit NetBuffer(size_t buf_len);
    NetBuffer(const NetBuffer&) = delete;
    NetBuffer(NetBuffer&&) noexcept = default;
    auto operator=(const NetBuffer&) = delete;
    auto operator=(NetBuffer&&) noexcept -> NetBuffer& = default;
    virtual ~NetBuffer() = default;

    [[nodiscard]] auto GetData() noexcept -> span<const uint8> { return {_bufData.data(), _bufEndPos}; }
    [[nodiscard]] auto GetDataSize() const noexcept -> size_t { return _bufEndPos; }

    static auto GenerateEncryptKey() -> uint32;
    void SetEncryptKey(uint32 seed);
    virtual void ResetBuf() noexcept;
    void GrowBuf(size_t len);

protected:
    auto EncryptKey(int32 move) noexcept -> uint8;
    void CopyBuf(const void* from, void* to, uint8 crypt_key, size_t len) const noexcept;

    vector<uint8> _bufData {};
    size_t _defaultBufLen {};
    size_t _bufEndPos {};
    bool _encryptActive {};
    int32 _encryptKeyPos {};
    uint8 _encryptKeys[CRYPT_KEYS_COUNT] {};
};

class NetOutBuffer final : public NetBuffer
{
public:
    explicit NetOutBuffer(size_t buf_len, bool debug_hashes) :
        NetBuffer(buf_len),
        _debugHashes {debug_hashes}
    {
    }
    NetOutBuffer(const NetOutBuffer&) = delete;
    NetOutBuffer(NetOutBuffer&&) noexcept = default;
    auto operator=(const NetOutBuffer&) = delete;
    auto operator=(NetOutBuffer&&) noexcept -> NetOutBuffer& = default;
    ~NetOutBuffer() override = default;

    [[nodiscard]] auto IsEmpty() const noexcept -> bool { return _bufEndPos == 0; }

    void Push(span<const uint8> buf);
    void Push(const void* buf, size_t len);
    void DiscardWriteBuf(size_t len);

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
    void Write(T value)
    {
        Push(&value, sizeof(T));
    }

    template<typename T>
        requires(std::same_as<T, string_view> || std::same_as<T, string>)
    void Write(const T& value)
    {
        const auto len = numeric_cast<uint32>(value.length());
        Push(&len, sizeof(len));
        Push(value.data(), len);
    }

    template<typename T>
        requires(std::same_as<T, hstring>)
    void Write(T value)
    {
        WriteHashedString(value);
    }

    void WritePropsData(vector<const uint8*>* props_data, const vector<uint32>* props_data_sizes);

    void StartMsg(NetMessage msg);
    void EndMsg();

private:
    void WriteHashedString(hstring value);

    bool _debugHashes;
    bool _msgStarted {};
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

    [[nodiscard]] auto GetReadPos() const noexcept -> size_t { return _bufReadPos; }
    [[nodiscard]] auto NeedProcess() -> bool;

    void AddData(span<const uint8> buf);
    void SetEndPos(size_t pos);
    void ShrinkReadBuf();
    void Pop(void* buf, size_t len);
    void ResetBuf() noexcept override;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
    [[nodiscard]] auto Read() -> T
    {
        T result = {};
        Pop(&result, sizeof(T));
        return result;
    }

    template<typename T>
        requires(std::same_as<T, string>)
    [[nodiscard]] auto Read() -> string
    {
        string result;
        uint32 len = 0;
        Pop(&len, sizeof(len));
        result.resize(len);
        Pop(result.data(), len);
        return result;
    }

    template<typename T>
        requires(std::same_as<T, hstring>)
    [[nodiscard]] auto Read(const HashResolver& hash_resolver) -> hstring
    {
        return ReadHashedString(hash_resolver);
    }

    void ReadPropsData(vector<vector<uint8>>& props_data);

    auto ReadMsg() -> NetMessage;

private:
    [[nodiscard]] auto ReadHashedString(const HashResolver& hash_resolver) -> hstring;

    size_t _bufReadPos {};
};

FO_END_NAMESPACE();
