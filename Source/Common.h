#ifndef __COMMON__
#define __COMMON__

// Some platform specific definitions
#include "PlatformSpecific.h"

// API
#if defined(FO_WINDOWS)
	#define WINVER 0x0501 // Windows XP
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
#else // FO_LINUX
	//
#endif

// Network
#if defined(FONLINE_CLIENT) || defined(FONLINE_SERVER)
	#if defined(FO_WINDOWS)
		#include <winsock2.h>
		#if defined(FO_MSVC)
            #pragma comment(lib,"Ws2_32.lib")
        #elif defined(FO_GCC)
            // Linker option: -lws2_32
        #endif
		const char* GetLastSocketError();
	#else // FO_LINUX
		// Todo: epoll/kqueue, libevent
	#endif
#endif

// STL Port
#if defined(FO_MSVC)
	#ifndef _DEBUG
		#pragma comment(lib,"stlport_static.lib")
	#else
		#pragma comment(lib,"stlportd_static.lib")
	#endif
#else FO_GCC
	// Linker option: -lstlport
	// Define: _STLP_USE_STATIC_LIB
#endif

// Standart stuff
#include <stdio.h>
#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <deque>
#include <algorithm>
#include <math.h>
using namespace std;

// Memory debug
#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <stdlib.h>
	#include <crtdbg.h>
#endif

// FOnline stuff
#include "Defines.h"
#include "Log.h"
#include "Timer.h"
#include "Debugger.h"
#include "Exception.h"
#include "FlexRect.h"
#include "Randomizer.h"
#include "Mutex.h"
#include "Text.h"

#define ___MSG1(x) #x
#define ___MSG0(x) ___MSG1(x)
#define MESSAGE(desc) message(__FILE__ "(" ___MSG0(__LINE__) "):" #desc)

#define SAFEREL(x)  {if(x) (x)->Release(); (x)=NULL;}
#define SAFEDEL(x)  {if(x) delete (x);     (x)=NULL;}
#define SAFEDELA(x) {if(x) delete[] (x);   (x)=NULL;}

#define STATIC_ASSERT(a) {static int static_assert_array__[(a)?1:-1];}
#define D3D_HR(expr)     {HRESULT hr__=expr; if(hr__!=D3D_OK){WriteLog(_FUNC_," - "#expr", error<%s - %s>.\n",DXGetErrorString(hr__),DXGetErrorDescription(hr__)); return 0;}}

#define PI_FLOAT       (3.14159265f)
#define PIBY2_FLOAT    (1.5707963f)
#define SQRT3T2_FLOAT  (3.4641016151f)
#define SQRT3_FLOAT    (1.732050807568877f)
#define BIAS_FLOAT     (0.02f)
#define RAD2DEG        (57.29577951f)

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#define OFFSETOF(type,member)   ((int)offsetof(type,member))
#define memzero(ptr,size)       memset(ptr,0,size)
#define PACKUINT64(u32hi,u32lo) (((uint64)u32hi<<32)|((uint64)u32lo))

typedef vector<INTRECT> IntRectVec;
typedef vector<FLTRECT> FltRectVec;

extern Randomizer DefaultRandomizer;
int Random(int minimum, int maximum);

int Procent(int full, int peace);
uint NumericalNumber(uint num);
uint DistSqrt(int x1, int y1, int x2, int y2);
uint DistGame(int x1, int y1, int x2, int y2);
int GetNearDir(int x1, int y1, int x2, int y2);
int GetFarDir(int x1, int y1, int x2, int y2);
int GetFarDir(int x1, int y1, int x2, int y2, float offset);
bool CheckDist(ushort x1, ushort y1, ushort x2, ushort y2, uint dist);
int ReverseDir(int dir);
void GetStepsXY(float& sx, float& sy, int x1, int y1, int x2, int y2);
void ChangeStepsXY(float& sx, float& sy, float deq);
bool MoveHexByDir(ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy);
void MoveHexByDirUnsafe(int& hx, int& hy, uchar dir);
bool IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2);
void RestoreMainDirectory();

// Containers comparator template
template<class T> inline bool CompareContainers(const T& a, const T& b){return a.size()==b.size() && (a.empty() || !memcmp(&a[0],&b[0],a.size()*sizeof(a[0])));}

// Hex offsets
#define MAX_HEX_OFFSET    (50) // Must be not odd
void GetHexOffsets(bool odd, short*& sx, short*& sy);
void GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y);

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

#define MODE_WIDTH				(GameOpt.ScreenWidth)
#define MODE_HEIGHT				(GameOpt.ScreenHeight)
#define WM_FLASH_WINDOW			(WM_USER+1) // Chat notification
#define DI_BUF_SIZE             (64)
#define DI_ONDOWN(a,b)          if((didod[i].dwOfs==a) && (didod[i].dwData&0x80)) {b;}
#define DI_ONUP(a,b)            if((didod[i].dwOfs==a) && !(didod[i].dwData&0x80)) {b;}
#define DI_ONMOUSE(a,b)         if(didod[i].dwOfs==a) {b;}

#ifdef FONLINE_CLIENT
	#include "ResourceClient.h"
	#define WINDOW_CLASS_NAME   "FOnline"
	#define WINDOW_NAME         "FOnline"
	#define WINDOW_NAME_SP      "FOnline Singleplayer"
	#define CFG_DEF_INT_FILE    "default800x600.ini"
#else // FONLINE_MAPPER
	#include "ResourceMapper.h"
	#define WINDOW_CLASS_NAME   "FOnline Mapper"
	#define WINDOW_NAME         "FOnline Mapper"
	const uchar SELECT_ALPHA		=100;
	#define CFG_DEF_INT_FILE    "mapper_default.ini"
#endif

#define PATH_MAP_FLAGS		".\\Data\\maps\\"
#define PATH_TEXT_FILES		".\\Data\\text\\"
#define PATH_LOG_FILE		".\\"
#define PATH_SCREENS_FILE	".\\"

#ifdef FONLINE_CLIENT
	// Sound, video
	#include <audiodefs.h>
	#include "Dx9/dsound.h"
	#include "Dx8/DShow.h"
	#pragma comment(lib,"9_dsound.lib")
	#pragma comment(lib,"8_strmiids.lib")
	#pragma comment(lib,"8_quartz.lib")
#endif

#define DIRECTINPUT_VERSION 0x0800
#include "Dx9/dinput.h"
#include "Dx9/dxerr.h"
#include "Dx9/d3dx9.h"

#ifndef D3D_DEBUG_INFO
	#pragma comment(lib,"9_d3dx9.lib")
#else
	#pragma comment(lib,"9_d3dx9d.lib")
#endif
#pragma comment(lib,"9_d3d9.lib")
#pragma comment(lib,"9_dinput8.lib")
#pragma comment(lib,"9_dxguid.lib")
#pragma comment(lib,"9_dxerr.lib")
#pragma comment(lib,"9_d3dxof.lib")
typedef LPDIRECT3D9 LPDIRECT3D;
typedef LPDIRECT3DDEVICE9 LPDIRECT3DDEVICE;
typedef LPDIRECT3DTEXTURE9 LPDIRECT3DTEXTURE;
typedef LPDIRECT3DSURFACE9 LPDIRECT3DSURFACE;
typedef LPDIRECT3DVERTEXBUFFER9 LPDIRECT3DVERTEXBUFFER;
typedef LPDIRECT3DINDEXBUFFER9 LPDIRECT3DINDEXBUFFER;
typedef D3DMATERIAL9 D3DMATERIAL;
#define Direct3DCreate Direct3DCreate9

uint GetColorDay(int* day_time, uchar* colors, int game_time, int* light);
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

#include "Script.h"
#include "ThreadSync.h"
#include "Jobs.h"

extern volatile bool FOAppQuit;
extern volatile bool FOQuit;
extern MutexEvent UpdateEvent;
extern MutexEvent LogEvent;
extern int ServerGameSleep;
extern int MemoryDebugLevel;
extern uint VarsGarbageTime;
extern bool WorldSaveManager;
extern bool LogicMT;

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

#ifdef FO_WINDOWS
struct WSAOVERLAPPED_EX
{
	WSAOVERLAPPED OV;
	Mutex Locker;
	void* PClient;
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
#endif

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
	ushort YearStart;
	uint64 YearStartFT;
	ushort Year;
	ushort Month;
	ushort Day;
	ushort Hour;
	ushort Minute;
	ushort Second;
	uint FullSecondStart;
	uint FullSecond;
	ushort TimeMultiplier;
	uint GameTimeTick;

	bool DisableTcpNagle;
	bool DisableZlibCompression;
	uint FloodSize;
	bool FreeExp;
	bool RegulatePvP;
	bool NoAnswerShuffle;
	bool DialogDemandRecheck;
	uint FixBoyDefaultExperience;
	uint SneakDivider;
	uint LevelCap;
	bool LevelCapAddExperience;
	uint LookNormal;
	uint LookMinimum;
	uint GlobalMapMaxGroupCount;
	uint CritterIdleTick;
	uint TurnBasedTick;
	int DeadHitPoints;
	uint Breaktime;
	uint TimeoutTransfer;
	uint TimeoutBattle;
	uint ApRegeneration;
	uint RtApCostCritterWalk;
	uint RtApCostCritterRun;
	uint RtApCostMoveItemContainer;
	uint RtApCostMoveItemInventory;
	uint RtApCostPickItem;
	uint RtApCostDropItem;
	uint RtApCostReloadWeapon;
	uint RtApCostPickCritter;
	uint RtApCostUseItem;
	uint RtApCostUseSkill;
	bool RtAlwaysRun;
	uint TbApCostCritterMove;
	uint TbApCostMoveItemContainer;
	uint TbApCostMoveItemInventory;
	uint TbApCostPickItem;
	uint TbApCostDropItem;
	uint TbApCostReloadWeapon;
	uint TbApCostPickCritter;
	uint TbApCostUseItem;
	uint TbApCostUseSkill;
	bool TbAlwaysRun;
	uint ApCostAimEyes;
	uint ApCostAimHead;
	uint ApCostAimGroin;
	uint ApCostAimTorso;
	uint ApCostAimArms;
	uint ApCostAimLegs;
	bool RunOnCombat;
	bool RunOnTransfer;
	uint GlobalMapWidth;
	uint GlobalMapHeight;
	uint GlobalMapZoneLength;
	uint EncounterTime;
	uint BagRefreshTime;
	uint AttackAnimationsMinDist;
	uint WhisperDist;
	uint ShoutDist;
	int LookChecks;
	uint LookDir[5];
	uint LookSneakDir[5];
	uint LookWeight;
	bool CustomItemCost;
	uint RegistrationTimeout;
	uint AccountPlayTime;
	bool LoggingVars;
	uint ScriptRunSuspendTimeout;
	uint ScriptRunMessageTimeout;
	uint TalkDistance;
	uint MinNameLength;
	uint MaxNameLength;
	uint DlgTalkMinTime;
	uint DlgBarterMinTime;
	uint MinimumOfflineTime;

	bool AbsoluteOffsets;
	uint SkillBegin;
	uint SkillEnd;
	uint TimeoutBegin;
	uint TimeoutEnd;
	uint KillBegin;
	uint KillEnd;
	uint PerkBegin;
	uint PerkEnd;
	uint AddictionBegin;
	uint AddictionEnd;
	uint KarmaBegin;
	uint KarmaEnd;
	uint DamageBegin;
	uint DamageEnd;
	uint TraitBegin;
	uint TraitEnd;
	uint ReputationBegin;
	uint ReputationEnd;

	int ReputationLoved;
	int ReputationLiked;
	int ReputationAccepted;
	int ReputationNeutral;
	int ReputationAntipathy;
	int ReputationHated;

	bool MapHexagonal;
	int MapHexWidth;
	int MapHexHeight;
	int MapHexLineHeight;
	int MapTileOffsX;
	int MapTileOffsY;
	int MapRoofOffsX;
	int MapRoofOffsY;
	int MapRoofSkipSize;
	float MapCameraAngle;
	bool MapSmoothPath;
	string MapDataPrefix;
	int MapDataPrefixRefCounter;

	// Client and Mapper
	bool Quit;
	int MouseX;
	int MouseY;
	int ScrOx;
	int ScrOy;
	bool ShowTile;
	bool ShowRoof;
	bool ShowItem;
	bool ShowScen;
	bool ShowWall;
	bool ShowCrit;
	bool ShowFast;
	bool ShowPlayerNames;
	bool ShowNpcNames;
	bool ShowCritId;
	bool ScrollKeybLeft;
	bool ScrollKeybRight;
	bool ScrollKeybUp;
	bool ScrollKeybDown;
	bool ScrollMouseLeft;
	bool ScrollMouseRight;
	bool ScrollMouseUp;
	bool ScrollMouseDown;
	bool ShowGroups;
	bool HelpInfo;
	bool DebugInfo;
	bool DebugNet;
	bool DebugSprites;
	bool FullScreen;
	bool VSync;
	int FlushVal;
	int BaseTexture;
	int ScreenClear;
	int Light;
	string Host;
	int HostRefCounter;
	uint Port;
	uint ProxyType;
	string ProxyHost;
	int ProxyHostRefCounter;
	uint ProxyPort;
	string ProxyUser;
	int ProxyUserRefCounter;
	string ProxyPass;
	int ProxyPassRefCounter;
	string Name;
	int NameRefCounter;
	string Pass;
	int PassRefCounter;
	int ScrollDelay;
	int ScrollStep;
	bool ScrollCheck;
	int MouseSpeed;
	bool GlobalSound;
	string FoDataPath;
	int FoDataPathRefCounter;
	int Sleep;
	bool MsgboxInvert;
	int ChangeLang;
	uchar DefaultCombatMode;
	bool MessNotify;
	bool SoundNotify;
	bool AlwaysOnTop;
	uint TextDelay;
	uint DamageHitDelay;
	int ScreenWidth;
	int ScreenHeight;
	int MultiSampling;
	bool SoftwareSkinning;
	bool MouseScroll;
	int IndicatorType;
	uint DoubleClickTime;
	uchar RoofAlpha;
	bool HideCursor;
	bool DisableLMenu;
	bool DisableMouseEvents;
	bool DisableKeyboardEvents;
	bool HidePassword;
	string PlayerOffAppendix;
	int PlayerOffAppendixRefCounter;
	int CombatMessagesType;
	bool DisableDrawScreens;
	uint Animation3dSmoothTime;
	uint Animation3dFPS;
	int RunModMul;
	int RunModDiv;
	int RunModAdd;
	float SpritesZoom;
	float SpritesZoomMax;
	float SpritesZoomMin;
	float EffectValues[EFFECT_SCRIPT_VALUES];
	bool AlwaysRun;
	uint AlwaysRunMoveDist;
	uint AlwaysRunUseDist;
	string KeyboardRemap;
	int KeyboardRemapRefCounter;
	uint CritterFidgetTime;
	uint Anim2CombatBegin;
	uint Anim2CombatIdle;
	uint Anim2CombatEnd;

	// Mapper
	string ClientPath;
	int ClientPathRefCounter;
	string ServerPath;
	int ServerPathRefCounter;
	bool ShowCorners;
	bool ShowSpriteCuts;
	bool ShowDrawOrder;
	bool SplitTilesCollection;

	// Engine data
	void (*CritterChangeParameter)(void*,uint);
	void* CritterTypes;

	void* ClientMap;
	uint ClientMapWidth;
	uint ClientMapHeight;

	void* (*GetDrawingSprites)(uint&);
	void* (*GetSpriteInfo)(uint);
	uint (*GetSpriteColor)(uint,int,int,bool);
	bool (*IsSpriteHit)(void*,int,int,bool);

	const char* (*GetNameByHash)(uint);
	uint (*GetHashByName)(const char*);

	// Callbacks
	uint (*GetUseApCost)(void*,void*,uchar);
	uint (*GetAttackDistantion)(void*,void*,uchar);

	GameOptions();
} extern GameOpt;

// ChangeLang
#define CHANGE_LANG_CTRL_SHIFT  (0)
#define CHANGE_LANG_ALT_SHIFT   (1)
// IndicatorType
#define INDICATOR_LINES         (0)
#define INDICATOR_NUMBERS       (1)
#define INDICATOR_BOTH          (2)
// Zoom
#define MIN_ZOOM                (0.2f)
#define MAX_ZOOM                (10.0f)

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
	uint startTick;

public:
	FileLogger(const char* fname);
	~FileLogger();
	void Write(const char* fmt, ...);
};

/************************************************************************/
/* Single player                                                        */
/************************************************************************/

#if defined(FO_WINDOWS)
	class InterprocessData
	{
	public:
		ushort NetPort;
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

	extern HANDLE SingleplayerClientProcess;
#else // FO_LINUX
	// Todo: linux
	class InterprocessData
	{
	public:
		ushort NetPort;
		bool Pause;
	};
#endif

extern bool Singleplayer;
extern InterprocessData SingleplayerData;

/************************************************************************/
/* File system                                                          */
/************************************************************************/

void* FileOpen(const char* fname, bool write);
void FileClose(void* file);
bool FileRead(void* file, void* buf, uint len, uint* rb = NULL);
bool FileWrite(void* file, const void* buf, uint len);
bool FileSetPointer(void* file, int offset, int origin);
// Todo: FileFindFirst, FileFindNext, FileGetTime, FileGetSize, FileDelete
// Todo: replace all fopen/fclose/etc on this functions

/************************************************************************/
/*                                                                      */
/************************************************************************/

// Deprecated stuff
// tiles.lst, items.lst, scenery.lst, walls.lst, misc.lst, intrface.lst, inven.lst
// pid == -1 - interface
// pid == -2 - tiles
// pid == -3 - inventory
string Deprecated_GetPicName(int pid, int type, ushort pic_num);
uint Deprecated_GetPicHash(int pid, int type, ushort pic_num);
void Deprecated_CondExtToAnim2(uchar cond, uchar cond_ext, uint& anim2ko, uint& anim2dead);

#endif // __COMMON__
