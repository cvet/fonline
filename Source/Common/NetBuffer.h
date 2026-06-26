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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(NetBufferException);
FO_DECLARE_EXCEPTION_EXT(UnknownMessageException, NetBufferException);

class NetBuffer
{
public:
    static constexpr size_t CRYPT_KEYS_COUNT = 50;
    static constexpr uint32_t NETMSG_SIGNATURE = 0x011E9422;

    explicit NetBuffer(size_t buf_len);
    NetBuffer(const NetBuffer&) = delete;
    NetBuffer(NetBuffer&&) noexcept = default;
    auto operator=(const NetBuffer&) = delete;
    auto operator=(NetBuffer&&) noexcept -> NetBuffer& = default;
    virtual ~NetBuffer() = default;

    [[nodiscard]] auto GetData() noexcept -> const_span<uint8_t>;
    [[nodiscard]] auto GetDataSize() const noexcept -> size_t { return _bufEndPos; }

    void SetEncryptKey(uint32_t seed);
    virtual void ResetBuf() noexcept;
    void GrowBuf(size_t len);

protected:
    auto EncryptKey(int32_t move) noexcept -> uint8_t;
    void CopyBuf(ptr<const void> from, ptr<void> to, uint8_t crypt_key, size_t len) const noexcept;

    vector<uint8_t> _bufData {};
    size_t _defaultBufLen {};
    size_t _bufEndPos {};
    bool _encryptActive {};
    int32_t _encryptKeyPos {};
    uint8_t _encryptKeys[CRYPT_KEYS_COUNT] {};
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

    [[nodiscard]] auto IsEmpty() const noexcept -> bool { return _bufEndPos == 0; }
    [[nodiscard]] auto IsMsgStarted() const noexcept -> bool { return _msgStarted; }

    void Push(const_span<uint8_t> buf);
    void Push(nptr<const void> buf, size_t len);
    void DiscardWriteBuf(size_t len);

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
    void Write(T value)
    {
        ptr<const T> value_ptr = &value;
        Push(value_ptr, sizeof(T));
    }

    template<typename T>
        requires(std::same_as<T, string_view> || std::same_as<T, string>)
    void Write(const T& value)
    {
        const auto len = numeric_cast<uint32_t>(value.length());
        ptr<const uint32_t> len_ptr = &len;
        nptr<const typename T::value_type> value_data = value.data();

        Push(len_ptr, sizeof(len));
        Push(value_data, len);
    }

    template<typename T>
        requires(std::same_as<T, hstring>)
    void Write(T value)
    {
        WriteHashedString(value);
    }

    void WritePropsData(const vector<nptr<const uint8_t>>& props_data, const vector<uint32_t>& props_data_sizes);

    void StartMsg(NetMessage msg);
    void EndMsg();

private:
    void WriteHashedString(hstring value);

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
    [[nodiscard]] auto GetUnreadSize() const noexcept -> size_t { return _bufEndPos - _bufReadPos; }
    [[nodiscard]] auto NeedProcess() -> bool;

    void SetMaxMsgLen(size_t len) noexcept { _maxMsgLen = len; }
    void AddData(const_span<uint8_t> buf);
    void SetEndPos(size_t pos);
    void ShrinkReadBuf();
    void Pop(nptr<void> buf, size_t len);
    void ResetBuf() noexcept override;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
    [[nodiscard]] auto Read() -> T
    {
        T result = {};
        ptr<T> result_ptr = &result;
        Pop(result_ptr, sizeof(T));
        return result;
    }

    template<typename T>
        requires(std::same_as<T, string>)
    [[nodiscard]] auto Read() -> string
    {
        string result;
        uint32_t len = 0;
        ptr<uint32_t> len_ptr = &len;
        Pop(len_ptr, sizeof(len));

        // A declared string can never be longer than the bytes still buffered; reject before allocating
        const auto unread = GetUnreadSize();

        if (len > unread) {
            ResetBuf();
            throw NetBufferException("String length exceeds remaining buffer", len, unread);
        }

        result.resize(len);
        ptr<char> result_data = result.data();
        Pop(result_data, len);
        return result;
    }

    template<typename T>
        requires(std::same_as<T, hstring>)
    [[nodiscard]] auto Read(const HashResolver& hash_resolver) -> hstring
    {
        return ReadHashedString(hash_resolver);
    }

    void ReadPropsData(vector<vector<uint8_t>>& props_data);

    auto ReadMsg() -> NetMessage;

private:
    [[nodiscard]] auto ReadHashedString(const HashResolver& hash_resolver) -> hstring;

    size_t _bufReadPos {};
    size_t _maxMsgLen {};
};

FO_END_NAMESPACE
