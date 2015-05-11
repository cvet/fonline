#ifndef ___DEFINES___
#define ___DEFINES___

// Bits
#define BIN__N( x )                           ( x ) | x >> 3 | x >> 6 | x >> 9
#define BIN__B( x )                           ( x ) & 0xf | ( x ) >> 12 & 0xf0
#define BIN8( v )                             ( BIN__B( BIN__N( 0x ## v ) ) )
#define BIN16( bin16, bin8 )                  ( ( BIN8( bin16 ) << 8 ) | ( BIN8( bin8 ) ) )
#define BIN32( bin32, bin24, bin16, bin8 )    ( ( BIN8( bin32 ) << 24 ) | ( BIN8( bin24 ) << 16 ) | ( BIN8( bin16 ) << 8 ) | ( BIN8( bin8 ) ) )

// Flags
#define FLAG( x, y )                          ( ( ( x ) & ( y ) ) != 0 )
#define FLAGS( x, y )                         ( ( ( x ) & ( y ) ) == y )
#define SETFLAG( x, y )                       ( ( x ) = ( x ) | ( y ) )
#define UNSETFLAG( x, y )                     ( ( x ) = ( ( x ) | ( y ) ) ^ ( y ) )

// Limits
#define MAX_UCHAR                    ( 0xFF )
#define MAX_USHORT                   ( 0xFFFF )
#define MAX_UINT                     ( 0xFFFFFFFF )
#define MAX_INT                      ( 0x7FFFFFFF )
#define MIN_INT                      ( 0x80000000 )

// Other stuff
#define CLAMP( x, low, high )                 ( ( ( x ) > ( high ) ) ? ( high ) : ( ( ( x ) < ( low ) ) ? ( low ) : ( x ) ) )
#define CONVERT_GRAMM( x )                    ( ( x ) * 453 )
#define RAD( deg )                            ( ( deg ) * 3.141592654f / 180.0f )

// World dump versions
#define WORLD_SAVE_V1                ( 0x01AB0F01 )
#define WORLD_SAVE_V2                ( 0x01AB0F02 )
#define WORLD_SAVE_V3                ( 0x01AB0F03 )
#define WORLD_SAVE_V4                ( 0x01AB0F04 )
#define WORLD_SAVE_V5                ( 0x01AB0F05 )
#define WORLD_SAVE_V6                ( 0x01AB0F06 )
#define WORLD_SAVE_V7                ( 0x01AB0F07 )
#define WORLD_SAVE_V8                ( 0x01AB0F08 )
#define WORLD_SAVE_V9                ( 0x01AB0F09 )
#define WORLD_SAVE_V10               ( 0x01AB0F10 )
#define WORLD_SAVE_V11               ( 0x01AB0F11 )
#define WORLD_SAVE_V12               ( 0x01AB0F12 )
#define WORLD_SAVE_V13               ( 0x01AB0F13 )
#define WORLD_SAVE_V14               ( 0x01AB0F14 )
#define WORLD_SAVE_V15               ( 0x01AB0F15 )
#define WORLD_SAVE_V16               ( 0x01AB0F16 )
#define WORLD_SAVE_V17               ( 0x01AB0F17 )
#define WORLD_SAVE_V18               ( 0x01AB0F18 )
#define WORLD_SAVE_V19               ( 0x01AB0F19 )
#define WORLD_SAVE_V20               ( 0x01AB0F20 )
#define WORLD_SAVE_V21               ( 0x01AB0F21 )
#define WORLD_SAVE_LAST              WORLD_SAVE_V21
#define SINGLEPLAYER_SAVE_V1         ( 1 )
#define SINGLEPLAYER_SAVE_V2         ( 2 )
#define SINGLEPLAYER_SAVE_LAST       ( SINGLEPLAYER_SAVE_V2 )

// Client save
#define CLIENT_SAVE_V1               ( 1 ) // Corrupted
#define CLIENT_SAVE_V2               ( 2 )
#define CLIENT_SAVE_LAST             ( CLIENT_SAVE_V2 )
const char ClientSaveSignature[ 4 ] = { 'F', 'O', 0, CLIENT_SAVE_LAST };

// Generic
#define WORLD_START_TIME             "07:00 30:10:2246 x00"
#define MAX_FOPATH                   UTF8_BUF_SIZE( 1024 )
#define CRAFT_SEND_TIME              ( 60000 )
#define MAX_HOLO_INFO                ( 250 )
#define AMBIENT_SOUND_TIME           ( 60000 ) // Random(X/2,X);
#define EFFECT_SCRIPT_VALUES         ( 10 )
#define ABC_SIZE                     ( 26 )
#define DIRS_COUNT                   ( GameOpt.MapHexagonal ? 6 : 8 )
#define IS_DIR_CORNER( dir )                  ( ( ( dir ) & 1 ) != 0 ) // 1, 3, 5, 7
#define UTF8_BUF_SIZE( count )                ( ( count ) * 4 )
#define DLGID_MASK                   ( 0xFFFFC000 )
#define DLG_STR_ID( dlg_id, idx )             ( ( ( dlg_id ) & DLGID_MASK ) | ( ( idx ) & ~DLGID_MASK ) )
#define LOCPID_MASK                  ( 0xFFFFF000 )
#define LOC_STR_ID( loc_pid, idx )            ( ( ( loc_pid ) & LOCPID_MASK ) | ( ( idx ) & ~LOCPID_MASK ) )
#define ITEMPID_MASK                 ( 0xFFFFFFF0 )
#define ITEM_STR_ID( item_pid, idx )          ( ( ( item_pid ) & ITEMPID_MASK ) | ( ( idx ) & ~ITEMPID_MASK ) )
#define CRPID_MASK                   ( 0xFFFFFFF0 )
#define CR_STR_ID( cr_pid, idx )              ( ( ( cr_pid ) & CRPID_MASK ) | ( ( idx ) & ~CRPID_MASK ) )

// Critters
#define GENDER_MALE                  ( 0 )
#define GENDER_FEMALE                ( 1 )
#define AGE_MAX                      ( 60 )
#define AGE_MIN                      ( 14 )
#define AGGRESSOR_TICK               ( 60000 )
#define MAX_ENEMY_STACK              ( 30 )
#define MAX_STORED_IP                ( 20 )

// SceneryCl flags
#define SCEN_CAN_USE                 ( 0x01 )
#define SCEN_CAN_TALK                ( 0x02 )

// Maps
#define TIME_CAN_FOLLOW_GM           ( 20000 ) // Can less than Map timeout

// Critter find types
#define FIND_LIFE                    ( 0x01 )
#define FIND_KO                      ( 0x02 )
#define FIND_DEAD                    ( 0x04 )
#define FIND_ONLY_PLAYERS            ( 0x10 )
#define FIND_ONLY_NPC                ( 0x20 )
#define FIND_ALL                     ( 0x0F )

// Proto maps
#define MAP_PROTO_EXT                ".fomap"

// Type entires
#define ENTIRE_DEFAULT               ( 0 )
#define ENTIRE_LOG_OUT               ( 241 )

// Sendmap info
#define SENDMAP_TILES                BIN8( 00000001 )
#define SENDMAP_WALLS                BIN8( 00000010 )
#define SENDMAP_SCENERY              BIN8( 00000100 )

// Ping
#define PING_PING                    ( 0 )
#define PING_WAIT                    ( 1 )
#define PING_CLIENT                  ( 2 )
#define PING_UID_FAIL                ( 3 )

// Say types
#define SAY_NORM                     ( 1 )
#define SAY_NORM_ON_HEAD             ( 2 )
#define SAY_SHOUT                    ( 3 )
#define SAY_SHOUT_ON_HEAD            ( 4 )
#define SAY_EMOTE                    ( 5 )
#define SAY_EMOTE_ON_HEAD            ( 6 )
#define SAY_WHISP                    ( 7 )
#define SAY_WHISP_ON_HEAD            ( 8 )
#define SAY_SOCIAL                   ( 9 )
#define SAY_RADIO                    ( 10 )
#define SAY_NETMSG                   ( 11 )
#define SAY_DIALOG                   ( 12 )
#define SAY_APPEND                   ( 13 )
#define SAY_ENCOUNTER_ANY            ( 14 )
#define SAY_ENCOUNTER_RT             ( 15 )
#define SAY_ENCOUNTER_TB             ( 16 )
#define SAY_FIX_RESULT               ( 17 )
#define SAY_DIALOGBOX_TEXT           ( 18 )
#define SAY_DIALOGBOX_BUTTON( b )             ( 19 + ( b ) ) // Max 20 buttons (0..19)
#define SAY_SAY_TITLE                ( 39 )
#define SAY_SAY_TEXT                 ( 40 )
#define SAY_FLASH_WINDOW             ( 41 )

#define MAX_DLGBOX_BUTTONS           ( 20 )

// Transfer types
#define TRANSFER_CLOSE               ( 0 )
#define TRANSFER_HEX_CONT_UP         ( 1 )
#define TRANSFER_HEX_CONT_DOWN       ( 2 )
#define TRANSFER_SELF_CONT           ( 3 )
#define TRANSFER_CRIT_LOOT           ( 4 )
#define TRANSFER_CRIT_STEAL          ( 5 )
#define TRANSFER_CRIT_BARTER         ( 6 )
#define TRANSFER_FAR_CONT            ( 7 )
#define TRANSFER_FAR_CRIT            ( 8 )

// Take flags
#define CONT_GET                     ( 1 )
#define CONT_PUT                     ( 2 )
#define CONT_GETALL                  ( 3 )
#define CONT_PUTALL                  ( 4 )

// Target types
#define TARGET_SELF                  ( 0 )
#define TARGET_SELF_ITEM             ( 1 )
#define TARGET_CRITTER               ( 2 )
#define TARGET_ITEM                  ( 3 )
#define TARGET_SCENERY               ( 4 )
#define TARGET_HEX                   ( 5 ) // Todo:

// Pick types
#define PICK_CRIT_LOOT               ( 0 )
#define PICK_CRIT_PUSH               ( 1 )

// Craft results
#define CRAFT_RESULT_NONE            ( 0 )
#define CRAFT_RESULT_SUCC            ( 1 )
#define CRAFT_RESULT_FAIL            ( 2 )
#define CRAFT_RESULT_TIMEOUT         ( 3 )

// Critters
#define CRITTER_INV_VOLUME           ( 1000 )

// Locker
#define LOCKER_ISOPEN                ( 0x01 )
#define LOCKER_NOOPEN                ( 0x10 )

// Hit locations
#define HIT_LOCATION_NONE            ( 0 )
#define HIT_LOCATION_HEAD            ( 1 )
#define HIT_LOCATION_LEFT_ARM        ( 2 )
#define HIT_LOCATION_RIGHT_ARM       ( 3 )
#define HIT_LOCATION_TORSO           ( 4 )
#define HIT_LOCATION_RIGHT_LEG       ( 5 )
#define HIT_LOCATION_LEFT_LEG        ( 6 )
#define HIT_LOCATION_EYES            ( 7 )
#define HIT_LOCATION_GROIN           ( 8 )
#define HIT_LOCATION_UNCALLED        ( 9 )
#define MAX_HIT_LOCATION             ( 10 )

// Global map
#define GM_MAXX                      ( GameOpt.GlobalMapWidth * GameOpt.GlobalMapZoneLength )
#define GM_MAXY                      ( GameOpt.GlobalMapHeight * GameOpt.GlobalMapZoneLength )
#define GM_ZONE_LEN                  ( GameOpt.GlobalMapZoneLength ) // Can be multiple to GM_MAXX and GM_MAXY
#define GM__MAXZONEX                 ( 100 )
#define GM__MAXZONEY                 ( 100 )
#define GM_ZONES_FOG_SIZE            ( ( ( GM__MAXZONEX / 4 ) + ( ( GM__MAXZONEX % 4 ) ? 1 : 0 ) ) * GM__MAXZONEY )
#define GM_FOG_FULL                  ( 0 )
#define GM_FOG_HALF                  ( 1 )
#define GM_FOG_HALF_EX               ( 2 )
#define GM_FOG_NONE                  ( 3 )
#define GM_MAX_GROUP_COUNT           ( GameOpt.GlobalMapMaxGroupCount )
#define GM_ANSWER_WAIT_TIME          ( 20000 )
#define GM_LIGHT_TIME                ( 5000 )
#define GM_ZONE( x )                          ( ( x ) / GM_ZONE_LEN )
#define GM_ENTRANCES_SEND_TIME       ( 60000 )
#define GM_TRACE_TIME                ( 1000 )

// Follow
#define FOLLOW_DIST                  ( 10 )
#define FOLLOW_FORCE                 ( 1 )
#define FOLLOW_PREP                  ( 2 )

// GM Info
#define GM_INFO_LOCATIONS            ( 0x01 )
#define GM_INFO_CRITTERS             ( 0x02 )
#define GM_INFO_GROUP_PARAM          ( 0x04 )
#define GM_INFO_ZONES_FOG            ( 0x08 )
#define GM_INFO_ALL                  ( 0x0F )
#define GM_INFO_FOG                  ( 0x10 )
#define GM_INFO_LOCATION             ( 0x20 )

// Global process types
#define GLOBAL_PROCESS_MOVE          ( 0 )
#define GLOBAL_PROCESS_ENTER         ( 1 )
#define GLOBAL_PROCESS_START_FAST    ( 2 )
#define GLOBAL_PROCESS_START         ( 3 )
#define GLOBAL_PROCESS_SET_MOVE      ( 4 )
#define GLOBAL_PROCESS_STOPPED       ( 5 )
#define GLOBAL_PROCESS_NPC_IDLE      ( 6 )
#define GLOBAL_PROCESS_KICK          ( 7 )

// GM Rule command
#define GM_CMD_SETMOVE               ( 1 ) // +r-a*x,y
#define GM_CMD_STOP                  ( 2 ) // +r-a
#define GM_CMD_TOLOCAL               ( 3 ) // +r-a*num_city,num_map
#define GM_CMD_KICKCRIT              ( 4 ) // +r-a*cr_id
#define GM_CMD_FOLLOW_CRIT           ( 5 ) // +r+a*cr_id
#define GM_CMD_FOLLOW                ( 6 )
#define GM_CMD_GIVE_RULE             ( 7 )
#define GM_CMD_ANSWER                ( 8 )
#define GM_CMD_ENTRANCES             ( 9 )
#define GM_CMD_VIEW_MAP              ( 10 )

// GM Walk types
#define GM_WALK_GROUND               ( 0 )
#define GM_WALK_FLY                  ( 1 )
#define GM_WALK_WATER                ( 2 )

// Flags Hex
// Proto map
#define FH_BLOCK                     BIN8( 00000001 )
#define FH_NOTRAKE                   BIN8( 00000010 )
#define FH_WALL                      BIN8( 00000100 )
#define FH_SCEN                      BIN8( 00001000 )
#define FH_SCEN_GRID                 BIN8( 00010000 )
#define FH_TRIGGER                   BIN8( 00100000 )
// Map copy
#define FH_CRITTER                   BIN8( 00000001 )
#define FH_DEAD_CRITTER              BIN8( 00000010 )
#define FH_ITEM                      BIN8( 00000100 )
#define FH_DOOR                      BIN8( 00001000 )
#define FH_BLOCK_ITEM                BIN8( 00010000 )
#define FH_NRAKE_ITEM                BIN8( 00100000 )
#define FH_WALK_ITEM                 BIN8( 01000000 )
#define FH_GAG_ITEM                  BIN8( 10000000 )

#define FH_NOWAY                     BIN16( 00010001, 00000001 )
#define FH_NOSHOOT                   BIN16( 00100000, 00000010 )

// Client map
#define CLIENT_MAP_FORMAT_VER        ( 8 )

// Coordinates
#define MAXHEX_DEF                   ( 200 )
#define MAXHEX_MIN                   ( 10 )
#define MAXHEX_MAX                   ( 4000 )

// Client parameters
#define MAX_NAME                     ( 30 )
#define MIN_NAME                     ( 1 )
#define MAX_CHAT_MESSAGE             ( 100 )
#define MAX_SAY_NPC_TEXT             ( 25 )
#define MAX_SCENERY                  ( 5000 )
#define MAX_DIALOG_TEXT              ( MAX_FOTEXT )
#define MAX_DLG_LEN_IN_BYTES         ( 64 * 1024 )
#define MAX_DLG_LEXEMS_TEXT          ( 1000 )
#define MAX_BUF_LEN                  ( 4096 )
#define PASS_HASH_SIZE               ( 32 )
#define FILE_UPDATE_PORTION          ( 0x10000 )

// Critters
#define MAX_CRIT_TYPES               ( 1000 )
#define NPC_START_ID                 ( 5000001 )
#define USERS_START_ID               ( 1 )
#define IS_USER_ID( id )                      ( ( id ) > 0 && ( id ) < NPC_START_ID )
#define IS_NPC_ID( id )                       ( ( id ) >= NPC_START_ID )
#define MAX_ANSWERS                  ( 100 )
#define PROCESS_TALK_TICK            ( 1000 )
#define MAX_SCRIPT_NAME              ( 64 )
#define TURN_BASED_TIMEOUT           ( 1000 )
#define FADING_PERIOD                ( 1000 )

#define RESPOWN_TIME_PLAYER          ( 3 )
#define RESPOWN_TIME_NPC             ( 120 )
#define RESPOWN_TIME_INFINITY        ( 4 * 24 * 60 * 60000 )

// Combat modes
#define COMBAT_MODE_ANY              ( 0 )
#define COMBAT_MODE_REAL_TIME        ( 1 )
#define COMBAT_MODE_TURN_BASED       ( 2 )

// Turn based
#define COMBAT_TB_END_TURN           ( 0 )
#define COMBAT_TB_END_COMBAT         ( 1 )

// Answer
#define ANSWER_BEGIN                 ( 0xF0 )
#define ANSWER_END                   ( 0xF1 )
#define ANSWER_BARTER                ( 0xF2 )

// Time AP
#define AP_DIVIDER                   ( 100 )

// Crit conditions
#define COND_LIFE                    ( 1 )
#define COND_KNOCKOUT                ( 2 )
#define COND_DEAD                    ( 3 )

// Run-time critters flags
#define FCRIT_PLAYER                 ( 0x00010000 ) // Player
#define FCRIT_NPC                    ( 0x00020000 ) // Npc
#define FCRIT_DISCONNECT             ( 0x00080000 ) // In offline
#define FCRIT_CHOSEN                 ( 0x00100000 ) // Chosen
#define FCRIT_RULEGROUP              ( 0x00200000 ) // Group rule

// Slots
#define SLOT_INV                     ( 0 )
#define SLOT_HAND1                   ( 1 )
#define SLOT_HAND2                   ( 2 )
#define SLOT_ARMOR                   ( 3 )
#define SLOT_GROUND                  ( 255 )

// Players barter
#define BARTER_DIST                  ( 1 )
// Types
#define BARTER_TRY                   ( 0 ) // opponentId, isHide
#define BARTER_ACCEPTED              ( 1 ) // opponentId, isHide
#define BARTER_BEGIN                 ( 2 )
#define BARTER_END                   ( 3 )
#define BARTER_SET_SELF              ( 4 )
#define BARTER_SET_OPPONENT          ( 5 )
#define BARTER_UNSET_SELF            ( 6 )
#define BARTER_UNSET_OPPONENT        ( 7 )
#define BARTER_OFFER                 ( 8 ) // isSet, isOpponent
#define BARTER_REFRESH               ( 9 )

// Scores
#define SCORE_SPEAKER                ( 3 )
#define SCORE_TRADER                 ( 4 )
#define SCORES_MAX                   ( 50 )
#define SCORES_SEND_TIME             ( 60000 )
#define SCORE_NAME_LEN               ( 64 )

// Show screen modes
// Ouput: it is 'uint param' in Critter::ShowScreen.
// Input: I - integer value 'uint answerI', S - string value 'string& answerS' in 'answer_' function.
#define SHOW_SCREEN_CLOSE            ( 0 )    // Close top window.
#define SHOW_SCREEN_TIMER            ( 1 )    // Timer box. Output: picture index in INVEN.LST. Input I: time in game minutes (1..599).
#define SHOW_SCREEN_DIALOGBOX        ( 2 )    // Dialog box. Output: buttons count - 0..20 (exit button added automatically). Input I: Choosed button - 0..19.
#define SHOW_SCREEN_SKILLBOX         ( 3 )    // Skill box. Input I: selected skill.
#define SHOW_SCREEN_BAG              ( 4 )    // Bag box. Input I: id of selected item.
#define SHOW_SCREEN_SAY              ( 5 )    // Say box. Output: all symbols - 0 or only numbers - any other number. Input S: typed string.
#define SHOW_ELEVATOR                ( 6 )    // Elevator. Output: look ELEVATOR_* macro. Input I: Choosed level button.
#define SHOW_SCREEN_INVENTORY        ( 7 )    // Inventory.
#define SHOW_SCREEN_CHARACTER        ( 8 )    // Character.
#define SHOW_SCREEN_FIXBOY           ( 9 )    // Fix-boy.
#define SHOW_SCREEN_PIPBOY           ( 10 )   // Pip-boy.
#define SHOW_SCREEN_MINIMAP          ( 11 )   // Mini-map.

// Timeouts
#define IS_TIMEOUT( to )                      ( ( to ) > GameOpt.FullSecond )
#define TB_BATTLE_TIMEOUT            ( GameOpt.FullSecond + 100000000 )
#define TB_BATTLE_TIMEOUT_CHECK( to )         ( ( to ) > GameOpt.FullSecond && ( to ) - GameOpt.FullSecond > 10000000 )

// Special send params
#define OTHER_BREAK_TIME             ( 0 )
#define OTHER_WAIT_TIME              ( 1 )
#define OTHER_FLAGS                  ( 2 )
#define OTHER_BASE_TYPE              ( 3 )
#define OTHER_MULTIHEX               ( 4 )
#define OTHER_YOU_TURN               ( 5 )
#define OTHER_CLEAR_MAP              ( 6 )
#define OTHER_TELEPORT               ( 7 )

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//                                          flags    actionExt                                                      item
#define ACTION_MOVE                  ( 0 )  // l
#define ACTION_RUN                   ( 1 )  // l
#define ACTION_MOVE_ITEM             ( 2 )  // l s      from slot                                                      +
#define ACTION_MOVE_ITEM_SWAP        ( 3 )  // l s      from slot                                                      +
#define ACTION_USE_ITEM              ( 4 )  // l s                                                                     +
#define ACTION_DROP_ITEM             ( 5 )  // l s      from slot                                                      +
#define ACTION_USE_WEAPON            ( 6 )  // l        fail attack 8 bit, use index (0-2) 4-7 bits, aim 0-3 bits      +
#define ACTION_RELOAD_WEAPON         ( 7 )  // l s                                                                     +
#define ACTION_USE_SKILL             ( 8 )  // l s      skill index (see SK_*)
#define ACTION_PICK_ITEM             ( 9 )  // l s                                                                     +
#define ACTION_PICK_CRITTER          ( 10 ) // l        0 - loot, 1 - steal, 2 - push
#define ACTION_OPERATE_CONTAINER     ( 11 ) // l s      transfer type * 10 + [0 - get, 1 - get all, 2 - put]           + (exclude get all)
#define ACTION_BARTER                ( 12 ) //   s      0 - item taken, 1 - item given                                 +
#define ACTION_DODGE                 ( 13 ) //          0 - front, 1 - back
#define ACTION_DAMAGE                ( 14 ) //          0 - front, 1 - back
#define ACTION_DAMAGE_FORCE          ( 15 ) //          0 - front, 1 - back
#define ACTION_KNOCKOUT              ( 16 ) //   s      0 - knockout anim2begin
#define ACTION_STANDUP               ( 17 ) //   s      0 - knockout anim2end
#define ACTION_FIDGET                ( 18 ) // l
#define ACTION_DEAD                  ( 19 ) //   s      dead type anim2 (see Anim2 in _animation.fos)
#define ACTION_CONNECT               ( 20 ) //
#define ACTION_DISCONNECT            ( 21 ) //
#define ACTION_RESPAWN               ( 22 ) //   s
#define ACTION_REFRESH               ( 23 ) //   s

// Script defines
// Look checks
#define LOOK_CHECK_DIR               ( 0x01 )
#define LOOK_CHECK_SNEAK_DIR         ( 0x02 )
#define LOOK_CHECK_SNEAK_WEIGHT      ( 0x04 )
#define LOOK_CHECK_TRACE             ( 0x08 )
#define LOOK_CHECK_SCRIPT            ( 0x10 )
#define LOOK_CHECK_ITEM_SCRIPT       ( 0x20 )

// In SendMessage
#define MESSAGE_TO_VISIBLE_ME        ( 0 )
#define MESSAGE_TO_IAM_VISIBLE       ( 1 )
#define MESSAGE_TO_ALL_ON_MAP        ( 2 )

// Special skill values
#define SKILL_PICK_ON_GROUND         ( -1 )
#define SKILL_PUT_CONT               ( -2 )
#define SKILL_TAKE_CONT              ( -3 )
#define SKILL_TAKE_ALL_CONT          ( -4 )
#define SKILL_LOOT_CRITTER           ( -5 )
#define SKILL_PUSH_CRITTER           ( -6 )
#define SKILL_TALK                   ( -7 )

// Anim1
#define ANIM1_UNARMED                ( 1 )
// Anim2
#define ANIM2_IDLE                   ( 1 )
#define ANIM2_WALK                   ( 3 )
#define ANIM2_LIMP                   ( 4 )
#define ANIM2_RUN                    ( 5 )
#define ANIM2_PANIC_RUN              ( 6 )
#define ANIM2_SNEAK_WALK             ( 7 )
#define ANIM2_SNEAK_RUN              ( 8 )
#define ANIM2_IDLE_PRONE_FRONT       ( 86 )
#define ANIM2_IDLE_PRONE_BACK        ( 87 )
#define ANIM2_DEAD_FRONT             ( 102 )
#define ANIM2_DEAD_BACK              ( 103 )

// Move params
// 6 next steps (each 5 bit) + stop bit + run bit
// Step bits: 012 - dir, 3 - allow, 4 - disallow
#define MOVE_PARAM_STEP_COUNT        ( 6 )
#define MOVE_PARAM_STEP_BITS         ( 5 )
#define MOVE_PARAM_STEP_DIR          ( 0x7 )
#define MOVE_PARAM_STEP_ALLOW        ( 0x8 )
#define MOVE_PARAM_STEP_DISALLOW     ( 0x10 )
#define MOVE_PARAM_RUN               ( 0x80000000 )

// Holodisks
#define USER_HOLO_TEXTMSG_FILE       "FOHOLOEXT.MSG"
#define USER_HOLO_START_NUM          ( 100000 )
#define USER_HOLO_MAX_TITLE_LEN      ( 40 )
#define USER_HOLO_MAX_LEN            ( 2000 )

// Cache names
#define CACHE_MAGIC_CHAR             "*"
#define CACHE_HASH_APPENDIX          "_hash"
#define CACHE_MSG_PREFIX             CACHE_MAGIC_CHAR "msg"
#define CACHE_ITEM_PROTOS            CACHE_MAGIC_CHAR "_item_protos"

#endif // ___DEFINES___
