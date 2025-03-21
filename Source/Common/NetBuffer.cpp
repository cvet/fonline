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
#include "Entity.h"
#include "GenericUtils.h"

static auto GetMsgSize(uint msg) -> uint
{
    STACK_TRACE_ENTRY();

    switch (msg) {
    case 0xFFFFFFFF:
        return 16;
    case NETMSG_DISCONNECT:
        return NETMSG_DISCONNECT_SIZE;
    case NETMSG_WRONG_NET_PROTO:
        return NETMSG_WRONG_NET_PROTO_SIZE;
    case NETMSG_REGISTER_SUCCESS:
        return NETMSG_REGISTER_SUCCESS_SIZE;
    case NETMSG_PING:
        return NETMSG_PING_SIZE;
    case NETMSG_PLACE_TO_GAME_COMPLETE:
        return NETMSG_PLACE_TO_GAME_COMPLETE_SIZE;
    case NETMSG_HANDSHAKE:
        return NETMSG_HANDSHAKE_SIZE;
    case NETMSG_GET_UPDATE_FILE:
        return NETMSG_GET_UPDATE_FILE_SIZE;
    case NETMSG_GET_UPDATE_FILE_DATA:
        return NETMSG_GET_UPDATE_FILE_DATA_SIZE;
    case NETMSG_REMOVE_CRITTER:
        return NETMSG_REMOVE_CRITTER_SIZE;
    case NETMSG_MSG:
        return NETMSG_MSG_SIZE;
    case NETMSG_MAP_TEXT_MSG:
        return NETMSG_MAP_TEXT_MSG_SIZE;
    case NETMSG_DIR:
        return NETMSG_DIR_SIZE;
    case NETMSG_CRITTER_DIR:
        return NETMSG_CRITTER_DIR_SIZE;
    case NETMSG_CRITTER_MOVE_SPEED:
        return NETMSG_CRITTER_MOVE_SPEED_SIZE;
    case NETMSG_SEND_STOP_MOVE:
        return NETMSG_SEND_STOP_MOVE_SIZE;
    case NETMSG_CRITTER_POS:
        return NETMSG_CRITTER_POS_SIZE;
    case NETMSG_CRITTER_TELEPORT:
        return NETMSG_CRITTER_TELEPORT_SIZE;
    case NETMSG_CHOSEN_REMOVE_ITEM:
        return NETMSG_CHOSEN_REMOVE_ITEM_SIZE;
    case NETMSG_REMOVE_ITEM_FROM_MAP:
        return NETMSG_REMOVE_ITEM_FROM_MAP_SIZE;
    case NETMSG_ANIMATE_ITEM:
        return NETMSG_ANIMATE_ITEM_SIZE;
    case NETMSG_CRITTER_SET_ANIMS:
        return NETMSG_CRITTER_SET_ANIMS_SIZE;
    case NETMSG_EFFECT:
        return NETMSG_EFFECT_SIZE;
    case NETMSG_FLY_EFFECT:
        return NETMSG_FLY_EFFECT_SIZE;
    case NETMSG_SEND_TALK_NPC:
        return NETMSG_SEND_TALK_NPC_SIZE;
    case NETMSG_TIME_SYNC:
        return NETMSG_TIME_SYNC_SIZE;
    case NETMSG_VIEW_MAP:
        return NETMSG_VIEW_MAP_SIZE;
    case NETMSG_REMOVE_CUSTOM_ENTITY:
        return NETMSG_REMOVE_CUSTOM_ENTITY_SIZE;
    case NETMSG_SEND_POD_PROPERTY(1, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(1, 0);
    case NETMSG_SEND_POD_PROPERTY(2, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(2, 0);
    case NETMSG_SEND_POD_PROPERTY(4, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(4, 0);
    case NETMSG_SEND_POD_PROPERTY(8, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(8, 0);
    case NETMSG_SEND_POD_PROPERTY(1, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(1, 1);
    case NETMSG_SEND_POD_PROPERTY(2, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(2, 1);
    case NETMSG_SEND_POD_PROPERTY(4, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(4, 1);
    case NETMSG_SEND_POD_PROPERTY(8, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(8, 1);
    case NETMSG_SEND_POD_PROPERTY(1, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(1, 2);
    case NETMSG_SEND_POD_PROPERTY(2, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(2, 2);
    case NETMSG_SEND_POD_PROPERTY(4, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(4, 2);
    case NETMSG_SEND_POD_PROPERTY(8, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(8, 2);
    case NETMSG_POD_PROPERTY(1, 0):
        return NETMSG_POD_PROPERTY_SIZE(1, 0);
    case NETMSG_POD_PROPERTY(2, 0):
        return NETMSG_POD_PROPERTY_SIZE(2, 0);
    case NETMSG_POD_PROPERTY(4, 0):
        return NETMSG_POD_PROPERTY_SIZE(4, 0);
    case NETMSG_POD_PROPERTY(8, 0):
        return NETMSG_POD_PROPERTY_SIZE(8, 0);
    case NETMSG_POD_PROPERTY(1, 1):
        return NETMSG_POD_PROPERTY_SIZE(1, 1);
    case NETMSG_POD_PROPERTY(2, 1):
        return NETMSG_POD_PROPERTY_SIZE(2, 1);
    case NETMSG_POD_PROPERTY(4, 1):
        return NETMSG_POD_PROPERTY_SIZE(4, 1);
    case NETMSG_POD_PROPERTY(8, 1):
        return NETMSG_POD_PROPERTY_SIZE(8, 1);
    case NETMSG_POD_PROPERTY(1, 2):
        return NETMSG_POD_PROPERTY_SIZE(1, 2);
    case NETMSG_POD_PROPERTY(2, 2):
        return NETMSG_POD_PROPERTY_SIZE(2, 2);
    case NETMSG_POD_PROPERTY(4, 2):
        return NETMSG_POD_PROPERTY_SIZE(4, 2);
    case NETMSG_POD_PROPERTY(8, 2):
        return NETMSG_POD_PROPERTY_SIZE(8, 2);

    case NETMSG_UPDATE_FILE_DATA:
    case NETMSG_LOGIN:
    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOAD_MAP:
    case NETMSG_REGISTER:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_CRITTER:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_CHOSEN_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEMS:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_PLAY_SOUND:
    case NETMSG_TALK_NPC:
    case NETMSG_REMOTE_CALL:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_SEND_MOVE:
    case NETMSG_CRITTER_MOVE:
    case NETMSG_CRITTER_ATTACHMENTS:
    case NETMSG_ADD_CUSTOM_ENTITY:
    case NETMSG_CRITTER_ACTION:
    case NETMSG_CRITTER_ANIMATE:
        return static_cast<uint>(-1);
    default:
        break;
    }

    BreakIntoDebugger();
    return 0;
}

NetBuffer::NetBuffer(size_t buf_len)
{
    STACK_TRACE_ENTRY();

    _defaultBufLen = buf_len;
    _bufData.resize(buf_len);
}

auto NetBuffer::GenerateEncryptKey() -> uint
{
    STACK_TRACE_ENTRY();

    return // Random 4 byte
        (GenericUtils::Random(1, 255) << 24) | //
        (GenericUtils::Random(1, 255) << 16) | //
        (GenericUtils::Random(1, 255) << 8) | //
        (GenericUtils::Random(1, 255) << 0);
}

void NetBuffer::SetEncryptKey(uint seed)
{
    STACK_TRACE_ENTRY();

    if (seed == 0) {
        _encryptActive = false;
        return;
    }

    std::mt19937 rnd_generator {seed};

    for (auto& key : _encryptKeys) {
        key = static_cast<uint8>(rnd_generator() % 256);
    }

    _encryptKeyPos = 0;
    _encryptActive = true;
}

auto NetBuffer::EncryptKey(int move) noexcept -> uint8
{
    STACK_TRACE_ENTRY();

    uint8 key = 0;

    if (_encryptActive) {
        key = _encryptKeys[_encryptKeyPos];
        _encryptKeyPos += move;

        if (_encryptKeyPos < 0 || _encryptKeyPos >= static_cast<int>(CRYPT_KEYS_COUNT)) {
            _encryptKeyPos %= static_cast<int>(CRYPT_KEYS_COUNT);

            if (_encryptKeyPos < 0) {
                _encryptKeyPos += static_cast<int>(CRYPT_KEYS_COUNT);
            }
        }
    }

    return key;
}

void NetBuffer::ResetBuf() noexcept
{
    STACK_TRACE_ENTRY();

    _bufEndPos = 0;

    if (_bufData.size() > _defaultBufLen) {
        _bufData.resize(_defaultBufLen);
        _bufData.shrink_to_fit();
    }
}

void NetBuffer::GrowBuf(size_t len)
{
    STACK_TRACE_ENTRY();

    if (_bufEndPos + len <= _bufData.size()) {
        return;
    }

    auto new_len = _bufData.size();

    while (_bufEndPos + len > new_len) {
        new_len *= 2;
    }

    _bufData.resize(new_len);
}

void NetBuffer::CopyBuf(const void* from, void* to, uint8 crypt_key, size_t len) const
{
    STACK_TRACE_ENTRY();

    const auto* from_ = static_cast<const uint8*>(from);
    auto* to_ = static_cast<uint8*>(to);

    for (size_t i = 0; i < len; i++, to_++, from_++) {
        *to_ = *from_ ^ crypt_key;
    }
}

void NetOutBuffer::Push(const void* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    GrowBuf(len);
    CopyBuf(buf, _bufData.data() + _bufEndPos, EncryptKey(static_cast<int>(len)), len);
    _bufEndPos += len;
}

void NetOutBuffer::Push(const_span<uint8> buf)
{
    STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return;
    }

    GrowBuf(buf.size());
    CopyBuf(buf.data(), _bufData.data() + _bufEndPos, EncryptKey(static_cast<int>(buf.size())), buf.size());
    _bufEndPos += buf.size();
}

void NetOutBuffer::DiscardWriteBuf(size_t len)
{
    STACK_TRACE_ENTRY();

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

void NetOutBuffer::WritePropsData(vector<const uint8*>* props_data, const vector<uint>* props_data_sizes)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(props_data->size() == props_data_sizes->size());
    RUNTIME_ASSERT(props_data->size() <= 0xFFFF);
    Write<uint16>(static_cast<uint16>(props_data->size()));

    for (size_t i = 0; i < props_data->size(); i++) {
        const auto data_size = static_cast<uint>(props_data_sizes->at(i));
        Write<uint>(data_size);
        Push({props_data->at(i), data_size});
    }
}

void NetOutBuffer::StartMsg(uint msg)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_msgStarted);

    _msgStarted = true;
    _startedMsg = msg;
    _startedBufPos = _bufEndPos;

    Push(&msg, sizeof(msg));

    const uint msg_len = GetMsgSize(msg);
    RUNTIME_ASSERT(msg_len != 0);

    if (msg_len == static_cast<uint>(-1)) {
        // Will be overwrited in message finalization
        Push(&msg_len, sizeof(msg_len));
    }
}

void NetOutBuffer::EndMsg()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_msgStarted);
    RUNTIME_ASSERT(_bufEndPos > _startedBufPos);

    _msgStarted = false;

    const auto actual_msg_len = static_cast<uint>(_bufEndPos - _startedBufPos);
    EncryptKey(-static_cast<int>(actual_msg_len));

    uint msg = 0;
    CopyBuf(_bufData.data() + _startedBufPos, &msg, EncryptKey(sizeof(msg)), sizeof(msg));
    RUNTIME_ASSERT(msg == _startedMsg);

    uint intended_msg_len = GetMsgSize(msg);
    RUNTIME_ASSERT(intended_msg_len != 0);

    if (intended_msg_len == static_cast<uint>(-1)) {
        intended_msg_len = actual_msg_len;
        CopyBuf(&actual_msg_len, _bufData.data() + _startedBufPos + sizeof(uint), EncryptKey(0), sizeof(actual_msg_len));
    }

    EncryptKey(static_cast<int>(actual_msg_len - sizeof(msg)));
    RUNTIME_ASSERT(actual_msg_len == intended_msg_len);
}

void NetInBuffer::ResetBuf() noexcept
{
    STACK_TRACE_ENTRY();

    NetBuffer::ResetBuf();

    _bufReadPos = 0;
}

void NetInBuffer::AddData(const void* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    GrowBuf(len);
    CopyBuf(buf, _bufData.data() + _bufEndPos, 0, len);
    _bufEndPos += len;
}

void NetInBuffer::SetEndPos(size_t pos)
{
    STACK_TRACE_ENTRY();

    if (pos > _bufData.size()) {
        throw NetBufferException("Invalid set end pos", pos, _bufData.size(), _bufEndPos);
    }

    _bufEndPos = pos;
}

void NetInBuffer::Pop(void* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    if (len == 0) {
        return;
    }

    if (_bufReadPos + len > _bufEndPos) {
        ResetBuf();
        throw NetBufferException("Invalid read length", len, _bufReadPos, _bufEndPos);
    }

    CopyBuf(_bufData.data() + _bufReadPos, buf, EncryptKey(static_cast<int>(len)), len);
    _bufReadPos += len;
}

void NetInBuffer::ShrinkReadBuf()
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    const auto data_count = Read<uint16>();
    props_data.resize(data_count);

    for (uint16 i = 0; i < data_count; i++) {
        const auto data_size = Read<uint>();
        props_data[i].resize(data_size);
        Pop(props_data[i].data(), data_size);
    }
}

auto NetInBuffer::ReadHashedString(const HashResolver& hash_resolver) -> hstring
{
    STACK_TRACE_ENTRY();

    const auto h = Read<hstring::hash_t>();

    bool failed = false;
    const hstring result = hash_resolver.ResolveHash(h, &failed);

    if (failed) {
        ResetBuf();
        throw NetBufferException("Can't resolve received hash", h);
    }

    return result;
}

auto NetInBuffer::NeedProcess() -> bool
{
    STACK_TRACE_ENTRY();

    uint msg = 0;

    if (_bufReadPos + sizeof(msg) > _bufEndPos) {
        return false;
    }

    CopyBuf(_bufData.data() + _bufReadPos, &msg, EncryptKey(0), sizeof(msg));

    uint msg_len = GetMsgSize(msg);

    // Unknown message
    if (msg_len == 0) {
        ResetBuf();
        throw NetBufferException("Unknown message", msg);
    }

    // Fixed size
    if (msg_len != static_cast<uint>(-1)) {
        return _bufReadPos + msg_len <= _bufEndPos;
    }

    // Variadic size
    if (_bufReadPos + sizeof(msg) + sizeof(msg_len) > _bufEndPos) {
        return false;
    }

    EncryptKey(sizeof(msg));
    CopyBuf(_bufData.data() + _bufReadPos + sizeof(msg), &msg_len, EncryptKey(0), sizeof(msg_len));
    EncryptKey(-static_cast<int>(sizeof(msg)));

    return _bufReadPos + msg_len <= _bufEndPos;
}

void NetInBuffer::SkipMsg(uint msg)
{
    STACK_TRACE_ENTRY();

    _bufReadPos -= sizeof(msg);
    EncryptKey(-static_cast<int>(sizeof(msg)));

    uint msg_len = GetMsgSize(msg);

    // Unknown message
    if (msg_len == 0) {
        ResetBuf();
        return;
    }

    // Variadic size
    if (msg_len == static_cast<uint>(-1)) {
        EncryptKey(sizeof(msg));
        CopyBuf(_bufData.data() + _bufReadPos + sizeof(msg), &msg_len, EncryptKey(0), sizeof(msg_len));
        EncryptKey(-static_cast<int>(sizeof(msg)));
    }

    _bufReadPos += msg_len;
    EncryptKey(static_cast<int>(msg_len));
}
