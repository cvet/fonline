#ifndef __FONLINE__
#define __FONLINE__

//
// FOnline engine structures, for native working
// Version 521
// Binaries built with MSVS Community 2013 (Toolset: v120_xp), GCC 4.9.1
// Default calling convention - cdecl
//

// Detect operating system
#if defined ( _WIN32 ) || defined ( _WIN64 )
# define FO_WINDOWS
#elif defined ( __linux__ )
# define FO_LINUX
#elif defined ( __APPLE__ )
# define FO_MACOSX
#else
# error "Unknown operating system."
#endif

// Detect compiler
#if defined ( __GNUC__ )
# define FO_GCC
#elif defined ( _MSC_VER ) && !defined ( __MWERKS__ )
# define FO_MSVC
#else
# error "Unknown compiler."
#endif

// Detect CPU
#if ( defined ( FO_MSVC ) && defined ( _M_IX86 ) ) || ( defined ( FO_GCC ) && !defined ( __LP64__ ) )
# define FO_X86
#elif ( defined ( FO_MSVC ) && defined ( _M_X64 ) ) || ( defined ( FO_GCC ) && defined ( __LP64__ ) )
# define FO_X64
#else
# error "Unknown CPU."
#endif

// Detect target
#if defined ( __SERVER )
# define TARGET_NAME              SERVER
#elif defined ( __CLIENT )
# define TARGET_NAME              CLIENT
#elif defined ( __MAPPER )
# define TARGET_NAME              MAPPER
#else
# error __SERVER / __CLIENT / __MAPPER any of this must be defined
#endif

// Platform specific options
#ifdef FO_WINDOWS
# ifdef FO_MSVC
#  define EXPORT                  extern "C" __declspec( dllexport )
#  define EXPORT_UNINITIALIZED    extern "C" __declspec( dllexport ) extern
# else // FO_GCC
#  define EXPORT                  extern "C" __attribute__( ( dllexport ) )
#  define EXPORT_UNINITIALIZED    extern "C" __attribute__( ( dllexport ) ) extern
# endif
#else
# define EXPORT                   extern "C" __attribute__( ( visibility( "default" ) ) )
# define EXPORT_UNINITIALIZED     extern "C" __attribute__( ( visibility( "default" ) ) )
#endif

// STL
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <deque>
#include <algorithm>

using std::string;
using std::list;
using std::vector;
using std::map;
using std::multimap;
using std::set;
using std::deque;
using std::pair;

// AngelScript
#include "AngelScript/angelscript.h"
EXPORT_UNINITIALIZED asIScriptEngine* ASEngine;

// AngelScript add-ons
#define FONLINE_DLL
#include "AngelScript/scriptstring.h"
#include "AngelScript/scriptarray.h"
#include "AngelScript/scriptfile.h"
#include "AngelScript/scriptdictionary.h"
#include "AngelScript/scriptdict.h"
#include "AngelScript/scriptany.h"
#include "AngelScript/scriptmath.h"
#undef FONLINE_DLL

// FOnline types
struct GameOptions;
struct Mutex;
struct Spinlock;
struct SyncObj;
struct Property;
struct Properties;
struct CritterType;
struct NpcPlane;
struct GlobalMapGroup;
struct Entity;
struct ProtoItem;
struct Item;
struct CritterTimeEvent;
struct Critter;
struct Client;
struct Npc;
struct CritterCl;
struct MapObject;
struct MapEntire;
struct SceneryToClient;
struct ProtoMap;
struct Map;
struct ProtoLocation;
struct Location;
struct Field;
struct SpriteInfo;
struct Sprite;
struct SpriteAnim;

#ifdef __SERVER
typedef Critter                CritterMutual;
#else
typedef CritterCl              CritterMutual;
#endif

typedef char                   int8;
typedef unsigned char          uint8;
typedef short                  int16;
typedef unsigned short         uint16;
typedef unsigned int           uint;
#if defined ( FO_MSVC )
typedef unsigned __int64       uint64;
typedef __int64                int64;
#elif defined ( FO_GCC )
# include <inttypes.h>
typedef uint64_t               uint64;
typedef int64_t                int64;
#endif
typedef uint                   hash;

typedef pair< int, int >       IntPair;
typedef pair< uint, uint >     UintPair;
typedef pair< uint16, uint16 > Uint16Pair;

typedef vector< uint >         UintVec;
typedef vector< uint8 >        Uint8Vec;
typedef vector< uint16 >       Uint16Vec;
typedef vector< int >          IntVec;
typedef vector< UintPair >     UintPairVec;
typedef vector< Uint16Pair >   Uint16PairVec;
typedef vector< hash >         HashVec;
typedef vector< void* >        PtrVec;
typedef set< int >             IntSet;
typedef set< uint >            UintSet;
typedef map< int, int >        IntMap;

typedef vector< NpcPlane* >    NpcPlaneVec;
typedef vector< Critter* >     CrVec;
typedef map< uint, Critter* >  CrMap;
typedef vector< CritterCl* >   CrClVec;
typedef vector< Client* >      ClVec;
typedef vector< Npc* >         PcVec;
typedef vector< Item* >        ItemVec;
typedef vector< MapObject* >   MapObjectVec;
typedef vector< Map* >         MapVec;
typedef vector< Location* >    LocVec;

// Generic
EXPORT_UNINITIALIZED bool              ( * RaiseAssert )( const char* message, const char* file, int line );
EXPORT_UNINITIALIZED void              ( * Log )( const char* frmt, ... );
EXPORT_UNINITIALIZED asIScriptContext* ( *ScriptGetActiveContext )( );
EXPORT_UNINITIALIZED const char* (ScriptGetLibraryOptions) ( );
EXPORT_UNINITIALIZED const char* (ScriptGetLibraryVersion) ( );

#define FONLINE_DLL_ENTRY( isCompiler )                                        \
    GameOptions * FOnline;                                                     \
    asIScriptEngine* ASEngine;                                                 \
    EXPORT void TARGET_NAME() {}                                               \
    bool ( * RaiseAssert )( const char* message, const char* file, int line ); \
    void ( * Log )( const char* frmt, ... );                                   \
    asIScriptContext* ( *ScriptGetActiveContext )( );                          \
    const char* (ScriptGetLibraryOptions) ( );                                 \
    const char* (ScriptGetLibraryVersion) ( );                                 \
    EXPORT void DllMainEx( bool isCompiler )
// FONLINE_DLL_ENTRY

#define RUNTIME_ASSERT( a )             ( !!( a ) || RaiseAssert( # a, __FILE__, __LINE__ ) )
#define RUNTIME_ASSERT_STR( a, str )    ( !!( a ) || RaiseAssert( str, __FILE__, __LINE__ ) )
#define STATIC_ASSERT( a )              { static int arr[ ( a ) ? 1 : -1 ]; }

#define BIN__N( x )                     ( x ) | x >> 3 | x >> 6 | x >> 9
#define BIN__B( x )                     ( x ) & 0xf | ( x ) >> 12 & 0xf0
#define BIN8( v )                       ( BIN__B( BIN__N( 0x ## v ) ) )
#define BIN16( bin16, bin8 )            ( ( BIN8( bin16 ) << 8 ) | ( BIN8( bin8 ) ) )

#define FLAG( x, y )                    ( ( ( x ) & ( y ) ) != 0 )
#define CLAMP( x, low, high )           ( ( ( x ) > ( high ) ) ? ( high ) : ( ( ( x ) < ( low ) ) ? ( low ) : ( x ) ) )
#define SQRT3T2_FLOAT               ( 3.4641016151f )
#define SQRT3_FLOAT                 ( 1.732050807568877f )
#define RAD2DEG                     ( 57.29577951f )
#define CONVERT_GRAMM( x )              ( ( x ) * 453 )
#define UTF8_BUF_SIZE( count )          ( ( count ) * 4 )

#define MAX_HOLO_INFO               ( 250 )
#define SCORES_MAX                  ( 50 )
#define MAX_NPC_BAGS_PACKS          ( 20 )
#define MAX_ENEMY_STACK             ( 30 )
#define MAX_NPC_BAGS                ( 50 )
#define GM_ZONES_FOG_SIZE           ( 2500 )
#define MAX_SCRIPT_NAME             ( 64 )
#define MAPOBJ_SCRIPT_NAME          ( 25 )
#define MAX_NAME                    ( 30 )
#define PASS_HASH_SIZE              ( 32 )
#define MAX_STORED_IP               ( 20 )
#define MAX_HEX_OFFSET              ( 50 )
#define AP_DIVIDER                  ( 100 )
#define MAX_CRIT_TYPES              ( 1000 )
#define EFFECT_TEXTURES             ( 10 )
#define EFFECT_SCRIPT_VALUES        ( 10 )
#define CRITTER_USER_DATA_SIZE      ( 400 )
#define MAX_FRAMES                  ( 50 )
#define DIRS_COUNT                  ( FOnline->MapHexagonal ? 6 : 8 )
#define ITEM_EVENT_MAX              ( 8 )
#define CRITTER_EVENT_MAX           ( 44 )
#define MAP_EVENT_MAX               ( 12 )
#define LOCATION_EVENT_MAX          ( 2 )

// Vars
#define VAR_CALC_QUEST( tid, val )      ( ( tid ) * 1000 + ( val ) )
#define VAR_GLOBAL                  ( 0 )
#define VAR_LOCAL                   ( 1 )
#define VAR_UNICUM                  ( 2 )
#define VAR_LOCAL_LOCATION          ( 3 )
#define VAR_LOCAL_MAP               ( 4 )
#define VAR_LOCAL_ITEM              ( 5 )
#define VAR_FLAG_QUEST              ( 0x1 )
#define VAR_FLAG_RANDOM             ( 0x2 )
#define VAR_FLAG_NO_CHECK           ( 0x4 )

// Items
#define ITEM_MAX_SCRIPT_VALUES      ( 10 )
#define USE_PRIMARY                 ( 0 )
#define USE_SECONDARY               ( 1 )
#define USE_THIRD                   ( 2 )
#define USE_RELOAD                  ( 3 )
#define USE_USE                     ( 4 )
#define MAX_USES                    ( 3 )
#define USE_NONE                    ( 15 )
#define ITEM_ACCESSORY_NONE         ( 0 )
#define ITEM_ACCESSORY_CRITTER      ( 1 )
#define ITEM_ACCESSORY_HEX          ( 2 )
#define ITEM_ACCESSORY_CONTAINER    ( 3 )

// Events
#define MAP_LOOP_FUNC_MAX           ( 5 )
#define MAP_MAX_DATA                ( 100 )

// Sprites cutting
#define SPRITE_CUT_HORIZONTAL       ( 1 )
#define SPRITE_CUT_VERTICAL         ( 2 )

// Map blocks
#define FH_BLOCK                    BIN8( 00000001 )
#define FH_NOTRAKE                  BIN8( 00000010 )
#define FH_WALL                     BIN8( 00000100 )
#define FH_SCEN                     BIN8( 00001000 )
#define FH_SCEN_GRID                BIN8( 00010000 )
#define FH_TRIGGER                  BIN8( 00100000 )
#define FH_CRITTER                  BIN8( 00000001 )
#define FH_DEAD_CRITTER             BIN8( 00000010 )
#define FH_ITEM                     BIN8( 00000100 )
#define FH_BLOCK_ITEM               BIN8( 00010000 )
#define FH_NRAKE_ITEM               BIN8( 00100000 )
#define FH_WALK_ITEM                BIN8( 01000000 )
#define FH_GAG_ITEM                 BIN8( 10000000 )
#define FH_NOWAY                    BIN16( 00010001, 00000001 )
#define FH_NOSHOOT                  BIN16( 00100000, 00000010 )

// GameOptions::IndicatorType
#define INDICATOR_LINES             ( 0 )
#define INDICATOR_NUMBERS           ( 1 )
#define INDICATOR_BOTH              ( 2 )
// GameOptions::Zoom
#define MIN_ZOOM                    ( 0.2f )
#define MAX_ZOOM                    ( 10.0f )

struct GameOptions
{
    const uint16        YearStart;
    const uint          YearStartFTLo;
    const uint          YearStartFTHi;
    const uint16        Year;
    const uint16        Month;
    const uint16        Day;
    const uint16        Hour;
    const uint16        Minute;
    const uint16        Second;
    const uint          FullSecondStart;
    const uint          FullSecond;
    const uint16        TimeMultiplier;
    const uint          GameTimeTick;

    const bool          Singleplayer;
    const bool          DisableTcpNagle;
    const bool          DisableZlibCompression;
    const uint          FloodSize;
    const uint          BruteForceTick;
    const bool          NoAnswerShuffle;
    const bool          DialogDemandRecheck;
    const uint          FixBoyDefaultExperience;
    const uint          SneakDivider;
    const uint          LevelCap;
    const bool          LevelCapAddExperience;
    const uint          LookNormal;
    const uint          LookMinimum;
    const uint          GlobalMapMaxGroupCount;
    const uint          CritterIdleTick;
    const uint          TurnBasedTick;
    const int           DeadHitPoints;
    const uint          Breaktime;
    const uint          TimeoutTransfer;
    const uint          TimeoutBattle;
    const uint          ApRegeneration;
    const uint          RtApCostCritterWalk;
    const uint          RtApCostCritterRun;
    const uint          RtApCostMoveItemContainer;
    const uint          RtApCostMoveItemInventory;
    const uint          RtApCostPickItem;
    const uint          RtApCostDropItem;
    const uint          RtApCostReloadWeapon;
    const uint          RtApCostPickCritter;
    const uint          RtApCostUseItem;
    const uint          RtApCostUseSkill;
    const bool          RtAlwaysRun;
    const uint          TbApCostCritterMove;
    const uint          TbApCostMoveItemContainer;
    const uint          TbApCostMoveItemInventory;
    const uint          TbApCostPickItem;
    const uint          TbApCostDropItem;
    const uint          TbApCostReloadWeapon;
    const uint          TbApCostPickCritter;
    const uint          TbApCostUseItem;
    const uint          TbApCostUseSkill;
    const bool          TbAlwaysRun;
    const uint          ApCostAimEyes;
    const uint          ApCostAimHead;
    const uint          ApCostAimGroin;
    const uint          ApCostAimTorso;
    const uint          ApCostAimArms;
    const uint          ApCostAimLegs;
    const bool          RunOnCombat;
    const bool          RunOnTransfer;
    const bool          RunOnTurnBased;
    const uint          GlobalMapWidth;
    const uint          GlobalMapHeight;
    const uint          GlobalMapZoneLength;
    const uint          GlobalMapMoveTime;
    const uint          BagRefreshTime;
    const uint          AttackAnimationsMinDist;
    const uint          WhisperDist;
    const uint          ShoutDist;
    const int           LookChecks;
    const uint          LookDir[ 5 ];
    const uint          LookSneakDir[ 5 ];
    const uint          LookWeight;
    const bool          CustomItemCost;
    const uint          RegistrationTimeout;
    const uint          AccountPlayTime;
    const bool          LoggingVars;
    const uint          ScriptRunSuspendTimeout;
    const uint          ScriptRunMessageTimeout;
    const uint          TalkDistance;
    const uint          NpcMaxTalkers;
    const uint          MinNameLength;
    const uint          MaxNameLength;
    const uint          DlgTalkMinTime;
    const uint          DlgBarterMinTime;
    const uint          MinimumOfflineTime;
    const bool          GameServer;
    const bool          UpdateServer;
    const bool          GenerateWorldDisabled;
    const bool          BuildMapperScripts;
    const ScriptString* CommandLine;

    const int           StartSpecialPoints;
    const int           StartTagSkillPoints;
    const int           SkillMaxValue;
    const int           SkillModAdd2;
    const int           SkillModAdd3;
    const int           SkillModAdd4;
    const int           SkillModAdd5;
    const int           SkillModAdd6;

    const int           ReputationLoved;
    const int           ReputationLiked;
    const int           ReputationAccepted;
    const int           ReputationNeutral;
    const int           ReputationAntipathy;
    const int           ReputationHated;

    const bool          MapHexagonal;
    const int           MapHexWidth;
    const int           MapHexHeight;
    const int           MapHexLineHeight;
    const int           MapTileOffsX;
    const int           MapTileOffsY;
    const int           MapTileStep;
    const int           MapRoofOffsX;
    const int           MapRoofOffsY;
    const int           MapRoofSkipSize;
    const float         MapCameraAngle;
    const bool          MapSmoothPath;
    const ScriptString& MapDataPrefix;

    // Client and Mapper
    const bool          Quit;
    const bool          OpenGLRendering;
    const bool          OpenGLDebug;
    const bool          AssimpLogging;
    const int           MouseX;
    const int           MouseY;
    const int           ScrOx;
    const int           ScrOy;
    const bool          ShowTile;
    const bool          ShowRoof;
    const bool          ShowItem;
    const bool          ShowScen;
    const bool          ShowWall;
    const bool          ShowCrit;
    const bool          ShowFast;
    const bool          ShowPlayerNames;
    const bool          ShowNpcNames;
    const bool          ShowCritId;
    const bool          ScrollKeybLeft;
    const bool          ScrollKeybRight;
    const bool          ScrollKeybUp;
    const bool          ScrollKeybDown;
    const bool          ScrollMouseLeft;
    const bool          ScrollMouseRight;
    const bool          ScrollMouseUp;
    const bool          ScrollMouseDown;
    const bool          ShowGroups;
    const bool          HelpInfo;
    const bool          DebugInfo;
    const bool          DebugNet;
    const bool          Enable3dRendering;
    const bool          FullScreen;
    const bool          VSync;
    const int           Light;
    const ScriptString& Host;
    const uint          Port;
    const ScriptString& UpdateServerHost;
    const uint          UpdateServerPort;
    const uint          ProxyType;
    const ScriptString& ProxyHost;
    const uint          ProxyPort;
    const ScriptString& ProxyUser;
    const ScriptString& ProxyPass;
    const ScriptString& Name;
    const int           ScrollDelay;
    const uint          ScrollStep;
    const bool          ScrollCheck;
    const int           FixedFPS;
    const uint          FPS;
    const uint          PingPeriod;
    const uint          Ping;
    const bool          MsgboxInvert;
    const uint8         DefaultCombatMode;
    const bool          MessNotify;
    const bool          SoundNotify;
    const bool          AlwaysOnTop;
    const uint          TextDelay;
    const uint          DamageHitDelay;
    const int           ScreenWidth;
    const int           ScreenHeight;
    const int           MultiSampling;
    const bool          MouseScroll;
    const int           IndicatorType;
    const uint          DoubleClickTime;
    const uint8         RoofAlpha;
    const bool          HideCursor;
    const bool          DisableLMenu;
    const bool          DisableMouseEvents;
    const bool          DisableKeyboardEvents;
    const bool          HidePassword;
    const ScriptString& PlayerOffAppendix;
    const int           CombatMessagesType;
    const uint          Animation3dSmoothTime;
    const uint          Animation3dFPS;
    const int           RunModMul;
    const int           RunModDiv;
    const int           RunModAdd;
    const bool          MapZooming;
    const float         SpritesZoom;
    const float         SpritesZoomMax;
    const float         SpritesZoomMin;
    const float         EffectValues[ EFFECT_SCRIPT_VALUES ];
    const bool          AlwaysRun;
    const int           AlwaysRunMoveDist;
    const int           AlwaysRunUseDist;
    const ScriptString& KeyboardRemap;
    const uint          CritterFidgetTime;
    const uint          Anim2CombatBegin;
    const uint          Anim2CombatIdle;
    const uint          Anim2CombatEnd;
    const uint          RainTick;
    const int16         RainSpeedX;
    const int16         RainSpeedY;
    const uint          ConsoleHistorySize;
    const int           SoundVolume;
    const int           MusicVolume;
    const ScriptString& RegName;
    const ScriptString& RegPassword;
    const uint          ChosenLightColor;
    const uint8         ChosenLightDistance;
    const int           ChosenLightIntensity;
    const uint8         ChosenLightFlags;

    // Mapper
    const ScriptString& ClientPath;
    const ScriptString& ServerPath;
    const bool          ShowCorners;
    const bool          ShowCuttedSprites;
    const bool          ShowDrawOrder;
    const bool          SplitTilesCollection;

    // Engine data
    CritterType*        CritterTypes;                                                                       // Array of critter types, maximum is MAX_CRIT_TYPES

    Field*              ClientMap;                                                                          // Array of client map hexes, accessing - ClientMap[ hexY * ClientMapWidth + hexX ]
    uint8*              ClientMapLight;                                                                     // Hex light, accessing - ClientMapLight[ hexY * ClientMapWidth * 3 + hexX * 3 {+ 0(R), 1(G), 2(B)} ]
    uint                ClientMapWidth;                                                                     // Map width
    uint                ClientMapHeight;                                                                    // Map height

    Sprite**            ( *GetDrawingSprites )( uint & count );                                             // Array of currently drawing sprites, tree is sorted
    SpriteInfo*         ( *GetSpriteInfo )(uint sprId);                                                     // Sprite information
    uint                ( * GetSpriteColor )( uint sprId, int x, int y, bool affectZoom );                  // Color of pixel on sprite
    bool                ( * IsSpriteHit )( Sprite* sprite, int x, int y, bool checkEgg );                   // Is position hitting sprite

    const char*         ( *GetNameByHash )(hash h);                                                         // Get name of file by hash
    hash                ( * GetHashByName )( const char* name );                                            // Get hash of file name

    bool                ( * ScriptLoadModule )( const char* moduleName );
    uint                ( * ScriptBind )( const char* moduleName, const char* funcDecl, bool temporaryId ); // Returning bindId
    bool                ( * ScriptPrepare )( uint bindId );
    void                ( * ScriptSetArgInt8 )( int8 value );
    void                ( * ScriptSetArgInt16 )( int16 value );
    void                ( * ScriptSetArgInt )( int value );
    void                ( * ScriptSetArgInt64 )( int64 value );
    void                ( * ScriptSetArgUInt8 )( uint8 value );
    void                ( * ScriptSetArgUInt16 )( uint16 value );
    void                ( * ScriptSetArgUInt )( uint value );
    void                ( * ScriptSetArgUInt64 )( uint64 value );
    void                ( * ScriptSetArgBool )( bool value );
    void                ( * ScriptSetArgFloat )( float value );
    void                ( * ScriptSetArgDouble )( double value );
    void                ( * ScriptSetArgObject )( void* value );
    void                ( * ScriptSetArgAddress )( void* value );
    bool                ( * ScriptRunPrepared )();
    int8                ( * ScriptGetReturnedInt8 )();
    int16               ( * ScriptGetReturnedInt16 )();
    int                 ( * ScriptGetReturnedInt )();
    int64               ( * ScriptGetReturnedInt64 )();
    uint8               ( * ScriptGetReturnedUInt8 )();
    uint16              ( * ScriptGetReturnedUInt16 )();
    uint                ( * ScriptGetReturnedUInt )();
    uint64              ( * ScriptGetReturnedUInt64 )();
    bool                ( * ScriptGetReturnedBool )();
    float               ( * ScriptGetReturnedFloat )();
    double              ( * ScriptGetReturnedDouble )();
    void*               ( *ScriptGetReturnedObject )( );
    void*               ( *ScriptGetReturnedAddress )( );

    int                 ( * Random )( int minimum, int maximumInclusive );
    uint                ( * GetTick )();
    void                ( * SetLogCallback )( void ( * function )( const char* str ), bool enable );
    void                ( * AddPropertyCallback )( void ( * function )( Entity* entity, Property* prop, void* curValue, void* oldValue ) );
};
EXPORT_UNINITIALIZED GameOptions* FOnline;

struct Mutex
{
    const int Locker1[ 6 ];      // Windows - CRITICAL_SECTION (Locker1), Linux - pthread_mutex_t (Locker1)
    const int Locker2[ 5 ];      // MacOSX - pthread_mutex_t (Locker1 + Locker2)
};

struct Spinlock
{
    const long Locker;
};

struct SyncObj
{
    const void* CurMngr;
};

struct Property
{
    const string         PropName;
    const string         TypeName;
    const int            DataType;
    const asIObjectType* AsObjType;
    const bool           IsIntDataType;
    const bool           IsSignedIntDataType;
    const bool           IsFloatDataType;
    const bool           IsBoolDataType;
    const bool           IsEnumDataType;
    const bool           IsArrayOfString;
    const bool           IsDictOfString;
    const bool           IsDictOfArray;
    const bool           IsDictOfArrayOfString;
    const int            AccessType;
    const bool           IsReadable;
    const bool           IsWritable;
    const bool           GenerateRandomValue;
    const bool           SetDefaultValue;
    const bool           CheckMinValue;
    const bool           CheckMaxValue;
    const int64          DefaultValue;
    const int64          MinValue;
    const int64          MaxValue;
    const void*          Registrator;
    const uint           RegIndex;
    const int            EnumValue;
    const uint           PodDataOffset;
    const uint           ComplexDataIndex;
    const uint           BaseSize;
    const uint           GetCallback;
    const uint           GetCallbackArgs;
    const UintVec        SetCallbacks;
    const IntVec         SetCallbacksArgs;
    const bool           SetCallbacksAnyOldValue;
    const void*          NativeSetCallback;
    const void*          NativeSendCallback;
    const bool           GetCallbackLocked;
    const bool           SetCallbackLocked;
    const void*          SendIgnoreEntity;
};

struct Properties
{
    const void*     Registrator;
    const uint8*    PodData;
    const PtrVec    ComplexData;
    const UintVec   ComplexDataSizes;
    const PtrVec    StoreData;
    const UintVec   StoreDataSizes;
    const Uint16Vec StoreDataComplexIndicies;
    const PtrVec    UnresolvedProperties;
};

struct Methods
{
    const void*   Registrator;
    const UintVec WatcherBindIds;
};

struct CritterType
{
    const bool Enabled;
    const char Name[ 64 ];
    const char SoundName[ 64 ];
    const uint Alias;
    const uint Multihex;
    const int  AnimType;

    const bool CanWalk;
    const bool CanRun;
    const bool CanAim;
    const bool CanArmor;
    const bool CanRotate;

    const bool Anim1[ 37 ];   // A..Z 0..9
};

struct NpcPlane
{
    const int       Type;
    const uint      Priority;
    const int       Identifier;
    const uint      IdentifierExt;
    const NpcPlane* ChildPlane;
    const bool      IsMove;

    union
    {
        struct
        {
            bool IsRun;
            uint WaitSecond;
            int  ScriptBindId;
        } const Misc;

        struct
        {
            bool   IsRun;
            uint   TargId;
            int    MinHp;
            bool   IsGag;
            uint16 GagHexX, GagHexY;
            uint16 LastHexX, LastHexY;
        } const Attack;

        struct
        {
            bool   IsRun;
            uint16 HexX;
            uint16 HexY;
            uint8  Dir;
            uint   Cut;
        } const Walk;

        struct
        {
            bool   IsRun;
            uint16 HexX;
            uint16 HexY;
            uint16 Pid;
            uint   UseItemId;
            bool   ToOpen;
        } const Pick;

        struct
        {
            uint Buffer[ 8 ];
        } const Buffer;
    };

    struct
    {
        const uint   PathNum;
        const uint   Iter;
        const bool   IsRun;
        const uint   TargId;
        const uint16 HexX;
        const uint16 HexY;
        const uint   Cut;
        const uint   Trace;
    } Move;

    const bool      Assigned;
    const int       RefCounter;

    const NpcPlane* GetCurPlane()           const { return ChildPlane ? ChildPlane->GetCurPlane() : this; }
    bool            IsSelfOrHas( int type ) const { return Type == type || ( ChildPlane ? ChildPlane->IsSelfOrHas( type ) : false ); }
    uint            GetChildIndex( NpcPlane* child ) const
    {
        uint index = 0;
        for( const NpcPlane* child_ = this; child_; index++ )
        {
            if( child_ == child ) break;
            else child_ = child_->ChildPlane;
        }
        return index;
    }
    uint GetChildsCount() const
    {
        uint            count = 0;
        const NpcPlane* child = ChildPlane;
        for( ; child; count++, child = child->ChildPlane ) ;
        return count;
    }
};

enum class EntityType
{
    Invalid = 0,
    Custom,
    Item,
    Client,
    Npc,
    Location,
    Map,
    CritterCl,
    ItemHex,
    Global,
    ClientMap,
    ClientLocation,
    ProtoItem,
    ProtoItemExt,
    ProtoCritter,
    Max,
};

struct Entity
{
    const Properties Props;
    const Methods    Meths;
    const uint       Id;
    const EntityType Type;
    const long       RefCounter;
    const bool       IsDestroyed;
    const bool       IsDestroying;
};

struct ProtoItem: Entity
{
    const Properties Props;
    const hash       ProtoId;
    const Properties ItemProps;
    const int64      InstanceCount;
    const UintVec    TextsLang;
    const PtrVec     Texts;

    #ifdef __SERVER
    const string     ScriptName;
    #endif
    #ifdef __MAPPER
    const string     CollectionName;
    #endif
};

struct Item: Entity
{
    const ProtoItem* Proto;
    const uint8      Accessory;
    const bool       ViewPlaceOnMap;

    union
    {
        struct
        {
            uint   MapId;
            uint16 HexX;
            uint16 HexY;
        } const AccHex;

        struct
        {
            uint  Id;
            uint8 Slot;
        } const AccCritter;

        struct
        {
            uint ContainerId;
            uint StackId;
        } const AccContainer;

        const char AccBuffer[ 8 ];
    };

    #ifdef __SERVER
    const int      FuncId[ ITEM_EVENT_MAX ];
    const Critter* ViewByCritter;
    const ItemVec* ChildItems;
    const SyncObj  Sync;
    #endif
};

struct GlobalMapGroup
{
    const CrVec    Group;
    const Critter* Rule;
    const uint     CarId;
    const float    CurX, CurY;
    const float    ToX, ToY;
    const float    Speed;
    const bool     IsSetMove;
    const uint     TimeCanFollow;
    const bool     IsMultiply;
    const uint     ProcessLastTick;
    const uint     EncounterDescriptor;
    const uint     EncounterTick;
    const bool     EncounterForce;
};

struct CritterTimeEvent
{
    const hash FuncNum;
    const uint Rate;
    const uint NextTime;
    const int  Identifier;
};
typedef vector< CritterTimeEvent >                 CritterTimeEventVec;
typedef vector< CritterTimeEvent >::const_iterator CritterTimeEventVecIt;

struct Critter: Entity
{
    const hash          ProtoId;
    const uint          CrType;
    const uint8         Cond;
    const bool          ClientToDelete;
    const uint16        HexX;
    const uint16        HexY;
    const uint8         Dir;
    const int           Multihex;
    const uint16        WorldX;
    const uint16        WorldY;
    const uint          MapId;
    const hash          MapPid;
    const uint          GlobalGroupUid;
    const uint16        LastMapHexX;
    const uint16        LastMapHexY;
    const uint          Anim1Life;
    const uint          Anim1Knockout;
    const uint          Anim1Dead;
    const uint          Anim2Life;
    const uint          Anim2Knockout;
    const uint          Anim2Dead;
    const uint          Anim2KnockoutEnd;
    const uint8         GlobalMapFog[ GM_ZONES_FOG_SIZE ];
    uint                UserData[ 10 ];
    const uint          Reserved[ 10 ];

    const SyncObj       Sync;
    const bool          CritterIsNpc;
    const uint          Flags;
    const ScriptString& NameStr;

    struct
    {
        const bool   IsAlloc;
        const uint8* Data;
        const uint   Width;
        const uint   Height;
        const uint   WidthB;
    } const GMapFog;

    const bool                IsRuning;
    const uint                PrevHexTick;
    const uint16              PrevHexX, PrevHexY;
    const int                 LockMapTransfers;
    const uint                AllowedToDownloadMap;
    const CrVec               VisCr;
    const CrVec               VisCrSelf;
    const CrMap               VisCrMap;
    const CrMap               VisCrSelfMap;
    const UintSet             VisCr1, VisCr2, VisCr3;
    const UintSet             VisItem;
    const Spinlock            VisItemLocker;
    const uint                ViewMapId;
    const hash                ViewMapPid;
    const uint16              ViewMapLook, ViewMapHx, ViewMapHy;
    const uint8               ViewMapDir;
    const uint                ViewMapLocId, ViewMapLocEnt;

    const GlobalMapGroup*     GroupSelf;
    const GlobalMapGroup*     GroupMove;

    const ItemVec             InvItems;
    const Item*               DefItemSlotHand;
    const Item*               DefItemSlotArmor;
    const Item*               ItemSlotMain;
    const Item*               ItemSlotExt;
    const Item*               ItemSlotArmor;
    const int                 FuncId[ CRITTER_EVENT_MAX ];
    const uint                KnockoutAp;
    const uint                NextIntellectCachingTick;
    const uint16              IntellectCacheValue;
    const uint                LookCacheValue;
    const uint                StartBreakTime;
    const uint                BreakTime;
    const uint                WaitEndTick;
    const int                 DisableSend;
    const uint                AccessContainerId;
    const uint                ItemTransferCount;
    const uint                TryingGoHomeTick;

    const CritterTimeEventVec CrTimeEvents;

    const uint                GlobalIdleNextTick;
    const uint                ApRegenerationTick;
    const bool                CanBeRemoved;
};

struct Client: Critter
{
    const char  Name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    const char  PassHash[ PASS_HASH_SIZE ];
    const uint8 Access;
    const uint  LanguageMsg;
};

struct Npc: Critter
{
    const uint        NextRefreshBagTick;
    const NpcPlaneVec AiPlanes;
    const uint        Reserved;
};

struct CritterCl: Entity
{
    const hash          Pid;
    const uint16        HexX, HexY;
    const uint8         Dir;
    const uint          NameColor;
    const uint          ContourColor;
    const Uint16Vec     LastHexX, LastHexY;
    const uint8         Cond;
    const uint          Anim1Life;
    const uint          Anim1Knockout;
    const uint          Anim1Dead;
    const uint          Anim2Life;
    const uint          Anim2Knockout;
    const uint          Anim2Dead;
    const uint          Flags;
    const uint          CrType, CrTypeAlias;
    const uint          ApRegenerationTick;
    const int           Multihex;
    void*               DrawEffect;

    const ScriptString& Name;
    const ScriptString& NameOnHead;
    const ScriptString& Avatar;

    const ItemVec       InvItems;
    const Item*         DefItemSlotHand;
    const Item*         DefItemSlotArmor;
    const Item*         ItemSlotMain;
    const Item*         ItemSlotExt;
    const Item*         ItemSlotArmor;

    const bool          IsRuning;
    const Uint16PairVec MoveSteps;
};

struct MapObject
{
    const uint8         MapObjType;
    const hash          ProtoId;
    #ifdef __MAPPER
    const ScriptString* ProtoName;
    #endif
    const uint16        MapX;
    const uint16        MapY;

    const uint          UID;
    const uint          ContainerUID;
    const uint          ParentUID;
    const uint          ParentChildIndex;

    const uint          LightRGB;
    const uint8         LightDay;
    const uint8         LightDirOff;
    const uint8         LightDistance;
    const int8          LightIntensity;

    const char          ScriptName[ MAPOBJ_SCRIPT_NAME + 1 ];
    const char          FuncName[ MAPOBJ_SCRIPT_NAME + 1 ];

    const int           UserData[ 10 ];

    const PtrVec*       Props;

    union
    {
        struct
        {
            uint8 Dir;
            uint8 Cond;
            uint  Anim1;
            uint  Anim2;
        } const MCritter;

        struct
        {
            int16  OffsetX;
            int16  OffsetY;
            hash   PicMap;
            hash   PicInv;

            uint   Count;
            uint8  ItemSlot;

            uint8  BrokenFlags;
            uint8  BrokenCount;
            uint16 Deterioration;

            hash   AmmoPid;
            uint   AmmoCount;

            uint   LockerDoorId;
            uint16 LockerCondition;
            uint16 LockerComplexity;

            int16  TrapValue;

            int    Val[ 10 ];
        } const MItem;

        struct
        {
            int16 OffsetX;
            int16 OffsetY;
            hash  PicMap;
            hash  PicInv;

            bool  CanUse;
            bool  CanTalk;
            uint  TriggerNum;

            uint8 ParamsCount;
            int   Param[ 5 ];

            hash  ToMap;
            uint  ToEntire;
            uint8 ToDir;
        } const MScenery;
    };
};

struct MapEntire
{
    const uint   Number;
    const uint16 HexX;
    const uint16 HexY;
    const uint8  Dir;
};
typedef vector< MapEntire > EntiresVec;

struct SceneryToClient
{
    const hash   ProtoId;
    const hash   PicMap;
    const uint16 MapX;
    const uint16 MapY;
    const int16  OffsetX;
    const int16  OffsetY;
    const uint   LightColor;
    const uint8  LightDistance;
    const uint8  LightFlags;
    const int8   LightIntensity;
    const uint8  Flags;
    const uint8  SpriteCut;
};
typedef vector< SceneryToClient > SceneryToClientVec;

struct ProtoMap
{
    struct
    {
        const uint   Version;
        const uint16 MaxHexX, MaxHexY;
        const int    WorkHexX, WorkHexY;
        const char   ScriptModule[ MAX_SCRIPT_NAME + 1 ];
        const char   ScriptFunc[ MAX_SCRIPT_NAME + 1 ];
        const int    Time;
        const bool   NoLogOut;
        const int    DayTime[ 4 ];
        const uint8  DayColor[ 12 ];

        // Deprecated
        const uint16 HeaderSize;
        const bool   Packed;
        const uint   UnpackedDataLen;
    } const Header;

    const MapObjectVec MObjects;
    const uint         LastObjectUID;

    struct Tile
    {
        const hash   Name;
        const uint16 HexX, HexY;
        const int16  OffsX, OffsY;
        const uint8  Layer;
        const bool   IsRoof;
        #ifdef __MAPPER
        const bool   IsSelected;
        #endif
    };
    typedef vector< Tile >    TileVec;
    const TileVec    Tiles;

    #ifdef __MAPPER
    typedef vector< TileVec > TileVecVec;
    const TileVecVec TilesField;
    const TileVecVec RoofsField;

    const TileVec&   GetTiles( uint16 hx, uint16 hy, bool is_roof ) const
    {
        uint index = hy * Header.MaxHexX + hx;
        return is_roof ? RoofsField[ index ] : TilesField[ index ];
    }
    #endif

    #ifdef __SERVER
    const SceneryToClientVec WallsToSend;
    const SceneryToClientVec SceneriesToSend;
    const uint               HashTiles;
    const uint               HashWalls;
    const uint               HashScen;

    const MapObjectVec       CrittersVec;
    const MapObjectVec       ItemsVec;
    const MapObjectVec       SceneriesVec;
    const MapObjectVec       GridsVec;
    const uint8*             HexFlags;
    #endif

    const EntiresVec         MapEntires;

    const string             Name;
    const hash               Pid;
};

struct Map: Entity
{
    const SyncObj   Sync;
    const Mutex     DataLocker;
    const uint8*    HexFlags;
    const CrVec     MapCritters;
    const ClVec     MapPlayers;
    const PcVec     MapNpcs;
    const ItemVec   HexItems;
    const Location* MapLocation;

    struct
    {
        const hash  MapPid;
        const uint8 MapRain;
        const bool  IsTurnBasedAviable;
        const int   MapTime;
        const hash  ScriptId;
        const int   MapDayTime[ 4 ];
        const uint8 MapDayColor[ 12 ];
        const uint  Reserved[ 20 ];
        const int   UserData[ MAP_MAX_DATA ];
    } const Data;

    const ProtoMap* Proto;

    const bool      NeedProcess;
    const int       FuncId[ MAP_EVENT_MAX ];
    const uint      LoopEnabled[ MAP_LOOP_FUNC_MAX ];
    const uint      LoopLastTick[ MAP_LOOP_FUNC_MAX ];
    const uint      LoopWaitTick[ MAP_LOOP_FUNC_MAX ];

    const bool      IsTurnBasedOn;
    const uint      TurnBasedEndTick;
    const int       TurnSequenceCur;
    const UintVec   TurnSequence;
    const bool      IsTurnBasedTimeout;
    const uint      TurnBasedBeginSecond;
    const bool      NeedEndTurnBased;
    const uint      TurnBasedRound;
    const uint      TurnBasedTurn;
    const uint      TurnBasedWholeTurn;

    uint16 GetMaxHexX() const { return Proto->Header.MaxHexX; }
    uint16 GetMaxHexY() const { return Proto->Header.MaxHexY; }

    #ifdef __SERVER
    bool   IsHexTrigger( uint16 hx, uint16 hy ) const { return FLAG( Proto->HexFlags[ hy * GetMaxHexX() + hx ], FH_TRIGGER ); }
    bool   IsHexTrap( uint16 hx, uint16 hy )    const { return FLAG( HexFlags[ hy * GetMaxHexX() + hx ], FH_WALK_ITEM ); }
    bool   IsHexCritter( uint16 hx, uint16 hy ) const { return FLAG( HexFlags[ hy * GetMaxHexX() + hx ], FH_CRITTER | FH_DEAD_CRITTER ); }
    bool   IsHexGag( uint16 hx, uint16 hy )     const { return FLAG( HexFlags[ hy * GetMaxHexX() + hx ], FH_GAG_ITEM ); }
    uint16 GetHexFlags( uint16 hx, uint16 hy )  const { return ( HexFlags[ hy * GetMaxHexX() + hx ] << 8 ) | Proto->HexFlags[ hy * GetMaxHexX() + hx ]; }
    bool   IsHexPassed( uint16 hx, uint16 hy )  const { return !FLAG( GetHexFlags( hx, hy ), FH_NOWAY ); }
    bool   IsHexRaked( uint16 hx, uint16 hy )   const { return !FLAG( GetHexFlags( hx, hy ), FH_NOSHOOT ); }
    #endif
};

struct ProtoLocation
{
    const hash        LocPid;
    const string      Name;

    const uint        MaxPlayers;
    const uint16      Radius;
    const bool        Visible;
    const bool        AutoGarbage;
    const bool        GeckVisible;

    const HashVec     ProtoMapPids;
    const HashVec     AutomapsPids;
    const UintPairVec Entrance;
    const int         EntranceScriptBindId;

    const UintVec     TextsLang;
    const IntVec      Texts;     // It is not int vector, do not use
};

struct Location
{
    const SyncObj Sync;
    const MapVec  LocMaps;

    struct
    {
        const hash   LocPid;
        const uint16 WX;
        const uint16 WY;
        const uint16 Radius;
        const bool   Visible;
        const bool   GeckVisible;
        const bool   AutoGarbage;
        const uint   Color;
        const uint   Reserved3[ 59 ];
    } const Data;

    const ProtoLocation* Proto;
    const int            GeckCount;
    const int            FuncId[ LOCATION_EVENT_MAX ];

    bool                 IsVisible()   const { return Data.Visible || ( Data.GeckVisible && GeckCount > 0 ); }
};

struct Field
{
    struct Tile
    {
        const SpriteAnim* Anim;
        const int16       OffsX;
        const int16       OffsY;
        const uint8       Layer;
    };
    typedef vector< Tile > TileVec;

    const CritterCl*  Crit;
    const CrClVec*    DeadCrits;       // NULL if empty
    const int         ScrX;
    const int         ScrY;
    const SpriteAnim* SimplyTile[ 2 ]; // First tile with zero offsets and layer (if present), 0 - Tile, 1 - Roof
    const TileVec*    Tiles[ 2 ];      // NULL if empty, 0 - Tile, 1 - Roof
    const ItemVec*    Items;           // NULL if empty
    const int16       RoofNum;

    struct
    {
        const bool ScrollBlock : 1;
        const bool IsWall : 1;
        const bool IsWallSAI : 1;
        const bool IsWallTransp : 1;
        const bool IsScen : 1;
        const bool IsExitGrid : 1;
        const bool IsNotPassed : 1;
        const bool IsNotRaked : 1;
        const bool IsNoLight : 1;
        const bool IsMultihex : 1;
    } const Flags;

    const uint8 Corner;
};

struct SpriteInfo
{
    const void*  Surface;
    const float  SurfaceUV[ 4 ];
    const uint16 Width;
    const uint16 Height;
    const int16  OffsX;
    const int16  OffsY;
    const void*  DrawEffect;
    const bool   UsedForAnim3d;
    const void*  Anim3d;
    const uint8* Data;
    const int    DataAtlasType;
    const bool   DataAtlasOneImage;
};

struct Sprite
{
    // Ordering
    const int     DrawOrderType;     // 0..4 - flat, 5..9 - normal
    const uint    DrawOrderPos;
    const uint    TreeIndex;

    // Sprite information, pass to GetSpriteInfo
    const uint    SprId;
    const uint*   PSprId;     // If PSprId == NULL than used SprId

    // Positions
    const int     HexX, HexY;
    const int     ScrX, ScrY;
    const int16*  OffsX, * OffsY;

    // Cutting
    const int     CutType;     // See Sprites cutting
    const Sprite* Parent, * Child;
    const float   CutX, CutW, CutTexL, CutTexR;

    // Other
    const uint8*  Alpha;
    const uint8*  Light;
    const uint8*  LightRight;
    const uint8*  LightLeft;
    const int     EggType;
    const int     ContourType;
    const uint    ContourColor;
    const uint    Color;
    const uint    FlashMask;
    const void*   DrawEffect;
    const bool*   ValidCallback;
    const bool    Valid;     // If Valid == false than this sprite not valid

    #ifdef __MAPPER
    const int     CutOyL, CutOyR;
    #endif

    uint const GetSprId()
    {
        return PSprId ? *PSprId : SprId;
    }

    SpriteInfo* const GetSprInfo()
    {
        return FOnline->GetSpriteInfo( PSprId ? *PSprId : SprId );
    }

    void const GetPos( int& x, int& y )
    {
        SpriteInfo* si = GetSprInfo();
        x = (int) ( (float) ( ScrX - si->Width / 2 + si->OffsX + ( OffsX ? *OffsX : 0 ) + FOnline->ScrOx ) / FOnline->SpritesZoom );
        y = (int) ( (float) ( ScrY - si->Height    + si->OffsY + ( OffsY ? *OffsY : 0 ) + FOnline->ScrOy ) / FOnline->SpritesZoom );
    }
};

struct SpriteAnim
{
    // Data
    const uint  Ind[ MAX_FRAMES ];
    const int16 NextX[ MAX_FRAMES ];
    const int16 NextY[ MAX_FRAMES ];
    const uint  CntFrm;
    const uint  Ticks;
    const uint  Anim1;
    const uint  Anim2;
    uint GetCurSprIndex() { return CntFrm > 1 ? ( ( FOnline->GetTick() % Ticks ) * 100 / Ticks ) * CntFrm / 100 : 0; }
    uint GetCurSprId()    { return Ind[ GetCurSprIndex() ]; }

    // Dir animations
    const bool        HaveDirs;
    const SpriteAnim* Dirs[ 7 ];         // 7 additional for square hexes, 5 for hexagonal
    int               DirCount()        { return HaveDirs ? DIRS_COUNT : 1; }
    const SpriteAnim* GetDir( int dir ) { return dir == 0 || !HaveDirs ? this : Dirs[ dir - 1 ]; }
};

inline Field* GetField( uint hexX, uint hexY )
{
    if( !FOnline->ClientMap || hexX >= FOnline->ClientMapWidth || hexY >= FOnline->ClientMapHeight )
        return NULL;
    return &FOnline->ClientMap[ hexY * FOnline->ClientMapWidth + hexX ];
}

inline uint GetFieldLight( uint hexX, uint hexY )
{
    if( !FOnline->ClientMapLight || hexX >= FOnline->ClientMapWidth || hexY >= FOnline->ClientMapHeight )
        return 0;
    uint r = FOnline->ClientMapLight[ hexY * FOnline->ClientMapWidth * 3 + hexX * 3 + 0 ];
    uint g = FOnline->ClientMapLight[ hexY * FOnline->ClientMapWidth * 3 + hexX * 3 + 1 ];
    uint b = FOnline->ClientMapLight[ hexY * FOnline->ClientMapWidth * 3 + hexX * 3 + 2 ];
    uint rgb = ( r << 16 ) | ( g << 8 ) | ( b );
    return rgb;
}

inline int GetDirection( int x1, int y1, int x2, int y2 )
{
    if( FOnline->MapHexagonal )
    {
        float hx = (float) x1;
        float hy = (float) y1;
        float tx = (float) x2;
        float ty = (float) y2;
        float nx = 3 * ( tx - hx );
        float ny = ( ty - hy ) * SQRT3T2_FLOAT - ( float(x2 & 1) - float(x1 & 1) ) * SQRT3_FLOAT;
        float dir = 180.0f + RAD2DEG* atan2f( ny, nx );

        if( dir >= 60.0f  && dir < 120.0f )
            return 5;
        if( dir >= 120.0f && dir < 180.0f )
            return 4;
        if( dir >= 180.0f && dir < 240.0f )
            return 3;
        if( dir >= 240.0f && dir < 300.0f )
            return 2;
        if( dir >= 300.0f )
            return 1;
        return 0;
    }
    else
    {
        float dir = 180.0f + RAD2DEG* atan2( (float) ( x2 - x1 ), (float) ( y2 - y1 ) );

        if( dir >= 22.5f  && dir <  67.5f )
            return 7;
        if( dir >= 67.5f  && dir < 112.5f )
            return 0;
        if( dir >= 112.5f && dir < 157.5f )
            return 1;
        if( dir >= 157.5f && dir < 202.5f )
            return 2;
        if( dir >= 202.5f && dir < 247.5f )
            return 3;
        if( dir >= 247.5f && dir < 292.5f )
            return 4;
        if( dir >= 292.5f && dir < 337.5f )
            return 5;
        return 6;
    }
}

inline int GetDistantion( int x1, int y1, int x2, int y2 )
{
    if( FOnline->MapHexagonal )
    {
        int dx = ( x1 > x2 ? x1 - x2 : x2 - x1 );
        if( x1 % 2 == 0 )
        {
            if( y2 <= y1 )
            {
                int rx = y1 - y2 - dx / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
            else
            {
                int rx = y2 - y1 - ( dx + 1 ) / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
        }
        else
        {
            if( y2 >= y1 )
            {
                int rx = y2 - y1 - dx / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
            else
            {
                int rx = y1 - y2 - ( dx + 1 ) / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
        }
    }
    else
    {
        int dx = abs( x2 - x1 );
        int dy = abs( y2 - y1 );
        return std::max( dx, dy );
    }
}

inline void static_asserts()
{
    // Check the sizes of base types
    STATIC_ASSERT( sizeof( char ) == 1 );
    STATIC_ASSERT( sizeof( short ) == 2 );
    STATIC_ASSERT( sizeof( int ) == 4 );
    STATIC_ASSERT( sizeof( int64 ) == 8 );
    STATIC_ASSERT( sizeof( uint8 ) == 1 );
    STATIC_ASSERT( sizeof( uint16 ) == 2 );
    STATIC_ASSERT( sizeof( uint ) == 4 );
    STATIC_ASSERT( sizeof( uint64 ) == 8 );
    STATIC_ASSERT( sizeof( bool ) == 1 );
    #if defined ( FO_X86 )
    STATIC_ASSERT( sizeof( size_t ) == 4 );
    STATIC_ASSERT( sizeof( void* ) == 4 );
    #elif defined ( FO_X64 )
    STATIC_ASSERT( sizeof( size_t ) == 8 );
    STATIC_ASSERT( sizeof( void* ) == 8 );
    #endif
}

#endif     // __FONLINE__
