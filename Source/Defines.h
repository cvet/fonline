#ifndef __DEFINES__
#define __DEFINES__

#include "PlatformSpecific.h"

typedef map<string,uchar> StrUCharMap;
typedef map<string,uchar>::iterator StrUCharMapIt;
typedef map<string,uchar>::value_type StrUCharMapVal;
typedef map<uchar,string> UCharStrMap;
typedef map<uchar,string>::iterator UCharStrMapIt;
typedef map<uchar,string>::value_type UCharStrMapVal;
typedef map<string,string> StrMap;
typedef map<string,string>::iterator StrMapIt;
typedef map<string,string>::value_type StrMapVal;
typedef map<uint,string> UIntStrMap;
typedef map<uint,string>::iterator UIntStrMapIt;
typedef map<uint,string>::value_type UIntStrMapVal;
typedef map<string,ushort> StrUShortMap;
typedef map<string,ushort>::iterator StrUShortMapIt;
typedef map<string,ushort>::value_type StrUShortMapVal;
typedef map<string,uint> StrUIntMap;
typedef map<string,uint>::iterator StrUIntMapIt;
typedef map<string,uint>::value_type StrUIntMapVal;
typedef map<ushort,string> UShortStrMap;
typedef map<ushort,string>::iterator UShortStrMapIt;
typedef map<ushort,string>::value_type UShortStrMapVal;
typedef map<string,uint> StrUIntMap;
typedef map<string,uint>::iterator StrUIntMapIt;
typedef map<string,uint>::value_type StrUIntMapVal;
typedef map<uint,uint> UIntMap;
typedef map<uint,uint>::iterator UIntMapIt;
typedef map<uint,uint>::value_type UIntMapVal;
typedef map<int,int> IntMap;
typedef map<int,int>::iterator IntMapIt;
typedef map<int,int>::value_type IntMapVal;
typedef map<int,float> IntFloatMap;
typedef map<int,float>::iterator IntFloatMapIt;
typedef map<int,float>::value_type IntFloatMapVal;
typedef map<int,void*> IntPtrMap;
typedef map<int,void*>::iterator IntPtrMapIt;
typedef map<int,void*>::value_type IntPtrMapVal;

typedef multimap<uint,string> UIntStrMulMap;
typedef multimap<uint,string>::iterator UIntStrMulMapIt;
typedef multimap<uint,string>::value_type UIntStrMulMapVal;

typedef vector<void*> PtrVec;
typedef vector<void*>::iterator PtrVecIt;
typedef vector<int> IntVec;
typedef vector<int>::iterator IntVecIt;
typedef vector<uchar> UCharVec;
typedef vector<uchar>::iterator UCharVecIt;
typedef vector<short> ShortVec;
typedef vector<ushort> UShortVec;
typedef vector<ushort>::iterator UShortVecIt;
typedef vector<uint> UIntVec;
typedef vector<uint>::iterator UIntVecIt;
typedef vector<char> CharVec;
typedef vector<string> StrVec;
typedef vector<string>::iterator StrVecIt;
typedef vector<char*> PCharVec;
typedef vector<uchar*> PUCharVec;
typedef vector<float> FloatVec;
typedef vector<float>::iterator FloatVecIt;
typedef vector<uint64> UInt64Vec;
typedef vector<uint64>::iterator UInt64VecIt;
typedef vector<bool>BoolVec;
typedef vector<bool>::iterator BoolVecIt;

typedef set<string> StrSet;
typedef set<uchar> UCharSet;
typedef set<uchar>::iterator UCharSetIt;
typedef set<ushort> UShortSet;
typedef set<ushort>::iterator UShortSetIt;
typedef set<uint> UIntSet;
typedef set<uint>::iterator UIntSetIt;
typedef set<int> IntSet;
typedef set<int>::iterator IntSetIt;

typedef pair<int,int> IntPair;
typedef pair<ushort,ushort> UShortPair;
typedef pair<uint,uint> UIntPair;
typedef pair<char,char> CharPair;
typedef pair<char*,char*> PCharPair;
typedef pair<uchar,uchar> UCharPair;

typedef vector<UShortPair> UShortPairVec;
typedef vector<UShortPair>::iterator UShortPairVecIt;
typedef vector<UShortPair>::value_type UShortPairVecVal;
typedef vector<IntPair> IntPairVec;
typedef vector<IntPair>::iterator IntPairVecIt;
typedef vector<IntPair>::value_type IntPairVecVal;
typedef vector<UIntPair> UIntPairVec;
typedef vector<UIntPair>::iterator UIntPairVecIt;
typedef vector<UIntPair>::value_type UIntPairVecVal;
typedef vector<PCharPair> PCharPairVec;
typedef vector<PCharPair>::iterator PCharPairVecIt;
typedef vector<PCharPair>::value_type PCharPairVecVal;
typedef vector<UCharPair> UCharPairVec;
typedef vector<UCharPair>::iterator UCharPairVecIt;
typedef vector<UCharPair>::value_type UCharPairVecVal;

// Bits
#define BIN__N(x)                     (x) | x>>3 | x>>6 | x>>9
#define BIN__B(x)                     (x) & 0xf | (x)>>12 & 0xf0
#define BIN8(v)                       (BIN__B(BIN__N(0x##v)))
#define BIN16(bin16,bin8)             ((BIN8(bin16)<<8)|(BIN8(bin8)))
#define BIN32(bin32,bin24,bin16,bin8) ((BIN8(bin32)<<24)|(BIN8(bin24)<<16)|(BIN8(bin16)<<8)|(BIN8(bin8)))

#define FLAG(x,y)         (((x)&(y))!=0)
#define FLAGS(x,y)        (((x)&(y))==y)
#define SETFLAG(x,y)      ((x)=(x)|(y))
#define UNSETFLAG(x,y)    ((x)=((x)|(y))^(y))

// Limits
#define MAX_UCHAR        (0xFF)
#define MAX_USHORT       (0xFFFF)
#define MAX_UINT         (0xFFFFFFFF)
#define MAX_INT          (0x7FFFFFFF)
#define MIN_INT          (0x80000000)

// Other stuff
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#define CONVERT_GRAMM(x)  ((x)*453)
#define RAD(deg)          ((deg)*3.141592654f/180.0f)

// World dump versions
#define WORLD_SAVE_V1           (0x01AB0F01)
#define WORLD_SAVE_V2           (0x01AB0F02)
#define WORLD_SAVE_V3           (0x01AB0F03)
#define WORLD_SAVE_V4           (0x01AB0F04)
#define WORLD_SAVE_V5           (0x01AB0F05)
#define WORLD_SAVE_V6           (0x01AB0F06)
#define WORLD_SAVE_V7           (0x01AB0F07)
#define WORLD_SAVE_V8           (0x01AB0F08)
#define WORLD_SAVE_V9           (0x01AB0F09)
#define WORLD_SAVE_V10          (0x01AB0F10)
#define WORLD_SAVE_V11          (0x01AB0F11)
#define WORLD_SAVE_V12          (0x01AB0F12)
#define WORLD_SAVE_LAST         WORLD_SAVE_V12

// Generic
#define WORLD_START_TIME        "07:00 30:10:2246 x00"
#define MAX_FOPATH              (1024)
#define CRAFT_SEND_TIME         (60000)
#define LEXEMS_SIZE             (128)
#define MAX_HOLO_INFO           (250)
#define MAX_PARAMETERS_ARRAYS   (100)
#define AMBIENT_SOUND_TIME      (60000) // Random(X/2,X);
#define EFFECT_TEXTURES         (10)
#define EFFECT_SCRIPT_VALUES    (10)
#define ABC_SIZE                (26)
#define DIRS_COUNT              (GameOpt.MapHexagonal?6:8)
#define IS_DIR_CORNER(dir)      (((dir)&1)!=0) // 1, 3, 5, 7

// Critters
#define GENDER_MALE             (0)
#define GENDER_FEMALE           (1)
#define GENDER_IT               (2)
#define AGE_MAX                 (60)
#define AGE_MIN                 (14)
#define AGGRESSOR_TICK          (60000)
#define MAX_ENEMY_STACK         (30)
#define MAX_STORED_IP           (20)

// Items
#define MAX_ADDED_NOGROUP_ITEMS (30)
#define ITEM_SLOT_BEGIN         (1000)
#define ITEM_SLOT_END           (1099)
#define ITEM_DEF_SLOT           (1000)
#define ITEM_DEF_ARMOR          (1100)
#define UNARMED_PUNCH           (1000)
#define UNARMED_KICK            (1020)

// SceneryCl flags
#define SCEN_CAN_USE            (0x01)
#define SCEN_CAN_TALK           (0x02)

// Maps
#define TIME_CAN_FOLLOW_GM      (20000) // Can less than Map timeout

// Critter find types
#define FIND_LIFE               (0x01)
#define FIND_KO                 (0x02)
#define FIND_DEAD               (0x04)
#define FIND_ONLY_PLAYERS       (0x10)
#define FIND_ONLY_NPC           (0x20)
#define FIND_ALL                (0x0F)

// Proto maps
#define MAP_PROTO_EXT			".fomap"
#define MAX_PROTO_MAPS          (30000)

// Type entires
#define ENTIRE_DEFAULT			(0)
#define ENTIRE_LOG_OUT			(241)

// Sendmap info
#define SENDMAP_TILES           BIN8(00000001)
#define SENDMAP_WALLS           BIN8(00000010)
#define SENDMAP_SCENERY         BIN8(00000100)

// Ping
#define PING_PING               (0)
#define PING_WAIT               (1)
#define PING_CLIENT             (2)
#define PING_UID_FAIL           (3)

// Say types
#define SAY_NORM                (1)
#define SAY_NORM_ON_HEAD        (2)
#define SAY_SHOUT               (3)
#define SAY_SHOUT_ON_HEAD       (4)
#define SAY_EMOTE               (5)
#define SAY_EMOTE_ON_HEAD       (6)
#define SAY_WHISP               (7)
#define SAY_WHISP_ON_HEAD       (8)
#define SAY_SOCIAL              (9)
#define SAY_RADIO               (10)
#define SAY_NETMSG              (11)
#define SAY_DIALOG              (12)
#define SAY_APPEND              (13)
#define SAY_ENCOUNTER_ANY       (14)
#define SAY_ENCOUNTER_RT        (15)
#define SAY_ENCOUNTER_TB        (16)
#define SAY_FIX_RESULT          (17)
#define SAY_DIALOGBOX_TEXT      (18)
#define SAY_DIALOGBOX_BUTTON(b) (19+(b)) // Max 20 buttons (0..19)
#define SAY_SAY_TITLE           (39)
#define SAY_SAY_TEXT            (40)
#define SAY_FLASH_WINDOW        (41)

#define MAX_DLGBOX_BUTTONS       (20)

// Transfer types
#define TRANSFER_CLOSE          (0)
#define TRANSFER_HEX_CONT_UP    (1)
#define TRANSFER_HEX_CONT_DOWN  (2)
#define TRANSFER_SELF_CONT      (3)
#define TRANSFER_CRIT_LOOT      (4)
#define TRANSFER_CRIT_STEAL     (5)
#define TRANSFER_CRIT_BARTER    (6)
#define TRANSFER_FAR_CONT       (7)
#define TRANSFER_FAR_CRIT       (8)

// Take flags
#define CONT_GET                (1)
#define CONT_PUT                (2)
#define CONT_GETALL             (3)
#define CONT_PUTALL             (4)
//#define CONT_UNLOAD             (5) // TODO:

// Target types
#define TARGET_SELF             (0)
#define TARGET_SELF_ITEM        (1)
#define TARGET_CRITTER          (2)
#define TARGET_ITEM             (3)
#define TARGET_SCENERY          (4)

// Pick types
#define PICK_CRIT_LOOT          (0)
#define PICK_CRIT_PUSH          (1)

// Craft results
#define CRAFT_RESULT_NONE       (0)
#define CRAFT_RESULT_SUCC       (1)
#define CRAFT_RESULT_FAIL       (2)
#define CRAFT_RESULT_TIMEOUT    (3)

// Critters
#define CRITTER_INV_VOLUME      (1000)

// Locker
#define LOCKER_ISOPEN           (0x01)
#define LOCKER_NOOPEN           (0x10)

// Hit locations
#define HIT_LOCATION_NONE       (0)
#define HIT_LOCATION_HEAD       (1)
#define HIT_LOCATION_LEFT_ARM   (2)
#define HIT_LOCATION_RIGHT_ARM  (3)
#define HIT_LOCATION_TORSO      (4)
#define HIT_LOCATION_RIGHT_LEG  (5)
#define HIT_LOCATION_LEFT_LEG   (6)
#define HIT_LOCATION_EYES       (7)
#define HIT_LOCATION_GROIN      (8)
#define HIT_LOCATION_UNCALLED   (9)
#define MAX_HIT_LOCATION        (10)

// Locations
#define MAX_PROTO_LOCATIONS     (30000)

// Global map
#define GM_MAXX                 (GameOpt.GlobalMapWidth*GameOpt.GlobalMapZoneLength)
#define GM_MAXY                 (GameOpt.GlobalMapHeight*GameOpt.GlobalMapZoneLength)
#define GM_ZONE_LEN             (GameOpt.GlobalMapZoneLength) // Can be multiple to GM_MAXX and GM_MAXY
#define GM__MAXZONEX            (100)
#define GM__MAXZONEY            (100)
#define GM_ZONES_FOG_SIZE       (((GM__MAXZONEX/4)+((GM__MAXZONEX%4)?1:0))*GM__MAXZONEY)
#define GM_FOG_FULL             (0)
#define GM_FOG_HALF             (1)
#define GM_FOG_HALF_EX          (2)
#define GM_FOG_NONE             (3)
#define GM_MAX_GROUP_COUNT      (GameOpt.GlobalMapMaxGroupCount)
#define GM_PROCESS_TIME         (500)
#define GM_MOVE_TIME            (50)
#define GM_ANSWER_WAIT_TIME     (20000)
#define GM_LIGHT_TIME           (5000)
#define GM_ZONE(x)              ((x)/GM_ZONE_LEN)
#define GM_ENTRANCES_SEND_TIME  (60000)
#define GM_TRACE_TIME           (1000)

// Follow
#define FOLLOW_DIST				(10)
#define FOLLOW_FORCE            (1)
#define FOLLOW_PREP             (2)

// GM Info
#define GM_INFO_LOCATIONS       (0x01)
#define GM_INFO_CRITTERS        (0x02)
#define GM_INFO_GROUP_PARAM     (0x04)
#define GM_INFO_ZONES_FOG       (0x08)
#define GM_INFO_ALL             (0x0F)
#define GM_INFO_FOG             (0x10)
#define GM_INFO_LOCATION        (0x20)

// Global process types
#define GLOBAL_PROCESS_MOVE         (0)
#define GLOBAL_PROCESS_ENTER        (1)
#define GLOBAL_PROCESS_START_FAST   (2)
#define GLOBAL_PROCESS_START        (3)
#define GLOBAL_PROCESS_SET_MOVE     (4)
#define GLOBAL_PROCESS_STOPPED      (5)
#define GLOBAL_PROCESS_NPC_IDLE     (6)
#define GLOBAL_PROCESS_NEW_ZONE     (7)

// GM Rule command
#define GM_CMD_SETMOVE     (1) // +r-a*x,y
#define GM_CMD_STOP        (2) // +r-a
#define GM_CMD_TOLOCAL     (3) // +r-a*num_city,num_map
#define GM_CMD_KICKCRIT    (4) // +r-a*cr_id
#define GM_CMD_FOLLOW_CRIT (5) // +r+a*cr_id
#define GM_CMD_FOLLOW      (6)
#define GM_CMD_GIVE_RULE   (7)
#define GM_CMD_ANSWER      (8)
#define GM_CMD_ENTRANCES   (9)
#define GM_CMD_VIEW_MAP    (10)

// GM Walk types
#define GM_WALK_GROUND          (0)
#define GM_WALK_FLY             (1)
#define GM_WALK_WATER           (2)

// Flags Hex
	// Proto map
#define FH_BLOCK                       BIN8(00000001)
#define FH_NOTRAKE                     BIN8(00000010)
#define FH_WALL                        BIN8(00000100)
#define FH_SCEN                        BIN8(00001000)
#define FH_SCEN_GRID                   BIN8(00010000)
#define FH_TRIGGER                     BIN8(00100000)
	// Map copy
#define FH_CRITTER            BIN8(00000001)
#define FH_DEAD_CRITTER       BIN8(00000010)
#define FH_ITEM               BIN8(00000100)
#define FH_DOOR               BIN8(00001000)
#define FH_BLOCK_ITEM         BIN8(00010000)
#define FH_NRAKE_ITEM         BIN8(00100000)
#define FH_WALK_ITEM          BIN8(01000000)
#define FH_GAG_ITEM           BIN8(10000000)

#define FH_NOWAY             BIN16(00010001,00000001)
#define FH_NOSHOOT           BIN16(00100000,00000010)

// Client map
#define SERVER_MAP_EXT			".map"
#define CLIENT_MAP_FORMAT_VER	(7)

// Coordinates
#define MAXHEX_DEF              (200)
#define MAXHEX_MIN              (10)
#define MAXHEX_MAX              (10000)

// Client parameters
#define MAX_NAME				(30)
#define MIN_NAME				(1)
#define MAX_NET_TEXT			(100)
#define MAX_SAY_NPC_TEXT		(25)
#define MAX_SCENERY				(5000)
#define MAX_DIALOG_TEXT			(MAX_FOTEXT)
#define MAX_DLG_LEN_IN_BYTES	(64*1024)
#define MAX_DLG_LEXEMS_TEXT     (1000)
#define MAX_BUF_LEN				(4096)

// Critters
#define MAX_CRIT_TYPES          (1000)
#define NPC_START_ID            (5000001)
#define USERS_START_ID          (1)
#define IS_USER_ID(id)          ((id)>0 && (id)<NPC_START_ID)
#define IS_NPC_ID(id)           ((id)>=NPC_START_ID)
#define MAX_ANSWERS				(100)
#define PROCESS_TALK_TICK       (1000)
#define DIALOGS_LST_NAME		"dialogs.lst"
#define MAX_SCRIPT_NAME			(64)
#define SCRIPTS_LST             "scripts.cfg"
#define MAX_START_SPECIAL		(40)
#define TURN_BASED_TIMEOUT      (1000)
#define MIN_VISIBLE_CRIT        (6)
#define FADING_PERIOD           (1000)

#define RESPOWN_TIME_PLAYER     (3)
#define RESPOWN_TIME_NPC        (120)
#define RESPOWN_TIME_INFINITY   (4*24*60*60000)

// Combat modes
#define COMBAT_MODE_ANY         (0)
#define COMBAT_MODE_REAL_TIME   (1)
#define COMBAT_MODE_TURN_BASED  (2)

// Turn based
#define COMBAT_TB_END_TURN      (0)
#define COMBAT_TB_END_COMBAT    (1)

// Answer
#define ANSWER_BEGIN            (0xF0)
#define ANSWER_END              (0xF1)
#define ANSWER_BARTER           (0xF2)

// Time AP
#define AP_DIVIDER              (100)

// Crit conditions
#define COND_LIFE               (1)
#define COND_KNOCKOUT           (2)
#define COND_DEAD               (3)

// Run-time critters flags
#define FCRIT_PLAYER            (0x00010000) // Player
#define FCRIT_NPC               (0x00020000) // Npc
#define FCRIT_DISCONNECT        (0x00080000) // In offline
#define FCRIT_CHOSEN            (0x00100000) // Chosen
#define FCRIT_RULEGROUP         (0x00200000) // Group rule

// Slots
#define SLOT_INV                (0)
#define SLOT_HAND1              (1)
#define SLOT_HAND2              (2)
#define SLOT_ARMOR              (3)
#define SLOT_GROUND             (255)

// Players barter
#define BARTER_DIST             (1)
	// Types
#define BARTER_TRY              (0) // opponentId, isHide
#define BARTER_ACCEPTED         (1) // opponentId, isHide
#define BARTER_BEGIN            (2)
#define BARTER_END              (3)
#define BARTER_SET_SELF         (4)
#define BARTER_SET_OPPONENT     (5)
#define BARTER_UNSET_SELF       (6)
#define BARTER_UNSET_OPPONENT   (7)
#define BARTER_OFFER            (8) // isSet, isOpponent
#define BARTER_REFRESH          (9)

// Scores
#define SCORE_SPEAKER           (3)
#define SCORE_TRADER            (4)
#define SCORES_MAX              (50)
#define SCORES_SEND_TIME        (60000)
#define SCORE_NAME_LEN          (64)

struct ScoreType
{
	uint ClientId;
	char ClientName[SCORE_NAME_LEN];
	int Value;
};

// Show screen modes
// Ouput: it is 'uint param' in Critter::ShowScreen.
// Input: I - integer value 'uint answerI', S - string value 'string& answerS' in 'answer_' function.
#define SHOW_SCREEN_CLOSE                (0)  // Close top window.
#define SHOW_SCREEN_TIMER                (1)  // Timer box. Output: picture index in INVEN.LST. Input I: time in game minutes (1..599).
#define SHOW_SCREEN_DIALOGBOX            (2)  // Dialog box. Output: buttons count - 0..20 (exit button added automatically). Input I: Choosed button - 0..19.
#define SHOW_SCREEN_SKILLBOX             (3)  // Skill box. Input I: selected skill.
#define SHOW_SCREEN_BAG                  (4)  // Bag box. Input I: id of selected item.
#define SHOW_SCREEN_SAY                  (5)  // Say box. Output: all symbols - 0 or only numbers - any other number. Input S: typed string.
#define SHOW_ELEVATOR                    (6)  // Elevator. Output: look ELEVATOR_* macro. Input I: Choosed level button.
#define SHOW_SCREEN_INVENTORY            (7)  // Inventory.
#define SHOW_SCREEN_CHARACTER            (8)  // Character.
#define SHOW_SCREEN_FIXBOY               (9)  // Fix-boy.
#define SHOW_SCREEN_PIPBOY               (10) // Pip-boy.
#define SHOW_SCREEN_MINIMAP              (11) // Mini-map.

// Parameters
#define MAX_PARAMS                  (1000)
#define SKILL_OFFSET(skill)         ((skill)-(GameOpt.AbsoluteOffsets?0:SKILL_BEGIN))
#define PERK_OFFSET(perk)           ((perk)-(GameOpt.AbsoluteOffsets?0:PERK_BEGIN))

// Stats
#define ST_STRENGTH                 (0)
#define ST_PERCEPTION               (1)
#define ST_ENDURANCE                (2)
#define ST_CHARISMA                 (3)
#define ST_INTELLECT                (4)
#define ST_AGILITY                  (5)
#define ST_LUCK                     (6)
#define ST_MAX_LIFE                 (7)
#define ST_ACTION_POINTS            (8)
#define ST_ARMOR_CLASS              (9)
#define ST_MELEE_DAMAGE             (10)
#define ST_CARRY_WEIGHT             (11)
#define ST_SEQUENCE                 (12)
#define ST_HEALING_RATE             (13)
#define ST_CRITICAL_CHANCE          (14)
#define ST_NORMAL_RESIST            (23)
#define ST_RADIATION_RESISTANCE     (30)
#define ST_POISON_RESISTANCE        (31)
#define ST_AGE                      (70)
#define ST_GENDER					(71)
#define ST_CURRENT_HP				(72)
#define ST_POISONING_LEVEL          (73)
#define ST_RADIATION_LEVEL          (74)
#define ST_CURRENT_AP				(75)
#define ST_EXPERIENCE				(76)
#define ST_LEVEL                    (77)
#define ST_UNSPENT_SKILL_POINTS     (78)
#define ST_UNSPENT_PERKS            (79)
#define ST_KARMA                    (80)
#define ST_FOLLOW_CRIT              (81)
#define ST_REPLICATION_MONEY        (82)
#define ST_REPLICATION_COUNT        (83)
#define ST_REPLICATION_TIME         (84)
#define ST_REPLICATION_COST         (85)
#define ST_TURN_BASED_AC            (86)
#define ST_MAX_MOVE_AP              (87)
#define ST_MOVE_AP                  (88)
#define ST_NPC_ROLE                 (89)
#define ST_BONUS_LOOK               (101)
#define ST_HANDS_ITEM_AND_MODE      (102)
#define ST_FREE_BARTER_PLAYER       (103)
#define ST_DIALOG_ID                (104)
#define ST_AI_ID                    (105)
#define ST_TEAM_ID                  (106)
#define ST_BAG_ID                   (107)
#define ST_LAST_WEAPON_ID           (110)
#define ST_BASE_CRTYPE              (112)
#define ST_TALK_DISTANCE            (115)
#define ST_SCALE_FACTOR             (116)
#define ST_WALK_TIME                (117)
#define ST_RUN_TIME                 (118)
#define ST_ANIM3D_LAYER_BEGIN       (150)
#define ST_ANIM3D_LAYER_END         (179)

// Skills
#define SKILL_BEGIN                 (GameOpt.SkillBegin)
#define SKILL_END                   (GameOpt.SkillEnd)
#define SKILL_COUNT                 (SKILL_END-SKILL_BEGIN+1)
#define MAX_SKILL_VAL               (300)
#define SK_UNARMED                  (203)
#define SK_FIRST_AID                (206)
#define SK_DOCTOR                   (207)
#define SK_SNEAK                    (208)
#define SK_LOCKPICK                 (209)
#define SK_STEAL                    (210)
#define SK_TRAPS                    (211)
#define SK_SCIENCE                  (212)
#define SK_REPAIR                   (213)
#define SK_SPEECH                   (214)
#define SK_BARTER                   (215)

// Tag skills
#define TAG_SKILL1                  (226)
#define TAG_SKILL2                  (227)
#define TAG_SKILL3                  (228)
#define TAG_SKILL4                  (229)

// Timeouts
#define TIMEOUT_BEGIN               (GameOpt.TimeoutBegin)
#define TIMEOUT_END                 (GameOpt.TimeoutEnd)
#define TO_SK_REPAIR                (232)
#define TO_SK_SCIENCE               (233)
#define TO_BATTLE                   (238)
#define TO_TRANSFER                 (239)
#define TO_REMOVE_FROM_GAME         (240)
#define TO_KARMA_VOTING             (242)
#define TB_BATTLE_TIMEOUT           (100000000)
#define TB_BATTLE_TIMEOUT_CHECK(to) ((to)>10000000)

// Kills
#define KILL_BEGIN                  (GameOpt.KillBegin)
#define KILL_END                    (GameOpt.KillEnd)

// Perks
#define PERK_BEGIN                  (GameOpt.PerkBegin)
#define PERK_END                    (GameOpt.PerkEnd)
#define PERK_COUNT                  (PERK_END-PERK_BEGIN+1)
#define PE_SILENT_RUNNING           (316)
#define PE_MASTER_TRADER            (318)
#define PE_SCOUT                    (346)
#define PE_QUICK_POCKETS            (349)
#define PE_SMOOTH_TALKER            (350)

// Addiction
#define ADDICTION_BEGIN             (GameOpt.AddictionBegin)
#define ADDICTION_END               (GameOpt.AddictionEnd)

// Karma
#define KARMA_BEGIN                 (GameOpt.KarmaBegin)
#define KARMA_END                   (GameOpt.KarmaEnd)

// Damages
#define DAMAGE_BEGIN                (GameOpt.DamageBegin)
#define DAMAGE_END                  (GameOpt.DamageEnd)
#define DAMAGE_POISONED             (500)
#define DAMAGE_RADIATED             (501)
#define DAMAGE_RIGHT_ARM            (503)
#define DAMAGE_LEFT_ARM             (504)
#define DAMAGE_RIGHT_LEG            (505)
#define DAMAGE_LEFT_LEG             (506)

// Modes
#define MODE_HIDE                   (510)
#define MODE_NO_STEAL               (511)
#define MODE_NO_BARTER              (512)
#define MODE_NO_ENEMY_STACK         (513)
#define MODE_NO_PVP                 (514)
#define MODE_END_COMBAT             (515)
#define MODE_DEFAULT_COMBAT         (516)
#define MODE_NO_HOME                (517)
#define MODE_GECK                   (518)
#define MODE_NO_FAVORITE_ITEM       (519)
#define MODE_NO_ITEM_GARBAGER       (520)
#define MODE_DLG_SCRIPT_BARTER      (521)
#define MODE_UNLIMITED_AMMO         (522)
#define MODE_INVULNERABLE           (527)
#define MODE_NO_FLATTEN             (528) // Client
#define MODE_RANGE_HTH              (530)
#define MODE_NO_LOOT                (532)
#define MODE_NO_PUSH                (536)
#define MODE_NO_UNARMED             (537)
#define MODE_NO_AIM                 (538)
#define MODE_NO_WALK                (539)
#define MODE_NO_RUN                 (540)
#define MODE_NO_TALK                (541)

// Traits
#define TRAIT_BEGIN                 (GameOpt.TraitBegin)
#define TRAIT_END                   (GameOpt.TraitEnd)
#define TRAIT_COUNT                 (TRAIT_END-TRAIT_BEGIN+1)

// Reputation
#define REPUTATION_BEGIN            (GameOpt.ReputationBegin)
#define REPUTATION_END              (GameOpt.ReputationEnd)

// Special send params
#define OTHER_BREAK_TIME            (0+MAX_PARAMS)
#define OTHER_WAIT_TIME             (1+MAX_PARAMS)
#define OTHER_FLAGS                 (2+MAX_PARAMS)
#define OTHER_BASE_TYPE             (3+MAX_PARAMS)
#define OTHER_MULTIHEX              (4+MAX_PARAMS)
#define OTHER_YOU_TURN              (5+MAX_PARAMS)
#define OTHER_CLEAR_MAP             (6+MAX_PARAMS)
#define OTHER_TELEPORT              (7+MAX_PARAMS)

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//                                          flags    actionExt                                                      item
#define ACTION_MOVE                 (0)  // l
#define ACTION_RUN                  (1)  // l
#define ACTION_SHOW_ITEM            (2)  // l s      prev slot                                                      +
#define ACTION_HIDE_ITEM            (3)  // l s      prev slot                                                      +
#define ACTION_MOVE_ITEM            (4)  // l s      prev slot                                                      +
#define ACTION_USE_ITEM             (5)  // l s                                                                     +
#define ACTION_DROP_ITEM            (6)  // l s      prev slot                                                      +
#define ACTION_USE_WEAPON           (7)  // l        fail attack 8 bit, use index (0-2) 4-7 bits, aim 0-3 bits      +
#define ACTION_RELOAD_WEAPON        (8)  // l s                                                                     +
#define ACTION_USE_SKILL            (9)  // l s      skill index (see SK_*)
#define ACTION_PICK_ITEM            (10) // l s                                                                     +
#define ACTION_PICK_CRITTER         (11) // l        0 - loot, 1 - steal, 2 - push
#define ACTION_OPERATE_CONTAINER    (12) // l s      transfer type * 10 + [0 - get, 1 - get all, 2 - put]           + (exclude get all)
#define ACTION_BARTER               (13) //   s      0 - item taken, 1 - item given                                 +
#define ACTION_DODGE                (14) //          0 - front, 1 - back
#define ACTION_DAMAGE               (15) //          0 - front, 1 - back
#define ACTION_DAMAGE_FORCE			(24) //          0 - front, 1 - back
#define ACTION_KNOCKOUT             (16) //   s      0 - knockout anim2begin
#define ACTION_STANDUP              (17) //   s      0 - knockout anim2end
#define ACTION_FIDGET               (18) // l
#define ACTION_DEAD                 (19) //   s      dead type anim2 (see Anim2 in _animation.fos)
#define ACTION_CONNECT              (20) //
#define ACTION_DISCONNECT           (21) //
#define ACTION_RESPAWN              (22) //   s
#define ACTION_REFRESH              (23) //   s

// Script defines
// Look checks
#define LOOK_CHECK_DIR              (0x01)
#define LOOK_CHECK_SNEAK_DIR        (0x02)
#define LOOK_CHECK_SNEAK_WEIGHT     (0x04)
#define LOOK_CHECK_TRACE            (0x08)
#define LOOK_CHECK_SCRIPT           (0x10)

// In SendMessage
#define MESSAGE_TO_VISIBLE_ME       (0)
#define MESSAGE_TO_IAM_VISIBLE      (1)
#define MESSAGE_TO_ALL_ON_MAP       (2)

// Special skill values
#define SKILL_PICK_ON_GROUND        (-1)
#define SKILL_PUT_CONT              (-2)
#define SKILL_TAKE_CONT             (-3)
#define SKILL_TAKE_ALL_CONT         (-4)
#define SKILL_LOOT_CRITTER          (-5)
#define SKILL_PUSH_CRITTER          (-6)
#define SKILL_TALK                  (-7)

// Anim1
#define ANIM1_UNARMED                (1)
// Anim2
#define ANIM2_IDLE                   (1)
#define ANIM2_WALK                   (3)
#define ANIM2_LIMP                   (4)
#define ANIM2_RUN                    (5)
#define ANIM2_PANIC_RUN              (6)
#define ANIM2_SNEAK_WALK             (7)
#define ANIM2_SNEAK_RUN              (8)
#define ANIM2_IDLE_PRONE_FRONT       (86)
#define ANIM2_IDLE_PRONE_BACK        (87)
#define ANIM2_DEAD_FRONT             (102)
#define ANIM2_DEAD_BACK              (103)

// Move params
// 6 next steps (each 5 bit) + stop bit + run bit
// Step bits: 012 - dir, 3 - allow, 4 - disallow
#define MOVE_PARAM_STEP_COUNT        (6)
#define MOVE_PARAM_STEP_BITS         (5)
#define MOVE_PARAM_STEP_DIR          (0x7)
#define MOVE_PARAM_STEP_ALLOW        (0x8)
#define MOVE_PARAM_STEP_DISALLOW     (0x10)
#define MOVE_PARAM_RUN               (0x80000000)

// Holodisks
#define USER_HOLO_TEXTMSG_FILE       "FOHOLOEXT.MSG"
#define USER_HOLO_START_NUM          (100000)
#define USER_HOLO_MAX_TITLE_LEN      (40)
#define USER_HOLO_MAX_LEN            (2000)

#endif // __DEFINES__
