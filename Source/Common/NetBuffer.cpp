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

#include "NetBuffer.h"

FO_BEGIN_NAMESPACE();

NetBuffer::NetBuffer(size_t buf_len)
{
    FO_STACK_TRACE_ENTRY();

    _defaultBufLen = buf_len;
    _bufData.resize(buf_len);
}

auto NetBuffer::GenerateEncryptKey() -> uint32
{
    FO_STACK_TRACE_ENTRY();

    return // Random 4 byte
        (static_cast<uint32>(GenericUtils::Random(1, 255)) << 24) | //
        (static_cast<uint32>(GenericUtils::Random(1, 255)) << 16) | //
        (static_cast<uint32>(GenericUtils::Random(1, 255)) << 8) | //
        (static_cast<uint32>(GenericUtils::Random(1, 255)) << 0);
}

void NetBuffer::SetEncryptKey(uint32 seed)
{
    FO_STACK_TRACE_ENTRY();

    if (seed == 0) {
        _encryptActive = false;
        return;
    }

    std::mt19937 rnd_generator {seed};

    for (auto& key : _encryptKeys) {
        key = numeric_cast<uint8>(rnd_generator() % 256);
    }

    _encryptKeyPos = 0;
    _encryptActive = true;
}

auto NetBuffer::EncryptKey(int32 move) noexcept -> uint8
{
    FO_STACK_TRACE_ENTRY();

    uint8 key = 0;

    if (_encryptActive) {
        key = _encryptKeys[_encryptKeyPos];
        _encryptKeyPos += move;

        if (_encryptKeyPos < 0 || _encryptKeyPos >= const_numeric_cast<int32>(CRYPT_KEYS_COUNT)) {
            _encryptKeyPos %= const_numeric_cast<int32>(CRYPT_KEYS_COUNT);

            if (_encryptKeyPos < 0) {
                _encryptKeyPos += const_numeric_cast<int32>(CRYPT_KEYS_COUNT);
            }
        }
    }

    return key;
}

void NetBuffer::ResetBuf() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _bufEndPos = 0;

    if (_bufData.size() > _defaultBufLen) {
        _bufData.resize(_defaultBufLen);
        _bufData.shrink_to_fit();
    }
}

void NetBuffer::GrowBuf(size_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (_bufEndPos + len <= _bufData.size()) {
        return;
    }

    auto new_len = _bufData.size();

    while (_bufEndPos + len > new_len) {
        new_len *= 2;
    }

    _bufData.resize(new_len);
}

void NetBuffer::CopyBuf(const void* from, void* to, uint8 crypt_key, size_t len) const noexcept
{
    FO_STACK_TRACE_ENTRY();

    const auto* from_ = static_cast<const uint8*>(from);
    auto* to_ = static_cast<uint8*>(to);

    for (size_t i = 0; i < len; i++, to_++, from_++) {
        *to_ = *from_ ^ crypt_key;
    }
}

void NetOutBuffer::Push(const void* buf, size_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    GrowBuf(len);
    CopyBuf(buf, _bufData.data() + _bufEndPos, EncryptKey(numeric_cast<int32>(len)), len);
    _bufEndPos += len;
}

void NetOutBuffer::Push(span<const uint8> buf)
{
    FO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return;
    }

    GrowBuf(buf.size());
    CopyBuf(buf.data(), _bufData.data() + _bufEndPos, EncryptKey(numeric_cast<int32>(buf.size())), buf.size());
    _bufEndPos += buf.size();
}

void NetOutBuffer::DiscardWriteBuf(size_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    if (len > _bufEndPos) {
        ResetBuf();
        throw NetBufferException("Invalid discard length", len, _bufEndPos);
    }

    auto* buf = _bufData.data();

    for (size_t i = 0; i + len < _bufEndPos; i++) {
        buf[i] = buf[i + len];
    }

    _bufEndPos -= len;
}

void NetOutBuffer::WritePropsData(vector<const uint8*>* props_data, const vector<uint32>* props_data_sizes)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(props_data->size() == props_data_sizes->size());
    FO_RUNTIME_ASSERT(props_data->size() <= 0xFFFF);
    Write<uint16>(numeric_cast<uint16>(props_data->size()));

    for (size_t i = 0; i < props_data->size(); i++) {
        const auto data_size = numeric_cast<uint32>(props_data_sizes->at(i));
        Write<uint32>(data_size);
        Push({props_data->at(i), data_size});
    }
}

void NetOutBuffer::StartMsg(NetMessage msg)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_msgStarted);

    _msgStarted = true;
    _startedBufPos = _bufEndPos;

    Write(NETMSG_SIGNATURE);

    // Will be overwrited in message finalization
    constexpr uint32 msg_len = 0;
    Write(msg_len);

    Write(msg);
}

void NetOutBuffer::EndMsg()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_msgStarted);
    FO_RUNTIME_ASSERT(_bufEndPos > _startedBufPos);

    _msgStarted = false;

    // Move to message start position
    const auto msg_len = numeric_cast<uint32>(_bufEndPos - _startedBufPos);
    EncryptKey(-numeric_cast<int32>(msg_len));

    // Verify signature
    uint32 msg_signature;
    CopyBuf(_bufData.data() + _startedBufPos, &msg_signature, EncryptKey(sizeof(msg_signature)), sizeof(msg_signature));
    FO_RUNTIME_ASSERT(msg_signature == NETMSG_SIGNATURE);

    // Write actual message length
    CopyBuf(&msg_len, _bufData.data() + _startedBufPos + sizeof(msg_signature), EncryptKey(0), sizeof(msg_len));

    // Return to the end
    EncryptKey(numeric_cast<int32>(msg_len - sizeof(msg_signature)));
}

void NetOutBuffer::WriteHashedString(hstring value)
{
    FO_STACK_TRACE_ENTRY();

    if (_debugHashes) {
        Push(&DEBUG_HASH_VALUE, sizeof(DEBUG_HASH_VALUE));
        Write(value.as_str());
    }

    const auto hash = value.as_hash();
    Push(&hash, sizeof(hash));
}

void NetInBuffer::ResetBuf() noexcept
{
    FO_STACK_TRACE_ENTRY();

    NetBuffer::ResetBuf();

    _bufReadPos = 0;
}

void NetInBuffer::AddData(span<const uint8> buf)
{
    FO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return;
    }

    GrowBuf(buf.size());
    CopyBuf(buf.data(), _bufData.data() + _bufEndPos, 0, buf.size());
    _bufEndPos += buf.size();
}

void NetInBuffer::SetEndPos(size_t pos)
{
    FO_STACK_TRACE_ENTRY();

    if (pos > _bufData.size()) {
        throw NetBufferException("Invalid set end pos", pos, _bufData.size(), _bufEndPos);
    }

    _bufEndPos = pos;
}

void NetInBuffer::Pop(void* buf, size_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    if (_bufReadPos + len > _bufEndPos) {
        ResetBuf();
        throw NetBufferException("Invalid read length", len, _bufReadPos, _bufEndPos);
    }

    CopyBuf(_bufData.data() + _bufReadPos, buf, EncryptKey(numeric_cast<int32>(len)), len);
    _bufReadPos += len;
}

void NetInBuffer::ShrinkReadBuf()
{
    FO_STACK_TRACE_ENTRY();

    if (_bufReadPos > _bufEndPos) {
        ResetBuf();
        throw NetBufferException("Invalid shrink pos", _bufReadPos, _bufEndPos);
    }

    if (_bufReadPos >= _bufEndPos) {
        if (_bufReadPos != 0) {
            ResetBuf();
        }
    }
    else if (_bufReadPos != 0) {
        for (size_t i = _bufReadPos; i < _bufEndPos; i++) {
            _bufData[i - _bufReadPos] = _bufData[i];
        }

        _bufEndPos -= _bufReadPos;
        _bufReadPos = 0;
    }
}

void NetInBuffer::ReadPropsData(vector<vector<uint8>>& props_data)
{
    FO_STACK_TRACE_ENTRY();

    const auto data_count = Read<uint16>();
    props_data.resize(data_count);

    for (uint16 i = 0; i < data_count; i++) {
        const auto data_size = Read<uint32>();
        props_data[i].resize(data_size);
        Pop(props_data[i].data(), data_size);
    }
}

auto NetInBuffer::ReadMsg() -> NetMessage
{
    FO_STACK_TRACE_ENTRY();

    if (_bufReadPos + sizeof(uint32) + sizeof(uint32) + sizeof(NetMessage) > _bufEndPos) {
        throw NetBufferException("Invalid msg read length", _bufReadPos, _bufEndPos);
    }

    uint32 msg_signature;
    CopyBuf(_bufData.data() + _bufReadPos, &msg_signature, EncryptKey(sizeof(msg_signature)), sizeof(msg_signature));
    _bufReadPos += sizeof(msg_signature);
    FO_RUNTIME_ASSERT(msg_signature == NETMSG_SIGNATURE);

    uint32 msg_len;
    CopyBuf(_bufData.data() + _bufReadPos, &msg_len, EncryptKey(sizeof(msg_len)), sizeof(msg_len));
    _bufReadPos += sizeof(msg_len);
    FO_RUNTIME_ASSERT(msg_len >= sizeof(NetMessage) + sizeof(msg_signature) + sizeof(msg_len));

    NetMessage msg;
    CopyBuf(_bufData.data() + _bufReadPos, &msg, EncryptKey(sizeof(msg)), sizeof(msg));
    _bufReadPos += sizeof(NetMessage);

    return msg;
}

auto NetInBuffer::ReadHashedString(const HashResolver& hash_resolver) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    auto hash = Read<hstring::hash_t>();

    if (hash == DEBUG_HASH_VALUE) {
        const auto real_value = Read<string>();
        hash = Read<hstring::hash_t>();

        bool failed = false;
        const auto result = hash_resolver.ResolveHash(hash, &failed);

        if (failed) {
            ResetBuf();
            throw NetBufferException("Can't resolve received hash", hash, real_value);
        }

        FO_RUNTIME_ASSERT(result.as_str() == real_value);
        return result;
    }
    else {
        bool failed = false;
        const auto result = hash_resolver.ResolveHash(hash, &failed);

        if (failed) {
            ResetBuf();
            throw NetBufferException("Can't resolve received hash", hash);
        }

        return result;
    }
}

auto NetInBuffer::NeedProcess() -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Check signature
    if (_bufReadPos + sizeof(uint32) > _bufEndPos) {
        return false;
    }

    uint32 msg_signature;
    CopyBuf(_bufData.data() + _bufReadPos, &msg_signature, EncryptKey(0), sizeof(msg_signature));

    if (msg_signature != NETMSG_SIGNATURE) {
        ResetBuf();
        throw UnknownMessageException("Invalid message signature", msg_signature);
    }

    // Check length
    if (_bufReadPos + sizeof(uint32) + sizeof(uint32) > _bufEndPos) {
        return false;
    }

    uint32 msg_len;
    EncryptKey(sizeof(msg_signature));
    CopyBuf(_bufData.data() + _bufReadPos + sizeof(msg_signature), &msg_len, EncryptKey(0), sizeof(msg_len));
    EncryptKey(-const_numeric_cast<int32>(sizeof(msg_signature)));

    if (msg_len < sizeof(uint32) + sizeof(uint32) + sizeof(NetMessage)) {
        ResetBuf();
        throw UnknownMessageException("Invalid message length", msg_len);
    }

    return _bufReadPos + msg_len <= _bufEndPos;
}

FO_END_NAMESPACE();
