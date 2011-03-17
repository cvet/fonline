#ifndef __NET_PROTOCOL__
#define __NET_PROTOCOL__

#include <Defines.h>

/************************************************************************/
/* Base                                                                 */
/************************************************************************/

#define FO_PROTOCOL_VERSION		    (0xF0AF) // FOnline Protocol Version
#define MAKE_NETMSG_HEADER(number)  ((MSGTYPE)((0xDEAD<<17)|(number<<8)|(0xAA)))
#define PING_CLIENT_LIFE_TIME       (15000) // Time to ping client life
#define PING_CLIENT_INFO_TIME       (2000) // Time to ping client for information

// Net states
#define STATE_DISCONNECT    (1)
#define STATE_CONN          (2)
#define STATE_GAME          (3)
#define STATE_LOGINOK       (4)
#define STATE_INIT_NET      (5) // Only for client

// Special message
// 0xFFFFFFFF - ping, answer
// 4 - players in game
// 12 - reserved

// Fixed headers
// NETMSG_LOGIN
// NETMSG_CREATE_CLIENT
// NETMSG_NET_SETTINGS

//************************************************************************
// LOGIN MESSAGES
//************************************************************************

#define NETMSG_LOGIN                MAKE_NETMSG_HEADER(1)
#define NETMSG_LOGIN_SIZE           (sizeof(MSGTYPE)+sizeof(WORD)+sizeof(DWORD)*8/*UIDs*/+\
MAX_NAME*2+sizeof(DWORD)+sizeof(DWORD)*10/*MSG*/+sizeof(DWORD)*13/*Proto*/+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// Enter to game
// Params:
// WORD protocol_version
// !DWORD uid4
// char name[MAX_NAME]
// !DWORD uid1
// char pass[MAX_NAME]
// DWORD msg_language
// DWORD hash_msg_game[TEXTMSG_COUNT]
// !DWORD uidxor
// !DWORD uid3
// !DWORD uid2
// !DWORD uidor
// DWORD hash_proto[ITEM_MAX_TYPES]
// !DWORD uidcalc
// BYTE default_combat_mode
// !DWORD uid0
//////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGIN_SUCCESS        MAKE_NETMSG_HEADER(2)
#define NETMSG_LOGIN_SUCCESS_SIZE   (sizeof(MSGTYPE)+sizeof(DWORD)*2)
//////////////////////////////////////////////////////////////////////////
// Login accepted
// DWORD bin_seed
// DWORD bout_seed
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CREATE_CLIENT        MAKE_NETMSG_HEADER(3)
//////////////////////////////////////////////////////////////////////////
// Registration query
// Params:
// DWORD mag_len
// WORD proto_ver
// MAX_NAME name
// MAX_NAME pass
// WORD params_count
//  WORD param_index
//  int param_val
//////////////////////////////////////////////////////////////////////////

#define NETMSG_REGISTER_SUCCESS     MAKE_NETMSG_HEADER(4)
#define NETMSG_REGISTER_SUCCESS_SIZE (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
// Answer about successes registration
//////////////////////////////////////////////////////////////////////////

#define NETMSG_PING                 MAKE_NETMSG_HEADER(5)
#define NETMSG_PING_SIZE            (sizeof(MSGTYPE)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// Ping
// BYTE ping (see Ping in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

/*#define NETMSG_UPDATE_FILES_LIST    MAKE_NETMSG_HEADER(7)
//////////////////////////////////////////////////////////////////////////
// Prepared message
// DWORD msg_len
// DWORD files_count
//  WORD path_len - max MAX_FOPATH
//  char path[path_len]
//  DWORD size
//  DWORD hash
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GET_UPDATE_FILE MAKE_NETMSG_HEADER(8)
#define NETMSG_SEND_GET_UPDATE_FILE_SIZE (sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// Request to updated file
// DWORD file_number
//////////////////////////////////////////////////////////////////////////

#define NETMSG_UPDATE_FILE          MAKE_NETMSG_HEADER(9)
//////////////////////////////////////////////////////////////////////////
// Portion of data
// DWORD msg_len
// WORD portion_len
// BYTE portion[portion_len]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_HELLO                MAKE_NETMSG_HEADER(10)
#define NETMSG_HELLO_SIZE           (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
// First message after connection
//////////////////////////////////////////////////////////////////////////
*/

#define NETMSG_SINGLEPLAYER_SAVE_LOAD MAKE_NETMSG_HEADER(10)
//////////////////////////////////////////////////////////////////////////
// Singleplayer
// DWORD msg_len
// bool save
// WORD fname_len
// char fname[fname_len]
// if save
//  DWORD save_pic_len
//  BYTE save_pic[save_pic_len]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CHECK_UID0           MAKE_NETMSG_HEADER(6)
#define NETMSG_CHECK_UID1           MAKE_NETMSG_HEADER(30)
#define NETMSG_CHECK_UID2           MAKE_NETMSG_HEADER(78)
#define NETMSG_CHECK_UID3           MAKE_NETMSG_HEADER(139)
#define NETMSG_CHECK_UID4           MAKE_NETMSG_HEADER(211)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
// DWORD uid3
// DWORD xor for uid0
// BYTE rnd_count, 1..21
// DWORD uid1
// DWORD xor for uid2
// char rnd_data[rnd_count] - random numbers
// DWORD uid2
// DWORD xor for uid1
// DWORD uid4
// BYTE rnd_count2, 2..12
// DWORD xor for uid3
// DWORD xor for uid4
// DWORD uid0
// char rnd_data2[rnd_count] - random numbers
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CHECK_UID_EXT        MAKE_NETMSG_HEADER(140)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// ADD/REMOVE CRITTER
//************************************************************************

#define NETMSG_ADD_PLAYER           MAKE_NETMSG_HEADER(11)
//////////////////////////////////////////////////////////////////////////
// Add player on map.
//////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_NPC              MAKE_NETMSG_HEADER(12)
//////////////////////////////////////////////////////////////////////////
// Add npc on map.
//////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_CRITTER       MAKE_NETMSG_HEADER(13)
#define NETMSG_REMOVE_CRITTER_SIZE  (sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// Remove critter from map.
// Params:
// DWORD crid
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// Commands, lexems
//************************************************************************

#define NETMSG_SEND_COMMAND         MAKE_NETMSG_HEADER(21)
//////////////////////////////////////////////////////////////////////////
// текстовое сообщение
// Params:
// DWORD msg_len
// BYTE cmd (see Commands in Access.h)
// Ext parameters
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_LEXEMS       MAKE_NETMSG_HEADER(22)
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD msg_len
// DWORD critter_id
// WORD lexems_length
// char lexems[lexems_length]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_ITEM_LEXEMS          MAKE_NETMSG_HEADER(23)
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD msg_len
// DWORD item_id
// WORD lexems_length
// char lexems[lexems_length]
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// TEXT
//************************************************************************

#define NETMSG_SEND_TEXT            MAKE_NETMSG_HEADER(31)
//////////////////////////////////////////////////////////////////////////
// текстовое сообщение
// Params:
// DWORD msg_len
// BYTE how_say (see Say types in FOdefines.h)
// WORD len
// char[len] str
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_TEXT         MAKE_NETMSG_HEADER(32)
//////////////////////////////////////////////////////////////////////////
// текст который надо нарисовать над криттером
// Params:
// DWORD msg_len
// CrID crid
// BYTE how_say
// WORD intellect
// bool unsafe_text
// WORD len
// char[len] str
//////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG                  MAKE_NETMSG_HEADER(33)
#define NETMSG_MSG_SIZE             (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(BYTE)+sizeof(WORD)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// заготовленна€ строка
// Params:
// CrID crid
// BYTE how_say
// WORD MSG_num
// DWORD num_str
//////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG_LEX              MAKE_NETMSG_HEADER(34)
//////////////////////////////////////////////////////////////////////////
// заготовленна€ строка
// Params:
// DWORD msg_len
// CrID crid
// BYTE how_say
// WORD MSG_num
// DWORD num_str
// WORD lex_len
// char lexems[lex_len]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT             MAKE_NETMSG_HEADER(35)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD msg_len
// WORD hx
// WORD hy
// DWORD color
// WORD len
// char[len] str
//////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG         MAKE_NETMSG_HEADER(36)
#define NETMSG_MAP_TEXT_MSG_SIZE    (sizeof(MSGTYPE)+sizeof(WORD)*2+\
sizeof(DWORD)+sizeof(WORD)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// заготовленна€ строка
// Params:
// WORD hx
// WORD hy
// DWORD color
// WORD MSG_num
// DWORD num_str
//////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG_LEX     MAKE_NETMSG_HEADER(37)
//////////////////////////////////////////////////////////////////////////
// заготовленна€ строка
// Params:
// DWORD msg_len
// WORD hx
// WORD hy
// DWORD color
// WORD MSG_num
// DWORD num_str
// WORD lexems_len
// char lexems[lexems_len]
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// DIR/MOVE
//************************************************************************

#define NETMSG_DIR                  MAKE_NETMSG_HEADER(41)
#define NETMSG_DIR_SIZE             (sizeof(MSGTYPE)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// сообщение направлени€
// Params:
// BYTE dir
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_DIR          MAKE_NETMSG_HEADER(42)
#define NETMSG_CRITTER_DIR_SIZE     (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// направление криттера
// Params:
// CrID id
// BYTE dir
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE_WALK       MAKE_NETMSG_HEADER(43)
#define NETMSG_SEND_MOVE_WALK_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(WORD)*2)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD move_params
// WORD hx
// WORD hy
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE_RUN        MAKE_NETMSG_HEADER(44)
#define NETMSG_SEND_MOVE_RUN_SIZE   (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(WORD)*2)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD move_params
// WORD hx
// WORD hy
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE         MAKE_NETMSG_HEADER(45)
#define NETMSG_CRITTER_MOVE_SIZE    (sizeof(MSGTYPE)+sizeof(DWORD)*2+sizeof(WORD)*2)
//////////////////////////////////////////////////////////////////////////
// передача направлени€ дл€ других криттеров
// Params:
// DWORD id
// DWORD move_params
// WORD hx
// WORD hy
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_XY           MAKE_NETMSG_HEADER(46)
#define NETMSG_CRITTER_XY_SIZE      (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(WORD)*2+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// сигнал о неверном положении чезена
// Params:
// CrID crid
// WORD hex_x
// WORD hex_y
// BYTE dir
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// CHOSEN Params
//************************************************************************

#define NETMSG_ALL_PARAMS           MAKE_NETMSG_HEADER(51)
#define NETMSG_ALL_PARAMS_SIZE      (sizeof(MSGTYPE)+MAX_PARAMS*sizeof(int))
//////////////////////////////////////////////////////////////////////////
// передача параметров
// Params:
// parameters[]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_PARAM                MAKE_NETMSG_HEADER(52)
#define NETMSG_PARAM_SIZE          (sizeof(MSGTYPE)+sizeof(WORD)+sizeof(int))
//////////////////////////////////////////////////////////////////////////
// передача отдельного параметра
// Params:
// WORD num_param
// int val
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_PARAM        MAKE_NETMSG_HEADER(53)
#define NETMSG_CRITTER_PARAM_SIZE   (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(WORD)+sizeof(int))
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD crid
// WORD num_param
// int val
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_LEVELUP         MAKE_NETMSG_HEADER(60)
//////////////////////////////////////////////////////////////////////////
// передача отдельного параметра
// Params:
// DWORD msg_len
// WORD count_skill_up
//	for count_skill_up
//	WORD num_skill
//	WORD val_up
// WORD perk_up
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRAFT_ASK            MAKE_NETMSG_HEADER(61)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
// WORD count
// DWORD crafts_nums[count]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_CRAFT           MAKE_NETMSG_HEADER(62)
#define NETMSG_SEND_CRAFT_SIZE      (sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
//
// DWORD craft_num
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRAFT_RESULT         MAKE_NETMSG_HEADER(63)
#define NETMSG_CRAFT_RESULT_SIZE    (sizeof(MSGTYPE)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
//
// BYTE craft_result (see Craft results in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// CHOSEN ITEMS
//************************************************************************

#define NETMSG_CLEAR_ITEMS        MAKE_NETMSG_HEADER(64)
#define NETMSG_CLEAR_ITEMS_SIZE	  (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_ITEM           MAKE_NETMSG_HEADER(65)
#define NETMSG_ADD_ITEM_SIZE      (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(WORD)+sizeof(BYTE)+92/*ItemData*/)
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD id
// WORD pid
// BYTE slot
// Item::ItemData Data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_ITEM        MAKE_NETMSG_HEADER(66)
#define NETMSG_REMOVE_ITEM_SIZE	  (sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD item_id
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SORT_VALUE_ITEM MAKE_NETMSG_HEADER(68)
#define NETMSG_SEND_SORT_VALUE_ITEM_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(WORD))
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD item_id
// WORD sort_val
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// ITEMS ON MAP
//************************************************************************

#define NETMSG_ADD_ITEM_ON_MAP    MAKE_NETMSG_HEADER(71)
#define NETMSG_ADD_ITEM_ON_MAP_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(WORD)*3+sizeof(BYTE)+92/*ItemData*/)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD item_id
// WORD item_pid
// WORD item_x
// WORD item_y
// BYTE is_added
// Item::ItemData data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CHANGE_ITEM_ON_MAP MAKE_NETMSG_HEADER(72)
#define NETMSG_CHANGE_ITEM_ON_MAP_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+92/*ItemData*/)
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD item_id
// Item::ItemData data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_RATE_ITEM     MAKE_NETMSG_HEADER(73)
#define NETMSG_SEND_RATE_ITEM_SIZE (sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD rate of main item
//////////////////////////////////////////////////////////////////////////

#define NETMSG_ERASE_ITEM_FROM_MAP MAKE_NETMSG_HEADER(74)
#define NETMSG_ERASE_ITEM_FROM_MAP_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD item_id
// BYTE is_deleted
//////////////////////////////////////////////////////////////////////////

#define NETMSG_ANIMATE_ITEM       MAKE_NETMSG_HEADER(75)
#define NETMSG_ANIMATE_ITEM_SIZE  (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(BYTE)*2)
//////////////////////////////////////////////////////////////////////////
// 
// Params:
// DWORD item_id
// BYTE from_frm
// BYTE to_frm
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// CHOSEN USE ITEM
//************************************************************************

#define NETMSG_SEND_CHANGE_ITEM     MAKE_NETMSG_HEADER(81)
#define NETMSG_SEND_CHANGE_ITEM_SIZE (sizeof(MSGTYPE)+sizeof(BYTE)+sizeof(DWORD)+sizeof(BYTE)*2+\
sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
//
// Params:
// BYTE ap
// DWORD item_id
// BYTE from_slot
// BYTE to_slot
// DWORD count
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_PICK_ITEM       MAKE_NETMSG_HEADER(82)
#define NETMSG_SEND_PICK_ITEM_SIZE  (sizeof(MSGTYPE)+sizeof(WORD)*3)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// WORD targ_x
// WORD targ_y
// WORD pid
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CONTAINER_INFO       MAKE_NETMSG_HEADER(83)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD msg_len
// BYTE transfer_type
// DWORD talk_time
// DWORD cont_id
// WORD cont_pid or barter_k
// DWORD count_items
//	while(cont_items)
//	DWORD item_id
//	WORD item_pid
//	DWORD item_count
//	Data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_ITEM_CONT       MAKE_NETMSG_HEADER(84)
#define NETMSG_SEND_ITEM_CONT_SIZE  (sizeof(MSGTYPE)+sizeof(BYTE)+sizeof(DWORD)*2+\
sizeof(DWORD)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// берем предмет из контейнера
// Params:
// BYTE transfer_type (see Transfer types in FOdefines.h)
// DWORD cont_id
// DWORD item_id
// DWORD item_count
// BYTE take_flags (see Take flags in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_USE_ITEM        MAKE_NETMSG_HEADER(85)
#define NETMSG_SEND_USE_ITEM_SIZE   (sizeof(MSGTYPE)+sizeof(BYTE)+sizeof(DWORD)+\
sizeof(WORD)+sizeof(BYTE)+sizeof(BYTE)+sizeof(DWORD)+sizeof(WORD)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// Use some item.
// Params:
// BYTE ap
// DWORD item_id
// WORD item_pid
// BYTE rate
// BYTE target_type
// DWORD target_id
// WORD target_pid
// DWORD param
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_USE_SKILL       MAKE_NETMSG_HEADER(86)
#define NETMSG_SEND_USE_SKILL_SIZE	(sizeof(MSGTYPE)+sizeof(WORD)+sizeof(BYTE)+\
sizeof(DWORD)+sizeof(WORD))
//////////////////////////////////////////////////////////////////////////
// Use some skill.
// Params:
// WORD skill
// BYTE targ_type
// DWORD target_id
// WORD target_pid
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_PICK_CRITTER    MAKE_NETMSG_HEADER(87)
#define NETMSG_SEND_PICK_CRITTER_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// Critter picking.
// Params:
// DWORD crid
// BYTE pick_type (see Pick types in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// CRITTER ACTION
//************************************************************************

#define NETMSG_SOME_ITEM            MAKE_NETMSG_HEADER(90)
#define NETMSG_SOME_ITEM_SIZE      (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(WORD)+\
sizeof(BYTE)+92/*ItemData*/)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD item_id
// WORD item_pid
// BYTE slot
// Item::ItemData data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ACTION       MAKE_NETMSG_HEADER(91)
#define NETMSG_CRITTER_ACTION_SIZE	(sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(int)*2+sizeof(bool))
//////////////////////////////////////////////////////////////////////////
// сигнал о том что криттер производит какоето действие
// Params:
// DWORD crid
// int action
// int action_ext
// bool is_item
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_KNOCKOUT      MAKE_NETMSG_HEADER(92)
#define NETMSG_CRITTER_KNOCKOUT_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(DWORD)*2+sizeof(WORD)+sizeof(WORD))
//////////////////////////////////////////////////////////////////////////
// сигнал о том что криттер производит какоето действие
// Params:
// DWORD crid
// DWORD anim2begin
// DWORD anim2idle
// WORD knock_hx
// WORD knock_hy
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE_ITEM    MAKE_NETMSG_HEADER(93)
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ITEM_DATA    MAKE_NETMSG_HEADER(94)
#define NETMSG_CRITTER_ITEM_DATA_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(BYTE)+92/*ItemData*/)
//////////////////////////////////////////////////////////////////////////
// Item data changed
// DWORD crid
// BYTE slot
// Item::ItemData data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ANIMATE      MAKE_NETMSG_HEADER(95)
#define NETMSG_CRITTER_ANIMATE_SIZE	(sizeof(MSGTYPE)+sizeof(DWORD)*3+\
sizeof(bool)+sizeof(bool)*2)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD crid
// DWORD ind1
// DWORD ind2
// bool is_item
// bool clear_sequence
// bool delay_play
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_SET_ANIMS    MAKE_NETMSG_HEADER(96)
#define NETMSG_CRITTER_SET_ANIMS_SIZE (sizeof(MSGTYPE)+sizeof(int)+sizeof(DWORD)*3)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD crid
// int cond
// DWORD ind1
// DWORD ind2
//////////////////////////////////////////////////////////////////////////

#define NETMSG_COMBAT_RESULTS       MAKE_NETMSG_HEADER(97)
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_EFFECT				MAKE_NETMSG_HEADER(98)
#define NETMSG_EFFECT_SIZE			(sizeof(MSGTYPE)+sizeof(WORD)*4)
//////////////////////////////////////////////////////////////////////////
// explode
// Params:
// WORD eff_pid
// WORD hex_x
// WORD hex_y
// WORD radius
//////////////////////////////////////////////////////////////////////////

#define NETMSG_FLY_EFFECT			MAKE_NETMSG_HEADER(99)
#define NETMSG_FLY_EFFECT_SIZE		(sizeof(MSGTYPE)+sizeof(WORD)+\
sizeof(DWORD)*2+sizeof(WORD)*4)
//////////////////////////////////////////////////////////////////////////
// shoot
// Params:
// WORD eff_pid
// CrID eff_cr1_id
// CrID eff_cr2_id
// WORD eff_cr1_hx
// WORD eff_cr1_hy
// WORD eff_cr2_hx
// WORD eff_cr2_hy
//////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAY_SOUND           MAKE_NETMSG_HEADER(101)
#define NETMSG_PLAY_SOUND_SIZE      (sizeof(MSGTYPE)+sizeof(DWORD)+16)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD crid_synchronize
// char sound_name[16]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAY_SOUND_TYPE      MAKE_NETMSG_HEADER(102)
#define NETMSG_PLAY_SOUND_TYPE_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(BYTE)*4)
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD crid_synchronize
// BYTE sound_type
// BYTE sound_type_ext
// BYTE sound_id
// BYTE sound_id_ext
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_KARMA_VOTING    MAKE_NETMSG_HEADER(103)
#define NETMSG_SEND_KARMA_VOTING_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+sizeof(bool))
//////////////////////////////////////////////////////////////////////////
//
// Params:
// DWORD crid
// bool val_up
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// TALK&BARTER NPC
//************************************************************************

#define NETMSG_SEND_TALK_NPC        MAKE_NETMSG_HEADER(109)
#define NETMSG_SEND_TALK_NPC_SIZE	(sizeof(MSGTYPE)+sizeof(BYTE)+\
sizeof(DWORD)+sizeof(BYTE))
//////////////////////////////////////////////////////////////////////////
// сигнал игрока о беседе
// BYTE is_npc
// DWORD id_talk
// BYTE answer - вариант ответа (see Answer in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SAY_NPC         MAKE_NETMSG_HEADER(110)
#define NETMSG_SEND_SAY_NPC_SIZE    (sizeof(MSGTYPE)+sizeof(BYTE)+\
sizeof(DWORD)+MAX_SAY_NPC_TEXT)
//////////////////////////////////////////////////////////////////////////
// say about
// BYTE is_npc
// DWORD id_talk
// char str[MAX_NET_TEXT] - вариант ответа (see Answer in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

#define NETMSG_TALK_NPC             MAKE_NETMSG_HEADER(111)
//////////////////////////////////////////////////////////////////////////
// ответ нпц -> игроку
// DWORD msg_len
// BYTE is_npc
// DWORD crid
// BYTE all_answers - всего вариантов ответа, если 0 - диалог закончен
// DWORD main_text - текст Ќѕ÷
// DWORD answ_text в кол-ве all_answers - варианты ответа
// DWORD talk_time
// WORD lexems_length
// char[lexems_length] lexems
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_BARTER          MAKE_NETMSG_HEADER(112)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
// CrID crid
//
// WORD count_sale
// for count_sale
//	DWORD item_id
//	DWORD item_count
//
// WORD count_buy
// for count_buy
//	DWORD item_id
//	DWORD item_count
//////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAYERS_BARTER       MAKE_NETMSG_HEADER(114)
#define NETMSG_PLAYERS_BARTER_SIZE  (sizeof(MSGTYPE)+sizeof(BYTE)+\
sizeof(DWORD)*2)
//////////////////////////////////////////////////////////////////////////
//
// BYTE barter
// DWORD param
// DWORD param_ext
//////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAYERS_BARTER_SET_HIDE MAKE_NETMSG_HEADER(115)
#define NETMSG_PLAYERS_BARTER_SET_HIDE_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(WORD)+sizeof(DWORD)+92/*ItemData*/)
//////////////////////////////////////////////////////////////////////////
//
// DWORD id
// WORD pid
// DWORD count
// Item::ItemData data
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// GAMETIME
//************************************************************************

#define NETMSG_SEND_GET_INFO        MAKE_NETMSG_HEADER(116)
#define NETMSG_SEND_GET_TIME_SIZE	(sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
// запрос на игровое врем€
//////////////////////////////////////////////////////////////////////////

#define NETMSG_GAME_INFO            MAKE_NETMSG_HEADER(117)
#define NETMSG_GAME_INFO_SIZE       (sizeof(MSGTYPE)+sizeof(WORD)*8+\
sizeof(int)+sizeof(BYTE)+sizeof(bool)*2+sizeof(int)*4+sizeof(BYTE)*12)
//////////////////////////////////////////////////////////////////////////
// Generic game info
// WORD GameOpt.YearStart;
// WORD GameOpt.Year;
// WORD GameOpt.Month;
// WORD GameOpt.Day;
// WORD GameOpt.Hour;
// WORD GameOpt.Minute;
// WORD GameOpt.Second;
// WORD GameOpt.TimeMultiplier;
// int time
// BYTE rain
// bool turn_based
// bool no_log_out
// int day_time[4]
// BYTE day_color[12]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_COMBAT          MAKE_NETMSG_HEADER(118)
#define NETMSG_SEND_COMBAT_SIZE     (sizeof(MSGTYPE)+sizeof(BYTE)+sizeof(int))
//////////////////////////////////////////////////////////////////////////
// Turn based
// BYTE type (see Combat in FOdefines.h)
// int val
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// MAP
//************************************************************************

#define NETMSG_LOADMAP              MAKE_NETMSG_HEADER(121)
#define NETMSG_LOADMAP_SIZE	        (sizeof(MSGTYPE)+sizeof(WORD)+\
sizeof(int)+sizeof(BYTE)+sizeof(DWORD)*3)
//////////////////////////////////////////////////////////////////////////
// высылаетс€ игроку команда загрузки карты
// WORD num_pid
// int map_time
// BYTE map_rain
// DWORD ver_tiles
// DWORD ver_walls
// DWORD ver_scen
//////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP                  MAKE_NETMSG_HEADER(122)
//////////////////////////////////////////////////////////////////////////
// Map data
// DWORD msg_len
// WORD pid_map
// BYTE send_info (see Sendmap info in FOdefines.h)
// DWORD ver_tiles
//	for TILESX*TILESY
//		WORD roof
//		WORD floor
// DWORD count_walls
//	for count_walls
//		WallToSend
// DWORD count_scen
//	for count_scen
//		ScenToSend (see ScenToSend in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GIVE_MAP        MAKE_NETMSG_HEADER(123)
#define NETMSG_SEND_GIVE_MAP_SIZE   (sizeof(MSGTYPE)+sizeof(bool)+sizeof(WORD)+\
sizeof(DWORD)+sizeof(DWORD)*3)
//////////////////////////////////////////////////////////////////////////
// Request on map data, on map loading or for automap
// bool automap
// WORD map_pid
// DWORD loc_id
// DWORD ver_tiles
// DWORD ver_walls
// DWORD ver_scen
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_LOAD_MAP_OK     MAKE_NETMSG_HEADER(124)
#define NETMSG_SEND_LOAD_MAP_OK_SIZE (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SHOW_SCREEN          MAKE_NETMSG_HEADER(125)
#define NETMSG_SHOW_SCREEN_SIZE     (sizeof(MSGTYPE)+sizeof(int)+sizeof(DWORD)+sizeof(bool))
//////////////////////////////////////////////////////////////////////////
//
// int screen_type
// DWORD param
// bool need_answer
//////////////////////////////////////////////////////////////////////////

#define NETMSG_RUN_CLIENT_SCRIPT    MAKE_NETMSG_HEADER(126)
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_RUN_SERVER_SCRIPT MAKE_NETMSG_HEADER(127)
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SCREEN_ANSWER   MAKE_NETMSG_HEADER(128)
#define NETMSG_SEND_SCREEN_ANSWER_SIZE (sizeof(MSGTYPE)+sizeof(DWORD)+MAX_SAY_NPC_TEXT)
//////////////////////////////////////////////////////////////////////////
//
// DWORD answer_i
// char answer_s[MAX_SAY_NPC_TEXT]
//////////////////////////////////////////////////////////////////////////


#define NETMSG_DROP_TIMERS          MAKE_NETMSG_HEADER(129)
#define NETMSG_DROP_TIMERS_SIZE     (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_REFRESH_ME      MAKE_NETMSG_HEADER(130)
#define NETMSG_SEND_REFRESH_ME_SIZE (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_VIEW_MAP             MAKE_NETMSG_HEADER(131)
#define NETMSG_VIEW_MAP_SIZE        (sizeof(MSGTYPE)+sizeof(WORD)*2+sizeof(DWORD)*2)
//////////////////////////////////////////////////////////////////////////
//
// WORD hx, hy
// DWORD loc_id, loc_ent
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// GLOBAL
//************************************************************************

#define NETMSG_SEND_GIVE_GLOBAL_INFO MAKE_NETMSG_HEADER(134)
#define NETMSG_SEND_GIVE_GLOBAL_INFO_SIZE (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
// запрос инфы по глобалу
// BYTE info_flag; (see GM Info in FOdefines.h)
//////////////////////////////////////////////////////////////////////////

#define NETMSG_GLOBAL_INFO          MAKE_NETMSG_HEADER(135)
//////////////////////////////////////////////////////////////////////////
// инфа по глобалу, see LocToSend in FOdefines.h
// DWORD msg_len
// BYTE info_flag; (see GM info in FOdefines.h)
// Data
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_RULE_GLOBAL     MAKE_NETMSG_HEADER(136)
#define NETMSG_SEND_RULE_GLOBAL_SIZE (sizeof(MSGTYPE)+sizeof(BYTE)+\
sizeof(DWORD)*2)
//////////////////////////////////////////////////////////////////////////
// управление группой
// BYTE command; (see GM Rule command in FOdefines.h)
// DWORD param1;
// DWORD param2;
//////////////////////////////////////////////////////////////////////////

#define NETMSG_FOLLOW               MAKE_NETMSG_HEADER(137)
#define NETMSG_FOLLOW_SIZE          (sizeof(MSGTYPE)+sizeof(DWORD)+\
sizeof(BYTE)+sizeof(WORD)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// DWORD follow_rule
// BYTE follow_type (see Follow in FOdefines.h)
// WORD map_pid
// DWORD waiting_time
//////////////////////////////////////////////////////////////////////////

#define NETMSG_GLOBAL_ENTRANCES     MAKE_NETMSG_HEADER(138)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
// DWORD loc_id
// BYTE count
//  BYTE entrances[count]
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// Cached data
//************************************************************************

#define NETMSG_MSG_DATA             MAKE_NETMSG_HEADER(141)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
// DWORD MSG_language
// WORD MSG_num
// DWORD MSG_hash
// char[msg_len-(sizeof(MSGTYPE)-sizeof(msg_len)-sizeof(MSG_language)-
//	sizeof(MSG_num)-sizeof(MSG_hash))] MSG_data
//	DWORD num
//	DWORD len
//	char data[len]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_ITEM_PROTOS          MAKE_NETMSG_HEADER(142)
//////////////////////////////////////////////////////////////////////////
//
// DWORD msg_len
// BYTE item_type
// DWORD item_hash
// ProtoItem[(msg_len-sizeof(MSGTYPE)-sizeof(msg_len)-sizeof(item_type)-
//	sizeof(msg_hash))/sizeof(ProtoItem)]
//////////////////////////////////////////////////////////////////////////

//************************************************************************
// Quest, Holodisk info, Automaps info
//************************************************************************

#define NETMSG_QUEST                MAKE_NETMSG_HEADER(161)
#define NETMSG_QUEST_SIZE			(sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// DWORD str_num
//////////////////////////////////////////////////////////////////////////

#define NETMSG_QUESTS               MAKE_NETMSG_HEADER(162)
//////////////////////////////////////////////////////////////////////////
// DWORD msg_len
// DWORD num_count
//	for num_count
//	DWORD str_num
//////////////////////////////////////////////////////////////////////////

#define NETMSG_HOLO_INFO            MAKE_NETMSG_HEADER(163)
//////////////////////////////////////////////////////////////////////////
// DWORD msg_len
// bool clear
// WORD offset
// WORD num_count
//	for num_count
//	WORD holo_num
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SET_USER_HOLO_STR MAKE_NETMSG_HEADER(164)
//////////////////////////////////////////////////////////////////////////
// DWORD msg_len
// DWORD holodisk_id
// WORD title_len
// WORD text_len
// char title[title_len]
// char text[text_len]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GET_USER_HOLO_STR MAKE_NETMSG_HEADER(165)
#define NETMSG_SEND_GET_USER_HOLO_STR_SIZE (sizeof(MSGTYPE)+sizeof(DWORD))
//////////////////////////////////////////////////////////////////////////
// DWORD str_num
//////////////////////////////////////////////////////////////////////////

#define NETMSG_USER_HOLO_STR        MAKE_NETMSG_HEADER(166)
//////////////////////////////////////////////////////////////////////////
// DWORD msg_len
// DWORD str_num
// WORD text_len
// char text[text_len]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GET_SCORES      MAKE_NETMSG_HEADER(167)
#define NETMSG_SEND_GET_SCORES_SIZE (sizeof(MSGTYPE))
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

#define NETMSG_SCORES               MAKE_NETMSG_HEADER(168)
#define NETMSG_SCORES_SIZE          (sizeof(MSGTYPE)+SCORE_NAME_LEN*SCORES_MAX)
//////////////////////////////////////////////////////////////////////////
// for SCORES_MAX
//	char[MAX_NAME] client_name - without null-terminated character
//////////////////////////////////////////////////////////////////////////

#define NETMSG_AUTOMAPS_INFO        MAKE_NETMSG_HEADER(170)
//////////////////////////////////////////////////////////////////////////
// Automaps information
// DWORD msg_len
// bool clear_list
// WORD locations_count
//  for locations_count
//  DWORD location_id
//  WORD location_pid
//  WORD maps_count
//   for maps_count
//   WORD map_pid
//////////////////////////////////////////////////////////////////////////

#endif // __NET_PROTOCOL__