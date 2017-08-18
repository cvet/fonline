#ifndef __COMMON__
#define __COMMON__

// Versions
#define FONLINE_VERSION                          ( 537 )

// Debugging
// #define DEV_VERSION
// #define SHOW_RACE_CONDITIONS // All known places with race conditions, not use in multithreading
// #define SHOW_DEPRECTAED // All known places with deprecated stuff
// #define SHOW_ANDROID_TODO
// #define DISABLE_EGG
#define DISABLE_UIDS

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
# define ERRORSTR                                 strerror( errno )
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
# define SOCKET                                   int
# define INVALID_SOCKET                           ( -1 )
# define SOCKET_ERROR                             ( -1 )
# define closesocket                              close
# define SD_RECEIVE                               SHUT_RD
# define SD_SEND                                  SHUT_WR
# define SD_BOTH                                  SHUT_RDWR
#endif

// DLL
#ifdef FO_WINDOWS
# define DLL_Load( name )                 (void*) LoadLibraryW( CharToWideChar( name ).c_str() )
# define DLL_Free( h )                    FreeLibrary( (HMODULE) h )
# define DLL_GetAddress( h, pname )       (size_t*) GetProcAddress( (HMODULE) h, pname )
# define DLL_Error()                      Str::ItoA( GetLastError() )
#else
# include <dlfcn.h>
# define DLL_Load( name )                 (void*) dlopen( name, RTLD_NOW | RTLD_LOCAL )
# define DLL_Free( h )                    dlclose( h )
# define DLL_GetAddress( h, pname )       (size_t*) dlsym( h, pname )
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

// FOnline stuff
#include "Types.h"
#include "Defines.h"
#include "Log.h"
#include "Timer.h"
#include "Debugger.h"
#include "Exception.h"
#include "FlexRect.h"
#include "Randomizer.h"
#include "Text.h"
#include "FileManager.h"
#include "IniParser.h"
#include "AngelScript/sdk/add_on/scriptarray/scriptarray.h"
#include "AngelScript/sdk/add_on/scriptdictionary/scriptdictionary.h"
#include "AngelScript/scriptdict.h"
#include "SHA/sha1.h"
#include "SHA/sha2.h"

#if defined ( FONLINE_NPCEDITOR ) || defined ( FONLINE_MRFIXIT ) || defined ( FONLINE_CLIENT )
# define NO_THREADING
#endif
#include "Threading.h"

#define ___MSG1( x )                      # x
#define ___MSG0( x )                      ___MSG1( x )
#define MESSAGE( desc )                   message( __FILE__ "(" ___MSG0( __LINE__ ) "):" # desc )

#define SAFEREL( x ) \
    { if( x )        \
          ( x )->Release(); ( x ) = NULL; }
#define SAFEDEL( x ) \
    { if( x )        \
          delete ( x ); ( x ) = NULL; }
#define SAFEDELA( x ) \
    { if( x )         \
          delete[] ( x ); ( x ) = NULL; }

#define MAX( a, b )                       ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )                       ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

#define PACKUINT64( u32hi, u32lo )        ( ( (uint64) u32hi << 32 ) | ( (uint64) u32lo ) )
#define MAKEUINT( ch0, ch1, ch2, ch3 )    ( (uint) (uchar) ( ch0 ) | ( (uint) (uchar) ( ch1 ) << 8 ) | ( (uint) (uchar) ( ch2 ) << 16 ) | ( (uint) (uchar) ( ch3 ) << 24 ) )

#ifdef SHOW_RACE_CONDITIONS
# define RACE_CONDITION                           MESSAGE( "Race condition" )
#else
# define RACE_CONDITION
#endif

#ifdef SHOW_DEPRECATED
# define DEPRECATED                               MESSAGE( "Deprecated" )
#else
# define DEPRECATED
#endif

#ifdef SHOW_ANDROID_TODO
# define ANDROID_TODO                             MESSAGE( "Android todo" )
#else
# define ANDROID_TODO
#endif

typedef vector< Rect >  IntRectVec;
typedef vector< RectF > FltRectVec;

string GetLastSocketError();

extern IniParser* MainConfig;
extern StrVec     GameModules;
void InitialSetup( uint argc, char** argv );

extern Randomizer DefaultRandomizer;
int Random( int minimum, int maximum );

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
void ShowMessage( const char* message );
int  ConvertParamValue( const char* str, bool& fail );

// Find in container of pointers
template< typename TIt, typename T >
inline TIt PtrCollectionFind( TIt it, TIt end, const T& v )
{
    for( ; it != end; ++it )
        if( *( *it ) == v ) // Invoke operator==
            return it;
    return end;
}

// Hex offsets
#define MAX_HEX_OFFSET                            ( 50 ) // Must be not odd
void GetHexOffsets( bool odd, short*& sx, short*& sy );
void GetHexInterval( int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y );

#ifdef FO_WINDOWS
std::wstring CharToWideChar( const char* str );
std::wstring CharToWideChar( const std::string& str );
std::string  WideCharToChar( const wchar_t* str );
#endif

/************************************************************************/
/* Client & Mapper                                                      */
/************************************************************************/

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )

# define PI_VALUE                                 ( 3.141592654f )

# include "SDL.h"
# include "SDL_syswm.h"
# include "GluStuff.h"
# ifndef FO_OGL_ES
#  include "GL/glew.h"
#  include "SDL_opengl.h"
#  ifdef FO_MAC
#   undef glGenVertexArrays
#   undef glBindVertexArray
#   undef glDeleteVertexArrays
#   define glGenVertexArrays                      glGenVertexArraysAPPLE
#   define glBindVertexArray                      glBindVertexArrayAPPLE
#   define glDeleteVertexArrays                   glDeleteVertexArraysAPPLE
#  endif
# else
#  include "SDL_opengles2.h"
#  define glGenVertexArrays                       glGenVertexArraysOES
#  define glBindVertexArray                       glBindVertexArrayOES
#  define glDeleteVertexArrays                    glDeleteVertexArraysOES
#  define glGenFramebuffersEXT                    glGenFramebuffers
#  define glBindFramebufferEXT                    glBindFramebuffer
#  define glFramebufferTexture2DEXT               glFramebufferTexture2D
#  define glRenderbufferStorageEXT                glRenderbufferStorage
#  define glGenRenderbuffersEXT                   glGenRenderbuffers
#  define glBindRenderbufferEXT                   glBindRenderbuffer
#  define glFramebufferRenderbufferEXT            glFramebufferRenderbuffer
#  define glCheckFramebufferStatusEXT             glCheckFramebufferStatus
#  define glDeleteRenderbuffersEXT                glDeleteRenderbuffers
#  define glDeleteFramebuffersEXT                 glDeleteFramebuffers
#  define glProgramBinary( a, b, c, d )
#  define glGetProgramBinary( a, b, c, d, e )
#  define glProgramParameteri                     glProgramParameteriEXT
#  define GL_PROGRAM_BINARY_RETRIEVABLE_HINT      0
#  define GL_PROGRAM_BINARY_LENGTH                0
#  define GL_FRAMEBUFFER_COMPLETE_EXT             GL_FRAMEBUFFER_COMPLETE
#  define GL_FRAMEBUFFER_EXT                      GL_FRAMEBUFFER
#  define GL_COLOR_ATTACHMENT0_EXT                GL_COLOR_ATTACHMENT0
#  define GL_RENDERBUFFER_EXT                     GL_RENDERBUFFER
#  define GL_DEPTH_ATTACHMENT_EXT                 GL_DEPTH_ATTACHMENT
#  define GL_RENDERBUFFER_BINDING_EXT             GL_RENDERBUFFER_BINDING
#  define GL_CLAMP                                GL_CLAMP_TO_EDGE
#  define GL_DEPTH24_STENCIL8                     GL_DEPTH24_STENCIL8_OES
#  define GL_DEPTH24_STENCIL8_EXT                 GL_DEPTH24_STENCIL8_OES
#  define GL_STENCIL_ATTACHMENT_EXT               GL_STENCIL_ATTACHMENT
#  define glGetTexImage( a, b, c, d, e )
#  define glDrawBuffer( a )
#  define GL_MAX_COLOR_TEXTURE_SAMPLES            0
#  define GL_TEXTURE_2D_MULTISAMPLE               0
#  if defined ( FO_MAC ) || defined ( FO_IOS )
#   define glRenderbufferStorageMultisample       glRenderbufferStorageMultisampleAPPLE
#   define glRenderbufferStorageMultisampleEXT    glRenderbufferStorageMultisampleAPPLE
#  endif
#  ifdef FO_ANDROID
#   undef glGenVertexArrays
#   undef glBindVertexArray
#   undef glDeleteVertexArrays
#   define glGenVertexArrays( a, b )
#   define glBindVertexArray( a )
#   define glDeleteVertexArrays( a, b )
#   define glRenderbufferStorageMultisample( a, b, c, d, e )
#   define glRenderbufferStorageMultisampleEXT( a, b, c, d, e )
#   define GL_MAX                                 GL_MAX_EXT
#   define GL_MIN                                 GL_MIN_EXT
#  endif
#  ifdef FO_WEB
#   define GL_MAX                                 GL_MAX_EXT
#   define GL_MIN                                 GL_MIN_EXT
#   define glRenderbufferStorageMultisample( a, b, c, d, e )
#   define glRenderbufferStorageMultisampleEXT( a, b, c, d, e )
#  endif
#  define glTexImage2DMultisample( a, b, c, d, e, f )
# endif
# ifdef FO_MSVC
#  pragma comment( lib, "opengl32.lib" )
#  pragma comment( lib, "glu32.lib" )
#  pragma comment( lib, "Version.lib" )
#  pragma comment( lib, "Winmm.lib" )
#  pragma comment( lib, "Imm32.lib" )
# endif
# define GL( expr )             { expr; if( GameOpt.OpenGLDebug ) { GLenum err__ = glGetError(); RUNTIME_ASSERT_STR( err__ == GL_NO_ERROR, fmt::format( # expr " error {:#X}", err__ ).c_str() ); } }

extern bool OGL_version_2_0;
extern bool OGL_vertex_buffer_object;
extern bool OGL_framebuffer_object;
extern bool OGL_framebuffer_object_ext;
extern bool OGL_framebuffer_multisample;
extern bool OGL_packed_depth_stencil;
extern bool OGL_texture_multisample;
extern bool OGL_vertex_array_object;
extern bool OGL_get_program_binary;
# define GL_HAS( extension )    ( OGL_ ## extension )

extern SDL_Window*   MainWindow;
extern SDL_GLContext GLContext;
extern IntVec        MainWindowKeyboardEvents;
extern StrVec        MainWindowKeyboardEventsText;
extern IntVec        MainWindowMouseEvents;

const uchar          SELECT_ALPHA    = 100;

uint GetColorDay( int* day_time, uchar* colors, int game_time, int* light );
void GetClientOptions();

struct ClientScriptFunctions
{
    void* Start;
    void* Finish;
    void* Loop;
    void* GetActiveScreens;
    void* ScreenChange;
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
    void* RenderMap;
    void* MouseDown;
    void* MouseUp;
    void* MouseMove;
    void* KeyDown;
    void* KeyUp;
    void* InputLost;
    void* CritterAnimation;
    void* CritterAnimationSubstitute;
    void* CritterAnimationFallout;
    void* MapLoad;
    void* MapSave;
    void* InspectorProperties;
} extern MapperFunctions;

#endif

/************************************************************************/
/* Server                                                               */
/************************************************************************/

#ifdef FONLINE_SERVER

extern bool FOQuit;
extern int  ServerGameSleep;
extern int  MemoryDebugLevel;
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
    void* WorldSave;
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
} extern ServerFunctions;

#endif

/************************************************************************/
/* Game options                                                         */
/************************************************************************/

struct GameOptions
{
    string      WorkDir;
    string      CommandLine;

    ushort      YearStart;
    uint        YearStartFTLo;
    uint        YearStartFTHi;
    ushort      Year;
    ushort      Month;
    ushort      Day;
    ushort      Hour;
    ushort      Minute;
    ushort      Second;
    uint        FullSecondStart;
    uint        FullSecond;
    ushort      TimeMultiplier;
    uint        GameTimeTick;

    bool        Singleplayer;
    bool        DisableTcpNagle;
    bool        DisableZlibCompression;
    uint        FloodSize;
    uint        BruteForceTick;
    bool        NoAnswerShuffle;
    bool        DialogDemandRecheck;
    uint        SneakDivider;
    uint        LookMinimum;
    int         DeadHitPoints;
    uint        Breaktime;
    uint        TimeoutTransfer;
    uint        TimeoutBattle;
    bool        RunOnCombat;
    bool        RunOnTransfer;
    uint        GlobalMapWidth;
    uint        GlobalMapHeight;
    uint        GlobalMapZoneLength;
    uint        BagRefreshTime;
    uint        WhisperDist;
    uint        ShoutDist;
    int         LookChecks;
    uint        LookDir[ 5 ];
    uint        LookSneakDir[ 5 ];
    uint        RegistrationTimeout;
    uint        AccountPlayTime;
    uint        ScriptRunSuspendTimeout;
    uint        ScriptRunMessageTimeout;
    uint        TalkDistance;
    uint        NpcMaxTalkers;
    uint        MinNameLength;
    uint        MaxNameLength;
    uint        DlgTalkMinTime;
    uint        DlgBarterMinTime;
    uint        MinimumOfflineTime;
    bool        GameServer;
    bool        UpdateServer;
    bool        ForceRebuildResources;

    bool        MapHexagonal;
    int         MapHexWidth;
    int         MapHexHeight;
    int         MapHexLineHeight;
    int         MapTileOffsX;
    int         MapTileOffsY;
    int         MapTileStep;
    int         MapRoofOffsX;
    int         MapRoofOffsY;
    int         MapRoofSkipSize;
    float       MapCameraAngle;
    bool        MapSmoothPath;
    string      MapDataPrefix;

    // Client and Mapper
    bool        Quit;
    int         WaitPing;
    bool        OpenGLRendering;
    bool        OpenGLDebug;
    bool        AssimpLogging;
    int         MouseX;
    int         MouseY;
    int         LastMouseX;
    int         LastMouseY;
    int         ScrOx;
    int         ScrOy;
    bool        ShowTile;
    bool        ShowRoof;
    bool        ShowItem;
    bool        ShowScen;
    bool        ShowWall;
    bool        ShowCrit;
    bool        ShowFast;
    bool        ShowPlayerNames;
    bool        ShowNpcNames;
    bool        ShowCritId;
    bool        ScrollKeybLeft;
    bool        ScrollKeybRight;
    bool        ScrollKeybUp;
    bool        ScrollKeybDown;
    bool        ScrollMouseLeft;
    bool        ScrollMouseRight;
    bool        ScrollMouseUp;
    bool        ScrollMouseDown;
    bool        ShowGroups;
    bool        HelpInfo;
    bool        DebugInfo;
    bool        DebugNet;
    bool        Enable3dRendering;
    bool        FullScreen;
    bool        VSync;
    int         Light;
    string      Host;
    uint        Port;
    string      UpdateServerHost;
    uint        UpdateServerPort;
    uint        ProxyType;
    string      ProxyHost;
    uint        ProxyPort;
    string      ProxyUser;
    string      ProxyPass;
    uint        ScrollDelay;
    int         ScrollStep;
    bool        ScrollCheck;
    int         FixedFPS;
    uint        FPS;
    uint        PingPeriod;
    uint        Ping;
    bool        MsgboxInvert;
    bool        MessNotify;
    bool        SoundNotify;
    bool        AlwaysOnTop;
    uint        TextDelay;
    uint        DamageHitDelay;
    int         ScreenWidth;
    int         ScreenHeight;
    int         MultiSampling;
    bool        MouseScroll;
    uint        DoubleClickTime;
    uchar       RoofAlpha;
    bool        HideCursor;
    bool        ShowMoveCursor;
    bool        DisableMouseEvents;
    bool        DisableKeyboardEvents;
    bool        HidePassword;
    string      PlayerOffAppendix;
    uint        Animation3dSmoothTime;
    uint        Animation3dFPS;
    int         RunModMul;
    int         RunModDiv;
    int         RunModAdd;
    bool        MapZooming;
    float       SpritesZoom;
    float       SpritesZoomMax;
    float       SpritesZoomMin;
    float       EffectValues[ EFFECT_SCRIPT_VALUES ];
    string      KeyboardRemap;
    uint        CritterFidgetTime;
    uint        Anim2CombatBegin;
    uint        Anim2CombatIdle;
    uint        Anim2CombatEnd;
    uint        RainTick;
    short       RainSpeedX;
    short       RainSpeedY;
    uint        ConsoleHistorySize;
    int         SoundVolume;
    int         MusicVolume;
    uint        ChosenLightColor;
    uchar       ChosenLightDistance;
    int         ChosenLightIntensity;
    uchar       ChosenLightFlags;

    // Mapper
    string      ServerDir;
    bool        ShowCorners;
    bool        ShowSpriteCuts;
    bool        ShowDrawOrder;
    bool        SplitTilesCollection;

    void*       ClientMap;
    uchar*      ClientMapLight;
    uint        ClientMapWidth;
    uint        ClientMapHeight;

    void*       ( *GetDrawingSprites )( uint & );
    void*       ( *GetSpriteInfo )( uint );
    uint        ( * GetSpriteColor )( uint, int, int, bool );
    bool        ( * IsSpriteHit )( void*, int, int, bool );

    const char* ( *GetNameByHash )( hash );
    hash        ( * GetHashByName )( const string& );

    uint        ( * ScriptBind )( const char*, const char*, bool );
    void        ( * ScriptPrepare )( uint );
    void        ( * ScriptSetArgInt8 )( char );
    void        ( * ScriptSetArgInt16 )( short );
    void        ( * ScriptSetArgInt )( int );
    void        ( * ScriptSetArgInt64 )( int64 );
    void        ( * ScriptSetArgUInt8 )( uchar );
    void        ( * ScriptSetArgUInt16 )( ushort );
    void        ( * ScriptSetArgUInt )( uint );
    void        ( * ScriptSetArgUInt64 )( uint64 );
    void        ( * ScriptSetArgBool )( bool );
    void        ( * ScriptSetArgFloat )( float );
    void        ( * ScriptSetArgDouble )( double );
    void        ( * ScriptSetArgObject )( void* );
    void        ( * ScriptSetArgAddress )( void* );
    bool        ( * ScriptRunPrepared )();
    char        ( * ScriptGetReturnedInt8 )();
    short       ( * ScriptGetReturnedInt16 )();
    int         ( * ScriptGetReturnedInt )();
    int64       ( * ScriptGetReturnedInt64 )();
    uchar       ( * ScriptGetReturnedUInt8 )();
    ushort      ( * ScriptGetReturnedUInt16 )();
    uint        ( * ScriptGetReturnedUInt )();
    uint64      ( * ScriptGetReturnedUInt64 )();
    bool        ( * ScriptGetReturnedBool )();
    float       ( * ScriptGetReturnedFloat )();
    double      ( * ScriptGetReturnedDouble )();
    void*       ( *ScriptGetReturnedObject )( );
    void*       ( *ScriptGetReturnedAddress )( );

    int         ( * Random )( int, int );
    uint        ( * GetTick )();
    void        ( * SetLogCallback )( void ( * )( const char* ), bool );
    void        ( * AddPropertyCallback )( void ( * )( void*, void*, void*, void* ) );

    GameOptions();
} extern GameOpt;

// Zoom
#define MIN_ZOOM    ( 0.1f )
#define MAX_ZOOM    ( 20.0f )

/************************************************************************/
/* Auto pointers                                                        */
/************************************************************************/

template< class T >
class AutoPtr
{
public:
    AutoPtr( T* ptr ): Ptr( ptr ) {}
    ~AutoPtr() { if( Ptr ) delete Ptr; }
    T& operator*() const  { return *Get(); }
    T* operator->() const { return Get(); }
    T* Get() const        { return Ptr; }
    T* Release()
    {
        T* tmp = Ptr;
        Ptr = nullptr;
        return tmp;
    }
    void Reset( T* ptr )
    {
        if( ptr != Ptr && Ptr != 0 ) delete Ptr;
        Ptr = ptr;
    }
    bool IsValid() const { return Ptr != nullptr; }

private:
    T* Ptr;
};

template< class T >
class AutoPtrArr
{
public:
    AutoPtrArr( T* ptr ): Ptr( ptr ) {}
    ~AutoPtrArr() { if( Ptr ) delete[] Ptr; }
    T& operator*() const  { return *Get(); }
    T* operator->() const { return Get(); }
    T* Get() const        { return Ptr; }
    T* Release()
    {
        T* tmp = Ptr;
        Ptr = nullptr;
        return tmp;
    }
    void Reset( T* ptr )
    {
        if( ptr != Ptr && Ptr != 0 ) delete[] Ptr;
        Ptr = ptr;
    }
    bool IsValid() const { return Ptr != nullptr; }

private:
    T* Ptr;
};

/************************************************************************/
/* File logger                                                          */
/************************************************************************/

class FileLogger
{
private:
    FILE* logFile;
    uint  startTick;

public:
    FileLogger( const char* fname );
    ~FileLogger();
    void Write( const char* fmt, ... );
};

/************************************************************************/
/* Single player                                                        */
/************************************************************************/

#ifdef FO_WINDOWS
class InterprocessData
{
public:
    ushort NetPort;
    bool   Pause;

private:
    HANDLE mapFileMutex;
    HANDLE mapFile;
    void*  mapFilePtr;

public:
    HANDLE Init();
    void   Finish();
    bool   Attach( HANDLE map_file );
    bool   Lock();
    void   Unlock();
    bool   Refresh();
};

extern HANDLE SingleplayerClientProcess;
#else
// Todo: linux
class InterprocessData
{
public:
    ushort NetPort;
    bool   Pause;
};
#endif

extern bool             Singleplayer;
extern InterprocessData SingleplayerData;

/************************************************************************/
/* Memory pool                                                          */
/************************************************************************/

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

/************************************************************************/
/* Data serialization helpers                                           */
/************************************************************************/

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

#endif // __COMMON__
