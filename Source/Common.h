#ifndef __COMMON__
#define __COMMON__

// Some platform specific definitions
#include "PlatformSpecific.h"

// Standart API
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include <math.h>
#if defined ( FO_WINDOWS )
# define WINVER               0x0501   // Windows XP
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else // FO_LINUX
# include <errno.h>
# define ExitProcess( code )              exit( code )
#endif

// Network
const char* GetLastSocketError();
#if defined ( FO_WINDOWS )
# include <winsock2.h>
# define socklen_t            int
# if defined ( FO_MSVC )
#  pragma comment(lib,"Ws2_32.lib")
# elif defined ( FO_GCC )
// Linker option:
//  Windows
//  -lws2_32
# endif
#else // FO_LINUX
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# define SOCKET               int
# define INVALID_SOCKET       ( -1 )
# define SOCKET_ERROR         ( -1 )
# define closesocket          close
# define SD_RECEIVE           SHUT_RD
# define SD_SEND              SHUT_WR
# define SD_BOTH              SHUT_RDWR
#endif

// FLTK
#if defined ( FO_MSVC )
# pragma comment(lib,"fltk.lib")
# pragma comment(lib,"fltkforms.lib")
# pragma comment(lib,"fltkgl.lib")
# pragma comment(lib,"fltkimages.lib")
# pragma comment(lib,"fltkjpeg.lib")
# pragma comment(lib,"fltkpng.lib")
# pragma comment(lib,"fltkzlib.lib")
#else // FO_GCC
      // Linker options:.
      //  Windows / Linux
      //  -lfltk
      //  -lfltk_forms
      //  -lfltk_gl
      //  -lfltk_images
      //  -lfltk_jpeg
      //  -lfltk_png
      //  -lfltk_z
#endif

// DLL
#if defined ( FO_WINDOWS )
# define DLL_Load( name )                 (void*) LoadLibrary( name )
# define DLL_Free( h )                    FreeLibrary( (HMODULE) h )
# define DLL_GetAddress( h, pname )       (size_t*) GetProcAddress( (HMODULE) h, pname )
# define DLL_Error()                      Str::ItoA( GetLastError() )
#else // FO_LINUX
# include <dlfcn.h>
# define DLL_Load( name )                 (void*) dlopen( name, RTLD_LAZY )
# define DLL_Free( h )                    dlclose( h )
# define DLL_GetAddress( h, pname )       (size_t*) dlsym( h, pname )
# define DLL_Error()                      dlerror()
#endif

// Memory debug
#ifdef _DEBUG
# define _CRTDBG_MAP_ALLOC
# include <stdlib.h>
# include <crtdbg.h>
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

#define STATIC_ASSERT( a )                { static int static_assert_array__[ ( a ) ? 1 : -1 ]; }

#define PI_FLOAT              ( 3.14159265f )
#define PIBY2_FLOAT           ( 1.5707963f )
#define SQRT3T2_FLOAT         ( 3.4641016151f )
#define SQRT3_FLOAT           ( 1.732050807568877f )
#define BIAS_FLOAT            ( 0.02f )
#define RAD2DEG               ( 57.29577951f )

#define MAX( a, b )                       ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )                       ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

#define OFFSETOF( type, member )          ( (int) offsetof( type, member ) )
#define memzero( ptr, size )              memset( ptr, 0, size )
#define PACKUINT64( u32hi, u32lo )        ( ( (uint64) u32hi << 32 ) | ( (uint64) u32lo ) )
#define MAKEUINT( ch0, ch1, ch2, ch3 )    ( (uint) (uchar) ( ch0 ) | ( (uint) (uchar) ( ch1 ) << 8 ) | ( (uint) (uchar) ( ch2 ) << 16 ) | ( (uint) (uchar) ( ch3 ) << 24 ) )

typedef vector< INTRECT > IntRectVec;
typedef vector< FLTRECT > FltRectVec;

extern char   CommandLine[ MAX_FOTEXT ];
extern char** CommandLineArgValues;
extern uint   CommandLineArgCount;
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
uint GetCurThreadId();
void ShowMessage( const char* message );

// Containers comparator template
template< class T >
inline bool CompareContainers( const T& a, const T& b ) { return a.size() == b.size() && ( a.empty() || !memcmp( &a[ 0 ], &b[ 0 ], a.size() * sizeof( a[ 0 ] ) ) ); }

// Hex offsets
#define MAX_HEX_OFFSET        ( 50 )   // Must be not odd
void GetHexOffsets( bool odd, short*& sx, short*& sy );
void GetHexInterval( int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y );

// Name / Password
bool CheckUserName( const char* str );
bool CheckUserPass( const char* str );

// Config file
#define CLIENT_CONFIG_APP     "Game Options"
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

# define COLOR_ARGB( a, r, g, b )         ( (uint) ( ( ( ( a ) & 0xff ) << 24 ) | ( ( ( r ) & 0xff ) << 16 ) | ( ( ( g ) & 0xff ) << 8 ) | ( ( b ) & 0xff ) ) )
# define COLOR_XRGB( r, g, b )            COLOR_ARGB( 0xff, r, g, b )

# include "FL/Fl.H"
# include "FL/Fl_Window.H"
# include "FL/x.H"

class FOWindow: public Fl_Window
{
public:
    FOWindow(): Fl_Window( 0, 0, "" ) {}
    virtual ~FOWindow() {}
    virtual int handle( int event );
};
extern FOWindow* MainWindow; // Initialized and handled in MainClient.cpp / MainMapper.cpp

# ifdef FO_D3D
#  include <dxerr.h>
#  include <d3dx9.h>
#  ifndef D3D_DEBUG_INFO
#   pragma comment(lib,"d3dx9.lib")
#  else
#   pragma comment(lib,"d3dx9d.lib")
#  endif
#  pragma comment(lib,"d3d9.lib")
#  pragma comment(lib,"dxguid.lib")
#  pragma comment(lib,"dxerr.lib")
#  pragma comment(lib,"d3dxof.lib")
#  define D3D_HR( expr )                  { HRESULT hr__ = expr; if( hr__ != D3D_OK ) { WriteLogF( _FUNC_, " - " # expr ", error<%s - %s>.\n", DXGetErrorString( hr__ ), DXGetErrorDescription( hr__ ) ); return 0; } }
# else
#  include <gl/GL.h>
#  include <gl/GLU.h>
#  include "Assimp/aiTypes.h"
# endif

# define PI_VALUE             ( 3.141592654f )

# ifdef FO_D3D
#  define Device_             LPDIRECT3DDEVICE9
#  define Texture_            LPDIRECT3DTEXTURE9
#  define Surface_            LPDIRECT3DSURFACE9
#  define EffectInstance_     D3DXEFFECTINSTANCE
#  define Effect_             LPD3DXEFFECT
#  define EffectDefaults_     LPD3DXEFFECTDEFAULT
#  define EffectValue_        D3DXHANDLE
#  define Material_           D3DMATERIAL9
#  define Mesh_               LPD3DXMESH
#  define Matrix_             D3DXMATRIX
#  define Buffer_             ID3DXBuffer
#  define Vector3_            D3DXVECTOR3
#  define Vector4_            D3DXVECTOR4
#  define AnimSet_            ID3DXAnimationSet
#  define AnimController_     ID3DXAnimationController
#  define PresentParams_      D3DPRESENT_PARAMETERS
#  define Caps_               D3DCAPS9
#  define VertexBuffer_       LPDIRECT3DVERTEXBUFFER9
#  define IndexBuffer_        LPDIRECT3DINDEXBUFFER9
#  define PixelShader_        IDirect3DPixelShader9
#  define ConstantTable_      ID3DXConstantTable
#  define SkinInfo_           LPD3DXSKININFO
#  define Light_              D3DLIGHT9
#  define ViewPort_           D3DVIEWPORT9
#  define LockRect_           D3DLOCKED_RECT
# else
#  define Device_             GLuint
#  define Texture_            GLuint
#  define Surface_            GLuint
#  define EffectInstance_     GLuint
#  define Effect_             GLuint
#  define EffectDefaults_     GLuint
#  define EffectValue_        GLuint
#  define Material_           GLuint
#  define Mesh_               GLuint
#  define Matrix_             aiMatrix4x4
#  define Buffer_             GLuint
#  define Vector3_            aiVector3D
#  define Vector4_            aiQuaternion
#  define AnimSet_            GLuint
#  define AnimController_     AnimController
#  define PresentParams_      GLuint
#  define Caps_               GLuint
#  define VertexBuffer_       GLuint
#  define IndexBuffer_        GLuint
#  define PixelShader_        GLuint
#  define ConstantTable_      GLuint
#  define SkinInfo_           GLuint
#  define Light_              GLuint
#  define ViewPort_           GLuint
#  define LockRect_           GLuint
# endif

# define MODE_WIDTH           ( GameOpt.ScreenWidth )
# define MODE_HEIGHT          ( GameOpt.ScreenHeight )

# ifdef FONLINE_CLIENT
#  include "ResourceClient.h"
#  define CFG_DEF_INT_FILE    "default800x600.ini"
# else // FONLINE_MAPPER
#  include "ResourceMapper.h"
const uchar SELECT_ALPHA    = 100;
#  define CFG_DEF_INT_FILE    "mapper_default.ini"
# endif

# define PATH_MAP_FLAGS       DIR_SLASH_SD "Data" DIR_SLASH_S "maps" DIR_SLASH_S ""
# define PATH_TEXT_FILES      DIR_SLASH_SD "Data" DIR_SLASH_S "text" DIR_SLASH_S ""
# define PATH_LOG_FILE        DIR_SLASH_SD
# define PATH_SCREENS_FILE    DIR_SLASH_SD

uint GetColorDay( int* day_time, uchar* colors, int game_time, int* light );
void GetClientOptions();

struct ClientScriptFunctions
{
    int Start;
    int Loop;
    int GetActiveScreens;
    int ScreenChange;
    int RenderIface;
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
    int CritterChangeItem;
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
#   pragma comment(lib,"libevent.lib")
#   pragma comment(lib,"libevent_core.lib")
#   pragma comment(lib,"libevent_extras.lib")
#  else       // FO_GCC
              // Linker options:
              //  Windows / Linux
              //  -levent
              //  -levent_core
              //  -levent_extras
              //  -levent_pthreads
#  endif
# endif

#endif
/************************************************************************/
/* Npc editor                                                           */
/************************************************************************/
#ifdef FONLINE_NPCEDITOR
# include <strstream>
# include <fstream>

# define _CRT_SECURE_NO_DEPRECATE
# define MAX_TEXT_DIALOG    ( 1000 )
#endif
/************************************************************************/
/* Game options                                                         */
/************************************************************************/

struct GameOptions
{
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

    bool        DisableTcpNagle;
    bool        DisableZlibCompression;
    uint        FloodSize;
    bool        FreeExp;
    bool        RegulatePvP;
    bool        NoAnswerShuffle;
    bool        DialogDemandRecheck;
    uint        FixBoyDefaultExperience;
    uint        SneakDivider;
    uint        LevelCap;
    bool        LevelCapAddExperience;
    uint        LookNormal;
    uint        LookMinimum;
    uint        GlobalMapMaxGroupCount;
    uint        CritterIdleTick;
    uint        TurnBasedTick;
    int         DeadHitPoints;
    uint        Breaktime;
    uint        TimeoutTransfer;
    uint        TimeoutBattle;
    uint        ApRegeneration;
    uint        RtApCostCritterWalk;
    uint        RtApCostCritterRun;
    uint        RtApCostMoveItemContainer;
    uint        RtApCostMoveItemInventory;
    uint        RtApCostPickItem;
    uint        RtApCostDropItem;
    uint        RtApCostReloadWeapon;
    uint        RtApCostPickCritter;
    uint        RtApCostUseItem;
    uint        RtApCostUseSkill;
    bool        RtAlwaysRun;
    uint        TbApCostCritterMove;
    uint        TbApCostMoveItemContainer;
    uint        TbApCostMoveItemInventory;
    uint        TbApCostPickItem;
    uint        TbApCostDropItem;
    uint        TbApCostReloadWeapon;
    uint        TbApCostPickCritter;
    uint        TbApCostUseItem;
    uint        TbApCostUseSkill;
    bool        TbAlwaysRun;
    uint        ApCostAimEyes;
    uint        ApCostAimHead;
    uint        ApCostAimGroin;
    uint        ApCostAimTorso;
    uint        ApCostAimArms;
    uint        ApCostAimLegs;
    bool        RunOnCombat;
    bool        RunOnTransfer;
    uint        GlobalMapWidth;
    uint        GlobalMapHeight;
    uint        GlobalMapZoneLength;
    uint        GlobalMapMoveTime;
    uint        BagRefreshTime;
    uint        AttackAnimationsMinDist;
    uint        WhisperDist;
    uint        ShoutDist;
    int         LookChecks;
    uint        LookDir[ 5 ];
    uint        LookSneakDir[ 5 ];
    uint        LookWeight;
    bool        CustomItemCost;
    uint        RegistrationTimeout;
    uint        AccountPlayTime;
    bool        LoggingVars;
    uint        ScriptRunSuspendTimeout;
    uint        ScriptRunMessageTimeout;
    uint        TalkDistance;
    uint        MinNameLength;
    uint        MaxNameLength;
    uint        DlgTalkMinTime;
    uint        DlgBarterMinTime;
    uint        MinimumOfflineTime;

    int         StartSpecialPoints;
    int         StartTagSkillPoints;
    int         SkillMaxValue;
    int         SkillModAdd2;
    int         SkillModAdd3;
    int         SkillModAdd4;
    int         SkillModAdd5;
    int         SkillModAdd6;

    bool        AbsoluteOffsets;
    uint        SkillBegin;
    uint        SkillEnd;
    uint        TimeoutBegin;
    uint        TimeoutEnd;
    uint        KillBegin;
    uint        KillEnd;
    uint        PerkBegin;
    uint        PerkEnd;
    uint        AddictionBegin;
    uint        AddictionEnd;
    uint        KarmaBegin;
    uint        KarmaEnd;
    uint        DamageBegin;
    uint        DamageEnd;
    uint        TraitBegin;
    uint        TraitEnd;
    uint        ReputationBegin;
    uint        ReputationEnd;

    int         ReputationLoved;
    int         ReputationLiked;
    int         ReputationAccepted;
    int         ReputationNeutral;
    int         ReputationAntipathy;
    int         ReputationHated;

    bool        MapHexagonal;
    int         MapHexWidth;
    int         MapHexHeight;
    int         MapHexLineHeight;
    int         MapTileOffsX;
    int         MapTileOffsY;
    int         MapRoofOffsX;
    int         MapRoofOffsY;
    int         MapRoofSkipSize;
    float       MapCameraAngle;
    bool        MapSmoothPath;
    string      MapDataPrefix;
    int         MapDataPrefixRefCounter;

    // Client and Mapper
    bool        Quit;
    int         MouseX;
    int         MouseY;
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
    bool        DebugSprites;
    bool        FullScreen;
    bool        VSync;
    int         FlushVal;
    int         BaseTexture;
    int         ScreenClear;
    int         Light;
    string      Host;
    int         HostRefCounter;
    uint        Port;
    uint        ProxyType;
    string      ProxyHost;
    int         ProxyHostRefCounter;
    uint        ProxyPort;
    string      ProxyUser;
    int         ProxyUserRefCounter;
    string      ProxyPass;
    int         ProxyPassRefCounter;
    string      Name;
    int         NameRefCounter;
    int         ScrollDelay;
    int         ScrollStep;
    bool        ScrollCheck;
    string      FoDataPath;
    int         FoDataPathRefCounter;
    int         Sleep;
    bool        MsgboxInvert;
    int         ChangeLang;
    uchar       DefaultCombatMode;
    bool        MessNotify;
    bool        SoundNotify;
    bool        AlwaysOnTop;
    uint        TextDelay;
    uint        DamageHitDelay;
    int         ScreenWidth;
    int         ScreenHeight;
    int         MultiSampling;
    bool        SoftwareSkinning;
    bool        MouseScroll;
    int         IndicatorType;
    uint        DoubleClickTime;
    uchar       RoofAlpha;
    bool        HideCursor;
    bool        DisableLMenu;
    bool        DisableMouseEvents;
    bool        DisableKeyboardEvents;
    bool        HidePassword;
    string      PlayerOffAppendix;
    int         PlayerOffAppendixRefCounter;
    int         CombatMessagesType;
    bool        DisableDrawScreens;
    uint        Animation3dSmoothTime;
    uint        Animation3dFPS;
    int         RunModMul;
    int         RunModDiv;
    int         RunModAdd;
    float       SpritesZoom;
    float       SpritesZoomMax;
    float       SpritesZoomMin;
    float       EffectValues[ EFFECT_SCRIPT_VALUES ];
    bool        AlwaysRun;
    uint        AlwaysRunMoveDist;
    uint        AlwaysRunUseDist;
    string      KeyboardRemap;
    int         KeyboardRemapRefCounter;
    uint        CritterFidgetTime;
    uint        Anim2CombatBegin;
    uint        Anim2CombatIdle;
    uint        Anim2CombatEnd;

    // Mapper
    string      ClientPath;
    int         ClientPathRefCounter;
    string      ServerPath;
    int         ServerPathRefCounter;
    bool        ShowCorners;
    bool        ShowSpriteCuts;
    bool        ShowDrawOrder;
    bool        SplitTilesCollection;

    // Engine data
    void        ( * CritterChangeParameter )( void*, uint );
    void*       CritterTypes;

    void*       ClientMap;
    uint        ClientMapWidth;
    uint        ClientMapHeight;

    void*       ( *GetDrawingSprites )( uint & );
    void*       ( *GetSpriteInfo )( uint );
    uint        ( * GetSpriteColor )( uint, int, int, bool );
    bool        ( * IsSpriteHit )( void*, int, int, bool );

    const char* ( *GetNameByHash )( uint );
    uint        ( * GetHashByName )( const char* );

    // Callbacks
    uint        ( * GetUseApCost )( void*, void*, uchar );
    uint        ( * GetAttackDistantion )( void*, void*, uchar );

    GameOptions();
} extern GameOpt;

// ChangeLang
#define CHANGE_LANG_CTRL_SHIFT    ( 0 )
#define CHANGE_LANG_ALT_SHIFT     ( 1 )
// IndicatorType
#define INDICATOR_LINES           ( 0 )
#define INDICATOR_NUMBERS         ( 1 )
#define INDICATOR_BOTH            ( 2 )
// Zoom
#define MIN_ZOOM                  ( 0.2f )
#define MAX_ZOOM                  ( 10.0f )

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

#if defined ( FO_WINDOWS )
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
#else // FO_LINUX
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

#if defined ( FO_WINDOWS )
# define PTW32_STATIC_LIB
# include "PthreadWnd/pthread.h"
#else // FO_LINUX
# include <pthread.h>
#endif

#if defined ( FO_MSVC )
# pragma comment( lib,"pthreadVC2.lib" )
#elif defined ( FO_GCC )
// Linker option:
//  Windows
//  -lpthreadGC2
#endif

class Thread
{
private:
    bool           isStarted;
    pthread_t      threadId;
    pthread_attr_t threadAttr;

public:
    Thread(): isStarted( false ) { pthread_attr_init( &threadAttr ); }
    ~Thread() { pthread_attr_destroy( &threadAttr ); }
    bool Start( void*( *func )(void*) )
    {
        isStarted = ( pthread_create( &threadId, &threadAttr, func, NULL ) == 0 );
        return isStarted;
    }
    bool Start( void*( *func )(void*), void* arg )
    {
        isStarted = ( pthread_create( &threadId, &threadAttr, func, arg ) == 0 );
        return isStarted;
    }
    void Wait()
    {
        if( isStarted ) pthread_join( threadId, NULL );
        isStarted = false;
    }
    void Finish()
    {
        if( isStarted ) pthread_cancel( threadId );
        isStarted = false;
    }

    #if defined ( FO_WINDOWS )
    uint   GetId()     { GetThreadId( pthread_getw32threadhandle_np( threadId ) ); }
    HANDLE GetHandle() { return pthread_getw32threadhandle_np( threadId ); }
    #else // FO_LINUX
    uint GetId() { return (uint) threadId; }
    #endif
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

#endif // __COMMON__
