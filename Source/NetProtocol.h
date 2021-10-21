#ifndef __NET_PROTOCOL__
#define __NET_PROTOCOL__

#include "Defines.h"

/************************************************************************/
/* Base                                                                 */
/************************************************************************/

#define MAKE_NETMSG_HEADER( number )    ( (uint) ( ( 0x5EAD << 16 ) | ( ( number ) << 8 ) | ( 0xAA ) ) )
#define PING_CLIENT_LIFE_TIME               ( 15000 )       // Time to ping client life

// Special message
// 0xFFFFFFFF - ping, answer
// 4 - players in game
// 12 - reserved

// ************************************************************************
// LOGIN MESSAGES
// ************************************************************************

#define NETMSG_DISCONNECT                   MAKE_NETMSG_HEADER( 10 )
#define NETMSG_DISCONNECT_SIZE              ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Disconnect
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGIN                        MAKE_NETMSG_HEADER( 1 )
#define NETMSG_LOGIN_SIZE \
    ( sizeof( uint ) + sizeof( ushort ) + UTF8_BUF_SIZE( MAX_NAME ) * 2 + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Enter to game
// Params:
// ushort protocol_version
// char name[MAX_NAME]
// char pass[MAX_NAME]
// uint msg_language
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGIN_SUCCESS                MAKE_NETMSG_HEADER( 2 )
// ////////////////////////////////////////////////////////////////////////
// Login accepted
// uint bin_seed
// uint bout_seed
// Properties global
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_WRONG_NET_PROTO              MAKE_NETMSG_HEADER( 8 )
#define NETMSG_WRONG_NET_PROTO_SIZE         ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Wrong network protocol
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CREATE_CLIENT                MAKE_NETMSG_HEADER( 3 )
// ////////////////////////////////////////////////////////////////////////
// Registration query
// Params:
// uint mag_len
// ushort proto_ver
// MAX_NAME name
// MAX_NAME password
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REGISTER_SUCCESS             MAKE_NETMSG_HEADER( 4 )
#define NETMSG_REGISTER_SUCCESS_SIZE        ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Answer about successes registration
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_PING                         MAKE_NETMSG_HEADER( 5 )
#define NETMSG_PING_SIZE                    ( sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
// Ping
// uchar ping (see Ping in FOdefines.h)
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_END_PARSE_TO_GAME            MAKE_NETMSG_HEADER( 7 )
#define NETMSG_END_PARSE_TO_GAME_SIZE       ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Update
// ************************************************************************

#define NETMSG_UPDATE                       MAKE_NETMSG_HEADER( 14 )
#define NETMSG_UPDATE_SIZE                  ( sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Request to update
// ushort protocol_version
// uint encrypt_key
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_UPDATE_FILES_LIST            MAKE_NETMSG_HEADER( 15 )
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

#define NETMSG_GET_UPDATE_FILE              MAKE_NETMSG_HEADER( 16 )
#define NETMSG_GET_UPDATE_FILE_SIZE         ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Request to updated file
// uint file_number
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GET_UPDATE_FILE_DATA         MAKE_NETMSG_HEADER( 17 )
#define NETMSG_GET_UPDATE_FILE_DATA_SIZE    ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Request to update file data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_UPDATE_FILE_DATA             MAKE_NETMSG_HEADER( 18 )
#define NETMSG_UPDATE_FILE_DATA_SIZE        ( sizeof( uint ) + FILE_UPDATE_PORTION )
// ////////////////////////////////////////////////////////////////////////
// Portion of data
// uchar data[FILE_UPDATE_PORTION]
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ADD/REMOVE CRITTER
// ************************************************************************

#define NETMSG_ADD_PLAYER                   MAKE_NETMSG_HEADER( 11 )
// ////////////////////////////////////////////////////////////////////////
// Add player on map.
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_NPC                      MAKE_NETMSG_HEADER( 12 )
// ////////////////////////////////////////////////////////////////////////
// Add npc on map.
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_CRITTER               MAKE_NETMSG_HEADER( 13 )
#define NETMSG_REMOVE_CRITTER_SIZE          ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Remove critter from map.
// Params:
// uint crid
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Commands
// ************************************************************************

#define NETMSG_SEND_COMMAND                 MAKE_NETMSG_HEADER( 21 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uchar cmd (see Commands in Access.h)
// Ext parameters
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// TEXT
// ************************************************************************

#define NETMSG_SEND_TEXT                    MAKE_NETMSG_HEADER( 31 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uchar how_say (see Say types in FOdefines.h)
// ushort len
// char[len] str
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_TEXT                 MAKE_NETMSG_HEADER( 32 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// CrID crid
// uchar how_say
// string text
// bool unsafe_text
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MSG                          MAKE_NETMSG_HEADER( 33 )
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

#define NETMSG_MSG_LEX                      MAKE_NETMSG_HEADER( 34 )
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

#define NETMSG_MAP_TEXT                     MAKE_NETMSG_HEADER( 35 )
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

#define NETMSG_MAP_TEXT_MSG                 MAKE_NETMSG_HEADER( 36 )
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

#define NETMSG_MAP_TEXT_MSG_LEX             MAKE_NETMSG_HEADER( 37 )
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

#define NETMSG_DIR                          MAKE_NETMSG_HEADER( 41 )
#define NETMSG_DIR_SIZE                     ( sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uchar dir
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_DIR                  MAKE_NETMSG_HEADER( 42 )
#define NETMSG_CRITTER_DIR_SIZE             ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// CrID id
// uchar dir
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE_WALK               MAKE_NETMSG_HEADER( 43 )
#define NETMSG_SEND_MOVE_WALK_SIZE          ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint move_params
// ushort hx
// ushort hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_MOVE_RUN                MAKE_NETMSG_HEADER( 44 )
#define NETMSG_SEND_MOVE_RUN_SIZE           ( sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint move_params
// ushort hx
// ushort hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_MOVE                 MAKE_NETMSG_HEADER( 45 )
#define NETMSG_CRITTER_MOVE_SIZE            ( sizeof( uint ) + sizeof( uint ) * 2 + sizeof( ushort ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint id
// uint move_params
// ushort hx
// ushort hy
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_XY                   MAKE_NETMSG_HEADER( 46 )
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

#define NETMSG_ALL_PROPERTIES               MAKE_NETMSG_HEADER( 51 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// Properties
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CUSTOM_COMMAND               MAKE_NETMSG_HEADER( 52 )
#define NETMSG_CUSTOM_COMMAND_SIZE          ( sizeof( uint ) + sizeof( ushort ) + sizeof( int ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// ushort cmd
// int val
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// CHOSEN ITEMS
// ************************************************************************

#define NETMSG_CLEAR_ITEMS                  MAKE_NETMSG_HEADER( 64 )
#define NETMSG_CLEAR_ITEMS_SIZE             ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_ITEM                     MAKE_NETMSG_HEADER( 65 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint id
// hash pid
// uchar slot
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_ITEM                  MAKE_NETMSG_HEADER( 66 )
#define NETMSG_REMOVE_ITEM_SIZE             ( sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ALL_ITEMS_SEND               MAKE_NETMSG_HEADER( 69 )
#define NETMSG_ALL_ITEMS_SEND_SIZE          ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// ITEMS ON MAP
// ************************************************************************

#define NETMSG_ADD_ITEM_ON_MAP              MAKE_NETMSG_HEADER( 71 )
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

#define NETMSG_ERASE_ITEM_FROM_MAP          MAKE_NETMSG_HEADER( 74 )
#define NETMSG_ERASE_ITEM_FROM_MAP_SIZE     ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint item_id
// uchar is_deleted
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ANIMATE_ITEM                 MAKE_NETMSG_HEADER( 75 )
#define NETMSG_ANIMATE_ITEM_SIZE            ( sizeof( uint ) + sizeof( uint ) + sizeof( uchar ) * 2 )
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

#define NETMSG_SOME_ITEMS                   MAKE_NETMSG_HEADER( 83 )
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

#define NETMSG_SOME_ITEM                    MAKE_NETMSG_HEADER( 90 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint item_id
// hash item_pid
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ACTION               MAKE_NETMSG_HEADER( 91 )
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

#define NETMSG_CRITTER_MOVE_ITEM            MAKE_NETMSG_HEADER( 93 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_CRITTER_ANIMATE              MAKE_NETMSG_HEADER( 95 )
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

#define NETMSG_CRITTER_SET_ANIMS            MAKE_NETMSG_HEADER( 96 )
#define NETMSG_CRITTER_SET_ANIMS_SIZE       ( sizeof( uint ) + sizeof( int ) + sizeof( uint ) * 3 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint crid
// int cond
// uint ind1
// uint ind2
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_COMBAT_RESULTS               MAKE_NETMSG_HEADER( 97 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_EFFECT                       MAKE_NETMSG_HEADER( 98 )
#define NETMSG_EFFECT_SIZE                  ( sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) * 3 )
// ////////////////////////////////////////////////////////////////////////
// explode
// Params:
// hash eff_pid
// ushort hex_x
// ushort hex_y
// ushort radius
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_FLY_EFFECT                   MAKE_NETMSG_HEADER( 99 )
#define NETMSG_FLY_EFFECT_SIZE          \
    ( sizeof( uint ) + sizeof( hash ) + \
      sizeof( uint ) * 2 + sizeof( ushort ) * 4 )
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

#define NETMSG_PLAY_SOUND                   MAKE_NETMSG_HEADER( 101 )
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

#define NETMSG_SEND_TALK_NPC                MAKE_NETMSG_HEADER( 109 )
#define NETMSG_SEND_TALK_NPC_SIZE        \
    ( sizeof( uint ) + sizeof( uchar ) + \
      sizeof( uint ) + sizeof( uchar ) )
// ////////////////////////////////////////////////////////////////////////
//
// uchar is_npc
// uint id_talk
// uchar answer - see Answer in FOdefines.h
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_TALK_NPC                     MAKE_NETMSG_HEADER( 111 )
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

// ************************************************************************
// GAMETIME
// ************************************************************************

#define NETMSG_SEND_GET_INFO                MAKE_NETMSG_HEADER( 116 )
#define NETMSG_SEND_GET_TIME_SIZE           ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_GAME_INFO                    MAKE_NETMSG_HEADER( 117 )
#define NETMSG_GAME_INFO_SIZE                 \
    ( sizeof( uint ) + sizeof( ushort ) * 8 + \
      sizeof( int ) + sizeof( uchar ) + sizeof( bool ) + sizeof( int ) * 4 + sizeof( uchar ) * 12 )
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

// ************************************************************************
// MAP
// ************************************************************************

#define NETMSG_LOADMAP                      MAKE_NETMSG_HEADER( 121 )
// ////////////////////////////////////////////////////////////////////////
//
// uint mag_len
// hash map_pid
// hash loc_pid
// uchar map_index_in_loc
// int map_time
// uchar map_rain
// uint ver_tiles
// uint ver_walls
// uint ver_scen
// Properties map
// Properties location
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_MAP                          MAKE_NETMSG_HEADER( 122 )
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

#define NETMSG_SEND_GIVE_MAP                MAKE_NETMSG_HEADER( 123 )
#define NETMSG_SEND_GIVE_MAP_SIZE                        \
    ( sizeof( uint ) + sizeof( bool ) + sizeof( hash ) + \
      sizeof( uint ) + sizeof( hash ) * 2 )
// ////////////////////////////////////////////////////////////////////////
// Request on map data, on map loading or for automap
// bool automap
// hash map_pid
// uint loc_id
// hash tiles_hash
// hash scen_hash
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_LOAD_MAP_OK             MAKE_NETMSG_HEADER( 124 )
#define NETMSG_SEND_LOAD_MAP_OK_SIZE        ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Message about successfully map loading
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_RPC                          MAKE_NETMSG_HEADER( 128 )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_REFRESH_ME              MAKE_NETMSG_HEADER( 130 )
#define NETMSG_SEND_REFRESH_ME_SIZE         ( sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
//
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_VIEW_MAP                     MAKE_NETMSG_HEADER( 131 )
#define NETMSG_VIEW_MAP_SIZE                ( sizeof( uint ) + sizeof( ushort ) * 2 + sizeof( uint ) * 2 )
// ////////////////////////////////////////////////////////////////////////
//
// ushort hx, hy
// uint loc_id, loc_ent
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// GLOBAL
// ************************************************************************

#define NETMSG_GLOBAL_INFO                  MAKE_NETMSG_HEADER( 135 )
// ////////////////////////////////////////////////////////////////////////
//
// uint msg_len
// uchar info_flag; (see GM info in FOdefines.h)
// Data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_ADD_CUSTOM_ENTITY                    MAKE_NETMSG_HEADER( 136 )
// ////////////////////////////////////////////////////////////////////////
//
// Params:
// uint msg_len
// uint entity_id
// uint entity_subtype
// hash entity_pid
// Properties data
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_REMOVE_CUSTOM_ENTITY               MAKE_NETMSG_HEADER( 137 )
#define NETMSG_REMOVE_CUSTOM_ENTITY_SIZE          ( sizeof( uint ) + sizeof( uint ) + sizeof( uint ) )
// ////////////////////////////////////////////////////////////////////////
// Params:
// uint entity_id
// uint sub_type
// ////////////////////////////////////////////////////////////////////////

// ************************************************************************
// Automaps info
// ************************************************************************

#define NETMSG_AUTOMAPS_INFO                MAKE_NETMSG_HEADER( 170 )
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

class NetProperty
{
public:
    enum Type
    {
        None = 0,
        Global,              // 0
        Critter,             // 1 cr_id
        Chosen,              // 0
        MapItem,             // 1 item_id
        CritterItem,         // 2 cr_id item_id
        ChosenItem,          // 1 item_id
        Map,                 // 0
        Location,            // 0
		CustomEntity,        // 2 entity_id sub_type
    };
};

#define NETMSG_POD_PROPERTY( b, x )              MAKE_NETMSG_HEADER( 190 + ( b ) + ( x ) * 10 )
#define NETMSG_POD_PROPERTY_SIZE( b, x )         ( sizeof( uint ) + sizeof( char ) + sizeof( uint ) * ( x ) + sizeof( ushort ) + ( b ) )
// ////////////////////////////////////////////////////////////////////////
// Property changed
// NetProperty::Type type
// uint vars[x]
// ushort property_index
// uchar data[b]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_COMPLEX_PROPERTY             MAKE_NETMSG_HEADER( 189 )
// ////////////////////////////////////////////////////////////////////////
// Property changed
// uint msg_len
// NetProperty::Type type
// uint vars[x]
// ushort property_index
// uchar data[msg_len - ...]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_POD_PROPERTY( b, x )         MAKE_NETMSG_HEADER( 220 + ( b ) + ( x ) * 10 )
#define NETMSG_SEND_POD_PROPERTY_SIZE( b, x )    ( sizeof( uint ) + sizeof( char ) + sizeof( uint ) * ( x ) + sizeof( ushort ) + ( b ) )
// ////////////////////////////////////////////////////////////////////////
// Client change property
// NetProperty::Type type
// uint vars[x]
// ushort property_index
// uchar data[b]
// ////////////////////////////////////////////////////////////////////////

#define NETMSG_SEND_COMPLEX_PROPERTY        MAKE_NETMSG_HEADER( 219 )
// ////////////////////////////////////////////////////////////////////////
// Client change property
// uint msg_len
// NetProperty::Type type
// uint vars[x]
// ushort property_index
// uchar data[msg_len - ...]
// ////////////////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////////////////
// Properties serialization helpers
// ////////////////////////////////////////////////////////////////////////

#define NET_WRITE_PROPERTIES( bout, data_vec, data_sizes_vec )     \
    {                                                              \
        bout << (ushort) data_vec->size();                         \
        for( size_t i_ = 0, j_ = data_vec->size(); i_ < j_; i_++ ) \
        {                                                          \
            uint data_size_ = data_sizes_vec->at( i_ );            \
            bout << data_size_;                                    \
            if( data_size_ )                                       \
                bout.Push( data_vec->at( i_ ), data_size_ );       \
        }                                                          \
    }

#define NET_READ_PROPERTIES( bin, data_vec )                  \
    {                                                         \
        ushort data_count_;                                   \
        bin >> data_count_;                                   \
        data_vec.resize( data_count_ );                       \
        for( ushort i_ = 0; i_ < data_count_; i_++ )          \
        {                                                     \
            uint data_size_;                                  \
            Bin >> data_size_;                                \
            data_vec[ i_ ].resize( data_size_ );              \
            if( data_size_ )                                  \
                Bin.Pop( &data_vec[ i_ ][ 0 ], data_size_ );  \
        }                                                     \
    }

#endif // __NET_PROTOCOL__
