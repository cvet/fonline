#pragma once

// Versions
#define FONLINE_VERSION    ( 840 )

// Debugging
// #define DEV_VERSION
// #define SHOW_DEPRECTAED // All known places with deprecated stuff
// #define SHOW_ANDROID_TODO
// #define DISABLE_EGG

// Some platform specific definitions
#include "PlatformSpecific.h"

// Standard API
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include <functional>
#include <math.h>
#ifndef FO_WINDOWS
# include <errno.h>
# include <string.h> // strerror
# include <unistd.h>
# define ERRORSTR          strerror( errno )
# define ExitProcess( code )              exit( code )
#endif

// String formatting
#include "fmt/fmt/format.h"

// Network
#ifdef FO_WINDOWS
# include <winsock2.h>
# if defined ( FO_MSVC )
#  pragma comment( lib, "Ws2_32.lib" )
# endif
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <netdb.h>
# define SOCKET            int
# define INVALID_SOCKET    ( -1 )
# define SOCKET_ERROR      ( -1 )
# define closesocket       close
# define SD_RECEIVE        SHUT_RD
# define SD_SEND           SHUT_WR
# define SD_BOTH           SHUT_RDWR
#endif

// DLL
#ifdef FO_WINDOWS
# define DLL_Load( name )                 (void*) LoadLibraryW( _str( name ).toWideChar().c_str() )
# define DLL_Free( h )                    FreeLibrary( (HMODULE) h )
# define DLL_GetAddress( h, pname )       (size_t*) GetProcAddress( (HMODULE) h, _str( pname ).c_str() )
# define DLL_Error()                      _str( "{}", GetLastError() )
#else
# include <dlfcn.h>
# define DLL_Load( name )                 (void*) dlopen( _str( name ).c_str(), RTLD_NOW | RTLD_LOCAL )
# define DLL_Free( h )                    dlclose( h )
# define DLL_GetAddress( h, pname )       (size_t*) dlsym( h, _str( pname ).c_str() )
# define DLL_Error()                      dlerror()
#endif

// Generic helpers
#define RUNTIME_ASSERT( a )               ( !!( a ) || RaiseAssert( # a, __FILE__, __LINE__ ) )
#define RUNTIME_ASSERT_STR( a, str )      ( !!( a ) || RaiseAssert( str, __FILE__, __LINE__ ) )
#define STATIC_ASSERT( a )                static_assert( a, # a )
#define OFFSETOF( s, m )                  ( (int) (size_t) ( &reinterpret_cast< s* >( 100000 )->m ) - 100000 )
#define UNUSED_VARIABLE( x )              (void) ( x )
#define memzero( ptr, size )              memset( ptr, 0, size )
#define CLEAN_CONTAINER( cont )           { decltype( cont ) __ ## cont; __ ## cont.swap( cont ); }
#define PI_VALUE           ( 3.141592654f )

// FOnline stuff
#include "Types.h"
#include "Defines.h"
#include "Log.h"
#include "Timer.h"
#include "Debugger.h"
#include "Exception.h"
#include "Text.h"
#include "IniParser.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "AngelScriptExt/scriptdict.h"

#define ___MSG1( x )                      # x
#define ___MSG0( x )                      ___MSG1( x )
#define MESSAGE( desc )                   message( __FILE__ "(" ___MSG0( __LINE__ ) "):" # desc )

#define SAFEREL( x ) \
    { if( x )        \
          ( x )->Release(); ( x ) = nullptr; }
#define SAFEDEL( x ) \
    { if( x )        \
          delete ( x ); ( x ) = nullptr; }
#define SAFEDELA( x ) \
    { if( x )         \
          delete[] ( x ); ( x ) = nullptr; }

#define MAX( a, b )                       ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )                       ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

#define PACKUINT64( u32hi, u32lo )        ( ( (uint64) u32hi << 32 ) | ( (uint64) u32lo ) )
#define MAKEUINT( ch0, ch1, ch2, ch3 )    ( (uint) (uchar) ( ch0 ) | ( (uint) (uchar) ( ch1 ) << 8 ) | ( (uint) (uchar) ( ch2 ) << 16 ) | ( (uint) (uchar) ( ch3 ) << 24 ) )

#ifdef SHOW_DEPRECATED
# define DEPRECATED        MESSAGE( "Deprecated" )
#else
# define DEPRECATED
#endif

#ifdef SHOW_ANDROID_TODO
# define ANDROID_TODO      MESSAGE( "Android todo" )
#else
# define ANDROID_TODO
#endif

extern IniParser* MainConfig;
extern StrVec     ProjectFiles;
void InitialSetup( const string& app_name, uint argc, char** argv );

int  Random( int minimum, int maximum );
int  Procent( int full, int peace );
uint NumericalNumber( uint num );
uint DistSqrt( int x1, int y1, int x2, int y2 );
uint DistGame( int x1, int y1, int x2, int y2 );
int  GetNearDir( int x1, int y1, int x2, int y2 );
int  GetFarDir( int x1, int y1, int x2, int y2 );
int  GetFarDir( int x1, int y1, int x2, int y2, float offset );
bool CheckDist( ushort x1, ushort y1, ushort x2, ushort y2, uint dist );
int  ReverseDir( int dir );
void GetStepsXY( float& sx, float& sy, int x1, int y1, int x2, int y2 );
void ChangeStepsXY( float& sx, float& sy, float deq );
bool MoveHexByDir( ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy );
void MoveHexByDirUnsafe( int& hx, int& hy, uchar dir );
bool IntersectCircleLine( int cx, int cy, int radius, int x1, int y1, int x2, int y2 );
void ShowMessage( const string& message );
int  ConvertParamValue( const string& str, bool& fail );

// Hex offsets
#define MAX_HEX_OFFSET     ( 50 )                        // Must be not odd
void GetHexOffsets( bool odd, short*& sx, short*& sy );
void GetHexInterval( int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y );

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
extern IntVec MainWindowKeyboardEvents;
extern StrVec MainWindowKeyboardEventsText;
extern IntVec MainWindowMouseEvents;

const uchar   SELECT_ALPHA    = 100;

uint GetColorDay( int* day_time, uchar* colors, int game_time, int* light );
void GetClientOptions();

struct ClientScriptFunctions
{
    void* Start;
    void* Finish;
    void* Loop;
    void* AutoLogin;
    void* GetActiveScreens;
    void* ScreenChange;
    void* ScreenScroll;
    void* RenderIface;
    void* RenderMap;
    void* MouseDown;
    void* MouseUp;
    void* MouseMove;
    void* KeyDown;
    void* KeyUp;
    void* InputLost;
    void* CritterIn;
    void* CritterOut;
    void* ItemMapIn;
    void* ItemMapChanged;
    void* ItemMapOut;
    void* ItemInvAllIn;
    void* ItemInvIn;
    void* ItemInvChanged;
    void* ItemInvOut;
    void* MapLoad;
    void* MapUnload;
    void* ReceiveItems;
    void* MapMessage;
    void* InMessage;
    void* OutMessage;
    void* MessageBox;
    void* CombatResult;
    void* ItemCheckMove;
    void* CritterAction;
    void* Animation2dProcess;
    void* Animation3dProcess;
    void* CritterAnimation;
    void* CritterAnimationSubstitute;
    void* CritterAnimationFallout;
    void* CritterCheckMoveItem;
    void* CritterGetAttackDistantion;
} extern ClientFunctions;

struct MapperScriptFunctions
{
    void* Start;
    void* Finish;
    void* Loop;
    void* ConsoleMessage;
    void* RenderIface;
    void* MouseDown;
    void* MouseUp;
    void* MouseMove;
    void* KeyDown;
    void* KeyUp;
    void* InputLost;
    void* MapLoad;
    void* MapSave;
    void* InspectorProperties;
} extern MapperFunctions;
#endif

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
extern int  ServerGameSleep;
extern bool AllowServerNativeCalls;
extern bool AllowClientNativeCalls;

void GetServerOptions();

struct ServerScriptFunctions
{
    void* ResourcesGenerated;
    void* Init;
    void* GenerateWorld;
    void* Start;
    void* Finish;
    void* Loop;
    void* GlobalMapCritterIn;
    void* GlobalMapCritterOut;

    void* LocationInit;
    void* LocationFinish;

    void* MapInit;
    void* MapFinish;
    void* MapLoop;
    void* MapCritterIn;
    void* MapCritterOut;
    void* MapCheckLook;
    void* MapCheckTrapLook;

    void* CritterInit;
    void* CritterFinish;
    void* CritterIdle;
    void* CritterGlobalMapIdle;
    void* CritterCheckMoveItem;
    void* CritterMoveItem;
    void* CritterShow;
    void* CritterShowDist1;
    void* CritterShowDist2;
    void* CritterShowDist3;
    void* CritterHide;
    void* CritterHideDist1;
    void* CritterHideDist2;
    void* CritterHideDist3;
    void* CritterShowItemOnMap;
    void* CritterHideItemOnMap;
    void* CritterChangeItemOnMap;
    void* CritterMessage;
    void* CritterTalk;
    void* CritterBarter;
    void* CritterGetAttackDistantion;
    void* PlayerRegistration;
    void* PlayerLogin;
    void* PlayerGetAccess;
    void* PlayerAllowCommand;
    void* PlayerLogout;

    void* ItemInit;
    void* ItemFinish;
    void* ItemWalk;
    void* ItemCheckMove;

    void* StaticItemWalk;
} extern ServerFunctions;
#endif

struct GameOptions
{
    string WorkDir;
    string CommandLine;

    uint   YearStartFTLo;
    uint   YearStartFTHi;
    uint   FullSecondStart;
    uint   FullSecond;
    uint   GameTimeTick;

    bool   DisableTcpNagle;
    bool   DisableZlibCompression;
    uint   FloodSize;
    bool   NoAnswerShuffle;
    bool   DialogDemandRecheck;
    uint   SneakDivider;
    uint   LookMinimum;
    int    DeadHitPoints;
    uint   Breaktime;
    uint   TimeoutTransfer;
    uint   TimeoutBattle;
    bool   RunOnCombat;
    bool   RunOnTransfer;
    uint   GlobalMapWidth;
    uint   GlobalMapHeight;
    uint   GlobalMapZoneLength;
    uint   BagRefreshTime;
    uint   WhisperDist;
    uint   ShoutDist;
    int    LookChecks;
    uint   LookDir[ 5 ];
    uint   LookSneakDir[ 5 ];
    uint   RegistrationTimeout;
    uint   AccountPlayTime;
    uint   ScriptRunSuspendTimeout;
    uint   ScriptRunMessageTimeout;
    uint   TalkDistance;
    uint   NpcMaxTalkers;
    uint   MinNameLength;
    uint   MaxNameLength;
    uint   DlgTalkMinTime;
    uint   DlgBarterMinTime;
    uint   MinimumOfflineTime;
    bool   ForceRebuildResources;

    bool   MapHexagonal;
    int    MapHexWidth;
    int    MapHexHeight;
    int    MapHexLineHeight;
    int    MapTileOffsX;
    int    MapTileOffsY;
    int    MapTileStep;
    int    MapRoofOffsX;
    int    MapRoofOffsY;
    int    MapRoofSkipSize;
    float  MapCameraAngle;
    bool   MapSmoothPath;
    string MapDataPrefix;

    bool   DesktopBuild;
    bool   TabletBuild;
    bool   WebBuild;
    bool   WindowsBuild;
    bool   LinuxBuild;
    bool   MacOsBuild;
    bool   AndroidBuild;
    bool   IOsBuild;

    // Client and Mapper
    bool   Quit;
    int    WaitPing;
    bool   OpenGLRendering;
    bool   OpenGLDebug;
    bool   AssimpLogging;
    int    MouseX;
    int    MouseY;
    int    LastMouseX;
    int    LastMouseY;
    int    ScrOx;
    int    ScrOy;
    bool   ShowTile;
    bool   ShowRoof;
    bool   ShowItem;
    bool   ShowScen;
    bool   ShowWall;
    bool   ShowCrit;
    bool   ShowFast;
    bool   ShowPlayerNames;
    bool   ShowNpcNames;
    bool   ShowCritId;
    bool   ScrollKeybLeft;
    bool   ScrollKeybRight;
    bool   ScrollKeybUp;
    bool   ScrollKeybDown;
    bool   ScrollMouseLeft;
    bool   ScrollMouseRight;
    bool   ScrollMouseUp;
    bool   ScrollMouseDown;
    bool   ShowGroups;
    bool   HelpInfo;
    bool   DebugInfo;
    bool   DebugNet;
    bool   Enable3dRendering;
    bool   FullScreen;
    bool   VSync;
    int    Light;
    string Host;
    uint   Port;
    uint   ProxyType;
    string ProxyHost;
    uint   ProxyPort;
    string ProxyUser;
    string ProxyPass;
    uint   ScrollDelay;
    int    ScrollStep;
    bool   ScrollCheck;
    int    FixedFPS;
    uint   FPS;
    uint   PingPeriod;
    uint   Ping;
    bool   MsgboxInvert;
    bool   MessNotify;
    bool   SoundNotify;
    bool   AlwaysOnTop;
    uint   TextDelay;
    uint   DamageHitDelay;
    int    ScreenWidth;
    int    ScreenHeight;
    int    MultiSampling;
    bool   MouseScroll;
    uint   DoubleClickTime;
    uchar  RoofAlpha;
    bool   HideCursor;
    bool   ShowMoveCursor;
    bool   DisableMouseEvents;
    bool   DisableKeyboardEvents;
    bool   HidePassword;
    string PlayerOffAppendix;
    uint   Animation3dSmoothTime;
    uint   Animation3dFPS;
    int    RunModMul;
    int    RunModDiv;
    int    RunModAdd;
    bool   MapZooming;
    float  SpritesZoom;
    float  SpritesZoomMax;
    float  SpritesZoomMin;
    float  EffectValues[ EFFECT_SCRIPT_VALUES ];
    string KeyboardRemap;
    uint   CritterFidgetTime;
    uint   Anim2CombatBegin;
    uint   Anim2CombatIdle;
    uint   Anim2CombatEnd;
    uint   RainTick;
    short  RainSpeedX;
    short  RainSpeedY;
    uint   ConsoleHistorySize;
    int    SoundVolume;
    int    MusicVolume;
    uint   ChosenLightColor;
    uchar  ChosenLightDistance;
    int    ChosenLightIntensity;
    uchar  ChosenLightFlags;

    // Mapper
    string ServerDir;
    bool   ShowCorners;
    bool   ShowSpriteCuts;
    bool   ShowDrawOrder;
    bool   SplitTilesCollection;

    void*  ClientMap;
    uchar* ClientMapLight;
    uint   ClientMapWidth;
    uint   ClientMapHeight;

    void*  ( *GetDrawingSprites )( uint & );
    void*  ( *GetSpriteInfo )( uint );
    uint   ( * GetSpriteColor )( uint, int, int, bool );
    bool   ( * IsSpriteHit )( void*, int, int, bool );

    string ( * GetNameByHash )( hash );
    hash   ( * GetHashByName )( const string& );

    uint   ( * ScriptBind )( const char*, const char*, bool );
    void   ( * ScriptPrepare )( uint );
    void   ( * ScriptSetArgInt8 )( char );
    void   ( * ScriptSetArgInt16 )( short );
    void   ( * ScriptSetArgInt )( int );
    void   ( * ScriptSetArgInt64 )( int64 );
    void   ( * ScriptSetArgUInt8 )( uchar );
    void   ( * ScriptSetArgUInt16 )( ushort );
    void   ( * ScriptSetArgUInt )( uint );
    void   ( * ScriptSetArgUInt64 )( uint64 );
    void   ( * ScriptSetArgBool )( bool );
    void   ( * ScriptSetArgFloat )( float );
    void   ( * ScriptSetArgDouble )( double );
    void   ( * ScriptSetArgObject )( void* );
    void   ( * ScriptSetArgAddress )( void* );
    bool   ( * ScriptRunPrepared )();
    char   ( * ScriptGetReturnedInt8 )();
    short  ( * ScriptGetReturnedInt16 )();
    int    ( * ScriptGetReturnedInt )();
    int64  ( * ScriptGetReturnedInt64 )();
    uchar  ( * ScriptGetReturnedUInt8 )();
    ushort ( * ScriptGetReturnedUInt16 )();
    uint   ( * ScriptGetReturnedUInt )();
    uint64 ( * ScriptGetReturnedUInt64 )();
    bool   ( * ScriptGetReturnedBool )();
    float  ( * ScriptGetReturnedFloat )();
    double ( * ScriptGetReturnedDouble )();
    void*  ( *ScriptGetReturnedObject )( );
    void*  ( *ScriptGetReturnedAddress )( );

    int    ( * Random )( int, int );
    uint   ( * GetTick )();
    void   ( * SetLogCallback )( void ( * )( const char* ), bool );
    void   ( * AddPropertyCallback )( void ( * )( void*, void*, void*, void* ) );

    GameOptions();
} extern GameOpt;

// Zoom
#define MIN_ZOOM    ( 0.1f )
#define MAX_ZOOM    ( 20.0f )

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
void WriteData( UCharVec& vec, T data )
{
    size_t cur = vec.size();
    vec.resize( cur + sizeof( data ) );
    memcpy( &vec[ cur ], &data, sizeof( data ) );
}

template< class T >
void WriteDataArr( UCharVec& vec, T* data, uint size )
{
    if( size )
    {
        uint cur = (uint) vec.size();
        vec.resize( cur + size );
        memcpy( &vec[ cur ], data, size );
    }
}

template< class T >
T ReadData( UCharVec& vec, uint& pos )
{
    T data;
    memcpy( &data, &vec[ pos ], sizeof( T ) );
    pos += sizeof( T );
    return data;
}

template< class T >
T* ReadDataArr( UCharVec& vec, uint size, uint& pos )
{
    pos += size;
    return size ? &vec[ pos - size ] : nullptr;
}