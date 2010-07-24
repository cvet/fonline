#ifndef __CLIENT__
#define __CLIENT__

#include "Keyboard.h"
#include "Common.h"
#include "SpriteManager.h"
#include "SoundManager.h"
#include "HexManager.h"
#include "Item.h"
#include "ItemManager.h"
#include "CritterCl.h"
#include "NetProtocol.h"
#include "BufferManager.h"
#include "Text.h"
#include "QuestManager.h"
#include "CraftManager.h"
#include "Names.h"
#include "ResourceManager.h"
#include "CritterType.h"
#include "DataMask.h"
#include "Script.h"

class FOClient
{
public:
	static FOClient* Self;
	bool Init(HWND hwnd);
	void Clear();
	void TryExit();
	bool IsScroll(){return CmnDiMup || CmnDiMright || CmnDiMdown || CmnDiMleft || CmnDiUp || CmnDiRight || CmnDiDown || CmnDiLeft;}
	void ProcessMouseScroll();
	void ProcessKeybScroll(bool down, BYTE dik);
	void DropScroll(){CmnDiMup=false;CmnDiMright=false;CmnDiMdown=false;CmnDiMleft=false;CmnDiUp=false;CmnDiRight=false;CmnDiDown=false;CmnDiLeft=false;}
	bool IsCurInWindow();
	int MainLoop();
	void NetDisconnect();
	WORD NetState;
	FOClient();
	bool Active;
	DWORD* UID1;

	int ShowScreenType;
	DWORD ShowScreenParam;
	bool ShowScreenNeedAnswer;

	int ScreenModeMain;
	void ShowMainScreen(int new_screen);
	int GetMainScreen(){return ScreenModeMain;}
	bool IsMainScreen(int check_screen){return check_screen==ScreenModeMain;}

	void ShowScreen(int screen, int p0 = -1, int p1 = 0, int p2 = 0);
	void HideScreen(int screen, int p0 = -1, int p1 = 0, int p2 = 0);
	int GetActiveScreen(IntVec** screens = NULL);
	bool IsScreenPresent(int screen);
	void RunScreenScript(bool show, int screen, int p0, int p1, int p2);

	int CurMode,CurModeLast;
	void SetCurMode(int new_cur);
	int GetCurMode(){return CurMode;};
	void SetCurCastling(int cur1, int cur2);
	void SetLastCurMode();
	bool IsCurMode(int check_cur){return (check_cur==CurMode);}
	void SetCurPos(int x, int y);

	HWND Wnd;
	LPDIRECTINPUT8 DInput;
	LPDIRECTINPUTDEVICE8 Keyboard;
	LPDIRECTINPUTDEVICE8 Mouse;
	FileManager FileMngr;
	SpriteManager SprMngr;
	HexManager HexMngr;
	IniParser IfaceIni;

	int InitDInput();
	void ParseKeyboard();
	void ParseMouse();

	char* ComBuf;
	DWORD ComLen;
	BufferManager Bin;
	BufferManager Bout;
	z_stream ZStream;
	bool ZStreamOk;
	DWORD BytesReceive,BytesRealReceive,BytesSend;
	SOCKADDR_IN SockAddr,ProxyAddr;
	SOCKET Sock;
	fd_set SockSet,SockSetErr;
	DWORD* UID0;
	bool UIDFail;
	Item SomeItem;

	bool InitNet();
	bool FillSockAddr(SOCKADDR_IN& saddr, const char* host, WORD port);
	bool NetConnect();
	void ParseSocket();
	int NetInput(bool unpack);
	bool NetOutput();
	void NetProcess();

	void Net_SendLogIn(const char* name, const char* pass);
	void Net_SendCreatePlayer(CritterCl* newcr);
	void Net_SendUseSkill(BYTE skill, CritterCl* cr);
	void Net_SendUseSkill(BYTE skill, ItemHex* item);
	void Net_SendUseSkill(BYTE skill, Item* item);
	void Net_SendUseItem(BYTE ap, DWORD item_id, WORD item_pid, BYTE rate, BYTE target_type, DWORD target_id, WORD target_pid, DWORD param);
	void Net_SendPickItem(WORD targ_x, WORD targ_y, WORD pid);
	void Net_SendPickCritter(DWORD crid, BYTE pick_type);
	void Net_SendChangeItem(BYTE ap, DWORD item_id, BYTE from_slot, BYTE to_slot, DWORD count);
	void Net_SendItemCont(BYTE transfer_type, DWORD cont_id, DWORD item_id, DWORD count, BYTE take_flags);
	void Net_SendRateItem();
	void Net_SendSortValueItem(Item* item);
	void Net_SendTalk(BYTE is_npc, DWORD id_to_talk, BYTE answer);
	void Net_SendSayNpc(BYTE is_npc, DWORD id_to_talk, const char* str);
	void Net_SendBarter(DWORD npc_id, ItemVec& cont_sale, ItemVec& cont_buy);
	void Net_SendGetGameInfo();
	void Net_SendGiveGlobalInfo(BYTE info_flags);
	void Net_SendRuleGlobal(BYTE command, DWORD param1=0, DWORD param2=0);
	void Net_SendGiveMap(bool automap, WORD map_pid, DWORD loc_id, DWORD tiles_hash, DWORD walls_hash, DWORD scen_hash);
	void Net_SendLoadMapOk();
	void Net_SendCommand(char* str);
	void Net_SendText(const char* send_str, BYTE how_say);
	void Net_SendDir();
    void Net_SendMove(ByteVec steps);
	void Net_SendRadio();
	void Net_SendLevelUp(WORD perk_up);
	void Net_SendCraftAsk(DwordVec numbers);
	void Net_SendCraft(DWORD craft_num);
	void Net_SendPing(BYTE ping);
	void Net_SendPlayersBarter(BYTE barter, DWORD param, DWORD param_ext);
	void Net_SendScreenAnswer(DWORD answer_i, const char* answer_s);
	void Net_SendGetScores();
	void Net_SendSetUserHoloStr(Item* holodisk, const char* title, const char* text);
	void Net_SendGetUserHoloStr(DWORD str_num);
	void Net_SendCombat(BYTE type, int val);
	void Net_SendRunScript(bool unsafe, const char* func_name, int p0, int p1, int p2, const char* p3, DwordVec& p4);
	void Net_SendKarmaVoting(DWORD crid, bool val_up);
	void Net_SendRefereshMe();

	void Net_OnLoginSuccess();
	void Net_OnAddCritter(bool is_npc);
	void Net_OnRemoveCritter();
	void Net_OnText();
	void Net_OnTextMsg(bool with_lexems);
	void Net_OnMapText();
	void Net_OnMapTextMsg();
	void Net_OnMapTextMsgLex();
	void Net_OnAddItemOnMap();
	void Net_OnChangeItemOnMap();
	void Net_OnEraseItemFromMap();
	void Net_OnAnimateItem();
	void Net_OnCombatResult();
	void Net_OnEffect();
	void Net_OnFlyEffect();
	void Net_OnPlaySound(bool by_type);
	void Net_OnPing();
	void Net_OnCheckUID0();

	void Net_OnCritterDir();
    void Net_OnCritterMove();
	void Net_OnSomeItem();
	void Net_OnCritterAction();
	void Net_OnCritterKnockout();
	void Net_OnCritterMoveItem();
	void Net_OnCritterItemData();
	void Net_OnCritterAnimate();
	void Net_OnCritterParam();
	void Net_OnCheckUID1();

	void Net_OnCritterXY();
	void Net_OnChosenParams();
	void Net_OnChosenParam();
	void Net_OnCraftAsk();
	void Net_OnCraftResult();
	void Net_OnChosenClearItems();
	void Net_OnChosenAddItem();
	void Net_OnChosenEraseItem();
	void Net_OnChosenTalk();
	void Net_OnCheckUID2();

	void Net_OnGameInfo();
	void Net_OnLoadMap();
	void Net_OnMap();
	void Net_OnGlobalInfo();
	void Net_OnGlobalEntrances();
	void Net_OnContainerInfo();
	void Net_OnFollow();
	void Net_OnPlayersBarter();
	void Net_OnPlayersBarterSetHide();
	void Net_OnShowScreen();
	void Net_OnRunClientScript();
	void Net_OnDropTimers();
	void Net_OnCritterLexems();
	void Net_OnItemLexems();
	void Net_OnCheckUID3();

	void Net_OnMsgData();
	void Net_OnProtoItemData();

	void Net_OnQuest(bool many);
	void Net_OnHoloInfo();
	void Net_OnScores();
	void Net_OnUserHoloStr();
	void Net_OnAutomapsInfo();
	void Net_OnCheckUID4();
	void Net_OnViewMap();

	void OnText(const char* str, DWORD crid, int how_say, WORD intellect);
	void OnMapText(const char* str, WORD hx, WORD hy, DWORD color);

	void WaitPing();

	bool SaveLogFile();
	bool SaveScreenshot();

	ByteVec MoveDirs;
	WORD MoveLastHx,MoveLastHy;
	WORD TargetX,TargetY;
	bool GetMouseHex();

	DWORD PingTime,PingTick,PingCallTick;
	DWORD FPS;

	// Sound
	void SoundProcess();

	// MSG File
	LanguagePack CurLang;
	FOMsg* MsgText,*MsgDlg,*MsgItem,*MsgGame,*MsgGM,*MsgCombat,*MsgQuest,*MsgHolo,*MsgUserHolo,*MsgCraft,*MsgInternal;

	const char* GetHoloText(DWORD str_num);
	const char* FmtGameText(DWORD str_num, ...);
	const char* FmtCombatText(DWORD str_num, ...);

#define DESC_INVENTORY_MAIN    (0)
#define DESC_INVENTORY_SPECIAL (1)
#define DESC_INVENTORY_STATS   (2)
#define DESC_INVENTORY_RESIST  (3)
	const char* FmtGenericDesc(int desc_type, int& ox, int& oy);

#define CRITTER_ONLY_NAME      (0)
#define CRITTER_LOOK_SHORT     (1)
#define CRITTER_LOOK_FULL      (2)
	const char* FmtCritLook(CritterCl* cr, int look_type);

#define ITEM_LOOK_DEFAULT      (0)
#define ITEM_LOOK_ONLY_NAME    (1)
#define ITEM_LOOK_MAP          (2)
#define ITEM_LOOK_BARTER       (3)
#define ITEM_LOOK_INVENTORY    (4)
#define ITEM_LOOK_WM_CAR       (5)
	const char* FmtItemLook(Item* item, int look_type);

	// Intellect text
	PCharPairVec IntellectWords;
	PCharPairVec IntellectSymbols;

	void ParseIntellectWords(char* words, PCharPairVec& text);
	PCharPairVecIt FindIntellectWord(const char* word, PCharPairVec& text);
	void FmtTextIntellect(char* str, WORD intellect);

#define SMTH_NONE         (0)
#define SMTH_CRITTER      (1)
#define SMTH_ITEM         (3)
#define SMTH_CONT_ITEM    (4)
	class SmthSelected
	{
	private:
		int smthType;
		DWORD smthId;
		int smthParam;
	public:
		SmthSelected(){Clear();}
		bool operator!=(const SmthSelected& r){return smthType!=r.smthType || smthId!=r.smthId || smthParam!=r.smthParam;}
		bool operator==(const SmthSelected& r){return smthType==r.smthType && smthId==r.smthId && smthParam==r.smthParam;}
		void Clear(){smthType=SMTH_NONE;smthId=0;}
		bool IsSmth(){return smthType!=SMTH_NONE;}
		bool IsCritter(){return smthType==SMTH_CRITTER;}
		bool IsItem(){return smthType==SMTH_ITEM;}
		bool IsContItem(){return smthType==SMTH_CONT_ITEM;}
		void SetCritter(DWORD id){smthType=SMTH_CRITTER;smthId=id;smthParam=0;}
		void SetItem(DWORD id){smthType=SMTH_ITEM;smthId=id;smthParam=0;}
		void SetContItem(DWORD id, int cont_type){smthType=SMTH_CONT_ITEM;smthId=id;smthParam=cont_type;}
		DWORD GetId(){return smthId;}
		int GetParam(){return smthParam;}
	};
	SmthSelected TargetSmth;
	Item* GetTargetContItem();

	struct ActionEvent
	{
		DWORD Type;
		DWORD Param[6];
		bool operator==(const ActionEvent& r){return Type==r.Type && Param[0]==r.Param[0] && Param[1]==r.Param[1] && Param[2]==r.Param[2] && Param[3]==r.Param[3] && Param[4]==r.Param[4] && Param[4]==r.Param[5];}
		ActionEvent(DWORD type, DWORD param0, DWORD param1, DWORD param2, DWORD param3, DWORD param4, DWORD param5):
			Type(type){Param[0]=param0;Param[1]=param1;Param[2]=param2;Param[3]=param3;Param[4]=param4;Param[5]=param5;}
		ActionEvent(const ActionEvent& r){memcpy(this,&r,sizeof(ActionEvent));}
	};
typedef vector<ActionEvent> ActionEventVec;
typedef vector<ActionEvent>::iterator ActionEventVecIt;

	ActionEventVec ChosenAction;
	void AddAction(bool to_front, ActionEvent& act);
	void SetAction(DWORD type_action, DWORD param0 = 0, DWORD param1 = 0, DWORD param2 = 0, DWORD param3 = 0, DWORD param4 = 0, DWORD param5 = 0);
	void SetAction(ActionEvent& act);
	void AddActionBack(DWORD type_action, DWORD param0 = 0, DWORD param1 = 0, DWORD param2 = 0, DWORD param3 = 0, DWORD param4 = 0, DWORD param5 = 0);
	void AddActionBack(ActionEvent& act);
	void AddActionFront(DWORD type_action, DWORD param0 = 0, DWORD param1 = 0, DWORD param2 = 0, DWORD param3 = 0, DWORD param4 = 0, DWORD param5 = 0);
	void EraseFrontAction();
	void EraseBackAction();
	bool IsAction(DWORD type_action);
	void ChosenChangeSlot();
	void CrittersProcess();
	void TryPickItemOnGround();

/************************************************************************/
/* Video                                                                */
/************************************************************************/
	struct ShowVideo
	{
		string FileName;
		string SoundName;
		bool CanStop;
	};
typedef vector<ShowVideo> ShowVideoVec;

	ShowVideoVec ShowVideos;
	IGraphBuilder* GraphBuilder;
	IMediaControl* MediaControl;
	IVMRWindowlessControl* WindowLess;
	IMediaSeeking* MediaSeeking;
	IBaseFilter* VMRFilter;
	IVMRFilterConfig* FilterConfig;
	IBasicAudio* BasicAudio;
	string MusicAfterVideo;
	int MusicVolumeRestore;

	bool IsVideoPlayed(){return !ShowVideos.empty();}
	bool IsCanStopVideo(){return ShowVideos.size() && ShowVideos[0].CanStop;}
	void AddVideo(const char* fname, const char* sound, bool can_stop, bool clear_sequence);
	void PlayVideo(ShowVideo& video);
	void NextVideo();
	void StopVideo();

/************************************************************************/
/* Animation                                                            */
/************************************************************************/
	struct IfaceAnim
	{
		AnyFrames* Frames;
		WORD Flags;
		DWORD CurSpr;
		DWORD LastTick;
		int ResType;

		IfaceAnim(AnyFrames* frm, int res_type):Frames(frm),Flags(0),CurSpr(0),LastTick(Timer::GameTick()),ResType(res_type){}
	};
typedef vector<IfaceAnim*> IfaceAnimVec;
typedef vector<IfaceAnim*>::iterator IfaceAnimVecIt;

#define ANIMRUN_TO_END         (0x0001)
#define ANIMRUN_FROM_END       (0x0002)
#define ANIMRUN_CYCLE          (0x0004)
#define ANIMRUN_STOP           (0x0008)
#define ANIMRUN_SET_FRM(frm)   ((DWORD(BYTE((frm)+1)))<<16)

	IfaceAnimVec Animations;

	DWORD AnimLoad(DWORD name_hash, BYTE dir, int res_type);
	DWORD AnimLoad(const char* fname, int path_type, int res_type);
	DWORD AnimGetCurSpr(DWORD anim_id);
	DWORD AnimGetCurSprCnt(DWORD anim_id);
	DWORD AnimGetSprCount(DWORD anim_id);
	AnyFrames* AnimGetFrames(DWORD anim_id);
	void AnimRun(DWORD anim_id, DWORD flags);
	void AnimProcess();
	void AnimFree(int res_type);

/************************************************************************/
/* Screen effects                                                       */
/************************************************************************/
	struct ScreenEffect
	{
		DWORD BeginTick;
		DWORD Time;
		DWORD StartColor;
		DWORD EndColor;
		ScreenEffect(DWORD begin_tick, DWORD time, DWORD col, DWORD end_col):BeginTick(begin_tick),Time(time),StartColor(col),EndColor(end_col){}
	};
typedef vector<ScreenEffect> ScreenEffectVec;
typedef vector<ScreenEffect>::iterator ScreenEffectVecIt;

	// Fading
	ScreenEffectVec ScreenEffects;
	// Quake
	int ScreenOffsX,ScreenOffsY;
	float ScreenOffsXf,ScreenOffsYf,ScreenOffsStep;
	DWORD ScreenOffsNextTick;
	// Mirror
	LPDIRECT3DTEXTURE ScreenMirrorTexture;
	int ScreenMirrorX,ScreenMirrorY;
	DWORD ScreenMirrorEndTick;
	bool ScreenMirrorStart;

	void ScreenFadeIn(){ScreenFade(1000,D3DCOLOR_ARGB(0,0,0,0),D3DCOLOR_ARGB(255,0,0,0),false);}
	void ScreenFadeOut(){ScreenFade(1000,D3DCOLOR_ARGB(255,0,0,0),D3DCOLOR_ARGB(0,0,0,0),false);}
	void ScreenFade(DWORD time, DWORD from_color, DWORD to_color, bool push_back);
	void ScreenQuake(int noise, DWORD time);
	void ScreenMirror();
	void ProcessScreenEffectFading();
	void ProcessScreenEffectQuake();
	void ProcessScreenEffectMirror();

/************************************************************************/
/* Scripting                                                            */
/************************************************************************/
	bool ReloadScripts();
	int ScriptGetHitProc(CritterCl* cr, int hit_location);
	void DrawIfaceLayer(DWORD layer);
	static bool PragmaCallbackCrData(const char* text);

	struct SScriptFunc
	{
		static int DataRef_Index(CritterClPtr& cr, DWORD index);
		static int DataVal_Index(CritterClPtr& cr, DWORD index);

		static bool Crit_IsChosen(CritterCl* cr);
		static bool Crit_IsPlayer(CritterCl* cr);
		static bool Crit_IsNpc(CritterCl* cr);
		static bool Crit_IsLife(CritterCl* cr);
		static bool Crit_IsKnockout(CritterCl* cr);
		static bool Crit_IsDead(CritterCl* cr);
		static bool Crit_IsFree(CritterCl* cr);
		static bool Crit_IsBusy(CritterCl* cr);

		static bool Crit_IsAnim3d(CritterCl* cr);
		static bool Crit_IsAnimAviable(CritterCl* cr, DWORD anim1, DWORD anim2);
		static bool Crit_IsAnimPlaying(CritterCl* cr);
		static DWORD Crit_GetAnim1(CritterCl* cr);
		static void Crit_Animate(CritterCl* cr, DWORD anim1, DWORD anim2);
		static void Crit_AnimateEx(CritterCl* cr, DWORD anim1, DWORD anim2, Item* item);
		static void Crit_ClearAnim(CritterCl* cr);
		static void Crit_Wait(CritterCl* cr, DWORD ms);
		static DWORD Crit_ItemsCount(CritterCl* cr);
		static DWORD Crit_ItemsWeight(CritterCl* cr);
		static DWORD Crit_ItemsVolume(CritterCl* cr);
		static DWORD Crit_CountItem(CritterCl* cr, WORD proto_id);
		static DWORD Crit_CountItemByType(CritterCl* cr, BYTE type);
		static Item* Crit_GetItem(CritterCl* cr, WORD proto_id, int slot);
		static DWORD Crit_GetItems(CritterCl* cr, int slot, asIScriptArray* items);
		static DWORD Crit_GetItemsByType(CritterCl* cr, int type, asIScriptArray* items);
		static ProtoItem* Crit_GetSlotProto(CritterCl* cr, int slot);
		static void Crit_SetVisible(CritterCl* cr, bool visible);
		static bool Crit_GetVisible(CritterCl* cr);
		static void Crit_set_ContourColor(CritterCl* cr, DWORD value);
		static DWORD Crit_get_ContourColor(CritterCl* cr);

		static bool Item_IsGrouped(Item* item);
		static bool Item_IsWeared(Item* item);
		static DWORD Item_GetScriptId(Item* item);
		static BYTE Item_GetType(Item* item);
		static WORD Item_GetProtoId(Item* item);
		static DWORD Item_GetCount(Item* item);
		static bool Item_GetMapPosition(Item* item, WORD& hx, WORD& hy);
		static void Item_Animate(Item* item, BYTE from_frame, BYTE to_frame);
		static bool Item_IsCar(Item* item);
		static Item* Item_CarGetBag(Item* item, int num_bag);

		static CritterCl* Global_GetChosen();
		static Item* Global_GetItem(DWORD item_id);
		static DWORD Global_GetCrittersDistantion(CritterCl* cr1, CritterCl* cr2);
		static CritterCl* Global_GetCritter(DWORD critter_id);
		static DWORD Global_GetCritters(WORD hx, WORD hy, DWORD radius, int find_type, asIScriptArray* critters);
		static DWORD Global_GetCrittersByPids(WORD pid, int find_type, asIScriptArray* critters);
		static DWORD Global_GetCrittersInPath(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters);
		static DWORD Global_GetCrittersInPathBlock(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters, WORD& pre_block_hx, WORD& pre_block_hy, WORD& block_hx, WORD& block_hy);
		static void Global_GetHexInPath(WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist);
		static DWORD Global_GetPathLength(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD cut);
		static void Global_FlushScreen(DWORD from_color, DWORD to_color, DWORD ms);
		static void Global_QuakeScreen(DWORD noise, DWORD ms);
		static void Global_PlaySound(CScriptString& sound_name);
		static void Global_PlaySoundType(BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext);
		static void Global_PlayMusic(CScriptString& music_name, DWORD pos, DWORD repeat);
		static void Global_PlayVideo(CScriptString& video_name, bool can_stop);
		static bool Global_IsTurnBased();
		static WORD Global_GetCurrentMapPid();
		static DWORD Global_GetMessageFilters(asIScriptArray* filters);
		static void Global_SetMessageFilters(asIScriptArray* filters);
		static void Global_Message(CScriptString& msg);
		static void Global_MessageType(CScriptString& msg, int type);
		static void Global_MessageMsg(int text_msg, DWORD str_num);
		static void Global_MessageMsgType(int text_msg, DWORD str_num, int type);
		static void Global_MapMessage(CScriptString& text, WORD hx, WORD hy, DWORD ms, DWORD color, bool fade, int ox, int oy);
		static CScriptString* Global_GetMsgStr(int text_msg, DWORD str_num);
		static CScriptString* Global_GetMsgStrSkip(int text_msg, DWORD str_num, DWORD skip_count);
		static DWORD Global_GetMsgStrNumUpper(int text_msg, DWORD str_num);
		static DWORD Global_GetMsgStrNumLower(int text_msg, DWORD str_num);
		static DWORD Global_GetMsgStrCount(int text_msg, DWORD str_num);
		static bool Global_IsMsgStr(int text_msg, DWORD str_num);
		static CScriptString* Global_ReplaceTextStr(CScriptString& text, CScriptString& replace, CScriptString& str);
		static CScriptString* Global_ReplaceTextInt(CScriptString& text, CScriptString& replace, int i);
		static CScriptString* Global_FormatTags(CScriptString& text, CScriptString* lexems);
		static int Global_GetSomeValue(int var);
		static bool Global_LoadDat(CScriptString& dat_name);
		static void Global_MoveScreen(WORD hx, WORD hy, DWORD speed);
		static void Global_LockScreenScroll(CritterCl* cr);
		static int Global_GetFog(WORD zone_x, WORD zone_y);
		static void Global_RefreshItemsCollection(int collection);
		static int Global_GetScroll(int scroll_element);
		static void Global_SetScroll(int scroll_element, int value);
		static DWORD Global_GetDayTime(DWORD day_part);
		static void Global_GetDayColor(DWORD day_part, BYTE& r, BYTE& g, BYTE& b);

		static CScriptString* Global_GetLastError();
		static void Global_Log(CScriptString& text);
		static ProtoItem* Global_GetProtoItem(WORD proto_id);
		static DWORD Global_GetDistantion(WORD hex_x1, WORD hex_y1, WORD hex_x2, WORD hex_y2);
		static BYTE Global_GetDirection(WORD from_x, WORD from_y, WORD to_x, WORD to_y);
		static BYTE Global_GetOffsetDir(WORD hx, WORD hy, WORD tx, WORD ty, float offset);
		static DWORD Global_GetFullMinute(WORD year, WORD month, WORD day, WORD hour, WORD minute);
		static void Global_GetGameTime(DWORD full_minute, WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute);
		static bool Global_StrToInt(CScriptString& text, int& result);
		static void Global_MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, DWORD steps);
		static CScriptString* Global_GetIfaceIniStr(CScriptString& key);
		static bool Global_Load3dFile(CScriptString& fname, int path_type);
		static DWORD Global_GetTick(){return Timer::FastTick();}
		static void Global_GetTime(WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute, WORD& second, WORD& milliseconds);
		static bool Global_SetParameterGetBehaviour(DWORD index, CScriptString& func_name);
		static bool Global_SetParameterChangeBehaviour(DWORD index, CScriptString& func_name);
		static void Global_AllowSlot(BYTE index, CScriptString& ini_option);
		static void Global_SetRegistrationParam(DWORD index, bool enabled);
		static DWORD Global_GetAngelScriptProperty(int property);
		static bool Global_SetAngelScriptProperty(int property, DWORD value);
		static DWORD Global_GetStrHash(CScriptString* str);
		static bool Global_IsCritterCanWalk(DWORD cr_type);
		static bool Global_IsCritterCanRun(DWORD cr_type);
		static bool Global_IsCritterCanRotate(DWORD cr_type);
		static bool Global_IsCritterCanAim(DWORD cr_type);
		static bool Global_IsCritterAnim1(DWORD cr_type, DWORD index);
		static bool Global_IsCritterAnim3d(DWORD cr_type);
		static int Global_GetGlobalMapRelief(DWORD x, DWORD y);
		static void Global_RunServerScript(CScriptString& func_name, int p0, int p1, int p2, CScriptString* p3, asIScriptArray* p4);
		static void Global_RunServerScriptUnsafe(CScriptString& func_name, int p0, int p1, int p2, CScriptString* p3, asIScriptArray* p4);

		static DWORD Global_LoadSprite(CScriptString& spr_name, int path_index);
		static DWORD Global_LoadSpriteHash(DWORD name_hash, BYTE dir);
		static int Global_GetSpriteWidth(DWORD spr_id, int spr_index);
		static int Global_GetSpriteHeight(DWORD spr_id, int spr_index);
		static DWORD Global_GetSpriteCount(DWORD spr_id);
		static void Global_DrawSprite(DWORD spr_id, int spr_index, int x, int y, DWORD color);
		static void Global_DrawSpriteSize(DWORD spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, DWORD color);
		static void Global_DrawText(CScriptString& text, int x, int y, int w, int h, DWORD color, int font, int flags);
		static void Global_DrawPrimitive(int primitive_type, asIScriptArray& data);
		static void Global_DrawMapSprite(WORD hx, WORD hy, WORD proto_id, DWORD spr_id, int spr_index, int ox, int oy);
		static void Global_DrawCritter2d(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir, int l, int t, int r, int b, bool scratch, bool center, DWORD color);
		static void Global_DrawCritter3d(DWORD instance, DWORD crtype, DWORD anim1, DWORD anim2, asIScriptArray* layers, asIScriptArray* position, DWORD color);

		static void Global_ShowScreen(int screen, int p0, int p1, int p2);
		static void Global_HideScreen(int screen, int p0, int p1, int p2);
		static void Global_GetHardcodedScreenPos(int screen, int& x, int& y);
		static void Global_DrawHardcodedScreen(int screen);
		static int Global_GetKeybLang(){return Keyb::Lang;}
		static bool Global_GetHexPos(WORD hx, WORD hy, int& x, int& y);
		static bool Global_GetMonitorHex(int x, int y, WORD& hx, WORD& hy);
		static Item* Global_GetMonitorItem(int x, int y);
		static CritterCl* Global_GetMonitorCritter(int x, int y);
		static void Global_GetMousePosition(int& x, int& y);
		static WORD Global_GetMapWidth();
		static WORD Global_GetMapHeight();
		static int Global_GetCurrentCursor();
		static int Global_GetLastCursor();
		static void Global_ChangeCursor(int cursor);
	} ScriptFunc;

	static bool SpritesCanDraw;

/************************************************************************/
/* Interface                                                            */
/************************************************************************/
	int IfaceHold;

	bool InitIfaceIni();
	int InitIface();
	bool IfaceLoadRect(INTRECT& comp, const char* name);
	void IfaceLoadRect2(INTRECT& comp, const char* name, int ox, int oy);
	void IfaceLoadSpr(DWORD& comp, const char* name);
	void IfaceLoadAnim(DWORD& comp, const char* name);
	void IfaceFreeResources();

	bool IsCurInRect(INTRECT& rect, int ax, int ay){return !rect.IsZero() && (CurX>=rect[0]+ax && CurY>=rect[1]+ay && CurX<=rect[2]+ax && CurY<=rect[3]+ay);}
	bool IsCurInRect(INTRECT& rect){return !rect.IsZero() && (CurX>=rect[0] && CurY>=rect[1] && CurX<=rect[2] && CurY<=rect[3]);}
	bool IsCurInRectNoTransp(DWORD spr_id,INTRECT& rect, int ax, int ay){return IsCurInRect(rect,ax,ay) && SprMngr.IsPixNoTransp(spr_id,CurX-rect.L-ax,CurY-rect.T-ay,false);}
	int GetSprCX(DWORD spr_id){SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id); return si?(si->Width/2)+si->OffsX:0;};
	int GetSprCY(DWORD spr_id){SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id); return si?(si->Height/2)+si->OffsY:0;};

	void DrawIndicator(INTRECT& rect, PointVec& points, DWORD color, int procent, DWORD& tick, bool is_vertical, bool from_top_or_left);

	// Initial state
	ItemVec InvContInit;
	ItemVec BarterCont1oInit,BarterCont2Init,BarterCont2oInit;
	ItemVec PupCont2Init;
	// After script processing
	ItemVec InvCont,BarterCont1,PupCont1,UseCont;
	ItemVec BarterCont1o,BarterCont2,BarterCont2o;
	ItemVec PupCont2;

	DWORD GetCurContainerItemId(INTRECT& pos, int height, int scroll, ItemVec& cont);
	void ContainerDraw(INTRECT& pos, int height, int scroll, ItemVec& cont, DWORD skip_id);
	Item* GetContainerItem(ItemVec& cont, DWORD id);
	void CollectContItems();
	void ProcessItemsCollection(int collection, ItemVec& init_items, ItemVec& result);
	void UpdateContLexems(ItemVec& cont, DWORD item_id, const char* lexems);

/************************************************************************/
/* Console                                                              */
/************************************************************************/
	DWORD ConsolePic;
	int ConsolePicX,ConsolePicY,ConsoleTextX,ConsoleTextY;
	bool ConsoleEdit;
	char ConsoleStr[MAX_NET_TEXT+1];
	int ConsoleCur;
	StrVec ConsoleHistory;
	int ConsoleHistoryCur;
	int ConsoleLastKey;

	void ConsoleDraw();
	void ConsoleKeyDown(BYTE dik);
	void ConsoleKeyUp(BYTE dik);
	void ConsoleProcess();

/************************************************************************/
/* Inventory                                                            */
/************************************************************************/
	int InvFocus;
#define INVF_NONE           (0)
#define INVF_RAD_CHANNEL    (1)

	DWORD InvPWMain,InvPBOkDw,InvPBOkUp,InvPBScrUpDw,InvPBScrUpUp,InvPBScrUpOff,InvPBScrDwDw,InvPBScrDwUp,InvPBScrDwOff;
	DWORD InvHoldId;
	string InvItemInfo;
	int InvItemInfoScroll,InvItemInfoMaxScroll;
	int InvScroll;
	int InvX,InvY;
	INTRECT InvWMain,InvWChosen;
	int InvHeightItem;
	INTRECT InvWInv,InvWSlot1,InvWSlot2,InvWArmor;
	INTRECT InvBScrUp,InvBScrDn,InvBOk;
	INTRECT InvWText;
	int InvVectX,InvVectY;
	// Extended slots
	struct SlotExt
	{
		int Index;
		char* IniName;
		INTRECT Rect;
	};
typedef vector<SlotExt> SlotExtVec;
typedef vector<SlotExt>::iterator SlotExtVecIt;
	SlotExtVec SlotsExt;
	// Radio
	DWORD RadMainPic,RadPBOn;
#define SET_INV_RADX	RadX=InvX+(InvWMain[2]-InvWMain[0])/2-(RadWMain[2]-RadWMain[0])/2
#define SET_INV_RADY	RadY=InvY+InvWMain[3]
	int RadX,RadY;
	INTRECT RadWMain,RadWMainText,RadWChannel,RadBOn;

	void InvDraw();
	void InvMouseMove();
	void InvLMouseDown();
	void InvLMouseUp();
	void InvRMouseDown();
	void InvKeyDown(BYTE dik);

/************************************************************************/
/* Use                                                                  */
/************************************************************************/
	DWORD UseWMainPicNone,UseBCancelPicDown,UseBScrUpPicDown,UseBScrDownPicDown,
		UseBScrUpPicUp,UseBScrDownPicUp,UseBScrUpPicOff,UseBScrDownPicOff;
	int UseX,UseY,UseVectX,UseVectY;
	int UseScroll,UseHeightItem;
	INTRECT UseWMain,UseWChosen,UseWInv,UseBScrUp,UseBScrDown,UseBCancel;
	DWORD UseHoldId;
	SmthSelected UseSelect;

	void UseDraw();
	void UseLMouseDown();
	void UseLMouseUp();
	void UseRMouseDown();
	void UseMouseMove();

/************************************************************************/
/* Game                                                                 */
/************************************************************************/
	struct MapText 
	{
		WORD HexX,HexY;
		DWORD StartTick,Tick;
		string Text;
		DWORD Color;
		bool Fade;
		INTRECT Rect;
		INTRECT EndRect;
		bool operator==(const MapText& r){return HexX==r.HexX && HexY==r.HexY;}
	};
typedef vector<MapText> MapTextVec;
typedef vector<MapText>::iterator MapTextVecIt;

	MapTextVec GameMapTexts;
	DWORD GameMouseStay;

	void GameDraw();
	void GameKeyDown(BYTE dik);
	void GameLMouseDown();
	void GameLMouseUp();
	void GameRMouseDown();
	void GameRMouseUp();

/************************************************************************/
/* Main iface                                                           */
/************************************************************************/
	DWORD IntMainPic,IntPWAddMess,IntPBAddMessDn,IntPBMessFilter1Dn,IntPBMessFilter2Dn,IntPBMessFilter3Dn,
	IntPBScrUpDn,IntPBScrDnDn,IntPBSlotsDn,
	IntPBInvDn,IntPBMenuDn,IntPBSkillDn,IntPBMapDn,IntPBChaDn,IntPBPipDn,IntPBFixDn;
	DWORD IntDiodeG,IntDiodeY,IntDiodeR,IntBreakTimePic,IntWApCostPicNone;

	int IntX,IntY;
	bool IntVisible,IntAddMess;
	INTRECT IntWMain,IntWAddMess,IntBAddMess,IntBMessFilter1,IntBMessFilter2,IntBMessFilter3;
	INTRECT IntBItem,IntWApCost;
	INTRECT IntBChangeSlot,IntBInv,IntBMenu,IntBSkill,IntBMap,IntBChar,IntBPip,IntBFix;
	INTRECT IntWMess,IntWMessLarge;
	INTRECT IntAP,IntHP,IntAC,IntBreakTime; //15 зеленых(200мс) 3 желтых(1000мс) 2 красных(10000мс)
	int IntAPstepX,IntAPstepY,IntAPMax;
	DWORD IntBItemPicDn;
	int IntBItemOffsX,IntBItemOffsY;
	DWORD IntAimPic;
	int IntAimX,IntAimY;
	int IntUseX,IntUseY;
	DWORD IntWCombatAnim,IntBCombatTurnPicDown,IntBCombatEndPicDown;
	INTRECT IntWCombat,IntBCombatTurn,IntBCombatEnd;

	INTRECT IntWAmmoCount,IntWWearProcent,IntWAmmoCountStr,IntWWearProcentStr;
	PointVec IntAmmoPoints,IntWearPoints;
	DWORD IntAmmoTick,IntWearTick;

	void IntDraw();
	int IntLMouseDown();
	void IntRMouseDown();
	void IntLMouseUp();
	void IntMouseMove();

	DWORD IntMessTabPicNone;
	INTRECT IntMessTab;
	int IntMessTabStepX,IntMessTabStepY;
	bool IntMessTabLevelUp;

/************************************************************************/
/* LMenu                                                                */
/************************************************************************/
	DWORD LmenuPTalkOff,LmenuPTalkOn,LmenuPLookOff,LmenuPLookOn,LmenuPBreakOff,LmenuPBreakOn,
		LmenuPUseOff,LmenuPUseOn,LmenuPGMFollowOff,LmenuPGMFollowOn,
		LmenuPRotateOn,LmenuPRotateOff,LmenuPDropOn,LmenuPDropOff,LmenuPUnloadOn,LmenuPUnloadOff,
		LmenuPSortUpOn,LmenuPSortUpOff,LmenuPSortDownOn,LmenuPSortDownOff,
		LmenuPPickItemOn,LmenuPPickItemOff,LmenuPPushOn,LmenuPPushOff,
		LmenuPBagOn,LmenuPBagOff,LmenuPSkillOn,LmenuPSkillOff,
		LmenuPBarterOpenOn,LmenuPBarterOpenOff,LmenuPBarterHideOn,LmenuPBarterHideOff,
		LmenuPGmapKickOff,LmenuPGmapKickOn,LmenuPGmapRuleOff,LmenuPGmapRuleOn,
		LmenuPVoteUpOff,LmenuPVoteUpOn,LmenuPVoteDownOff,LmenuPVoteDownOn;

	bool LMenuActive;
	bool LMenuTryActivated;
	DWORD LMenuStartTime;
	int LMenuX,LMenuY,LMenuRestoreCurX,LMenuRestoreCurY;
	int LMenuNodeHeight;
	ByteVec* LMenuCurNodes;
	int LMenuCurNode;
	ByteVec LMenuCritNodes,LMenuScenNodes,LMenuNodes;
	BYTE LMenuMode;

	bool IsLMenu();
	void LMenuTryActivate();
	void LMenuStayOff();
	void LMenuTryCreate();
	void LMenuCollect();
	void LMenuSet(BYTE set_lmenu);
	void LMenuDraw();
	void LMenuMouseMove();
	void LMenuMouseUp();

/************************************************************************/
/* Login                                                                */
/************************************************************************/
	DWORD LogPMain,LogPBLogin,LogPBReg,LogPBOptions,LogPBCredits,LogPBExit;
	int LogFocus;

	int LogX,LogY;
	INTRECT LogMain,LogWName,LogWPass,LogBOk,LogBOkText,LogBOptions,LogBOptionsText,LogBCredits,LogBCreditsText,
		LogBReg,LogBRegText,LogBExit,LogBExitText,LogWChat,LogWVersion;

	void LogDraw();
	void LogKeyDown(BYTE dik);
	void LogLMouseDown();
	void LogLMouseUp();
	void LogTryConnect();
	
/************************************************************************/
/* Dialog                                                               */
/************************************************************************/
	DWORD DlgPMain,DlgPAnsw,DlgPBBarter,DlgPBSay,DlgAvatarSprId;
	int DlgNextAnswX,DlgNextAnswY;
	int DlgCurAnsw,DlgHoldAnsw;
	DWORD DlgCurAnswPage,DlgMaxAnswPage;
	int DlgVectX,DlgVectY;
	BYTE DlgIsNpc;
	DWORD DlgNpcId;
	DWORD DlgEndTick;

	struct Answer
	{
		DWORD Page;
		INTRECT Position;
		string Text;
		int AnswerNum; // -1 prev page, -2 next page

		Answer(DWORD page, INTRECT pos, string text, DWORD answer_num):Page(page),Position(pos),Text(text),AnswerNum(answer_num){}
	};
	vector<Answer> DlgAllAnswers,DlgAnswers;

	string DlgMainText;
	int DlgMainTextCur,DlgMainTextLinesReal,DlgMainTextLinesRect;
	int DlgX,DlgY;
	INTRECT DlgWMain,DlgWText,DlgBScrUp,DlgBScrDn,DlgAnsw,DlgAnswText,DlgWMoney,DlgBBarter,
		DlgBBarterText,DlgBSay,DlgBSayText,DlgWAvatar,DlgWTimer;

	//Barter
	DWORD BarterPMain,BarterPBOfferDn,BarterPBTalkDn,
		BarterPBC1ScrUpDn,BarterPBC2ScrUpDn,BarterPBC1oScrUpDn,BarterPBC2oScrUpDn,
		BarterPBC1ScrDnDn,BarterPBC2ScrDnDn,BarterPBC1oScrDnDn,BarterPBC2oScrDnDn;
	INTRECT BarterWMain,BarterBOffer,BarterBOfferText,BarterBTalk,BarterBTalkText,BarterWCont1Pic,BarterWCont2Pic,
		BarterWCont1,BarterWCont2,BarterWCont1o,BarterWCont2o,
		BarterBCont1ScrUp,BarterBCont2ScrUp,BarterBCont1oScrUp,BarterBCont2oScrUp,
		BarterBCont1ScrDn,BarterBCont2ScrDn,BarterBCont1oScrDn,BarterBCont2oScrDn,
		BarterWCost1,BarterWCost2,BarterWChosen,BarterWCritter;
	DWORD BarterPlayerId;
	int BarterCont1HeightItem,BarterCont2HeightItem,
		BarterCont1oHeightItem,BarterCont2oHeightItem;
	int BarterScroll1,BarterScroll2,BarterScroll1o,BarterScroll2o;
	DWORD BarterHoldId;
	DWORD BarterCount;
	WORD BarterK;
	string BarterText;
	//Players barter extra
	DWORD BarterOpponentId;
	bool BarterIsPlayers,BarterHide,BarterOpponentHide,BarterOffer,BarterOpponentOffer;

	bool IsScreenPlayersBarter();
	void BarterTryOffer();
	void BarterTransfer(DWORD item_id, int item_cont, DWORD item_count);
	void ContainerCalcInfo(ItemVec& cont, DWORD& cost, DWORD& weigth, DWORD& volume, int barter_k, bool sell);
	void FormatTags(char* text, size_t text_len, CritterCl* player, CritterCl* npc, const char* lexems);

	void DlgDraw(bool is_dialog);
	void DlgMouseMove(bool is_dialog);
	void DlgLMouseDown(bool is_dialog);
	void DlgLMouseUp(bool is_dialog);
	void DlgRMouseDown(bool is_dialog);
	void DlgKeyDown(bool is_dialog, BYTE dik);
	void DlgCollectAnswers(bool next);

/************************************************************************/
/* Mini-map                                                             */
/************************************************************************/
#define MINIMAP_PREPARE_TICK      (1000)
	DWORD LmapPMain,LmapPBOkDw,LmapPBScanDw,LmapPBLoHiDw,LmapPPix;
	PointVec LmapPrepPix;
	INTRECT LmapMain,LmapWMap,LmapBOk,LmapBScan,LmapBLoHi;
	short LmapX,LmapY;
	int LmapVectX,LmapVectY;
	int LmapZoom;
	bool LmapSwitchHi;
	DWORD LmapPrepareNextTick;

	void LmapPrepareMap();
	void LmapDraw();
	void LmapLMouseDown();
	void LmapMouseMove();
	void LmapLMouseUp();

/************************************************************************/
/* Global map                                                           */
/************************************************************************/
	DWORD GmapTilesX,GmapTilesY;
	DwordVec GmapPic;
	char GmapTilesPic[64];

	DWORD GmapPBTownDw,GmapWMainPic,GmapPGr,GmapPTarg,GmapPStay,GmapPStayDn,GmapPStayMask,GmapWDayTimeAnim,GmapLocPic;
	DWORD GmapPTownInPic,GmapPTownInPicDn,GmapPTownInPicMask,GmapPTownViewPic,GmapPTownViewPicDn,GmapPTownViewPicMask;
	int GmapPTownInOffsX,GmapPTownInOffsY,GmapPTownViewOffsX,GmapPTownViewOffsY;
	DWORD GmapPFollowCrit,GmapPFollowCritSelf;
	DWORD GmapPWTab,GmapPWBlankTab,GmapPBTabLoc,GmapPTabScrUpDw,GmapPTabScrDwDw; //Tabs pics
	DWORD GmapBInvPicDown,GmapBMenuPicDown,GmapBChaPicDown,GmapBPipPicDown,GmapBFixPicDown;
	DWORD GmapPLightPic0,GmapPLightPic1;
	int GmapX,GmapY,GmapVectX,GmapVectY,GmapMapScrX,GmapMapScrY,GmapWNameStepX,GmapWNameStepY;
	INTRECT GmapWMain,GmapWMap,GmapBTown,GmapWName,GmapWChat,GmapWPanel,GmapWCar,GmapWLock,GmapWTime,GmapWDayTime;
	INTRECT GmapBInv,GmapBMenu,GmapBCha,GmapBPip,GmapBFix;
	PointVec GmapMapCutOff;
	bool GmapIsProc;
	float GmapZoom;

	void GmapNullParams();
	void GmapProcess();
	// Town
	DWORD GmapTownPicId;
	INTRECT GmapTownPicPos;
	IntRectVec GmapTownButtonPos;
	IntRectVec GmapTownTextPos;
	StrVec GmapTownText;
	int GmapTownCurButton;
	DWORD GmapNextShowEntrancesTick;
	DWORD GmapShowEntrancesLocId;
	bool GmapShowEntrances[0x100];

	// Relief
	C4BitMask* GmapRelief;

	// Mask
	C2BitMask GmapFog;
	PointVec GmapFogPix;

	// Locations
	struct GmapLocation
	{
		DWORD LocId;
		WORD LocPid;
		WORD LocWx;
		WORD LocWy;
		WORD Radius;
		DWORD Color;
		bool operator==(const DWORD& _right){return (this->LocId==_right);}
	};
typedef vector<GmapLocation> GmapLocationVec;
typedef vector<GmapLocation>::iterator GmapLocationVecIt;
	GmapLocationVec GmapLoc;
	GmapLocation GmapTownLoc;

	// Trace
	IntPairVec GmapTrace;

	// Params
	float GmapGroupXf,GmapGroupYf;
	DWORD GmapProcLastTick,GmapMoveLastTick;
	int GmapGroupX,GmapGroupY,GmapMoveX,GmapMoveY;
	float GmapSpeedX,GmapSpeedY;
	bool GmapWait;

	// Cars
	struct
	{
		DWORD MasterId;
		Item* Car;
	} GmapCar;

	// Tabs
	INTRECT GmapWTabs,GmapWTab,GmapWTabLoc,GmapBTabLoc,GmapBTabsScrUp,GmapBTabsScrDn;
	int GmapTabNextX,GmapTabNextY,GmapCurHoldBLoc,GmapTabsScrX,GmapTabsScrY;
	DWORD GmapTabsLastScr;

	void GmapDraw();
	void GmapTownDraw();
	void GmapLMouseDown();
	void GmapLMouseUp();
	void GmapRMouseDown();
	void GmapRMouseUp();
	void GmapMouseMove();
	void GmapKeyDown(BYTE dik);
	void GmapChangeZoom(float offs);
	Item* GmapGetCar();
	void GmapFreeResources();

/************************************************************************/
/* Skillbox                                                             */
/************************************************************************/
	DWORD SboxPMain,SboxPBCancelDn,SboxPBSneakDn,SboxPBLockPickDn,SboxPBStealDn,
		SboxPBTrapsDn,SboxPBFirstaidDn,SboxPBDoctorDn,SboxPBScienceDn,SboxPBRepairDn;
	INTRECT SboxWMain,SboxWMainText,SboxBCancel,SboxBCancelText,SboxBSneak,SboxBLockpick,SboxBSteal,
		SboxBTrap,SboxBFirstAid,SboxBDoctor,SboxBScience,SboxBRepair;
	INTRECT SboxTSneak,SboxTLockpick,SboxTSteal,SboxTTrap,SboxTFirstAid,
		SboxTDoctor,SboxTScience,SboxTRepair;
	int SboxX,SboxY;
	int SboxVectX,SboxVectY;

	WORD CurSkill;
	SmthSelected SboxUseOn;

	void SboxDraw();
	void SboxLMouseDown();
	void SboxMouseMove();
	void SboxLMouseUp();

/************************************************************************/
/* Menu Options                                                         */
/************************************************************************/
	DWORD MoptPMain,MoptPBResumeOn,MoptPBExitOn;
	int MoptX,MoptY;
	INTRECT MoptMain,MoptBResume,MoptBExit;

	void MoptDraw();
	void MoptLMouseDown();
	void MoptLMouseUp();

/************************************************************************/
/* Credits                                                              */
/************************************************************************/
	DWORD CreditsNextTick,CreditsMoveTick;
	int CreditsYPos;

	void CreditsDraw();

/************************************************************************/
/* Character                                                            */
/************************************************************************/
	// Switch
	int ChaCurSwitch;
#define CHA_SWITCH_PERKS (0)
#define CHA_SWITCH_KARMA (1)
#define CHA_SWITCH_KILLS (2)

	DWORD ChaPBSwitchPerks,ChaPBSwitchKarma,ChaPBSwitchKills,ChaPBSwitchMask,
		ChaPBSwitchScrUpUp,ChaPBSwitchScrUpDn,ChaPBSwitchScrDnUp,ChaPBSwitchScrDnDn;
	INTRECT ChaBSwitch,ChaTSwitch,ChaBSwitchScrUp,ChaBSwitchScrDn;

	struct SwitchElement
	{
		DWORD NameStrNum;
		DWORD DescStrNum;
		WORD PictureId;
		DWORD DrawFlags;
		char Addon[64];

		SwitchElement(DWORD name, DWORD desc, WORD pic, DWORD flags):NameStrNum(name),DescStrNum(desc),DrawFlags(flags),PictureId(pic){ZeroMemory(Addon,sizeof(Addon));}
		SwitchElement(const char* add, DWORD flags):NameStrNum(0),DescStrNum(0),PictureId(0),DrawFlags(flags){CopyMemory(Addon,add,sizeof(Addon));}
	};
typedef vector<SwitchElement> SwitchElementVec;

	SwitchElementVec ChaSwitchText[3];
	int ChaSwitchScroll[3];
	void ChaPrepareSwitch();

	// Pics
	DWORD ChaPMain,ChaPBPrintDn,ChaPBOkDn,ChaPBCancelDn;
	int ChaX,ChaY;
	int ChaVectX,ChaVectY;
	INTRECT ChaWMain,ChaBPrint,ChaBPrintText,ChaBOk,ChaBOkText,ChaBCancel,ChaBCancelText;

	// Special
	INTRECT ChaWSpecialText,ChaWSpecialValue,ChaWSpecialLevel;
	int ChaWSpecialNextX,ChaWSpecialNextY;

	// Skills
	INTRECT ChaWSkillText,ChaWSkillName,ChaWSkillValue;
	int ChaWSkillNextX,ChaWSkillNextY;
	WORD ChaSkillUp[MAX_PARAMS];
	INTRECT ChaWUnspentSP,ChaWUnspentSPText;
	int ChaUnspentSkillPoints;

	// Slider
	DWORD ChaPWSlider,ChaPBSliderPlusDn,ChaPBSliderMinusDn;
	INTRECT ChaBSliderPlus,ChaBSliderMinus;
	int ChaWSliderX,ChaWSliderY;
	int ChaCurSkill;

	// Level
	INTRECT ChaWLevel,ChaWExp,ChaWNextLevel;

	// Damage
	INTRECT ChaWDmgLife,ChaWDmg;
	int ChaWDmgNextX,ChaWDmgNextY;

	// Stats
	INTRECT ChaWStatsName,ChaWStatsValue;
	int ChaWStatsNextX,ChaWStatsNextY;

	// Tip
	INTRECT ChaWName,ChaWDesc,ChaWPic;
	char ChaName[MAX_FOTEXT];
	char ChaDesc[MAX_FOTEXT];
	int ChaSkilldexPic;

	// Buttons
	DWORD ChaPBNameDn,ChaPBAgeDn,ChaPBSexDn,RegPBAccDn;
	INTRECT ChaBName,ChaBAge,ChaBSex;

	// Registration
	DWORD RegPMain,RegPBSpecialPlusDn,RegPBSpecialMinusDn,RegPBTagSkillDn;
	INTRECT RegWMain,RegBSpecialPlus,RegBSpecialMinus,RegBTagSkill,RegWUnspentSpecial,RegWUnspentSpecialText;
	int RegBSpecialNextX,RegBSpecialNextY,RegBTagSkillNextX,RegBTagSkillNextY;
	int RegCurSpecial,RegCurTagSkill;
	CritterCl* RegNewCr;

	// Trait
	DWORD RegPBTraitDn;
	INTRECT RegBTraitL,RegBTraitR,RegWTraitL,RegWTraitR;
	int RegTraitNextX,RegTraitNextY;
	int RegTraitNum;

	// Methods
	bool RegCheckData(CritterCl* newcr);
	void ChaDraw(bool is_reg);
	void ChaLMouseDown(bool is_reg);
	void ChaLMouseUp(bool is_reg);
	void ChaMouseMove(bool is_reg);

/************************************************************************/
/* Character name                                                       */
/************************************************************************/
	DWORD ChaNamePic;
	INTRECT ChaNameWMain,ChaNameWName,ChaNameWNameText,ChaNameWPass,ChaNameWPassText;
	int ChaNameX,ChaNameY;

	void ChaNameDraw();
	void ChaNameLMouseDown();
	void ChaNameKeyDown(BYTE dik);

/************************************************************************/
/* Character age                                                        */
/************************************************************************/
	DWORD ChaAgePic,ChaAgeBUpDn,ChaAgeBDownDn;
	INTRECT ChaAgeWMain,ChaAgeBUp,ChaAgeBDown,ChaAgeWAge;
	int ChaAgeX,ChaAgeY;

	void ChaAgeDraw();
	void ChaAgeLMouseDown();
	void ChaAgeLMouseUp();

/************************************************************************/
/* Character sex                                                        */
/************************************************************************/
	DWORD ChaSexPic,ChaSexBMaleDn,ChaSexBFemaleDn;
	INTRECT ChaSexWMain,ChaSexBMale,ChaSexBFemale;
	int ChaSexX,ChaSexY;

	void ChaSexDraw();
	void ChaSexLMouseDown();
	void ChaSexLMouseUp();

/************************************************************************/
/* Perk                                                                 */
/************************************************************************/
	DWORD PerkPMain,PerkPBScrUpDn,PerkPBScrDnDn,PerkPBOkDn,PerkPBCancelDn;
	INTRECT PerkWMain,PerkWText,PerkWPerks,PerkWPic,PerkBScrUp,PerkBScrDn,PerkBOk,PerkBCancel,PerkBOkText,PerkBCancelText;
	int PerkX,PerkY;
	int PerkVectX,PerkVectY;
	int PerkNextX,PerkNextY;
	int PerkScroll;
	int PerkCurPerk;
	WordVec PerkCollection;

	void PerkPrepare();
	void PerkDraw();
	void PerkLMouseDown();
	void PerkLMouseUp();
	void PerkMouseMove();

/************************************************************************/
/* Town view                                                            */
/************************************************************************/
	DWORD TViewWMainPic,TViewBBackPicDn,TViewBEnterPicDn,TViewBContoursPicDn;
	INTRECT TViewWMain,TViewBBack,TViewBEnter,TViewBContours;
	int TViewX,TViewY,TViewVectX,TViewVectY;
	bool TViewShowCountours;

#define TOWN_VIEW_FROM_NONE        (0)
#define TOWN_VIEW_FROM_GLOBAL      (1)
	int TViewType;
	DWORD TViewGmapLocId,TViewGmapLocEntrance; // TOWN_VIEW_FROM_GLOBAL

	void TViewDraw();
	void TViewLMouseDown();
	void TViewLMouseUp();
	void TViewMouseMove();

/************************************************************************/
/* PipBoy                                                               */
/************************************************************************/
	int PipMode;
#define PIP__NONE           (0)
#define PIP__STATUS         (1)
#define PIP__STATUS_QUESTS  (2)
#define PIP__STATUS_SCORES  (3)
#define PIP__GAMES          (4)
#define PIP__AUTOMAPS       (5)
#define PIP__AUTOMAPS_LOC   (6)
#define PIP__AUTOMAPS_MAP   (7)
#define PIP__ARCHIVES       (8)
#define PIP__ARCHIVES_INFO  (9)

	DWORD PipPMain,PipPBStatusDn/*,PipPBGamesDn*/,PipPBAutomapsDn,PipPBArchivesDn,PipPBCloseDn,PipPWMonitor;
	int PipX,PipY;
	int PipVectX,PipVectY;
	INTRECT PipWMain,PipWMonitor,PipBStatus/*,PipBGames*/,PipBAutomaps,PipBArchives,PipBClose,PipWTime;
	int PipScroll[PIP__ARCHIVES_INFO+1];

	void PipDraw();
	void PipLMouseDown();
	void PipLMouseUp();
	void PipRMouseDown();
	void PipMouseMove();

	// Quests
	QuestManager QuestMngr;
	DWORD QuestNumTab;
	WORD QuestNumQuest;
	// HoloInfo
	DWORD HoloInfo[MAX_HOLO_INFO];
	DWORD PipInfoNum;
	// Scores
	DWORD ScoresNextUploadTick;
	char BestScores[SCORES_MAX][SCORE_NAME_LEN];
	// Automaps
	struct Automap
	{
		DWORD LocId;
		WORD LocPid;
		string LocName;
		WordVec MapPids;
		StrVec MapNames;
		size_t CurMap;

		Automap():LocId(0),LocPid(0),CurMap(0){}
		bool operator==(const DWORD id)const{return LocId==id;}
	};
typedef vector<Automap> AutomapVec;
typedef vector<Automap>::iterator AutomapVecIt;
	AutomapVec Automaps;
	Automap AutomapSelected;
	WordSet AutomapWaitPids;
	WordSet AutomapReceivedPids;
	PointVec AutomapPoints;
	WORD AutomapCurMapPid;
	float AutomapScrX,AutomapScrY;
	float AutomapZoom;

/************************************************************************/
/* Aim                                                                  */
/************************************************************************/
	DWORD AimPWMain,AimPBCancelDn,AimPic;

	int AimX,AimY;
	int AimVectX,AimVectY;
	int AimPicX,AimPicY;
	INTRECT AimWMain,AimBCancel,
		AimWHeadT,AimWLArmT,AimWRArmT,AimWTorsoT,AimWRLegT,AimWLLegT,AimWEyesT,AimWGroinT,
		AimWHeadP,AimWLArmP,AimWRArmP,AimWTorsoP,AimWRLegP,AimWLLegP,AimWEyesP,AimWGroinP;
	int AimHeadP,AimLArmP,AimRArmP,AimTorsoP,AimRLegP,AimLLegP,AimEyesP,AimGroinP;
	StrDWordMap AimLoadedPics;
	DWORD AimTargetId;

	void AimDraw();
	void AimLMouseDown();
	void AimLMouseUp();
	void AimMouseMove();
	DWORD AimGetPic(CritterCl* cr);

/************************************************************************/
/* PickUp                                                               */
/************************************************************************/
	DWORD PupPMain,PupPTakeAllOn,PupPBOkOn,
		PupPBScrUpOn1,PupPBScrUpOff1,PupPBScrDwOn1,PupPBScrDwOff1,
		PupPBScrUpOn2,PupPBScrUpOff2,PupPBScrDwOn2,PupPBScrDwOff2,
		PupBNextCritLeftPicUp,PupBNextCritLeftPicDown,PupBNextCritRightPicUp,PupBNextCritRightPicDown;
	DWORD PupHoldId;

	int PupScroll1,PupScroll2,PupScrollCrit;
	int PupX,PupY;
	int PupVectX,PupVectY;
	INTRECT PupWMain,PupWInfo,PupWCont1,PupWCont2,PupBTakeAll,PupBOk,
		PupBScrUp1,PupBScrDw1,PupBScrUp2,PupBScrDw2,PupBNextCritLeft,PupBNextCritRight;
	int PupHeightItem1,PupHeightItem2;
	BYTE PupTransferType;
	DWORD PupContId,PupClosedContId,PupLastPutId;
	WORD PupContPid;
	DWORD PupCount;
	WORD PupSize;
	DWORD PupWeight;

	void PupDraw();
	void PupMouseMove();
	void PupLMouseDown();
	void PupLMouseUp();
	void PupRMouseDown();
	void PupTransfer(DWORD item_id, DWORD cont, DWORD count);
	CritVec& PupGetLootCrits();
	CritterCl* PupGetLootCrit(int scroll);

/************************************************************************/
/* Dialog box                                                           */
/************************************************************************/
	DWORD DlgboxWTopPicNone,DlgboxWMiddlePicNone,DlgboxWBottomPicNone,DlgboxBButtonPicDown;
	INTRECT DlgboxWTop,DlgboxWMiddle,DlgboxWBottom,DlgboxWText,DlgboxBButton,DlgboxBButtonText;
	int DlgboxX,DlgboxY;
	int DlgboxVectX,DlgboxVectY;
	BYTE DlgboxType;
#define DIALOGBOX_NONE           (0)
#define DIALOGBOX_FOLLOW         (1)
#define DIALOGBOX_BARTER         (2)
#define DIALOGBOX_ENCOUNTER_ANY  (3)
#define DIALOGBOX_ENCOUNTER_RT   (4)
#define DIALOGBOX_ENCOUNTER_TB   (5)
#define DIALOGBOX_MANUAL         (6)
	DWORD DlgboxWait;
	char DlgboxText[MAX_FOTEXT];
	string DlgboxButtonText[MAX_DLGBOX_BUTTONS];
	DWORD DlgboxButtonsCount;
	DWORD DlgboxSelectedButton;
	// For follow
	BYTE FollowType;
	DWORD FollowRuleId;
	WORD FollowMap;
	// For barter
	DWORD PBarterPlayerId;
	bool PBarterHide;

	void DlgboxDraw();
	void DlgboxLMouseDown();
	void DlgboxLMouseUp();
	void DlgboxMouseMove();

/************************************************************************/
/* Elevator                                                             */
/************************************************************************/
	DWORD ElevatorMainPic,ElevatorExtPic,ElevatorIndicatorAnim;
	INTRECT ElevatorMain,ElevatorExt,ElevatorIndicator;
	DWORD ElevatorButtonPicDown,ElevatorButtonsCount;
	INTRECT ElevatorButtons[MAX_DLGBOX_BUTTONS];
	DWORD ElevatorType,ElevatorLevelsCount,ElevatorStartLevel,ElevatorCurrentLevel;
	int ElevatorX,ElevatorY,ElevatorVectX,ElevatorVectY;
	int ElevatorSelectedButton;
	bool ElevatorAnswerDone;
	DWORD ElevatorSendAnswerTick;

	void ElevatorDraw();
	void ElevatorLMouseDown();
	void ElevatorLMouseUp();
	void ElevatorMouseMove();
	void ElevatorGenerate(DWORD param);
	void ElevatorProcess();
	int ElevatorGetCurButton();

/************************************************************************/
/* Say dialog                                                           */
/************************************************************************/
	DWORD SayWMainPicNone,SayBOkPicDown,SayBCancelPicDown;
	int SayX,SayY;
	int SayVectX,SayVectY;
	INTRECT SayWMain,SayWMainText,SayWSay,SayBOk,SayBOkText,SayBCancel,SayBCancelText;
	BYTE SayType;
	bool SayOnlyNumbers;
#define DIALOGSAY_NONE        (0)
#define DIALOGSAY_TEXT        (1)
	string SayTitle;
	char SayText[MAX_FOTEXT];

	void SayDraw();
	void SayLMouseDown();
	void SayLMouseUp();
	void SayMouseMove();
	void SayKeyDown(BYTE dik);

/************************************************************************/
/* Wait                                                                 */
/************************************************************************/
	DWORD WaitPic;

	void WaitDraw();

/************************************************************************/
/* Split                                                                */
/************************************************************************/
#define MAX_SPLIT_VALUE        (100000)

	DWORD SplitMainPic,SplitPBUpDn,SplitPBDnDn,SplitPBAllDn,SplitPBDoneDn,SplitPBCancelDn,SplitItemPic,SplitItemColor;
	int SplitX,SplitY;
	int SplitVectX,SplitVectY;
	INTRECT SplitWMain,SplitWTitle,SplitWItem,SplitBUp,SplitBDown,SplitBAll,SplitWValue,SplitBDone,SplitBCancel;
	DWORD SplitItemId,SplitCont;
	int SplitValue,SplitMinValue,SplitMaxValue;
	bool SplitValueKeyPressed;
	int SplitParentScreen;

	void SplitStart(Item* item, int to_cont);
	void SplitClose(bool change);
	void SplitDraw();
	void SplitKeyDown(BYTE dik);
	void SplitLMouseDown();
	void SplitLMouseUp();
	void SplitMouseMove();

/************************************************************************/
/* Timer                                                                */
/************************************************************************/
#define TIMER_MIN_VALUE       (1)
#define TIMER_MAX_VALUE       (599)

	DWORD TimerMainPic,TimerBUpPicDown,TimerBDownPicDown,TimerBDonePicDown,TimerBCancelPicDown,TimerItemPic,TimerItemColor;
	int TimerX,TimerY;
	int TimerVectX,TimerVectY;
	INTRECT TimerWMain,TimerWTitle,TimerWItem,TimerBUp,TimerBDown,TimerWValue,TimerBDone,TimerBCancel;
	int TimerValue;
	DWORD TimerItemId;

	void TimerStart(DWORD item_id, DWORD pic, DWORD pic_color);
	void TimerClose(bool done);
	void TimerDraw();
	void TimerKeyDown(BYTE dik);
	void TimerLMouseDown();
	void TimerLMouseUp();
	void TimerMouseMove();

/************************************************************************/
/* FixBoy                                                               */
/************************************************************************/
	int FixMode;
#define FIX_MODE_LIST        (0)
#define FIX_MODE_FIXIT       (1)
#define FIX_MODE_RESULT      (2)

	DWORD FixMainPic,FixPBDoneDn,FixPBScrUpDn,FixPBScrDnDn,FixPBFixDn;
	INTRECT FixWMain,FixBDone,FixBScrUp,FixBScrDn,FixWWin,FixBFix;
	int FixX,FixY,FixVectX,FixVectY;
	int FixCurCraft;

	struct SCraft 
	{
		INTRECT Pos;
		string Name;
		DWORD Num;
		bool IsTrue;

		SCraft(INTRECT& pos, string& name, DWORD num, bool is_true){Pos=pos;Name=name;Num=num;IsTrue=is_true;}
		SCraft(const SCraft& _right){Pos=_right.Pos;Name=_right.Name;Num=_right.Num;IsTrue=_right.IsTrue;}
		SCraft& operator=(const SCraft& _right){Pos=_right.Pos;Name=_right.Name;Num=_right.Num;IsTrue=_right.IsTrue;return *this;}
	};

typedef vector<SCraft> SCraftVec;
typedef vector<SCraftVec> SCraftVecVec;

	SCraftVecVec FixCraftLst;
	int FixScrollLst;
	SCraftVecVec FixCraftFix;
	int FixScrollFix;
	BYTE FixResult;

	struct FixDrawComponent 
	{
		bool IsText;
		INTRECT Rect;

		string Text;
		DWORD SprId;

		FixDrawComponent(INTRECT& r, string& text):IsText(true),SprId(0){Rect=r;Text=text;}
		FixDrawComponent(INTRECT& r, DWORD spr_id):IsText(false),SprId(spr_id){Rect=r;}
	};
typedef vector<FixDrawComponent*> FixDrawComponentVec;
#define FIX_DRAW_PIC_WIDTH          (40)
#define FIX_DRAW_PIC_HEIGHT         (40)

	FixDrawComponentVec FixDrawComp;
	string FixResultStr;
	DwordSet FixShowCraft;
	DWORD FixNextShowCraftTick;

	void FixGenerate(int fix_mode);
	int GetMouseCraft();
	SCraftVec* GetCurSCrafts();

	void FixDraw();
	void FixLMouseDown();
	void FixLMouseUp();
	void FixMouseMove();

/************************************************************************/
/* Input Box                                                            */
/************************************************************************/
	int IboxMode;
#define IBOX_MODE_NONE         (0)
#define IBOX_MODE_HOLO         (1)

	DWORD IboxWMainPicNone,IboxBDonePicDown,IboxBCancelPicDown;
	INTRECT IboxWMain,IboxWTitle,IboxWText,IboxBDone,IboxBDoneText,IboxBCancel,IboxBCancelText;
	int IboxX,IboxY,IboxVectX,IboxVectY;
	string IboxTitle,IboxText;
	int IboxTitleCur,IboxTextCur;
	DWORD IboxLastKey;

	// Holodisk
	DWORD IboxHolodiskId;

	void IboxDraw();
	void IboxLMouseDown();
	void IboxLMouseUp();
	void IboxKeyDown(BYTE dik);
	void IboxKeyUp(BYTE dik);
	void IboxProcess();
	void IboxMouseMove();

/************************************************************************/
/* Cursor                                                               */
/************************************************************************/
	DWORD CurPDef,CurPMove,CurPMoveBlock,CurPHand,CurPUseItem,CurPUseSkill,CurPWait;
	DWORD CurPScrRt,CurPScrLt,CurPScrUp,CurPScrDw,CurPScrRU,CurPScrLU,CurPScrRD,CurPScrLD;
	int CurX,CurY;

	void CurDraw();

/************************************************************************/
/* Generic                                                              */
/************************************************************************/
	DWORD GameStartMinute;
	DWORD GameStartTick;
	DWORD DaySumRGB;

	void ProcessGameTime();
	void SetDayTime(bool refresh);
	void ProcessMouseWheel(int data);
	void SetGameColor(DWORD color);

//	StringMap CritsNames;
	CritterCl* Chosen;

	void AddCritter(CritterCl* cr);
	CritterCl* GetCritter(DWORD crid){return HexMngr.GetCritter(crid);}
	ItemHex* GetItem(DWORD item_id){return HexMngr.GetItemById(item_id);}
	void ClearCritters();
	void EraseCritter(DWORD remid);

//	void AddCritName(CrID crid, string name){if(CritsNames.count(crid)) CritsNames.erase(crid); CritsNames.insert(StringMapVal(crid,name));}
//	const char* GetCritName(CrID crid){StringMapIt it=CritsNames.find(crid); return it!=CritsNames.end()?(*it).second.c_str():NULL;}

	bool IsTurnBased;
	DWORD TurnBasedTime;
	bool NoLogOut;
	DWORD* UID3,*UID2;

	bool IsTurnBasedMyTurn(){return IsTurnBased && Timer::GameTick()<TurnBasedTime && Chosen && Chosen->GetAllAp()>0;}
	DWORD GetTurnBasedMyTime(){return TurnBasedTime-Timer::GameTick();}

	bool RebuildLookBorders;
	bool DrawLookBorders,DrawShootBorders;
	PointVec LookBorders,ShootBorders;
	void LookBordersPrepare();
	void LookBordersDraw();

/************************************************************************/
/* MessBox                                                              */
/************************************************************************/
#define FOMB_GAME          (0)
#define FOMB_TALK          (1)
#define FOMB_COMBAT_RESULT (2)
#define FOMB_VIEW          (3)
	struct MessBoxMessage
	{
		int Type;
		string Mess;
		string Time;

		MessBoxMessage(int type, const char* mess, const char* time):Type(type),Mess(mess),Time(time){}
		MessBoxMessage(const MessBoxMessage& r){Type=r.Type;Mess=r.Mess;Time=r.Time;}
		MessBoxMessage& operator=(const MessBoxMessage& r){Type=r.Type;Mess=r.Mess;Time=r.Time;return *this;}
	};
typedef vector<MessBoxMessage> MessBoxMessageVec;

	MessBoxMessageVec MessBox;
	string MessBoxCurText;
	int MessBoxScroll,MessBoxMaxScroll,MessBoxScrollLines;
	IntVec MessBoxFilters;

	void MessBoxGenerate();
	void AddMess(int mess_type, const char* msg);
	void MessBoxDraw();
	INTRECT MessBoxCurRectDraw();
	INTRECT MessBoxCurRectScroll();
	bool MessBoxLMouseDown();

	BYTE GetCmdNum(char*& str);

/************************************************************************/
/*                                                                      */
/************************************************************************/
};

//Fonts
#define FONT_FO                 (0)
#define FONT_NUM                (1)
#define FONT_BIG_NUM            (2)
#define FONT_SAND_NUM           (3)
#define FONT_SPECIAL            (4)
#define FONT_DEF                (5)
#define FONT_THIN               (6)
#define FONT_FAT                (7)
#define FONT_BIG                (8)

//Screens
#define SCREEN_NONE             (0)
//Primary screens
#define SCREEN_LOGIN            (1)
#define SCREEN_REGISTRATION     (2)
#define SCREEN_CREDITS          (3)
#define SCREEN_OPTIONS          (4)
#define SCREEN_GAME             (5)
#define SCREEN_GLOBAL_MAP       (6)
#define SCREEN_WAIT             (7)
//Secondary screens
#define SCREEN__INVENTORY       (10)
#define SCREEN__PICKUP          (11)
#define SCREEN__MINI_MAP        (12)
#define SCREEN__CHARACTER       (13)
#define SCREEN__DIALOG          (14)
#define SCREEN__BARTER          (15)
#define SCREEN__PIP_BOY         (16)
#define SCREEN__FIX_BOY         (17)
#define SCREEN__MENU_OPTION     (18)
#define SCREEN__AIM             (19)
#define SCREEN__SPLIT           (20)
#define SCREEN__TIMER           (21)
#define SCREEN__DIALOGBOX       (22)
#define SCREEN__ELEVATOR        (23)
#define SCREEN__SAY             (24)
#define SCREEN__CHA_NAME        (25)
#define SCREEN__CHA_AGE         (26)
#define SCREEN__CHA_SEX         (27)
#define SCREEN__GM_TOWN         (28)
#define SCREEN__INPUT_BOX       (29)
#define SCREEN__SKILLBOX        (30)
#define SCREEN__USE             (31)
#define SCREEN__PERK            (32)
#define SCREEN__TOWN_VIEW       (33)

// Cur modes
#define CUR_DEFAULT             (0)
#define CUR_MOVE                (1)
#define CUR_USE_ITEM            (2)
#define CUR_USE_WEAPON          (3)
#define CUR_USE_SKILL           (4)
#define CUR_WAIT                (5)
#define CUR_HAND                (6)

// Lmenu
#define LMENU_SHOW_TIME         (400)
#define LMENU_OFF               (0)
#define LMENU_PLAYER            (1)
#define LMENU_NPC               (2)
#define LMENU_ITEM              (3)
#define LMENU_ITEM_INV          (4)
#define LMENU_GMAP_CRIT         (5)
// Lmenu Nodes
#define LMENU_NODE_LOOK         (0)
#define LMENU_NODE_TALK         (1)
#define LMENU_NODE_BREAK        (2)
#define LMENU_NODE_PICK         (3)
#define LMENU_NODE_GMFOLLOW     (4)
#define LMENU_NODE_ROTATE       (5)
#define LMENU_NODE_DROP         (6)
#define LMENU_NODE_UNLOAD       (7)
#define LMENU_NODE_USE          (8)
#define LMENU_NODE_SORT_UP      (10)
#define LMENU_NODE_SORT_DOWN    (11)
#define LMENU_NODE_PICK_ITEM    (12)
#define LMENU_NODE_PUSH         (13)
#define LMENU_NODE_BAG          (14)
#define LMENU_NODE_SKILL        (15)
#define LMENU_NODE_BARTER_OPEN  (16)
#define LMENU_NODE_BARTER_HIDE  (17)
#define LMENU_NODE_GMAP_KICK    (18)
#define LMENU_NODE_GMAP_RULE    (19)
#define LMENU_NODE_VOTE_UP      (20)
#define LMENU_NODE_VOTE_DOWN    (21)

// Chosen actions
#define CHOSEN_NONE               (0)
#define CHOSEN_MOVE               (1)  // XXXX, YYYY, (Wait double click >> 8, Move type 0xFF), Cut path, Double click tick
#define CHOSEN_MOVE_TO_CRIT       (2)  // Crit Id, -, Move type, Cut path
#define CHOSEN_DIR                (3)  // CW - 0, CCW - 1
#define CHOSEN_SHOW_ITEM          (4)  // Item id
#define CHOSEN_HIDE_ITEM          (5)  // Item id
#define CHOSEN_USE_ITEM           (6)  // Item id, Item pid, Target type, Target id, Rate item, Some param
#define CHOSEN_MOVE_ITEM          (7)  // Item id, Item count, To slot
#define CHOSEN_MOVE_ITEM_CONT     (8)  // From cont, Item id, Count
#define CHOSEN_TAKE_ALL           (9)
#define CHOSEN_USE_SKL_ON_CRITTER (10) // Skill, Crit id
#define CHOSEN_USE_SKL_ON_ITEM    (11) // IsInventory, Skill, Id
#define CHOSEN_USE_SKL_ON_SCEN    (12) // Skill, Pid, XXXX, YYYY
#define CHOSEN_TALK_NPC           (13) // Crit id
#define CHOSEN_PICK_ITEM          (14) // Pid, HexX, HexY
#define CHOSEN_PICK_CRIT          (15) // Crit id, (loot - 0, push - 1)
#define CHOSEN_WRITE_HOLO         (16) // Holodisk id

// Proxy types
#define PROXY_SOCKS4            (1)
#define PROXY_SOCKS5            (2)
#define PROXY_HTTP              (3)

// Items collections
#define ITEMS_INVENTORY               (0)
#define ITEMS_USE                     (1)
#define ITEMS_BARTER                  (2)
#define ITEMS_BARTER_OFFER            (3)
#define ITEMS_BARTER_OPPONENT         (4)
#define ITEMS_BARTER_OPPONENT_OFFER   (5)
#define ITEMS_PICKUP                  (6)
#define ITEMS_PICKUP_FROM             (7)

// Interface elements
#define IFACE_NONE              (0)
#define IFACE_INT_ITEM          (1)
#define IFACE_INT_CHSLOT        (2)
#define IFACE_INT_INV           (3)
#define IFACE_INT_MENU          (4)
#define IFACE_INT_SKILL         (5)
#define IFACE_INT_MAP           (6)
#define IFACE_INT_CHAR          (7)
#define IFACE_INT_PIP           (8)
#define IFACE_INT_FIX           (9)
#define IFACE_INT_ADDMESS       (10)
#define IFACE_INT_FILTER1       (11)
#define IFACE_INT_FILTER2       (12)
#define IFACE_INT_FILTER3       (13)
#define IFACE_INT_COMBAT_TURN   (14)
#define IFACE_INT_COMBAT_END    (15)
#define IFACE_INT_MAIN          (16)
#define IFACE_INV_INV           (20)
#define IFACE_INV_SLOT1         (21)
#define IFACE_INV_SLOT2         (22)
#define IFACE_INV_ARMOR         (23)
#define IFACE_INV_SLOTS_EXT     (24)
#define IFACE_INV_SCRUP         (25)
#define IFACE_INV_SCRDW         (26)
#define IFACE_INV_OK            (27)
#define IFACE_INV_MAIN          (28)
#define IFACE_INV_RAD_ON        (29)
#define IFACE_USE_INV           (40)
#define IFACE_USE_SCRUP         (41)
#define IFACE_USE_SCRDW         (42)
#define IFACE_USE_CANCEL        (43)
#define IFACE_USE_MAIN          (44)
#define IFACE_GAME_MNEXT        (60)
#define IFACE_LOG_NAME          (80)
#define IFACE_LOG_PASS          (81)
#define IFACE_LOG_OK            (82)
#define IFACE_LOG_REG           (83)
#define IFACE_LOG_OPTIONS       (84)
#define IFACE_LOG_CREDITS       (85)
#define IFACE_LOG_EXIT          (86)
#define IFACE_DLG_MAIN          (100)
#define IFACE_DLG_SCR_UP        (101)
#define IFACE_DLG_SCR_DN        (102)
#define IFACE_DLG_ANSWER        (103)
#define IFACE_DLG_BARTER        (104)
#define IFACE_DLG_SAY           (105)
#define IFACE_BARTER_OFFER      (106)
#define IFACE_BARTER_TALK       (107)
#define IFACE_BARTER_CONT1      (108)
#define IFACE_BARTER_CONT2      (109)
#define IFACE_BARTER_CONT1O     (110)
#define IFACE_BARTER_CONT2O     (111)
#define IFACE_BARTER_CONT1SU    (112)
#define IFACE_BARTER_CONT1SD    (113)
#define IFACE_BARTER_CONT2SU    (114)
#define IFACE_BARTER_CONT2SD    (115)
#define IFACE_BARTER_CONT1OSU   (116)
#define IFACE_BARTER_CONT1OSD   (117)
#define IFACE_BARTER_CONT2OSU   (118)
#define IFACE_BARTER_CONT2OSD   (119)
#define IFACE_LMAP_OK           (120)
#define IFACE_LMAP_SCAN         (121)
#define IFACE_LMAP_LOHI         (122)
#define IFACE_LMAP_MAIN         (123)
#define IFACE_GMAP_MAP          (140)
#define IFACE_GMAP_TOWN         (141)
#define IFACE_GMAP_TABBTN       (142)
#define IFACE_GMAP_TABSCRUP     (143)
#define IFACE_GMAP_TABSCRDW     (144)
#define IFACE_GMAP_TOLOC        (145)
#define IFACE_GMAP_TOWN_BUT     (146)
#define IFACE_GMAP_VIEW_BUT     (147)
#define IFACE_GMAP_INV          (148)
#define IFACE_GMAP_MENU         (149)
#define IFACE_GMAP_CHA          (150)
#define IFACE_GMAP_PIP          (151)
#define IFACE_GMAP_FIX          (152)
#define IFACE_GMAP_MOVE_MAP     (153)
#define IFACE_SBOX_CANCEL       (160)
#define IFACE_SBOX_MAIN         (161)
#define IFACE_SBOX_FIRSTAID     (162)
#define IFACE_SBOX_DOCTOR       (163)
#define IFACE_SBOX_SNEAK        (164)
#define IFACE_SBOX_LOCKPICK     (165)
#define IFACE_SBOX_STEAL        (166)
#define IFACE_SBOX_TRAP         (167)
#define IFACE_SBOX_SCIENCE      (168)
#define IFACE_SBOX_REPAIR       (169)
#define IFACE_MOPT_RESUME       (180)
#define IFACE_MOPT_EXIT         (181)
#define IFACE_CHA_PRINT         (200)
#define IFACE_CHA_OK            (201)
#define IFACE_CHA_CANCEL        (202)
#define IFACE_CHA_PLUS          (203)
#define IFACE_CHA_MINUS         (204)
#define IFACE_CHA_MAIN          (205)
#define IFACE_CHA_NAME          (206)
#define IFACE_CHA_AGE           (207)
#define IFACE_CHA_SEX           (208)
#define IFACE_CHA_SW_SCRUP      (209)
#define IFACE_CHA_SW_SCRDN      (210)
#define IFACE_REG_SPEC_PL       (211)
#define IFACE_REG_SPEC_MN       (212)
#define IFACE_REG_TAGSKILL      (213)
#define IFACE_REG_TRAIT_L       (214)
#define IFACE_REG_TRAIT_R       (215)
#define IFACE_CHA_NAME_NAME     (220)
#define IFACE_CHA_NAME_PASS     (221)
#define IFACE_CHA_AGE_UP        (222)
#define IFACE_CHA_AGE_DOWN      (223)
#define IFACE_CHA_SEX_MALE      (224)
#define IFACE_CHA_SEX_FEMALE    (225)
#define IFACE_PERK_MAIN         (240)
#define IFACE_PERK_SCRUP        (241)
#define IFACE_PERK_SCRDN        (242)
#define IFACE_PERK_OK           (243)
#define IFACE_PERK_CANCEL       (244)
#define IFACE_PERK_PERKS        (245)
#define IFACE_TOWN_VIEW_MAIN    (250)
#define IFACE_TOWN_VIEW_BACK    (251)
#define IFACE_TOWN_VIEW_ENTER   (252)
#define IFACE_TOWN_VIEW_CONTOUR (253)
#define IFACE_PIP_STATUS        (260)
//#define IFACE_PIP_GAMES        (261)
#define IFACE_PIP_AUTOMAPS      (262)
#define IFACE_PIP_AUTOMAPS_SCR  (263)
#define IFACE_PIP_ARCHIVES      (264)
#define IFACE_PIP_CLOSE         (265)
#define IFACE_PIP_MAIN          (266)
#define IFACE_AIM_CANCEL        (280)
#define IFACE_AIM_HEAD          (281)
#define IFACE_AIM_LARM          (282)
#define IFACE_AIM_RARM          (283)
#define IFACE_AIM_TORSO         (284)
#define IFACE_AIM_RLEG          (285)
#define IFACE_AIM_LLEG          (286)
#define IFACE_AIM_EYES          (287)
#define IFACE_AIM_GROIN         (288)
#define IFACE_AIM_MAIN          (289)
#define IFACE_PUP_CONT1         (300)
#define IFACE_PUP_CONT2         (301)
#define IFACE_PUP_OK            (302)
#define IFACE_PUP_SCRUP1        (303)
#define IFACE_PUP_SCRDOWN1      (304)
#define IFACE_PUP_SCRUP2        (305)
#define IFACE_PUP_SCRDOWN2      (306)
#define IFACE_PUP_TAKEALL       (307)
#define IFACE_PUP_SCRCR_L       (308)
#define IFACE_PUP_SCRCR_R       (309)
#define IFACE_PUP_MAIN          (310)
#define IFACE_DIALOG_BTN        (320)
#define IFACE_DIALOG_MAIN       (321)
#define IFACE_ELEVATOR_MAIN     (330)
#define IFACE_ELEVATOR_BTN      (331)
#define IFACE_SAY_OK            (340)
#define IFACE_SAY_CANCEL        (341)
#define IFACE_SAY_MAIN          (342)
#define IFACE_SPLIT_MAIN        (360)
#define IFACE_SPLIT_UP          (361)
#define IFACE_SPLIT_DOWN        (362)
#define IFACE_SPLIT_ALL         (363)
#define IFACE_SPLIT_DONE        (364)
#define IFACE_SPLIT_CANCEL      (365)
#define IFACE_TIMER_MAIN        (380)
#define IFACE_TIMER_UP          (381)
#define IFACE_TIMER_DOWN        (382)
#define IFACE_TIMER_DONE        (383)
#define IFACE_TIMER_CANCEL      (384)
#define IFACE_FIX_DONE          (400)
#define IFACE_FIX_SCRUP         (401)
#define IFACE_FIX_SCRDN         (402)
#define IFACE_FIX_CHOOSE        (403)
#define IFACE_FIX_FIX           (404)
#define IFACE_FIX_MAIN          (405)
#define IFACE_IBOX_DONE         (420)
#define IFACE_IBOX_CANCEL       (421)
#define IFACE_IBOX_TITLE        (422)
#define IFACE_IBOX_TEXT         (423)
#define IFACE_IBOX_MAIN         (424)

#define ACCELERATE_NONE             (0)
#define ACCELERATE_CONSOLE          (1)
#define ACCELERATE_IBOX             (2)
#define ACCELERATE_PAGE_UP          (3)
#define ACCELERATE_PAGE_DOWN        (4)
#define ACCELERATE_MESSBOX          (5)
#define ACCELERATE_SPLIT_UP         (6)
#define ACCELERATE_SPLIT_DOWN       (7)
#define ACCELERATE_TIMER_UP         (8)
#define ACCELERATE_TIMER_DOWN       (9)
#define ACCELERATE_USE_SCRUP        (10)
#define ACCELERATE_USE_SCRDOWN      (11)
#define ACCELERATE_INV_SCRUP        (12)
#define ACCELERATE_INV_SCRDOWN      (13)
#define ACCELERATE_PUP_SCRUP1       (14)
#define ACCELERATE_PUP_SCRDOWN1     (15)
#define ACCELERATE_PUP_SCRUP2       (16)
#define ACCELERATE_PUP_SCRDOWN2     (17)
#define ACCELERATE_CHA_SW_SCRUP     (18)
#define ACCELERATE_CHA_SW_SCRDOWN   (19)
#define ACCELERATE_CHA_PLUS         (20)
#define ACCELERATE_CHA_MINUS        (21)
#define ACCELERATE_CHA_AGE_UP       (22)
#define ACCELERATE_CHA_AGE_DOWN     (23)
#define ACCELERATE_BARTER_CONT1SU   (24)
#define ACCELERATE_BARTER_CONT1SD   (25)
#define ACCELERATE_BARTER_CONT2SU   (26)
#define ACCELERATE_BARTER_CONT2SD   (27)
#define ACCELERATE_BARTER_CONT1OSU  (28)
#define ACCELERATE_BARTER_CONT1OSD  (29)
#define ACCELERATE_BARTER_CONT2OSU  (30)
#define ACCELERATE_BARTER_CONT2OSD  (31)
#define ACCELERATE_PERK_SCRUP       (32)
#define ACCELERATE_PERK_SCRDOWN     (33)
#define ACCELERATE_DLG_TEXT_UP      (34)
#define ACCELERATE_DLG_TEXT_DOWN    (35)

#endif // __CLIENT__