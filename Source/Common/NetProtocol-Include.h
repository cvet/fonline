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
#define NETMSG_HANDSHAKE_SIZE (sizeof(uint) + sizeof(uint) + sizeof(uint) + sizeof(uchar) * 28) // 40 bytes
// ////////////////////////////////////////////////////////////////////////
// Request to update
// uint protocol_version
// uint encrypt_key
// uchar padding[28]
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
#define NETMSG_PING_SIZE (sizeof(uint) + sizeof(uchar))
// ////////////////////////////////////////////////////////////////////////
// Ping
// uchar ping (see Ping in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_END_PARSE_TO_GAME MAKE_NETMSG_HEADER(7)
#define NETMSG_END_PARSE_TO_GAME_SIZE (sizeof(uint))
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
//   short path_len
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
#define NETMSG_UPDATE_FILE_DATA_SIZE (sizeof(uint) + FILE_UPDATE_PORTION)
// ////////////////////////////////////////////////////////////////////////
// Portion of data
// uchar data[FILE_UPDATE_PORTION]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ADD/REMOVE CRITTER
// ************************************************************************

#define NETMSG_ADD_CRITTER MAKE_NETMSG_HEADER(11)
// ////////////////////////////////////////////////////////////////////////
// Add critter on map
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_CRITTER MAKE_NETMSG_HEADER(13)
#define NETMSG_REMOVE_CRITTER_SIZE (sizeof(uint) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Remove critter from map.
// Params:
// uint crid
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Commands
// ************************************************************************

#define NETMSG_SEND_COMMAND MAKE_NETMSG_HEADER(21)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uchar cmd
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
// uchar how_say (see Say types in FOdefines.h)
// string str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_TEXT MAKE_NETMSG_HEADER(32)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// CrID crid
// uchar how_say
// string text
// bool unsafe_text
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG MAKE_NETMSG_HEADER(33)
#define NETMSG_MSG_SIZE (sizeof(uint) + sizeof(uint) + sizeof(uchar) + sizeof(ushort) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// CrID crid
// uchar how_say
// ushort MSG_num
// uint num_str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG_LEX MAKE_NETMSG_HEADER(34)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// CrID crid
// uchar how_say
// ushort MSG_num
// uint num_str
// string lexems
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT MAKE_NETMSG_HEADER(35)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ushort hx
// ushort hy
// uint color
// string text
// bool unsafe_text
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG MAKE_NETMSG_HEADER(36)
#define NETMSG_MAP_TEXT_MSG_SIZE (sizeof(uint) + sizeof(ushort) * 2 + sizeof(uint) + sizeof(ushort) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ushort hx
// ushort hy
// uint color
// ushort MSG_num
// uint num_str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG_LEX MAKE_NETMSG_HEADER(37)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ushort hx
// ushort hy
// uint color
// ushort MSG_num
// uint num_str
// string lexems
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// DIR/MOVE
// ************************************************************************

#define NETMSG_DIR MAKE_NETMSG_HEADER(41)
#define NETMSG_DIR_SIZE (sizeof(uint) + sizeof(short))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// short dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_DIR MAKE_NETMSG_HEADER(42)
#define NETMSG_CRITTER_DIR_SIZE (sizeof(uint) + sizeof(uint) + sizeof(short))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint cr_id
// short dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE MAKE_NETMSG_HEADER(45)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint map_id
// bool is_run
// ushort start_hx
// ushort start_hy
// ushort steps_count
// uchar steps[]
// ushort control_steps_count
// ushort control_steps[]
// char end_hex_ox - -16..16
// char end_hex_oy - -8..8
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_STOP_MOVE MAKE_NETMSG_HEADER(46)
#define NETMSG_SEND_STOP_MOVE_SIZE (sizeof(uint) + sizeof(uint) * 2 + sizeof(ushort) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint map_id
// ushort hx
// ushort hy
// short hex_ox
// short hex_oy
// short dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE MAKE_NETMSG_HEADER(47)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint cr_id
// uint whole_time
// uint offset_time
// bool is_run
// ushort start_hx
// ushort start_hy
// ushort steps_count
// uchar steps[]
// ushort control_steps_count
// ushort control_steps[]
// char end_hex_ox - -16..16
// char end_hex_oy - -8..8
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_STOP_MOVE MAKE_NETMSG_HEADER(48)
#define NETMSG_CRITTER_STOP_MOVE_SIZE (sizeof(uint) + sizeof(ushort) * 2 + sizeof(short) * 2 + sizeof(short))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint cr_id
// ushort start_hx
// ushort start_hy
// short hex_ox
// short hex_oy
// short dir_angle
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_POS MAKE_NETMSG_HEADER(49)
#define NETMSG_CRITTER_POS_SIZE (sizeof(uint) + sizeof(uint) + sizeof(ushort) * 2 + sizeof(short) * 2 + sizeof(short))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint cr_id
// ushort hex_x
// ushort hex_y
// short hex_ox
// short hex_oy
// short dir_angle
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
#define NETMSG_CRITTER_TELEPORT_SIZE (sizeof(uint) + sizeof(ushort) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint cr_id
// ushort to_hx
// ushort to_hy
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
// uint id
// hash pid
// uchar slot
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_ITEM MAKE_NETMSG_HEADER(66)
#define NETMSG_REMOVE_ITEM_SIZE (sizeof(uint) + sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
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
// ushort item_x
// ushort item_y
// uchar is_added
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ERASE_ITEM_FROM_MAP MAKE_NETMSG_HEADER(74)
#define NETMSG_ERASE_ITEM_FROM_MAP_SIZE (sizeof(uint) + sizeof(uint) + sizeof(uchar))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// uchar is_deleted
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ANIMATE_ITEM MAKE_NETMSG_HEADER(75)
#define NETMSG_ANIMATE_ITEM_SIZE (sizeof(uint) + sizeof(uint) + sizeof(uchar) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// uchar from_frm
// uchar to_frm
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
//	uint item_id
//	hash item_pid
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
// uint item_id
// hash item_pid
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ACTION MAKE_NETMSG_HEADER(91)
#define NETMSG_CRITTER_ACTION_SIZE (sizeof(uint) + sizeof(uint) + sizeof(int) * 2 + sizeof(bool))
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// int action
// int action_ext
// bool is_item
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE_ITEM MAKE_NETMSG_HEADER(93)
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ANIMATE MAKE_NETMSG_HEADER(95)
#define NETMSG_CRITTER_ANIMATE_SIZE (sizeof(uint) + sizeof(uint) * 3 + sizeof(bool) + sizeof(bool) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// uint ind1
// uint ind2
// bool is_item
// bool clear_sequence
// bool delay_play
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_SET_ANIMS MAKE_NETMSG_HEADER(96)
#define NETMSG_CRITTER_SET_ANIMS_SIZE (sizeof(uint) + sizeof(uint) + sizeof(uchar) + sizeof(uint) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// CritterCondition cond
// uint ind1
// uint ind2
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_EFFECT MAKE_NETMSG_HEADER(98)
#define NETMSG_EFFECT_SIZE (sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ushort) * 3)
// ////////////////////////////////////////////////////////////////////////
// explode
// Params:
// hash eff_pid
// ushort hex_x
// ushort hex_y
// ushort radius
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_FLY_EFFECT MAKE_NETMSG_HEADER(99)
#define NETMSG_FLY_EFFECT_SIZE (sizeof(uint) + sizeof(hstring::hash_t) + sizeof(uint) * 2 + sizeof(ushort) * 4)
// ////////////////////////////////////////////////////////////////////////
// shoot
// Params:
// hash eff_pid
// CrID eff_cr1_id
// CrID eff_cr2_id
// ushort eff_cr1_hx
// ushort eff_cr1_hy
// ushort eff_cr2_hx
// ushort eff_cr2_hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAY_SOUND MAKE_NETMSG_HEADER(101)
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint crid_synchronize
// string sound_name
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// TALK&BARTER NPC
// ************************************************************************

#define NETMSG_SEND_TALK_NPC MAKE_NETMSG_HEADER(109)
#define NETMSG_SEND_TALK_NPC_SIZE (sizeof(uint) + sizeof(uchar) + sizeof(uint) + sizeof(uchar))
// ////////////////////////////////////////////////////////////////////////
//
// uchar is_npc
// uint id_talk
// uchar answer - see Answer in FOdefines.h
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_TALK_NPC MAKE_NETMSG_HEADER(111)
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uchar is_npc
// uint crid
// uchar all_answers
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

#define NETMSG_GAME_INFO MAKE_NETMSG_HEADER(117)
#define NETMSG_GAME_INFO_SIZE (sizeof(uint) + sizeof(ushort) * 8 + sizeof(int) * 4 + sizeof(uchar) * 12)
// ////////////////////////////////////////////////////////////////////////
// Generic game info
// ushort year_start
// ushort year
// ushort month
// ushort day
// ushort hour
// ushort minute
// ushort second
// ushort time_multiplier
// int day_time[4]
// uchar day_color[12]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// MAP
// ************************************************************************

#define NETMSG_LOADMAP MAKE_NETMSG_HEADER(121)
// ////////////////////////////////////////////////////////////////////////
//
// uint mag_len
// uint loc_id
// uint map_id
// hash loc_pid
// hash map_pid
// uchar map_index_in_loc
// uint ver_tiles
// uint ver_walls
// uint ver_scen
// Properties map
// Properties location
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP MAKE_NETMSG_HEADER(122)
// ////////////////////////////////////////////////////////////////////////
// Map data
// uint msg_len
// hash pid_map
// uchar send_info (see Sendmap info in FOdefines.h)
// uint ver_tiles
//	for TILESX*TILESY
//		ushort roof
//		ushort floor
// uint count_walls
//	for count_walls
//		WallToSend
// uint count_scen
//	for count_scen
//		ScenToSend (see ScenToSend in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GIVE_MAP MAKE_NETMSG_HEADER(123)
#define NETMSG_SEND_GIVE_MAP_SIZE (sizeof(uint) + sizeof(bool) + sizeof(hstring::hash_t) + sizeof(uint) + sizeof(hstring::hash_t) * 2)
// ////////////////////////////////////////////////////////////////////////
// Request on map data, on map loading or for automap
// bool automap
// hash map_pid
// uint loc_id
// hash tiles_hash
// hash scen_hash
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_LOAD_MAP_OK MAKE_NETMSG_HEADER(124)
#define NETMSG_SEND_LOAD_MAP_OK_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_RPC MAKE_NETMSG_HEADER(128)
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_REFRESH_ME MAKE_NETMSG_HEADER(130)
#define NETMSG_SEND_REFRESH_ME_SIZE (sizeof(uint))
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_VIEW_MAP MAKE_NETMSG_HEADER(131)
#define NETMSG_VIEW_MAP_SIZE (sizeof(uint) + sizeof(ushort) * 2 + sizeof(uint) * 2)
// ////////////////////////////////////////////////////////////////////////
//
// ushort hx, hy
// uint loc_id, loc_ent
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// GLOBAL
// ************************************************************************

#define NETMSG_GLOBAL_INFO MAKE_NETMSG_HEADER(135)
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uchar info_flag; (see GM info in FOdefines.h)
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
// ushort locations_count
//  for locations_count
//  uint location_id
//  hash location_pid
//  ushort maps_count
//   for maps_count
//   hash map_pid
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Properties
// ************************************************************************

#define NETMSG_POD_PROPERTY(b, x) MAKE_NETMSG_HEADER(190 + (b) + (x)*10)
#define NETMSG_POD_PROPERTY_SIZE(b, x) (sizeof(uint) + sizeof(char) + sizeof(uint) * (x) + sizeof(ushort) + (b))
// ////////////////////////////////////////////////////////////////////////
// Property changed
// NetProperty type
// uint vars[x]
// ushort property_index
// uchar data[b]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_COMPLEX_PROPERTY MAKE_NETMSG_HEADER(189)
// ////////////////////////////////////////////////////////////////////////
// Property changed
// uint msg_len
// NetProperty type
// uint vars[x]
// ushort property_index
// uchar data[msg_len - ...]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_POD_PROPERTY(b, x) MAKE_NETMSG_HEADER(220 + (b) + (x)*10)
#define NETMSG_SEND_POD_PROPERTY_SIZE(b, x) (sizeof(uint) + sizeof(char) + sizeof(uint) * (x) + sizeof(ushort) + (b))
// ////////////////////////////////////////////////////////////////////////
// Client change property
// NetProperty type
// uint vars[x]
// ushort property_index
// uchar data[b]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_COMPLEX_PROPERTY MAKE_NETMSG_HEADER(219)
// ////////////////////////////////////////////////////////////////////////
// Client change property
// uint msg_len
// NetProperty type
// uint vars[x]
// ushort property_index
// uchar data[msg_len - ...]
// ////////////////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////////////////
// Properties serialization helpers
// ////////////////////////////////////////////////////////////////////////

#define NET_WRITE_PROPERTIES(bout, data_vec, data_sizes_vec) \
    do { \
        (bout) << (ushort)(data_vec)->size(); \
        for (size_t i_ = 0, j_ = (data_vec)->size(); i_ < j_; i_++) { \
            uint data_size_ = (data_sizes_vec)->at(i_); \
            (bout) << data_size_; \
            if (data_size_ > 0u) { \
                (bout).Push((data_vec)->at(i_), data_size_); \
            } \
        } \
    } while (0)

#define NET_READ_PROPERTIES(bin, data_vec) \
    do { \
        ushort data_count_; \
        (bin) >> data_count_; \
        (data_vec).resize(data_count_); \
        for (ushort i_ = 0; i_ < data_count_; i_++) { \
            uint data_size_; \
            (bin) >> data_size_; \
            (data_vec)[i_].resize(data_size_); \
            if (data_size_ > 0u) { \
                (bin).Pop(&(data_vec)[i_][0], data_size_); \
            } \
        } \
    } while (0)
