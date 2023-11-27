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

// ReSharper disable CppMissingIncludeGuard

/************************************************************************/
/* Base                                                                 */
/************************************************************************/

#define MAKE_NETMSG_HEADER(number) (static_cast<uint>(number))
constexpr uint PING_CLIENT_LIFE_TIME = 15000;

// Special message
// 0xFFFFFFFF - ping, answer
// 4 - players in game
// 12 - reserved

// ************************************************************************
// LOGIN MESSAGES
// ************************************************************************

#define NETMSG_HANDSHAKE MAKE_NETMSG_HEADER(9)
#define NETMSG_HANDSHAKE_SIZE (sizeof(uint) + sizeof(uint) + sizeof(uint) + sizeof(uint8) * 28) // 40 bytes
// ////////////////////////////////////////////////////////////////////////
// Request to update
// uint protocol_version
// uint encrypt_key
// uint8 padding[28]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_DISCONNECT MAKE_NETMSG_HEADER(10)
#define NETMSG_DISCONNECT_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Disconnect
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGIN MAKE_NETMSG_HEADER(1)
// ////////////////////////////////////////////////////////////////////////
// Enter to game
// uint msg_len
// string name
// string password
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGIN_SUCCESS MAKE_NETMSG_HEADER(2)
// ////////////////////////////////////////////////////////////////////////
// Login accepted
// uint bin_seed
// uint bout_seed
// Properties global
// Properties player
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_WRONG_NET_PROTO MAKE_NETMSG_HEADER(8)
#define NETMSG_WRONG_NET_PROTO_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Wrong network protocol
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REGISTER MAKE_NETMSG_HEADER(3)
// ////////////////////////////////////////////////////////////////////////
// Registration query
// uint mag_len
// string name
// string password
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REGISTER_SUCCESS MAKE_NETMSG_HEADER(4)
#define NETMSG_REGISTER_SUCCESS_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Answer about successes registration
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PING MAKE_NETMSG_HEADER(5)
#define NETMSG_PING_SIZE (sizeof(uint) + sizeof(uint8))
// ////////////////////////////////////////////////////////////////////////
// Ping
// uint8 ping (see Ping in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLACE_TO_GAME_COMPLETE MAKE_NETMSG_HEADER(7)
#define NETMSG_PLACE_TO_GAME_COMPLETE_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Update
// ************************************************************************

#define NETMSG_UPDATE_FILES_LIST MAKE_NETMSG_HEADER(15)
// ////////////////////////////////////////////////////////////////////////
// Files list to update
// uint msg_len
// bool outdated
// uint files_count
//   int16 path_len
//   char path[path_len]
//   uint size
// Properties global
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GET_UPDATE_FILE MAKE_NETMSG_HEADER(16)
#define NETMSG_GET_UPDATE_FILE_SIZE (sizeof(uint) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Request to updated file
// uint file_number
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GET_UPDATE_FILE_DATA MAKE_NETMSG_HEADER(17)
#define NETMSG_GET_UPDATE_FILE_DATA_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Request to update file data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_UPDATE_FILE_DATA MAKE_NETMSG_HEADER(18)
// ////////////////////////////////////////////////////////////////////////
// Portion of data
// uint msg_len
// uint data_len
// uint8 data[data_len]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ADD/REMOVE CRITTER
// ************************************************************************

#define NETMSG_ADD_CRITTER MAKE_NETMSG_HEADER(11)
// ////////////////////////////////////////////////////////////////////////
// Add critter on map
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_CRITTER MAKE_NETMSG_HEADER(13)
#define NETMSG_REMOVE_CRITTER_SIZE (sizeof(uint) + sizeof(ident_t))
// ////////////////////////////////////////////////////////////////////////
// Remove critter from map.
// Params:
// ident_t cr_id
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Commands
// ************************************************************************

#define NETMSG_SEND_COMMAND MAKE_NETMSG_HEADER(21)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint8 cmd
// Ext parameters
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// TEXT
// ************************************************************************

#define NETMSG_SEND_TEXT MAKE_NETMSG_HEADER(31)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint8 how_say (see Say types in FOdefines.h)
// string str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_TEXT MAKE_NETMSG_HEADER(32)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t cr_id
// uint8 how_say
// string text
// bool unsafe_text
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG MAKE_NETMSG_HEADER(33)
#define NETMSG_MSG_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(uint8) + sizeof(uint16) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// uint8 how_say
// uint16 MSG_num
// uint str_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG_LEX MAKE_NETMSG_HEADER(34)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t cr_id
// uint8 how_say
// uint16 MSG_num
// uint str_num
// string lexems
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT MAKE_NETMSG_HEADER(35)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint16 hx
// uint16 hy
// uint color
// string text
// bool unsafe_text
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG MAKE_NETMSG_HEADER(36)
#define NETMSG_MAP_TEXT_MSG_SIZE (sizeof(uint) + sizeof(uint16) * 2 + sizeof(uint) + sizeof(uint16) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint16 hx
// uint16 hy
// uint color
// uint16 MSG_num
// uint str_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG_LEX MAKE_NETMSG_HEADER(37)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint16 hx
// uint16 hy
// uint color
// uint16 MSG_num
// uint str_num
// string lexems
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// DIR/MOVE
// ************************************************************************

#define NETMSG_DIR MAKE_NETMSG_HEADER(41)
#define NETMSG_DIR_SIZE (sizeof(uint) + sizeof(ident_t) * 2 + sizeof(int16))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t map_id
// ident_t cr_id
// int16 dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_DIR MAKE_NETMSG_HEADER(42)
#define NETMSG_CRITTER_DIR_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(int16))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// int16 dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE MAKE_NETMSG_HEADER(45)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t map_id
// ident_t cr_id
// uint16 speed
// uint16 start_hx
// uint16 start_hy
// uint16 steps_count
// uint8 steps[]
// uint16 control_steps_count
// uint16 control_steps[]
// char end_hex_ox - -16..16
// char end_hex_oy - -8..8
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_STOP_MOVE MAKE_NETMSG_HEADER(46)
#define NETMSG_SEND_STOP_MOVE_SIZE (sizeof(uint) + sizeof(ident_t) * 2 + sizeof(uint16) * 2 + sizeof(int16) * 3)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t map_id
// ident_t cr_id
// uint16 hx
// uint16 hy
// int16 hex_ox
// int16 hex_oy
// int16 dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE MAKE_NETMSG_HEADER(47)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t cr_id
// uint whole_time
// uint offset_time
// uint16 speed
// uint16 start_hx
// uint16 start_hy
// uint16 steps_count
// uint8 steps[]
// uint16 control_steps_count
// uint16 control_steps[]
// char end_hex_ox - -16..16
// char end_hex_oy - -8..8
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_POS MAKE_NETMSG_HEADER(49)
#define NETMSG_CRITTER_POS_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(uint16) * 2 + sizeof(int16) * 2 + sizeof(int16))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// uint16 hex_x
// uint16 hex_y
// int16 hex_ox
// int16 hex_oy
// int16 dir_angle
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CHOSEN Params
// ************************************************************************

#define NETMSG_ALL_PROPERTIES MAKE_NETMSG_HEADER(51)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// Properties
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_TELEPORT MAKE_NETMSG_HEADER(52)
#define NETMSG_CRITTER_TELEPORT_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(uint16) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// uint16 to_hx
// uint16 to_hy
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CHOSEN ITEMS
// ************************************************************************

#define NETMSG_CLEAR_ITEMS MAKE_NETMSG_HEADER(64)
#define NETMSG_CLEAR_ITEMS_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_ITEM MAKE_NETMSG_HEADER(65)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t id
// hstring pid
// uint8 slot
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_ITEM MAKE_NETMSG_HEADER(66)
#define NETMSG_REMOVE_ITEM_SIZE (sizeof(uint) + sizeof(ident_t))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t item_id
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ALL_ITEMS_SEND MAKE_NETMSG_HEADER(69)
#define NETMSG_ALL_ITEMS_SEND_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ITEMS ON MAP
// ************************************************************************

#define NETMSG_ADD_ITEM_ON_MAP MAKE_NETMSG_HEADER(71)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint item_id
// hash item_pid
// uint16 item_x
// uint16 item_y
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ERASE_ITEM_FROM_MAP MAKE_NETMSG_HEADER(74)
#define NETMSG_ERASE_ITEM_FROM_MAP_SIZE (sizeof(uint) + sizeof(ident_t))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t item_id
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ANIMATE_ITEM MAKE_NETMSG_HEADER(75)
#define NETMSG_ANIMATE_ITEM_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(hstring::hash_t) + sizeof(bool) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t item_id
// hstring anim_name
// bool looped
// bool reversed
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CHOSEN USE ITEM
// ************************************************************************

#define NETMSG_SOME_ITEMS MAKE_NETMSG_HEADER(83)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// int param
// bool is_null
// uint count
//	ident_t item_id
//	hstring item_pid
//	Properties data
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CRITTER ACTION
// ************************************************************************

#define NETMSG_SOME_ITEM MAKE_NETMSG_HEADER(90)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t item_id
// hstring item_pid
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ACTION MAKE_NETMSG_HEADER(91)
#define NETMSG_CRITTER_ACTION_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(CritterAction) + sizeof(int) + sizeof(bool))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// CritterAction action
// int action_ext
// bool is_context_item
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE_ITEM MAKE_NETMSG_HEADER(93)
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ANIMATE MAKE_NETMSG_HEADER(95)
#define NETMSG_CRITTER_ANIMATE_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(CritterStateAnim) + sizeof(CritterActionAnim) + sizeof(bool) * 3)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// CritterStateAnim state_anim
// CritterActionAnim action_anim
// bool is_context_item
// bool clear_sequence
// bool delay_play
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_SET_ANIMS MAKE_NETMSG_HEADER(96)
#define NETMSG_CRITTER_SET_ANIMS_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(CritterCondition) + sizeof(CritterStateAnim) + sizeof(CritterActionAnim))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ident_t cr_id
// CritterCondition cond
// CritterStateAnim state_anim
// CritterActionAnim action_anim
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_EFFECT MAKE_NETMSG_HEADER(98)
#define NETMSG_EFFECT_SIZE (sizeof(uint) + sizeof(hstring::hash_t) + sizeof(uint16) * 3)
// ////////////////////////////////////////////////////////////////////////
// explode
// Params:
// hstring eff_pid
// uint16 hex_x
// uint16 hex_y
// uint16 radius
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_FLY_EFFECT MAKE_NETMSG_HEADER(99)
#define NETMSG_FLY_EFFECT_SIZE (sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ident_t) * 2 + sizeof(uint16) * 4)
// ////////////////////////////////////////////////////////////////////////
// shoot
// Params:
// hash eff_pid
// ident_t eff_cr1_id
// ident_t eff_cr2_id
// uint16 eff_cr1_hx
// uint16 eff_cr1_hy
// uint16 eff_cr2_hx
// uint16 eff_cr2_hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAY_SOUND MAKE_NETMSG_HEADER(101)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ident_t cr_id_synchronize
// string sound_name
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// TALK&BARTER NPC
// ************************************************************************

#define NETMSG_SEND_TALK_NPC MAKE_NETMSG_HEADER(109)
#define NETMSG_SEND_TALK_NPC_SIZE (sizeof(uint) + sizeof(ident_t) + sizeof(hstring::hash_t) + sizeof(uint8))
// ////////////////////////////////////////////////////////////////////////
//
// ident_t cr_id
// hstring dlg_pack_id
// uint8 answer
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_TALK_NPC MAKE_NETMSG_HEADER(111)
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uint8 is_npc
// ident_t cr_id
// uint8 all_answers
// uint main_text
// uint answ_text
// uint talk_time
// string lexems
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// GAMETIME
// ************************************************************************

#define NETMSG_SEND_GET_INFO MAKE_NETMSG_HEADER(116)
#define NETMSG_SEND_GET_TIME_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_TIME_SYNC MAKE_NETMSG_HEADER(117)
#define NETMSG_TIME_SYNC_SIZE (sizeof(uint) + sizeof(uint16) * 7)
// ////////////////////////////////////////////////////////////////////////
//
// uint16 year
// uint16 month
// uint16 day
// uint16 hour
// uint16 minute
// uint16 second
// uint16 time_multiplier
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// MAP
// ************************************************************************

#define NETMSG_LOADMAP MAKE_NETMSG_HEADER(121)
// ////////////////////////////////////////////////////////////////////////
//
// uint mag_len
// ident_t loc_id
// ident_t map_id
// hstring loc_pid
// hstring map_pid
// uint8 map_index_in_loc
// Properties map
// Properties location
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_RPC MAKE_NETMSG_HEADER(128)
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_VIEW_MAP MAKE_NETMSG_HEADER(131)
#define NETMSG_VIEW_MAP_SIZE (sizeof(uint) + sizeof(uint16) * 2 + sizeof(ident_t) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// uint16 hx
// uint16 hy
// ident_t loc_id
// uint loc_ent
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// GLOBAL
// ************************************************************************

#define NETMSG_GLOBAL_INFO MAKE_NETMSG_HEADER(135)
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uint8 info_flag; (see GM info in FOdefines.h)
// Data
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Automaps info
// ************************************************************************

#define NETMSG_AUTOMAPS_INFO MAKE_NETMSG_HEADER(170)
// ////////////////////////////////////////////////////////////////////////
// Automaps information
// uint msg_len
// bool clear_list
// uint16 locations_count
//  for locations_count
//  ident_t location_id
//  hstring location_pid
//  uint16 maps_count
//   for maps_count
//   hstring map_pid
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Properties
// ************************************************************************

#define NETMSG_POD_PROPERTY(b, x) MAKE_NETMSG_HEADER(190 + (b) + (x)*10)
#define NETMSG_POD_PROPERTY_SIZE(b, x) (sizeof(uint) + sizeof(char) + sizeof(ident_t) * (x) + sizeof(uint16) + (b))
// ////////////////////////////////////////////////////////////////////////
// Property changed
// NetProperty type
// ident_t vars[x]
// uint16 property_index
// uint8 data[b]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_COMPLEX_PROPERTY MAKE_NETMSG_HEADER(189)
// ////////////////////////////////////////////////////////////////////////
// Property changed
// uint msg_len
// NetProperty type
// uint vars[x]
// uint16 property_index
// uint8 data[msg_len - ...]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_POD_PROPERTY(b, x) MAKE_NETMSG_HEADER(220 + (b) + (x)*10)
#define NETMSG_SEND_POD_PROPERTY_SIZE(b, x) (sizeof(uint) + sizeof(char) + sizeof(ident_t) * (x) + sizeof(uint16) + (b))
// ////////////////////////////////////////////////////////////////////////
// Client change property
// NetProperty type
// ident_t vars[x]
// uint16 property_index
// uint8 data[b]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_COMPLEX_PROPERTY MAKE_NETMSG_HEADER(219)
// ////////////////////////////////////////////////////////////////////////
// Client change property
// uint msg_len
// NetProperty type
// ident_t vars[x]
// uint16 property_index
// uint8 data[msg_len - ...]
// ////////////////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////////////////
// Properties serialization helpers
// ////////////////////////////////////////////////////////////////////////

#define NET_WRITE_PROPERTIES(bout, data_vec, data_sizes_vec) \
    do { \
        RUNTIME_ASSERT((data_vec)->size() <= 0xFFFF); \
        bout.Write<uint16>(static_cast<uint16>((data_vec)->size())); \
        for (size_t i_ = 0; i_ < (data_vec)->size(); i_++) { \
            const auto data_size_ = static_cast<uint>(data_sizes_vec->at(i_)); \
            bout.Write<uint>(data_size_); \
            bout.Push((data_vec)->at(i_), data_size_); \
        } \
    } while (0)

#define NET_READ_PROPERTIES(bin, data_vec) \
    do { \
        const auto data_count_ = bin.Read<uint16>(); \
        data_vec.resize(data_count_); \
        for (uint16 i_ = 0; i_ < data_count_; i_++) { \
            const auto data_size_ = bin.Read<uint>(); \
            data_vec[i_].resize(data_size_); \
            bin.Pop(data_vec[i_].data(), data_size_); \
        } \
    } while (0)
