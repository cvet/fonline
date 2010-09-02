#ifndef __COMMON__
#define __COMMON__

// Disable some warnings, erase in future
#pragma warning (disable : 4786)
#pragma warning (disable : 4018) // Signed-Unsigned mismatch
#pragma warning (disable : 4244) // Conversion
#pragma warning (disable : 4696) // Unsafe functions

// Memory debug
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

// WinSock
#if defined(FONLINE_CLIENT) || defined(FONLINE_SERVER)
#include <winsock2.h>
#pragma comment(lib,"Ws2_32.lib")
const char* GetLastSocketError();
#endif

// Windows API
#include <windows.h>

// STL
#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <deque>
#include <algorithm>
#include <math.h>
using namespace std;

// FOnline stuff
#include "Defines.h"
#include "Log.h"
#include "Timer.h"
#include "Debugger.h"
#include "Exception.h"
#include "FlexRect.h"

#define ___MSG1(x) #x
#define ___MSG0(x) ___MSG1(x)
#define MESSAGE(desc) message(__FILE__ "(" ___MSG0(__LINE__) "):" #desc)

#define SAFEREL(x)  {if(x) (x)->Release();(x)=NULL;}
#define SAFEDEL(x)  {if(x) delete (x);(x)=NULL;}
#define SAFEDELA(x) {if(x) delete[] (x);(x)=NULL;}

#define STATIC_ASSERT(a) {static int arr[(a)?1:-1];}
#define D3D_HR(expr)     {HRESULT hr__=expr; if(hr__!=D3D_OK){WriteLog(__FUNCTION__" - "#expr", error<%s - %s>.\n",DXGetErrorString(hr__),DXGetErrorDescription(hr__)); return 0;}}

#define PI_FLOAT       (3.14159265f)
#define PIBY2_FLOAT    (1.5707963f)
#define SQRT3T2_FLOAT  (3.4641016151f)
#define SQRT3_FLOAT    (1.732050807568877f)
#define BIAS_FLOAT     (0.02f)
#define RAD2DEG        (57.29577951f)

typedef vector<INTRECT> IntRectVec;
typedef vector<FLTRECT> FltRectVec;

int Random(int minimum, int maximum);
int Procent(int full, int peace);
DWORD DistSqrt(int x1, int y1, int x2, int y2);
DWORD DistGame(int x1, int y1, int x2, int y2);
DWORD NumericalNumber(DWORD num);
int NextLevel(int cur_level);
int GetDir(int x1, int y1, int x2, int y2);
int GetFarDir(int x1, int y1, int x2, int y2);
bool GetCoords(WORD x1, WORD y1, BYTE ori, WORD& x2, WORD& y2);
bool CheckDist(WORD x1, WORD y1, WORD x2, WORD y2, DWORD dist);
int ReverseDir(int dir);
void GetStepsXY(float& sx, float& sy, int x1, int y1, int x2, int y2);
void ChangeStepsXY(float& sx, float& sy, float deq);
bool MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, WORD maxhx, WORD maxhy);
void MoveHexByDirUnsafe(int& hx, int& hy, BYTE dir);
bool IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2);

// Hex offset
#define MAX_HEX_OFFSET    (50) // Must be not odd
#define HEX_OFFSET_SIZE   ((MAX_HEX_OFFSET*MAX_HEX_OFFSET/2+MAX_HEX_OFFSET/2)*6) // 7650
extern short SXEven[HEX_OFFSET_SIZE];
extern short SYEven[HEX_OFFSET_SIZE];
extern short SXOdd[HEX_OFFSET_SIZE];
extern short SYOdd[HEX_OFFSET_SIZE];

// Relief
const float GlobalMapKRelief[16]={1.5f,1.4f,1.3f,1.2f,1.1f,1.0f,0.95f,0.9f,0.85f,0.8f,0.75f,0.7f,0.65f,0.6f,0.55f,1.0f};
//const float GlobalMapKRelief[16]={1.0f,0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,0.8f,0.9f,1.0f,1.1f,1.2f,1.3f,1.4f,1.5f};

bool CheckUserName(const char* str);
bool CheckUserPass(const char* str);

// Config files
#define SERVER_CONFIG_FILE      "FOnlineServer.cfg"
#define CLIENT_CONFIG_FILE      "FOnline.cfg"
#define CLIENT_CONFIG_APP       "Game Options"
#define MAPPER_CONFIG_FILE      "Mapper.cfg"

/************************************************************************/
/* Client & Mapper                                                      */
/************************************************************************/
#if defined(FONLINE_CLIENT) || defined(FONLINE_MAPPER)

#define MODE_WIDTH				(OptScreenWidth)
#define MODE_HEIGHT				(OptScreenHeight)
#define WM_FLASH_WINDOW			(WM_USER+1) // Chat notification
#define DI_BUF_SIZE             (64)
#define DI_ONDOWN(a,b)          if((didod[i].dwOfs==a) && (didod[i].dwData&0x80)) {b;}
#define DI_ONUP(a,b)            if((didod[i].dwOfs==a) && !(didod[i].dwData&0x80)) {b;}
#define DI_ONMOUSE(a,b)         if(didod[i].dwOfs==a) {b;}
#define GAME_TIME

#ifdef FONLINE_CLIENT
	#include "ResourceClient.h"
	#define WINDOW_CLASS_NAME   "FOnline"
	#define WINDOW_NAME         "FOnline"
	#define WINDOW_NAME_SP      "FOnline Singleplayer"
	#define CFG_DEF_INT_FILE    "default800x600.ini"
#else
	#include "ResourceMapper.h"
	#define WINDOW_CLASS_NAME   "FOnline Mapper"
	#define WINDOW_NAME         "FOnline Mapper"
	const BYTE SELECT_ALPHA		=100;
	#define CFG_DEF_INT_FILE    "mapper_default.ini"
#endif

#define PATH_MAP_FLAGS		".\\Data\\maps\\"
#define PATH_TEXT_FILES		".\\Data\\text\\"
#define PATH_LOG_FILE		".\\"
#define PATH_SCREENS_FILE	".\\"

#define DIRECTINPUT_VERSION 0x0800
#include "Dx9/d3dx9.h"
#include "Dx9/dinput.h"
#include "Dx9/dxerr.h"
#include "Dx9/dsound.h"
#include "Dx8/DShow.h"
#pragma comment(lib,"9_d3dx9.lib")
#pragma comment(lib,"9_d3d9.lib")
#pragma comment(lib,"9_dinput8.lib")
#pragma comment(lib,"9_dxguid.lib")
#pragma comment(lib,"9_dxerr.lib")
#pragma comment(lib,"9_dsound.lib")
#pragma comment(lib,"8_strmiids.lib")
#pragma comment(lib,"8_quartz.lib")
#pragma comment(lib,"9_d3dxof.lib")
typedef LPDIRECT3D9 LPDIRECT3D;
typedef LPDIRECT3DDEVICE9 LPDIRECT3DDEVICE;
typedef LPDIRECT3DTEXTURE9 LPDIRECT3DTEXTURE;
typedef LPDIRECT3DSURFACE9 LPDIRECT3DSURFACE;
typedef LPDIRECT3DVERTEXBUFFER9 LPDIRECT3DVERTEXBUFFER;
typedef LPDIRECT3DINDEXBUFFER9 LPDIRECT3DINDEXBUFFER;
typedef D3DMATERIAL9 D3DMATERIAL;
#define Direct3DCreate Direct3DCreate9

extern bool CmnQuit;
extern int CmnScrOx;
extern int CmnScrOy;
extern bool CmnDiLeft;
extern bool CmnDiRight;
extern bool CmnDiUp;
extern bool CmnDiDown;
extern bool CmnDiMleft;
extern bool CmnDiMright;
extern bool CmnDiMup;
extern bool CmnDiMdown;

extern bool OptShowTile;
extern bool OptShowRoof;
extern bool OptShowItem;
extern bool OptShowScen;
extern bool OptShowWall;
extern bool OptShowCrit;
extern bool OptShowFast;
extern bool CmnShowPlayerNames;
extern bool CmnShowNpcNames;
extern bool CmnShowCritId;

extern bool OptShowGroups;
extern bool OptHelpInfo;
extern bool OptDebugInfo;
extern bool OptDebugNet;
extern bool OptDebugSprites;
extern bool OptFullScr;
extern bool OptVSync;
extern int OptFlushVal;
extern int OptBaseTex;
extern int OptScreenClear;
extern int OptLight;
extern int OptScrollDelay;
extern int OptScrollStep;
extern bool OptMouseScroll;
extern bool OptScrollCheck;
extern int OptMouseSpeed;
extern bool OptGlobalSound;
extern string OptMasterPath;
extern string OptCritterPath;
extern string OptFoPatchPath;
extern string OptFoDataPath;
extern string OptHost;
extern DWORD OptPort;
extern DWORD OptProxyType;
extern string OptProxyHost;
extern DWORD OptProxyPort;
extern string OptProxyUser;
extern string OptProxyPass;
extern string OptName;
extern string OptPass;
extern DWORD OptTextDelay;
extern DWORD OptDamageHitDelay;
extern bool OptAlwaysOnTop;
extern int OptScreenWidth;
extern int OptScreenHeight;
extern int OptMultiSampling;
extern bool OptSoftwareSkinning;
extern int OptSleep;
extern bool OptMsgboxInvert;
extern int OptChangeLang;
extern BYTE OptDefaultCombatMode;
#define CHANGE_LANG_CTRL_SHIFT  (0)
#define CHANGE_LANG_ALT_SHIFT   (1)
extern bool OptMessNotify;
extern bool OptSoundNotify;
extern int OptIndicatorType;
#define INDICATOR_LINES         (0)
#define INDICATOR_NUMBERS       (1)
#define INDICATOR_BOTH          (2)
extern DWORD OptDoubleClickTime;
extern BYTE OptRoofAlpha;
extern bool OptHideCursor;
extern bool OptDisableLMenu;
extern bool OptDisableMouseEvents;
extern bool OptDisableKeyboardEvents;
extern string OptPlayerOffAppendix;
extern int OptCombatMessagesType;

#ifdef FONLINE_MAPPER
extern string OptClientPath;
extern string OptServerPath;
#endif

//extern bool OptWeightFount;
//#define GAME_MASS(x) (OptWeightFount?((x)/453):(x))

DWORD GetColorDay(int* day_time, BYTE* colors, int game_time, int* light);
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
} extern MapperFunctions;

#endif
/************************************************************************/
/* Server                                                               */
/************************************************************************/
#ifdef FONLINE_SERVER

#include <richedit.h>
#include "Script.h"

#define ITEMS_STATISTICS
//#define FOSERVER_DUMP
#define RADIO_SAFE
#define GAME_TIME

struct WSAOVERLAPPED_EX
{
	WSAOVERLAPPED OV;
	CRITICAL_SECTION CS;
	void* Client;
	WSABUF Buffer;
	long Operation;
	DWORD Flags;
	DWORD Bytes;
};
#define WSA_BUF_SIZE       (4096)
#define WSAOP_END          (0)
#define WSAOP_FREE         (1)
#define WSAOP_SEND         (2)
#define WSAOP_RECV         (3)
typedef void(*WSASendCallback)(WSAOVERLAPPED_EX*);

extern bool FOAppQuit;
extern volatile bool FOQuit;
extern HANDLE UpdateEvent;
extern HANDLE LogEvent;
extern int ServerGameSleep;
extern int MemoryDebugLevel;
extern DWORD VarsGarbageTime;
extern bool WorldSaveManager;

void GetServerOptions();

struct ServerScriptFunctions
{
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
} extern ServerFunctions;

#endif
/************************************************************************/
/* Npc editor                                                           */
/************************************************************************/
#ifdef FONLINE_NPCEDITOR
#include <strstream>
#include <fstream>

#define _CRT_SECURE_NO_DEPRECATE
#define MAX_TEXT_DIALOG      (1000)
#endif
/************************************************************************/
/* Game options                                                         */
/************************************************************************/

struct GameOptions
{
	WORD YearStart;
	ULONGLONG YearStartFT;
	WORD Year;
	WORD Month;
	WORD Day;
	WORD Hour;
	WORD Minute;
	WORD Second;
	DWORD FullSecondStart;
	DWORD FullSecond;
	WORD TimeMultiplier;
	DWORD GameTimeTick;

	bool DisableTcpNagle;
	bool DisableZlibCompression;
	DWORD FloodSize;
	bool FreeExp;
	bool RegulatePvP;
	bool NoAnswerShuffle;
	DWORD ForceDialog;
	bool DialogDemandRecheck;
	DWORD FixBoyDefaultExperience;
	DWORD SneakDivider;
	DWORD LevelCap;
	bool LevelCapAddExperience;
	DWORD LookNormal;
	DWORD LookMinimum;
	DWORD GlobalMapMaxGroupCount;
	DWORD CritterIdleTick;
	DWORD TurnBasedTick;
	int DeadHitPoints;
	DWORD Breaktime;
	DWORD TimeoutTransfer;
	DWORD TimeoutBattle;
	DWORD ApRegeneration;
	DWORD RtApCostCritterWalk;
	DWORD RtApCostCritterRun;
	DWORD RtApCostMoveItemContainer;
	DWORD RtApCostMoveItemInventory;
	DWORD RtApCostPickItem;
	DWORD RtApCostDropItem;
	DWORD RtApCostReloadWeapon;
	DWORD RtApCostPickCritter;
	DWORD RtApCostUseItem;
	DWORD RtApCostUseSkill;
	bool RtAlwaysRun;
	DWORD TbApCostCritterMove;
	DWORD TbApCostMoveItemContainer;
	DWORD TbApCostMoveItemInventory;
	DWORD TbApCostPickItem;
	DWORD TbApCostDropItem;
	DWORD TbApCostReloadWeapon;
	DWORD TbApCostPickCritter;
	DWORD TbApCostUseItem;
	DWORD TbApCostUseSkill;
	bool TbAlwaysRun;
	DWORD ApCostAimEyes;
	DWORD ApCostAimHead;
	DWORD ApCostAimGroin;
	DWORD ApCostAimTorso;
	DWORD ApCostAimArms;
	DWORD ApCostAimLegs;
	DWORD HitAimEyes;
	DWORD HitAimHead;
	DWORD HitAimGroin;
	DWORD HitAimTorso;
	DWORD HitAimArms;
	DWORD HitAimLegs;
	bool RunOnCombat;
	bool RunOnTransfer;
	DWORD GlobalMapWidth;
	DWORD GlobalMapHeight;
	DWORD GlobalMapZoneLength;
	DWORD BagRefreshTime;
	DWORD AttackAnimationsMinDist;
	DWORD WhisperDist;
	DWORD ShoutDist;
	int LookChecks;
	DWORD LookDir[4];
	DWORD LookSneakDir[4];
	DWORD LookWeight;
	bool CustomItemCost;
	DWORD RegistrationTimeout;
	DWORD AccountPlayTime;
	bool LoggingVars;
	DWORD ScriptRunSuspendTimeout;
	DWORD ScriptRunMessageTimeout;
	DWORD TalkDistance;
	DWORD MinNameLength;
	DWORD MaxNameLength;

	bool AbsoluteOffsets;
	DWORD SkillBegin;
	DWORD SkillEnd;
	DWORD TimeoutBegin;
	DWORD TimeoutEnd;
	DWORD KillBegin;
	DWORD KillEnd;
	DWORD PerkBegin;
	DWORD PerkEnd;
	DWORD AddictionBegin;
	DWORD AddictionEnd;
	DWORD KarmaBegin;
	DWORD KarmaEnd;
	DWORD DamageBegin;
	DWORD DamageEnd;
	DWORD TraitBegin;
	DWORD TraitEnd;
	DWORD ReputationBegin;
	DWORD ReputationEnd;

	int ReputationLoved;
	int ReputationLiked;
	int ReputationAccepted;
	int ReputationNeutral;
	int ReputationAntipathy;
	int ReputationHated;

	// Client
	string UserInterface;
	bool DisableDrawScreens;
	DWORD Animation3dSmoothTime;
	DWORD Animation3dFPS;
	int RunModMul;
	int RunModDiv;
	int RunModAdd;

	GameOptions();
} extern GameOpt;

/************************************************************************/
/* Game time                                                            */
/************************************************************************/

DWORD GetFullSecond(WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second);
SYSTEMTIME GetGameTime(DWORD full_second);
DWORD GameTimeMonthDay(WORD year, WORD month);
void ProcessGameTime();
DWORD GameAimApCost(int hit_location);
DWORD GameHitAim(int hit_location);

/************************************************************************/
/* Mutex                                                                */
/************************************************************************/

class Mutex
{
private:
	CRITICAL_SECTION mutexCS;
	Mutex(const Mutex&){}
	void operator=(const Mutex&){}

public:
	Mutex(){InitializeCriticalSection(&mutexCS);}
	~Mutex(){DeleteCriticalSection(&mutexCS);}
	void Lock(){EnterCriticalSection(&mutexCS);}
	void Unlock(){LeaveCriticalSection(&mutexCS);}

	class Unlocker
	{
	private:
		CRITICAL_SECTION* pCS;

	public:
		Unlocker():pCS(NULL){}
		Unlocker(Unlocker& unlock){unlock.pCS=pCS; pCS=NULL;}
		Unlocker(CRITICAL_SECTION& cs){pCS=&cs; EnterCriticalSection(pCS);}
		Unlocker& operator=(Unlocker& unlock){unlock.pCS=pCS; pCS=NULL; return *this;}
		~Unlocker(){if(pCS) LeaveCriticalSection(pCS);}
	};

	Unlocker AutoLock(){return Unlocker(mutexCS);}
};

/************************************************************************/
/* Auto pointers                                                        */
/************************************************************************/

template<class T>
class AutoPtr
{
public:
	AutoPtr(T* ptr):Ptr(ptr){}
	~AutoPtr(){if(Ptr) delete Ptr;}
	T& operator*() const {return *Get();}
	T* operator->() const {return Get();}
	T* Get() const {return Ptr;}
	T* Release(){T* tmp=Ptr; Ptr=NULL; return tmp;}
	void Reset(T* ptr){if(ptr!=Ptr && Ptr!=0) delete Ptr; Ptr=ptr;}
	bool IsValid() const {return Ptr!=NULL;}

private:
	T* Ptr;
};

template<class T>
class AutoPtrArr
{
public:
	AutoPtrArr(T* ptr):Ptr(ptr){}
	~AutoPtrArr(){if(Ptr) delete[] Ptr;}
	T& operator*() const {return *Get();}
	T* operator->() const {return Get();}
	T* Get() const {return Ptr;}
	T* Release(){T* tmp=Ptr; Ptr=NULL; return tmp;}
	void Reset(T* ptr){if(ptr!=Ptr && Ptr!=0) delete[] Ptr; Ptr=ptr;}
	bool IsValid() const {return Ptr!=NULL;}

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
	DWORD startTick;

public:
	FileLogger(const char* fname);
	~FileLogger();
	void Write(const char* fmt, ...);
};

/************************************************************************/
/* Safe string functions                                                */
/************************************************************************/

void StringCopy(char* to, size_t size, const char* from);
template<int Size> inline void StringCopy(char (&to)[Size], const char* from){return StringCopy(to,Size,from);}
void StringAppend(char* to, size_t size, const char* from);
template<int Size> inline void StringAppend(char (&to)[Size], const char* from){return StringAppend(to,Size,from);}
char* StringDuplicate(const char* str);

/************************************************************************/
/* Single player                                                        */
/************************************************************************/

class InterprocessData
{
public:
	WORD NetPort;
	bool Pause;

private:
	HANDLE mapFileMutex;
	HANDLE mapFile;
	void* mapFilePtr;

public:
	HANDLE Init();
	void Finish();
	bool Attach(HANDLE map_file);
	bool Lock();
	void Unlock();
	bool Refresh();
};

extern bool Singleplayer;
extern InterprocessData SingleplayerData;
extern HANDLE SingleplayerClientProcess;

/************************************************************************/
/*                                                                      */
/************************************************************************/

// Deprecated stuff
// tiles.lst, items.lst, scenery.lst, walls.lst, misc.lst, intrface.lst, inven.lst
// pid == -1 - interface
// pid == -2 - tiles
// pid == -3 - inventory
string Deprecated_GetPicName(int pid, int type, WORD pic_num);
DWORD Deprecated_GetPicHash(int pid, int type, WORD pic_num);

#endif // __COMMON__