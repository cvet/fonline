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

#include "NetBuffer.h"

FO_BEGIN_NAMESPACE

static auto NetBufferDataAt(vector<uint8_t>& data, size_t pos) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(pos < data.size(), "Network buffer offset is past the end of the data");

    ptr<uint8_t> data_ptr = data.data();
    ptr<uint8_t> data_pos = data_ptr.get() + pos;
    return data_pos;
}

template<typename T>
    requires(!std::is_const_v<T>)
static auto NetObjectBytes(T& value) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    ptr<T> value_ptr = &value;
    return value_ptr.reinterpret_as<uint8_t>();
}

template<typename T>
static auto NetObjectBytes(const T& value) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    ptr<const T> value_ptr = &value;
    return value_ptr.reinterpret_as<const uint8_t>();
}

static auto NetBytesData(const_span<uint8_t> data) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!data.empty(), "Network buffer data span is empty");

    ptr<const uint8_t> data_ptr = data.data();
    return data_ptr;
}

NetBuffer::NetBuffer(size_t buf_len)
{
    FO_STACK_TRACE_ENTRY();

    _defaultBufLen = buf_len;
    _bufData.resize(buf_len);
}

auto NetBuffer::GetData() noexcept -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (_bufEndPos == 0) {
        return {};
    }

    auto data = NetBufferDataAt(_bufData, 0);
    return {data.get(), _bufEndPos};
}

void NetBuffer::SetEncryptKey(uint32_t seed)
{
    FO_STACK_TRACE_ENTRY();

    if (seed == 0) {
        _encryptActive = false;
        return;
    }

    std::mt19937 rnd_generator {seed};

    for (auto& key : _encryptKeys) {
        key = numeric_cast<uint8_t>(rnd_generator() % 256);
    }

    _encryptKeyPos = 0;
    _encryptActive = true;
}

auto NetBuffer::EncryptKey(int32_t move) noexcept -> uint8_t
{
    FO_STACK_TRACE_ENTRY();

    uint8_t key = 0;

    if (_encryptActive) {
        key = _encryptKeys[_encryptKeyPos];
        _encryptKeyPos += move;

        if (_encryptKeyPos < 0 || _encryptKeyPos >= const_numeric_cast<int32_t>(CRYPT_KEYS_COUNT)) {
            _encryptKeyPos %= const_numeric_cast<int32_t>(CRYPT_KEYS_COUNT);

            if (_encryptKeyPos < 0) {
                _encryptKeyPos += const_numeric_cast<int32_t>(CRYPT_KEYS_COUNT);
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

void NetBuffer::CopyBuf(ptr<const void> from, ptr<void> to, uint8_t crypt_key, size_t len) const noexcept
{
    FO_STACK_TRACE_ENTRY();

    ptr<const uint8_t> from_buf = cast_from_void<const uint8_t*>(from.get());
    ptr<uint8_t> to_buf = cast_from_void<uint8_t*>(to.get());

    for (size_t i = 0; i < len; i++) {
        to_buf[i] = from_buf[i] ^ crypt_key;
    }
}

void NetOutBuffer::Push(nptr<const void> buf, size_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    FO_VERIFY_AND_THROW(buf, "Network buffer push received a null source for a non-empty write");

    GrowBuf(len);
    auto target = NetBufferDataAt(_bufData, _bufEndPos);
    CopyBuf(buf.as_ptr(), target, EncryptKey(numeric_cast<int32_t>(len)), len);
    _bufEndPos += len;
}

void NetOutBuffer::Push(const_span<uint8_t> buf)
{
    FO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return;
    }

    GrowBuf(buf.size());
    auto source = NetBytesData(buf);
    auto target = NetBufferDataAt(_bufData, _bufEndPos);
    CopyBuf(source, target, EncryptKey(numeric_cast<int32_t>(buf.size())), buf.size());
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

    const size_t move_len = _bufEndPos - len;

    if (move_len != 0) {
        auto target = NetBufferDataAt(_bufData, 0);
        auto source = NetBufferDataAt(_bufData, len);
        MemMove(target.get(), source.get(), move_len);
    }

    _bufEndPos -= len;
}

void NetOutBuffer::WritePropsData(const vector<nptr<const uint8_t>>& props_data, const vector<uint32_t>& props_data_sizes)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(props_data.size() == props_data_sizes.size(), "Property payload pointer list and size list have different lengths", props_data.size(), props_data_sizes.size());
    FO_VERIFY_AND_THROW(props_data.size() <= 0xFFFF, "Property payload list is too large for uint16 network encoding", props_data.size(), 0xFFFF);
    Write<uint16_t>(numeric_cast<uint16_t>(props_data.size()));

    for (size_t i = 0; i < props_data.size(); i++) {
        const uint32_t data_size = numeric_cast<uint32_t>(props_data_sizes.at(i));
        Write<uint32_t>(data_size);
        Push(props_data.at(i), data_size);
    }
}

void NetOutBuffer::StartMsg(NetMessage msg)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_msgStarted, "Msg started is already set");

    _msgStarted = true;
    _startedBufPos = _bufEndPos;

    Write(NETMSG_SIGNATURE);

    // Will be overwrited in message finalization
    constexpr uint32_t msg_len = 0;
    Write(msg_len);

    Write(msg);
}

void NetOutBuffer::EndMsg()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_msgStarted, "Network message stream is not started");
    FO_VERIFY_AND_THROW(_bufEndPos > _startedBufPos, "Network message ended without any payload after its start marker", _startedBufPos, _bufEndPos);

    _msgStarted = false;

    // Move to message start position
    const auto msg_len = numeric_cast<uint32_t>(_bufEndPos - _startedBufPos);
    EncryptKey(-numeric_cast<int32_t>(msg_len));

    // Verify signature
    uint32_t msg_signature;
    auto msg_signature_source = NetBufferDataAt(_bufData, _startedBufPos);
    auto msg_signature_target = NetObjectBytes(msg_signature);
    CopyBuf(msg_signature_source, msg_signature_target, EncryptKey(sizeof(msg_signature)), sizeof(msg_signature));
    FO_VERIFY_AND_THROW(msg_signature == NETMSG_SIGNATURE, "Outgoing network message signature was corrupted before finalizing length", msg_signature, NETMSG_SIGNATURE, _startedBufPos, _bufEndPos);

    // Write actual message length
    auto msg_len_source = NetObjectBytes(msg_len);
    auto msg_len_target = NetBufferDataAt(_bufData, _startedBufPos + sizeof(msg_signature));
    CopyBuf(msg_len_source, msg_len_target, EncryptKey(0), sizeof(msg_len));

    // Return to the end
    EncryptKey(numeric_cast<int32_t>(msg_len - sizeof(msg_signature)));
}

void NetOutBuffer::WriteHashedString(hstring value)
{
    FO_STACK_TRACE_ENTRY();

    const auto hash = value.as_hash();
    auto hash_bytes = NetObjectBytes(hash);
    Push(hash_bytes, sizeof(hash));
}

void NetInBuffer::ResetBuf() noexcept
{
    FO_STACK_TRACE_ENTRY();

    NetBuffer::ResetBuf();

    _bufReadPos = 0;
}

void NetInBuffer::AddData(const_span<uint8_t> buf)
{
    FO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return;
    }

    GrowBuf(buf.size());
    auto source = NetBytesData(buf);
    auto target = NetBufferDataAt(_bufData, _bufEndPos);
    CopyBuf(source, target, 0, buf.size());
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

void NetInBuffer::Pop(nptr<void> buf, size_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    if (_bufReadPos + len > _bufEndPos) {
        ResetBuf();
        throw NetBufferException("Invalid read length", len, _bufReadPos, _bufEndPos);
    }

    FO_VERIFY_AND_THROW(buf, "Network buffer pop received a null destination for a non-empty read");

    auto source = NetBufferDataAt(_bufData, _bufReadPos);
    CopyBuf(source, buf.as_ptr(), EncryptKey(numeric_cast<int32_t>(len)), len);
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
        const size_t move_len = _bufEndPos - _bufReadPos;

        auto target = NetBufferDataAt(_bufData, 0);
        auto source = NetBufferDataAt(_bufData, _bufReadPos);
        MemMove(target.get(), source.get(), move_len);

        _bufEndPos -= _bufReadPos;
        _bufReadPos = 0;
    }
}

void NetInBuffer::ReadPropsData(vector<vector<uint8_t>>& props_data)
{
    FO_STACK_TRACE_ENTRY();

    const auto data_count = Read<uint16_t>();

    // Each entry carries at least its uint32 size prefix, so the count can never exceed unread/4; reject before allocating
    if (data_count > GetUnreadSize() / sizeof(uint32_t)) {
        ResetBuf();
        throw NetBufferException("Property data count exceeds remaining buffer", data_count, GetUnreadSize());
    }

    props_data.resize(data_count);

    for (uint16_t i = 0; i < data_count; i++) {
        const auto data_size = Read<uint32_t>();

        // A declared block can never be longer than the bytes still buffered; reject before allocating
        const auto unread = GetUnreadSize();

        if (data_size > unread) {
            ResetBuf();
            throw NetBufferException("Property data size exceeds remaining buffer", data_size, unread);
        }

        props_data[i].resize(data_size);
        nptr<uint8_t> data = props_data[i].data();
        Pop(data, data_size);
    }
}

auto NetInBuffer::ReadMsg() -> NetMessage
{
    FO_STACK_TRACE_ENTRY();

    if (_bufReadPos + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage) > _bufEndPos) {
        throw NetBufferException("Invalid msg read length", _bufReadPos, _bufEndPos);
    }

    uint32_t msg_signature;
    auto msg_signature_source = NetBufferDataAt(_bufData, _bufReadPos);
    auto msg_signature_target = NetObjectBytes(msg_signature);
    CopyBuf(msg_signature_source, msg_signature_target, EncryptKey(sizeof(msg_signature)), sizeof(msg_signature));
    _bufReadPos += sizeof(msg_signature);
    FO_VERIFY_AND_THROW(msg_signature == NETMSG_SIGNATURE, "Incoming network message signature does not match protocol marker", msg_signature, NETMSG_SIGNATURE, _bufReadPos, _bufEndPos);

    uint32_t msg_len;
    auto msg_len_source = NetBufferDataAt(_bufData, _bufReadPos);
    auto msg_len_target = NetObjectBytes(msg_len);
    CopyBuf(msg_len_source, msg_len_target, EncryptKey(sizeof(msg_len)), sizeof(msg_len));
    _bufReadPos += sizeof(msg_len);
    FO_VERIFY_AND_THROW(msg_len >= sizeof(NetMessage) + sizeof(msg_signature) + sizeof(msg_len), "Incoming network message length is smaller than the protocol header", msg_len, sizeof(NetMessage) + sizeof(msg_signature) + sizeof(msg_len));

    NetMessage msg;
    auto msg_source = NetBufferDataAt(_bufData, _bufReadPos);
    auto msg_target = NetObjectBytes(msg);
    CopyBuf(msg_source, msg_target, EncryptKey(sizeof(msg)), sizeof(msg));
    _bufReadPos += sizeof(NetMessage);

    return msg;
}

auto NetInBuffer::ReadHashedString(const HashResolver& hash_resolver) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    const auto hash = Read<hstring::hash_t>();

    bool failed = false;
    const auto result = hash_resolver.ResolveHash(hash, &failed);

    if (failed) {
        ResetBuf();
        throw NetBufferException("Can't resolve received hash", hash);
    }

    return result;
}

auto NetInBuffer::NeedProcess() -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Check signature
    if (_bufReadPos + sizeof(uint32_t) > _bufEndPos) {
        return false;
    }

    uint32_t msg_signature;
    auto msg_signature_source = NetBufferDataAt(_bufData, _bufReadPos);
    auto msg_signature_target = NetObjectBytes(msg_signature);
    CopyBuf(msg_signature_source, msg_signature_target, EncryptKey(0), sizeof(msg_signature));

    if (msg_signature != NETMSG_SIGNATURE) {
        ResetBuf();
        throw UnknownMessageException("Invalid message signature", msg_signature);
    }

    // Check length
    if (_bufReadPos + sizeof(uint32_t) + sizeof(uint32_t) > _bufEndPos) {
        return false;
    }

    uint32_t msg_len;
    EncryptKey(sizeof(msg_signature));
    auto msg_len_source = NetBufferDataAt(_bufData, _bufReadPos + sizeof(msg_signature));
    auto msg_len_target = NetObjectBytes(msg_len);
    CopyBuf(msg_len_source, msg_len_target, EncryptKey(0), sizeof(msg_len));
    EncryptKey(-const_numeric_cast<int32_t>(sizeof(msg_signature)));

    if (msg_len < sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage)) {
        ResetBuf();
        throw UnknownMessageException("Invalid message length", msg_len);
    }

    // Reject an oversized message at the header so the receive buffer never accumulates the whole payload
    if (_maxMsgLen != 0 && msg_len > _maxMsgLen) {
        ResetBuf();
        throw UnknownMessageException("Message length exceeds maximum", msg_len, _maxMsgLen);
    }

    return _bufReadPos + msg_len <= _bufEndPos;
}

FO_END_NAMESPACE
