#ifndef __COMMON__
#define __COMMON__

// Debugging
// #define DEV_VESRION
// #define SHOW_RACE_CONDITIONS // All known places with race conditions, not use in multithreading
// #define DISABLE_EGG

// Some platform specific definitions
#include "PlatformSpecific.h"

// Standard API
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include <math.h>
#ifdef FO_WINDOWS
# define WINVER                                  0x0501 // Windows XP
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else
# include <errno.h>
# include <string.h> // strerror
# include <unistd.h>
# define ERRORSTR                                strerror( errno )
# define ExitProcess( code )              exit( code )
#endif

// Network
const char* GetLastSocketError();
#ifdef FO_WINDOWS
# include <winsock2.h>
# define socklen_t                               int
# if defined ( FO_MSVC )
#  pragma comment( lib, "Ws2_32.lib" )
# endif
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# define SOCKET                                  int
# define INVALID_SOCKET                          ( -1 )
# define SOCKET_ERROR                            ( -1 )
# define closesocket                             close
# define SD_RECEIVE                              SHUT_RD
# define SD_SEND                                 SHUT_WR
# define SD_BOTH                                 SHUT_RDWR
#endif

// DLL
#ifdef FO_WINDOWS
# define DLL_Load( name )                 (void*) LoadLibrary( name )
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

// FOnline stuff
#include "Types.h"
#include "Defines.h"
#include "Log.h"
#include "Timer.h"
#include "Debugger.h"
#include "Exception.h"
#include "FlexRect.h"
#include "Randomizer.h"
#include "Mutex.h"
#include "Text.h"
#include "FileSystem.h"
#include "AngelScript/scriptstring.h"

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

#define STATIC_ASSERT( a )                static_assert( a, # a )
#define OFFSETOF( s, m )                  ( (int) (size_t) ( &reinterpret_cast< s* >( 100000 )->m ) - 100000 )
#define UNUSED_VARIABLE( x )              (void) ( x )
#define memzero( ptr, size )              memset( ptr, 0, size )

#define PI_FLOAT                                 ( 3.14159265f )
#define PIBY2_FLOAT                              ( 1.5707963f )
#define SQRT3T2_FLOAT                            ( 3.4641016151f )
#define SQRT3_FLOAT                              ( 1.732050807568877f )
#define BIAS_FLOAT                               ( 0.02f )
#define RAD2DEG                                  ( 57.29577951f )

#define MAX( a, b )                       ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )                       ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

#define PACKUINT64( u32hi, u32lo )        ( ( (uint64) u32hi << 32 ) | ( (uint64) u32lo ) )
#define MAKEUINT( ch0, ch1, ch2, ch3 )    ( (uint) (uchar) ( ch0 ) | ( (uint) (uchar) ( ch1 ) << 8 ) | ( (uint) (uchar) ( ch2 ) << 16 ) | ( (uint) (uchar) ( ch3 ) << 24 ) )

#ifdef SHOW_RACE_CONDITIONS
# define RACE_CONDITION                          MESSAGE( "Race condition" )
#else
# define RACE_CONDITION
#endif

typedef vector< Rect >  IntRectVec;
typedef vector< RectF > FltRectVec;

extern char CommandLine[ MAX_FOTEXT ];
void SetCommandLine( uint argc, char** argv );

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
void RestoreMainDirectory();
void ShowMessage( const char* message );
uint GetDoubleClickTicks();

// Containers comparator template
template< class T >
inline bool CompareContainers( const T& a, const T& b ) { return a.size() == b.size() && ( a.empty() || !memcmp( &a[ 0 ], &b[ 0 ], a.size() * sizeof( a[ 0 ] ) ) ); }

// Hex offsets
#define MAX_HEX_OFFSET                           ( 50 ) // Must be not odd
void GetHexOffsets( bool odd, short*& sx, short*& sy );
void GetHexInterval( int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y );

// Config file
#define CLIENT_CONFIG_APP                        "Game Options"
const char* GetConfigFileName();

// Window name
const char* GetWindowName();

// Shared structure
struct ScoreType
{
    uint ClientId;
    char ClientName[ SCORE_NAME_LEN ];
    int  Value;
};

/************************************************************************/
/* Client & Mapper                                                      */
/************************************************************************/
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )

# define PI_VALUE                                ( 3.141592654f )

# include "SDL/SDL.h"
# ifndef FO_OGL_ES
#  define GLEW_NO_GLU
#  include "GL/glew.h"
#  include "SDL/SDL_opengl.h"
# else
#  include "SDL/SDL_opengles2.h"
#  define glGenVertexArrays                      glGenVertexArraysOES
#  define glBindVertexArray                      glBindVertexArrayOES
#  define glBindVertexArray                      glBindVertexArrayOES
#  define glDeleteVertexArrays                   glDeleteVertexArraysOES
#  define glGenFramebuffersEXT                   glGenFramebuffers
#  define glBindFramebufferEXT                   glBindFramebuffer
#  define glFramebufferTexture2DEXT              glFramebufferTexture2D
#  define glRenderbufferStorageEXT               glRenderbufferStorage
#  define glGenRenderbuffersEXT                  glGenRenderbuffers
#  define glBindRenderbufferEXT                  glBindRenderbuffer
#  define glFramebufferRenderbufferEXT           glFramebufferRenderbuffer
#  define glCheckFramebufferStatusEXT            glCheckFramebufferStatus
#  define glDeleteRenderbuffersEXT               glDeleteRenderbuffers
#  define glDeleteFramebuffersEXT                glDeleteFramebuffers
#  define glProgramBinary( a, b, c, d )
#  define glGetProgramBinary( a, b, c, d, e )
#  define glProgramParameteri                    glProgramParameteriEXT
#  define GL_PROGRAM_BINARY_RETRIEVABLE_HINT     0
#  define GL_PROGRAM_BINARY_LENGTH               0
#  define GL_FRAMEBUFFER_COMPLETE_EXT            GL_FRAMEBUFFER_COMPLETE
#  define GL_FRAMEBUFFER_EXT                     GL_FRAMEBUFFER
#  define GL_COLOR_ATTACHMENT0_EXT               GL_COLOR_ATTACHMENT0
#  define GL_RENDERBUFFER_EXT                    GL_RENDERBUFFER
#  define GL_DEPTH_ATTACHMENT_EXT                GL_DEPTH_ATTACHMENT
#  define GL_CLAMP                               GL_CLAMP_TO_EDGE
#  define GL_DEPTH24_STENCIL8                    GL_DEPTH24_STENCIL8_OES
#  define GL_DEPTH24_STENCIL8_EXT                GL_DEPTH24_STENCIL8_OES
#  define GL_STENCIL_ATTACHMENT_EXT              GL_STENCIL_ATTACHMENT
#  define glGetTexImage( a, b, c, d, e )
#  define glDrawBuffer( a )
#  define GL_MAX_COLOR_TEXTURE_SAMPLES           0
#  define GL_TEXTURE_2D_MULTISAMPLE              0
#  define glRenderbufferStorageMultisample       glRenderbufferStorageMultisampleAPPLE
#  define glRenderbufferStorageMultisampleEXT    glRenderbufferStorageMultisampleAPPLE
#  define glTexImage2DMultisample( a, b, c, d, e, f )
# endif
# include "GL/glu_stuff.h"
# ifdef FO_MSVC
#  pragma comment( lib, "opengl32.lib" )
#  pragma comment( lib, "glu32.lib" )
#  pragma comment( lib, "SDL2_static.lib" )
#  pragma comment( lib, "SDL2main.lib" )
#  pragma comment( lib, "Version.lib" )
#  pragma comment( lib, "Winmm.lib" )
#  pragma comment( lib, "Imm32.lib" )
# endif
# define GL( expr )             { expr; if( GameOpt.OpenGLDebug ) { GLenum err__ = glGetError(); if( err__ != GL_NO_ERROR ) { WriteLogF( _FUNC_, " - " # expr ", error<0x%08X>.\n", err__ ); ExitProcess( 0 ); } } }

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

# include "Assimp/aiTypes.h"

# define MODE_WIDTH                              ( GameOpt.ScreenWidth )
# define MODE_HEIGHT                             ( GameOpt.ScreenHeight )

# ifdef FONLINE_CLIENT
#  include "ResourceClient.h"
#  define CFG_DEF_INT_FILE                       "default800x600.ini"
# else // FONLINE_MAPPER
#  include "ResourceMapper.h"
const uchar SELECT_ALPHA    = 100;
#  define CFG_DEF_INT_FILE                       "mapper_default.ini"
# endif

# define PATH_MAP_FLAGS                          DIR_SLASH_SD "Data" DIR_SLASH_S "maps" DIR_SLASH_S ""
# define PATH_TEXT_FILES                         DIR_SLASH_SD "Data" DIR_SLASH_S "text" DIR_SLASH_S ""

uint GetColorDay( int* day_time, uchar* colors, int game_time, int* light );
void GetClientOptions();

struct ClientScriptFunctions
{
    int Start;
    int Loop;
    int GetActiveScreens;
    int ScreenChange;
    int RenderIface;
    int RenderIfaceScreen;
    int RenderMap;
    int MouseDown;
    int MouseUp;
    int MouseMove;
    int KeyDown;
    int KeyUp;
    int InputLost;
    int CritterIn;
    int CritterOut;
    int ItemMapIn;
    int ItemMapChanged;
    int ItemMapOut;
    int ItemInvIn;
    int ItemInvOut;
    int MapMessage;
    int InMessage;
    int OutMessage;
    int ToHit;
    int HitAim;
    int CombatResult;
    int GenericDesc;
    int ItemLook;
    int CritterLook;
    int GetElevator;
    int ItemCost;
    int PerkCheck;
    int PlayerGeneration;
    int PlayerGenerationCheck;
    int CritterAction;
    int Animation2dProcess;
    int Animation3dProcess;
    int ItemsCollection;
    int CritterAnimation;
    int CritterAnimationSubstitute;
    int CritterAnimationFallout;
    int FilenameLogfile;
    int FilenameScreenshot;
    int CritterCheckMoveItem;
} extern ClientFunctions;

struct MapperScriptFunctions
{
    int Start;
    int Loop;
    int ConsoleMessage;
    int RenderIface;
    int RenderMap;
    int MouseDown;
    int MouseUp;
    int MouseMove;
    int KeyDown;
    int KeyUp;
    int InputLost;
    int CritterAnimation;
    int CritterAnimationSubstitute;
    int CritterAnimationFallout;
} extern MapperFunctions;

#endif
/************************************************************************/
/* Server                                                               */
/************************************************************************/
#ifdef FONLINE_SERVER

# include "Script.h"
# include "ThreadSync.h"
# include "Jobs.h"

extern bool FOQuit;
extern int  ServerGameSleep;
extern int  MemoryDebugLevel;
extern uint VarsGarbageTime;
extern bool WorldSaveManager;
extern bool LogicMT;
extern bool AllowServerNativeCalls;
extern bool AllowClientNativeCalls;

void GetServerOptions();

struct ServerScriptFunctions
{
    int Init;
    int Start;
    int GetStartTime;
    int Finish;
    int Loop;
    int GlobalProcess;
    int GlobalInvite;
    int CritterAttack;
    int CritterAttacked;
    int CritterStealing;
    int CritterUseItem;
    int CritterUseSkill;
    int CritterReloadWeapon;
    int CritterInit;
    int CritterFinish;
    int CritterIdle;
    int CritterDead;
    int CritterRespawn;
    int CritterCheckMoveItem;
    int CritterMoveItem;
    int MapCritterIn;
    int MapCritterOut;
    int NpcPlaneBegin;
    int NpcPlaneEnd;
    int NpcPlaneRun;
    int KarmaVoting;
    int CheckLook;
    int ItemCost;
    int ItemsBarter;
    int ItemsCrafted;
    int PlayerLevelUp;
    int TurnBasedBegin;
    int TurnBasedEnd;
    int TurnBasedProcess;
    int TurnBasedSequence;
    int WorldSave;
    int PlayerRegistration;
    int PlayerLogin;
    int PlayerGetAccess;
    int PlayerAllowCommand;
    int CheckTrapLook;
} extern ServerFunctions;

// Net events
# if defined ( USE_LIBEVENT )
#  if defined ( FO_MSVC )
#   pragma comment( lib, "libevent_core.lib" )
#  endif
# endif

#endif
/************************************************************************/
/* Tools                                                                */
/************************************************************************/
#ifdef FONLINE_NPCEDITOR
# include <strstream>
# include <fstream>

# define _CRT_SECURE_NO_DEPRECATE
# define MAX_TEXT_DIALOG    ( 1000 )

# define ScriptString       string
#endif
#ifdef FONLINE_MRFIXIT
# define ScriptString       string
#endif
/************************************************************************/
/* Game options                                                         */
/************************************************************************/

struct GameOptions
{
    ushort       YearStart;
    uint         YearStartFTLo;
    uint         YearStartFTHi;
    ushort       Year;
    ushort       Month;
    ushort       Day;
    ushort       Hour;
    ushort       Minute;
    ushort       Second;
    uint         FullSecondStart;
    uint         FullSecond;
    ushort       TimeMultiplier;
    uint         GameTimeTick;

    bool         DisableTcpNagle;
    bool         DisableZlibCompression;
    uint         FloodSize;
    bool         NoAnswerShuffle;
    bool         DialogDemandRecheck;
    uint         FixBoyDefaultExperience;
    uint         SneakDivider;
    uint         LevelCap;
    bool         LevelCapAddExperience;
    uint         LookNormal;
    uint         LookMinimum;
    uint         GlobalMapMaxGroupCount;
    uint         CritterIdleTick;
    uint         TurnBasedTick;
    int          DeadHitPoints;
    uint         Breaktime;
    uint         TimeoutTransfer;
    uint         TimeoutBattle;
    uint         ApRegeneration;
    uint         RtApCostCritterWalk;
    uint         RtApCostCritterRun;
    uint         RtApCostMoveItemContainer;
    uint         RtApCostMoveItemInventory;
    uint         RtApCostPickItem;
    uint         RtApCostDropItem;
    uint         RtApCostReloadWeapon;
    uint         RtApCostPickCritter;
    uint         RtApCostUseItem;
    uint         RtApCostUseSkill;
    bool         RtAlwaysRun;
    uint         TbApCostCritterMove;
    uint         TbApCostMoveItemContainer;
    uint         TbApCostMoveItemInventory;
    uint         TbApCostPickItem;
    uint         TbApCostDropItem;
    uint         TbApCostReloadWeapon;
    uint         TbApCostPickCritter;
    uint         TbApCostUseItem;
    uint         TbApCostUseSkill;
    bool         TbAlwaysRun;
    uint         ApCostAimEyes;
    uint         ApCostAimHead;
    uint         ApCostAimGroin;
    uint         ApCostAimTorso;
    uint         ApCostAimArms;
    uint         ApCostAimLegs;
    bool         RunOnCombat;
    bool         RunOnTransfer;
    uint         GlobalMapWidth;
    uint         GlobalMapHeight;
    uint         GlobalMapZoneLength;
    uint         GlobalMapMoveTime;
    uint         BagRefreshTime;
    uint         AttackAnimationsMinDist;
    uint         WhisperDist;
    uint         ShoutDist;
    int          LookChecks;
    uint         LookDir[ 5 ];
    uint         LookSneakDir[ 5 ];
    uint         LookWeight;
    bool         CustomItemCost;
    uint         RegistrationTimeout;
    uint         AccountPlayTime;
    bool         LoggingVars;
    uint         ScriptRunSuspendTimeout;
    uint         ScriptRunMessageTimeout;
    uint         TalkDistance;
    uint         NpcMaxTalkers;
    uint         MinNameLength;
    uint         MaxNameLength;
    uint         DlgTalkMinTime;
    uint         DlgBarterMinTime;
    uint         MinimumOfflineTime;

    int          StartSpecialPoints;
    int          StartTagSkillPoints;
    int          SkillMaxValue;
    int          SkillModAdd2;
    int          SkillModAdd3;
    int          SkillModAdd4;
    int          SkillModAdd5;
    int          SkillModAdd6;

    bool         AbsoluteOffsets;
    uint         SkillBegin;
    uint         SkillEnd;
    uint         TimeoutBegin;
    uint         TimeoutEnd;
    uint         KillBegin;
    uint         KillEnd;
    uint         PerkBegin;
    uint         PerkEnd;
    uint         AddictionBegin;
    uint         AddictionEnd;
    uint         KarmaBegin;
    uint         KarmaEnd;
    uint         DamageBegin;
    uint         DamageEnd;
    uint         TraitBegin;
    uint         TraitEnd;
    uint         ReputationBegin;
    uint         ReputationEnd;

    int          ReputationLoved;
    int          ReputationLiked;
    int          ReputationAccepted;
    int          ReputationNeutral;
    int          ReputationAntipathy;
    int          ReputationHated;

    bool         MapHexagonal;
    int          MapHexWidth;
    int          MapHexHeight;
    int          MapHexLineHeight;
    int          MapTileOffsX;
    int          MapTileOffsY;
    int          MapRoofOffsX;
    int          MapRoofOffsY;
    int          MapRoofSkipSize;
    float        MapCameraAngle;
    bool         MapSmoothPath;
    ScriptString MapDataPrefix;

    // Client and Mapper
    bool         Quit;
    bool         OpenGLRendering;
    bool         OpenGLDebug;
    bool         AssimpLogging;
    int          MouseX;
    int          MouseY;
    int          ScrOx;
    int          ScrOy;
    bool         ShowTile;
    bool         ShowRoof;
    bool         ShowItem;
    bool         ShowScen;
    bool         ShowWall;
    bool         ShowCrit;
    bool         ShowFast;
    bool         ShowPlayerNames;
    bool         ShowNpcNames;
    bool         ShowCritId;
    bool         ScrollKeybLeft;
    bool         ScrollKeybRight;
    bool         ScrollKeybUp;
    bool         ScrollKeybDown;
    bool         ScrollMouseLeft;
    bool         ScrollMouseRight;
    bool         ScrollMouseUp;
    bool         ScrollMouseDown;
    bool         ShowGroups;
    bool         HelpInfo;
    bool         DebugInfo;
    bool         DebugNet;
    bool         DebugSprites;
    bool         FullScreen;
    bool         VSync;
    int          Light;
    ScriptString Host;
    uint         Port;
    uint         ProxyType;
    ScriptString ProxyHost;
    uint         ProxyPort;
    ScriptString ProxyUser;
    ScriptString ProxyPass;
    ScriptString Name;
    uint         ScrollDelay;
    int          ScrollStep;
    bool         ScrollCheck;
    ScriptString FoDataPath;
    int          FixedFPS;
    uint         FPS;
    uint         PingPeriod;
    uint         Ping;
    bool         MsgboxInvert;
    uchar        DefaultCombatMode;
    bool         MessNotify;
    bool         SoundNotify;
    bool         AlwaysOnTop;
    uint         TextDelay;
    uint         DamageHitDelay;
    int          ScreenWidth;
    int          ScreenHeight;
    int          MultiSampling;
    bool         MouseScroll;
    int          IndicatorType;
    uint         DoubleClickTime;
    uchar        RoofAlpha;
    bool         HideCursor;
    bool         DisableLMenu;
    bool         DisableMouseEvents;
    bool         DisableKeyboardEvents;
    bool         HidePassword;
    ScriptString PlayerOffAppendix;
    int          CombatMessagesType;
    uint         Animation3dSmoothTime;
    uint         Animation3dFPS;
    int          RunModMul;
    int          RunModDiv;
    int          RunModAdd;
    bool         MapZooming;
    float        SpritesZoom;
    float        SpritesZoomMax;
    float        SpritesZoomMin;
    float        EffectValues[ EFFECT_SCRIPT_VALUES ];
    bool         AlwaysRun;
    uint         AlwaysRunMoveDist;
    uint         AlwaysRunUseDist;
    ScriptString KeyboardRemap;
    uint         CritterFidgetTime;
    uint         Anim2CombatBegin;
    uint         Anim2CombatIdle;
    uint         Anim2CombatEnd;
    uint         RainTick;
    short        RainSpeedX;
    short        RainSpeedY;

    // Mapper
    ScriptString ClientPath;
    ScriptString ServerPath;
    bool         ShowCorners;
    bool         ShowSpriteCuts;
    bool         ShowDrawOrder;
    bool         SplitTilesCollection;

    // Engine data
    void         ( * CritterChangeParameter )( void*, uint );
    void*        CritterTypes;

    void*        ClientMap;
    uchar*       ClientMapLight;
    uint         ClientMapWidth;
    uint         ClientMapHeight;

    void*        ( *GetDrawingSprites )( uint & );
    void*        ( *GetSpriteInfo )( uint );
    uint         ( * GetSpriteColor )( uint, int, int, bool );
    bool         ( * IsSpriteHit )( void*, int, int, bool );

    const char*  ( *GetNameByHash )( uint );
    uint         ( * GetHashByName )( const char* );

    bool         ( * ScriptLoadModule )( const char* );
    uint         ( * ScriptBind )( const char*, const char*, bool );
    bool         ( * ScriptPrepare )( uint );
    void         ( * ScriptSetArgInt8 )( char );
    void         ( * ScriptSetArgInt16 )( short );
    void         ( * ScriptSetArgInt )( int );
    void         ( * ScriptSetArgInt64 )( int64 );
    void         ( * ScriptSetArgUInt8 )( uchar );
    void         ( * ScriptSetArgUInt16 )( ushort );
    void         ( * ScriptSetArgUInt )( uint );
    void         ( * ScriptSetArgUInt64 )( uint64 );
    void         ( * ScriptSetArgBool )( bool );
    void         ( * ScriptSetArgFloat )( float );
    void         ( * ScriptSetArgDouble )( double );
    void         ( * ScriptSetArgObject )( void* );
    void         ( * ScriptSetArgAddress )( void* );
    bool         ( * ScriptRunPrepared )();
    char         ( * ScriptGetReturnedInt8 )();
    short        ( * ScriptGetReturnedInt16 )();
    int          ( * ScriptGetReturnedInt )();
    int64        ( * ScriptGetReturnedInt64 )();
    uchar        ( * ScriptGetReturnedUInt8 )();
    ushort       ( * ScriptGetReturnedUInt16 )();
    uint         ( * ScriptGetReturnedUInt )();
    uint64       ( * ScriptGetReturnedUInt64 )();
    bool         ( * ScriptGetReturnedBool )();
    float        ( * ScriptGetReturnedFloat )();
    double       ( * ScriptGetReturnedDouble )();
    void*        ( *ScriptGetReturnedObject )( );
    void*        ( *ScriptGetReturnedAddress )( );

    int          ( * Random )( int, int );
    uint         ( * GetTick )();
    void         ( * SetLogCallback )( void ( * )( const char* str ), bool );

    // Callbacks
    uint         ( * GetUseApCost )( void*, void*, uchar );
    uint         ( * GetAttackDistantion )( void*, void*, uchar );
    void         ( * GetRainOffset )( short*, short* );

    GameOptions();
} extern GameOpt;

// IndicatorType
#define INDICATOR_LINES      ( 0 )
#define INDICATOR_NUMBERS    ( 1 )
#define INDICATOR_BOTH       ( 2 )
// Zoom
#define MIN_ZOOM             ( 0.2f )
#define MAX_ZOOM             ( 10.0f )

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
        Ptr = NULL;
        return tmp;
    }
    void Reset( T* ptr )
    {
        if( ptr != Ptr && Ptr != 0 ) delete Ptr;
        Ptr = ptr;
    }
    bool IsValid() const { return Ptr != NULL; }

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
        Ptr = NULL;
        return tmp;
    }
    void Reset( T* ptr )
    {
        if( ptr != Ptr && Ptr != 0 ) delete[] Ptr;
        Ptr = ptr;
    }
    bool IsValid() const { return Ptr != NULL; }

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
/* Threads                                                              */
/************************************************************************/

#if !defined ( FONLINE_NPCEDITOR ) && !defined ( FONLINE_MRFIXIT )

# ifdef FO_WINDOWS
#  define PTW32_STATIC_LIB
#  include "PthreadWnd/pthread.h"
#  if defined ( FO_MSVC )
#   pragma comment( lib, "pthread.lib" )
#  endif
# else
#  include <pthread.h>
# endif

class Thread
{
private:
    static THREAD char threadName[ 64 ];
    static SizeTStrMap threadNames;
    static Mutex       threadNamesLocker;
    bool               isStarted;
    pthread_t          threadId;
    pthread_attr_t     threadAttr;

public:
    Thread();
    ~Thread();
    bool Start( void ( * func )( void* ), const char* name, void* arg = NULL );
    void Wait();
    void Finish();

    # if defined ( FO_WINDOWS )
    HANDLE GetWindowsHandle();
    # elif defined ( FO_LINUX )
    pid_t GetPid();
    # endif

    static size_t      GetCurrentId();
    static void        SetCurrentName( const char* name );
    static const char* GetCurrentName();
    static const char* FindName( uint thread_id );
    static void        Sleep( uint ms );
};

#endif

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
/*                                                                      */
/************************************************************************/

// Deprecated stuff
// tiles.lst, items.lst, scenery.lst, walls.lst, misc.lst, intrface.lst, inven.lst
// pid == -1 - interface
// pid == -2 - tiles
// pid == -3 - inventory
string Deprecated_GetPicName( int pid, int type, ushort pic_num );
uint   Deprecated_GetPicHash( int pid, int type, ushort pic_num );
void   Deprecated_CondExtToAnim2( uchar cond, uchar cond_ext, uint& anim2ko, uint& anim2dead );

// Preprocessor output formatting
void FormatPreprocessorOutput( string& str );

#endif // __COMMON__
