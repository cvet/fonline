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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    case NETMSG_SEND_STOP_MOVE:
        return NETMSG_SEND_STOP_MOVE_SIZE;
    case NETMSG_CRITTER_POS:
        return NETMSG_CRITTER_POS_SIZE;
    case NETMSG_CRITTER_TELEPORT:
        return NETMSG_CRITTER_TELEPORT_SIZE;
    case NETMSG_CLEAR_ITEMS:
        return NETMSG_CLEAR_ITEMS_SIZE;
    case NETMSG_REMOVE_ITEM:
        return NETMSG_REMOVE_ITEM_SIZE;
    case NETMSG_ALL_ITEMS_SEND:
        return NETMSG_ALL_ITEMS_SEND_SIZE;
    case NETMSG_ERASE_ITEM_FROM_MAP:
        return NETMSG_ERASE_ITEM_FROM_MAP_SIZE;
    case NETMSG_ANIMATE_ITEM:
        return NETMSG_ANIMATE_ITEM_SIZE;
    case NETMSG_CRITTER_ACTION:
        return NETMSG_CRITTER_ACTION_SIZE;
    case NETMSG_CRITTER_ANIMATE:
        return NETMSG_CRITTER_ANIMATE_SIZE;
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
    case NETMSG_LOADMAP:
    case NETMSG_REGISTER:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_CRITTER:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEM:
    case NETMSG_SOME_ITEMS:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_PLAY_SOUND:
    case NETMSG_TALK_NPC:
    case NETMSG_RPC:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES:
    case NETMSG_SEND_MOVE:
    case NETMSG_CRITTER_MOVE:
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
    _bufLen = buf_len;
    _bufData = std::make_unique<uint8[]>(_bufLen);
}

auto NetBuffer::IsError() const -> bool
{
    STACK_TRACE_ENTRY();

    return _isError;
}

auto NetBuffer::GetEndPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _bufEndPos;
}

void NetBuffer::SetError(bool value)
{
    STACK_TRACE_ENTRY();

    _isError = value;
}

auto NetBuffer::GenerateEncryptKey() -> uint
{
    STACK_TRACE_ENTRY();

    return (GenericUtils::Random(1, 255) << 24) | (GenericUtils::Random(1, 255) << 16) | (GenericUtils::Random(1, 255) << 8) | GenericUtils::Random(1, 255);
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

auto NetBuffer::EncryptKey(int move) -> uint8
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

void NetBuffer::ResetBuf()
{
    STACK_TRACE_ENTRY();

    if (_isError) {
        return;
    }

    _bufEndPos = 0;

    if (_bufLen > _defaultBufLen) {
        _bufLen = _defaultBufLen;
        _bufData = std::make_unique<uint8[]>(_bufLen);
    }
}

void NetBuffer::GrowBuf(size_t len)
{
    STACK_TRACE_ENTRY();

    if (_bufEndPos + len < _bufLen) {
        return;
    }

    while (_bufEndPos + len >= _bufLen) {
        _bufLen <<= 1;
    }

    auto* new_buf = new uint8[_bufLen];
    std::memcpy(new_buf, _bufData.get(), _bufEndPos);
    _bufData.reset(new_buf);
}

auto NetBuffer::GetData() -> uint8*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _bufData.get();
}

void NetBuffer::CopyBuf(const void* from, void* to, uint8 crypt_key, size_t len)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_isError) {
        return;
    }

    const auto* from_ = static_cast<const uint8*>(from);
    auto* to_ = static_cast<uint8*>(to);

    for (size_t i = 0; i < len; i++, to_++, from_++) {
        *to_ = *from_ ^ crypt_key;
    }
}

void NetOutBuffer::Push(const void* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    if (_isError || len == 0) {
        return;
    }

    if (_bufEndPos + len >= _bufLen) {
        GrowBuf(len);
    }

    CopyBuf(buf, _bufData.get() + _bufEndPos, EncryptKey(static_cast<int>(len)), len);
    _bufEndPos += len;
}

void NetOutBuffer::Cut(size_t len)
{
    STACK_TRACE_ENTRY();

    if (_isError || len == 0) {
        return;
    }

    if (len > _bufEndPos) {
        _isError = true;
        return;
    }

    auto* buf = _bufData.get();
    for (size_t i = 0; i + len < _bufEndPos; i++) {
        buf[i] = buf[i + len];
    }

    _bufEndPos -= len;
}

void NetOutBuffer::StartMsg(uint msg)
{
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
    RUNTIME_ASSERT(_msgStarted);
    RUNTIME_ASSERT(_bufEndPos > _startedBufPos);

    _msgStarted = false;

    const auto actual_msg_len = static_cast<uint>(_bufEndPos - _startedBufPos);
    EncryptKey(-static_cast<int>(actual_msg_len));

    uint msg = 0;
    CopyBuf(_bufData.get() + _startedBufPos, &msg, EncryptKey(sizeof(msg)), sizeof(msg));
    RUNTIME_ASSERT(msg == _startedMsg);

    uint intended_msg_len = GetMsgSize(msg);
    RUNTIME_ASSERT(intended_msg_len != 0);

    if (intended_msg_len == static_cast<uint>(-1)) {
        intended_msg_len = actual_msg_len;
        CopyBuf(&actual_msg_len, _bufData.get() + _startedBufPos + sizeof(uint), EncryptKey(0), sizeof(actual_msg_len));
    }

    EncryptKey(static_cast<int>(actual_msg_len - sizeof(msg)));
    RUNTIME_ASSERT(actual_msg_len == intended_msg_len);
}

void NetInBuffer::ResetBuf()
{
    STACK_TRACE_ENTRY();

    NetBuffer::ResetBuf();

    _bufReadPos = 0;
}

void NetInBuffer::AddData(const void* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    if (_isError || len == 0) {
        return;
    }

    if (_bufEndPos + len >= _bufLen) {
        GrowBuf(len);
    }

    CopyBuf(buf, _bufData.get() + _bufEndPos, 0, len);
    _bufEndPos += len;
}

void NetInBuffer::SetEndPos(size_t pos)
{
    STACK_TRACE_ENTRY();

    _bufEndPos = pos;
}

void NetInBuffer::Pop(void* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    if (_isError) {
        std::memset(buf, 0, len);
        return;
    }

    if (len == 0) {
        return;
    }

    if (_bufReadPos + len > _bufEndPos) {
        _isError = true;
        std::memset(buf, 0, len);
        return;
    }

    CopyBuf(_bufData.get() + _bufReadPos, buf, EncryptKey(static_cast<int>(len)), len);
    _bufReadPos += len;
}

void NetInBuffer::ShrinkReadBuf()
{
    STACK_TRACE_ENTRY();

    if (_isError) {
        return;
    }

    if (_bufReadPos > _bufEndPos) {
        _isError = true;
        return;
    }

    if (_bufReadPos >= _bufEndPos) {
        if (_bufReadPos != 0) {
            ResetBuf();
        }
    }
    else if (_bufReadPos != 0) {
        for (auto i = _bufReadPos; i < _bufEndPos; i++) {
            _bufData[i - _bufReadPos] = _bufData[i];
        }

        _bufEndPos -= _bufReadPos;
        _bufReadPos = 0;
    }
}

auto NetInBuffer::ReadHashedString(const HashResolver& hash_resolver) -> hstring
{
    STACK_TRACE_ENTRY();

    const auto h = Read<hstring::hash_t>();

    hstring result;

    if (!_isError) {
        bool failed = false;
        result = hash_resolver.ResolveHash(h, &failed);
        if (failed) {
            _isError = true;
        }
    }

    return result;
}

auto NetInBuffer::NeedProcess() -> bool
{
    STACK_TRACE_ENTRY();

    if (_isError) {
        return false;
    }

    uint msg = 0;
    if (_bufReadPos + sizeof(msg) > _bufEndPos) {
        return false;
    }

    CopyBuf(_bufData.get() + _bufReadPos, &msg, EncryptKey(0), sizeof(msg));

    uint msg_len = GetMsgSize(msg);

    // Unknown message
    if (msg_len == 0) {
        ResetBuf();
        _isError = true;
        return false;
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
    CopyBuf(_bufData.get() + _bufReadPos + sizeof(msg), &msg_len, EncryptKey(0), sizeof(msg_len));
    EncryptKey(-static_cast<int>(sizeof(msg)));

    return _bufReadPos + msg_len <= _bufEndPos;
}

void NetInBuffer::SkipMsg(uint msg)
{
    STACK_TRACE_ENTRY();

    if (_isError) {
        return;
    }

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
        CopyBuf(_bufData.get() + _bufReadPos + sizeof(msg), &msg_len, EncryptKey(0), sizeof(msg_len));
        EncryptKey(-static_cast<int>(sizeof(msg)));
    }

    _bufReadPos += msg_len;
    EncryptKey(static_cast<int>(msg_len));
}
