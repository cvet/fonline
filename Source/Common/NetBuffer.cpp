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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

NetBuffer::NetBuffer(size_t buf_len)
{
    _defaultBufLen = buf_len;
    _bufData.resize(buf_len);
}

auto NetBuffer::GetData() noexcept -> const_span<uint8_t>
{
    if (_bufEndPos == 0) {
        return {};
    }

    auto data = make_ptr(_bufData.data());
    return {data.get(), _bufEndPos};
}

void NetBuffer::ResetBuf() noexcept
{
    _bufEndPos = 0;

    if (_bufData.size() > _defaultBufLen) {
        _bufData.resize(_defaultBufLen);
        _bufData.shrink_to_fit();
    }
}

void NetBuffer::GrowBuf(size_t len)
{
    if (_bufEndPos + len <= _bufData.size()) {
        return;
    }

    auto new_len = _bufData.size();

    while (_bufEndPos + len > new_len) {
        new_len *= 2;
    }

    _bufData.resize(new_len);
}

void NetOutBuffer::Push(nptr<const void> buf, size_t len)
{
    if (len == 0) {
        return;
    }

    FO_VERIFY_AND_THROW(buf, "Network buffer push received a null source for a non-empty write");

    GrowBuf(len);
    auto target = make_ptr(_bufData.data()).offset(_bufEndPos);
    memory::copy(target, buf, len);
    _bufEndPos += len;
}

void NetOutBuffer::Push(const_span<uint8_t> buf)
{
    if (buf.empty()) {
        return;
    }

    GrowBuf(buf.size());
    auto target = make_ptr(_bufData.data()).offset(_bufEndPos);
    memory::copy(target, buf.data(), buf.size());
    _bufEndPos += buf.size();
}

void NetOutBuffer::DiscardWriteBuf(size_t len)
{
    if (len == 0) {
        return;
    }

    if (len > _bufEndPos) {
        ResetBuf();
        throw NetBufferException("Invalid discard length", len, _bufEndPos);
    }

    size_t move_len = _bufEndPos - len;

    if (move_len != 0) {
        auto target = make_ptr(_bufData.data());
        auto source = make_ptr(_bufData.data()).offset(len);
        memory::move(target, source, move_len);
    }

    _bufEndPos -= len;
}

void NetOutBuffer::WritePropsData(const vector<nptr<const uint8_t>>& props_data, const vector<uint32_t>& props_data_sizes)
{
    FO_VERIFY_AND_THROW(props_data.size() == props_data_sizes.size(), "Property payload pointer list and size list have different lengths", props_data.size(), props_data_sizes.size());
    FO_VERIFY_AND_THROW(props_data.size() <= 0xFFFF, "Property payload list is too large for uint16 network encoding", props_data.size(), 0xFFFF);
    Write<uint16_t>(numeric_cast<uint16_t>(props_data.size()));

    for (size_t i = 0; i < props_data.size(); i++) {
        uint32_t data_size = numeric_cast<uint32_t>(props_data_sizes.at(i));
        Write<uint32_t>(data_size);
        Push(props_data.at(i), data_size);
    }
}

void NetOutBuffer::StartMsg(NetMessage msg)
{
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
    FO_VERIFY_AND_THROW(_msgStarted, "Network message stream is not started");
    FO_VERIFY_AND_THROW(_bufEndPos > _startedBufPos, "Network message ended without any payload after its start marker", _startedBufPos, _bufEndPos);

    _msgStarted = false;

    auto msg_len = numeric_cast<uint32_t>(_bufEndPos - _startedBufPos);

    uint32_t msg_signature;
    auto msg_signature_source = make_ptr(_bufData.data()).offset(_startedBufPos);
    memory::copy(&msg_signature, msg_signature_source, sizeof(msg_signature));
    FO_STRONG_ASSERT(msg_signature == NETMSG_SIGNATURE, "Outgoing network message signature was corrupted before finalizing length", msg_signature, NETMSG_SIGNATURE, _startedBufPos, _bufEndPos);

    auto msg_len_target = make_ptr(_bufData.data()).offset(_startedBufPos + sizeof(msg_signature));
    memory::copy(msg_len_target, &msg_len, sizeof(msg_len));
}

void NetOutBuffer::WriteHashedString(hstring value)
{
    auto hash = value.as_hash();
    auto hash_bytes = make_ptr(&hash).reinterpret_as<uint8_t>();
    Push(hash_bytes, sizeof(hash));
}

void NetInBuffer::ResetBuf() noexcept
{
    NetBuffer::ResetBuf();

    _bufReadPos = 0;
    _msgEndPos = 0;
}

void NetInBuffer::AddData(const_span<uint8_t> buf)
{
    if (buf.empty()) {
        return;
    }

    size_t buffered_unread = GetBufferedUnreadSize();

    if (_maxBufLen != 0 && (buf.size() > _maxBufLen || buffered_unread > _maxBufLen - buf.size())) {
        ResetBuf();
        throw NetBufferException("Network receive buffer exceeds maximum", buffered_unread, buf.size(), _maxBufLen);
    }

    GrowBuf(buf.size());
    auto target = make_ptr(_bufData.data()).offset(_bufEndPos);
    memory::copy(target, buf.data(), buf.size());
    _bufEndPos += buf.size();
}

void NetInBuffer::SetEndPos(size_t pos)
{
    if (pos > _bufData.size() || (_msgEndPos != 0 && pos < _msgEndPos)) {
        throw NetBufferException("Invalid set end pos", pos, _bufData.size(), _bufEndPos);
    }

    _bufEndPos = pos;
}

void NetInBuffer::Pop(nptr<void> buf, size_t len)
{
    if (len == 0) {
        return;
    }

    size_t read_limit = GetReadLimit();

    if (_bufReadPos > read_limit || len > read_limit - _bufReadPos) {
        ResetBuf();
        throw NetBufferException("Invalid read length", len, _bufReadPos, read_limit, _bufEndPos);
    }

    FO_VERIFY_AND_THROW(buf, "Network buffer pop received a null destination for a non-empty read");

    auto source = make_ptr(_bufData.data()).offset(_bufReadPos);
    memory::copy(buf, source, len);
    _bufReadPos += len;
}

void NetInBuffer::ShrinkReadBuf()
{
    FinishMessageRead();

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
        size_t move_len = _bufEndPos - _bufReadPos;

        auto target = make_ptr(_bufData.data());
        auto source = make_ptr(_bufData.data()).offset(_bufReadPos);
        memory::move(target, source, move_len);

        _bufEndPos -= _bufReadPos;
        _bufReadPos = 0;
    }
}

void NetInBuffer::ReadPropsData(vector<vector<uint8_t>>& props_data)
{
    auto data_count = Read<uint16_t>();

    // Each entry carries at least its uint32 size prefix, so the count can never exceed unread/4; reject before allocating
    if (data_count > GetUnreadSize() / sizeof(uint32_t)) {
        ResetBuf();
        throw NetBufferException("Property data count exceeds remaining buffer", data_count, GetUnreadSize());
    }

    props_data.resize(data_count);

    for (uint16_t i = 0; i < data_count; i++) {
        auto data_size = Read<uint32_t>();

        // A declared block can never be longer than the bytes still buffered; reject before allocating
        size_t unread = GetUnreadSize();

        if (data_size > unread) {
            ResetBuf();
            throw NetBufferException("Property data size exceeds remaining buffer", data_size, unread);
        }

        props_data[i].resize(data_size);
        Pop(props_data[i].data(), data_size);
    }
}

auto NetInBuffer::ReadMsg() -> NetMessage
{
    FO_VERIFY_AND_THROW(_msgEndPos == 0, "Network message read started before the previous frame was completed", _bufReadPos, _msgEndPos, _bufEndPos);

    constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage);

    if (_bufReadPos > _bufEndPos || header_size > _bufEndPos - _bufReadPos) {
        throw NetBufferException("Invalid msg read length", _bufReadPos, _bufEndPos);
    }

    size_t msg_start_pos = _bufReadPos;

    uint32_t msg_signature;
    auto msg_signature_source = make_ptr(_bufData.data()).offset(_bufReadPos);
    memory::copy(&msg_signature, msg_signature_source, sizeof(msg_signature));
    _bufReadPos += sizeof(msg_signature);
    FO_VERIFY_AND_THROW(msg_signature == NETMSG_SIGNATURE, "Incoming network message signature does not match protocol marker", msg_signature, NETMSG_SIGNATURE, _bufReadPos, _bufEndPos);

    uint32_t msg_len;
    auto msg_len_source = make_ptr(_bufData.data()).offset(_bufReadPos);
    memory::copy(&msg_len, msg_len_source, sizeof(msg_len));
    _bufReadPos += sizeof(msg_len);
    FO_VERIFY_AND_THROW(msg_len >= sizeof(NetMessage) + sizeof(msg_signature) + sizeof(msg_len), "Incoming network message length is smaller than the protocol header", msg_len, sizeof(NetMessage) + sizeof(msg_signature) + sizeof(msg_len));

    if (msg_len > _bufEndPos - msg_start_pos) {
        ResetBuf();
        throw NetBufferException("Incoming network message extends past buffered data", msg_len, msg_start_pos, _bufEndPos);
    }

    _msgEndPos = msg_start_pos + msg_len;

    NetMessage msg;
    auto msg_source = make_ptr(_bufData.data()).offset(_bufReadPos);
    memory::copy(&msg, msg_source, sizeof(msg));
    _bufReadPos += sizeof(NetMessage);

    return msg;
}

auto NetInBuffer::ReadHashedString(const hash_resolver& hashes) -> hstring
{
    auto hash = Read<hstring::hash_t>();

    bool failed = false;
    hstring result = hashes.resolve_hash(hash, &failed);

    if (failed) {
        ResetBuf();
        throw NetBufferException("Can't resolve received hash", hash);
    }

    return result;
}

auto NetInBuffer::NeedProcess() -> bool
{
    FinishMessageRead();

    // Check signature
    if (_bufReadPos + sizeof(uint32_t) > _bufEndPos) {
        return false;
    }

    uint32_t msg_signature;
    auto msg_signature_source = make_ptr(_bufData.data()).offset(_bufReadPos);
    memory::copy(&msg_signature, msg_signature_source, sizeof(msg_signature));

    if (msg_signature != NETMSG_SIGNATURE) {
        ResetBuf();
        throw UnknownMessageException("Invalid message signature", msg_signature);
    }

    // Check length
    if (_bufReadPos + sizeof(uint32_t) + sizeof(uint32_t) > _bufEndPos) {
        return false;
    }

    uint32_t msg_len;
    auto msg_len_source = make_ptr(_bufData.data()).offset(_bufReadPos + sizeof(msg_signature));
    memory::copy(&msg_len, msg_len_source, sizeof(msg_len));

    if (msg_len < sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage)) {
        ResetBuf();
        throw UnknownMessageException("Invalid message length", msg_len);
    }

    // Reject an oversized message at the header so the receive buffer never accumulates the whole payload
    if (_maxMsgLen != 0 && msg_len > _maxMsgLen) {
        ResetBuf();
        throw UnknownMessageException("Message length exceeds maximum", msg_len, _maxMsgLen);
    }

    return msg_len <= _bufEndPos - _bufReadPos;
}

void NetInBuffer::FinishMessageRead()
{
    if (_msgEndPos == 0) {
        return;
    }

    if (_bufReadPos != _msgEndPos) {
        size_t read_pos = _bufReadPos;
        size_t msg_end_pos = _msgEndPos;
        size_t buf_end_pos = _bufEndPos;
        ResetBuf();
        throw NetBufferException("Network message payload was not consumed exactly", read_pos, msg_end_pos, buf_end_pos);
    }

    _msgEndPos = 0;
}

FO_END_NAMESPACE
