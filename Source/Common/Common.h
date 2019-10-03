#pragma once

// FOnline build target (passed outside)
#if !defined ( FONLINE_SERVER ) && !defined ( FONLINE_CLIENT ) && !defined ( FONLINE_EDITOR )
# error "Build target not specified"
#endif
#if ( defined ( FONLINE_SERVER ) && ( defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )  ) ) || \
    ( defined ( FONLINE_CLIENT ) && ( defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )  ) ) || \
    ( defined ( FONLINE_EDITOR ) && ( defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT )  ) )
# error "Multiple build targets not allowed"
#endif

// Operating system (passed outside)
// FO_WINDOWS
// FO_LINUX
// FO_MAC
// FO_IOS
// FO_ANDROID
// FO_WEB
#if !defined ( FO_WINDOWS ) && !defined ( FO_LINUX ) && !defined ( FO_MAC ) && \
    !defined ( FO_ANDROID ) && !defined ( FO_IOS ) && !defined ( FO_WEB )
# error "Unknown operating system"
#endif
#if ( defined ( FO_WINDOWS ) && ( defined ( FO_LINUX ) || defined ( FO_MAC ) || defined ( FO_ANDROID ) || defined ( FO_IOS ) || defined ( FO_WEB )  ) ) || \
    ( defined ( FO_LINUX ) && ( defined ( FO_WINDOWS ) || defined ( FO_MAC ) || defined ( FO_ANDROID ) || defined ( FO_IOS ) || defined ( FO_WEB )  ) ) || \
    ( defined ( FO_MAC ) && ( defined ( FO_LINUX ) || defined ( FO_WINDOWS ) || defined ( FO_ANDROID ) || defined ( FO_IOS ) || defined ( FO_WEB )  ) ) || \
    ( defined ( FO_ANDROID ) && ( defined ( FO_LINUX ) || defined ( FO_MAC ) || defined ( FO_WINDOWS ) || defined ( FO_IOS ) || defined ( FO_WEB )  ) ) || \
    ( defined ( FO_IOS ) && ( defined ( FO_LINUX ) || defined ( FO_MAC ) || defined ( FO_ANDROID ) || defined ( FO_WINDOWS ) || defined ( FO_WEB )  ) ) || \
    ( defined ( FO_WEB ) && ( defined ( FO_LINUX ) || defined ( FO_MAC ) || defined ( FO_ANDROID ) || defined ( FO_IOS ) || defined ( FO_WINDOWS )  ) )
# error "Multiple operating systems not allowed"
#endif

// Rendering
#if defined ( FO_IOS ) || defined ( FO_ANDROID ) || defined ( FO_WEB )
# define FO_OPENGL_ES
#endif
#if defined ( FO_WINDOWS )
# define FO_HAVE_DX
#endif

// Standard API
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <clocale>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <functional>
#include <sstream>
#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <deque>
#include <tuple>
#include <optional>
#include <typeinfo>
#include <typeindex>
#include <chrono>

#if defined ( FO_MAC ) || defined ( FO_IOS )
# include <TargetConditionals.h>
#endif

#if defined ( FO_WEB )
# include <emscripten.h>
# include <emscripten/html5.h>
#endif

#if !defined ( FO_WINDOWS )
# include <signal.h>
#endif

// String formatting
#include "fmt/format.h"

// Types
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using uint64 = uint64_t;
using int64 = int64_t;
using hash = uint;
using max_t = uint;

// Check the sizes of base types
static_assert( sizeof( char ) == 1 );
static_assert( sizeof( short ) == 2 );
static_assert( sizeof( int ) == 4 );
static_assert( sizeof( int64 ) == 8 );
static_assert( sizeof( uchar ) == 1 );
static_assert( sizeof( ushort ) == 2 );
static_assert( sizeof( uint ) == 4 );
static_assert( sizeof( uint64 ) == 8 );
static_assert( sizeof( bool ) == 1 );
static_assert( sizeof( void* ) >= 4 );
static_assert( sizeof( void* ) == sizeof( size_t ) );

using std::string;
using std::list;
using std::vector;
using std::map;
using std::unordered_map;
using std::multimap;
using std::set;
using std::deque;
using std::pair;
using std::tuple;
using std::istringstream;
using std::initializer_list;
using std::type_index;
using std::function;
using std::optional;

using StrUCharMap = map< string, uchar >;
using UCharStrMap = map< uchar, string >;
using StrMap = map< string, string >;
using UIntStrMap = map< uint, string >;
using StrUShortMap = map< string, ushort >;
using StrUIntMap = map< string, uint >;
using StrInt64Map = map< string, int64 >;
using StrPtrMap = map< string, void* >;
using UShortStrMap = map< ushort, string >;
using StrUIntMap = map< string, uint >;
using UIntMap = map< uint, uint >;
using IntMap = map< int, int >;
using IntFloatMap = map< int, float >;
using IntPtrMap = map< int, void* >;
using UIntFloatMap = map< uint, float >;
using UShortUIntMap = map< ushort, uint >;
using SizeTStrMap = map< size_t, string >;
using UIntIntMap = map< uint, int >;
using HashIntMap = map< hash, int >;
using HashUIntMap = map< hash, uint >;
using IntStrMap = map< int, string >;
using StrIntMap = map< string, int >;

using UIntStrMulMap = multimap< uint, string >;

using PtrVec = vector< void* >;
using IntVec = vector< int >;
using UCharVec = vector< uchar >;
using UCharVecVec = vector< UCharVec >;
using ShortVec = vector< short >;
using UShortVec = vector< ushort >;
using UIntVec = vector< uint >;
using UIntVecVec = vector< UIntVec >;
using CharVec = vector< char >;
using StrVec = vector< string >;
using PCharVec = vector< char* >;
using PUCharVec = vector< uchar* >;
using FloatVec = vector< float >;
using UInt64Vec = vector< uint64 >;
using BoolVec = vector< bool >;
using SizeVec = vector< size_t >;
using HashVec = vector< hash >;
using HashVecVec = vector< HashVec >;
using MaxTVec = vector< max_t >;
using PStrMapVec = vector< StrMap* >;

using StrSet = set< string >;
using UCharSet = set< uchar >;
using UShortSet = set< ushort >;
using UIntSet = set< uint >;
using IntSet = set< int >;
using HashSet = set< hash >;

using IntPair = pair< int, int >;
using UShortPair = pair< ushort, ushort >;
using UIntPair = pair< uint, uint >;
using CharPair = pair< char, char >;
using PCharPair = pair< char*, char* >;
using UCharPair = pair< uchar, uchar >;

using UShortPairVec = vector< UShortPair >;
using IntPairVec = vector< IntPair >;
using UIntPairVec = vector< UIntPair >;
using PCharPairVec = vector< PCharPair >;
using UCharPairVec = vector< UCharPair >;

using UIntUIntPairMap = map< uint, UIntPair >;
using UIntIntPairVecMap = map< uint, IntPairVec >;
using UIntHashVecMap = map< uint, HashVec >;

// DLL
#ifdef FO_WINDOWS
# define DLL_Load( name )                     (void*) LoadLibraryA( fmt::format( "{}", name ).c_str() )
# define DLL_Free( h )                        FreeLibrary( (HMODULE) h )
# define DLL_GetAddress( h, name )            (size_t*) GetProcAddress( (HMODULE) h, fmt::format( "{}", name ).c_str() )
# define DLL_Error()                          fmt::format( "{}", GetLastError() )
#else
# include <dlfcn.h>
# define DLL_Load( name )                     (void*) dlopen( fmt::format( "{}", name ).c_str(), RTLD_NOW | RTLD_LOCAL )
# define DLL_Free( h )                        dlclose( h )
# define DLL_GetAddress( h, name )            (size_t*) dlsym( h, fmt::format( "{}", name ).c_str() )
# define DLL_Error()                          fmt::format( "{}", dlerror() )
#endif

// Network
#ifdef FO_WINDOWS
# include <WinSock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <netdb.h>
# define SOCKET                      int
# define INVALID_SOCKET              ( -1 )
# define SOCKET_ERROR                ( -1 )
# define closesocket                 close
# define SD_RECEIVE                  SHUT_RD
# define SD_SEND                     SHUT_WR
# define SD_BOTH                     SHUT_RDWR
#endif

// Generic exception
class fo_exception: public std::exception
{
    string exceptionMessage;

public:
    fo_exception( const char* message ): exceptionMessage( message ) {}
    template< typename ... Args > fo_exception( const char* format, Args ... args ): exceptionMessage( std::move( fmt::format( format, args ... ) ) ) {}
    ~fo_exception() noexcept = default;
    const char* what() const noexcept override { return exceptionMessage.c_str(); }
};

// Generic helpers
#define OFFSETOF( s, m )                      ( (int) (size_t) ( &reinterpret_cast< s* >( 100000 )->m ) - 100000 )
#define UNUSED_VARIABLE( x )                  (void) ( x )
#define memzero( ptr, size )                  memset( ptr, 0, size )
#define UNIQUE_FUNCTION_NAME( name, ... )     UNIQUE_FUNCTION_NAME2( MERGE_ARGS( name, __COUNTER__ ), __VA_ARGS__ )
#define UNIQUE_FUNCTION_NAME2( name, ... )    name( __VA_ARGS__ )
#define STRINGIZE_INT( x )                    STRINGIZE_INT2( x )
#define STRINGIZE_INT2( x )                   # x
#define MERGE_ARGS( a, b )                    MERGE_ARGS2( a, b )
#define MERGE_ARGS2( a, b )                   a ## b
#define MESSAGE( desc )                       message( __FILE__ "(" STRINGIZE_INT( __LINE__ ) "):" # desc )

#define SAFEREL( x ) \
    { if( x )        \
          ( x )->Release(); ( x ) = nullptr; }
#define SAFEDEL( x ) \
    { if( x )        \
          delete ( x ); ( x ) = nullptr; }
#define SAFEDELA( x ) \
    { if( x )         \
          delete[] ( x ); ( x ) = nullptr; }

#define MAX( a, b )                           ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )                           ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define PACKUINT64( u32hi, u32lo )            ( ( (uint64) u32hi << 32 ) | ( (uint64) u32lo ) )
#define MAKEUINT( ch0, ch1, ch2, ch3 )        ( (uint) (uchar) ( ch0 ) | ( (uint) (uchar) ( ch1 ) << 8 ) | ( (uint) (uchar) ( ch2 ) << 16 ) | ( (uint) (uchar) ( ch3 ) << 24 ) )

// Floats
#define PI_VALUE                     ( 3.14159265f )
#define PI_FLOAT                     ( 3.14159265f )
#define SQRT3T2_FLOAT                ( 3.4641016151f )
#define SQRT3_FLOAT                  ( 1.732050807568877f )
#define BIAS_FLOAT                   ( 0.02f )
#define RAD2DEG                      ( 57.29577951f )
#define RAD( deg )                            ( ( deg ) * 3.141592654f / 180.0f )

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

// Other stuff
#define CLAMP( x, low, high )                 ( ( ( x ) > ( high ) ) ? ( high ) : ( ( ( x ) < ( low ) ) ? ( low ) : ( x ) ) )
#define MAX_INT                      ( 0x7FFFFFFF )

// Generic
#define CLIENT_DATA                  "./Data/"
#define WORLD_START_TIME             "07:00 30:10:2246 x00"
#define TEMP_BUF_SIZE                ( 8192 )
#define MAX_FOPATH                   UTF8_BUF_SIZE( 2048 )
#define MAX_HOLO_INFO                ( 250 )
#define EFFECT_SCRIPT_VALUES         ( 10 )
#define ABC_SIZE                     ( 26 )
#define DIRS_COUNT                   ( GameOpt.MapHexagonal ? 6 : 8 )
#define IS_DIR_CORNER( dir )                  ( ( ( dir ) & 1 ) != 0 )         // 1, 3, 5, 7
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
#define AGGRESSOR_TICK               ( 60000 )
#define MAX_ENEMY_STACK              ( 30 )

// Critter find types
#define FIND_LIFE                    ( 0x01 )
#define FIND_KO                      ( 0x02 )
#define FIND_DEAD                    ( 0x04 )
#define FIND_ONLY_PLAYERS            ( 0x10 )
#define FIND_ONLY_NPC                ( 0x20 )
#define FIND_ALL                     ( 0x0F )

// Ping
#define PING_PING                    ( 0 )
#define PING_WAIT                    ( 1 )
#define PING_CLIENT                  ( 2 )

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
#define SAY_FLASH_WINDOW             ( 41 )

// Target types
#define TARGET_SELF                  ( 0 )
#define TARGET_SELF_ITEM             ( 1 )
#define TARGET_CRITTER               ( 2 )
#define TARGET_ITEM                  ( 3 )

// Critters
#define CRITTER_INV_VOLUME           ( 1000 )

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
#define GM_ANSWER_WAIT_TIME          ( 20000 )
#define GM_LIGHT_TIME                ( 5000 )
#define GM_ZONE( x )                          ( ( x ) / GM_ZONE_LEN )
#define GM_ENTRANCES_SEND_TIME       ( 60000 )
#define GM_TRACE_TIME                ( 1000 )

// GM Info
#define GM_INFO_LOCATIONS            ( 0x01 )
#define GM_INFO_CRITTERS             ( 0x02 )
#define GM_INFO_ZONES_FOG            ( 0x08 )
#define GM_INFO_ALL                  ( 0x0F )
#define GM_INFO_FOG                  ( 0x10 )
#define GM_INFO_LOCATION             ( 0x20 )

// Flags Hex
// Proto map
#define FH_BLOCK                     BIN8( 00000001 )
#define FH_NOTRAKE                   BIN8( 00000010 )
#define FH_STATIC_TRIGGER            BIN8( 00100000 )
// Map copy
#define FH_CRITTER                   BIN8( 00000001 )
#define FH_DEAD_CRITTER              BIN8( 00000010 )
#define FH_DOOR                      BIN8( 00001000 )
#define FH_BLOCK_ITEM                BIN8( 00010000 )
#define FH_NRAKE_ITEM                BIN8( 00100000 )
#define FH_TRIGGER                   BIN8( 01000000 )
#define FH_GAG_ITEM                  BIN8( 10000000 )

#define FH_NOWAY                     BIN16( 00010001, 00000001 )
#define FH_NOSHOOT                   BIN16( 00100000, 00000010 )

// Client map
#define CLIENT_MAP_FORMAT_VER        ( 10 )

// Coordinates
#define MAXHEX_DEF                   ( 200 )
#define MAXHEX_MIN                   ( 10 )
#define MAXHEX_MAX                   ( 4000 )

// Client parameters
#define MAX_NAME                     ( 30 )
#define MIN_NAME                     ( 1 )
#define MAX_CHAT_MESSAGE             ( 100 )
#define MAX_SCENERY                  ( 5000 )
#define MAX_DIALOG_TEXT              ( MAX_FOTEXT )
#define MAX_DLG_LEN_IN_BYTES         ( 64 * 1024 )
#define MAX_DLG_LEXEMS_TEXT          ( 1000 )
#define MAX_BUF_LEN                  ( 4096 )
#define PASS_HASH_SIZE               ( 32 )
#define FILE_UPDATE_PORTION          ( 16384 )

// Critters
#define MAKE_CLIENT_ID( name )                ( ( 1 << 31 ) | _str( name ).toHash() )
#define IS_CLIENT_ID( id )                    ( ( ( id ) >> 31 ) != 0 )
#define MAX_ANSWERS                  ( 100 )
#define PROCESS_TALK_TICK            ( 1000 )
#define TURN_BASED_TIMEOUT           ( 1000 )
#define FADING_PERIOD                ( 1000 )

#define RESPOWN_TIME_PLAYER          ( 3 )
#define RESPOWN_TIME_NPC             ( 120 )
#define RESPOWN_TIME_INFINITY        ( 4 * 24 * 60 * 60000 )

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
#define FCRIT_PLAYER                 ( 0x00010000 )
#define FCRIT_NPC                    ( 0x00020000 )
#define FCRIT_DISCONNECT             ( 0x00080000 )
#define FCRIT_CHOSEN                 ( 0x00100000 )

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

// Special send params
#define OTHER_BREAK_TIME             ( 0 )
#define OTHER_WAIT_TIME              ( 1 )
#define OTHER_FLAGS                  ( 2 )
#define OTHER_CLEAR_MAP              ( 6 )
#define OTHER_TELEPORT               ( 7 )

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//                                          flags    actionExt                                                      item
#define ACTION_MOVE_ITEM             ( 2 )  // l s      from slot                                                      +
#define ACTION_MOVE_ITEM_SWAP        ( 3 )  // l s      from slot                                                      +
#define ACTION_DROP_ITEM             ( 5 )  // l s      from slot                                                      +
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
#define LOOK_CHECK_TRACE             ( 0x08 )
#define LOOK_CHECK_SCRIPT            ( 0x10 )
#define LOOK_CHECK_ITEM_SCRIPT       ( 0x20 )

// In SendMessage
#define MESSAGE_TO_VISIBLE_ME        ( 0 )
#define MESSAGE_TO_IAM_VISIBLE       ( 1 )
#define MESSAGE_TO_ALL_ON_MAP        ( 2 )

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

// Corner type
#define CORNER_NORTH_SOUTH           ( 0 )
#define CORNER_WEST                  ( 1 )
#define CORNER_EAST                  ( 2 )
#define CORNER_SOUTH                 ( 3 )
#define CORNER_NORTH                 ( 4 )
#define CORNER_EAST_WEST             ( 5 )

// Items accessory
#define ITEM_ACCESSORY_NONE          ( 0 )
#define ITEM_ACCESSORY_CRITTER       ( 1 )
#define ITEM_ACCESSORY_HEX           ( 2 )
#define ITEM_ACCESSORY_CONTAINER     ( 3 )

// Generic
#define MAX_ADDED_NOGROUP_ITEMS      ( 30 )

// Uses
#define USE_PRIMARY                  ( 0 )
#define USE_SECONDARY                ( 1 )
#define USE_THIRD                    ( 2 )
#define USE_RELOAD                   ( 3 )
#define USE_USE                      ( 4 )
#define MAX_USES                     ( 3 )
#define USE_NONE                     ( 15 )
#define MAKE_ITEM_MODE( use, aim )            ( ( ( ( aim ) << 4 ) | ( ( use ) & 0xF ) ) & 0xFF )

// Radio
// Flags
#define RADIO_DISABLE_SEND           ( 0x01 )
#define RADIO_DISABLE_RECV           ( 0x02 )
// Broadcast
#define RADIO_BROADCAST_WORLD        ( 0 )
#define RADIO_BROADCAST_MAP          ( 20 )
#define RADIO_BROADCAST_LOCATION     ( 40 )
#define RADIO_BROADCAST_ZONE( x )             ( 100 + CLAMP( x, 1, 100 ) )         // 1..100
#define RADIO_BROADCAST_FORCE_ALL    ( 250 )

// Light flags
#define LIGHT_DISABLE_DIR( dir )              ( 1 << CLAMP( dir, 0, 5 ) )
#define LIGHT_GLOBAL                 ( 0x40 )
#define LIGHT_INVERSE                ( 0x80 )

// Access
#define ACCESS_CLIENT                ( 0 )
#define ACCESS_TESTER                ( 1 )
#define ACCESS_MODER                 ( 2 )
#define ACCESS_ADMIN                 ( 3 )
#define ACCESS_DEFAULT               ACCESS_CLIENT

// Commands
#define CMD_EXIT                     ( 1 )
#define CMD_MYINFO                   ( 2 )
#define CMD_GAMEINFO                 ( 3 )
#define CMD_CRITID                   ( 4 )
#define CMD_MOVECRIT                 ( 5 )
#define CMD_DISCONCRIT               ( 7 )
#define CMD_TOGLOBAL                 ( 8 )
#define CMD_PROPERTY                 ( 10 )
#define CMD_GETACCESS                ( 11 )
#define CMD_ADDITEM                  ( 12 )
#define CMD_ADDITEM_SELF             ( 14 )
#define CMD_ADDNPC                   ( 15 )
#define CMD_ADDLOCATION              ( 16 )
#define CMD_RELOADSCRIPTS            ( 17 )
#define CMD_RELOAD_CLIENT_SCRIPTS    ( 19 )
#define CMD_RUNSCRIPT                ( 20 )
#define CMD_RELOAD_PROTOS            ( 21 )
#define CMD_REGENMAP                 ( 25 )
#define CMD_RELOADDIALOGS            ( 26 )
#define CMD_LOADDIALOG               ( 27 )
#define CMD_RELOADTEXTS              ( 28 )
#define CMD_SETTIME                  ( 32 )
#define CMD_BAN                      ( 33 )
#define CMD_DELETE_ACCOUNT           ( 34 )
#define CMD_CHANGE_PASSWORD          ( 35 )
#define CMD_LOG                      ( 37 )
#define CMD_DEV_EXEC                 ( 38 )
#define CMD_DEV_FUNC                 ( 39 )
#define CMD_DEV_GVAR                 ( 40 )

// Lines foreach helper
#define FOREACH_PROTO_ITEM_LINES( lines, hx, hy, maxhx, maxhy, work )        \
    int hx__ = hx, hy__ = hy;                                                \
    int maxhx__ = maxhx, maxhy__ = maxhy;                                    \
    for( uint i__ = 0, j__ = ( lines )->GetSize() / 2; i__ < j__; i__++ )    \
    {                                                                        \
        uchar dir__ = *(uchar*) ( lines )->At( i__ * 2 );                    \
        uchar steps__ = *(uchar*) ( lines )->At( i__ * 2 + 1 );              \
        if( dir__ >= DIRS_COUNT || !steps__ || steps__ > 9 )                 \
            break;                                                           \
        for( uchar k__ = 0; k__ < steps__; k__++ )                           \
        {                                                                    \
            MoveHexByDirUnsafe( hx__, hy__, dir__ );                         \
            if( hx__ < 0 || hy__ < 0 || hx__ >= maxhx__ || hy__ >= maxhy__ ) \
                continue;                                                    \
            hx = hx__, hy = hy__;                                            \
            work                                                             \
        }                                                                    \
    }

extern void InitialSetup( const string& app_name, uint argc, char** argv );
extern int  Random( int minimum, int maximum );
extern int  Procent( int full, int peace );
extern uint NumericalNumber( uint num );
extern uint DistSqrt( int x1, int y1, int x2, int y2 );
extern uint DistGame( int x1, int y1, int x2, int y2 );
extern int  GetNearDir( int x1, int y1, int x2, int y2 );
extern int  GetFarDir( int x1, int y1, int x2, int y2 );
extern int  GetFarDir( int x1, int y1, int x2, int y2, float offset );
extern bool CheckDist( ushort x1, ushort y1, ushort x2, ushort y2, uint dist );
extern int  ReverseDir( int dir );
extern void GetStepsXY( float& sx, float& sy, int x1, int y1, int x2, int y2 );
extern void ChangeStepsXY( float& sx, float& sy, float deq );
extern bool MoveHexByDir( ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy );
extern void MoveHexByDirUnsafe( int& hx, int& hy, uchar dir );
extern bool IntersectCircleLine( int cx, int cy, int radius, int x1, int y1, int x2, int y2 );
extern void ShowErrorMessage( const string& message, const string& traceback );
extern int  ConvertParamValue( const string& str, bool& fail );

// Hex offsets
#define MAX_HEX_OFFSET               ( 50 )              // Must be not odd
extern void GetHexOffsets( bool odd, short*& sx, short*& sy );
extern void GetHexInterval( int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y );

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
extern IntVec MainWindowKeyboardEvents;
extern StrVec MainWindowKeyboardEventsText;
extern IntVec MainWindowMouseEvents;
extern uint GetColorDay( int* day_time, uchar* colors, int game_time, int* light );
#endif

// Zoom
#define MIN_ZOOM                     ( 0.1f )
#define MAX_ZOOM                     ( 20.0f )

// Memory pool
template< int StructSize, int PoolSize >
class MemoryPool
{
private:
    vector< char* > allocatedData;

    void Grow()
    {
        allocatedData.reserve( allocatedData.size() + PoolSize );
        for( int i = 0; i < PoolSize; i++ )
            allocatedData.push_back( new char[ StructSize ] );
    }

public:
    MemoryPool()
    {
        Grow();
    }

    ~MemoryPool()
    {
        for( auto it = allocatedData.begin(); it != allocatedData.end(); ++it )
            delete[] *it;
        allocatedData.clear();
    }

    void* Get()
    {
        if( allocatedData.empty() )
            Grow();
        void* result = allocatedData.back();
        allocatedData.pop_back();
        return result;
    }

    void Put( void* t )
    {
        allocatedData.push_back( (char*) t );
    }
};

// Data serialization helpers
template< class T >
inline void WriteData( UCharVec& vec, T data )
{
    size_t cur = vec.size();
    vec.resize( cur + sizeof( data ) );
    memcpy( &vec[ cur ], &data, sizeof( data ) );
}

template< class T >
inline void WriteDataArr( UCharVec& vec, T* data, uint size )
{
    if( size )
    {
        uint cur = (uint) vec.size();
        vec.resize( cur + size );
        memcpy( &vec[ cur ], data, size );
    }
}

template< class T >
inline T ReadData( UCharVec& vec, uint& pos )
{
    T data;
    memcpy( &data, &vec[ pos ], sizeof( T ) );
    pos += sizeof( T );
    return data;
}

template< class T >
inline T* ReadDataArr( UCharVec& vec, uint size, uint& pos )
{
    pos += size;
    return size ? &vec[ pos - size ] : nullptr;
}

// Two bit mask
class TwoBitMask
{
public:
    TwoBitMask();
    TwoBitMask( uint width_2bit, uint height_2bit, uchar* ptr );
    ~TwoBitMask();
    void   Set2Bit( uint x, uint y, int val );
    int    Get2Bit( uint x, uint y );
    void   Fill( int fill );
    uchar* GetData();

private:
    bool   isAlloc;
    uchar* data;
    uint   width;
    uint   height;
    uint   widthBytes;
};

// Flex rect
template< typename Ty >
struct FlexRect
{
    Ty L, T, R, B;

    FlexRect(): L( 0 ), T( 0 ), R( 0 ), B( 0 ) {}
    template< typename Ty2 > FlexRect( const FlexRect< Ty2 >& fr ): L( ( Ty )fr.L ), T( ( Ty )fr.T ), R( ( Ty )fr.R ), B( ( Ty )fr.B ) {}
    FlexRect( Ty l, Ty t, Ty r, Ty b ): L( l ), T( t ), R( r ), B( b ) {}
    FlexRect( Ty l, Ty t, Ty r, Ty b, Ty ox, Ty oy ): L( l + ox ), T( t + oy ), R( r + ox ), B( b + oy ) {}
    FlexRect( const FlexRect& fr, Ty ox, Ty oy ): L( fr.L + ox ), T( fr.T + oy ), R( fr.R + ox ), B( fr.B + oy ) {}

    template< typename Ty2 >
    FlexRect& operator=( const FlexRect< Ty2 >& fr )
    {
        L = (Ty) fr.L;
        T = (Ty) fr.T;
        R = (Ty) fr.R;
        B = (Ty) fr.B;
        return *this;
    }

    void Clear()
    {
        L = 0;
        T = 0;
        R = 0;
        B = 0;
    }

    bool IsZero() const { return !L && !T && !R && !B; }
    Ty   W()      const { return R - L + 1; }
    Ty   H()      const { return B - T + 1; }
    Ty   CX()     const { return L + W() / 2; }
    Ty   CY()     const { return T + H() / 2; }

    Ty& operator[]( int index )
    {
        switch( index )
        {
        case 0:
            return L;
        case 1:
            return T;
        case 2:
            return R;
        case 3:
            return B;
        default:
            break;
        }
        return L;
    }

    FlexRect& operator()( Ty l, Ty t, Ty r, Ty b )
    {
        L = l;
        T = t;
        R = r;
        B = b;
        return *this;
    }

    FlexRect& operator()( Ty ox, Ty oy )
    {
        L += ox;
        T += oy;
        R += ox;
        B += oy;
        return *this;
    }

    FlexRect< Ty > Interpolate( const FlexRect< Ty >& to, int procent )
    {
        FlexRect< Ty > result( L, T, R, B );
        result.L += (Ty) ( (int) ( to.L - L ) * procent / 100 );
        result.T += (Ty) ( (int) ( to.T - T ) * procent / 100 );
        result.R += (Ty) ( (int) ( to.R - R ) * procent / 100 );
        result.B += (Ty) ( (int) ( to.B - B ) * procent / 100 );
        return result;
    }
};
using Rect = FlexRect< int >;
using RectF = FlexRect< float >;
using IntRectVec = std::vector< Rect >;
using FltRectVec = std::vector< RectF >;

template< typename Ty >
struct FlexPoint
{
    Ty X, Y;

    FlexPoint(): X( 0 ), Y( 0 ) {}
    template< typename Ty2 > FlexPoint( const FlexPoint< Ty2 >& r ): X( ( Ty )r.X ), Y( ( Ty )r.Y ) {}
    FlexPoint( Ty x, Ty y ): X( x ), Y( y ) {}
    FlexPoint( const FlexPoint& fp, Ty ox, Ty oy ): X( fp.X + ox ), Y( fp.Y + oy ) {}

    template< typename Ty2 >
    FlexPoint& operator=( const FlexPoint< Ty2 >& fp )
    {
        X = (Ty) fp.X;
        Y = (Ty) fp.Y;
        return *this;
    }

    void Clear()
    {
        X = 0;
        Y = 0;
    }

    bool IsZero()
    {
        return !X && !Y;
    }

    Ty& operator[]( int index )
    {
        switch( index )
        {
        case 0:
            return X;
        case 1:
            return Y;
        default:
            break;
        }
        return X;
    }

    FlexPoint& operator()( Ty x, Ty y )
    {
        X = x;
        Y = y;
        return *this;
    }
};
using Point = FlexPoint< int >;
using PointF = FlexPoint< float >;

// Net command helper
class NetBuffer;
extern bool PackNetCommand( const string& str, NetBuffer* pbuf, std::function< void(const string&) > logcb, const string& name );
