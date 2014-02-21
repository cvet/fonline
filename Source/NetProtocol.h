#ifndef __NET_PROTOCOL__
#define __NET_PROTOCOL__

#include "Defines.h"

/************************************************************************/
/* Base                                                                 */
/************************************************************************/

#define FO_PROTOCOL_VERSION                   ( 0xF102 )    // FOnline Protocol Version
#define MAKE_NETMSG_HEADER( number )    ( (uint) ( ( 0x5EAD << 17 ) | ( number << 8 ) | ( 0xAA ) ) )
#define PING_CLIENT_LIFE_TIME                 ( 15000 )     // Time to ping client life

// Special message
// 0xFFFFFFFF - ping, answer
// 4 - players in game
// 12 - reserved

// Fixed headers
// NETMSG_LOGIN
// NETMSG_CREATE_CLIENT
// NETMSG_NET_SETTINGS

// ************************************************************************
// LOGIN MESSAGES
// ************************************************************************

#define NETMSG_LOGIN                          MAKE_NETMSG_HEADER( 1 )
#define NETMSG_LOGIN_SIZE                                               \
    ( sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) * 8 /*UIDs*/ + \
      UTF8_BUF_SIZE( MAX_NAME ) + PASS_HASH_SIZE + sizeof( uint ) + sizeof( uint ) * 10 /*MSG*/ + sizeof( uint ) * 14 /*Proto*/ + sizeof( uchar ) + 100 )
// ////////////////////////////////////////////////////////////////////////
// Enter to game
// Params:
// ushort protocol_version
// !uint uid4
// char name[MAX_NAME]
// !uint uid1
// char pass_hash[PASS_HASH_SIZE]
// uint msg_language
// uint hash_msg_game[TEXTMSG_COUNT]
// !uint uidxor
// !uint uid3
// !uint uid2
// !uint uidor
// uint hash_proto[ITEM_MAX_TYPES]
// !uint uidcalc
// uchar default_combat_mode
// !uint uid0
// char[100] - reserved
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGIN_SUCCESS                  MAKE_NETMSG_HEADER( 2 )
#define NETMSG_LOGIN_SUCCESS_SIZE             ( sizeof( uint ) + sizeof( uint ) * 2 )
// ////////////////////////////////////////////////////////////////////////
// Login accepted
// uint bin_seed
// uint bout_seed
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_WRONG_NET_PROTO                MAKE_NETMSG_HEADER( 8 )
#define NETMSG_WRONG_NET_PROTO_SIZE           ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Wrong network protocol
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CREATE_CLIENT                  MAKE_NETMSG_HEADER( 3 )
// ////////////////////////////////////////////////////////////////////////
// Registration query
// Params:
// uint mag_len
// ushort proto_ver
// MAX_NAME name
// char pass_hash[PASS_HASH_SIZE]
// ushort params_count
//  ushort param_index
//  int param_val
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REGISTER_SUCCESS               MAKE_NETMSG_HEADER( 4 )
#define NETMSG_REGISTER_SUCCESS_SIZE          ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Answer about successes registration
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PING                           MAKE_NETMSG_HEADER( 5 )
#define NETMSG_PING_SIZE                      ( sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
// Ping
// uchar ping (see Ping in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_END_PARSE_TO_GAME              MAKE_NETMSG_HEADER( 7 )
#define NETMSG_END_PARSE_TO_GAME_SIZE         ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SINGLEPLAYER_SAVE_LOAD         MAKE_NETMSG_HEADER( 10 )
// ////////////////////////////////////////////////////////////////////////
// Singleplayer
// uint msg_len
// bool save
// ushort fname_len
// char fname[fname_len]
// if save
//  uint save_pic_len
//  uchar save_pic[save_pic_len]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CHECK_UID0                     MAKE_NETMSG_HEADER( 6 )
#define NETMSG_CHECK_UID1                     MAKE_NETMSG_HEADER( 30 )
#define NETMSG_CHECK_UID2                     MAKE_NETMSG_HEADER( 78 )
#define NETMSG_CHECK_UID3                     MAKE_NETMSG_HEADER( 139 )
#define NETMSG_CHECK_UID4                     MAKE_NETMSG_HEADER( 211 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uint uid3
// uint xor for uid0
// uchar rnd_count, 1..21
// uint uid1
// uint xor for uid2
// char rnd_data[rnd_count] - random numbers
// uint uid2
// uint xor for uid1
// uint uid4
// uchar rnd_count2, 2..12
// uint xor for uid3
// uint xor for uid4
// uint uid0
// char rnd_data2[rnd_count] - random numbers
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CHECK_UID_EXT                  MAKE_NETMSG_HEADER( 140 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Update
// ************************************************************************

#define NETMSG_UPDATE                         MAKE_NETMSG_HEADER( 14 )
#define NETMSG_UPDATE_SIZE                    ( sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Request to update
// ushort protocol_version
// uint encrypt_key
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_UPDATE_FILES_LIST              MAKE_NETMSG_HEADER( 15 )
// ////////////////////////////////////////////////////////////////////////
// Files list to update
// uint msg_len
// uint files_count
//   short path_len
//   char path[path_len]
//   uint size
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GET_UPDATE_FILE                MAKE_NETMSG_HEADER( 16 )
#define NETMSG_GET_UPDATE_FILE_SIZE           ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Request to updated file
// uint file_number
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GET_UPDATE_FILE_DATA           MAKE_NETMSG_HEADER( 17 )
#define NETMSG_GET_UPDATE_FILE_DATA_SIZE      ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Request to update file data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_UPDATE_FILE_DATA               MAKE_NETMSG_HEADER( 18 )
#define NETMSG_UPDATE_FILE_DATA_SIZE          ( sizeof( uint ) + FILE_UPDATE_PORTION )
// ////////////////////////////////////////////////////////////////////////
// Portion of data
// uchar data[FILE_UPDATE_PORTION]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ADD/REMOVE CRITTER
// ************************************************************************

#define NETMSG_ADD_PLAYER                     MAKE_NETMSG_HEADER( 11 )
// ////////////////////////////////////////////////////////////////////////
// Add player on map.
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_NPC                        MAKE_NETMSG_HEADER( 12 )
// ////////////////////////////////////////////////////////////////////////
// Add npc on map.
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_CRITTER                 MAKE_NETMSG_HEADER( 13 )
#define NETMSG_REMOVE_CRITTER_SIZE            ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Remove critter from map.
// Params:
// uint crid
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Commands, lexems
// ************************************************************************

#define NETMSG_SEND_COMMAND                   MAKE_NETMSG_HEADER( 21 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uchar cmd (see Commands in Access.h)
// Ext parameters
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_LEXEMS                 MAKE_NETMSG_HEADER( 22 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint critter_id
// ushort lexems_length
// char lexems[lexems_length]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ITEM_LEXEMS                    MAKE_NETMSG_HEADER( 23 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint item_id
// ushort lexems_length
// char lexems[lexems_length]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// TEXT
// ************************************************************************

#define NETMSG_SEND_TEXT                      MAKE_NETMSG_HEADER( 31 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uchar how_say (see Say types in FOdefines.h)
// ushort len
// char[len] str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_TEXT                   MAKE_NETMSG_HEADER( 32 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// CrID crid
// uchar how_say
// ushort intellect
// bool unsafe_text
// ushort len
// char[len] str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG                            MAKE_NETMSG_HEADER( 33 )
#define NETMSG_MSG_SIZE                 \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( uchar ) + sizeof( ushort ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// CrID crid
// uchar how_say
// ushort MSG_num
// uint num_str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG_LEX                        MAKE_NETMSG_HEADER( 34 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// CrID crid
// uchar how_say
// ushort MSG_num
// uint num_str
// ushort lex_len
// char lexems[lex_len]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT                       MAKE_NETMSG_HEADER( 35 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ushort hx
// ushort hy
// uint color
// ushort len
// char[len] str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG                   MAKE_NETMSG_HEADER( 36 )
#define NETMSG_MAP_TEXT_MSG_SIZE              \
    ( sizeof( uint ) + sizeof( ushort ) * 2 + \
      sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ushort hx
// ushort hy
// uint color
// ushort MSG_num
// uint num_str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP_TEXT_MSG_LEX               MAKE_NETMSG_HEADER( 37 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ushort hx
// ushort hy
// uint color
// ushort MSG_num
// uint num_str
// ushort lexems_len
// char lexems[lexems_len]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// DIR/MOVE
// ************************************************************************

#define NETMSG_DIR                            MAKE_NETMSG_HEADER( 41 )
#define NETMSG_DIR_SIZE                       ( sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uchar dir
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_DIR                    MAKE_NETMSG_HEADER( 42 )
#define NETMSG_CRITTER_DIR_SIZE               ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// CrID id
// uchar dir
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE_WALK                 MAKE_NETMSG_HEADER( 43 )
#define NETMSG_SEND_MOVE_WALK_SIZE            ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint move_params
// ushort hx
// ushort hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE_RUN                  MAKE_NETMSG_HEADER( 44 )
#define NETMSG_SEND_MOVE_RUN_SIZE             ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint move_params
// ushort hx
// ushort hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE                   MAKE_NETMSG_HEADER( 45 )
#define NETMSG_CRITTER_MOVE_SIZE              ( sizeof( uint ) + sizeof( uint ) * 2 + sizeof( ushort ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint id
// uint move_params
// ushort hx
// ushort hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_XY                     MAKE_NETMSG_HEADER( 46 )
#define NETMSG_CRITTER_XY_SIZE          \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( ushort ) * 2 + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// CrID crid
// ushort hex_x
// ushort hex_y
// uchar dir
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CHOSEN Params
// ************************************************************************

#define NETMSG_ALL_PARAMS                     MAKE_NETMSG_HEADER( 51 )
#define NETMSG_ALL_PARAMS_SIZE                ( sizeof( uint ) + MAX_PARAMS * sizeof( int ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// parameters[]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PARAM                          MAKE_NETMSG_HEADER( 52 )
#define NETMSG_PARAM_SIZE                     ( sizeof( uint ) + sizeof( ushort ) + sizeof( int ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ushort num_param
// int val
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_PARAM                  MAKE_NETMSG_HEADER( 53 )
#define NETMSG_CRITTER_PARAM_SIZE             ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) + sizeof( int ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// ushort num_param
// int val
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_LEVELUP                   MAKE_NETMSG_HEADER( 60 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// ushort count_skill_up
//	for count_skill_up
//	ushort num_skill
//	ushort val_up
// ushort perk_up
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRAFT_ASK                      MAKE_NETMSG_HEADER( 61 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// ushort count
// uint crafts_nums[count]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_CRAFT                     MAKE_NETMSG_HEADER( 62 )
#define NETMSG_SEND_CRAFT_SIZE                ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// uint craft_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRAFT_RESULT                   MAKE_NETMSG_HEADER( 63 )
#define NETMSG_CRAFT_RESULT_SIZE              ( sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// uchar craft_result (see Craft results in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CHOSEN ITEMS
// ************************************************************************

#define NETMSG_CLEAR_ITEMS                    MAKE_NETMSG_HEADER( 64 )
#define NETMSG_CLEAR_ITEMS_SIZE               ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_ITEM                       MAKE_NETMSG_HEADER( 65 )
#define NETMSG_ADD_ITEM_SIZE            \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( ushort ) + sizeof( uchar ) + 120 /*ItemData*/ )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint id
// ushort pid
// uchar slot
// Item::ItemData Data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_ITEM                    MAKE_NETMSG_HEADER( 66 )
#define NETMSG_REMOVE_ITEM_SIZE               ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SORT_VALUE_ITEM           MAKE_NETMSG_HEADER( 68 )
#define NETMSG_SEND_SORT_VALUE_ITEM_SIZE      ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// ushort sort_val
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ALL_ITEMS_SEND                 MAKE_NETMSG_HEADER( 69 )
#define NETMSG_ALL_ITEMS_SEND_SIZE            ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ITEMS ON MAP
// ************************************************************************

#define NETMSG_ADD_ITEM_ON_MAP                MAKE_NETMSG_HEADER( 71 )
#define NETMSG_ADD_ITEM_ON_MAP_SIZE     \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( ushort ) * 3 + sizeof( uchar ) + 120 /*ItemData*/ )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// ushort item_pid
// ushort item_x
// ushort item_y
// uchar is_added
// Item::ItemData data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CHANGE_ITEM_ON_MAP             MAKE_NETMSG_HEADER( 72 )
#define NETMSG_CHANGE_ITEM_ON_MAP_SIZE        ( sizeof( uint ) + sizeof( uint ) + 120 /*ItemData*/ )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// Item::ItemData data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_RATE_ITEM                 MAKE_NETMSG_HEADER( 73 )
#define NETMSG_SEND_RATE_ITEM_SIZE            ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint rate of main item
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ERASE_ITEM_FROM_MAP            MAKE_NETMSG_HEADER( 74 )
#define NETMSG_ERASE_ITEM_FROM_MAP_SIZE       ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// uchar is_deleted
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ANIMATE_ITEM                   MAKE_NETMSG_HEADER( 75 )
#define NETMSG_ANIMATE_ITEM_SIZE              ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) * 2 )
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

#define NETMSG_SEND_CHANGE_ITEM               MAKE_NETMSG_HEADER( 81 )
#define NETMSG_SEND_CHANGE_ITEM_SIZE                                            \
    ( sizeof( uint ) + sizeof( uchar ) + sizeof( uint ) + sizeof( uchar ) * 2 + \
      sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uchar ap
// uint item_id
// uchar from_slot
// uchar to_slot
// uint count
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_PICK_ITEM                 MAKE_NETMSG_HEADER( 82 )
#define NETMSG_SEND_PICK_ITEM_SIZE            ( sizeof( uint ) + sizeof( ushort ) * 3 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// ushort targ_x
// ushort targ_y
// ushort pid
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CONTAINER_INFO                 MAKE_NETMSG_HEADER( 83 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uchar transfer_type
// uint talk_time
// uint cont_id
// ushort cont_pid or barter_k
// uint count_items
//	while(cont_items)
//	uint item_id
//	ushort item_pid
//	uint item_count
//	Data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_ITEM_CONT                 MAKE_NETMSG_HEADER( 84 )
#define NETMSG_SEND_ITEM_CONT_SIZE                            \
    ( sizeof( uint ) + sizeof( uchar ) + sizeof( uint ) * 2 + \
      sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uchar transfer_type (see Transfer types in FOdefines.h)
// uint cont_id
// uint item_id
// uint item_count
// uchar take_flags (see Take flags in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_USE_ITEM                  MAKE_NETMSG_HEADER( 85 )
#define NETMSG_SEND_USE_ITEM_SIZE                         \
    ( sizeof( uint ) + sizeof( uchar ) + sizeof( uint ) + \
      sizeof( ushort ) + sizeof( uchar ) + sizeof( uchar ) + sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Use some item.
// Params:
// uchar ap
// uint item_id
// ushort item_pid
// uchar rate
// uchar target_type
// uint target_id
// ushort target_pid
// uint param
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_USE_SKILL                 MAKE_NETMSG_HEADER( 86 )
#define NETMSG_SEND_USE_SKILL_SIZE                          \
    ( sizeof( uint ) + sizeof( ushort ) + sizeof( uchar ) + \
      sizeof( uint ) + sizeof( ushort ) )
// ////////////////////////////////////////////////////////////////////////
// Use some skill.
// Params:
// ushort skill
// uchar targ_type
// uint target_id
// ushort target_pid
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_PICK_CRITTER              MAKE_NETMSG_HEADER( 87 )
#define NETMSG_SEND_PICK_CRITTER_SIZE         ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
// Critter picking.
// Params:
// uint crid
// uchar pick_type (see Pick types in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CRITTER ACTION
// ************************************************************************

#define NETMSG_SOME_ITEM                      MAKE_NETMSG_HEADER( 90 )
#define NETMSG_SOME_ITEM_SIZE                              \
    ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) + \
      sizeof( uchar ) + 120 /*ItemData*/ )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// ushort item_pid
// uchar slot
// Item::ItemData data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ACTION                 MAKE_NETMSG_HEADER( 91 )
#define NETMSG_CRITTER_ACTION_SIZE      \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( int ) * 2 + sizeof( bool ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// int action
// int action_ext
// bool is_item
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_KNOCKOUT               MAKE_NETMSG_HEADER( 92 )
#define NETMSG_CRITTER_KNOCKOUT_SIZE    \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( uint ) * 2 + sizeof( ushort ) + sizeof( ushort ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// uint anim2begin
// uint anim2idle
// ushort knock_hx
// ushort knock_hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE_ITEM              MAKE_NETMSG_HEADER( 93 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ITEM_DATA              MAKE_NETMSG_HEADER( 94 )
#define NETMSG_CRITTER_ITEM_DATA_SIZE   \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( uchar ) + 120 /*ItemData*/ )
// ////////////////////////////////////////////////////////////////////////
// Item data changed
// uint crid
// uchar slot
// Item::ItemData data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ANIMATE                MAKE_NETMSG_HEADER( 95 )
#define NETMSG_CRITTER_ANIMATE_SIZE         \
    ( sizeof( uint ) + sizeof( uint ) * 3 + \
      sizeof( bool ) + sizeof( bool ) * 2 )
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

#define NETMSG_CRITTER_SET_ANIMS              MAKE_NETMSG_HEADER( 96 )
#define NETMSG_CRITTER_SET_ANIMS_SIZE         ( sizeof( uint ) + sizeof( int ) + sizeof( uint ) * 3 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// int cond
// uint ind1
// uint ind2
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_COMBAT_RESULTS                 MAKE_NETMSG_HEADER( 97 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_EFFECT                         MAKE_NETMSG_HEADER( 98 )
#define NETMSG_EFFECT_SIZE                    ( sizeof( uint ) + sizeof( ushort ) * 4 )
// ////////////////////////////////////////////////////////////////////////
// explode
// Params:
// ushort eff_pid
// ushort hex_x
// ushort hex_y
// ushort radius
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_FLY_EFFECT                     MAKE_NETMSG_HEADER( 99 )
#define NETMSG_FLY_EFFECT_SIZE            \
    ( sizeof( uint ) + sizeof( ushort ) + \
      sizeof( uint ) * 2 + sizeof( ushort ) * 4 )
// ////////////////////////////////////////////////////////////////////////
// shoot
// Params:
// ushort eff_pid
// CrID eff_cr1_id
// CrID eff_cr2_id
// ushort eff_cr1_hx
// ushort eff_cr1_hy
// ushort eff_cr2_hx
// ushort eff_cr2_hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAY_SOUND                     MAKE_NETMSG_HEADER( 101 )
#define NETMSG_PLAY_SOUND_SIZE                ( sizeof( uint ) + sizeof( uint ) + 100 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid_synchronize
// char sound_name[16]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAY_SOUND_TYPE                MAKE_NETMSG_HEADER( 102 )
#define NETMSG_PLAY_SOUND_TYPE_SIZE           ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) * 4 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid_synchronize
// uchar sound_type
// uchar sound_type_ext
// uchar sound_id
// uchar sound_id_ext
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_KARMA_VOTING              MAKE_NETMSG_HEADER( 103 )
#define NETMSG_SEND_KARMA_VOTING_SIZE         ( sizeof( uint ) + sizeof( uint ) + sizeof( bool ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// bool val_up
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// TALK&BARTER NPC
// ************************************************************************

#define NETMSG_SEND_TALK_NPC                  MAKE_NETMSG_HEADER( 109 )
#define NETMSG_SEND_TALK_NPC_SIZE        \
    ( sizeof( uint ) + sizeof( uchar ) + \
      sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// uchar is_npc
// uint id_talk
// uchar answer - see Answer in FOdefines.h
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SAY_NPC                   MAKE_NETMSG_HEADER( 110 )
#define NETMSG_SEND_SAY_NPC_SIZE         \
    ( sizeof( uint ) + sizeof( uchar ) + \
      sizeof( uint ) + MAX_SAY_NPC_TEXT )
// ////////////////////////////////////////////////////////////////////////
// say about
// uchar is_npc
// uint id_talk
// char str[MAX_NET_TEXT] - see Answer in FOdefines.h
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_TALK_NPC                       MAKE_NETMSG_HEADER( 111 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uchar is_npc
// uint crid
// uchar all_answers
// uint main_text
// uint answ_text
// uint talk_time
// ushort lexems_length
// char[lexems_length] lexems
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_BARTER                    MAKE_NETMSG_HEADER( 112 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// CrID crid
//
// ushort count_sale
// for count_sale
//	uint item_id
//	uint item_count
//
// ushort count_buy
// for count_buy
//	uint item_id
//	uint item_count
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAYERS_BARTER                 MAKE_NETMSG_HEADER( 114 )
#define NETMSG_PLAYERS_BARTER_SIZE       \
    ( sizeof( uint ) + sizeof( uchar ) + \
      sizeof( uint ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// uchar barter
// uint param
// uint param_ext
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PLAYERS_BARTER_SET_HIDE        MAKE_NETMSG_HEADER( 115 )
#define NETMSG_PLAYERS_BARTER_SET_HIDE_SIZE \
    ( sizeof( uint ) + sizeof( uint ) +     \
      sizeof( ushort ) + sizeof( uint ) + 120 /*ItemData*/ )
// ////////////////////////////////////////////////////////////////////////
//
// uint id
// ushort pid
// uint count
// Item::ItemData data
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// GAMETIME
// ************************************************************************

#define NETMSG_SEND_GET_INFO                  MAKE_NETMSG_HEADER( 116 )
#define NETMSG_SEND_GET_TIME_SIZE             ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GAME_INFO                      MAKE_NETMSG_HEADER( 117 )
#define NETMSG_GAME_INFO_SIZE                 \
    ( sizeof( uint ) + sizeof( ushort ) * 8 + \
      sizeof( int ) + sizeof( uchar ) + sizeof( bool ) * 2 + sizeof( int ) * 4 + sizeof( uchar ) * 12 )
// ////////////////////////////////////////////////////////////////////////
// Generic game info
// ushort GameOpt.YearStart;
// ushort GameOpt.Year;
// ushort GameOpt.Month;
// ushort GameOpt.Day;
// ushort GameOpt.Hour;
// ushort GameOpt.Minute;
// ushort GameOpt.Second;
// ushort GameOpt.TimeMultiplier;
// int time
// uchar rain
// bool turn_based
// bool no_log_out
// int day_time[4]
// uchar day_color[12]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_COMBAT                    MAKE_NETMSG_HEADER( 118 )
#define NETMSG_SEND_COMBAT_SIZE               ( sizeof( uint ) + sizeof( uchar ) + sizeof( int ) )
// ////////////////////////////////////////////////////////////////////////
// Turn based
// uchar type (see Combat in FOdefines.h)
// int val
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// MAP
// ************************************************************************

#define NETMSG_LOADMAP                        MAKE_NETMSG_HEADER( 121 )
#define NETMSG_LOADMAP_SIZE               \
    ( sizeof( uint ) + sizeof( ushort ) + \
      sizeof( int ) + sizeof( uchar ) + sizeof( uint ) * 3 )
// ////////////////////////////////////////////////////////////////////////
//
// ushort num_pid
// int map_time
// uchar map_rain
// uint ver_tiles
// uint ver_walls
// uint ver_scen
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP                            MAKE_NETMSG_HEADER( 122 )
// ////////////////////////////////////////////////////////////////////////
// Map data
// uint msg_len
// ushort pid_map
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

#define NETMSG_SEND_GIVE_MAP                  MAKE_NETMSG_HEADER( 123 )
#define NETMSG_SEND_GIVE_MAP_SIZE                          \
    ( sizeof( uint ) + sizeof( bool ) + sizeof( ushort ) + \
      sizeof( uint ) + sizeof( uint ) * 3 )
// ////////////////////////////////////////////////////////////////////////
// Request on map data, on map loading or for automap
// bool automap
// ushort map_pid
// uint loc_id
// uint ver_tiles
// uint ver_walls
// uint ver_scen
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_LOAD_MAP_OK               MAKE_NETMSG_HEADER( 124 )
#define NETMSG_SEND_LOAD_MAP_OK_SIZE          ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SHOW_SCREEN                    MAKE_NETMSG_HEADER( 125 )
#define NETMSG_SHOW_SCREEN_SIZE               ( sizeof( uint ) + sizeof( int ) + sizeof( uint ) + sizeof( bool ) )
// ////////////////////////////////////////////////////////////////////////
//
// int screen_type
// uint param
// bool need_answer
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_RUN_CLIENT_SCRIPT              MAKE_NETMSG_HEADER( 126 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_RUN_SERVER_SCRIPT         MAKE_NETMSG_HEADER( 127 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SCREEN_ANSWER             MAKE_NETMSG_HEADER( 128 )
#define NETMSG_SEND_SCREEN_ANSWER_SIZE        ( sizeof( uint ) + sizeof( uint ) + MAX_SAY_NPC_TEXT )
// ////////////////////////////////////////////////////////////////////////
//
// uint answer_i
// char answer_s[MAX_SAY_NPC_TEXT]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_DROP_TIMERS                    MAKE_NETMSG_HEADER( 129 )
#define NETMSG_DROP_TIMERS_SIZE               ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_REFRESH_ME                MAKE_NETMSG_HEADER( 130 )
#define NETMSG_SEND_REFRESH_ME_SIZE           ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_VIEW_MAP                       MAKE_NETMSG_HEADER( 131 )
#define NETMSG_VIEW_MAP_SIZE                  ( sizeof( uint ) + sizeof( ushort ) * 2 + sizeof( uint ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// ushort hx, hy
// uint loc_id, loc_ent
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// GLOBAL
// ************************************************************************

#define NETMSG_SEND_GIVE_GLOBAL_INFO          MAKE_NETMSG_HEADER( 134 )
#define NETMSG_SEND_GIVE_GLOBAL_INFO_SIZE     ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// uchar info_flag; (see GM Info in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GLOBAL_INFO                    MAKE_NETMSG_HEADER( 135 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uchar info_flag; (see GM info in FOdefines.h)
// Data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_RULE_GLOBAL               MAKE_NETMSG_HEADER( 136 )
#define NETMSG_SEND_RULE_GLOBAL_SIZE     \
    ( sizeof( uint ) + sizeof( uchar ) + \
      sizeof( uint ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// uchar command; (see GM Rule command in FOdefines.h)
// uint param1;
// uint param2;
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_FOLLOW                         MAKE_NETMSG_HEADER( 137 )
#define NETMSG_FOLLOW_SIZE              \
    ( sizeof( uint ) + sizeof( uint ) + \
      sizeof( uchar ) + sizeof( ushort ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// uint follow_rule
// uchar follow_type (see Follow in FOdefines.h)
// ushort map_pid
// uint waiting_time
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GLOBAL_ENTRANCES               MAKE_NETMSG_HEADER( 138 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uint loc_id
// uchar count
//  uchar entrances[count]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Quest, Holodisk info, Automaps info
// ************************************************************************

#define NETMSG_QUEST                          MAKE_NETMSG_HEADER( 161 )
#define NETMSG_QUEST_SIZE                     ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// uint str_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_QUESTS                         MAKE_NETMSG_HEADER( 162 )
// ////////////////////////////////////////////////////////////////////////
// uint msg_len
// uint num_count
//	for num_count
//	uint str_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_HOLO_INFO                      MAKE_NETMSG_HEADER( 163 )
// ////////////////////////////////////////////////////////////////////////
// uint msg_len
// bool clear
// ushort offset
// ushort num_count
//	for num_count
//	ushort holo_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_SET_USER_HOLO_STR         MAKE_NETMSG_HEADER( 164 )
// ////////////////////////////////////////////////////////////////////////
// uint msg_len
// uint holodisk_id
// ushort title_len
// ushort text_len
// char title[title_len]
// char text[text_len]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GET_USER_HOLO_STR         MAKE_NETMSG_HEADER( 165 )
#define NETMSG_SEND_GET_USER_HOLO_STR_SIZE    ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// uint str_num
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_USER_HOLO_STR                  MAKE_NETMSG_HEADER( 166 )
// ////////////////////////////////////////////////////////////////////////
// uint msg_len
// uint str_num
// ushort text_len
// char text[text_len]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_GET_SCORES                MAKE_NETMSG_HEADER( 167 )
#define NETMSG_SEND_GET_SCORES_SIZE           ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SCORES                         MAKE_NETMSG_HEADER( 168 )
#define NETMSG_SCORES_SIZE                    ( sizeof( uint ) + SCORE_NAME_LEN * SCORES_MAX )
// ////////////////////////////////////////////////////////////////////////
// for SCORES_MAX
//	char[MAX_NAME] client_name - without null-terminated character
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_AUTOMAPS_INFO                  MAKE_NETMSG_HEADER( 170 )
// ////////////////////////////////////////////////////////////////////////
// Automaps information
// uint msg_len
// bool clear_list
// ushort locations_count
//  for locations_count
//  uint location_id
//  ushort location_pid
//  ushort maps_count
//   for maps_count
//   ushort map_pid
// ////////////////////////////////////////////////////////////////////////

#endif // __NET_PROTOCOL__
