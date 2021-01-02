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

#include "NetBuffer.h"

NetBuffer::NetBuffer()
{
    _bufLen = DEFAULT_BUF_SIZE;
    _bufData = std::make_unique<uchar[]>(_bufLen);
}

void NetBuffer::SetEncryptKey(uint seed)
{
    if (seed == 0u) {
        _encryptActive = false;
        return;
    }

    std::mt19937 rnd_generator {seed};
    std::uniform_int_distribution<> rnd_distr {1, 255};
    for (auto& key : _encryptKeys) {
        key = static_cast<uchar>(rnd_distr(rnd_generator));
    }

    _encryptKeyPos = 0;
    _encryptActive = true;
}

auto NetBuffer::EncryptKey(int move) -> uchar
{
    uchar key = 0;
    if (_encryptActive) {
        key = _encryptKeys[_encryptKeyPos];
        _encryptKeyPos += move;
        if (_encryptKeyPos < 0 || _encryptKeyPos >= CRYPT_KEYS_COUNT) {
            _encryptKeyPos %= CRYPT_KEYS_COUNT;
            if (_encryptKeyPos < 0) {
                _encryptKeyPos += CRYPT_KEYS_COUNT;
            }
        }
    }
    return key;
}

void NetBuffer::Refresh()
{
    if (_isError) {
        return;
    }

    if (_bufReadPos > _bufEndPos) {
        _isError = true;
        return;
    }

    if (_bufReadPos != 0u) {
        for (auto i = _bufReadPos; i < _bufEndPos; i++) {
            _bufData[i - _bufReadPos] = _bufData[i];
        }

        _bufEndPos -= _bufReadPos;
        _bufReadPos = 0;
    }
}

void NetBuffer::Reset()
{
    _bufEndPos = 0;
    _bufReadPos = 0;

    if (_bufLen > DEFAULT_BUF_SIZE) {
        _bufLen = DEFAULT_BUF_SIZE;
        _bufData = std::make_unique<uchar[]>(_bufLen);
    }
}

void NetBuffer::GrowBuf(uint len)
{
    if (_bufEndPos + len < _bufLen) {
        return;
    }

    while (_bufEndPos + len >= _bufLen) {
        _bufLen <<= 1;
    }

    auto* new_buf = new uchar[_bufLen];
    std::memcpy(new_buf, _bufData.get(), _bufEndPos);
    _bufData.reset(new_buf);
}

auto NetBuffer::GetData() -> uchar*
{
    NON_CONST_METHOD_HINT();

    return _bufData.get();
}

auto NetBuffer::GetCurData() -> uchar*
{
    NON_CONST_METHOD_HINT();

    return _bufData.get() + _bufReadPos;
}

void NetBuffer::MoveReadPos(int val)
{
    _bufReadPos += val;
    EncryptKey(val);
}

void NetBuffer::Push(const void* buf, uint len)
{
    if (_isError || len == 0u) {
        return;
    }

    if (_bufEndPos + len >= _bufLen) {
        GrowBuf(len);
    }

    CopyBuf(buf, _bufData.get() + _bufEndPos, EncryptKey(len), len);
    _bufEndPos += len;
}

void NetBuffer::Push(const void* buf, uint len, bool no_crypt)
{
    if (_isError || len == 0u) {
        return;
    }

    if (_bufEndPos + len >= _bufLen) {
        GrowBuf(len);
    }

    CopyBuf(buf, _bufData.get() + _bufEndPos, no_crypt ? 0 : EncryptKey(len), len);
    _bufEndPos += len;
}

void NetBuffer::Pop(void* buf, uint len)
{
    if (_isError) {
        std::memset(buf, 0, len);
        return;
    }

    if (len == 0u) {
        return;
    }

    if (_bufReadPos + len > _bufEndPos) {
        _isError = true;
        std::memset(buf, 0, len);
        return;
    }

    CopyBuf(_bufData.get() + _bufReadPos, buf, EncryptKey(len), len);
    _bufReadPos += len;
}

void NetBuffer::Cut(uint len)
{
    if (_isError || len == 0u) {
        return;
    }

    if (_bufReadPos + len > _bufEndPos) {
        _isError = true;
        return;
    }

    auto* buf = _bufData.get() + _bufReadPos;
    for (uint i = 0; i + _bufReadPos + len < _bufEndPos; i++) {
        buf[i] = buf[i + len];
    }

    _bufEndPos -= len;
}

void NetBuffer::CopyBuf(const void* from, void* to, uchar crypt_key, uint len)
{
    const auto* from_ = static_cast<const uchar*>(from);
    auto* to_ = static_cast<uchar*>(to);

    for (uint i = 0; i < len; i++, to_++, from_++) {
        *to_ = *from_ ^ crypt_key;
    }
}

auto NetBuffer::NeedProcess() -> bool
{
    uint msg = 0;
    if (_bufReadPos + sizeof(msg) > _bufEndPos) {
        return false;
    }

    CopyBuf(_bufData.get() + _bufReadPos, &msg, EncryptKey(0), sizeof(msg));

    // Known size
    switch (msg) {
    case 0xFFFFFFFF:
        return true; // Ping
    case NETMSG_DISCONNECT:
        return NETMSG_DISCONNECT_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_LOGIN:
        return NETMSG_LOGIN_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_WRONG_NET_PROTO:
        return NETMSG_WRONG_NET_PROTO_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_REGISTER_SUCCESS:
        return NETMSG_REGISTER_SUCCESS_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_PING:
        return NETMSG_PING_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_END_PARSE_TO_GAME:
        return NETMSG_END_PARSE_TO_GAME_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_UPDATE:
        return NETMSG_UPDATE_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_GET_UPDATE_FILE:
        return NETMSG_GET_UPDATE_FILE_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_GET_UPDATE_FILE_DATA:
        return NETMSG_GET_UPDATE_FILE_DATA_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_UPDATE_FILE_DATA:
        return NETMSG_UPDATE_FILE_DATA_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_REMOVE_CRITTER:
        return NETMSG_REMOVE_CRITTER_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_MSG:
        return NETMSG_MSG_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_MAP_TEXT_MSG:
        return NETMSG_MAP_TEXT_MSG_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_DIR:
        return NETMSG_DIR_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CRITTER_DIR:
        return NETMSG_CRITTER_DIR_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_MOVE_WALK:
        return NETMSG_SEND_MOVE_WALK_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_MOVE_RUN:
        return NETMSG_SEND_MOVE_RUN_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CRITTER_MOVE:
        return NETMSG_CRITTER_MOVE_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CRITTER_XY:
        return NETMSG_CRITTER_XY_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CUSTOM_COMMAND:
        return NETMSG_CUSTOM_COMMAND_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CLEAR_ITEMS:
        return NETMSG_CLEAR_ITEMS_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_REMOVE_ITEM:
        return NETMSG_REMOVE_ITEM_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_ALL_ITEMS_SEND:
        return NETMSG_ALL_ITEMS_SEND_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_ERASE_ITEM_FROM_MAP:
        return NETMSG_ERASE_ITEM_FROM_MAP_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_ANIMATE_ITEM:
        return NETMSG_ANIMATE_ITEM_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CRITTER_ACTION:
        return NETMSG_CRITTER_ACTION_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CRITTER_ANIMATE:
        return NETMSG_CRITTER_ANIMATE_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_CRITTER_SET_ANIMS:
        return NETMSG_CRITTER_SET_ANIMS_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_EFFECT:
        return NETMSG_EFFECT_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_FLY_EFFECT:
        return NETMSG_FLY_EFFECT_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_TALK_NPC:
        return NETMSG_SEND_TALK_NPC_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_GET_INFO:
        return NETMSG_SEND_GET_TIME_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_GAME_INFO:
        return NETMSG_GAME_INFO_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_GIVE_MAP:
        return NETMSG_SEND_GIVE_MAP_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_LOAD_MAP_OK:
        return NETMSG_SEND_LOAD_MAP_OK_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_REFRESH_ME:
        return NETMSG_SEND_REFRESH_ME_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_VIEW_MAP:
        return NETMSG_VIEW_MAP_SIZE + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(1, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(1, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(2, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(2, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(4, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(4, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(8, 0):
        return NETMSG_SEND_POD_PROPERTY_SIZE(8, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(1, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(1, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(2, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(2, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(4, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(4, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(8, 1):
        return NETMSG_SEND_POD_PROPERTY_SIZE(8, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(1, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(1, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(2, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(2, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(4, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(4, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_SEND_POD_PROPERTY(8, 2):
        return NETMSG_SEND_POD_PROPERTY_SIZE(8, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(1, 0):
        return NETMSG_POD_PROPERTY_SIZE(1, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(2, 0):
        return NETMSG_POD_PROPERTY_SIZE(2, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(4, 0):
        return NETMSG_POD_PROPERTY_SIZE(4, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(8, 0):
        return NETMSG_POD_PROPERTY_SIZE(8, 0) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(1, 1):
        return NETMSG_POD_PROPERTY_SIZE(1, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(2, 1):
        return NETMSG_POD_PROPERTY_SIZE(2, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(4, 1):
        return NETMSG_POD_PROPERTY_SIZE(4, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(8, 1):
        return NETMSG_POD_PROPERTY_SIZE(8, 1) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(1, 2):
        return NETMSG_POD_PROPERTY_SIZE(1, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(2, 2):
        return NETMSG_POD_PROPERTY_SIZE(2, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(4, 2):
        return NETMSG_POD_PROPERTY_SIZE(4, 2) + _bufReadPos <= _bufEndPos;
    case NETMSG_POD_PROPERTY(8, 2):
        return NETMSG_POD_PROPERTY_SIZE(8, 2) + _bufReadPos <= _bufEndPos;
    default:
        break;
    }

    // Changeable size
    uint msg_len = 0;
    if (_bufReadPos + sizeof(msg) + sizeof(msg_len) > _bufEndPos) {
        return false;
    }

    EncryptKey(sizeof(msg));
    CopyBuf(_bufData.get() + _bufReadPos + sizeof(msg), &msg_len, EncryptKey(-static_cast<int>(sizeof(msg))), sizeof(msg_len));

    switch (msg) {
    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOADMAP:
    case NETMSG_CREATE_CLIENT:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_PLAYER:
    case NETMSG_ADD_NPC:
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
    case NETMSG_COMBAT_RESULTS:
    case NETMSG_PLAY_SOUND:
    case NETMSG_TALK_NPC:
    case NETMSG_MAP:
    case NETMSG_RPC:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES:
        return _bufReadPos + msg_len <= _bufEndPos;
    default:
        // Unknown message
        Reset();
        _isError = true;
        return false;
    }
}

void NetBuffer::SkipMsg(uint msg)
{
    _bufReadPos -= sizeof(msg);
    EncryptKey(-static_cast<int>(sizeof(msg)));

    // Known size
    uint size;
    switch (msg) {
    case 0xFFFFFFFF:
        size = 16;
        break;
    case NETMSG_DISCONNECT:
        size = NETMSG_DISCONNECT_SIZE;
        break;
    case NETMSG_LOGIN:
        size = NETMSG_LOGIN_SIZE;
        break;
    case NETMSG_WRONG_NET_PROTO:
        size = NETMSG_WRONG_NET_PROTO_SIZE;
        break;
    case NETMSG_REGISTER_SUCCESS:
        size = NETMSG_REGISTER_SUCCESS_SIZE;
        break;
    case NETMSG_PING:
        size = NETMSG_PING_SIZE;
        break;
    case NETMSG_END_PARSE_TO_GAME:
        size = NETMSG_END_PARSE_TO_GAME_SIZE;
        break;
    case NETMSG_UPDATE:
        size = NETMSG_UPDATE_SIZE;
        break;
    case NETMSG_GET_UPDATE_FILE:
        size = NETMSG_GET_UPDATE_FILE_SIZE;
        break;
    case NETMSG_GET_UPDATE_FILE_DATA:
        size = NETMSG_GET_UPDATE_FILE_DATA_SIZE;
        break;
    case NETMSG_UPDATE_FILE_DATA:
        size = NETMSG_UPDATE_FILE_DATA_SIZE;
        break;
    case NETMSG_REMOVE_CRITTER:
        size = NETMSG_REMOVE_CRITTER_SIZE;
        break;
    case NETMSG_MSG:
        size = NETMSG_MSG_SIZE;
        break;
    case NETMSG_MAP_TEXT_MSG:
        size = NETMSG_MAP_TEXT_MSG_SIZE;
        break;
    case NETMSG_DIR:
        size = NETMSG_DIR_SIZE;
        break;
    case NETMSG_CRITTER_DIR:
        size = NETMSG_CRITTER_DIR_SIZE;
        break;
    case NETMSG_SEND_MOVE_WALK:
        size = NETMSG_SEND_MOVE_WALK_SIZE;
        break;
    case NETMSG_SEND_MOVE_RUN:
        size = NETMSG_SEND_MOVE_RUN_SIZE;
        break;
    case NETMSG_CRITTER_MOVE:
        size = NETMSG_CRITTER_MOVE_SIZE;
        break;
    case NETMSG_CRITTER_XY:
        size = NETMSG_CRITTER_XY_SIZE;
        break;
    case NETMSG_CUSTOM_COMMAND:
        size = NETMSG_CUSTOM_COMMAND_SIZE;
        break;
    case NETMSG_CLEAR_ITEMS:
        size = NETMSG_CLEAR_ITEMS_SIZE;
        break;
    case NETMSG_REMOVE_ITEM:
        size = NETMSG_REMOVE_ITEM_SIZE;
        break;
    case NETMSG_ALL_ITEMS_SEND:
        size = NETMSG_ALL_ITEMS_SEND_SIZE;
        break;
    case NETMSG_ERASE_ITEM_FROM_MAP:
        size = NETMSG_ERASE_ITEM_FROM_MAP_SIZE;
        break;
    case NETMSG_ANIMATE_ITEM:
        size = NETMSG_ANIMATE_ITEM_SIZE;
        break;
    case NETMSG_CRITTER_ACTION:
        size = NETMSG_CRITTER_ACTION_SIZE;
        break;
    case NETMSG_CRITTER_ANIMATE:
        size = NETMSG_CRITTER_ANIMATE_SIZE;
        break;
    case NETMSG_CRITTER_SET_ANIMS:
        size = NETMSG_CRITTER_SET_ANIMS_SIZE;
        break;
    case NETMSG_EFFECT:
        size = NETMSG_EFFECT_SIZE;
        break;
    case NETMSG_FLY_EFFECT:
        size = NETMSG_FLY_EFFECT_SIZE;
        break;
    case NETMSG_SEND_TALK_NPC:
        size = NETMSG_SEND_TALK_NPC_SIZE;
        break;
    case NETMSG_SEND_GET_INFO:
        size = NETMSG_SEND_GET_TIME_SIZE;
        break;
    case NETMSG_GAME_INFO:
        size = NETMSG_GAME_INFO_SIZE;
        break;
    case NETMSG_SEND_GIVE_MAP:
        size = NETMSG_SEND_GIVE_MAP_SIZE;
        break;
    case NETMSG_SEND_LOAD_MAP_OK:
        size = NETMSG_SEND_LOAD_MAP_OK_SIZE;
        break;
    case NETMSG_SEND_REFRESH_ME:
        size = NETMSG_SEND_REFRESH_ME_SIZE;
        break;
    case NETMSG_VIEW_MAP:
        size = NETMSG_VIEW_MAP_SIZE;
        break;
    case NETMSG_SEND_POD_PROPERTY(1, 0):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(1, 0);
        break;
    case NETMSG_SEND_POD_PROPERTY(2, 0):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(2, 0);
        break;
    case NETMSG_SEND_POD_PROPERTY(4, 0):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(4, 0);
        break;
    case NETMSG_SEND_POD_PROPERTY(8, 0):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(8, 0);
        break;
    case NETMSG_SEND_POD_PROPERTY(1, 1):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(1, 1);
        break;
    case NETMSG_SEND_POD_PROPERTY(2, 1):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(2, 1);
        break;
    case NETMSG_SEND_POD_PROPERTY(4, 1):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(4, 1);
        break;
    case NETMSG_SEND_POD_PROPERTY(8, 1):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(8, 1);
        break;
    case NETMSG_SEND_POD_PROPERTY(1, 2):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(1, 2);
        break;
    case NETMSG_SEND_POD_PROPERTY(2, 2):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(2, 2);
        break;
    case NETMSG_SEND_POD_PROPERTY(4, 2):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(4, 2);
        break;
    case NETMSG_SEND_POD_PROPERTY(8, 2):
        size = NETMSG_SEND_POD_PROPERTY_SIZE(8, 2);
        break;
    case NETMSG_POD_PROPERTY(1, 0):
        size = NETMSG_POD_PROPERTY_SIZE(1, 0);
        break;
    case NETMSG_POD_PROPERTY(2, 0):
        size = NETMSG_POD_PROPERTY_SIZE(2, 0);
        break;
    case NETMSG_POD_PROPERTY(4, 0):
        size = NETMSG_POD_PROPERTY_SIZE(4, 0);
        break;
    case NETMSG_POD_PROPERTY(8, 0):
        size = NETMSG_POD_PROPERTY_SIZE(8, 0);
        break;
    case NETMSG_POD_PROPERTY(1, 1):
        size = NETMSG_POD_PROPERTY_SIZE(1, 1);
        break;
    case NETMSG_POD_PROPERTY(2, 1):
        size = NETMSG_POD_PROPERTY_SIZE(2, 1);
        break;
    case NETMSG_POD_PROPERTY(4, 1):
        size = NETMSG_POD_PROPERTY_SIZE(4, 1);
        break;
    case NETMSG_POD_PROPERTY(8, 1):
        size = NETMSG_POD_PROPERTY_SIZE(8, 1);
        break;
    case NETMSG_POD_PROPERTY(1, 2):
        size = NETMSG_POD_PROPERTY_SIZE(1, 2);
        break;
    case NETMSG_POD_PROPERTY(2, 2):
        size = NETMSG_POD_PROPERTY_SIZE(2, 2);
        break;
    case NETMSG_POD_PROPERTY(4, 2):
        size = NETMSG_POD_PROPERTY_SIZE(4, 2);
        break;
    case NETMSG_POD_PROPERTY(8, 2):
        size = NETMSG_POD_PROPERTY_SIZE(8, 2);
        break;

    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOADMAP:
    case NETMSG_CREATE_CLIENT:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_PLAYER:
    case NETMSG_ADD_NPC:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_SOME_ITEMS:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_COMBAT_RESULTS:
    case NETMSG_PLAY_SOUND:
    case NETMSG_TALK_NPC:
    case NETMSG_MAP:
    case NETMSG_RPC:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEM:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES: {
        // Changeable size
        uint msg_len = 0;
        EncryptKey(sizeof(msg));
        CopyBuf(_bufData.get() + _bufReadPos + sizeof(msg), &msg_len, EncryptKey(-static_cast<int>(sizeof(msg))), sizeof(msg_len));
        size = msg_len;
    } break;
    default:
        Reset();
        return;
    }

    _bufReadPos += size;
    EncryptKey(size);
}
