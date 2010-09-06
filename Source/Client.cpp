#include "StdAfx.h"
#include "Client.h"
#include "Access.h"
#include "Defence.h"

// Check buffer for error
#define CHECK_IN_BUFF_ERROR \
	if(Bin.IsError())\
	{\
		WriteLog(__FUNCTION__" - Wrong MSG data.\n");\
		return;\
	}

void* zlib_alloc_(void *opaque, unsigned int items, unsigned int size){return calloc(items, size);}
void zlib_free_(void *opaque, void *address){free(address);}

FOClient* FOClient::Self=NULL;
bool FOClient::SpritesCanDraw=false;
static DWORD* UID4=NULL;
FOClient::FOClient():Active(0),Wnd(NULL),DInput(NULL),Keyboard(NULL),Mouse(NULL)
{
	Self=this;

	ComLen=4096;
	ComBuf=new char[ComLen];
	ZStreamOk=false;
	NetState=STATE_DISCONNECT;
	Sock=INVALID_SOCKET;
	BytesReceive=0;
	BytesRealReceive=0;
	BytesSend=0;
	InitNetReason=0;

	Chosen=NULL;
	FPS=0;
	PingTime=0;
	PingTick=0;
	PingCallTick=0;
	IsTurnBased=false;
	TurnBasedTime=0;
	DaySumRGB=0;
	CurMode=0;
	CurModeLast=0;

	GmapRelief=NULL;
	GmapCar.Car=NULL;
	Animations.resize(10000);

	GraphBuilder=NULL;
	MediaControl=NULL;
	WindowLess=NULL;
	MediaSeeking=NULL;
	BasicAudio=NULL;
	VMRFilter=NULL;
	FilterConfig=NULL;
	MusicVolumeRestore=-1;
	UIDFail=false;

	MoveLastHx=-1;
	MoveLastHy=-1;

	SaveLoadDraft=NULL;
}

void _PreRestore()
{
	FOClient::Self->HexMngr.PreRestore();

	// Save/load surface
	if(Singleplayer)
	{
		SAFEREL(FOClient::Self->SaveLoadDraft);
		FOClient::Self->SaveLoadDraftValid=false;
	}
}

void _PostRestore()
{
	FOClient::Self->HexMngr.PostRestore();
	FOClient::Self->SetDayTime(true);

	// Save/load surface
	if(Singleplayer)
	{
		SAFEREL(FOClient::Self->SaveLoadDraft);
		if(FAILED(FOClient::Self->SprMngr.GetDevice()->CreateRenderTarget(SAVE_LOAD_IMAGE_WIDTH,SAVE_LOAD_IMAGE_HEIGHT,
			D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&FOClient::Self->SaveLoadDraft,NULL))) WriteLog("Create save/load draft surface fail.\n");
		FOClient::Self->SaveLoadDraftValid=false;
	}
}

DWORD* UID1;
bool FOClient::Init(HWND hwnd)
{
	WriteLog("Engine initialization...\n");

	STATIC_ASSERT(sizeof(DWORD)==4);
	STATIC_ASSERT(sizeof(WORD)==2);
	STATIC_ASSERT(sizeof(BYTE)==1);
	STATIC_ASSERT(sizeof(int)==4);
	STATIC_ASSERT(sizeof(short)==2);
	STATIC_ASSERT(sizeof(char)==1);
	STATIC_ASSERT(sizeof(bool)==1);
	STATIC_ASSERT(sizeof(Item::ItemData)==92);
	STATIC_ASSERT(sizeof(GmapLocation)==16);
	STATIC_ASSERT(sizeof(ScenToSend)==32);
	STATIC_ASSERT(sizeof(ProtoItem)==184);
	GET_UID0(UID0);

	Wnd=hwnd;

	Keyb::InitKeyb();
	if(!InitDInput()) return false;

	// File manager
	FileManager::SetDataPath(GameOpt.FoDataPath.c_str());
	if(!FileManager::LoadDataFile(GameOpt.MasterPath.c_str()))
	{
		MessageBox(Wnd,"MASTER.DAT not found.","Fallout Online",MB_OK);
		return false;
	}
	if(!FileManager::LoadDataFile(GameOpt.CritterPath.c_str()))
	{
		MessageBox(Wnd,"CRITTER.DAT not found.","Fallout Online",MB_OK);
		return false;
	}
	FileManager::LoadDataFile(GameOpt.FoPatchPath.c_str());

	// Cache
	bool refresh_cache=(!Singleplayer && !strstr(GetCommandLine(),"-DefCache") && !Crypt.IsCacheTable(Str::Format("%s%s.%u.cache",FileMngr.GetDataPath(PT_DATA),GameOpt.Host.c_str(),GameOpt.Port)));

	if(!Crypt.SetCacheTable(Str::Format("%sdefault.cache",FileMngr.GetDataPath(PT_DATA))))
	{
		WriteLog(__FUNCTION__" - Can't set default cache.\n");
		return false;
	}
	if(!Singleplayer && !strstr(GetCommandLine(),"-DefCache") && !Crypt.SetCacheTable(Str::Format("%s%s.%u.cache",FileMngr.GetDataPath(PT_DATA),GameOpt.Host.c_str(),GameOpt.Port)))
	{
		WriteLog(__FUNCTION__" - Can't set new cache.\n");
		return false;
	}
	if(Singleplayer && !strstr(GetCommandLine(),"-DefCache") && !Crypt.SetCacheTable(Str::Format("%ssingleplayer.cache",FileMngr.GetDataPath(PT_DATA))))
	{
		WriteLog(__FUNCTION__" - Can't set single player cache.\n");
		return false;
	}

	// User and password
	if(GameOpt.Name.empty() && GameOpt.Pass.empty() && !Singleplayer)
	{
		bool fail=false;

		DWORD len;
		char* str=(char*)Crypt.GetCache("__name",len);
		if(str && len<=min(GameOpt.MaxNameLength,MAX_NAME)+1) GameOpt.Name=str;
		else fail=true;
		delete[] str;

		str=(char*)Crypt.GetCache("__pass",len);
		if(str && len<=min(GameOpt.MaxNameLength,MAX_NAME)+1) GameOpt.Pass=str;
		else fail=true;
		delete[] str;

		if(fail)
		{
			GameOpt.Name="login";
			GameOpt.Pass="password";
			refresh_cache=true;
		}
	}

	// Sprite manager
	SpriteMngrParams params;
	params.WndHeader=hwnd;
	params.PreRestoreFunc=&_PreRestore;
	params.PostRestoreFunc=&_PostRestore;
	if(!SprMngr.Init(params)) return false;
	GET_UID1(UID1);

	// Set sprite manager pointer to critters class
	CritterCl::SprMngr=&SprMngr;

	// Fonts
	if(!SprMngr.LoadFont(FONT_FO,"fnt_def",1)) return false;
	if(!SprMngr.LoadFont(FONT_NUM,"fnt_num",1)) return false;
	if(!SprMngr.LoadFont(FONT_BIG_NUM,"fnt_big_num",1)) return false;
	if(!SprMngr.LoadFont(FONT_SAND_NUM,"fnt_sand_num",1)) return false;
	if(!SprMngr.LoadFontAAF(FONT_SPECIAL,"font0.aaf",1)) return false;
	if(!SprMngr.LoadFontAAF(FONT_DEF,"font1.aaf",1)) return false;
	if(!SprMngr.LoadFontAAF(FONT_THIN,"font2.aaf",1)) return false;
	if(!SprMngr.LoadFontAAF(FONT_FAT,"font3.aaf",1)) return false;
	if(!SprMngr.LoadFontAAF(FONT_BIG,"font4.aaf",1)) return false;
	SprMngr.SetDefaultFont(FONT_DEF,COLOR_TEXT);

	// Sound manager
	IniParser cfg;
	cfg.LoadFile(CLIENT_CONFIG_FILE,PT_ROOT);
	SndMngr.Init(Wnd);
	SndMngr.SetSoundVolume(cfg.GetInt(CLIENT_CONFIG_APP,"SoundVolume",100));
	SndMngr.SetMusicVolume(cfg.GetInt(CLIENT_CONFIG_APP,"MusicVolume",100));

	// Language Packs
	char lang_name[MAX_FOTEXT];
	cfg.GetStr(CLIENT_CONFIG_APP,"Language",DEFAULT_LANGUAGE,lang_name);
	if(strlen(lang_name)!=4) StringCopy(lang_name,DEFAULT_LANGUAGE);
	Str::Lwr(lang_name);

	CurLang.Init(lang_name,PT_TEXTS);

	MsgText=&CurLang.Msg[TEXTMSG_TEXT];
	MsgDlg=&CurLang.Msg[TEXTMSG_DLG];
	MsgItem=&CurLang.Msg[TEXTMSG_ITEM];
	MsgGame=&CurLang.Msg[TEXTMSG_GAME];
	MsgGM=&CurLang.Msg[TEXTMSG_GM];
	MsgCombat=&CurLang.Msg[TEXTMSG_COMBAT];
	MsgQuest=&CurLang.Msg[TEXTMSG_QUEST];
	MsgHolo=&CurLang.Msg[TEXTMSG_HOLO];
	MsgCraft=&CurLang.Msg[TEXTMSG_CRAFT];
	MsgInternal=&CurLang.Msg[TEXTMSG_INTERNAL];
	MsgUserHolo=new FOMsg;
	MsgUserHolo->LoadMsgFile(USER_HOLO_TEXTMSG_FILE,PT_TEXTS);

	// CritterCl types
	CritType::InitFromMsg(MsgInternal);
	GET_UID2(UID2);

	// Lang
	Keyb::Lang=LANG_ENG;
	if(!_stricmp(lang_name,"russ")) Keyb::Lang=LANG_RUS;

	// Resource manager
	ResMngr.Refresh(&SprMngr);

	// Wait screen
	ScreenModeMain=SCREEN_WAIT;
	CurMode=CUR_WAIT;
	WaitPic=ResMngr.GetRandomSplash();
	if(SprMngr.BeginScene(D3DCOLOR_XRGB(0,0,0)))
	{
		WaitDraw();
		SprMngr.EndScene();
	}

	// Ini options
	if(!InitIfaceIni()) return false;

	// Scripts
	if(!ReloadScripts()) refresh_cache=true;

	// Names
	FONames::GenerateFoNames(PT_DATA);

	// Load interface
	int res=InitIface();
	if(res!=0)
	{
		WriteLog("Init interface fail, error<%d>.\n",res);
		return false;
	}

	// Quest Manager
	QuestMngr.Init(MsgQuest);

	// Item manager
	if(!ItemMngr.Init()) return false;

	// Item prototypes
	ItemMngr.ClearProtos();
	DWORD protos_len;
	BYTE* protos=Crypt.GetCache("__item_protos",protos_len);
	if(protos)
	{
		BYTE* protos_uc=Crypt.Uncompress(protos,protos_len,15);
		delete[] protos;
		if(protos_uc)
		{
			ProtoItemVec proto_items;
			proto_items.resize(protos_len/sizeof(ProtoItem));
			memcpy((void*)&proto_items[0],protos_uc,protos_len);
			ItemMngr.ParseProtos(proto_items);
			//ItemMngr.PrintProtosHash();
			delete[] protos_uc;
		}
	}

	// MrFixit
	MrFixit.LoadCrafts(*MsgCraft);
	MrFixit.GenerateNames(*MsgGame,*MsgItem); // After Item manager init

	// Hex manager
	if(!HexMngr.Init(&SprMngr)) return false;
	GET_UID3(UID3);

	// Other
	NetState=STATE_DISCONNECT;
	SetGameColor(COLOR_IFACE);
	SetCurPos(MODE_WIDTH/2,MODE_HEIGHT/2);
	ShowCursor(0);
	ScreenOffsX=0;
	ScreenOffsY=0;
	ScreenOffsXf=0.0f;
	ScreenOffsYf=0.0f;
	ScreenOffsStep=0.0f;
	ScreenOffsNextTick=0;
	ScreenMirrorTexture=NULL;
	ScreenMirrorEndTick=0;
	ScreenMirrorStart=false;
	RebuildLookBorders=false;
	DrawLookBorders=false;
	DrawShootBorders=false;
	LookBorders.clear();
	ShootBorders.clear();
	WriteLog("Engine initialization complete.\n");
	Active=true;
	GET_UID4(UID4);

	// Start game
	// Check cache
	if(refresh_cache)
	{
		NetState=STATE_INIT_NET;
		InitNetReason=INIT_NET_REASON_CACHE;
	}
	// Begin game
	else if(strstr(GetCommandLine(),"-Start") && !Singleplayer)
	{
		LogTryConnect();
	}
	// Skip intro
	else if(!strstr(GetCommandLine(),"-SkipIntro"))
	{
		if(MsgGame->Count(STR_MUSIC_MAIN_THEME)) MusicAfterVideo=MsgGame->GetStr(STR_MUSIC_MAIN_THEME);
		for(DWORD i=STR_VIDEO_INTRO_BEGIN;i<STR_VIDEO_INTRO_END;i++)
		{
			if(MsgGame->Count(i)==1) AddVideo(MsgGame->GetStr(i,0),NULL,true,false);
			else if(MsgGame->Count(i)==2) AddVideo(MsgGame->GetStr(i,0),MsgGame->GetStr(i,1),true,false);
		}
	}

	return true;
}

int FOClient::InitDInput()
{
	WriteLog("DirectInput initialization...\n");

    HRESULT hr=DirectInput8Create(GetModuleHandle(NULL),DIRECTINPUT_VERSION,IID_IDirectInput8,(void**)&DInput,NULL);
	if(hr!=DI_OK)
	{
		WriteLog("Can't create DirectInput.\n");
		return false;
	}

    // Obtain an interface to the system keyboard device.
    hr=DInput->CreateDevice(GUID_SysKeyboard,&Keyboard,NULL);
	if(hr!=DI_OK)
	{
		WriteLog("Can't create GUID_SysKeyboard.\n");
		return false;
	}

    hr=DInput->CreateDevice(GUID_SysMouse,&Mouse,NULL);
	if(hr!=DI_OK)
	{
		WriteLog("Can't create GUID_SysMouse.\n");
		return false;
	}
    // Set the data format to "keyboard format" - a predefined data format 
    // This tells DirectInput that we will be passing an array
    // of 256 bytes to IDirectInputDevice::GetDeviceState.
    hr=Keyboard->SetDataFormat(&c_dfDIKeyboard);
	if(hr!=DI_OK)
	{
		WriteLog("Unable to set data format for keyboard.\n");
		return false;
	}

    hr=Mouse->SetDataFormat(&c_dfDIMouse2);
	if(hr!=DI_OK)
	{
		WriteLog("Unable to set data format for mouse.\n");
		return false;
	}


    hr=Keyboard->SetCooperativeLevel( Wnd,DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if(hr!=DI_OK)
	{
		WriteLog("Unable to set cooperative level for keyboard.\n");
		return false;
	}

	hr=Mouse->SetCooperativeLevel( Wnd,DISCL_FOREGROUND | (GameOpt.FullScreen?DISCL_EXCLUSIVE:DISCL_NONEXCLUSIVE));
//	hr=Mouse->SetCooperativeLevel( Wnd,DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	if(hr!=DI_OK)
	{
		WriteLog("Unable to set cooperative level for mouse.\n");
		return false;
	}

    // Important step to use buffered device data!
    DIPROPDWORD dipdw;
    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = DI_BUF_SIZE; // Arbitrary buffer size

    hr=Keyboard->SetProperty(DIPROP_BUFFERSIZE,&dipdw.diph);
	if(hr!=DI_OK)
	{
		WriteLog("Unable to set property for keyboard.\n");
		return false;
	}

    hr=Mouse->SetProperty(DIPROP_BUFFERSIZE,&dipdw.diph);
	if(hr!=DI_OK)
	{
		WriteLog("Unable to set property for mouse.\n");
		return false;
	}

    // Acquire the newly created device
	if(Keyboard->Acquire()!=DI_OK) WriteLog("Can't acquire keyboard.\n");
	if(Mouse->Acquire()!=DI_OK) WriteLog("Can't acquire mouse.\n");

	WriteLog("DirectInput initialization complete.\n");
	return true;
}

void FOClient::Finish()
{
	WriteLog("Engine finish...\n");

	SAFEREL(SaveLoadDraft);

	Keyb::ClearKeyb();
	NetDisconnect();
	ResMngr.Finish();
	HexMngr.Clear();
	SprMngr.Clear();
	SndMngr.Clear();
	QuestMngr.Clear();
	MrFixit.Finish();
	Script::Finish();

	if(Keyboard) Keyboard->Unacquire();
	SAFEREL(Keyboard);
	if(Mouse) Mouse->Unacquire();
	SAFEREL(Mouse);
	SAFEREL(DInput);
	SAFEDELA(ComBuf);

	for(PCharPairVecIt it=IntellectWords.begin(),end=IntellectWords.end();it!=end;++it)
	{
		delete[] (*it).first;
		delete[] (*it).second;
	}
	IntellectWords.clear();
	for(PCharPairVecIt it=IntellectSymbols.begin(),end=IntellectSymbols.end();it!=end;++it)
	{
		delete[] (*it).first;
		delete[] (*it).second;
	}
	IntellectSymbols.clear();
	FileManager::EndOfWork();

	Active=false;
	WriteLog("Engine finish complete.\n");
}

void FOClient::ClearCritters()
{
	HexMngr.ClearCritters();
//	CritsNames.clear();
	Chosen=NULL;
	ChosenAction.clear();
	InvContInit.clear();
	BarterCont1oInit=BarterCont2Init=BarterCont2oInit=PupCont2Init=InvContInit;
	PupCont2.clear();
	InvCont=BarterCont1=PupCont1=UseCont=BarterCont1o=BarterCont2=BarterCont2o=PupCont2;
}

void FOClient::AddCritter(CritterCl* cr)
{
	DWORD fading=0;
	CritterCl* cr_=GetCritter(cr->GetId());
	if(cr_) fading=cr_->FadingTick;
	EraseCritter(cr->GetId());
	if(HexMngr.IsMapLoaded())
	{
		Field& f=HexMngr.GetField(cr->GetHexX(),cr->GetHexY());
		if(f.Crit && f.Crit->IsFinishing()) EraseCritter(f.Crit->GetId());
	}
	if(cr->IsChosen()) Chosen=cr;
	HexMngr.AddCrit(cr);
	cr->FadingTick=Timer::GameTick()+FADING_PERIOD-(fading>Timer::GameTick()?fading-Timer::GameTick():0);
}

void FOClient::EraseCritter(DWORD remid)
{
	if(Chosen && Chosen->GetId()==remid) Chosen=NULL;
	HexMngr.EraseCrit(remid);
}

void FOClient::LookBordersPrepare()
{
	if(!DrawLookBorders && !DrawShootBorders) return;

	LookBorders.clear();
	ShootBorders.clear();
	if(HexMngr.IsMapLoaded() && Chosen)
	{
		DWORD dist=Chosen->GetLook();
		WORD base_hx=Chosen->GetHexX();
		WORD base_hy=Chosen->GetHexY();
		int hx=base_hx;
		int hy=base_hy;
		int dir=Chosen->GetDir();
		DWORD dist_shoot=Chosen->GetAttackDist();
		WORD maxhx=HexMngr.GetMaxHexX();
		WORD maxhy=HexMngr.GetMaxHexY();
		const int dirs[6]={2,3,4,5,0,1};
		bool seek_start=true;
		for(int i=0;i<6;i++)
		{
			for(DWORD j=0;j<dist;j++)
			{
				if(seek_start)
				{
					// Move to start position
					for(DWORD l=0;l<dist;l++) MoveHexByDirUnsafe(hx,hy,0);
					seek_start=false;
					j=-1;
				}
				else
				{
					// Move to next hex
					MoveHexByDirUnsafe(hx,hy,dirs[i]);
				}

				WORD hx_=CLAMP(hx,0,maxhx-1);
				WORD hy_=CLAMP(hy,0,maxhy-1);
				if(FLAG(GameOpt.LookChecks,LOOK_CHECK_DIR))
				{
					int dir_=GetFarDir(base_hx,base_hy,hx_,hy_);
					int ii=(dir>dir_?dir-dir_:dir_-dir);
					if(ii>3) ii=6-ii;
					DWORD dist_=dist-dist*GameOpt.LookDir[ii]/100;
					WordPair block;
					HexMngr.TraceBullet(base_hx,base_hy,hx_,hy_,dist_,0.0f,NULL,false,NULL,0,NULL,&block,NULL,false);
					hx_=block.first;
					hy_=block.second;
				}

				if(FLAG(GameOpt.LookChecks,LOOK_CHECK_TRACE))
				{
					WordPair block;
					HexMngr.TraceBullet(base_hx,base_hy,hx_,hy_,0,0.0f,NULL,false,NULL,0,NULL,&block,NULL,true);
					hx_=block.first;
					hy_=block.second;
				}

				WORD hx__=hx_;
				WORD hy__=hy_;
				DWORD dist_look=DistGame(base_hx,base_hy,hx_,hy_);
				WordPair block;
				HexMngr.TraceBullet(base_hx,base_hy,hx_,hy_,min(dist_look,dist_shoot),0.0f,NULL,false,NULL,0,NULL,&block,NULL,true);
				hx__=block.first;
				hy__=block.second;

				int x,y,x_,y_;
				HexMngr.GetHexCurrentPosition(hx_,hy_,x,y);
				HexMngr.GetHexCurrentPosition(hx__,hy__,x_,y_);
				LookBorders.push_back(PrepPoint(x+16,y+6,D3DCOLOR_ARGB(80,0,255,0),(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
				ShootBorders.push_back(PrepPoint(x_+16,y_+6,D3DCOLOR_ARGB(80,255,0,0),(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
			}
		}

		if(LookBorders.size()<2) LookBorders.clear();
		else LookBorders.push_back(*LookBorders.begin());
		if(ShootBorders.size()<2) ShootBorders.clear();
		else ShootBorders.push_back(*ShootBorders.begin());
	}
}

void FOClient::LookBordersDraw()
{
	if(RebuildLookBorders)
	{
		LookBordersPrepare();
		RebuildLookBorders=false;
	}
	if(DrawLookBorders) SprMngr.DrawPoints(LookBorders,D3DPT_LINESTRIP,&GameOpt.SpritesZoom);
	if(DrawShootBorders) SprMngr.DrawPoints(ShootBorders,D3DPT_LINESTRIP,&GameOpt.SpritesZoom);
}

int FOClient::MainLoop()
{
	// Fps counter
	static DWORD last_call=Timer::FastTick();
	static WORD call_cnt=0;
    if((Timer::FastTick()-last_call)>=1000)
	{
		FPS=call_cnt;
		call_cnt=0;
		last_call=Timer::FastTick();
	}
	else call_cnt++;

	if(!Active) return 0;

	// Singleplayer data synchronization
	if(Singleplayer)
	{
		bool pause=SingleplayerData.Pause;

		if(!pause)
		{
			int main_screen=GetMainScreen();
			if((main_screen!=SCREEN_GAME && main_screen!=SCREEN_GLOBAL_MAP && main_screen!=SCREEN_WAIT) ||
				IsScreenPresent(SCREEN__MENU_OPTION)) pause=true;
		}

		SingleplayerData.Lock(); // Read data
		if(pause!=SingleplayerData.Pause)
		{
			SingleplayerData.Pause=pause;
			Timer::SetGamePause(pause);
		}
		SingleplayerData.Unlock(); // Write data
	}

	// Network
	// Init Net
	if(NetState==STATE_INIT_NET)
	{
		ShowMainScreen(SCREEN_WAIT);

		if(!InitNet())
		{
			ShowMainScreen(SCREEN_LOGIN);
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_CONN_FAIL));
			return 1;
		}

		if(InitNetReason==INIT_NET_REASON_CACHE) Net_SendLogIn(NULL,NULL);
		else if(InitNetReason==INIT_NET_REASON_LOGIN) Net_SendLogIn(GameOpt.Name.c_str(),GameOpt.Pass.c_str());
		else if(InitNetReason==INIT_NET_REASON_REG) Net_SendCreatePlayer(RegNewCr);
		else if(InitNetReason==INIT_NET_REASON_LOAD) Net_SendSaveLoad(false,SaveLoadFileName.c_str(),NULL);
		else NetDisconnect();
	}

	// Parse Net
	if(NetState!=STATE_DISCONNECT) ParseSocket();

	// Exit in Login screen if net disconnect
	if(NetState==STATE_DISCONNECT && !IsMainScreen(SCREEN_LOGIN) && !IsMainScreen(SCREEN_REGISTRATION) && !IsMainScreen(SCREEN_CREDITS)) ShowMainScreen(SCREEN_LOGIN);

	// Input
	ConsoleProcess();
	IboxProcess();
	ParseKeyboard();
	ParseMouse();

	// Process
	SoundProcess();
	AnimProcess();

	// Game time
	WORD full_second=GameOpt.FullSecond;
	ProcessGameTime();
	if(full_second!=GameOpt.FullSecond) SetDayTime(false);

	if(IsMainScreen(SCREEN_GLOBAL_MAP))
	{
		CrittersProcess();
		GmapProcess();
		LMenuTryCreate();
	}
	else if(IsMainScreen(SCREEN_GAME) && HexMngr.IsMapLoaded())
	{
		int screen=GetActiveScreen();
		if(screen==SCREEN_NONE || screen==SCREEN__TOWN_VIEW || HexMngr.AutoScroll.Active)
		{
			if(HexMngr.Scroll())
			{
				LMenuSet(LMENU_OFF);
				LookBordersPrepare();
			}
		}
		CrittersProcess();
		HexMngr.ProcessItems();
		HexMngr.ProcessRain();
		LMenuTryCreate();
	}
	else if(IsMainScreen(SCREEN_CREDITS))
	{
		//CreditsDraw();
	}

	if(IsScreenPresent(SCREEN__ELEVATOR)) ElevatorProcess();

	// Script loop
	static DWORD next_call=0;
	if(Timer::FastTick()>=next_call)
	{
		DWORD wait_tick=60000;
		if(Script::PrepareContext(ClientFunctions.Loop,CALL_FUNC_STR,"Game") && Script::RunPrepared()) wait_tick=Script::GetReturnedDword();
		next_call=Timer::FastTick()+wait_tick;
	}
	Script::CollectGarbage(false);

	// Video
	if(IsVideoPlayed())
	{
		LONGLONG cur,stop;
		if(!MediaSeeking || FAILED(MediaSeeking->GetPositions(&cur,&stop)) || cur>=stop) NextVideo();
		return 0;
	}

	// Render
	if(!SprMngr.BeginScene(IsMainScreen(SCREEN_GAME)?(GameOpt.ScreenClear?D3DCOLOR_XRGB(100,100,100):0):D3DCOLOR_XRGB(0,0,0))) return 0;

	ProcessScreenEffectQuake();
	DrawIfaceLayer(0);

	switch(GetMainScreen())
	{
	case SCREEN_GAME:
		if(HexMngr.IsMapLoaded())
		{
			GameDraw();
			if(SaveLoadProcessDraft) SaveLoadFillDraft();
			ProcessScreenEffectMirror();
			DrawIfaceLayer(1);
			IntDraw();
		}
		else
		{
			WaitDraw();
		}
		break;
	case SCREEN_GLOBAL_MAP:
		{
			GmapDraw();
			if(SaveLoadProcessDraft) SaveLoadFillDraft();
		}
		break;
	case SCREEN_LOGIN: LogDraw(); break;
	case SCREEN_REGISTRATION: ChaDraw(true); break;
	case SCREEN_CREDITS: CreditsDraw(); break;
	default:
	case SCREEN_WAIT: WaitDraw(); break;
	}

	DrawIfaceLayer(2);
	ConsoleDraw();
	MessBoxDraw();
	DrawIfaceLayer(3);

	/*if(!GameOpt.DisableDrawScreens)
	{
		for(size_t i=0,j=ScreenMode.size();i<j;i++)
		{
			switch(ScreenMode[i])
			{
			case SCREEN__INVENTORY:  InvDraw(); break;
			case SCREEN__PICKUP:     PupDraw(); break;
			case SCREEN__MINI_MAP:   LmapDraw(); break;
			case SCREEN__DIALOG:     DlgDraw(); break;
			case SCREEN__PIP_BOY:    PipDraw(); break;
			case SCREEN__FIX_BOY:    FixDraw(); break;
			case SCREEN__MENU_OPTION:MoptDraw(); break;
			case SCREEN__CHARACTER:  ChaDraw(false); break;
			case SCREEN__AIM:        AimDraw(); break;
			case SCREEN__SPLIT:      SplitDraw();break;
			case SCREEN__TIMER:      TimerDraw();break;
			case SCREEN__DIALOGBOX:  DlgboxDraw();break;
			case SCREEN__ELEVATOR:   ElevatorDraw();break;
			case SCREEN__SAY:        SayDraw();break;
			case SCREEN__CHA_NAME:   ChaNameDraw();break;
			case SCREEN__CHA_AGE:    ChaAgeDraw();break;
			case SCREEN__CHA_SEX:    ChaSexDraw();break;
			case SCREEN__GM_TOWN:    GmapTownDraw();break;
			case SCREEN__INPUT_BOX:  IboxDraw(); break;
			case SCREEN__SKILLBOX:   SboxDraw(); break;
			case SCREEN__USE:        UseDraw(); break;
			case SCREEN__PERK:       PerkDraw(); break;
			default: break;
			}
		}
	}*/
	DrawIfaceLayer(4);
	LMenuDraw();
	CurDraw();
	DrawIfaceLayer(5);
	SprMngr.Flush();
	ProcessScreenEffectFading();

	SprMngr.EndScene();
	return 1;
}

void FOClient::ScreenFade(DWORD time, DWORD from_color, DWORD to_color, bool push_back)
{
	if(!push_back || ScreenEffects.empty())
	{
		ScreenEffects.push_back(ScreenEffect(Timer::FastTick(),time,from_color,to_color));
	}
	else
	{
		DWORD last_tick=0;
		for(ScreenEffectVecIt it=ScreenEffects.begin();it!=ScreenEffects.end();++it)
		{
			ScreenEffect& e=(*it);
			if(e.BeginTick+e.Time>last_tick) last_tick=e.BeginTick+e.Time;
		}
		ScreenEffects.push_back(ScreenEffect(last_tick,time,from_color,to_color));
	}
}

void FOClient::ScreenQuake(int noise, DWORD time)
{
	GameOpt.ScrOx-=ScreenOffsX;
	GameOpt.ScrOy-=ScreenOffsY;
	ScreenOffsX=Random(0,1)?noise:-noise;
	ScreenOffsY=Random(0,1)?noise:-noise;
	ScreenOffsXf=ScreenOffsX;
	ScreenOffsYf=ScreenOffsY;
	ScreenOffsStep=fabs(ScreenOffsXf)/(time/30);
	GameOpt.ScrOx+=ScreenOffsX;
	GameOpt.ScrOy+=ScreenOffsY;
	ScreenOffsNextTick=Timer::GameTick()+30;
}

void FOClient::ProcessScreenEffectFading()
{
	static PointVec six_points;
	if(six_points.empty()) SprMngr.PrepareSquare(six_points,FLTRECT(0,0,MODE_WIDTH,MODE_HEIGHT),0);

	for(ScreenEffectVecIt it=ScreenEffects.begin();it!=ScreenEffects.end();)
	{
		ScreenEffect& e=(*it);
		if(Timer::FastTick()>=e.BeginTick+e.Time)
		{
			it=ScreenEffects.erase(it);
			continue;
		}
		else if(Timer::FastTick()>=e.BeginTick)
		{
			int proc=Procent(e.Time,Timer::FastTick()-e.BeginTick)+1;
			int res[4];

			for(int i=0;i<4;i++)
			{
				int sc=((BYTE*)&e.StartColor)[i];
				int ec=((BYTE*)&e.EndColor)[i];
				int dc=ec-sc;
				res[i]=sc+dc*proc/100;
			}

			DWORD color=D3DCOLOR_ARGB(res[3],res[2],res[1],res[0]);
			for(int i=0;i<6;i++) six_points[i].PointColor=color;

			SprMngr.DrawPoints(six_points,D3DPT_TRIANGLELIST);
		}
		it++;
	}
}

void FOClient::ProcessScreenEffectQuake()
{
	if((ScreenOffsX || ScreenOffsY) && Timer::GameTick()>=ScreenOffsNextTick)
	{
		GameOpt.ScrOx-=ScreenOffsX;
		GameOpt.ScrOy-=ScreenOffsY;
		if(ScreenOffsXf<0.0f) ScreenOffsXf+=ScreenOffsStep;
		else if(ScreenOffsXf>0.0f) ScreenOffsXf-=ScreenOffsStep;
		if(ScreenOffsYf<0.0f) ScreenOffsYf+=ScreenOffsStep;
		else if(ScreenOffsYf>0.0f) ScreenOffsYf-=ScreenOffsStep;
		ScreenOffsXf=-ScreenOffsXf;
		ScreenOffsYf=-ScreenOffsYf;
		ScreenOffsX=ScreenOffsXf;
		ScreenOffsY=ScreenOffsYf;
		GameOpt.ScrOx+=ScreenOffsX;
		GameOpt.ScrOy+=ScreenOffsY;
		ScreenOffsNextTick=Timer::GameTick()+30;
	}
}

void FOClient::ProcessScreenEffectMirror()
{
/*
	if(ScreenMirrorStart)
	{
		ScreenQuake(10,1000);
		ScreenMirrorX=0;
		ScreenMirrorY=0;
		SAFEREL(ScreenMirrorTexture);
		ScreenMirrorEndTick=Timer::FastTick()+1000;
		ScreenMirrorStart=false;

		if(FAILED(SprMngr.GetDevice()->CreateTexture(MODE_WIDTH,MODE_HEIGHT,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&ScreenMirrorTexture))) return;
		LPDIRECT3DSURFACE8 mirror=NULL;
		LPDIRECT3DSURFACE8 back_buf=NULL;
		if(SUCCEEDED(ScreenMirrorTexture->GetSurfaceLevel(0,&mirror))
		&& SUCCEEDED(SprMngr.GetDevice()->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back_buf)))
			D3DXLoadSurfaceFromSurface(mirror,NULL,NULL,back_buf,NULL,NULL,D3DX_DEFAULT,0);
		SAFEREL(back_buf);
		SAFEREL(mirror);
	}
	else if(ScreenMirrorEndTick)
	{
		if(Timer::FastTick()>=ScreenMirrorEndTick)
		{
			SAFEREL(ScreenMirrorTexture);
			ScreenMirrorEndTick=0;
		}
		else
		{
			MYVERTEX vb_[6];
			vb_[0].x=0+ScreenMirrorX-0.5f;
			vb_[0].y=MODE_HEIGHT+ScreenMirrorY-0.5f;
			vb_[0].tu=0.0f;
			vb_[0].tv=1.0f;
			vb_[0].Diffuse=0x7F7F7F7F;

			vb_[1].x=0+ScreenMirrorX-0.5f;
			vb_[1].y=0+ScreenMirrorY-0.5f;
			vb_[1].tu=0.0f;
			vb_[1].tv=0.0f;
			vb_[1].Diffuse=0x7F7F7F7F;
			vb_[3].x=0+ScreenMirrorX-0.5f;
			vb_[3].y=0+ScreenMirrorY-0.5f;
			vb_[3].tu=0.0f;
			vb_[3].tv=0.0f;
			vb_[3].Diffuse=0x7F7F7F7F;

			vb_[2].x=MODE_WIDTH+ScreenMirrorX-0.5f;
			vb_[2].y=MODE_HEIGHT+ScreenMirrorY-0.5f;
			vb_[2].tu=1.0f;
			vb_[2].tv=1.0f;
			vb_[2].Diffuse=0x7F7F7F7F;
			vb_[5].x=MODE_WIDTH+ScreenMirrorX-0.5f;
			vb_[5].y=MODE_HEIGHT+ScreenMirrorY-0.5f;
			vb_[5].tu=1.0f;
			vb_[5].tv=1.0f;
			vb_[5].Diffuse=0x7F7F7F7F;

			vb_[4].x=MODE_WIDTH+ScreenMirrorX-0.5f;
			vb_[4].y=0+ScreenMirrorY-0.5f;
			vb_[4].tu=1.0f;
			vb_[4].tv=0.0f;
			vb_[4].Diffuse=0x7F7F7F7F;

			SprMngr.GetDevice()->SetTexture(0,ScreenMirrorTexture);
			LPDIRECT3DVERTEXBUFFER vb;
			SprMngr.GetDevice()->CreateVertexBuffer(6*sizeof(MYVERTEX),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFVF_MYVERTEX,D3DPOOL_DEFAULT,&vb);
			void* vertices;
			vb->Lock(0,6*sizeof(MYVERTEX),(BYTE**)&vertices,D3DLOCK_DISCARD);
			memcpy(vertices,vb_,6*sizeof(MYVERTEX));
			vb->Unlock();
			SprMngr.GetDevice()->SetStreamSource(0,vb,sizeof(MYVERTEX));
			SprMngr.GetDevice()->SetVertexShader(D3DFVF_MYVERTEX);
			SprMngr.GetDevice()->DrawPrimitive(D3DPT_TRIANGLELIST,0,2);
			SAFEREL(vb);
			SprMngr.GetDevice()->SetStreamSource(0,SprMngr.GetVB(),sizeof(MYVERTEX));
			SprMngr.GetDevice()->SetVertexShader(D3DFVF_MYVERTEX);
		}
	}
*/
}

void FOClient::ParseKeyboard()
{
	DWORD elements=DI_BUF_SIZE;
	DIDEVICEOBJECTDATA didod[DI_BUF_SIZE]; //Receives buffered data 
	if(/*GetTopWindow(NULL)!=Wnd || */Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),didod,&elements,0)!=DI_OK)
	{
		Keyboard->Acquire();
		Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),didod,&elements,0); //Clear buffer
		Keyb::Lost();
		Timer::StartAccelerator(ACCELERATE_NONE);
		if(Script::PrepareContext(ClientFunctions.InputLost,CALL_FUNC_STR,"Game")) Script::RunPrepared();
		return;
	}

	if(!IsCurMode(CUR_WAIT))
	{
		if(Timer::ProcessAccelerator(ACCELERATE_PAGE_UP)) ProcessMouseWheel(1);
		if(Timer::ProcessAccelerator(ACCELERATE_PAGE_DOWN)) ProcessMouseWheel(-1);
	}

	for(int i=0;i<elements;i++) 
	{
		BYTE dikdw=(didod[i].dwData&0x80?didod[i].dwOfs:0);
		BYTE dikup=(!(didod[i].dwData&0x80)?didod[i].dwOfs:0);

		if(IsVideoPlayed())
		{
			if(IsCanStopVideo() && (dikdw==DIK_ESCAPE || dikdw==DIK_SPACE || dikdw==DIK_RETURN || dikdw==DIK_NUMPADENTER))
			{
				NextVideo();
				return;
			}
			continue;
		}

		bool script_result=false;
		if(dikdw && Script::PrepareContext(ClientFunctions.KeyDown,CALL_FUNC_STR,"Game"))
		{
			Script::SetArgByte(dikdw);
			if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
		}
		if(dikup && Script::PrepareContext(ClientFunctions.KeyUp,CALL_FUNC_STR,"Game"))
		{
			Script::SetArgByte(dikup);
			if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
		}

		Keyb::KeyPressed[dikup]=false;
		Keyb::KeyPressed[dikdw]=true;

		if(dikdw==DIK_RCONTROL || dikdw==DIK_LCONTROL)
		{
			Keyb::CtrlDwn=true;
			goto label_TryChangeLang;
		}
		else if(dikdw==DIK_LMENU || dikdw==DIK_RMENU)
		{
			Keyb::AltDwn=true;
			goto label_TryChangeLang;
		}
		else if(dikdw==DIK_LSHIFT || dikdw==DIK_RSHIFT)
		{
			Keyb::ShiftDwn=true;
label_TryChangeLang:
			if(Keyb::ShiftDwn && Keyb::CtrlDwn && GameOpt.ChangeLang==CHANGE_LANG_CTRL_SHIFT) //Ctrl+Shift
				Keyb::Lang=(Keyb::Lang==LANG_RUS)?LANG_ENG:LANG_RUS;
			if(Keyb::ShiftDwn && Keyb::AltDwn && GameOpt.ChangeLang==CHANGE_LANG_ALT_SHIFT) //Alt+Shift
				Keyb::Lang=(Keyb::Lang==LANG_RUS)?LANG_ENG:LANG_RUS;
		}
		if(dikup==DIK_RCONTROL || dikup==DIK_LCONTROL) Keyb::CtrlDwn=false;
		else if(dikup==DIK_LMENU || dikup==DIK_RMENU) Keyb::AltDwn=false;
		else if(dikup==DIK_LSHIFT || dikup==DIK_RSHIFT) Keyb::ShiftDwn=false;

		if(script_result || GameOpt.DisableKeyboardEvents)
		{
			if(dikdw==DIK_ESCAPE && Keyb::ShiftDwn) GameOpt.Quit=true;
			continue;
		}

		// Hotkeys
		if(!Keyb::ShiftDwn && !Keyb::AltDwn && !Keyb::CtrlDwn)
		{
			// F Buttons
			switch(dikdw)
			{
			case DIK_F1: GameOpt.HelpInfo=!GameOpt.HelpInfo; break;
			case DIK_F2:
				if(SaveLogFile()) AddMess(FOMB_GAME,MsgGame->GetStr(STR_LOG_SAVED));
				else AddMess(FOMB_GAME,MsgGame->GetStr(STR_LOG_NOT_SAVED));
				break;
			case DIK_F3:
				if(SaveScreenshot()) AddMess(FOMB_GAME,MsgGame->GetStr(STR_SCREENSHOT_SAVED));
				else AddMess(FOMB_GAME,MsgGame->GetStr(STR_SCREENSHOT_NOT_SAVED));
				break;
			case DIK_F4: IntVisible=!IntVisible; MessBoxGenerate(); break;
			case DIK_F5: IntAddMess=!IntAddMess; MessBoxGenerate(); break;
			case DIK_F6: GameOpt.ShowPlayerNames=!GameOpt.ShowPlayerNames; break;

			case DIK_F7: if(GameOpt.DebugInfo) GameOpt.ShowNpcNames=!GameOpt.ShowNpcNames; break;

			case DIK_F8: GameOpt.MouseScroll=!GameOpt.MouseScroll; GameOpt.ScrollMouseRight=false; GameOpt.ScrollMouseLeft=false; GameOpt.ScrollMouseDown=false; GameOpt.ScrollMouseUp=false; break;

			case DIK_F9: if(GameOpt.DebugInfo) HexMngr.SwitchShowTrack(); break;
			case DIK_F10: if(GameOpt.DebugInfo) HexMngr.SwitchShowHex(); break;

				// Volume buttons
			case DIK_VOLUMEUP:
				SndMngr.SetSoundVolume(SndMngr.GetSoundVolume()+2);
				SndMngr.SetMusicVolume(SndMngr.GetMusicVolume()+2);
				break;
			case DIK_VOLUMEDOWN:
				SndMngr.SetSoundVolume(SndMngr.GetSoundVolume()-2);
				SndMngr.SetMusicVolume(SndMngr.GetMusicVolume()-2);
				break;
				// Exit buttons
			case DIK_TAB: if(GetActiveScreen()==SCREEN__MINI_MAP) {TryExit(); continue;} break;
				// Mouse wheel emulate
			case DIK_PRIOR: ProcessMouseWheel(1); Timer::StartAccelerator(ACCELERATE_PAGE_UP); break;
			case DIK_NEXT: ProcessMouseWheel(-1); Timer::StartAccelerator(ACCELERATE_PAGE_DOWN); break;

			case DIK_F11: if(GameOpt.DebugInfo) SprMngr.SaveSufaces(); break;
			//case DIK_F11: HexMngr.SwitchShowRain(); break;

			case DIK_ESCAPE: TryExit(); break;
			default:
				break;
			}

			if(!ConsoleEdit)
			{
				switch(dikdw)
				{
				case DIK_C: if(GetActiveScreen()==SCREEN__CHARACTER) {TryExit(); continue;} break;
				case DIK_P:
					if(GetActiveScreen()==SCREEN__PIP_BOY)
					{
						if(PipMode==PIP__NONE) PipMode=PIP__STATUS;
						else TryExit();
						continue;
					}
				case DIK_F: if(GetActiveScreen()==SCREEN__FIX_BOY) {TryExit(); continue;} break;
				case DIK_I: if(GetActiveScreen()==SCREEN__INVENTORY) {TryExit(); continue;} break;
				case DIK_Q: if(IsMainScreen(SCREEN_GAME) && GetActiveScreen()==SCREEN_NONE) {DrawLookBorders=!DrawLookBorders; RebuildLookBorders=true;} break;
				case DIK_W: if(IsMainScreen(SCREEN_GAME) && GetActiveScreen()==SCREEN_NONE) {DrawShootBorders=!DrawShootBorders; RebuildLookBorders=true;} break;
				case DIK_SPACE: if(Singleplayer) SingleplayerData.Pause=!SingleplayerData.Pause; break;
				default: break;
				}
			}
		}
		else
		{
			switch(dikdw)
			{
			case DIK_F6: if(GameOpt.DebugInfo && Keyb::CtrlDwn) GameOpt.ShowCritId=!GameOpt.ShowCritId; break;
			case DIK_F7: if(GameOpt.DebugInfo && Keyb::CtrlDwn) GameOpt.ShowCritId=!GameOpt.ShowCritId; break;

				// Num Pad
			case DIK_EQUALS:
			case DIK_ADD:
				if(ConsoleEdit) break;
				if(Keyb::CtrlDwn) SndMngr.SetSoundVolume(SndMngr.GetSoundVolume()+2);
				else if(Keyb::ShiftDwn) SndMngr.SetMusicVolume(SndMngr.GetMusicVolume()+2);
				else if(Keyb::AltDwn && GameOpt.Sleep<100) GameOpt.Sleep++;
				break;
			case DIK_MINUS:
			case DIK_SUBTRACT:
				if(ConsoleEdit) break;
				if(Keyb::CtrlDwn) SndMngr.SetSoundVolume(SndMngr.GetSoundVolume()-2);
				else if(Keyb::ShiftDwn) SndMngr.SetMusicVolume(SndMngr.GetMusicVolume()-2);
				else if(Keyb::AltDwn && GameOpt.Sleep>-1) GameOpt.Sleep--;
				break;
				// Escape
			case DIK_ESCAPE:
				if(Keyb::ShiftDwn) GameOpt.Quit=true;
				break;
			default: break;
			}
		}

		// Wait mode
		if(IsCurMode(CUR_WAIT))
		{
			ConsoleKeyDown(dikdw);
			continue;
		}

		// Cursor steps
		if(HexMngr.IsMapLoaded() && (dikdw==DIK_RCONTROL || dikdw==DIK_LCONTROL || dikup==DIK_RCONTROL || dikup==DIK_LCONTROL)) HexMngr.SetCursorPos(CurX,CurY,Keyb::CtrlDwn,true);

		// Other by screens
		// Key down
		if(dikdw)
		{
			if(GetActiveScreen())
			{
				switch(GetActiveScreen())
				{
				case SCREEN__SPLIT: SplitKeyDown(dikdw); break;
				case SCREEN__TIMER: TimerKeyDown(dikdw); break;
				case SCREEN__CHA_NAME: ChaNameKeyDown(dikdw); break;
				case SCREEN__SAY: SayKeyDown(dikdw); break;
				case SCREEN__INPUT_BOX: IboxKeyDown(dikdw); break;
				case SCREEN__INVENTORY: InvKeyDown(dikdw); ConsoleKeyDown(dikdw); break;
				case SCREEN__DIALOG: DlgKeyDown(true,dikdw); ConsoleKeyDown(dikdw); break;
				case SCREEN__BARTER: DlgKeyDown(false,dikdw); ConsoleKeyDown(dikdw); break;
				default: ConsoleKeyDown(dikdw); break;
				}
			}
			else
			{
				switch(GetMainScreen())
				{
				case SCREEN_LOGIN: LogKeyDown(dikdw); break;
				case SCREEN_CREDITS: TryExit(); break;
				case SCREEN_GAME: GameKeyDown(dikdw); break;
				case SCREEN_GLOBAL_MAP: GmapKeyDown(dikdw); break;
				default: break;
				}
				ConsoleKeyDown(dikdw);
			}

			ProcessKeybScroll(true,dikdw);
		}

		// Key up
		if(dikup)
		{
			if(GetActiveScreen()==SCREEN__INPUT_BOX) IboxKeyUp(dikup);
			ConsoleKeyUp(dikup);
			ProcessKeybScroll(false,dikup);
			Timer::StartAccelerator(ACCELERATE_NONE);
		}
	}
}

#define MOUSE_CLICK_LEFT        (0)
#define MOUSE_CLICK_RIGHT       (1)
#define MOUSE_CLICK_MIDDLE      (2)
#define MOUSE_CLICK_WHEEL_UP    (3)
#define MOUSE_CLICK_WHEEL_DOWN  (4)
#define MOUSE_CLICK_EXT0        (5)
#define MOUSE_CLICK_EXT1        (6)
#define MOUSE_CLICK_EXT2        (7)
#define MOUSE_CLICK_EXT3        (8)
#define MOUSE_CLICK_EXT4        (9)
void FOClient::ParseMouse()
{
	// Windows mouse move in windowed mode
	if(!GameOpt.FullScreen)
	{
		WINDOWINFO wi;
		wi.cbSize=sizeof(wi);
		GetWindowInfo(Wnd,&wi);
		POINT p;
		GetCursorPos(&p);
		CurX=p.x-wi.rcClient.left;
		CurY=p.y-wi.rcClient.top;

		if(CurX>=MODE_WIDTH) CurX=MODE_WIDTH-1;
		if(CurX<0) CurX=0;
		if(CurY>=MODE_HEIGHT) CurY=MODE_HEIGHT-1;
		if(CurY<0) CurY=0;
	}

	// DirectInput mouse
	DWORD elements=DI_BUF_SIZE;
	DIDEVICEOBJECTDATA didod[DI_BUF_SIZE]; //Receives buffered data 
	if(Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),didod,&elements,0)!=DI_OK)
	{
		Mouse->Acquire();
		Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),didod,&elements,0); //Clear buffer
		IfaceHold=IFACE_NONE;
		Timer::StartAccelerator(ACCELERATE_NONE);
		if(Script::PrepareContext(ClientFunctions.InputLost,CALL_FUNC_STR,"Game")) Script::RunPrepared();
		return;
	}

	if(Timer::GetAcceleratorNum()!=ACCELERATE_NONE && !IsCurMode(CUR_WAIT))
	{
		int iface_hold=IfaceHold;
		if(Timer::ProcessAccelerator(ACCELERATE_MESSBOX)) MessBoxLMouseDown();
		else if(Timer::ProcessAccelerator(ACCELERATE_SPLIT_UP)) SplitLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_SPLIT_DOWN)) SplitLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_TIMER_UP)) TimerLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_TIMER_DOWN)) TimerLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_USE_SCRUP)) UseLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_USE_SCRDOWN)) UseLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_INV_SCRUP)) InvLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_INV_SCRDOWN)) InvLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_PUP_SCRUP1)) PupLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_PUP_SCRDOWN1)) PupLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_PUP_SCRUP2)) PupLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_PUP_SCRDOWN2)) PupLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_CHA_SW_SCRUP)) ChaLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_CHA_SW_SCRDOWN)) ChaLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_CHA_PLUS)) ChaLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_CHA_MINUS)) ChaLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_CHA_AGE_UP)) ChaAgeLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_CHA_AGE_DOWN)) ChaAgeLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT1SU)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT1SD)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT2SU)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT2SD)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT1OSU)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT1OSD)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT2OSU)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_BARTER_CONT2OSD)) DlgLMouseUp(false);
		else if(Timer::ProcessAccelerator(ACCELERATE_PERK_SCRUP)) PerkLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_PERK_SCRDOWN)) PerkLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_DLG_TEXT_UP)) DlgLMouseUp(true);
		else if(Timer::ProcessAccelerator(ACCELERATE_DLG_TEXT_DOWN)) DlgLMouseUp(true);
		else if(Timer::ProcessAccelerator(ACCELERATE_SAVE_LOAD_SCR_UP)) SaveLoadLMouseUp();
		else if(Timer::ProcessAccelerator(ACCELERATE_SAVE_LOAD_SCR_DN)) SaveLoadLMouseUp();
		IfaceHold=iface_hold;
	}

	// Windows mouse move in windowed mode
	if(!GameOpt.FullScreen)
	{
		static int old_cur_x=CurX;
		static int old_cur_y=CurY;

		if(old_cur_x!=CurX || old_cur_y!=CurY)
		{
			old_cur_x=CurX;
			old_cur_y=CurY;

			if(GetActiveScreen())
			{
				switch(GetActiveScreen())
				{
				case SCREEN__SPLIT: SplitMouseMove(); break;
				case SCREEN__TIMER: TimerMouseMove(); break;
				case SCREEN__DIALOGBOX: DlgboxMouseMove(); break;
				case SCREEN__ELEVATOR: ElevatorMouseMove(); break;
				case SCREEN__SAY: SayMouseMove(); break;
				case SCREEN__INPUT_BOX: IboxMouseMove(); break;
				case SCREEN__SKILLBOX: SboxMouseMove(); break;
				case SCREEN__USE: UseMouseMove(); break;
				case SCREEN__PERK: PerkMouseMove(); break;
				case SCREEN__TOWN_VIEW: TViewMouseMove(); break;
				case SCREEN__DIALOG: DlgMouseMove(true); break;
				case SCREEN__BARTER: DlgMouseMove(false); break;
				case SCREEN__INVENTORY: InvMouseMove(); break;
				case SCREEN__PICKUP: PupMouseMove(); break;
				case SCREEN__MINI_MAP: LmapMouseMove(); break;
				case SCREEN__CHARACTER: ChaMouseMove(false); break;
				case SCREEN__PIP_BOY: PipMouseMove(); break;
				case SCREEN__FIX_BOY: FixMouseMove(); break;
				case SCREEN__AIM: AimMouseMove(); break;
				case SCREEN__SAVE_LOAD: SaveLoadMouseMove(); break;
				default: break;
				}
			}
			else
			{
				switch(GetMainScreen())
				{
				case SCREEN_GLOBAL_MAP: GmapMouseMove(); break;
				default: break;
				}
			}

			ProcessMouseScroll();
			LMenuMouseMove();

			if(Script::PrepareContext(ClientFunctions.MouseMove,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(CurX);
				Script::SetArgDword(CurY);
				Script::RunPrepared();
			}
		}	
	}

	bool ismoved=false;
	for(int i=0;i<elements;i++) 
	{
		// Mouse Move
		// Direct input move cursor in full screen mode
		if(GameOpt.FullScreen)
		{
			DI_ONMOUSE( DIMOFS_X, CurX+=didod[i].dwData*GameOpt.MouseSpeed; ismoved=true; continue;);
			DI_ONMOUSE( DIMOFS_Y, CurY+=didod[i].dwData*GameOpt.MouseSpeed; ismoved=true; continue;);
		}

		if(IsVideoPlayed())
		{
			if(IsCanStopVideo() &&
				(((didod[i].dwOfs==DIMOFS_BUTTON0) && (didod[i].dwData&0x80)) ||
				((didod[i].dwOfs==DIMOFS_BUTTON1) && (didod[i].dwData&0x80))))
			{
				NextVideo();
				return;
			}
			continue;
		}

		// Scripts
		bool script_result=false;
		DI_ONMOUSE( DIMOFS_Z,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(int(didod[i].dwData)>0?MOUSE_CLICK_WHEEL_UP:MOUSE_CLICK_WHEEL_DOWN);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON0,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_LEFT);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON0,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_LEFT);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON1,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_RIGHT);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON1,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_RIGHT);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON2,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_MIDDLE);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON2,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_MIDDLE);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
				if(!script_result && !GameOpt.DisableMouseEvents && !ConsoleEdit && Keyb::KeyPressed[DIK_Z] && GameOpt.SpritesZoomMin!=GameOpt.SpritesZoomMax)
				{
					int screen=GetActiveScreen();
					if(IsMainScreen(SCREEN_GAME) && (screen==SCREEN_NONE || screen==SCREEN__TOWN_VIEW))
					{
						HexMngr.ChangeZoom(0);
						RebuildLookBorders=true;
					}
				}
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON3,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT0);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON3,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT0);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON4,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT1);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON4,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT1);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON5,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT2);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON5,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT2);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON6,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT3);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON6,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT3);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONDOWN( DIMOFS_BUTTON7,
			if(Script::PrepareContext(ClientFunctions.MouseDown,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT4);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);
		DI_ONUP( DIMOFS_BUTTON7,
			if(Script::PrepareContext(ClientFunctions.MouseUp,CALL_FUNC_STR,"Game"))
			{
				Script::SetArgDword(MOUSE_CLICK_EXT4);
				if(Script::RunPrepared()) script_result=Script::GetReturnedBool();
			}
		);

		if(script_result || GameOpt.DisableMouseEvents) continue;
		if(IsCurMode(CUR_WAIT)) continue;

		// Wheel
		DI_ONMOUSE( DIMOFS_Z,
			ProcessMouseWheel((int)didod[i].dwData);
			continue;
		);

		// Left Button Down
		DI_ONDOWN( DIMOFS_BUTTON0,
			if(GetActiveScreen())
			{
				switch(GetActiveScreen())
				{
				case SCREEN__SPLIT: SplitLMouseDown(); break;
				case SCREEN__TIMER: TimerLMouseDown(); break;
				case SCREEN__DIALOGBOX: DlgboxLMouseDown(); break;
				case SCREEN__ELEVATOR: ElevatorLMouseDown(); break;
				case SCREEN__SAY: SayLMouseDown(); break;
				case SCREEN__CHA_NAME: ChaNameLMouseDown(); break;
				case SCREEN__CHA_AGE: ChaAgeLMouseDown(); break;
				case SCREEN__CHA_SEX: ChaSexLMouseDown(); break;
				case SCREEN__GM_TOWN: GmapLMouseDown(); break;
				case SCREEN__INPUT_BOX: IboxLMouseDown(); break;
				case SCREEN__SKILLBOX: SboxLMouseDown(); break;
				case SCREEN__USE: UseLMouseDown(); break;
				case SCREEN__PERK: PerkLMouseDown(); break;
				case SCREEN__TOWN_VIEW: TViewLMouseDown(); break;
				case SCREEN__INVENTORY: InvLMouseDown(); break;
				case SCREEN__PICKUP: PupLMouseDown(); break;
				case SCREEN__DIALOG: DlgLMouseDown(true); break;
				case SCREEN__BARTER: DlgLMouseDown(false); break;
				case SCREEN__MINI_MAP: LmapLMouseDown(); break;
				case SCREEN__MENU_OPTION: MoptLMouseDown(); break;
				case SCREEN__CHARACTER: ChaLMouseDown(false); break;
				case SCREEN__PIP_BOY: PipLMouseDown(); break;
				case SCREEN__FIX_BOY: FixLMouseDown(); break;	
				case SCREEN__AIM: AimLMouseDown(); break;
				case SCREEN__SAVE_LOAD: SaveLoadLMouseDown(); break;
				default: break;
				}
			}
			else
			{
				switch(GetMainScreen())
				{
				case SCREEN_GAME: if(IntLMouseDown()==IFACE_NONE) GameLMouseDown(); break;
				case SCREEN_GLOBAL_MAP: GmapLMouseDown(); break;
				case SCREEN_LOGIN: LogLMouseDown(); break;
				case SCREEN_REGISTRATION: ChaLMouseDown(true); break;
				case SCREEN_CREDITS: TryExit(); break;
				default: break;
				}
			}

			if(MessBoxLMouseDown()) Timer::StartAccelerator(ACCELERATE_MESSBOX);
			continue;	
		);

		// Left Button Up
		DI_ONUP( DIMOFS_BUTTON0,
			if(GetActiveScreen())
			{
				switch(GetActiveScreen())
				{
				case SCREEN__SPLIT: SplitLMouseUp(); break;
				case SCREEN__TIMER: TimerLMouseUp(); break;
				case SCREEN__DIALOGBOX: DlgboxLMouseUp(); break;
				case SCREEN__ELEVATOR: ElevatorLMouseUp(); break;
				case SCREEN__SAY: SayLMouseUp(); break;
				case SCREEN__CHA_AGE: ChaAgeLMouseUp(); break;
				case SCREEN__CHA_SEX: ChaSexLMouseUp(); break;
				case SCREEN__GM_TOWN: GmapLMouseUp(); break;
				case SCREEN__INPUT_BOX: IboxLMouseUp(); break;
				case SCREEN__SKILLBOX: SboxLMouseUp(); break;
				case SCREEN__USE: UseLMouseUp(); break;
				case SCREEN__PERK: PerkLMouseUp(); break;
				case SCREEN__TOWN_VIEW: TViewLMouseUp(); break;
				case SCREEN__INVENTORY: InvLMouseUp(); break;
				case SCREEN__PICKUP: PupLMouseUp(); break;
				case SCREEN__DIALOG: DlgLMouseUp(true); break;
				case SCREEN__BARTER: DlgLMouseUp(false); break;
				case SCREEN__MINI_MAP: LmapLMouseUp(); break;
				case SCREEN__MENU_OPTION:MoptLMouseUp(); break;
				case SCREEN__CHARACTER: ChaLMouseUp(false); break;
				case SCREEN__PIP_BOY: PipLMouseUp(); break;
				case SCREEN__FIX_BOY: FixLMouseUp(); break;
				case SCREEN__AIM: AimLMouseUp(); break;
				case SCREEN__SAVE_LOAD: SaveLoadLMouseUp(); break;
				default: break;
				}
			}
			else
			{
				switch(GetMainScreen())
				{
				case SCREEN_GAME: (IfaceHold==IFACE_NONE)?GameLMouseUp():IntLMouseUp(); break;
				case SCREEN_GLOBAL_MAP: GmapLMouseUp(); break;
				case SCREEN_LOGIN: LogLMouseUp(); break;
				case SCREEN_REGISTRATION: ChaLMouseUp(true); break;
				default: break;
				}
			}

			LMenuMouseUp();
			Timer::StartAccelerator(ACCELERATE_NONE);
			continue;
		);

		// Right Button Down
		DI_ONDOWN( DIMOFS_BUTTON1,
			if(GetActiveScreen())
			{
				switch(GetActiveScreen())
				{
				case SCREEN__USE: UseRMouseDown(); break;
				case SCREEN__INVENTORY: InvRMouseDown(); break;
				case SCREEN__DIALOG: DlgRMouseDown(true); break;
				case SCREEN__BARTER: DlgRMouseDown(false); break;
				case SCREEN__PICKUP: PupRMouseDown(); break;
				case SCREEN__PIP_BOY: PipRMouseDown(); break;
				default: break;
				}
			}
			else
			{
				switch(GetMainScreen())
				{
				case SCREEN_GAME: IntRMouseDown(); GameRMouseDown(); break;
				case SCREEN_GLOBAL_MAP: GmapRMouseDown(); break;
				default: break;
				}		
			}
			continue;
		);

		// Right Button Up
		DI_ONUP( DIMOFS_BUTTON1,
			if(!GetActiveScreen())
			{
				switch(GetMainScreen())
				{
				case SCREEN_GAME: GameRMouseUp(); break;
				case SCREEN_GLOBAL_MAP: GmapRMouseUp(); break;
				default: break;
				}
			}
			continue;
		);
	}

	// Direct input move cursor in full screen mode
	if(ismoved)
	{
		if(CurX>=MODE_WIDTH) CurX=MODE_WIDTH-1;
		if(CurX<0) CurX=0;
		if(CurY>=MODE_HEIGHT) CurY=MODE_HEIGHT-1;
		if(CurY<0) CurY=0;

		if(GetActiveScreen())
		{
			switch(GetActiveScreen())
			{
			case SCREEN__SPLIT: SplitMouseMove(); break;
			case SCREEN__TIMER: TimerMouseMove(); break;
			case SCREEN__DIALOGBOX: DlgboxMouseMove(); break;
			case SCREEN__ELEVATOR: ElevatorMouseMove(); break;
			case SCREEN__SAY: SayMouseMove(); break;
			case SCREEN__INPUT_BOX: IboxMouseMove(); break;
			case SCREEN__SKILLBOX: SboxMouseMove(); break;
			case SCREEN__USE: UseMouseMove(); break;
			case SCREEN__PERK: PerkMouseMove(); break;
			case SCREEN__TOWN_VIEW: TViewMouseMove(); break;
			case SCREEN__DIALOG: DlgMouseMove(true); break;
			case SCREEN__BARTER: DlgMouseMove(false); break;
			case SCREEN__INVENTORY: InvMouseMove(); break;
			case SCREEN__PICKUP: PupMouseMove(); break;
			case SCREEN__MINI_MAP: LmapMouseMove(); break;
			case SCREEN__CHARACTER: ChaMouseMove(false); break;
			case SCREEN__PIP_BOY: PipMouseMove(); break;
			case SCREEN__FIX_BOY: FixMouseMove(); break;
			case SCREEN__AIM: AimMouseMove(); break;
			case SCREEN__SAVE_LOAD: SaveLoadMouseMove(); break;
			default: break;
			}
		}
		else
		{
			switch(GetMainScreen())
			{
			case SCREEN_GLOBAL_MAP: GmapMouseMove(); break;
			default: break;
			}
		}

		ProcessMouseScroll();
		LMenuMouseMove();

		if(Script::PrepareContext(ClientFunctions.MouseMove,CALL_FUNC_STR,"Game"))
		{
			Script::SetArgDword(CurX);
			Script::SetArgDword(CurY);
			Script::RunPrepared();
		}
	}
}

void ContainerWheelScroll(int items_count, int cont_height, int item_height, int& cont_scroll, int wheel_data)
{
	int height_items=cont_height/item_height;
	int scroll=1;
	if(Keyb::ShiftDwn) scroll=height_items;
	if(wheel_data>0) scroll=-scroll;
	cont_scroll+=scroll;
	if(cont_scroll<0) cont_scroll=0;
	else if(cont_scroll>items_count-height_items) cont_scroll=max(0,items_count-height_items);
}

void FOClient::ProcessMouseWheel(int data)
{
	int screen=GetActiveScreen();

/************************************************************************/
/* Split value                                                          */
/************************************************************************/
	if(screen==SCREEN__SPLIT)
	{
		if(IsCurInRect(SplitWValue,SplitX,SplitY) || IsCurInRect(SplitWItem,SplitX,SplitY))
		{
			if(data>0 && SplitValue<SplitMaxValue)  SplitValue++;
			else if(data<0 && SplitValue>SplitMinValue)  SplitValue--;
		}
	}
/************************************************************************/
/* Timer value                                                          */
/************************************************************************/
	else if(screen==SCREEN__TIMER)
	{
		if(IsCurInRect(TimerWValue,TimerX,TimerY) || IsCurInRect(TimerWItem,TimerX,TimerY))
		{
			if(data>0 && TimerValue<TIMER_MAX_VALUE) TimerValue++;
			else if(data<0 && TimerValue>TIMER_MIN_VALUE) TimerValue--;
		}
	}
/************************************************************************/
/* Use scroll                                                           */
/************************************************************************/
	else if(screen==SCREEN__USE)
	{
		if(IsCurInRect(UseWInv,UseX,UseY)) ContainerWheelScroll(UseCont.size(),UseWInv.H(),UseHeightItem,UseScroll,data);
	}
/************************************************************************/
/* PerkUp scroll                                                        */
/************************************************************************/
	else if(screen==SCREEN__PERK)
	{
		if(data>0 && IsCurInRect(PerkWPerks,PerkX,PerkY) && PerkScroll>0) PerkScroll--;
		else if(data<0 && IsCurInRect(PerkWPerks,PerkX,PerkY) && PerkScroll<(int)PerkCollection.size()-1) PerkScroll++;
	}
/************************************************************************/
/* Local, global map zoom, global tabs scroll, mess box scroll          */
/************************************************************************/
	else if(screen==SCREEN_NONE || screen==SCREEN__TOWN_VIEW)
	{
		INTRECT r=MessBoxCurRectDraw();
		if(!r.IsZero() && IsCurInRect(r))
		{
			if(data>0)
			{
				if(GameOpt.MsgboxInvert && MessBoxScroll>0) MessBoxScroll--;
				if(!GameOpt.MsgboxInvert && MessBoxScroll<MessBoxMaxScroll) MessBoxScroll++;
				MessBoxGenerate();
			}
			else
			{
				if(GameOpt.MsgboxInvert && MessBoxScroll<MessBoxMaxScroll) MessBoxScroll++;
				if(!GameOpt.MsgboxInvert && MessBoxScroll>0) MessBoxScroll--;
				MessBoxGenerate();
			}
		}
		else if(IsMainScreen(SCREEN_GAME))
		{
			if(IntVisible && Chosen && IsCurInRect(IntBItem))
			{
				bool send=false;
				if(data>0) send=Chosen->NextRateItem(true);
				else send=Chosen->NextRateItem(false);
				if(send) Net_SendRateItem();
			}

			if(!ConsoleEdit && Keyb::KeyPressed[DIK_Z] && IsMainScreen(SCREEN_GAME) && GameOpt.SpritesZoomMin!=GameOpt.SpritesZoomMax)
			{
				HexMngr.ChangeZoom(data>0?-1:1);
				RebuildLookBorders=true;
			}
		}
		else if(IsMainScreen(SCREEN_GLOBAL_MAP))
		{
			GmapChangeZoom(-data/2000.0f);
			if(IsCurInRect(GmapWTabs))
			{
				if(data>0)
				{
					if(GmapTabNextX) GmapTabsScrX-=26;
					if(GmapTabNextY) GmapTabsScrY-=26;
					if(GmapTabsScrX<0) GmapTabsScrX=0;
					if(GmapTabsScrY<0) GmapTabsScrY=0;
				}
				else
				{
					if(GmapTabNextX) GmapTabsScrX+=26;
					if(GmapTabNextY) GmapTabsScrY+=26;

					int tabs_count=0;
					for(int i=0,j=GmapLoc.size();i<j;i++)
					{
						GmapLocation& loc=GmapLoc[i];
						if(MsgGM->Count(STR_GM_LABELPIC_(loc.LocPid)) && ResMngr.GetIfaceSprId(Str::GetHash(MsgGM->GetStr(STR_GM_LABELPIC_(loc.LocPid))))) tabs_count++;
					}
					if(GmapTabNextX && GmapTabsScrX>GmapWTab.W()*tabs_count) GmapTabsScrX=GmapWTab.W()*tabs_count;
					if(GmapTabNextY && GmapTabsScrY>GmapWTab.H()*tabs_count) GmapTabsScrY=GmapWTab.H()*tabs_count;
				}
			}
		}
	}
/************************************************************************/
/* Inventory scroll                                                     */
/************************************************************************/
	else if(screen==SCREEN__INVENTORY)
	{
		if(IsCurInRect(InvWInv,InvX,InvY)) ContainerWheelScroll(InvCont.size(),InvWInv.H(),InvHeightItem,InvScroll,data);
		else if(IsCurInRect(InvWText,InvX,InvY))
		{
			if(data>0)
			{
				if(InvItemInfoScroll>0) InvItemInfoScroll--;
			}
			else
			{
				if(InvItemInfoScroll<InvItemInfoMaxScroll) InvItemInfoScroll++;
			}
		}
	}
/************************************************************************/
/* Dialog texts, answers                                                */
/************************************************************************/
	else if(screen==SCREEN__DIALOG)
	{
		if(IsCurInRect(DlgWText,DlgX,DlgY))
		{
			if(data>0 && DlgMainTextCur>0) DlgMainTextCur--;
			else if(data<0 && DlgMainTextCur<DlgMainTextLinesReal-DlgMainTextLinesRect) DlgMainTextCur++;
		}
		else if(IsCurInRect(DlgAnswText,DlgX,DlgY))
		{
			DlgCollectAnswers(data<0);
		}
	}
/************************************************************************/
/* Barter scroll                                                        */
/************************************************************************/
	else if(screen==SCREEN__BARTER)
	{
		if(IsCurInRect(BarterWCont1,DlgX,DlgY)) ContainerWheelScroll(BarterCont1.size(),BarterWCont1.H(),BarterCont1HeightItem,BarterScroll1,data);
		else if(IsCurInRect(BarterWCont2,DlgX,DlgY)) ContainerWheelScroll(BarterCont2.size(),BarterWCont2.H(),BarterCont2HeightItem,BarterScroll2,data);
		else if(IsCurInRect(BarterWCont1o,DlgX,DlgY)) ContainerWheelScroll(BarterCont1o.size(),BarterWCont1o.H(),BarterCont1oHeightItem,BarterScroll1o,data);
		else if(IsCurInRect(BarterWCont2o,DlgX,DlgY)) ContainerWheelScroll(BarterCont2o.size(),BarterWCont2o.H(),BarterCont2oHeightItem,BarterScroll2o,data);
		else if(IsCurInRect(DlgWText,DlgX,DlgY))
		{
			if(data>0 && DlgMainTextCur>0) DlgMainTextCur--;
			else if(data<0 && DlgMainTextCur<DlgMainTextLinesReal-DlgMainTextLinesRect) DlgMainTextCur++;
		}
	}
/************************************************************************/
/* PickUp scroll                                                        */
/************************************************************************/
	else if(screen==SCREEN__PICKUP)
	{
		if(IsCurInRect(PupWCont1,PupX,PupY)) ContainerWheelScroll(PupCont1.size(),PupWCont1.H(),PupHeightItem1,PupScroll1,data);
		else if(IsCurInRect(PupWCont2,PupX,PupY)) ContainerWheelScroll(PupCont2.size(),PupWCont2.H(),PupHeightItem2,PupScroll2,data);
	}
/************************************************************************/
/* Character switch scroll                                              */
/************************************************************************/
	else if(screen==SCREEN__CHARACTER)
	{
		if(data>0)
		{
			if(IsCurInRect(ChaTSwitch,ChaX,ChaY) && ChaSwitchScroll[ChaCurSwitch]>0) ChaSwitchScroll[ChaCurSwitch]--;
		}
		else
		{
			if(IsCurInRect(ChaTSwitch,ChaX,ChaY))
			{
				int max_lines=ChaTSwitch.H()/11;
				SwitchElementVec& text=ChaSwitchText[ChaCurSwitch];
				int& scroll=ChaSwitchScroll[ChaCurSwitch];
				if(scroll+max_lines<text.size()) scroll++;
			}
		}

	}
/************************************************************************/
/* Minimap zoom                                                         */
/************************************************************************/
	else if(screen==SCREEN__MINI_MAP)
	{
		if(IsCurInRect(LmapWMap,LmapX,LmapY))
		{
			if(data>0) LmapZoom++;
			else LmapZoom--;
			LmapZoom=CLAMP(LmapZoom,2,13);
			LmapPrepareMap();
		}
	}
/************************************************************************/
/* PipBoy scroll                                                        */
/************************************************************************/
	else if(screen==SCREEN__PIP_BOY)
	{
		if(IsCurInRect(PipWMonitor,PipX,PipY))
		{
			if(PipMode!=PIP__AUTOMAPS_MAP)
			{
				int scroll=1;
				if(Keyb::ShiftDwn) scroll=SprMngr.GetLinesCount(0,PipWMonitor.H(),NULL,FONT_DEF);
				if(data>0) scroll=-scroll;
				PipScroll[PipMode]+=scroll;
				if(PipScroll[PipMode]<0) PipScroll[PipMode]=0;
#pragma MESSAGE("Check maximums in PipBoy scrolling.")
			}
			else
			{
				float scr_x=((float)(PipWMonitor.CX()+PipX)-AutomapScrX)*AutomapZoom;
				float scr_y=((float)(PipWMonitor.CY()+PipY)-AutomapScrY)*AutomapZoom;
				if(data>0) AutomapZoom-=0.1f;
				else AutomapZoom+=0.1f;
				AutomapZoom=CLAMP(AutomapZoom,0.1f,10.0f);
				AutomapScrX=(float)(PipWMonitor.CX()+PipX)-scr_x/AutomapZoom;
				AutomapScrY=(float)(PipWMonitor.CY()+PipY)-scr_y/AutomapZoom;
			}
		}
	}
/************************************************************************/
/* Save/Load                                                            */
/************************************************************************/
	else if(screen==SCREEN__SAVE_LOAD)
	{
		int ox=(SaveLoadLoginScreen?SaveLoadCX:SaveLoadX);
		int oy=(SaveLoadLoginScreen?SaveLoadCY:SaveLoadY);

		if(IsCurInRect(SaveLoadSlots,ox,oy))
		{
			if(data>0)
			{
				if(SaveLoadSlotScroll>0) SaveLoadSlotScroll--;
			}
			else
			{
				int max=(int)SaveLoadDataSlots.size()-SaveLoadSlotsMax+(SaveLoadSave?1:0);
				if(SaveLoadSlotScroll<max) SaveLoadSlotScroll++;
			}
		}
	}
/************************************************************************/
/*                                                                      */
/************************************************************************/
}

bool FOClient::InitNet()
{
	WriteLog("Network init...\n");
	NetState=STATE_DISCONNECT;

	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2),&wsa))
	{
		WriteLog("WSAStartup error<%s>.\n",GetLastSocketError());
		return false;
	}

	if(!Singleplayer)
	{
		if(!FillSockAddr(SockAddr,GameOpt.Host.c_str(),GameOpt.Port)) return false;
		if(GameOpt.ProxyType && !FillSockAddr(ProxyAddr,GameOpt.ProxyHost.c_str(),GameOpt.ProxyPort)) return false;
	}
	else
	{
		for(int i=0;i<60;i++) // Wait 1 minute, than abort
		{
			if(!SingleplayerData.Refresh()) return false;
			if(SingleplayerData.NetPort) break;
			Sleep(1000);
		}
		if(!SingleplayerData.NetPort) return false;
		SockAddr.sin_family=AF_INET;
		SockAddr.sin_port=SingleplayerData.NetPort;
		SockAddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	}

	if(!NetConnect()) return false;
	NetState=STATE_CONN;
	WriteLog("Network init successful.\n");
	return true;
}

bool FOClient::FillSockAddr(SOCKADDR_IN& saddr, const char* host, WORD port)
{
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(port);
	if((saddr.sin_addr.s_addr=inet_addr(host))==-1)
	{
		hostent* h=gethostbyname(host);
		if(!h)
		{
			WriteLog(__FUNCTION__" - Can't resolve remote host<%s>, error<%s>.",host,GetLastSocketError());
			return false;
		}

		memcpy(&saddr.sin_addr,h->h_addr,sizeof(in_addr));
	}
	return true;
}

bool FOClient::NetConnect()
{
	if(!Singleplayer) WriteLog("Connecting to server<%s:%d>.\n",GameOpt.Host.c_str(),GameOpt.Port);
	else WriteLog("Connecting to server.\n");

	if((Sock=WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,0))==INVALID_SOCKET)
	{
		WriteLog("Create socket error<%s>.\n",GetLastSocketError());
		return false;
	}

	// Nagle
	if(GameOpt.DisableTcpNagle)
	{
		int optval=1;
		if(setsockopt(Sock,IPPROTO_TCP,TCP_NODELAY,(char*)&optval,sizeof(optval)))
			WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error<%s>.\n",WSAGetLastError());
	}

	// Direct connect
	if(!GameOpt.ProxyType || Singleplayer)
	{
		if(connect(Sock,(sockaddr*)&SockAddr,sizeof(SOCKADDR_IN)))
		{
			WriteLog("Can't connect to game server, error<%s>.\n",GetLastSocketError());
			return false;
		}
	}
	// Proxy connect
	else
	{
		if(connect(Sock,(sockaddr*)&ProxyAddr,sizeof(SOCKADDR_IN)))
		{
			WriteLog("Can't connect to Proxy server, error<%s>.\n",GetLastSocketError());
			return false;
		}

//==========================================
#define SEND_RECV do{\
	if(!NetOutput())\
	{\
		WriteLog("Net output error.\n");\
		return false;\
	}\
	DWORD tick=Timer::FastTick();\
	while(true)\
	{\
		int receive=NetInput(false);\
		if(receive>0) break;\
		if(receive<0)\
		{\
			WriteLog("Net input error.\n");\
			return false;\
		}\
		if(Timer::FastTick()-tick>10000)\
		{\
			WriteLog("Proxy answer timeout.\n");\
			return false;\
		}\
		Sleep(1);\
	}\
}while(0)
//==========================================

		BYTE b1,b2;
		Bin.Reset();
		Bout.Reset();
		// Authentication
		if(GameOpt.ProxyType==PROXY_SOCKS4)
		{
			// Connect
			Bout << BYTE(4); // Socks version
			Bout << BYTE(1); // Connect command
			Bout << WORD(SockAddr.sin_port);
			Bout << DWORD(SockAddr.sin_addr.s_addr);
			Bout << BYTE(0);
			SEND_RECV;
			Bin >> b1; // Null byte
			Bin >> b2; // Answer code
			if(b2!=0x5A)
			{
				switch(b2)
				{
				case 0x5B: WriteLog("Proxy connection error, request rejected or failed.\n"); break;
				case 0x5C: WriteLog("Proxy connection error, request failed because client is not running identd (or not reachable from the server).\n"); break;
				case 0x5D: WriteLog("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request.\n"); break;
				default: WriteLog("Proxy connection error, Unknown error<%u>.\n",b2); break;
				}
				return false;
			}
		}
		else if(GameOpt.ProxyType==PROXY_SOCKS5)
		{
			Bout << BYTE(5); // Socks version
			Bout << BYTE(1); // Count methods
			Bout << BYTE(2); // Method
			SEND_RECV;
			Bin >> b1; // Socks version
			Bin >> b2; // Method
			if(b2==2) // User/Password
			{
				Bout << BYTE(1); // Subnegotiation version
				Bout << BYTE(GameOpt.ProxyUser.length()); // Name length
				Bout.Push(GameOpt.ProxyUser.c_str(),GameOpt.ProxyUser.length()); // Name
				Bout << BYTE(GameOpt.ProxyPass.length()); // Pass length
				Bout.Push(GameOpt.ProxyPass.c_str(),GameOpt.ProxyPass.length()); // Pass
				SEND_RECV;
				Bin >> b1; // Subnegotiation version
				Bin >> b2; // Status
				if(b2!=0)
				{
					WriteLog("Invalid proxy user or password.\n");
					return false;
				}
			}
			else if(b2!=0) // Other authorization
			{
				WriteLog("Socks server connect fail.\n");
				return false;
			}
			// Connect
			Bout << BYTE(5); // Socks version
			Bout << BYTE(1); // Connect command
			Bout << BYTE(0); // Reserved
			Bout << BYTE(1); // IP v4 address
			Bout << DWORD(SockAddr.sin_addr.s_addr);
			Bout << WORD(SockAddr.sin_port);
			SEND_RECV;
			Bin >> b1; // Socks version
			Bin >> b2; // Answer code
			if(b2!=0)
			{
				switch(b2)
				{
				case 1: WriteLog("Proxy connection error, SOCKS-server error.\n"); break;
				case 2: WriteLog("Proxy connection error, connections fail by proxy rules.\n"); break;
				case 3: WriteLog("Proxy connection error, network is not aviable.\n"); break;
				case 4: WriteLog("Proxy connection error, host is not aviable.\n"); break;
				case 5: WriteLog("Proxy connection error, connection denied.\n"); break;
				case 6: WriteLog("Proxy connection error, TTL expired.\n"); break;
				case 7: WriteLog("Proxy connection error, command not supported.\n"); break;
				case 8: WriteLog("Proxy connection error, address type not supported.\n"); break;
				default: WriteLog("Proxy connection error, Unknown error<%u>.\n",b2); break;
				}
				return false;
			}
		}
		else if(GameOpt.ProxyType==PROXY_HTTP)
		{
			char* buf=(char*)Str::Format("CONNECT %s:%d HTTP/1.0\r\n\r\n",inet_ntoa(SockAddr.sin_addr),GameOpt.Port);
			Bout.Push(buf,strlen(buf));
			SEND_RECV;
			buf=Bin.GetCurData();
			/*if(strstr(buf," 407 "))
			{
				char* buf=(char*)Str::Format("CONNECT %s:%d HTTP/1.0\r\nProxy-authorization: basic \r\n\r\n",inet_ntoa(SockAddr.sin_addr),OptPort);
				Bout.Push(buf,strlen(buf));
				SEND_RECV;
				buf=Bin.GetCurData();
			}*/
			if(!strstr(buf," 200 "))
			{
				WriteLog("Proxy connection error, receive message<%s>.\n",buf);
				return false;
			}
		}
		else
		{
			WriteLog("Unknown proxy type<%u>.\n",GameOpt.ProxyType);
			return false;
		}

		Bin.Reset();
		Bout.Reset();
	}

	WriteLog("Connecting successful.\n");

	ZStream.zalloc=zlib_alloc_;
	ZStream.zfree=zlib_free_;
	ZStream.opaque=NULL;
	if(inflateInit(&ZStream)!=Z_OK)
	{
		WriteLog("ZStream InflateInit error.\n");
		return false;
	}
	ZStreamOk=true;

	return true;
}

void FOClient::NetDisconnect()
{
	WriteLog("Disconnect.\n");

	if(ZStreamOk) inflateEnd(&ZStream);
	ZStreamOk=false;
	if(Sock!=INVALID_SOCKET) closesocket(Sock);
	Sock=INVALID_SOCKET;
	NetState=STATE_DISCONNECT;

	WriteLog("Traffic transfered statistics, in bytes:\n"
		"Send<%u> Receive<%u> Sum<%u>.\n"
		"ReceiveReal<%u>.\n",
		BytesSend,BytesReceive,BytesReceive+BytesSend,
		BytesRealReceive);

	SetCurMode(CUR_DEFAULT);
	HexMngr.UnloadMap();
	ClearCritters();
	QuestMngr.Clear();
	Bin.Reset();
	Bout.Reset();
	WriteLog("Disconnect success.\n");
}

void FOClient::ParseSocket()
{
	if(Sock==INVALID_SOCKET) return;

	if(NetInput(true)<0)
	{
		NetState=STATE_DISCONNECT;
	}
	else
	{
		NetProcess();

		if(GameOpt.HelpInfo && Bout.IsEmpty() && !PingTick && Timer::FastTick()>=PingCallTick)
		{
			Net_SendPing(PING_PING);
			PingTick=Timer::FastTick();
		}

		NetOutput();
	}

	if(NetState==STATE_DISCONNECT) NetDisconnect();
}

bool FOClient::NetOutput()
{
	if(Bout.IsEmpty()) return true;

	timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=0;
	FD_ZERO(&SockSet);
	FD_ZERO(&SockSetErr);
	FD_SET(Sock,&SockSet);
	FD_SET(Sock,&SockSetErr);
	if(select(0,NULL,&SockSet,&SockSetErr,&tv)==SOCKET_ERROR) WriteLog(__FUNCTION__" - Select error<%s>.\n",GetLastSocketError());
	if(FD_ISSET(Sock,&SockSetErr)) WriteLog(__FUNCTION__" - Socket error.\n");
	if(!FD_ISSET(Sock,&SockSet)) return true;

	int tosend=Bout.GetEndPos();
	int sendpos=0;
	while(sendpos<tosend)
	{
		DWORD len;
		WSABUF buf;
		buf.buf=Bout.GetData()+sendpos;
		buf.len=tosend-sendpos;
		if(WSASend(Sock,&buf,1,&len,0,NULL,NULL)==SOCKET_ERROR || len==0)
		{
			WriteLog(__FUNCTION__" - SOCKET_ERROR while send to server, error<%s>.\n",GetLastSocketError());
			NetState=STATE_DISCONNECT;
			return false;
		}
		sendpos+=len;
		BytesSend+=len;
	}

	Bout.Reset();
	return true;
}

int FOClient::NetInput(bool unpack)
{
	timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=0;
	FD_ZERO(&SockSet);
	FD_ZERO(&SockSetErr);
	FD_SET(Sock,&SockSet);
	FD_SET(Sock,&SockSetErr);
	if(select(0,&SockSet,NULL,&SockSetErr,&tv)==SOCKET_ERROR) WriteLog(__FUNCTION__" - Select error<%s>.\n",GetLastSocketError());
	if(FD_ISSET(Sock,&SockSetErr)) WriteLog(__FUNCTION__" - Socket error.\n");
	if(!FD_ISSET(Sock,&SockSet)) return 0;

	DWORD len;
	DWORD flags=0;
	WSABUF buf;
	buf.buf=ComBuf;
	buf.len=ComLen;
	if(WSARecv(Sock,&buf,1,&len,&flags,NULL,NULL)==SOCKET_ERROR) 
	{
		WriteLog(__FUNCTION__" - SOCKET_ERROR while receive from server, error<%s>.\n",GetLastSocketError());
		return -1;
	}
	if(len==0)
	{
		WriteLog(__FUNCTION__" - Socket is closed.\n");
		return -2;
	}

	DWORD pos=len;
	while(pos==ComLen)
	{
		DWORD newcomlen=(ComLen<<1);
		char* combuf=new char[newcomlen];
		memcpy(combuf,ComBuf,ComLen);
		SAFEDELA(ComBuf);
		ComBuf=combuf;
		ComLen=newcomlen;

		flags=0;
		buf.buf=ComBuf+pos;
		buf.len=ComLen-pos;
		if(WSARecv(Sock,&buf,1,&len,&flags,NULL,NULL)==SOCKET_ERROR)
		{
			WriteLog(__FUNCTION__" - SOCKET_ERROR2 while receive from server, error<%s>.\n",GetLastSocketError());
			return -1;
		}
		if(len==0)
		{
			WriteLog(__FUNCTION__" - Socket is closed2.\n");
			return -2;
		}

		pos+=len;
	}

	Bin.Refresh();
	DWORD old_pos=Bin.GetEndPos(); // Fix position

 	if(unpack && !GameOpt.DisableZlibCompression)
	{
		ZStream.next_in=(UCHAR*)ComBuf;
		ZStream.avail_in=pos;
		ZStream.next_out=(UCHAR*)Bin.GetData()+Bin.GetEndPos();
		ZStream.avail_out=Bin.GetLen()-Bin.GetEndPos();

		if(inflate(&ZStream,Z_SYNC_FLUSH)!=Z_OK)
		{
			WriteLog(__FUNCTION__" - ZStream Inflate error.\n");
			return -3;
		}

		Bin.SetEndPos(ZStream.next_out-(UCHAR*)Bin.GetData());

		while(ZStream.avail_in)
		{
			Bin.GrowBuf(2048);

			ZStream.next_out=(UCHAR*)Bin.GetData()+Bin.GetEndPos();
			ZStream.avail_out=Bin.GetLen()-Bin.GetEndPos();

			if(inflate(&ZStream,Z_SYNC_FLUSH)!=Z_OK)
			{
				WriteLog(__FUNCTION__" - ZStream Inflate continue error.\n");
				return -4;
			}

			Bin.SetEndPos(ZStream.next_out-(UCHAR*)Bin.GetData());
		}
	}
	else
	{
		Bin.Push(ComBuf,pos);
	}

	BytesReceive+=pos;
	BytesRealReceive+=Bin.GetEndPos()-old_pos;
	return Bin.GetEndPos()-old_pos;
}

void FOClient::NetProcess()
{
	while(Bin.NeedProcess())
	{
		MSGTYPE msg=0;
		Bin >> msg;

		if(GameOpt.DebugNet)
		{
			static DWORD count=0;
			AddMess(FOMB_GAME,Str::Format("%04u) Input net message<%u>.",count,(msg>>8)&0xFF));
			WriteLog("%04u) Input net message<%u>.\n",count,(msg>>8)&0xFF);
			count++;
		}

		switch(msg)
		{
		case NETMSG_LOGIN_SUCCESS:
			Net_OnLoginSuccess();
			break;
		case NETMSG_REGISTER_SUCCESS:
			if(!Singleplayer)
			{
				WriteLog("Registration success.\n");
				if(RegNewCr)
				{
					GameOpt.Name=RegNewCr->Name;
					GameOpt.Pass=RegNewCr->Pass;
					SAFEDEL(RegNewCr);
				}
			}
			else
			{
				WriteLog("World loaded, enter to it.\n");
				LogTryConnect();
			}
			break;

		case NETMSG_PING:
			Net_OnPing();
			break;
		case NETMSG_CHECK_UID0:
			Net_OnCheckUID0();
			break;

		case NETMSG_ADD_PLAYER:
			Net_OnAddCritter(false);
			break;
		case NETMSG_ADD_NPC:
			Net_OnAddCritter(true);
			break;
		case NETMSG_REMOVE_CRITTER:
			Net_OnRemoveCritter();
			break;
		case NETMSG_SOME_ITEM:
			Net_OnSomeItem();
			break;
		case NETMSG_CRITTER_ACTION:
			Net_OnCritterAction();
			break;
		case NETMSG_CRITTER_KNOCKOUT:
			Net_OnCritterKnockout();
			break;
		case NETMSG_CRITTER_MOVE_ITEM:
			Net_OnCritterMoveItem();
			break;
		case NETMSG_CRITTER_ITEM_DATA:
			Net_OnCritterItemData();
			break;
		case NETMSG_CRITTER_ANIMATE:
			Net_OnCritterAnimate();
			break;
		case NETMSG_CRITTER_PARAM:
			Net_OnCritterParam();
			break;
        case NETMSG_CRITTER_MOVE:
			Net_OnCritterMove();
			break;
		case NETMSG_CRITTER_DIR:
			Net_OnCritterDir();
			break;
		case NETMSG_CRITTER_XY:
			Net_OnCritterXY();
			break;
		case NETMSG_CHECK_UID1:
			Net_OnCheckUID1();
			break;

		case NETMSG_CRITTER_TEXT:
			Net_OnText();
			break;
		case NETMSG_MSG:
			Net_OnTextMsg(false);
			break;
		case NETMSG_MSG_LEX:
			Net_OnTextMsg(true);
			break;
		case NETMSG_MAP_TEXT:
			Net_OnMapText();
			break;
		case NETMSG_MAP_TEXT_MSG:
			Net_OnMapTextMsg();
			break;
		case NETMSG_MAP_TEXT_MSG_LEX:
			Net_OnMapTextMsgLex();
			break;
		case NETMSG_CHECK_UID2:
			Net_OnCheckUID2();
			break;

			break;
		case NETMSG_ALL_PARAMS:
			Net_OnChosenParams();
			break;
		case NETMSG_PARAM:
			Net_OnChosenParam();
			break;
		case NETMSG_CRAFT_ASK:
			Net_OnCraftAsk();
			break;
		case NETMSG_CRAFT_RESULT:
			Net_OnCraftResult();
			break;
		case NETMSG_CLEAR_ITEMS:
			Net_OnChosenClearItems();
			break;
		case NETMSG_ADD_ITEM:
			Net_OnChosenAddItem();
			break;
		case NETMSG_REMOVE_ITEM:
			Net_OnChosenEraseItem();
			break;
		case NETMSG_CHECK_UID3:
			Net_OnCheckUID3();
			break;

		case NETMSG_TALK_NPC:
			Net_OnChosenTalk();
			break;

		case NETMSG_GAME_INFO:
			Net_OnGameInfo();
			break;

		case NETMSG_QUEST:
			Net_OnQuest(false);
			break;
		case NETMSG_QUESTS:
			Net_OnQuest(true);
			break;
		case NETMSG_HOLO_INFO:
			Net_OnHoloInfo();
			break;
		case NETMSG_SCORES:
			Net_OnScores();
			break;
		case NETMSG_USER_HOLO_STR:
			Net_OnUserHoloStr();
			break;
		case NETMSG_AUTOMAPS_INFO:
			Net_OnAutomapsInfo();
			break;
		case NETMSG_CHECK_UID4:
			Net_OnCheckUID4();
			break;
		case NETMSG_VIEW_MAP:
			Net_OnViewMap();
			break;

		case NETMSG_LOADMAP:
			Net_OnLoadMap();
			break;
		case NETMSG_MAP:
			Net_OnMap();
			break;
		case NETMSG_GLOBAL_INFO:
			Net_OnGlobalInfo();
			break;
		case NETMSG_GLOBAL_ENTRANCES:
			Net_OnGlobalEntrances();
			break;
		case NETMSG_CONTAINER_INFO:
			Net_OnContainerInfo();
			break;
		case NETMSG_FOLLOW:
			Net_OnFollow();
			break;
		case NETMSG_PLAYERS_BARTER:
			Net_OnPlayersBarter();
			break;
		case NETMSG_PLAYERS_BARTER_SET_HIDE:
			Net_OnPlayersBarterSetHide();
			break;
		case NETMSG_SHOW_SCREEN:
			Net_OnShowScreen();
			break;
		case NETMSG_RUN_CLIENT_SCRIPT:
			Net_OnRunClientScript();
			break;
		case NETMSG_DROP_TIMERS:
			Net_OnDropTimers();
			break;
		case NETMSG_CRITTER_LEXEMS:
			Net_OnCritterLexems();
			break;
		case NETMSG_ITEM_LEXEMS:
			Net_OnItemLexems();
			break;

		case NETMSG_ADD_ITEM_ON_MAP:
			Net_OnAddItemOnMap();
			break;
		case NETMSG_CHANGE_ITEM_ON_MAP:
			Net_OnChangeItemOnMap();
			break;
		case NETMSG_ERASE_ITEM_FROM_MAP:
			Net_OnEraseItemFromMap();
			break;
		case NETMSG_ANIMATE_ITEM:
			Net_OnAnimateItem();
			break;
		case NETMSG_COMBAT_RESULTS:
			Net_OnCombatResult();
			break;
		case NETMSG_EFFECT:
			Net_OnEffect();
			break;
		case NETMSG_FLY_EFFECT:
			Net_OnFlyEffect();
			break;
		case NETMSG_PLAY_SOUND:
			Net_OnPlaySound(false);
			break;
		case NETMSG_PLAY_SOUND_TYPE:
			Net_OnPlaySound(true);
			break;

		case NETMSG_MSG_DATA:
			Net_OnMsgData();
			break;
		case NETMSG_ITEM_PROTOS:
			Net_OnProtoItemData();
			break;

		default:
			if(GameOpt.DebugNet) AddMess(FOMB_GAME,Str::Format("Invalid msg<%u>. Seek valid.",(msg>>8)&0xFF));
			WriteLog("Invalid msg<%u>. Seek valid.\n",msg);
			Bin.MoveReadPos(sizeof(MSGTYPE));
			Bin.SeekValidMsg();
			return;
		}

		if(NetState==STATE_DISCONNECT) return;
	}
}

void FOClient::Net_SendLogIn(const char* name, const char* pass)
{
	char name_[MAX_NAME+1]={0};
	char pass_[MAX_NAME+1]={0};
	if(name) StringCopy(name_,name);
	if(pass) StringCopy(pass_,pass);

	DWORD uid1=*UID1;
	Bout << NETMSG_LOGIN;
	Bout << (WORD)FO_PROTOCOL_VERSION;
	DWORD uid4=*UID4;
	Bout << uid4; uid4=uid1;																	// UID4
	Bout.Push(name_,MAX_NAME);
	Bout << uid1; uid4^=uid1*Random(0,432157)+*UID3;											// UID1
	Bout.Push(pass_,MAX_NAME);
	DWORD uid2=*UID2;
	Bout << CurLang.Name;
	for(int i=0;i<TEXTMSG_COUNT;i++) Bout << CurLang.Msg[i].GetHash();
	Bout << UIDXOR;																				// UID xor
	DWORD uid3=*UID3;
	Bout << uid3; uid3^=uid1; uid1|=uid3/Random(2,53);											// UID3
	Bout << uid2; uid3^=uid2; uid1|=uid4+222-*UID2;												// UID2
	Bout << UIDOR;																				// UID or
	for(int i=1;i<ITEM_MAX_TYPES;i++) Bout << ItemMngr.GetProtosHash(i);
	Bout << UIDCALC;																			// UID uidcalc
	Bout << GameOpt.DefaultCombatMode;
	DWORD uid0=*UID0;
	Bout << uid0; uid3^=uid1+Random(0,53245); uid1|=uid3+*UID0; uid3^=uid1+uid3; uid1|=uid3;	// UID0

	if(!Singleplayer && name) AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_CONN_SUCCESS));
}

void FOClient::Net_SendCreatePlayer(CritterCl* newcr)
{
	WriteLog("Player registration...");

	if(!newcr)
	{
		WriteLog("internal error.\n");
		return;
	}

	WORD count=0;
	for(int i=0;i<MAX_PARAMS;i++)
		if(newcr->ParamsReg[i] && CritterCl::ParamsRegEnabled[i]) count++;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(WORD)+MAX_NAME*2+sizeof(count)+(sizeof(WORD)+sizeof(int))*count;

	Bout << NETMSG_CREATE_CLIENT;
	Bout << msg_len;

	Bout << (WORD)FO_PROTOCOL_VERSION;
	Bout.Push(newcr->GetName(),MAX_NAME);
	Bout.Push(newcr->GetPass(),MAX_NAME);

	Bout << count;
	for(WORD i=0;i<MAX_PARAMS;i++)
	{
		int val=newcr->ParamsReg[i];
		if(val && CritterCl::ParamsRegEnabled[i])
		{
			Bout << i;
			Bout << val;
		}
	}

	WriteLog("complete.\n");
}

void FOClient::Net_SendSaveLoad(bool save, const char* fname, ByteVec* pic_data)
{
	if(save) WriteLog("Request save to file<%s>...",fname);
	else WriteLog("Request load from file<%s>...",fname);

	MSGTYPE msg=NETMSG_SINGLEPLAYER_SAVE_LOAD;
	WORD fname_len=strlen(fname);
	DWORD msg_len=sizeof(msg)+sizeof(save)+sizeof(fname_len)+fname_len;
	if(save) msg_len+=sizeof(DWORD)+pic_data->size();

	Bout << msg;
	Bout << msg_len;
	Bout << save;
	Bout << fname_len;
	Bout.Push(fname,fname_len);
	if(save)
	{
		Bout << (DWORD)pic_data->size();
		if(pic_data->size()) Bout.Push((char*)&(*pic_data)[0],pic_data->size());
	}

	WriteLog("complete.\n");
}

void FOClient::Net_SendText(const char* send_str, BYTE how_say)
{
	if(!send_str || !send_str[0]) return;

	char str_buf[MAX_FOTEXT];
	char* str=str_buf;
	StringCopy(str_buf,send_str);

	bool result=false;
	if(Script::PrepareContext(ClientFunctions.OutMessage,CALL_FUNC_STR,"Game"))
	{
		int say_type=how_say;
		CScriptString* sstr=new CScriptString(str);
		Script::SetArgObject(sstr);
		Script::SetArgAddress(&say_type);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
		StringCopy(str,MAX_FOTEXT,sstr->c_str());
		sstr->Release();
		how_say=say_type;
	}

	if(!result || !str[0]) return;

	if(str[0]=='~')
	{
		str++;
		Net_SendCommand(str);
		return;
	}

	WORD len=strlen(str);
	if(len>=MAX_NET_TEXT) len=MAX_NET_TEXT;
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(how_say)+sizeof(len)+len;

	Bout << NETMSG_SEND_TEXT;
	Bout << msg_len;
	Bout << how_say;
	Bout << len;
	Bout.Push(str,len);
}

struct CmdListDef
{
	char cmd[20];
	BYTE id;
};

const CmdListDef cmdlist[]=
{
	{"~1",CMD_EXIT},
	{"exit",CMD_EXIT},
	{"~2",CMD_MYINFO},
	{"myinfo",CMD_MYINFO},
	{"~3",CMD_GAMEINFO},
	{"gameinfo",CMD_GAMEINFO},
	{"~4",CMD_CRITID},
	{"id",CMD_CRITID},
	{"~5",CMD_MOVECRIT},
	{"move",CMD_MOVECRIT},
	{"~6",CMD_KILLCRIT},
	{"kill",CMD_KILLCRIT},
	{"~7",CMD_DISCONCRIT},
	{"disconnect",CMD_DISCONCRIT},
	{"~8",CMD_TOGLOBAL},
	{"toglobal",CMD_TOGLOBAL},
	{"~9",CMD_RESPAWN},
	{"respawn",CMD_RESPAWN},
	{"~10",CMD_PARAM},
	{"param",CMD_PARAM},
	{"~11",CMD_GETACCESS},
	{"getaccess",CMD_GETACCESS},
	{"~12",CMD_CRASH},
	{"crash",CMD_CRASH},
	{"~13",CMD_ADDITEM},
	{"additem",CMD_ADDITEM},
	{"~14",CMD_ADDITEM_SELF},
	{"additemself",CMD_ADDITEM_SELF},
	{"ais",CMD_ADDITEM_SELF},
	{"~15",CMD_ADDNPC},
	{"addnpc",CMD_ADDNPC},
	{"~16",CMD_ADDLOCATION},
	{"addloc",CMD_ADDLOCATION},
	{"~17",CMD_RELOADSCRIPTS},
	{"reloadscripts",CMD_RELOADSCRIPTS},
	{"~18",CMD_LOADSCRIPT},
	{"loadscript",CMD_LOADSCRIPT},
	{"load",CMD_LOADSCRIPT},
	{"~19",CMD_RELOAD_CLIENT_SCRIPTS},
	{"reloadclientscripts",CMD_RELOAD_CLIENT_SCRIPTS},
	{"rcs",CMD_RELOAD_CLIENT_SCRIPTS},
	{"~20",CMD_RUNSCRIPT},
	{"runscript",CMD_RUNSCRIPT},
	{"run",CMD_RUNSCRIPT},
	{"~21",CMD_RELOADLOCATIONS},
	{"reloadlocations",CMD_RELOADLOCATIONS},
	{"~22",CMD_LOADLOCATION},
	{"loadlocation",CMD_LOADLOCATION},
	{"~23",CMD_RELOADMAPS},
	{"reloadmaps",CMD_RELOADMAPS},
	{"~24",CMD_LOADMAP},
	{"loadmap",CMD_LOADMAP},
	{"~25",CMD_REGENMAP},
	{"regenmap",CMD_REGENMAP},
	{"~26",CMD_RELOADDIALOGS},
	{"reloaddialogs",CMD_RELOADDIALOGS},
	{"~27",CMD_LOADDIALOG},
	{"loaddialog",CMD_LOADDIALOG},
	{"~28",CMD_RELOADTEXTS},
	{"reloadtexts",CMD_RELOADTEXTS},
	{"~29",CMD_RELOADAI},
	{"reloadai",CMD_RELOADAI},
	{"~30",CMD_CHECKVAR},
	{"checkvar",CMD_CHECKVAR},
	{"cvar",CMD_CHECKVAR},
	{"~31",CMD_SETVAR},
	{"setvar",CMD_SETVAR},
	{"svar",CMD_SETVAR},
	{"~32",CMD_SETTIME},
	{"settime",CMD_SETTIME},
	{"~33",CMD_BAN},
	{"ban",CMD_BAN},
	{"~34",CMD_DELETE_ACCOUNT},
	{"deleteself",CMD_DELETE_ACCOUNT},
	{"~35",CMD_CHANGE_PASSWORD},
	{"changepassword",CMD_CHANGE_PASSWORD},
	{"changepass",CMD_CHANGE_PASSWORD},
	{"~36",CMD_DROP_UID},
	{"dropuid",CMD_DROP_UID},
	{"drop",CMD_DROP_UID},
};
const int CMN_LIST_COUNT=sizeof(cmdlist)/sizeof(CmdListDef);

void FOClient::Net_SendCommand(char* str)
{
	AddMess(FOMB_GAME,Str::Format("Command: %s.",str));

	BYTE cmd=GetCmdNum(str);
	if(!cmd)
	{
		AddMess(FOMB_GAME,"Unknown command.");
		WriteLog(__FUNCTION__" - Unknown command<%s>.\n",str);
		return;
	}

	MSGTYPE msg=NETMSG_SEND_COMMAND;
	DWORD msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(cmd);

	switch(cmd)
	{
	case CMD_EXIT:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_MYINFO:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_GAMEINFO:
		{
			int type;
			if(sscanf(str,"%d",&type)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~gameinfo type>.");
				break;
			}

			msg_len+=sizeof(type);
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << type;
		}
		break;
	case CMD_CRITID:
		{
			AddMess(FOMB_GAME,"Command is not implemented.");
			return;

			BYTE name_len=0;
			char cr_name[MAX_NAME+1];
			msg_len+=name_len;
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << name_len;
			Bout.Push(cr_name,name_len);
		}
		break;
	case CMD_MOVECRIT:
		{
			DWORD crid;
			WORD hex_x;
			WORD hex_y;
			if(sscanf(str,"%u%u%u",&crid,&hex_x,&hex_y)!=3)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~move crid hx hy>.");
				break;
			}

			msg_len+=sizeof(crid)+sizeof(hex_x)+sizeof(hex_y);
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << crid;
			Bout << hex_x;
			Bout << hex_y;
		}
		break;
	case CMD_KILLCRIT:
		{
			DWORD crid;
			if(sscanf(str,"%u",&crid)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~kill crid>.");
				break;
			}

			msg_len+=sizeof(crid);
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << crid;
		}
		break;
	case CMD_DISCONCRIT:
		{
			DWORD crid;
			if(sscanf(str,"%u",&crid)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~disconnect crid>.");
				break;
			}

			msg_len+=sizeof(crid);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << crid;
		}
		break;
	case CMD_TOGLOBAL:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_RESPAWN:
		{
			DWORD crid;
			if(sscanf(str,"%u",&crid)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~respawn crid>.");
				break;
			}
			msg_len+=sizeof(crid);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << crid;
		}
		break;
	case CMD_PARAM:
		{
			WORD param_type;
			WORD param_num;
			int param_val;
			if(sscanf(str,"%d%d%d",&param_type,&param_num,&param_val)!=3) break;
			msg_len+=sizeof(WORD)*2+sizeof(int);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << param_type;
			Bout << param_num;
			Bout << param_val;	
		}
		break;
	case CMD_GETACCESS:
		{
			char name_access[64];
			char pasw_access[128];
			if(sscanf(str,"%s%s",name_access,pasw_access)!=2) break;
			Str::Replacement(name_access,'*',' ');
			Str::Replacement(pasw_access,'*',' ');
			msg_len+=MAX_NAME+128;

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;

			Bout.Push(name_access,MAX_NAME);
			Bout.Push(pasw_access,128);
		}
		break;	
	case CMD_CRASH:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_ADDITEM:
		{
			WORD hex_x;
			WORD hex_y;
			WORD pid;
			DWORD count;
			if(sscanf(str,"%u%u%u%u",&hex_x,&hex_y,&pid,&count)!=4)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~additem hx hy pid count>.");
				break;
			}
			msg_len+=sizeof(hex_x)+sizeof(hex_y)+sizeof(pid)+sizeof(count);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << hex_x;
			Bout << hex_y;
			Bout << pid;
			Bout << count;
		}
		break;
	case CMD_ADDITEM_SELF:
		{
			WORD pid;
			DWORD count;
			if(sscanf(str,"%u%u",&pid,&count)!=2)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~additemself pid count>.");
				break;
			}
			msg_len+=sizeof(pid)+sizeof(count);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << pid;
			Bout << count;
		}
		break;
	case CMD_ADDNPC:
		{
			WORD hex_x;
			WORD hex_y;
			BYTE dir;
			WORD pid;
			if(sscanf(str,"%d%d%d%d",&hex_x,&hex_y,&dir,&pid)!=4)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~addnpc hx hy dir pid>.");
				break;
			}
			msg_len+=sizeof(hex_x)+sizeof(hex_y)+sizeof(dir)+sizeof(pid);
			
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << hex_x;
			Bout << hex_y;
			Bout << dir;
			Bout << pid;
		}
		break;
	case CMD_ADDLOCATION:
		{
			WORD wx;
			WORD wy;
			WORD pid;
			if(sscanf(str,"%d%d%d",&wx,&wy,&pid)!=3)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~addloc wx wy pid>.");
				break;
			}
			msg_len+=sizeof(wx)+sizeof(wy)+sizeof(pid);
			
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << wx;
			Bout << wy;
			Bout << pid;
		}
		break;
	case CMD_RELOADSCRIPTS:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_LOADSCRIPT:
		{
			char script_name[MAX_SCRIPT_NAME+1];
			if(sscanf(str,"%s",script_name)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~loadscript name>.");
				break;
			}
			script_name[MAX_SCRIPT_NAME]=0;
			msg_len+=MAX_SCRIPT_NAME;

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout.Push(script_name,MAX_SCRIPT_NAME);
		}
		break;
	case CMD_RELOAD_CLIENT_SCRIPTS:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_RUNSCRIPT:
		{
			char script_name[MAX_SCRIPT_NAME+1];
			char func_name[MAX_SCRIPT_NAME+1];
			DWORD param0,param1,param2;
			if(sscanf(str,"%s%s%d%d%d",script_name,func_name,&param0,&param1,&param2)!=5)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~runscript module func param0 param1 param2>.");
				break;
			}
			script_name[MAX_SCRIPT_NAME]=0;
			func_name[MAX_SCRIPT_NAME]=0;
			msg_len+=MAX_SCRIPT_NAME*2+sizeof(DWORD)*3;

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout.Push(script_name,MAX_SCRIPT_NAME);
			Bout.Push(func_name,MAX_SCRIPT_NAME);
			Bout << param0;
			Bout << param1;
			Bout << param2;
		}
		break;
	case CMD_RELOADLOCATIONS:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_LOADLOCATION:
		{
			WORD loc_pid;
			if(sscanf(str,"%u",&loc_pid)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~loadlocation pid>.");
				break;
			}
			msg_len+=sizeof(loc_pid);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << loc_pid;
		}
		break;
	case CMD_RELOADMAPS:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_LOADMAP:
		{
			WORD map_pid;
			if(sscanf(str,"%u",&map_pid)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~loadmap pid>.");
				break;
			}
			msg_len+=sizeof(map_pid);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << map_pid;
		}
		break;
	case CMD_REGENMAP:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_RELOADDIALOGS:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_LOADDIALOG:
		{
			char dlg_name[128];
			DWORD dlg_id;
			if(sscanf(str,"%s%u",dlg_name,&dlg_id)!=2)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~loaddialog name id>.");
				break;
			}
			dlg_name[127]=0;
			msg_len+=128+sizeof(dlg_id);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout.Push(dlg_name,128);
			Bout << dlg_id;
		}
		break;
	case CMD_RELOADTEXTS:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_RELOADAI:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	case CMD_CHECKVAR:
		{
			WORD tid_var;
			BYTE master_is_npc;
			DWORD master_id;
			DWORD slave_id;
			BYTE full_info;
			if(sscanf(str,"%u%u%u%u%u",&tid_var,&master_is_npc,&master_id,&slave_id,&full_info)!=5)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~checkvar tid_var master_is_npc master_id slave_id full_info>.");
				break;
			}
			msg_len+=sizeof(tid_var)+sizeof(master_is_npc)+sizeof(master_id)+sizeof(slave_id)+sizeof(full_info);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << tid_var;
			Bout << master_is_npc;
			Bout << master_id;
			Bout << slave_id;
			Bout << full_info;
		}
		break;
	case CMD_SETVAR:
		{
			WORD tid_var;
			BYTE master_is_npc;
			DWORD master_id;
			DWORD slave_id;
			int value;
			if(sscanf(str,"%u%u%u%u%d",&tid_var,&master_is_npc,&master_id,&slave_id,&value)!=5)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~setvar tid_var master_is_npc master_id slave_id value>.");
				break;
			}
			msg_len+=sizeof(tid_var)+sizeof(master_is_npc)+sizeof(master_id)+sizeof(slave_id)+sizeof(value);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << tid_var;
			Bout << master_is_npc;
			Bout << master_id;
			Bout << slave_id;
			Bout << value;
		}
		break;
	case CMD_SETTIME:
		{
			int multiplier;
			int year;
			int month;
			int day;
			int hour;
			int minute;
			int second;
			if(sscanf(str,"%d%d%d%d%d%d%d",&multiplier,&year,&month,&day,&hour,&minute,&second)!=7)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~settime tmul year month day hour minute second>.");
				break;
			}
			msg_len+=sizeof(multiplier)+sizeof(year)+sizeof(month)+sizeof(day)+sizeof(hour)+sizeof(minute)+sizeof(second);

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout << multiplier;
			Bout << year;
			Bout << month;
			Bout << day;
			Bout << hour;
			Bout << minute;
			Bout << second;
		}
		break;
	case CMD_BAN:
		{
			istrstream str_(str);
			char params[128]={0};
			char name[128]={0};
			DWORD ban_hours=0;
			char info[128]={0};
			str_ >> params;
			if(!str_.fail()) str_ >> name;
			if(!str_.fail()) str_ >> ban_hours;
			if(!str_.fail()) StringCopy(info,str_.str());
			if(_stricmp(params,"add") && _stricmp(params,"add+") && _stricmp(params,"delete") && _stricmp(params,"list"))
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~ban [add,add+,delete,list] [user] [hours] [comment]>.");
				break;
			}
			Str::Replacement(name,'*',' ');
			Str::Replacement(info,'$','*');
			while(info[0]==' ') Str::CopyBack(info);
			msg_len+=MAX_NAME*2+sizeof(ban_hours)+128;

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout.Push(name,MAX_NAME);
			Bout.Push(params,MAX_NAME);
			Bout << ban_hours;
			Bout.Push(info,128);
		}
		break;
	case CMD_DELETE_ACCOUNT:
		{
			char pass[128];
			if(sscanf(str,"%s",pass)!=1)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~deleteself user_password>.");
				break;
			}
			Str::Replacement(pass,'*',' ');
			msg_len+=MAX_NAME;

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout.Push(pass,MAX_NAME);
		}
		break;
	case CMD_CHANGE_PASSWORD:
		{
			char pass[128];
			char new_pass[128];
			if(sscanf(str,"%s%s",pass,&new_pass)!=2)
			{
				AddMess(FOMB_GAME,"Invalid arguments. Example: <~changepassword current_password new_password>.");
				break;
			}
			Str::Replacement(pass,'*',' ');
			Str::Replacement(new_pass,'*',' ');
			msg_len+=MAX_NAME*2;

			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
			Bout.Push(pass,MAX_NAME);
			Bout.Push(new_pass,MAX_NAME);
		}
		break;
	case CMD_DROP_UID:
		{
			Bout << msg;
			Bout << msg_len;
			Bout << cmd;
		}
		break;
	default:
		AddMess(FOMB_GAME,"Command is not implemented.");
		WriteLog(__FUNCTION__" - Command<%s> is not implemented.\n",str);
		return;
	}
}

BYTE FOClient::GetCmdNum(char*& str)
{
	if(!str) return 0;

	char cmd[64];

	int i=0;
	while(str[0])
	{
		if(str[0]==' ') break;
		cmd[i]=str[0];
		str++;
		i++;
	}
	if(!i) return 0;

	cmd[i]='\0';

	for(int cur_cmd=0;cur_cmd<CMN_LIST_COUNT;cur_cmd++)
		if(!_stricmp(cmd,cmdlist[cur_cmd].cmd)) return cmdlist[cur_cmd].id;

	return 0;
}

void FOClient::Net_SendDir()
{
	if(!Chosen) return;

	Bout << NETMSG_DIR;
	Bout << (BYTE)Chosen->GetDir();
}

void FOClient::Net_SendMove(ByteVec steps)
{
	if(!Chosen) return;

	WORD move_params=0;
	for(int p=0;p<5;p++)
	{
		if(p>=steps.size()) steps.push_back(0xFF);

		switch(steps[p])
		{
		case 0: SETFLAG(move_params,0<<(3*p)); break;
		case 1: SETFLAG(move_params,1<<(3*p)); break;
		case 2: SETFLAG(move_params,2<<(3*p)); break;
		case 3: SETFLAG(move_params,3<<(3*p)); break;
		case 4: SETFLAG(move_params,4<<(3*p)); break;
		case 5: SETFLAG(move_params,5<<(3*p)); break;
		case 0xFF: SETFLAG(move_params,0x7<<(3*p));  break;
		default: return;
		}
	}

	if(Chosen->IsRunning) SETFLAG(move_params,0x8000);

	Bout << (Chosen->IsRunning?NETMSG_SEND_MOVE_RUN:NETMSG_SEND_MOVE_WALK);
	Bout << move_params;
	Bout << Chosen->HexX;
	Bout << Chosen->HexY;
}

void FOClient::Net_SendUseSkill(WORD skill, CritterCl* cr)
{
	Bout << NETMSG_SEND_USE_SKILL;
	Bout << skill;
	Bout << (BYTE)TARGET_CRITTER;
	Bout << cr->GetId();
	Bout << (WORD)0;
}

void FOClient::Net_SendUseSkill(WORD skill, ItemHex* item)
{
	Bout << NETMSG_SEND_USE_SKILL;
	Bout << skill;

	if(item->IsScenOrGrid())
	{
		DWORD hex=(item->GetHexX()<<16)|item->GetHexY();

		Bout << (BYTE)TARGET_SCENERY;
		Bout << hex;
		Bout << item->GetProtoId();
	}
	else
	{
		Bout << (BYTE)TARGET_ITEM;
		Bout << item->GetId();
		Bout << (WORD)0;
	}
}

void FOClient::Net_SendUseSkill(WORD skill, Item* item)
{
	Bout << NETMSG_SEND_USE_SKILL;
	Bout << skill;
	Bout << (BYTE)TARGET_SELF_ITEM;
	Bout << item->GetId();
	Bout << (WORD)0;
}

void FOClient::Net_SendUseItem(BYTE ap, DWORD item_id, WORD item_pid, BYTE rate, BYTE target_type, DWORD target_id, WORD target_pid, DWORD param)
{
	Bout << NETMSG_SEND_USE_ITEM;
	Bout << ap;
	Bout << item_id;
	Bout << item_pid;
	Bout << rate;
	Bout << target_type;
	Bout << target_id;
	Bout << target_pid;
	Bout << param;
}

void FOClient::Net_SendPickItem(WORD targ_x, WORD targ_y, WORD pid)
{
	Bout << NETMSG_SEND_PICK_ITEM;
	Bout << targ_x;
	Bout << targ_y;
	Bout << pid;
}

void FOClient::Net_SendPickCritter(DWORD crid, BYTE pick_type)
{
	Bout << NETMSG_SEND_PICK_CRITTER;
	Bout << crid;
	Bout << pick_type;
}

void FOClient::Net_SendChangeItem(BYTE ap, DWORD item_id, BYTE from_slot, BYTE to_slot, DWORD count)
{
	Bout << NETMSG_SEND_CHANGE_ITEM;
	Bout << ap;
	Bout << item_id;
	Bout << from_slot;
	Bout << to_slot;
	Bout << count;

	CollectContItems();
}

void FOClient::Net_SendItemCont(BYTE transfer_type, DWORD cont_id, DWORD item_id, DWORD count, BYTE take_flags)
{
	Bout << NETMSG_SEND_ITEM_CONT;
	Bout << transfer_type;
	Bout << cont_id;
	Bout << item_id;
	Bout << count;
	Bout << take_flags;

	CollectContItems();
}

void FOClient::Net_SendRateItem()
{
	if(!Chosen) return;
	DWORD rate=Chosen->ItemSlotMain->Data.Rate;
	if(!Chosen->ItemSlotMain->GetId()) rate|=Chosen->ItemSlotMain->GetProtoId()<<16;

	Bout << NETMSG_SEND_RATE_ITEM;
	Bout << rate;
}

void FOClient::Net_SendSortValueItem(Item* item)
{
	Bout << NETMSG_SEND_SORT_VALUE_ITEM;
	Bout << item->GetId();
	Bout << item->GetSortValue();

	CollectContItems();
}

void FOClient::Net_SendTalk(BYTE is_npc, DWORD id_to_talk, BYTE answer)
{
	Bout << NETMSG_SEND_TALK_NPC;
	Bout << is_npc;
	Bout << id_to_talk;
	Bout << answer;
}

void FOClient::Net_SendSayNpc(BYTE is_npc, DWORD id_to_talk, const char* str)
{
	Bout << NETMSG_SEND_SAY_NPC;
	Bout << is_npc;
	Bout << id_to_talk;
	Bout.Push(str,MAX_SAY_NPC_TEXT);
}

void FOClient::Net_SendBarter(DWORD npc_id, ItemVec& cont_sale, ItemVec& cont_buy)
{
	WORD sale_count=(WORD)cont_sale.size();
	WORD buy_count=(WORD)cont_buy.size();
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(sale_count)+sizeof(buy_count)+
		(sizeof(DWORD)+sizeof(DWORD))*sale_count+(sizeof(DWORD)+sizeof(DWORD))*buy_count;

	Bout << NETMSG_SEND_BARTER;
	Bout << msg_len;
	Bout << npc_id;

	Bout << sale_count;
	for(int i=0;i<sale_count;++i)
	{
		Bout << cont_sale[i].GetId();
		Bout << cont_sale[i].GetCount();
	}

	Bout << buy_count;
	for(int i=0;i<buy_count;++i)
	{
		Bout << cont_buy[i].GetId();
		Bout << cont_buy[i].GetCount();
	}
}

void FOClient::Net_SendGetGameInfo()
{
	Bout << NETMSG_SEND_GET_INFO;
}

void FOClient::Net_SendGiveMap(bool automap, WORD map_pid, DWORD loc_id, DWORD tiles_hash, DWORD walls_hash, DWORD scen_hash)
{
	Bout << NETMSG_SEND_GIVE_MAP;
	Bout << automap;
	Bout << map_pid;
	Bout << loc_id;
	Bout << tiles_hash;
	Bout << walls_hash;
	Bout << scen_hash;
}

void FOClient::Net_SendLoadMapOk()
{
	Bout << NETMSG_SEND_LOAD_MAP_OK;
}

void FOClient::Net_SendGiveGlobalInfo(BYTE info_flags)
{
	Bout << NETMSG_SEND_GIVE_GLOBAL_INFO;
	Bout << info_flags;
}

void FOClient::Net_SendRuleGlobal(BYTE command, DWORD param1, DWORD param2)
{
	Bout << NETMSG_SEND_RULE_GLOBAL;
	Bout << command;
	Bout << param1;
	Bout << param2;

	WaitPing();
}

void FOClient::Net_SendRadio()
{
	if(!Chosen) return;
	Item* radio=Chosen->GetRadio();
	if(!radio) return;

	Bout << NETMSG_RADIO;
	Bout << radio->RadioGetChannel();
}

void FOClient::Net_SendLevelUp(WORD perk_up)
{
	if(!Chosen) return;

	WORD count=0;
	for(int i=0;i<SKILL_COUNT;i++) if(ChaSkillUp[i]) count++;
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(count)+sizeof(WORD)*2*count+sizeof(perk_up);

	Bout << NETMSG_SEND_LEVELUP;
	Bout << msg_len;

	// Skills
	Bout << count;
	for(int i=0;i<SKILL_COUNT;i++)
	{
		if(ChaSkillUp[i])
		{
			WORD skill_up=ChaSkillUp[i];
			if(Chosen->IsTagSkill(i+SKILL_BEGIN)) skill_up/=2;

			Bout << WORD(i+SKILL_BEGIN);
			Bout << skill_up;
		}
	}

	// Perks
	Bout << perk_up;
}

void FOClient::Net_SendCraftAsk(DwordVec numbers)
{
	WORD count=numbers.size();
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(count)+sizeof(DWORD)*count;
	Bout << NETMSG_CRAFT_ASK;
	Bout << msg_len;
	Bout << count;
	for(int i=0;i<count;i++) Bout << numbers[i];
	FixNextShowCraftTick=Timer::FastTick()+CRAFT_SEND_TIME;
}

void FOClient::Net_SendCraft(DWORD craft_num)
{
	Bout << NETMSG_SEND_CRAFT;
	Bout << craft_num;
}

void FOClient::Net_SendPing(BYTE ping)
{
	Bout << NETMSG_PING;
	Bout << ping;
}

void FOClient::Net_SendPlayersBarter(BYTE barter, DWORD param, DWORD param_ext)
{
	Bout << NETMSG_PLAYERS_BARTER;
	Bout << barter;
	Bout << param;
	Bout << param_ext;
}

void FOClient::Net_SendScreenAnswer(DWORD answer_i, const char* answer_s)
{
	char answer_s_buf[MAX_SAY_NPC_TEXT+1];
	ZeroMemory(answer_s_buf,sizeof(answer_s_buf));
	StringCopy(answer_s_buf,MAX_SAY_NPC_TEXT+1,answer_s);

	Bout << NETMSG_SEND_SCREEN_ANSWER;
	Bout << answer_i;
	Bout.Push(answer_s_buf,MAX_SAY_NPC_TEXT);
}

void FOClient::Net_SendGetScores()
{
	Bout << NETMSG_SEND_GET_SCORES;
}

void FOClient::Net_SendSetUserHoloStr(Item* holodisk, const char* title, const char* text)
{
	if(!holodisk || !title || !text)
	{
		WriteLog(__FUNCTION__" - Null pointers arguments.\n");
		return;
	}

	WORD title_len=strlen(title);
	WORD text_len=strlen(text);
	if(!title_len || !text_len || title_len>USER_HOLO_MAX_TITLE_LEN || text_len>USER_HOLO_MAX_LEN)
	{
		WriteLog(__FUNCTION__" - Length of texts is greather of maximum or zero, title cur<%u>, title max<%u>, text cur<%u>, text max<%u>.\n",title_len,USER_HOLO_MAX_TITLE_LEN,text_len,USER_HOLO_MAX_LEN);
		return;
	}

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(title_len)+sizeof(text_len)+title_len+text_len;
	Bout << NETMSG_SEND_SET_USER_HOLO_STR;
	Bout << msg_len;
	Bout << holodisk->GetId();
	Bout << title_len;
	Bout << text_len;
	Bout.Push(title,title_len);
	Bout.Push(text,text_len);
}

void FOClient::Net_SendGetUserHoloStr(DWORD str_num)
{
	static DwordSet already_send;
	if(already_send.count(str_num)) return;
	already_send.insert(str_num);

	Bout << NETMSG_SEND_GET_USER_HOLO_STR;
	Bout << str_num;
}

void FOClient::Net_SendCombat(BYTE type, int val)
{
	Bout << NETMSG_SEND_COMBAT;
	Bout << type;
	Bout << val;
}

void FOClient::Net_SendRunScript(bool unsafe, const char* func_name, int p0, int p1, int p2, const char* p3, DwordVec& p4)
{
	WORD func_name_len=strlen(func_name);
	WORD p3len=(p3?strlen(p3):0);
	WORD p4size=p4.size();
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(unsafe)+sizeof(func_name_len)+func_name_len+sizeof(p0)+sizeof(p1)+sizeof(p2)+sizeof(p3len)+p3len+sizeof(p4size)+p4size*sizeof(DWORD);

	Bout << NETMSG_SEND_RUN_SERVER_SCRIPT;
	Bout << msg_len;
	Bout << unsafe;
	Bout << func_name_len;
	Bout.Push(func_name,func_name_len);
	Bout << p0;
	Bout << p1;
	Bout << p2;
	Bout << p3len;
	if(p3len) Bout.Push(p3,p3len);
	Bout << p4size;
	if(p4size) Bout.Push((char*)&p4[0],p4size*sizeof(DWORD));
}

void FOClient::Net_SendKarmaVoting(DWORD crid, bool val_up)
{
	Bout << NETMSG_SEND_KARMA_VOTING;
	Bout << crid;
	Bout << val_up;
}

void FOClient::Net_SendRefereshMe()
{
	Bout << NETMSG_SEND_REFRESH_ME;

	WaitPing();
}

void FOClient::Net_OnLoginSuccess()
{
	NetState=STATE_LOGINOK;
	if(!Singleplayer) AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_LOGINOK));
	WriteLog("Auntification success.\n");

	GmapFreeResources();
	ResMngr.FreeResources(RES_ITEMS);
	CritterCl::FreeAnimations();
}

void FOClient::Net_OnAddCritter(bool is_npc)
{
	DWORD msg_len;
	Bin >> msg_len;

	DWORD crid;
	DWORD base_type;
	Bin >> crid;
	Bin >> base_type;

	WORD hx,hy;
	BYTE dir;
	Bin >> hx;
	Bin >> hy;
	Bin >> dir;

	BYTE cond,cond_ext;
	DWORD flags;
	short multihex;
	Bin >> cond;
	Bin >> cond_ext;
	Bin >> flags;
	Bin >> multihex;

	// Npc
	WORD npc_pid;
	DWORD npc_dialog_id;
	if(is_npc)
	{
		Bin >> npc_pid;
		Bin >> npc_dialog_id;
	}

	// Player
	char cl_name[MAX_NAME+1];
	if(!is_npc)
	{	
		Bin.Pop(cl_name,MAX_NAME);
		cl_name[MAX_NAME]=0;
	}

	// Parameters
	int params[MAX_PARAMS];
	ZeroMemory(params,sizeof(params));
	WORD count,index;
	Bin >> count;
	for(int i=0,j=min(count,MAX_PARAMS);i<j;i++)
	{
		Bin >> index;
		Bin >> params[index<MAX_PARAMS?index:0];
	}

	CHECK_IN_BUFF_ERROR;

	if(!CritType::IsEnabled(base_type)) base_type=DEFAULT_CRTYPE;

	if(!crid) WriteLog(__FUNCTION__" - CritterCl id is zero.\n");
	else if(HexMngr.IsMapLoaded() && (hx>=HexMngr.GetMaxHexX() || hy>=HexMngr.GetMaxHexY() || dir>5)) WriteLog(__FUNCTION__" - Invalid positions hx<%u>, hy<%u>, dir<%u>.\n",hx,hy,dir);
	else
	{
		CritterCl* cr=new CritterCl();
		cr->Id=crid;
		cr->HexX=hx;
		cr->HexY=hy;
		cr->CrDir=dir;
		cr->Cond=cond;
		cr->CondExt=cond_ext;
		cr->Flags=flags;
		cr->Multihex=multihex;
		memcpy(cr->Params,params,sizeof(params));

		if(is_npc)
		{
			cr->Pid=npc_pid;
			cr->Params[ST_DIALOG_ID]=npc_dialog_id;
			cr->Name=MsgDlg->GetStr(STR_NPC_NAME_(cr->Params[ST_DIALOG_ID],cr->Pid));
			if(MsgDlg->Count(STR_NPC_AVATAR_(cr->Params[ST_DIALOG_ID]))) cr->Avatar=MsgDlg->GetStr(STR_NPC_AVATAR_(cr->Params[ST_DIALOG_ID]));
		}
		else
		{
			cr->Name=cl_name;
		}

		cr->DefItemSlotMain.Init(ItemMngr.GetProtoItem(ITEM_DEF_SLOT));
		cr->DefItemSlotExt.Init(ItemMngr.GetProtoItem(ITEM_DEF_SLOT));
		cr->DefItemSlotArmor.Init(ItemMngr.GetProtoItem(ITEM_DEF_ARMOR));

		cr->SetBaseType(base_type);
		cr->Init();

		if(FLAG(cr->Flags,FCRIT_CHOSEN))
		{
			cr->Human=true;
			SetAction(CHOSEN_NONE);
		}

		AddCritter(cr);

		if(cr->IsChosen() && !IsMainScreen(SCREEN_GLOBAL_MAP))
		{
			MoveDirs.clear();
			cr->MoveSteps.clear();
			HexMngr.FindSetCenter(cr->HexX,cr->HexY);
			cr->AnimateStay();
			SndMngr.PlayAmbient(MsgGM->GetStr(STR_MAP_AMBIENT_(HexMngr.GetCurPidMap())));
			ShowMainScreen(SCREEN_GAME);
			ScreenFadeOut();
			HexMngr.RebuildLight();
		}

		const char* look=FmtCritLook(cr,CRITTER_ONLY_NAME);
		if(look) cr->Name=look;
		if(Script::PrepareContext(ClientFunctions.CritterIn,CALL_FUNC_STR,"Game"))
		{
			Script::SetArgObject(cr);
			Script::RunPrepared();
		}

		for(int i=0;i<MAX_PARAMS;i++) cr->ChangeParam(i);
		cr->ProcessChangedParams();
	}
}

void FOClient::Net_OnRemoveCritter()
{
	DWORD remid;
	Bin >> remid;
	CritterCl* cr=GetCritter(remid);
	if(cr) cr->Finish();

	if(Script::PrepareContext(ClientFunctions.CritterOut,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(cr);
		Script::RunPrepared();
	}
}

void FOClient::Net_OnText()
{
	DWORD msg_len;
	DWORD crid;
	BYTE how_say;
	WORD intellect;
	bool unsafe_text;
	WORD len;
	char str[MAX_FOTEXT+1];

	Bin >> msg_len;
	Bin >> crid;
	Bin >> how_say;
	Bin >> intellect;
	Bin >> unsafe_text;

	Bin >> len;
	Bin.Pop(str,min(len,MAX_FOTEXT));
	if(len>MAX_FOTEXT) Bin.Pop(len-MAX_FOTEXT);
	str[min(len,MAX_FOTEXT)]=0;

	CHECK_IN_BUFF_ERROR;

	if(how_say==SAY_FLASH_WINDOW)
	{
		SendMessage(Wnd,WM_FLASH_WINDOW,0,0);
		return;
	}

	if(unsafe_text) Keyb::EraseInvalidChars(str,KIF_NO_SPEC_SYMBOLS);

	OnText(str,crid,how_say,intellect);
}

void FOClient::Net_OnTextMsg(bool with_lexems)
{
	DWORD msg_len;
	DWORD crid;
	BYTE how_say;
	WORD msg_num;
	DWORD num_str;

	if(with_lexems) Bin >> msg_len;
	Bin >> crid;
	Bin >> how_say;
	Bin >> msg_num;
	Bin >> num_str;

	char lexems[MAX_DLG_LEXEMS_TEXT+1]={0};
	if(with_lexems)
	{
		WORD lexems_len;
		Bin >> lexems_len;
		if(lexems_len && lexems_len<=MAX_DLG_LEXEMS_TEXT)
		{
			Bin.Pop(lexems,lexems_len);
			lexems[lexems_len]='\0';
		}
	}

	CHECK_IN_BUFF_ERROR;

	if(how_say==SAY_FLASH_WINDOW)
	{
		SendMessage(Wnd,WM_FLASH_WINDOW,0,0);
		return;
	}

	if(msg_num>=TEXTMSG_COUNT)
	{
		WriteLog(__FUNCTION__" - Msg num invalid value<%u>.\n",msg_num);
		return;
	}

	FOMsg& msg=CurLang.Msg[msg_num];
	if(msg_num==TEXTMSG_HOLO)
	{
		char str[MAX_FOTEXT];
		StringCopy(str,GetHoloText(num_str));
		OnText(str,crid,how_say,10);
	}
	else if(msg.Count(num_str))
	{
		char str[MAX_FOTEXT];
		StringCopy(str,msg.GetStr(num_str));
		FormatTags(str,MAX_FOTEXT,Chosen,GetCritter(crid),lexems);
		OnText(str,crid,how_say,10);
	}
}

void FOClient::OnText(const char* str, DWORD crid, int how_say, WORD intellect)
{
	char fstr[MAX_FOTEXT];
	StringCopy(fstr,str);
	DWORD len=strlen(str);
	if(!len) return;

	DWORD text_delay=GameOpt.TextDelay+len*100;
	if(Script::PrepareContext(ClientFunctions.InMessage,CALL_FUNC_STR,"Game"))
	{
		CScriptString* sstr=new CScriptString(fstr);
		Script::SetArgObject(sstr);
		Script::SetArgAddress(&how_say);
		Script::SetArgAddress(&crid);
		Script::SetArgAddress(&text_delay);
		Script::RunPrepared();
		StringCopy(fstr,sstr->c_str());
		sstr->Release();

		if(!Script::GetReturnedBool()) return;

		len=strlen(fstr);
		if(!len) return;
	}

	// Intellect format
	if(how_say>=SAY_NORM && how_say<=SAY_RADIO) FmtTextIntellect(fstr,intellect);

	// Type stream
	DWORD fstr_cr=0;
	DWORD fstr_mb=0;
	int mess_type=FOMB_TALK;

	switch(how_say)
	{
	case SAY_NORM:
		fstr_mb=STR_MBNORM;
	case SAY_NORM_ON_HEAD:
		fstr_cr=STR_CRNORM;
		break;
	case SAY_SHOUT:
		fstr_mb=STR_MBSHOUT;
	case SAY_SHOUT_ON_HEAD:
		fstr_cr=STR_CRSHOUT;
		Str::Upr(fstr);
		break;
	case SAY_EMOTE:
		fstr_mb=STR_MBEMOTE;
	case SAY_EMOTE_ON_HEAD:
		fstr_cr=STR_CREMOTE;
		break;
	case SAY_WHISP:
		fstr_mb=STR_MBWHISP;
	case SAY_WHISP_ON_HEAD:
		fstr_cr=STR_CRWHISP;
		Str::Lwr(fstr);
		break;
	case SAY_SOCIAL:
		fstr_cr=STR_CRSOCIAL;
		fstr_mb=STR_MBSOCIAL;
		break;
	case SAY_RADIO:
		fstr_mb=STR_MBRADIO;
		break;
	case SAY_NETMSG:
		mess_type=FOMB_GAME;
		fstr_mb=STR_MBNET;
		break;
	default:
		break;
	}

	CritterCl* cr=GetCritter(crid);
	string crit_name=(cr?cr->GetName():"?");

	// CritterCl on head text
	if(fstr_cr && cr) cr->SetText(FmtGameText(fstr_cr,fstr),COLOR_TEXT,text_delay);

	// Message box text
	if(fstr_mb)
	{
		if(how_say==SAY_NETMSG || how_say==SAY_RADIO)
			AddMess(mess_type,FmtGameText(fstr_mb,fstr));
		else
			AddMess(mess_type,FmtGameText(fstr_mb,crit_name.c_str(),fstr));

		if(IsScreenPlayersBarter() && Chosen && (crid==BarterOpponentId || crid==Chosen->GetId()))
		{
			if(how_say==SAY_NETMSG || how_say==SAY_RADIO)
				BarterText+=FmtGameText(fstr_mb,fstr);
			else
				BarterText+=FmtGameText(fstr_mb,crit_name.c_str(),fstr);
			BarterText+="\n";
			BarterText+=Str::Format("|%u ",COLOR_TEXT);
			DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
		}
	}

	// Dialog text
	bool is_dialog=IsScreenPresent(SCREEN__DIALOG);
	bool is_barter=IsScreenPresent(SCREEN__BARTER);
	if((how_say==SAY_DIALOG || how_say==SAY_APPEND) && (is_dialog || is_barter))
	{
		if(is_dialog)
		{
			CritterCl* npc=GetCritter(DlgNpcId);
			FormatTags(fstr,MAX_FOTEXT,Chosen,npc,"");
			if(how_say==SAY_APPEND)
			{
				DlgMainText+=fstr;
				DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,str);
			}
		}
		else if(is_barter)
		{
			BarterText=str;
			DlgMainTextCur=0;
			DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
		}
	}

	// Encounter question
	if(how_say>=SAY_ENCOUNTER_ANY && how_say<=SAY_ENCOUNTER_TB && IsMainScreen(SCREEN_GLOBAL_MAP))
	{
		ShowScreen(SCREEN__DIALOGBOX);
		if(how_say==SAY_ENCOUNTER_ANY)
		{
			DlgboxType=DIALOGBOX_ENCOUNTER_ANY;
			DlgboxButtonText[0]=MsgGame->GetStr(STR_DIALOGBOX_ENCOUNTER_RT);
			DlgboxButtonText[1]=MsgGame->GetStr(STR_DIALOGBOX_ENCOUNTER_TB);
			DlgboxButtonText[2]=MsgGame->GetStr(STR_DIALOGBOX_CANCEL);
			DlgboxButtonsCount=3;
		}
		else if(how_say==SAY_ENCOUNTER_RT)
		{
			DlgboxType=DIALOGBOX_ENCOUNTER_RT;
			DlgboxButtonText[0]=MsgGame->GetStr(STR_DIALOGBOX_ENCOUNTER_RT);
			DlgboxButtonText[1]=MsgGame->GetStr(STR_DIALOGBOX_CANCEL);
			DlgboxButtonsCount=2;
		}
		else
		{
			DlgboxType=DIALOGBOX_ENCOUNTER_TB;
			DlgboxButtonText[0]=MsgGame->GetStr(STR_DIALOGBOX_ENCOUNTER_TB);
			DlgboxButtonText[1]=MsgGame->GetStr(STR_DIALOGBOX_CANCEL);
			DlgboxButtonsCount=2;
		}
		DlgboxWait=Timer::GameTick()+GM_ANSWER_WAIT_TIME;
		StringCopy(DlgboxText,fstr);
	}

	// FixBoy result
	if(how_say==SAY_FIX_RESULT) FixResultStr=fstr;

	// Dialogbox
	if(how_say==SAY_DIALOGBOX_TEXT) StringCopy(DlgboxText,fstr);
	else if(how_say>=SAY_DIALOGBOX_BUTTON(0) && how_say<=SAY_DIALOGBOX_BUTTON(MAX_DLGBOX_BUTTONS)) DlgboxButtonText[how_say-SAY_DIALOGBOX_BUTTON(0)]=fstr;

	// Say box
	if(how_say==SAY_SAY_TITLE) SayTitle=fstr;
	else if(how_say==SAY_SAY_TEXT) StringCopy(SayText,fstr);

	SendMessage(Wnd,WM_FLASH_WINDOW,0,0);
}

void FOClient::OnMapText(const char* str, WORD hx, WORD hy, DWORD color)
{
	DWORD len=strlen(str);
	DWORD text_delay=GameOpt.TextDelay+len*100;
	CScriptString* sstr=new CScriptString(str);
	if(Script::PrepareContext(ClientFunctions.MapMessage,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(sstr);
		Script::SetArgAddress(&hx);
		Script::SetArgAddress(&hy);
		Script::SetArgAddress(&color);
		Script::SetArgAddress(&text_delay);
		Script::RunPrepared();
		if(!Script::GetReturnedBool())
		{
			sstr->Release();
			return;
		}
	}

	MapText t;
	t.HexX=hx;
	t.HexY=hy;
	t.Color=(color?color:COLOR_TEXT);
	t.Fade=false;
	t.StartTick=Timer::GameTick();
	t.Tick=text_delay;
	t.Text=sstr->c_std_str();
	t.Rect=HexMngr.GetRectForText(hx,hy);
	t.EndRect=t.Rect;
	MapTextVecIt it=std::find(GameMapTexts.begin(),GameMapTexts.end(),t);
	if(it!=GameMapTexts.end()) GameMapTexts.erase(it);
	GameMapTexts.push_back(t);

	sstr->Release();
	SendMessage(Wnd,WM_FLASH_WINDOW,0,0);
}

void FOClient::Net_OnMapText()
{
	DWORD msg_len;
	WORD hx,hy;
	DWORD color;
	WORD len;
	char str[MAX_FOTEXT+1];

	Bin >> msg_len;
	Bin >> hx;
	Bin >> hy;
	Bin >> color;

	Bin >> len;
	Bin.Pop(str,min(len,MAX_FOTEXT));
	if(len>MAX_FOTEXT) Bin.Pop(len-MAX_FOTEXT);
	str[min(len,MAX_FOTEXT)]=0;

	CHECK_IN_BUFF_ERROR;

	if(hx>=HexMngr.GetMaxHexX() || hy>=HexMngr.GetMaxHexY())
	{
		WriteLog(__FUNCTION__" - Invalid coords, hx<%u>, hy<%u>, text<%s>.\n",hx,hy,str);
		return;
	}

	OnMapText(str,hx,hy,color);
}

void FOClient::Net_OnMapTextMsg()
{
	WORD hx,hy;
	DWORD color;
	WORD msg_num;
	DWORD num_str;

	Bin >> hx;
	Bin >> hy;
	Bin >> color;
	Bin >> msg_num;
	Bin >> num_str;

	if(msg_num>=TEXTMSG_COUNT)
	{
		WriteLog(__FUNCTION__" - Msg num invalid value, num<%u>.\n",msg_num);
		return;
	}

	static char str[MAX_FOTEXT];
	StringCopy(str,CurLang.Msg[msg_num].GetStr(num_str));
	FormatTags(str,MAX_FOTEXT,Chosen,NULL,"");

	OnMapText(str,hx,hy,color);
}

void FOClient::Net_OnMapTextMsgLex()
{
	DWORD msg_len;
	WORD hx,hy;
	DWORD color;
	WORD msg_num;
	DWORD num_str;
	WORD lexems_len;

	Bin >> msg_len;
	Bin >> hx;
	Bin >> hy;
	Bin >> color;
	Bin >> msg_num;
	Bin >> num_str;
	Bin >> lexems_len;

	char lexems[MAX_DLG_LEXEMS_TEXT+1]={0};
	if(lexems_len && lexems_len<=MAX_DLG_LEXEMS_TEXT)
	{
		Bin.Pop(lexems,lexems_len);
		lexems[lexems_len]='\0';
	}

	CHECK_IN_BUFF_ERROR;

	if(msg_num>=TEXTMSG_COUNT)
	{
		WriteLog(__FUNCTION__" - Msg num invalid value, num<%u>.\n",msg_num);
		return;
	}

	char str[MAX_FOTEXT];
	StringCopy(str,CurLang.Msg[msg_num].GetStr(num_str));
	FormatTags(str,MAX_FOTEXT,Chosen,NULL,lexems);

	OnMapText(str,hx,hy,color);
}

void FOClient::Net_OnCritterDir()
{
	DWORD crid;
	BYTE dir;
	Bin >> crid;
	Bin >> dir;

	if(dir>5)
	{
		WriteLog(__FUNCTION__" - Invalid dir<%u>.\n",dir);
		return;
	}

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;
	cr->SetDir(dir);
}

void FOClient::Net_OnCritterMove()
{
	DWORD crid;
	WORD move_params;
	WORD new_hx;
	WORD new_hy;
	Bin >> crid;
	Bin >> move_params;
	Bin >> new_hx;
	Bin >> new_hy;

	if(new_hx>=HexMngr.GetMaxHexX() || new_hy>=HexMngr.GetMaxHexY()) return;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	WORD last_hx=cr->GetHexX();
	WORD last_hy=cr->GetHexY();
	cr->IsRunning=FLAG(move_params,0x8000);

	if(cr!=Chosen)
	{
		cr->MoveSteps.resize(cr->CurMoveStep>0?cr->CurMoveStep:0);
		if(cr->CurMoveStep>=0) cr->MoveSteps.push_back(WordPairVecVal(new_hx,new_hy));
		for(int i=1,j=cr->CurMoveStep+1;i<5;i++,j++)
		{
			int dir=(move_params>>(i*3))&7;
			if(dir>5)
			{
				if(j<=0 && (cr->GetHexX()!=new_hx || cr->GetHexY()!=new_hy))
				{
					HexMngr.TransitCritter(cr,new_hx,new_hy,true,true);
					cr->CurMoveStep=-1;
				}
				break;
			}
			MoveHexByDir(new_hx,new_hy,dir,HexMngr.GetMaxHexX(),HexMngr.GetMaxHexY());
			if(j<0) continue;
			cr->MoveSteps.push_back(WordPairVecVal(new_hx,new_hy));
		}
		cr->CurMoveStep++;

		/*if(HexMngr.IsShowTrack())
		{
			HexMngr.ClearHexTrack();
			for(int i=4;i<cr->MoveDirs.size();i++)
			{
				WordPair& step=cr->MoveDirs[i];
				HexMngr.HexTrack[step.second][step.first]=1;
			}
			HexMngr.HexTrack[last_hy][last_hx]=2;
			HexMngr.RefreshMap();
		}*/
	}
	else
	{
		if(IsAction(ACTION_MOVE)) EraseFrontAction();
		MoveDirs.clear();
		HexMngr.TransitCritter(cr,new_hx,new_hy,true,true);
	}
}

void FOClient::Net_OnSomeItem()
{
	DWORD item_id;
	WORD item_pid;
	BYTE slot;
	Item::ItemData data;
	Bin >> item_id;
	Bin >> item_pid;
	Bin >> slot;
	Bin.Pop((char*)&data,sizeof(data));

	ProtoItem* proto_item=ItemMngr.GetProtoItem(item_pid);
	if(!proto_item)
	{
		WriteLog(__FUNCTION__" - Proto item<%u> not found.\n",item_pid);
		return;
	}

	SomeItem.Id=item_id;
	SomeItem.ACC_CRITTER.Slot=slot;
	SomeItem.Init(proto_item);
	SomeItem.Data=data;
}

void FOClient::Net_OnCritterAction()
{
	DWORD crid;
	int action;
	int action_ext;
	bool is_item;
	Bin >> crid;
	Bin >> action;
	Bin >> action_ext;
	Bin >> is_item;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;
	cr->Action(action,action_ext,is_item?&SomeItem:NULL,false);
}

void FOClient::Net_OnCritterKnockout()
{
	DWORD crid;
	bool face_up;
	WORD knock_hx;
	WORD knock_hy;
	Bin >> crid;
	Bin >> face_up;
	Bin >> knock_hx;
	Bin >> knock_hy;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	cr->Action(ACTION_KNOCKOUT,face_up?0:1,NULL,false);

	if(cr->GetHexX()!=knock_hx || cr->GetHexY()!=knock_hy)
	{
		// TODO: offsets
		HexMngr.TransitCritter(cr,knock_hx,knock_hy,false,true);
	}
}

void FOClient::Net_OnCritterMoveItem()
{
	DWORD msg_len;
	DWORD crid;
	BYTE action;
	BYTE prev_slot;
	bool is_item;
	Bin >> msg_len;
	Bin >> crid;
	Bin >> action;
	Bin >> prev_slot;
	Bin >> is_item;

	// Slot items
	ByteVec slots_data_slot;
	DwordVec slots_data_id;
	WordVec slots_data_pid;
	vector<Item::ItemData> slots_data_data;
	BYTE slots_data_count;
	Bin >> slots_data_count;
	for(BYTE i=0;i<slots_data_count;i++)
	{
		BYTE slot;
		DWORD id;
		WORD pid;
		Item::ItemData data;
		Bin >> slot;
		Bin >> id;
		Bin >> pid;
		Bin.Pop((char*)&data,sizeof(data));
		slots_data_slot.push_back(slot);
		slots_data_id.push_back(id);
		slots_data_pid.push_back(pid);
		slots_data_data.push_back(data);
	}

	CHECK_IN_BUFF_ERROR;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	if(action==ACTION_HIDE_ITEM || action==ACTION_MOVE_ITEM) cr->Action(action,prev_slot,is_item?&SomeItem:NULL,false);

	if(cr!=Chosen)
	{
		__int64 prev_hash_sum=0;
		for(ItemPtrMapIt it=cr->InvItems.begin(),end=cr->InvItems.end();it!=end;++it)
		{
			Item* item=(*it).second;
			prev_hash_sum+=item->LightGetHash();
		}

		cr->EraseAllItems();
		cr->DefItemSlotMain.Init(ItemMngr.GetProtoItem(ITEM_DEF_SLOT));
		cr->DefItemSlotExt.Init(ItemMngr.GetProtoItem(ITEM_DEF_SLOT));
		cr->DefItemSlotArmor.Init(ItemMngr.GetProtoItem(ITEM_DEF_ARMOR));

		for(BYTE i=0;i<slots_data_count;i++)
		{
			ProtoItem* proto_item=ItemMngr.GetProtoItem(slots_data_pid[i]);
			if(proto_item)
			{
				Item* item=new Item();
				item->Id=slots_data_id[i];
				item->Init(proto_item);
				item->Data=slots_data_data[i];
				item->ACC_CRITTER.Slot=slots_data_slot[i];
				cr->AddItem(item);
			}
		}

		__int64 hash_sum=0;
		for(ItemPtrMapIt it=cr->InvItems.begin(),end=cr->InvItems.end();it!=end;++it)
		{
			Item* item=(*it).second;
			hash_sum+=item->LightGetHash();
		}
		if(hash_sum!=prev_hash_sum) HexMngr.RebuildLight();
	}

	if(action==ACTION_SHOW_ITEM || action==ACTION_REFRESH) cr->Action(action,prev_slot,is_item?&SomeItem:NULL,false);
}

void FOClient::Net_OnCritterItemData()
{
	DWORD crid;
	BYTE slot;
	Item::ItemData data;
	ZeroMemory(&data,sizeof(data));
	Bin >> crid;
	Bin >> slot;
	Bin.Pop((char*)&data,sizeof(data));

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	Item* item=cr->GetItemSlot(slot);
	if(item)
	{
		DWORD light_hash=item->LightGetHash();
		item->Data=data;
		if(item->LightGetHash()!=light_hash) HexMngr.RebuildLight();
	}
}

void FOClient::Net_OnCritterAnimate()
{
	DWORD crid;
	DWORD anim1;
	DWORD anim2;
	bool is_item;
	bool clear_sequence;
	bool delay_play;
	Bin >> crid;
	Bin >> anim1;
	Bin >> anim2;
	Bin >> is_item;
	Bin >> clear_sequence;
	Bin >> delay_play;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	if(clear_sequence) cr->ClearAnim();
	if(delay_play || !cr->IsAnim()) cr->Animate(anim1,anim2,is_item?&SomeItem:NULL);
}

void FOClient::Net_OnCheckUID0()
{
#define CHECKUIDBIN \
	DWORD uid[5];\
	DWORD uidxor[5];\
	BYTE rnd_count,rnd_count2;\
	BYTE dummy;\
	DWORD msg_len;\
	Bin >> msg_len;\
	Bin >> uid[3];\
	Bin >> uidxor[0];\
	Bin >> rnd_count;\
	Bin >> uid[1];\
	Bin >> uidxor[2];\
	for(int i=0;i<rnd_count;i++) Bin >> dummy;\
	Bin >> uid[2];\
	Bin >> uidxor[1];\
	Bin >> uid[4];\
	Bin >> rnd_count2;\
	Bin >> uidxor[3];\
	Bin >> uidxor[4];\
	Bin >> uid[0];\
	for(int i=0;i<5;i++) uid[i]^=uidxor[i];\
	for(int i=0;i<rnd_count2;i++) Bin >> dummy;\
	CHECK_IN_BUFF_ERROR

	CHECKUIDBIN;
	if(CHECK_UID0(uid)) UIDFail=true;
}

void FOClient::Net_OnCritterParam()
{
	DWORD crid;
	WORD index;
	int value;
	Bin >> crid;
	Bin >> index;
	Bin >> value;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	if(index<MAX_PARAMS)
	{
		cr->ChangeParam(index);
		cr->Params[index]=value;
		cr->ProcessChangedParams();
	}

	if(index>=ST_ANIM3D_LAYER_BEGIN && index<=ST_ANIM3D_LAYER_END)
	{
		if(!cr->IsAnim()) cr->Action(ACTION_REFRESH,0,NULL,false);
	}
	else if(index==OTHER_FLAGS)
	{
		cr->Flags=value;
	}
	else if(index==OTHER_BASE_TYPE)
	{
		if(cr->Multihex<0 && CritType::GetMultihex(cr->GetCrType())!=CritType::GetMultihex(value))
		{
			HexMngr.SetMultihex(cr->GetHexX(),cr->GetHexY(),CritType::GetMultihex(cr->GetCrType()),false);
			HexMngr.SetMultihex(cr->GetHexX(),cr->GetHexY(),CritType::GetMultihex(value),true);
		}

		cr->SetBaseType(value);
		if(!cr->IsAnim()) cr->Action(ACTION_REFRESH,0,NULL,false);
	}
	else if(index==OTHER_MULTIHEX)
	{
		int old_mh=cr->GetMultihex();
		cr->Multihex=value;
		if(old_mh!=cr->GetMultihex())
		{
			HexMngr.SetMultihex(cr->GetHexX(),cr->GetHexY(),old_mh,false);
			HexMngr.SetMultihex(cr->GetHexX(),cr->GetHexY(),cr->GetMultihex(),true);
		}
	}
	else if(index==OTHER_YOU_TURN)
	{
		TurnBasedTime=0;
		HexMngr.SetCritterContour(cr->GetId(),Sprite::ContourCustom);
	}
}

void FOClient::Net_OnCheckUID1()
{
	CHECKUIDBIN;
	if(CHECK_UID1(uid)) ExitProcess(0);
}

void FOClient::Net_OnCritterXY()
{
	DWORD crid;
	WORD hx;
	WORD hy;
	BYTE dir;
	Bin >> crid;
	Bin >> hx;
	Bin >> hy;
	Bin >> dir;
	if(GameOpt.DebugNet) AddMess(FOMB_GAME,Str::Format(" - crid<%u> hx<%u> hy<%u> dir<%u>.",crid,hx,hy,dir));

	if(!HexMngr.IsMapLoaded()) return;

	CritterCl* cr=GetCritter(crid);
	if(!cr) return;

	if(hx>=HexMngr.GetMaxHexX() || hy>=HexMngr.GetMaxHexY() || dir>5)
	{
		WriteLog(__FUNCTION__" - Error data, hx<%d>, hy<%d>, dir<%d>.\n",hx,hy,dir);
		return;
	}

	if(cr->GetDir()!=dir) cr->SetDir(dir);

	if(cr->GetHexX()!=hx || cr->GetHexY()!=hy)
	{
		Field& f=HexMngr.GetField(hx,hy);
		if(f.Crit && f.Crit->IsFinishing()) EraseCritter(f.Crit->GetId());

		HexMngr.TransitCritter(cr,hx,hy,false,true);
		cr->AnimateStay();
		if(cr==Chosen)
		{
			//Chosen->TickStart(GameOpt.Breaktime);
			Chosen->TickStart(200);
			//SetAction(CHOSEN_NONE);
			MoveDirs.clear();
			Chosen->MoveSteps.clear();
			RebuildLookBorders=true;
		}
	}

	cr->ZeroSteps();
}

void FOClient::Net_OnChosenParams()
{
	WriteLog("Chosen parameters...");

	if(!Chosen)
	{
		char buf[NETMSG_ALL_PARAMS_SIZE-sizeof(MSGTYPE)];
		Bin.Pop(buf,NETMSG_ALL_PARAMS_SIZE-sizeof(MSGTYPE));
		WriteLog("chosen not created, skip.\n");
		return;
	}

	// Params
	Bin.Pop((char*)Chosen->Params,sizeof(Chosen->Params));

	// Process
	if(Chosen->GetTimeout(TO_BATTLE)) AnimRun(IntWCombatAnim,ANIMRUN_SET_FRM(244));
	else AnimRun(IntWCombatAnim,ANIMRUN_SET_FRM(0));
	if(IsScreenPresent(SCREEN__CHARACTER)) ChaPrepareSwitch();

	// Process all changed parameters
	for(int i=0;i<MAX_PARAMS;i++) Chosen->ChangeParam(i);
	Chosen->ProcessChangedParams();

	// Animate
	if(!Chosen->IsAnim()) Chosen->AnimateStay();

	// Refresh borders
	RebuildLookBorders=true;

	WriteLog("complete.\n");
}

void FOClient::Net_OnChosenParam()
{
	WORD index;
	int value;
	Bin >> index;
	Bin >> value;

	// Chosen specified parameters
	if(!Chosen) return;

	int old_value=0;
	if(index<MAX_PARAMS)
	{
		Chosen->ChangeParam(index);
		old_value=Chosen->Params[index];
		Chosen->Params[index]=value;
		Chosen->ProcessChangedParams();
	}

	if(index>=ST_ANIM3D_LAYER_BEGIN && index<=ST_ANIM3D_LAYER_END)
	{
		if(!Chosen->IsAnim()) Chosen->Action(ACTION_REFRESH,0,NULL,false);
		return;
	}

	switch(index)
	{
	case ST_LEVEL:
		{
			if(value>old_value)
			{
				SndMngr.PlaySound(SND_LEVELUP);
				AddMess(FOMB_GAME,MsgGame->GetStr(STR_GAIN_LEVELUP));
				IntMessTabLevelUp=true;
			}
		}
		break;
	case ST_EXPERIENCE:
		{
			if(value>old_value) AddMess(FOMB_GAME,FmtGameText(STR_GAIN_EXPERIENCE,value-old_value));
		}
		break;
	case ST_UNSPENT_PERKS:
		{
			if(value>0 && GetActiveScreen()==SCREEN__CHARACTER) ShowScreen(SCREEN__PERK);
		}
		break;
	case ST_CURRENT_AP:
		{
			Chosen->ApRegenerationTick=0;
		}
		break;
	case TO_BATTLE:
		{
			if(Chosen->GetTimeout(TO_BATTLE))
			{
				AnimRun(IntWCombatAnim,ANIMRUN_TO_END);
				if(AnimGetCurSprCnt(IntWCombatAnim)==0) SndMngr.PlaySound(SND_COMBAT_MODE_ON);
			}
			else
			{
				AnimRun(IntWCombatAnim,ANIMRUN_FROM_END);
				if(AnimGetCurSprCnt(IntWCombatAnim)==AnimGetSprCount(IntWCombatAnim)-1) SndMngr.PlaySound(SND_COMBAT_MODE_OFF);
			}
		}
		break;
	case OTHER_BREAK_TIME:
		{
			if(value<0) value=0;
			Chosen->TickStart(value);
			SetAction(CHOSEN_NONE);
			MoveDirs.clear();
			Chosen->MoveSteps.clear();
		}
		break;
	case OTHER_FLAGS:
		{
			Chosen->Flags=value;
		}
		break;
	case OTHER_BASE_TYPE:
		{
			if(Chosen->Multihex<0 && CritType::GetMultihex(Chosen->GetCrType())!=CritType::GetMultihex(value))
			{
				HexMngr.SetMultihex(Chosen->GetHexX(),Chosen->GetHexY(),CritType::GetMultihex(Chosen->GetCrType()),false);
				HexMngr.SetMultihex(Chosen->GetHexX(),Chosen->GetHexY(),CritType::GetMultihex(value),true);
			}

			Chosen->SetBaseType(value);
			if(!Chosen->IsAnim()) Chosen->Action(ACTION_REFRESH,0,NULL,false);
		}
		break;
	case OTHER_MULTIHEX:
		{
			int old_mh=Chosen->GetMultihex();
			Chosen->Multihex=value;
			if(old_mh!=Chosen->GetMultihex())
			{
				HexMngr.SetMultihex(Chosen->GetHexX(),Chosen->GetHexY(),old_mh,false);
				HexMngr.SetMultihex(Chosen->GetHexX(),Chosen->GetHexY(),Chosen->GetMultihex(),true);
			}
		}
		break;
	case OTHER_YOU_TURN:
		{
			if(value<0) value=0;
			ChosenAction.clear();
			TurnBasedTime=Timer::GameTick()+value;
			HexMngr.SetCritterContour(0,Sprite::ContourNone);
			SendMessage(Wnd,WM_FLASH_WINDOW,0,0);
		}
		break;
	case OTHER_CLEAR_MAP:
		{
			CritMap crits=HexMngr.GetCritters();
			for(CritMapIt it=crits.begin(),end=crits.end();it!=end;++it)
			{
				CritterCl* cr=(*it).second;
				if(cr!=Chosen) EraseCritter(cr->GetId());
			}
			ItemHexVec items=HexMngr.GetItems();
			for(ItemHexVecIt it=items.begin(),end=items.end();it!=end;++it)
			{
				ItemHex* item=*it;
				if(item->IsItem()) HexMngr.DeleteItem(*it,true);
			}
		}
		break;
	case OTHER_TELEPORT:
		{
			WORD hx=(value>>16)&0xFFFF;
			WORD hy=value&0xFFFF;
			if(hx<HexMngr.GetMaxHexX() && hy<HexMngr.GetMaxHexY())
			{
				CritterCl* cr=HexMngr.GetField(hx,hy).Crit;
				if(Chosen==cr) break;
				if(!Chosen->IsDead() && cr) EraseCritter(cr->GetId());
				HexMngr.RemoveCrit(Chosen);
				Chosen->HexX=hx;
				Chosen->HexY=hy;
				HexMngr.SetCrit(Chosen);
				HexMngr.ScrollToHex(Chosen->GetHexX(),Chosen->GetHexY(),0.1,true);
			}
		}
		break;
	default:
		break;
	}

	if(IsScreenPresent(SCREEN__CHARACTER)) ChaPrepareSwitch();
	RebuildLookBorders=true; // Maybe changed some parameter influencing on look borders
}

void FOClient::Net_OnChosenClearItems()
{
	if(Chosen)
	{
		if(Chosen->IsHaveLightSources()) HexMngr.RebuildLight();
		Chosen->EraseAllItems();
	}
	CollectContItems();
}

void FOClient::Net_OnChosenAddItem()
{
	DWORD item_id;
	WORD pid;
	BYTE slot;
	Bin >> item_id;
	Bin >> pid;
	Bin >> slot;

	Item* item=NULL;
	BYTE prev_slot=SLOT_INV;
	DWORD prev_light_hash=0;
	if(Chosen)
	{
		item=Chosen->GetItem(item_id);
		if(item)
		{
			prev_slot=item->ACC_CRITTER.Slot;
			prev_light_hash=item->LightGetHash();
			Chosen->EraseItem(item,false);
			item=NULL;
		}

		ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
		if(proto_item)
		{
			item=new Item();
			item->Id=item_id;
			item->Init(proto_item);
		}
	}

	if(!item) 
	{
		WriteLog(__FUNCTION__" - Can't create item, pid<%u>.\n",pid);
		Bin.Pop(sizeof(item->Data));
		return;
	}

	Bin.Pop((char*)&item->Data,sizeof(item->Data));
	item->Accessory=ITEM_ACCESSORY_CRITTER;
	item->ACC_CRITTER.Slot=slot;
	if(item!=Chosen->ItemSlotMain || !item->IsWeapon()) item->SetRate(item->Data.Rate);
	Chosen->AddItem(item);

	if(Script::PrepareContext(ClientFunctions.ItemInvIn,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(item);
		Script::RunPrepared();
	}

	if(slot==SLOT_HAND1 || prev_slot==SLOT_HAND1) RebuildLookBorders=true;
	if(item->LightGetHash()!=prev_light_hash && (slot!=SLOT_INV || prev_slot!=SLOT_INV)) HexMngr.RebuildLight();
	if(item->IsHidden()) Chosen->EraseItem(item,true);
	CollectContItems();
}

void FOClient::Net_OnChosenEraseItem()
{
	DWORD item_id;
	Bin >> item_id;

	if(!Chosen)
	{
		WriteLog(__FUNCTION__" - Chosen is not created.\n");
		return;
	}

	Item* item=Chosen->GetItem(item_id);
	if(!item)
	{
		WriteLog(__FUNCTION__" - Item not found, id<%u>.\n",item_id);
		return;
	}

	if(Script::PrepareContext(ClientFunctions.ItemInvOut,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(item);
		Script::RunPrepared();
	}

	if(item->IsLight() && item->ACC_CRITTER.Slot!=SLOT_INV) HexMngr.RebuildLight();
	Chosen->EraseItem(item,true);
	CollectContItems();
}

void FOClient::Net_OnAddItemOnMap()
{
	DWORD item_id;
	WORD item_pid;
	WORD item_x;
	WORD item_y;
	BYTE is_added;
	Item::ItemData data;
	Bin >> item_id;
	Bin >> item_pid;
	Bin >> item_x;
	Bin >> item_y;
	Bin >> is_added;
	Bin.Pop((char*)&data,sizeof(data));

	if(HexMngr.IsMapLoaded())
	{
		HexMngr.AddItem(item_id,item_pid,item_x,item_y,is_added!=0,&data);
	}
	else // Global map car
	{
		ProtoItem* proto_item=ItemMngr.GetProtoItem(item_pid);
		if(proto_item && proto_item->IsCar())
		{
			SAFEDEL(GmapCar.Car);
			GmapCar.Car=new Item();
			GmapCar.Car->Id=item_id;
			GmapCar.Car->Init(proto_item);
			GmapCar.Car->Data=data;
		}
	}

	Item* item=HexMngr.GetItemById(item_id);
	if(item && Script::PrepareContext(ClientFunctions.ItemMapIn,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(item);
		Script::RunPrepared();
	}

	// Refresh borders
	if(item && !item->IsRaked()) RebuildLookBorders=true;
}

void FOClient::Net_OnChangeItemOnMap()
{
	DWORD item_id;
	Item::ItemData data;
	Bin >> item_id;
	Bin.Pop((char*)&data,sizeof(data));

	Item* item=HexMngr.GetItemById(item_id);
	bool is_raked=(item && item->IsRaked());

	HexMngr.ChangeItem(item_id,data);

	if(item && Script::PrepareContext(ClientFunctions.ItemMapChanged,CALL_FUNC_STR,"Game"))
	{
		Item* prev_state=new Item(*item);
		prev_state->RefCounter=1;
		prev_state->Data=data;
		Script::SetArgObject(item);
		Script::SetArgObject(prev_state);
		Script::RunPrepared();
		prev_state->Release();
	}

	// Refresh borders
	if(item && is_raked!=item->IsRaked()) RebuildLookBorders=true;
}

void FOClient::Net_OnEraseItemFromMap()
{
	DWORD item_id;
	bool is_deleted;
	Bin >> item_id;
	Bin >> is_deleted;

	HexMngr.FinishItem(item_id,is_deleted);

	Item* item=HexMngr.GetItemById(item_id);
	if(item && Script::PrepareContext(ClientFunctions.ItemMapOut,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(item);
		Script::RunPrepared();
	}

	// Refresh borders
	if(item && !item->IsRaked()) RebuildLookBorders=true;
}

void FOClient::Net_OnAnimateItem()
{
	DWORD item_id;
	BYTE from_frm;
	BYTE to_frm;
	Bin >> item_id;
	Bin >> from_frm;
	Bin >> to_frm;

	ItemHex* item=HexMngr.GetItemById(item_id);
	if(item) item->SetAnim(from_frm,to_frm);
}

void FOClient::Net_OnCombatResult()
{
	DWORD msg_len;
	DWORD data_count;
	DwordVec data_vec;
	Bin >> msg_len;
	Bin >> data_count;
	if(data_count>GameOpt.FloodSize/sizeof(DWORD)) return; // Insurance
	if(data_count)
	{
		data_vec.resize(data_count);
		Bin.Pop((char*)&data_vec[0],data_count*sizeof(DWORD));
	}

	CHECK_IN_BUFF_ERROR;

	asIScriptArray* arr=Script::CreateArray("uint[]");
	if(!arr) return;
	arr->Resize(data_count);
	for(int i=0;i<data_count;i++) *((DWORD*)arr->GetElementPointer(i))=data_vec[i];

	if(Script::PrepareContext(ClientFunctions.CombatResult,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(arr);
		Script::RunPrepared();
	}

	arr->Release();
}

void FOClient::Net_OnEffect()
{
	WORD eff_pid;
	WORD hx;
	WORD hy;
	WORD radius;
	Bin >> eff_pid;
	Bin >> hx;
	Bin >> hy;
	Bin >> radius;

	// Base hex effect
	HexMngr.RunEffect(eff_pid,hx,hy,hx,hy);

	// Radius hexes effect
	if(radius>MAX_HEX_OFFSET) radius=MAX_HEX_OFFSET;
	int cnt=NumericalNumber(radius)*6;
	bool odd=(hx&1)!=0;
	short* sx=(odd?SXOdd:SXEven);
	short* sy=(odd?SYOdd:SYEven);
	int maxhx=HexMngr.GetMaxHexX();
	int maxhy=HexMngr.GetMaxHexY();

	for(int i=0;i<cnt;i++)
	{
		int ex=hx+sx[i];
		int ey=hy+sy[i];
		if(ex>=0 && ey>=0 && ex<maxhx && ey<maxhy) HexMngr.RunEffect(eff_pid,ex,ey,ex,ey);
	}
}

#pragma MESSAGE("Synchronize effects showing.")
void FOClient::Net_OnFlyEffect()
{
	WORD eff_pid;
	DWORD eff_cr1_id;
	DWORD eff_cr2_id;
	WORD eff_cr1_hx;
	WORD eff_cr1_hy;
	WORD eff_cr2_hx;
	WORD eff_cr2_hy;
	Bin >> eff_pid;
	Bin >> eff_cr1_id;
	Bin >> eff_cr2_id;
	Bin >> eff_cr1_hx;
	Bin >> eff_cr1_hy;
	Bin >> eff_cr2_hx;
	Bin >> eff_cr2_hy;

	CritterCl* cr1=GetCritter(eff_cr1_id);
	CritterCl* cr2=GetCritter(eff_cr2_id);

	if(cr1)
	{
		eff_cr1_hx=cr1->GetHexX();
		eff_cr1_hy=cr1->GetHexY();
	}

	if(cr2)
	{
		eff_cr2_hx=cr2->GetHexX();
		eff_cr2_hy=cr2->GetHexY();
	}

	if(!HexMngr.RunEffect(eff_pid,eff_cr1_hx,eff_cr1_hy,eff_cr2_hx,eff_cr2_hy))
	{
		WriteLog(__FUNCTION__" - Run effect fail, pid<%u>.\n",eff_pid);
		return;
	}
}

void FOClient::Net_OnPlaySound(bool by_type)
{
	if(!by_type)
	{
		DWORD synchronize_crid;
		char sound_name[17];
		Bin >> synchronize_crid;
		Bin.Pop(sound_name,16);
		sound_name[16]=0;

		SndMngr.PlaySound(sound_name);
	}
	else
	{
		DWORD synchronize_crid;
		BYTE sound_type;
		BYTE sound_type_ext;
		BYTE sound_id;
		BYTE sound_id_ext;
		Bin >> synchronize_crid;
		Bin >> sound_type;
		Bin >> sound_type_ext;
		Bin >> sound_id;
		Bin >> sound_id_ext;
		SndMngr.PlaySoundType(sound_type,sound_type_ext,sound_id,sound_id_ext);
	}
}

void FOClient::Net_OnPing()
{
	BYTE ping;
	Bin >> ping;

	if(ping==PING_WAIT)
	{
		if(GetActiveScreen()==SCREEN__BARTER) SetCurMode(CUR_HAND);
		else SetLastCurMode();
	}
	else if(ping==PING_CLIENT)
	{
		if(UIDFail) return;

		Bout << NETMSG_PING;
		Bout << (BYTE)PING_CLIENT;
	}
	else if(ping==PING_PING)
	{
		PingTime=Timer::FastTick()-PingTick;
		PingTick=0;
		PingCallTick=Timer::FastTick()+PING_CLIENT_INFO_TIME;
	}
}

void FOClient::Net_OnChosenTalk()
{
	DWORD msg_len;
	BYTE is_npc;
	DWORD talk_id;
	BYTE count_answ;
	DWORD text_id;
	DWORD talk_time;

	Bin >> msg_len;
	Bin >> is_npc;
	Bin >> talk_id;
	Bin >> count_answ;

	DlgCurAnswPage=0;
	DlgMaxAnswPage=0;
	DlgAllAnswers.clear();
	DlgAnswers.clear();

	if(!count_answ)
	{
		// End dialog or barter
		if(IsScreenPresent(SCREEN__DIALOG)) HideScreen(SCREEN__DIALOG);
		if(IsScreenPresent(SCREEN__BARTER)) HideScreen(SCREEN__BARTER);
		return;
	}

	// Text params
	WORD lexems_len;
	char lexems[MAX_DLG_LEXEMS_TEXT+1]={0};
	Bin >> lexems_len;
	if(lexems_len && lexems_len<=MAX_DLG_LEXEMS_TEXT)
	{
		Bin.Pop(lexems,lexems_len);
		lexems[lexems_len]='\0';
	}

	// Find critter
	DlgIsNpc=is_npc;
	DlgNpcId=talk_id;
	CritterCl* npc=(is_npc?GetCritter(talk_id):NULL);

	// Avatar
	DlgAvatarSprId=0;
	if(npc && !npc->Avatar.empty()) DlgAvatarSprId=ResMngr.GetAvatarSprId(npc->Avatar.c_str());

	// Main text
	Bin >> text_id;

	char str[MAX_FOTEXT];
	StringCopy(str,MsgDlg->GetStr(text_id));
	FormatTags(str,MAX_FOTEXT,Chosen,npc,lexems);
	DlgMainText=str;
	DlgMainTextCur=0;
	DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,str);

	// Answers
	DwordVec answers_texts;
	for(int i=0;i<count_answ;i++)
	{
		Bin >> text_id;
		answers_texts.push_back(text_id);
	}

	const char answ_beg[]={' ',' ',(char)TEXT_SYMBOL_DOT,' ',0};
	const char page_up[]={(char)TEXT_SYMBOL_UP,(char)TEXT_SYMBOL_UP,(char)TEXT_SYMBOL_UP,0};
	const int page_up_height=SprMngr.GetLinesHeight(DlgAnswText.W(),0,page_up);
	const char page_down[]={(char)TEXT_SYMBOL_DOWN,(char)TEXT_SYMBOL_DOWN,(char)TEXT_SYMBOL_DOWN,0};
	const int page_down_height=SprMngr.GetLinesHeight(DlgAnswText.W(),0,page_down);

	int line=0,height=0,page=0,answ=0;
	while(true)
	{
		INTRECT pos(
			DlgAnswText.L+DlgNextAnswX*line,
			DlgAnswText.T+DlgNextAnswY*line+height,
			DlgAnswText.R+DlgNextAnswX*line,
			DlgAnswText.T+DlgNextAnswY*line+height);

		// Up arrow
		if(page && !line)
		{
			height+=page_up_height;
			pos.B+=page_up_height;
			line++;
			DlgAllAnswers.push_back(Answer(page,pos,page_up,-1));
			continue;
		}

		StringCopy(str,MsgDlg->GetStr(answers_texts[answ]));
		FormatTags(str,MAX_FOTEXT,Chosen,npc,lexems);
		Str::Insert(str,answ_beg); // TODO: GetStr

		height+=SprMngr.GetLinesHeight(DlgAnswText.W(),0,str);
		pos.B=DlgAnswText.T+DlgNextAnswY*line+height;

		if(pos.B>=DlgAnswText.B && line>1)
		{
			// Down arrow
			Answer& answ_prev=DlgAllAnswers.back();
			if(line>2 && DlgAnswText.B-answ_prev.Position.B<page_down_height) // Check free space
			{
				// Not enough space
				// Migrate last answer to next page
				DlgAllAnswers.pop_back();
				pos=answ_prev.Position;
				pos.B=pos.T+page_down_height;
				DlgAllAnswers.push_back(Answer(page,pos,page_down,-2));
				answ--;
			}
			else
			{
				// Add down arrow
				pos.B=pos.T+page_down_height;
				DlgAllAnswers.push_back(Answer(page,pos,page_down,-2));
			}

			page++;
			height=0;
			line=0;
			DlgMaxAnswPage=page;
		}
		else
		{
			// Answer
			DlgAllAnswers.push_back(Answer(page,pos,str,answ));
			line++;
			answ++;
			if(answ>=count_answ) break;
		}
	}

	Bin >> talk_time;

	DlgCollectAnswers(false);
	DlgEndTick=Timer::GameTick()+talk_time;
	if(IsScreenPresent(SCREEN__BARTER)) HideScreen(SCREEN__BARTER);
	ShowScreen(SCREEN__DIALOG);
}

void FOClient::Net_OnCheckUID2()
{
	CHECKUIDBIN;
	if(CHECK_UID2(uid)) UIDFail=true;
}

void FOClient::Net_OnGameInfo()
{
	int time;
	BYTE rain;
	bool turn_based;
	bool no_log_out;
	int* day_time=HexMngr.GetMapDayTime();
	BYTE* day_color=HexMngr.GetMapDayColor();
	Bin >> GameOpt.YearStart;
	Bin >> GameOpt.Year;
	Bin >> GameOpt.Month;
	Bin >> GameOpt.Day;
	Bin >> GameOpt.Hour;
	Bin >> GameOpt.Minute;
	Bin >> GameOpt.Second;
	Bin >> GameOpt.TimeMultiplier;
	Bin >> time;
	Bin >> rain;
	Bin >> turn_based;
	Bin >> no_log_out;
	Bin.Pop((char*)day_time,sizeof(int)*4);
	Bin.Pop((char*)day_color,sizeof(BYTE)*12);

	CHECK_IN_BUFF_ERROR;

	SYSTEMTIME st={GameOpt.YearStart,1,0,1,0,0,0,0};
	FILETIMELI ft;
	if(!SystemTimeToFileTime(&st,&ft.ft)) WriteLog(__FUNCTION__" - FileTimeToSystemTime error<%u>.\n",GetLastError());
	GameOpt.YearStartFT=ft.ul.QuadPart;
	GameOpt.FullSecond=GetFullSecond(GameOpt.Year,GameOpt.Month,GameOpt.Day,GameOpt.Hour,GameOpt.Minute,GameOpt.Second);
	GameOpt.FullSecondStart=GameOpt.FullSecond;
	GameOpt.GameTimeTick=Timer::GameTick();

	HexMngr.SetWeather(time,rain);
	SetDayTime(true);
	IsTurnBased=turn_based;
	if(!IsTurnBased) HexMngr.SetCritterContour(0,Sprite::ContourNone);
	NoLogOut=no_log_out;
}

void FOClient::Net_OnLoadMap()
{
	WriteLog("Change map...");

	WORD map_pid;
	int map_time;
	BYTE map_rain;
	DWORD hash_tiles;
	DWORD hash_walls;
	DWORD hash_scen;
	Bin >> map_pid;
	Bin >> map_time;
	Bin >> map_rain;
	Bin >> hash_tiles;
	Bin >> hash_walls;
	Bin >> hash_scen;

	GameOpt.SpritesZoom=1.0f;
	GmapZoom=1.0f;

	GameMapTexts.clear();
	WriteLog("Unload map...");
	HexMngr.UnloadMap();
	WriteLog("Clear sounds...");
	SndMngr.ClearSounds();
	SendMessage(Wnd,WM_FLASH_WINDOW,0,0);
	ShowMainScreen(SCREEN_WAIT);
	WriteLog("Clear critters...");
	ClearCritters();
	WriteLog("Complete.\n");

	WriteLog("Free resources...");
	ResMngr.FreeResources(RES_IFACE_EXT);
	if(map_pid) // Free global map resources
	{
		GmapFreeResources();
	}
	else // Free local map resources
	{
		ResMngr.FreeResources(RES_ITEMS);
		CritterCl::FreeAnimations();
	}
	WriteLog("Complete.\n");

	DropScroll();
	IsTurnBased=false;

	// Global
	if(!map_pid)
	{
		GmapNullParams();
		ShowMainScreen(SCREEN_GLOBAL_MAP);
		Net_SendLoadMapOk();
		if(IsVideoPlayed()) MusicAfterVideo=MsgGM->GetStr(STR_MAP_MUSIC_(map_pid));
		else SndMngr.PlayMusic(MsgGM->GetStr(STR_MAP_MUSIC_(map_pid)));
		WriteLog("Global map loaded.\n");
		return;
	}

	// Local
	DWORD hash_tiles_cl=0;
	DWORD hash_walls_cl=0;
	DWORD hash_scen_cl=0;
	HexMngr.GetMapHash(map_pid,hash_tiles_cl,hash_walls_cl,hash_scen_cl);

	if(hash_tiles!=hash_tiles_cl || hash_walls!=hash_walls_cl || hash_scen!=hash_scen_cl)
	{
		Net_SendGiveMap(false,map_pid,0,hash_tiles_cl,hash_walls_cl,hash_scen_cl);
		return;
	}

	if(!HexMngr.LoadMap(map_pid))
	{
		WriteLog("Disconnect.\n");
		NetState=STATE_DISCONNECT;
		return;
	}

	HexMngr.SetWeather(map_time,map_rain);
	SetDayTime(true);
	Net_SendLoadMapOk();
	LookBorders.clear();
	ShootBorders.clear();
	if(IsVideoPlayed()) MusicAfterVideo=MsgGM->GetStr(STR_MAP_MUSIC_(map_pid));
	else SndMngr.PlayMusic(MsgGM->GetStr(STR_MAP_MUSIC_(map_pid)));
	WriteLog("Local map loaded.\n");
}

void FOClient::Net_OnMap()
{
	WriteLog("Get map...");

	DWORD msg_len;
	WORD map_pid;
	WORD maxhx,maxhy;
	BYTE send_info;
	Bin >> msg_len;
	Bin >> map_pid;
	Bin >> maxhx;
	Bin >> maxhy;
	Bin >> send_info;

	WriteLog("%u...",map_pid);

	char map_name[256];
	sprintf(map_name,"map%u",map_pid);

	bool tiles=false;
	char* tiles_data=NULL;
	DWORD tiles_len=0;
	bool walls=false;
	char* walls_data=NULL;
	DWORD walls_len=0;
	bool scen=false;
	char* scen_data=NULL;
	DWORD scen_len=0;

	WriteLog("New:");
	if(FLAG(send_info,SENDMAP_TILES))
	{
		WriteLog(" Tiles");
		DWORD count_tiles;
		Bin >> count_tiles;
		if(count_tiles)
		{
			tiles_len=count_tiles*sizeof(DWORD)*2;
			tiles_data=new char[tiles_len];
			Bin.Pop(tiles_data,tiles_len);
		}
		tiles=true;
	}

	if(FLAG(send_info,SENDMAP_WALLS))
	{
		WriteLog(" Walls");
		DWORD count_walls=0;
		Bin >> count_walls;
		if(count_walls)
		{
			walls_len=count_walls*sizeof(ScenToSend);
			walls_data=new char[walls_len];
			Bin.Pop(walls_data,walls_len);
		}
		walls=true;
	}

	if(FLAG(send_info,SENDMAP_SCENERY))
	{
		WriteLog(" Scenery");
		DWORD count_scen=0;
		Bin >> count_scen;
		if(count_scen)
		{
			scen_len=count_scen*sizeof(ScenToSend);
			scen_data=new char[scen_len];
			Bin.Pop(scen_data,scen_len);
		}
		scen=true;
	}

	CHECK_IN_BUFF_ERROR;

	DWORD cache_len;
	BYTE* cache=Crypt.GetCache(map_name,cache_len);
	FileManager fm;

	WriteLog(" Old:");
	if(cache && fm.LoadStream(cache,cache_len))
	{
		DWORD buf_len=fm.GetFsize();
		BYTE* buf=Crypt.Uncompress(fm.GetBuf(),buf_len,50);
		if(buf)
		{
			fm.UnloadFile();
			fm.LoadStream(buf,buf_len);
			delete[] buf;

			if(fm.GetBEDWord()==CLIENT_MAP_FORMAT_VER)
			{
				fm.SetCurPos(0x04); // Skip pid
				fm.GetBEDWord(); // Skip max hx/hy
				fm.GetBEDWord(); // Reserved
				fm.GetBEDWord(); // Reserved

				fm.SetCurPos(0x20);
				DWORD old_tiles_len=fm.GetBEDWord();
				DWORD old_walls_len=fm.GetBEDWord();
				DWORD old_scen_len=fm.GetBEDWord();

				if(!tiles)
				{
					WriteLog(" Tiles");
					tiles_len=old_tiles_len;
					fm.SetCurPos(0x2C);
					tiles_data=new char[tiles_len];
					fm.CopyMem(tiles_data,tiles_len);
					tiles=true;
				}

				if(!walls)
				{
					WriteLog(" Walls");
					walls_len=old_walls_len;
					fm.SetCurPos(0x2C+old_tiles_len);
					walls_data=new char[walls_len];
					fm.CopyMem(walls_data,walls_len);
					walls=true;
				}

				if(!scen)
				{
					WriteLog(" Scenery");
					scen_len=old_scen_len;
					fm.SetCurPos(0x2C+old_tiles_len+old_walls_len);
					scen_data=new char[scen_len];
					fm.CopyMem(scen_data,scen_len);
					scen=true;
				}

				fm.UnloadFile();
			}
		}
	}
	SAFEDELA(cache);
	WriteLog(". ");

	if(tiles && walls && scen)
	{
		fm.ClearOutBuf();

		fm.SetBEDWord(CLIENT_MAP_FORMAT_VER);
		fm.SetBEDWord(map_pid);
		fm.SetBEWord(maxhx);
		fm.SetBEWord(maxhy);
		fm.SetBEDWord(0);
		fm.SetBEDWord(0);

		fm.SetBEDWord(tiles_len/sizeof(DWORD)/2);
		fm.SetBEDWord(walls_len/sizeof(ScenToSend));
		fm.SetBEDWord(scen_len/sizeof(ScenToSend));
		fm.SetBEDWord(tiles_len);
		fm.SetBEDWord(walls_len);
		fm.SetBEDWord(scen_len);

		if(tiles_len) fm.SetData(tiles_data,tiles_len);
		if(walls_len) fm.SetData(walls_data,walls_len);
		if(scen_len) fm.SetData(scen_data,scen_len);

		DWORD obuf_len=fm.GetOutBufLen();
		BYTE* buf=Crypt.Compress(fm.GetOutBuf(),obuf_len);
		if(!buf)
		{
			WriteLog("Failed to compress data<%s>, disconnect.\n",map_name);
			NetState=STATE_DISCONNECT;
			fm.ClearOutBuf();
			return;
		}

		fm.ClearOutBuf();
		fm.SetData(buf,obuf_len);
		delete[] buf;

		Crypt.SetCache(map_name,fm.GetOutBuf(),fm.GetOutBufLen());
		fm.ClearOutBuf();
	}
	else
	{
		WriteLog("Not for all data of map, disconnect.\n");
		NetState=STATE_DISCONNECT;
		SAFEDELA(tiles_data);
		SAFEDELA(walls_data);
		SAFEDELA(scen_data);
		return;
	}

	SAFEDELA(tiles_data);
	SAFEDELA(walls_data);
	SAFEDELA(scen_data);

	AutomapWaitPids.erase(map_pid);
	AutomapReceivedPids.insert(map_pid);

	WriteLog("Map saved.\n");
}

void FOClient::Net_OnGlobalInfo()
{
	DWORD msg_len;
	BYTE info_flags;
	Bin >> msg_len;
	Bin >> info_flags;

	if(FLAG(info_flags,GM_INFO_LOCATIONS))
	{
		GmapLoc.clear();
		WORD count_loc;
		Bin >> count_loc;

		for(int i=0;i<count_loc;i++)
		{
			GmapLocation loc;
			Bin >> loc.LocId;
			Bin >> loc.LocPid;
			Bin >> loc.LocWx;
			Bin >> loc.LocWy;
			Bin >> loc.Radius;
			Bin >> loc.Color;

			if(loc.LocId) GmapLoc.push_back(loc);
		}
	}

	if(FLAG(info_flags,GM_INFO_LOCATION))
	{
		GmapLocation loc;
		bool add;
		Bin >> add;
		Bin >> loc.LocId;
		Bin >> loc.LocPid;
		Bin >> loc.LocWx;
		Bin >> loc.LocWy;
		Bin >> loc.Radius;
		Bin >> loc.Color;

		GmapLocationVecIt it=std::find(GmapLoc.begin(),GmapLoc.end(),loc.LocId);
		if(add)
		{
			if(it!=GmapLoc.end()) *it=loc;
			else GmapLoc.push_back(loc);
		}
		else
		{
			if(it!=GmapLoc.end()) GmapLoc.erase(it);
		}
	}

	if(FLAG(info_flags,GM_INFO_GROUP_PARAM))
	{
		WORD group_x;
		WORD group_y;
		WORD move_x;
		WORD move_y;
		int speed_x;
		int speed_y;
		BYTE wait;
		Bin >> group_x;
		Bin >> group_y;
		Bin >> move_x;
		Bin >> move_y;
		Bin >> speed_x;
		Bin >> speed_y;
		Bin >> wait;

		GmapGroupXf=group_x;
		GmapGroupYf=group_y;
		GmapMoveLastTick=Timer::GameTick();
		GmapProcLastTick=Timer::GameTick();
		GmapGroupX=group_x;
		GmapGroupY=group_y;
		GmapWait=(wait!=0);

		if(GmapIsProc==false)
		{
			GmapMapScrX=(GmapWMap[2]-GmapWMap[0])/2+GmapWMap[0]-GmapGroupX;
			GmapMapScrY=(GmapWMap[3]-GmapWMap[1])/2+GmapWMap[1]-GmapGroupY;

			int w=GM_MAXX;
			int h=GM_MAXY;
			if(GmapMapScrX>GmapWMap[0]) GmapMapScrX=GmapWMap[0];
			if(GmapMapScrY>GmapWMap[1]) GmapMapScrY=GmapWMap[1];
			if(GmapMapScrX<GmapWMap[2]-w) GmapMapScrX=GmapWMap[2]-w;
			if(GmapMapScrY<GmapWMap[3]-h) GmapMapScrY=GmapWMap[3]-h;

			SetCurMode(CUR_DEFAULT);
		}

		GmapMoveX=move_x;
		GmapMoveY=move_y;
		GmapSpeedX=(float)(speed_x)/1000000;
		GmapSpeedY=(float)(speed_y)/1000000;

		int dist=DistSqrt(GmapMoveX,GmapMoveY,GmapGroupX,GmapGroupY);

		// Car master id
		Bin >> GmapCar.MasterId;
		//if(GmapCar.MasterId) SndMngr.PlayMusic(MsgGM->GetStr(STR_MAP_MUSIC(0)-1));

		GmapIsProc=true;
	}

	if(FLAG(info_flags,GM_INFO_ZONES_FOG))
	{
		Bin.Pop((char*)GmapFog.GetData(),GM_ZONES_FOG_SIZE);
	}

	if(FLAG(info_flags,GM_INFO_FOG))
	{
		WORD zx,zy;
		BYTE fog;
		Bin >> zx;
		Bin >> zy;
		Bin >> fog;
		GmapFog.Set2Bit(zx,zy,fog);
	}

	if(FLAG(info_flags,GM_INFO_CRITTERS))
	{
		ClearCritters();
		// After wait AddCritters
	}

	CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnGlobalEntrances()
{
	DWORD msg_len;
	DWORD loc_id;
	BYTE count;
	bool entrances[0x100];
	ZeroMemory(entrances,sizeof(entrances));
	Bin >> msg_len;
	Bin >> loc_id;
	Bin >> count;

	for(int i=0;i<count;i++)
	{
		BYTE e;
		Bin >> e;
		entrances[e]=true;
	}

	CHECK_IN_BUFF_ERROR;

	GmapShowEntrancesLocId=loc_id;
	memcpy(GmapShowEntrances,entrances,sizeof(entrances));
}

void FOClient::Net_OnContainerInfo()
{
	DWORD msg_len;
	BYTE transfer_type;
	DWORD talk_time;
	DWORD cont_id;
	WORD cont_pid; // Or Barter K
	DWORD items_count;
	Bin >> msg_len;
	Bin >> transfer_type;

	bool open_screen=(FLAG(transfer_type,0x80)?true:false);
	UNSETFLAG(transfer_type,0x80);

	if(transfer_type==TRANSFER_CLOSE)
	{
		if(IsScreenPresent(SCREEN__BARTER)) HideScreen(SCREEN__BARTER);
		if(IsScreenPresent(SCREEN__PICKUP)) HideScreen(SCREEN__PICKUP);
		return;
	}

	Bin >> talk_time;
	Bin >> cont_id;
	Bin >> cont_pid;
	Bin >> items_count;

	ItemVec item_container;
	for(DWORD i=0;i<items_count;i++)
	{
		DWORD item_id;
		WORD item_pid;
		Item::ItemData data;
		Bin >> item_id;
		Bin >> item_pid;
		Bin.Pop((char*)&data,sizeof(data));

		ProtoItem* proto_item=ItemMngr.GetProtoItem(item_pid);
		if(item_id && proto_item)
		{
			Item item;
			ZeroMemory(&item,sizeof(item));
			item.Init(proto_item);
			item.Id=item_id;
			item.Data=data;
			item_container.push_back(item);
		}
	}

	CHECK_IN_BUFF_ERROR;

	if(!Chosen) return;

	Item::SortItems(item_container);
	DlgEndTick=Timer::GameTick()+talk_time;
	PupContId=cont_id;
	PupContPid=0;

	if(transfer_type==TRANSFER_CRIT_BARTER)
	{
		PupTransferType=transfer_type;
		BarterK=cont_pid;
		BarterCount=items_count;
		BarterCont2Init.clear();
		BarterCont2Init=item_container;
		BarterScroll1o=0;
		BarterScroll2o=0;
		BarterCont1oInit.clear();
		BarterCont2oInit.clear();

		if(open_screen)
		{
			if(IsScreenPresent(SCREEN__DIALOG)) HideScreen(SCREEN__DIALOG);
			if(!IsScreenPresent(SCREEN__BARTER)) ShowScreen(SCREEN__BARTER);
			BarterScroll1=0;
			BarterScroll2=0;
			BarterText="";
			DlgMainTextCur=0;
			DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
		}
	}
	else
	{
		PupTransferType=transfer_type;
		PupContPid=(transfer_type==TRANSFER_HEX_CONT_UP || transfer_type==TRANSFER_HEX_CONT_DOWN || transfer_type==TRANSFER_FAR_CONT?cont_pid:0);
		PupCount=items_count;
		PupCont2Init.clear();
		PupCont2Init=item_container;
		PupScrollCrit=0;
		PupLastPutId=0;

		if(open_screen)
		{
			if(!IsScreenPresent(SCREEN__PICKUP)) ShowScreen(SCREEN__PICKUP);
			PupScroll1=0;
			PupScroll2=0;
		}

		if(PupTransferType==TRANSFER_CRIT_LOOT)
		{
			CritVec& loot=PupGetLootCrits();
			for(int i=0,j=loot.size();i<j;i++)
			{
				if(loot[i]->GetId()==PupContId)
				{
					PupScrollCrit=i;
					break;
				}
			}
		}
	}

	CollectContItems();
}

void FOClient::Net_OnFollow()
{
	DWORD rule;
	BYTE follow_type;
	WORD map_pid;
	DWORD wait_time;
	Bin >> rule;
	Bin >> follow_type;
	Bin >> map_pid;
	Bin >> wait_time;

	ShowScreen(SCREEN__DIALOGBOX);
	DlgboxType=DIALOGBOX_FOLLOW;
	FollowRuleId=rule;
	FollowType=follow_type;
	FollowMap=map_pid;
	DlgboxWait=Timer::GameTick()+wait_time;

	// Find rule
	char cr_name[64];
	CritterCl* cr=GetCritter(rule);
	StringCopy(cr_name,cr?cr->GetName():MsgGame->GetStr(STR_FOLLOW_UNKNOWN_CRNAME));
	// Find map
	char map_name[64];
	StringCopy(map_name,map_pid?"локальную карту":MsgGame->GetStr(STR_FOLLOW_GMNAME));

	switch(FollowType)
	{
	case FOLLOW_PREP:
		StringCopy(DlgboxText,FmtGameText(STR_FOLLOW_PREP,cr_name,map_name));
		break;
	case FOLLOW_FORCE:
		StringCopy(DlgboxText,FmtGameText(STR_FOLLOW_FORCE,cr_name,map_name));
		break;
	default:
		WriteLog(__FUNCTION__" - Error FollowType\n");
		StringCopy(DlgboxText,"ERROR!");
		break;
	}

	DlgboxButtonText[0]=MsgGame->GetStr(STR_DIALOGBOX_FOLLOW);
	DlgboxButtonText[1]=MsgGame->GetStr(STR_DIALOGBOX_CANCEL);
	DlgboxButtonsCount=2;
}

void FOClient::Net_OnPlayersBarter()
{
	BYTE barter;
	DWORD param;
	DWORD param_ext;
	Bin >> barter;
	Bin >> param;
	Bin >> param_ext;

	switch(barter)
	{
	case BARTER_BEGIN:
		{
			CritterCl* cr=GetCritter(param);
			if(!cr) break;
			BarterText=FmtGameText(STR_BARTER_BEGIN,cr->GetName(),
				!FLAG(param_ext,1)?MsgGame->GetStr(STR_BARTER_OPEN_MODE):MsgGame->GetStr(STR_BARTER_HIDE_MODE),
				!FLAG(param_ext,2)?MsgGame->GetStr(STR_BARTER_OPEN_MODE):MsgGame->GetStr(STR_BARTER_HIDE_MODE));
			BarterText+="\n";
			DlgMainTextCur=0;
			DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
			param_ext=(FLAG(param_ext,2)?true:false);
		}
	case BARTER_REFRESH:
		{
			CritterCl* cr=GetCritter(param);
			if(!cr) break;

			if(IsScreenPresent(SCREEN__DIALOG)) HideScreen(SCREEN__DIALOG);
			if(!IsScreenPresent(SCREEN__BARTER)) ShowScreen(SCREEN__BARTER);

			BarterIsPlayers=true;
			BarterOpponentId=param;
			BarterOpponentHide=(param_ext!=0);
			BarterOffer=false;
			BarterOpponentOffer=false;
			BarterCont2Init.clear();
			BarterCont1oInit.clear();
			BarterCont2oInit.clear();
			CollectContItems();
		}
		break;
	case BARTER_END:
		{
			if(IsScreenPlayersBarter()) ShowScreen(0);
		}
		break;
	case BARTER_TRY:
		{
			CritterCl* cr=GetCritter(param);
			if(!cr) break;
			ShowScreen(SCREEN__DIALOGBOX);
			DlgboxType=DIALOGBOX_BARTER;
			PBarterPlayerId=param;
			BarterOpponentHide=(param_ext!=0);
			PBarterHide=BarterOpponentHide;
			StringCopy(DlgboxText,FmtGameText(STR_BARTER_DIALOGBOX,cr->GetName(),!BarterOpponentHide?MsgGame->GetStr(STR_BARTER_OPEN_MODE):MsgGame->GetStr(STR_BARTER_HIDE_MODE)));
			DlgboxWait=Timer::GameTick()+20000;
			DlgboxButtonText[0]=MsgGame->GetStr(STR_DIALOGBOX_BARTER_OPEN);
			DlgboxButtonText[1]=MsgGame->GetStr(STR_DIALOGBOX_BARTER_HIDE);
			DlgboxButtonText[2]=MsgGame->GetStr(STR_DIALOGBOX_CANCEL);
			DlgboxButtonsCount=3;
		}
		break;
	case BARTER_SET_SELF:
	case BARTER_SET_OPPONENT:
		{
			if(!IsScreenPlayersBarter())
			{
				Net_SendPlayersBarter(BARTER_END,0,0);
				break;
			}

			BarterOffer=false;
			BarterOpponentOffer=false;
			bool is_hide=(barter==BARTER_SET_OPPONENT && BarterOpponentHide);
			ItemVec& cont=(barter==BARTER_SET_SELF?InvContInit:BarterCont2Init);
			ItemVec& cont_o=(barter==BARTER_SET_SELF?BarterCont1oInit:BarterCont2oInit);

			if(!is_hide)
			{
				ItemVecIt it=std::find(cont.begin(),cont.end(),param);
				if(it==cont.end() || param_ext>(*it).GetCount())
				{
					Net_SendPlayersBarter(BARTER_REFRESH,0,0);
					break;
				}
				Item& citem=*it;
				ItemVecIt it_=std::find(cont_o.begin(),cont_o.end(),param);
				if(it_==cont_o.end())
				{
					cont_o.push_back(citem);
					it_=cont_o.begin()+cont_o.size()-1;
					(*it_).Count_Set(0);
				}
				(*it_).Count_Add(param_ext);
				citem.Count_Sub(param_ext);
				if(!citem.GetCount() || !citem.IsGrouped()) cont.erase(it);
			}
			else // Hide
			{
				WriteLog(__FUNCTION__" - Invalid argument.\n");
				Net_SendPlayersBarter(BARTER_END,0,0);
				// See void FOClient::Net_OnPlayersBarterSetHide()
			}
			CollectContItems();
		}
		break;
	case BARTER_UNSET_SELF:
	case BARTER_UNSET_OPPONENT:
		{
			if(!IsScreenPlayersBarter())
			{
				Net_SendPlayersBarter(BARTER_END,0,0);
				break;
			}

			BarterOffer=false;
			BarterOpponentOffer=false;
			bool is_hide=(barter==BARTER_UNSET_OPPONENT && BarterOpponentHide);
			ItemVec& cont=(barter==BARTER_UNSET_SELF?InvContInit:BarterCont2Init);
			ItemVec& cont_o=(barter==BARTER_UNSET_SELF?BarterCont1oInit:BarterCont2oInit);

			if(!is_hide)
			{
				ItemVecIt it=std::find(cont_o.begin(),cont_o.end(),param);
				if(it==cont_o.end() || param_ext>(*it).GetCount())
				{
					Net_SendPlayersBarter(BARTER_REFRESH,0,0);
					break;
				}
				Item& citem=*it;
				ItemVecIt it_=std::find(cont.begin(),cont.end(),param);
				if(it_==cont.end())
				{
					cont.push_back(citem);
					it_=cont.begin()+cont.size()-1;
					(*it_).Count_Set(0);
				}
				(*it_).Count_Add(param_ext);
				citem.Count_Sub(param_ext);
				if(!citem.GetCount() || !citem.IsGrouped()) cont_o.erase(it);
			}
			else // Hide
			{
				Item* citem=GetContainerItem(cont_o,param);
				if(!citem || param_ext>citem->GetCount())
				{
					Net_SendPlayersBarter(BARTER_REFRESH,0,0);
					break;
				}
				citem->Count_Sub(param_ext);
				if(!citem->GetCount() || !citem->IsGrouped())
				{
					ItemVecIt it=std::find(cont_o.begin(),cont_o.end(),param);
					cont_o.erase(it);
				}
			}
			CollectContItems();
		}
		break;
	case BARTER_OFFER:
		{
			if(!IsScreenPlayersBarter())
			{
				Net_SendPlayersBarter(BARTER_END,0,0);
				break;
			}

			if(!param_ext) BarterOffer=(param!=0);
			else BarterOpponentOffer=(param!=0);

			if(BarterOpponentOffer && !BarterOffer)
			{
				CritterCl* cr=GetCritter(BarterOpponentId);
				if(!cr) break;
				BarterText+=FmtGameText(STR_BARTER_READY_OFFER,cr->GetName());
				BarterText+="\n";
				DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
			}
		}
		break;
	default:
		Net_SendPlayersBarter(BARTER_END,0,0);
		break;
	}
}

void FOClient::Net_OnPlayersBarterSetHide()
{
	DWORD id;
	WORD pid;
	DWORD count;
	Item::ItemData data;
	Bin >> id;
	Bin >> pid;
	Bin >> count;
	Bin.Pop((char*)&data,sizeof(data));

	if(!IsScreenPlayersBarter())
	{
		Net_SendPlayersBarter(BARTER_END,0,0);
		return;
	}

	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	if(!proto_item)
	{
		WriteLog(__FUNCTION__" - Proto target_item<%u> not found.\n",pid);
		Net_SendPlayersBarter(BARTER_REFRESH,0,0);
		return;
	}

	Item* citem=GetContainerItem(BarterCont2oInit,id);
	if(!citem)
	{
		Item item;
		ZeroMemory(&item,sizeof(item));
		item.Init(proto_item);
		item.Id=id;
		item.Count_Set(count);
		BarterCont2oInit.push_back(item);
	}
	else
	{
		citem->Count_Add(count);
		citem->Data=data;
	}
	CollectContItems();
}

void FOClient::Net_OnShowScreen()
{
	int screen_type;
	DWORD param;
	bool need_answer;
	Bin >> screen_type;
	Bin >> param;
	Bin >> need_answer;

// Close current
	switch(screen_type)
	{
	case SHOW_SCREEN_TIMER:
	case SHOW_SCREEN_DIALOGBOX:
	case SHOW_SCREEN_SKILLBOX:
	case SHOW_SCREEN_BAG:
	case SHOW_SCREEN_SAY:
		ShowScreen(SCREEN_NONE);
		break;
	default:
		ShowScreen(SCREEN_NONE);
		break;
	}

// Open new
	switch(screen_type)
	{
	case SHOW_SCREEN_TIMER:
		TimerStart(0,ResMngr.GetInvSprId(param),0);
		break;
	case SHOW_SCREEN_DIALOGBOX:
		ShowScreen(SCREEN__DIALOGBOX);
		DlgboxWait=0;
		DlgboxText[0]=0;
		DlgboxType=DIALOGBOX_MANUAL;
		if(param>=MAX_DLGBOX_BUTTONS) param=MAX_DLGBOX_BUTTONS-1;
		DlgboxButtonsCount=param+1;
		for(int i=0;i<DlgboxButtonsCount;i++) DlgboxButtonText[i]="";
		DlgboxButtonText[param]=MsgGame->GetStr(STR_DIALOGBOX_CANCEL);
		break;
	case SHOW_SCREEN_SKILLBOX:
		SboxUseOn.Clear();
		ShowScreen(SCREEN__SKILLBOX);
		break;
	case SHOW_SCREEN_BAG:
		UseSelect.Clear();
		ShowScreen(SCREEN__USE);
		break;
	case SHOW_SCREEN_SAY:
		ShowScreen(SCREEN__SAY);
		SayType=DIALOGSAY_TEXT;
		SayText[0]=0;
		if(param) SayOnlyNumbers=true;
		break;
	case SHOW_ELEVATOR:
		ElevatorGenerate(param);
		break;
	// Just open
	case SHOW_SCREEN_INVENTORY: ShowScreen(SCREEN__INVENTORY); return;
	case SHOW_SCREEN_CHARACTER: ShowScreen(SCREEN__CHARACTER); return;
	case SHOW_SCREEN_FIXBOY: ShowScreen(SCREEN__FIX_BOY); return;
	case SHOW_SCREEN_PIPBOY: ShowScreen(SCREEN__PIP_BOY); return;
	case SHOW_SCREEN_MINIMAP: ShowScreen(SCREEN__MINI_MAP); return;
	case SHOW_SCREEN_CLOSE: return;
	default: return;
	}

	ShowScreenType=screen_type;
	ShowScreenParam=param;
	ShowScreenNeedAnswer=need_answer;
}

void FOClient::Net_OnRunClientScript()
{
	char str[MAX_FOTEXT];
	DWORD msg_len;
	WORD func_name_len;
	CScriptString* func_name=new CScriptString();
	int p0,p1,p2;
	WORD p3len;
	CScriptString* p3=NULL;
	WORD p4size;
	asIScriptArray* p4=NULL;
	Bin >> msg_len;
	Bin >> func_name_len;
	if(func_name_len && func_name_len<MAX_FOTEXT)
	{
		Bin.Pop(str,func_name_len);
		str[func_name_len]=0;
		*func_name=str;
	}
	Bin >> p0;
	Bin >> p1;
	Bin >> p2;
	Bin >> p3len;
	if(p3len && p3len<MAX_FOTEXT)
	{
		Bin.Pop(str,p3len);
		str[p3len]=0;
		p3=new CScriptString(str);
	}
	Bin >> p4size;
	if(p4size)
	{
		p4=Script::CreateArray("int[]");
		if(p4)
		{
			p4->Resize(p4size);
			Bin.Pop((char*)p4->GetElementPointer(0),p4size*sizeof(DWORD));
		}
	}

	CHECK_IN_BUFF_ERROR;

	// Reparse module
	int bind_id;
	if(strstr(func_name->c_str(),"@"))
		bind_id=Script::Bind(func_name->c_str(),"void %s(int, int, int, string@, int[]@)",true);
	else
		bind_id=Script::Bind("client_main",func_name->c_str(),"void %s(int, int, int, string@, int[]@)",true);

	if(bind_id>0 && Script::PrepareContext(bind_id,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgDword(p0);
		Script::SetArgDword(p1);
		Script::SetArgDword(p2);
		Script::SetArgObject(p3);
		Script::SetArgObject(p4);
		Script::RunPrepared();
	}

	func_name->Release();
	if(p3) p3->Release();
	if(p4) p4->Release();
}

void FOClient::Net_OnDropTimers()
{
	ScoresNextUploadTick=0;
	FixNextShowCraftTick=0;
	GmapNextShowEntrancesTick=0;
	GmapShowEntrancesLocId=0;
}

void FOClient::Net_OnCritterLexems()
{
	DWORD msg_len;
	DWORD critter_id;
	WORD lexems_len;
	char lexems[LEXEMS_SIZE];
	Bin >> msg_len;
	Bin >> critter_id;
	Bin >> lexems_len;

	if(lexems_len+1>=LEXEMS_SIZE)
	{
		WriteLog(__FUNCTION__" - Invalid lexems length<%u>, disconnect.\n",lexems_len);
		NetState=STATE_DISCONNECT;
		return;
	}

	if(lexems_len) Bin.Pop(lexems,lexems_len);
	lexems[lexems_len]=0;

	CHECK_IN_BUFF_ERROR;

	CritterCl* cr=GetCritter(critter_id);
	if(cr)
	{
		cr->Lexems=lexems;
		const char* look=FmtCritLook(cr,CRITTER_ONLY_NAME);
		if(look) cr->Name=look;
	}
}

void FOClient::Net_OnItemLexems()
{
	DWORD msg_len;
	DWORD item_id;
	WORD lexems_len;
	char lexems[LEXEMS_SIZE];
	Bin >> msg_len;
	Bin >> item_id;
	Bin >> lexems_len;

	if(lexems_len+1>=LEXEMS_SIZE)
	{
		WriteLog(__FUNCTION__" - Invalid lexems length<%u>, disconnect.\n",lexems_len);
		NetState=STATE_DISCONNECT;
		return;
	}

	if(lexems_len) Bin.Pop(lexems,lexems_len);
	lexems[lexems_len]=0;

	CHECK_IN_BUFF_ERROR;

	// Find on map
	Item* item=GetItem(item_id);
	if(item) item->Lexems=lexems;
	// Find in inventory
	item=(Chosen?Chosen->GetItem(item_id):NULL);
	if(item) item->Lexems=lexems;
	// Find in containers
	UpdateContLexems(InvContInit,item_id,lexems);
	UpdateContLexems(BarterCont1oInit,item_id,lexems);
	UpdateContLexems(BarterCont2Init,item_id,lexems);
	UpdateContLexems(BarterCont2oInit,item_id,lexems);
	UpdateContLexems(PupCont2Init,item_id,lexems);
	UpdateContLexems(InvCont,item_id,lexems);
	UpdateContLexems(BarterCont1,item_id,lexems);
	UpdateContLexems(PupCont1,item_id,lexems);
	UpdateContLexems(UseCont,item_id,lexems);
	UpdateContLexems(BarterCont1o,item_id,lexems);
	UpdateContLexems(BarterCont2,item_id,lexems);
	UpdateContLexems(BarterCont2o,item_id,lexems);
	UpdateContLexems(PupCont2,item_id,lexems);

	// Some item
	if(SomeItem.GetId()==item_id) SomeItem.Lexems=lexems;
}

void FOClient::Net_OnCheckUID3()
{
	CHECKUIDBIN;
	if(CHECK_UID1(uid)) Net_SendPing(PING_UID_FAIL);
}

void FOClient::Net_OnMsgData()
{
	DWORD msg_len;
	DWORD lang;
	WORD num_msg;
	DWORD data_hash;
	CharVec data;
	Bin >> msg_len;
	Bin >> lang;
	Bin >> num_msg;
	Bin >> data_hash;
	data.resize(msg_len-(sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(lang)+sizeof(num_msg)+sizeof(data_hash)));
	Bin.Pop((char*)&data[0],data.size());

	CHECK_IN_BUFF_ERROR;

	if(lang!=CurLang.Name)
	{
		WriteLog(__FUNCTION__" - Received text in another language, set as default.\n");
		CurLang.Name=lang;
		WritePrivateProfileString(CLIENT_CONFIG_APP,"Language",CurLang.NameStr,".\\"CLIENT_CONFIG_FILE);
	}

	if(num_msg>=TEXTMSG_COUNT)
	{
		WriteLog(__FUNCTION__" - Incorrect value of msg num.\n");
		return;
	}

	if(data_hash!=Crypt.Crc32((BYTE*)&data[0],data.size()))
	{
		WriteLog(__FUNCTION__" - Invalid hash<%s>.\n",TextMsgFileName[num_msg]);
		return;	
	}

	if(CurLang.Msg[num_msg].LoadMsgStream(data)<0)
	{
		WriteLog(__FUNCTION__" - Unable to load<%s> from stream.\n",TextMsgFileName[num_msg]);
		return;
	}

	CurLang.Msg[num_msg].SaveMsgFile(Str::Format("%s\\%s",CurLang.NameStr,TextMsgFileName[num_msg]),PT_TEXTS);
	CurLang.Msg[num_msg].CalculateHash();

	switch(num_msg)
	{
	case TEXTMSG_ITEM:
		MrFixit.GenerateNames(*MsgGame,*MsgItem);
		break;
	case TEXTMSG_CRAFT:
		// Reload crafts
		MrFixit.Finish();
		MrFixit.LoadCrafts(*MsgCraft);
		MrFixit.GenerateNames(*MsgGame,*MsgItem);
		break;
	case TEXTMSG_INTERNAL:
		// Reload critter types
		CritType::InitFromMsg(MsgInternal);
		CritterCl::FreeAnimations(); // Free animations, maybe critters table is changed

		// Reload scripts
		if(!ReloadScripts()) NetState=STATE_DISCONNECT;

		// Names
		FONames::GenerateFoNames(PT_DATA);

		// Reload interface
		if(int res=InitIface())
		{
			WriteLog("Init interface fail, error<%d>.\n",res);
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_FAIL_TO_LOAD_IFACE));
			NetState=STATE_DISCONNECT;
		}

#pragma MESSAGE("Reboot client.")
		// Need restart client
		break;
	default:
		break;
	}
}

void FOClient::Net_OnProtoItemData()
{
	DWORD msg_len;
	BYTE type;
	DWORD data_hash;
	ProtoItemVec data;
	Bin >> msg_len;
	Bin >> type;
	Bin >> data_hash;
	data.resize((msg_len-sizeof(MSGTYPE)-sizeof(msg_len)-sizeof(type)-sizeof(data_hash))/sizeof(ProtoItem));
	Bin.Pop((char*)&data[0],data.size()*sizeof(ProtoItem));

	CHECK_IN_BUFF_ERROR;

	if(data_hash!=Crypt.Crc32((BYTE*)&data[0],data.size()*sizeof(ProtoItem)))
	{
		WriteLog(__FUNCTION__" - Hash error.\n");
		return;	
	}

	ItemMngr.ClearProtos(type);
	ItemMngr.ParseProtos(data);
	//ItemMngr.PrintProtosHash();

	ProtoItemVec proto_items;
	ItemMngr.GetAllProtos(proto_items);
	DWORD len=proto_items.size()*sizeof(ProtoItem);
	BYTE* proto_data=Crypt.Compress((BYTE*)&proto_items[0],len);
	if(!proto_data)
	{
		WriteLog(__FUNCTION__" - Compression fail.\n");
		return;
	}

	Crypt.SetCache("__item_protos",proto_data,len);
	delete[] proto_data;
}

void FOClient::Net_OnQuest(bool many)
{
	many?\
		WriteLog("Quests..."):
		WriteLog("Quest...");

	if(many)
	{
		DWORD msg_len;
		DWORD q_count;
		Bin >> msg_len;
		Bin >> q_count;

		QuestMngr.Clear();
		if(q_count)
		{
			DwordVec quests;
			quests.resize(q_count);
			Bin.Pop((char*)&quests[0],q_count*sizeof(DWORD));
			for(int i=0;i<quests.size();++i) QuestMngr.OnQuest(quests[i]);
		}
	}
	else
	{
		DWORD q_num;
		Bin >> q_num;
		QuestMngr.OnQuest(q_num);

		// Inform player
		Quest* quest=QuestMngr.GetQuest(q_num);
		if(quest) AddMess(FOMB_GAME,quest->str.c_str());
	}

	WriteLog("Complete.\n");
}

void FOClient::Net_OnHoloInfo()
{
	DWORD msg_len;
	bool clear;
	WORD offset;
	WORD count;
	Bin >> msg_len;
	Bin >> clear;
	Bin >> offset;
	Bin >> count;

	if(clear) ZeroMemory(HoloInfo,sizeof(HoloInfo));
	if(count) Bin.Pop((char*)&HoloInfo[offset],count*sizeof(DWORD));
}

void FOClient::Net_OnScores()
{
	ZeroMemory(BestScores,sizeof(BestScores));
	for(int i=0;i<SCORES_MAX;i++)
	{
		Bin.Pop(&BestScores[i][0],SCORE_NAME_LEN);
		BestScores[i][SCORE_NAME_LEN-1]='\0';
	}
}

void FOClient::Net_OnUserHoloStr()
{
	DWORD msg_len;
	DWORD str_num;
	WORD text_len;
	char text[USER_HOLO_MAX_LEN+1];
	Bin >> msg_len;
	Bin >> str_num;
	Bin >> text_len;

	if(text_len>USER_HOLO_MAX_LEN)
	{
		WriteLog(__FUNCTION__" - Text length greater than maximum, cur<%u>, max<%u>. Disconnect.\n",text_len,USER_HOLO_MAX_LEN);
		NetState=STATE_DISCONNECT;
		return;
	}

	Bin.Pop(text,text_len);
	text[text_len]='\0';

	CHECK_IN_BUFF_ERROR;

	if(MsgUserHolo->Count(str_num)) MsgUserHolo->EraseStr(str_num);
	MsgUserHolo->AddStr(str_num,text);
	MsgUserHolo->SaveMsgFile(USER_HOLO_TEXTMSG_FILE,PT_TEXTS);
}

void FOClient::Net_OnAutomapsInfo()
{
	DWORD msg_len;
	bool clear;
	WORD locs_count;
	Bin >> msg_len;
	Bin >> clear;

	if(clear)
	{
		Automaps.clear();
		AutomapWaitPids.clear();
		AutomapReceivedPids.clear();
		AutomapPoints.clear();
		AutomapCurMapPid=0;
		AutomapScrX=0.0f;
		AutomapScrY=0.0f;
		AutomapZoom=1.0f;
	}

	Bin >> locs_count;
	for(WORD i=0;i<locs_count;i++)
	{
		DWORD loc_id;
		WORD loc_pid;
		WORD maps_count;
		Bin >> loc_id;
		Bin >> loc_pid;
		Bin >> maps_count;

		AutomapVecIt it=std::find(Automaps.begin(),Automaps.end(),loc_id);

		// Delete from collection
		if(!maps_count)
		{
			if(it!=Automaps.end()) Automaps.erase(it);
		}
		// Add or modify
		else
		{
			Automap amap;
			amap.LocId=loc_id;
			amap.LocPid=loc_pid;
			amap.LocName=MsgGM->GetStr(STR_GM_NAME_(loc_pid));

			for(WORD j=0;j<maps_count;j++)
			{
				WORD map_pid;
				Bin >> map_pid;

				amap.MapPids.push_back(map_pid);
				amap.MapNames.push_back(MsgGM->GetStr(STR_MAP_NAME_(map_pid)));
			}

			if(it!=Automaps.end()) *it=amap;
			else Automaps.push_back(amap);
		}
	}

	CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnCheckUID4()
{
	CHECKUIDBIN;
	if(CHECK_UID2(uid)) Net_SendPing(PING_UID_FAIL);
}

void FOClient::Net_OnViewMap()
{
	WORD hx,hy;
	DWORD loc_id,loc_ent;
	Bin >> hx;
	Bin >> hy;
	Bin >> loc_id;
	Bin >> loc_ent;

	if(!HexMngr.IsMapLoaded()) return;

	HexMngr.FindSetCenter(hx,hy);
	SndMngr.PlayAmbient(MsgGM->GetStr(STR_MAP_AMBIENT_(HexMngr.GetCurPidMap())));
	ShowMainScreen(SCREEN_GAME);
	ScreenFadeOut();
	HexMngr.RebuildLight();
	ShowScreen(SCREEN__TOWN_VIEW);

	if(loc_id)
	{
		TViewType=TOWN_VIEW_FROM_GLOBAL;
		TViewGmapLocId=loc_id;
		TViewGmapLocEntrance=loc_ent;
	}
}

void FOClient::Net_OnCraftAsk()
{
	DWORD msg_len;
	WORD count;
	Bin >> msg_len;
	Bin >> count;

	FixShowCraft.clear();
	for(int i=0;i<count;i++)
	{
		DWORD craft_num;
		Bin >> craft_num;
		FixShowCraft.insert(craft_num);
	}

	CHECK_IN_BUFF_ERROR;

	if(IsScreenPresent(SCREEN__FIX_BOY) && FixMode==FIX_MODE_LIST) FixGenerate(FIX_MODE_LIST);
}

void FOClient::Net_OnCraftResult()
{
	BYTE craft_result;
	Bin >> craft_result;

	if(craft_result!=CRAFT_RESULT_NONE)
	{
		FixResult=craft_result;
		FixGenerate(FIX_MODE_RESULT);
	}
}

void FOClient::SetGameColor(DWORD color)
{
	SprMngr.SetSpritesColor(color);
	HexMngr.RefreshMap();
}

bool FOClient::GetMouseHex()
{
	TargetX=0;
	TargetY=0;

	if(IntVisible)
	{
		if(IsCurInRectNoTransp(IntMainPic,IntWMain,0,0)) return false;
		if(IntAddMess && IsCurInRectNoTransp(IntPWAddMess,IntWAddMess,0,0)) return false;
	}

	//if(ConsoleEdit && IsCurInRectNoTransp(ConsolePic,Main,0,0)) //TODO:
	return HexMngr.GetHexPixel(CurX,CurY,TargetX,TargetY);
}

bool FOClient::RegCheckData(CritterCl* newcr)
{
	// Name
	if(newcr->Name.length()<MIN_NAME || newcr->Name.length()<GameOpt.MinNameLength ||
		newcr->Name.length()>MAX_NAME || newcr->Name.length()>GameOpt.MaxNameLength)
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_WRONG_NAME));
		return false;
	}

	if(newcr->Name[0]==' ' || newcr->Name[newcr->Name.length()-1]==' ')
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_BEGIN_END_SPACES));
		return false;
	}

	for(int i=0,j=newcr->Name.length()-1;i<j;i++)
	{
		if(newcr->Name[i]==' ' && newcr->Name[i+1]==' ')
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_TWO_SPACE));
			return false;
		}
	}

	int letters_rus=0,letters_eng=0;
	for(int i=0,j=newcr->Name.length();i<j;i++)
	{
		if((newcr->Name[i]>='a' && newcr->Name[i]<='z') || (newcr->Name[i]>='A' && newcr->Name[i]<='Z')) letters_eng++;
		else if((newcr->Name[i]>='а' && newcr->Name[i]<='я') || (newcr->Name[i]>='А' && newcr->Name[i]<='Я')) letters_rus++;
	}

	if(letters_eng && letters_rus)
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_DIFFERENT_LANG));
		return false;
	}

	int letters_len=letters_eng+letters_rus;
	if(Procent(newcr->Name.length(),letters_len)<70)
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_MANY_SYMBOLS));
		return false;
	}

	if(!CheckUserName(newcr->Name.c_str()))
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_NAME_WRONG_CHARS));
		return false;
	}

	// Password
	if(!Singleplayer)
	{
		if(strlen(newcr->Pass)<MIN_NAME || strlen(newcr->Pass)<GameOpt.MinNameLength ||
			strlen(newcr->Pass)>MAX_NAME || strlen(newcr->Pass)>GameOpt.MaxNameLength)
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_WRONG_PASS));
			return false;
		}

		if(!CheckUserPass(newcr->Pass))
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_PASS_WRONG_CHARS));
			return false;
		}
	}

	if(Script::PrepareContext(ClientFunctions.PlayerGenerationCheck,CALL_FUNC_STR,"Registration"))
	{
		asIScriptArray* arr=Script::CreateArray("int[]");
		if(!arr) return false;

		arr->Resize(MAX_PARAMS);
		for(int i=0;i<MAX_PARAMS;i++) (*(int*)arr->GetElementPointer(i))=newcr->ParamsReg[i];
		bool result=false;
		Script::SetArgObject(arr);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();

		if(!result)
		{
			arr->Release();
			return false;
		}

		if(arr->GetElementCount()==MAX_PARAMS)
			for(int i=0;i<MAX_PARAMS;i++) newcr->ParamsReg[i]=(*(int*)arr->GetElementPointer(i));
		arr->Release();
	}

	return true;
}

void FOClient::SetDayTime(bool refresh)
{
	if(!HexMngr.IsMapLoaded()) return;

	static DWORD old_color=-1;
	DWORD color=GetColorDay(HexMngr.GetMapDayTime(),HexMngr.GetMapDayColor(),HexMngr.GetMapTime(),NULL);
	color=COLOR_GAME_RGB((color>>16)&0xFF,(color>>8)&0xFF,color&0xFF);

	if(refresh) old_color=-1;
	if(old_color!=color)
	{
		old_color=color;
		SetGameColor(old_color);
	}
}

Item* FOClient::GetTargetContItem()
{
	static Item inv_slot;
#define TRY_SEARCH_IN_CONT(cont) do{ItemVecIt it=std::find(cont.begin(),cont.end(),item_id); if(it!=cont.end()) return &(*it);}while(0)
#define TRY_SEARCH_IN_SLOT(target_item) do{if(target_item->GetId()==item_id){inv_slot=*target_item;return &inv_slot;}}while(0)

	if(!TargetSmth.IsContItem()) return NULL;
	DWORD item_id=TargetSmth.GetId();

	if(GetActiveScreen()!=SCREEN_NONE)
	{
		switch(GetActiveScreen())
		{
		case SCREEN__USE:
			TRY_SEARCH_IN_CONT(UseCont);
			break;
		case SCREEN__INVENTORY:
			TRY_SEARCH_IN_CONT(InvCont);
			if(Chosen)
			{
				Item* item=Chosen->GetItem(item_id);
				if(item) TRY_SEARCH_IN_SLOT(item);
			}
			break;
		case SCREEN__PICKUP:
			TRY_SEARCH_IN_CONT(PupCont1);
			TRY_SEARCH_IN_CONT(PupCont2);
			break;
		case SCREEN__BARTER:
			if(TargetSmth.GetParam()==0) TRY_SEARCH_IN_CONT(BarterCont1);
			if(TargetSmth.GetParam()==0) TRY_SEARCH_IN_CONT(BarterCont2);
			if(TargetSmth.GetParam()==1) TRY_SEARCH_IN_CONT(BarterCont1o);
			if(TargetSmth.GetParam()==1) TRY_SEARCH_IN_CONT(BarterCont2o);
			break;
		default:
			break;
		}
	}
	TargetSmth.Clear();
	return NULL;
}

void FOClient::AddAction(bool to_front, ActionEvent& act)
{
	for(ActionEventVecIt it=ChosenAction.begin();it!=ChosenAction.end();)
	{
		ActionEvent& a=*it;
		if(a==act) it=ChosenAction.erase(it);
		else ++it;
	}

	if(to_front) ChosenAction.insert(ChosenAction.begin(),act);
	else ChosenAction.push_back(act);
}

void FOClient::SetAction(DWORD type_action, DWORD param0, DWORD param1, DWORD param2, DWORD param3, DWORD param4, DWORD param5)
{
	SetAction(ActionEvent(type_action,param0,param1,param2,param3,param4,param5));
}

void FOClient::SetAction(ActionEvent& act)
{
	ChosenAction.clear();
	AddActionBack(act);
}

void FOClient::AddActionBack(DWORD type_action, DWORD param0, DWORD param1, DWORD param2, DWORD param3, DWORD param4, DWORD param5)
{
	AddActionBack(ActionEvent(type_action,param0,param1,param2,param3,param4,param5));
}

void FOClient::AddActionBack(ActionEvent& act)
{
	AddAction(false,act);
}

void FOClient::AddActionFront(DWORD type_action, DWORD param0, DWORD param1, DWORD param2, DWORD param3, DWORD param4, DWORD param5)
{
	AddAction(true,ActionEvent(type_action,param0,param1,param2,param3,param4,param5));
}

void FOClient::EraseFrontAction()
{
	if(ChosenAction.size()) ChosenAction.erase(ChosenAction.begin());
}

void FOClient::EraseBackAction()
{
	if(ChosenAction.size()) ChosenAction.pop_back();
}

bool FOClient::IsAction(DWORD type_action)
{
	return ChosenAction.size() && (*ChosenAction.begin()).Type==type_action;
}

void FOClient::ChosenChangeSlot()
{
	if(!Chosen) return;

	if(Chosen->ItemSlotMain->GetId())
		AddActionBack(CHOSEN_MOVE_ITEM,Chosen->ItemSlotMain->GetId(),Chosen->ItemSlotMain->GetCount(),SLOT_HAND2);
	else if(Chosen->ItemSlotExt->GetId())
		AddActionBack(CHOSEN_MOVE_ITEM,Chosen->ItemSlotExt->GetId(),Chosen->ItemSlotExt->GetCount(),SLOT_HAND1);
	else
	{
		BYTE tree=Chosen->DefItemSlotMain.Proto->Weapon.UnarmedTree+1;
		ProtoItem* unarmed=Chosen->GetUnarmedItem(tree,0);
		if(!unarmed) unarmed=Chosen->GetUnarmedItem(0,0);
		Chosen->DefItemSlotMain.Init(unarmed);
	}
}

void FOClient::WaitPing()
{
	SetCurMode(CUR_WAIT);
	Net_SendPing(PING_WAIT);
}

void FOClient::CrittersProcess()
{
/************************************************************************/
/* All critters                                                         */
/************************************************************************/
	for(CritMapIt it=HexMngr.allCritters.begin(),end=HexMngr.allCritters.end();it!=end;)
	{
		CritterCl* crit=(*it).second;
		++it;

		crit->Process();

		if(crit->IsNeedReSet())
		{
			HexMngr.RemoveCrit(crit);
			HexMngr.SetCrit(crit);
			crit->ReSetOk();
		}

		if(!crit->IsChosen() && crit->IsNeedMove() && HexMngr.TransitCritter(crit,crit->MoveSteps[0].first,crit->MoveSteps[0].second,true,crit->CurMoveStep>0))
		{
			crit->MoveSteps.erase(crit->MoveSteps.begin());
			crit->CurMoveStep--;
		}

		if(crit->IsFinish())
		{
			EraseCritter(crit->GetId());
			end=HexMngr.allCritters.end();
		}
	}

/************************************************************************/
/* Chosen                                                               */
/************************************************************************/
	if(!Chosen) return;

	if(IsMainScreen(SCREEN_GAME))
	{
		// Timeout mode
		if(!Chosen->GetTimeout(TO_BATTLE) && AnimGetCurSprCnt(IntWCombatAnim)>0)
		{
			AnimRun(IntWCombatAnim,ANIMRUN_FROM_END);
			if(AnimGetCurSprCnt(IntWCombatAnim)==AnimGetSprCount(IntWCombatAnim)-1) SndMngr.PlaySound(SND_COMBAT_MODE_OFF);
		}

		// Roof Visible
		static int last_hx=0;
		static int last_hy=0;

		if(last_hx!=Chosen->GetHexX() || last_hy!=Chosen->GetHexY())
		{
			last_hx=Chosen->GetHexX();
			last_hy=Chosen->GetHexY();
			HexMngr.SetSkipRoof(last_hx,last_hy);
		}

		// Hide Mode
		if(Chosen->IsPerk(MODE_HIDE))
			Chosen->Alpha=0x82;
		else
			Chosen->Alpha=0xFF;
	}

	// Actions
	if(!Chosen->IsFree()) return;

	// Game pause
	if(Timer::IsGamePaused()) return;

	// Ap regeneration
	if(Chosen->GetAp()<Chosen->GetParam(ST_ACTION_POINTS) && !IsTurnBased)
	{
		DWORD tick=Timer::GameTick();
		if(!Chosen->ApRegenerationTick) Chosen->ApRegenerationTick=tick;
		else
		{
			DWORD delta=tick-Chosen->ApRegenerationTick;
			if(delta>=500)
			{
				int max_ap=Chosen->GetParam(ST_ACTION_POINTS)*AP_DIVIDER;
				Chosen->Params[ST_CURRENT_AP]+=max_ap*delta/GameOpt.ApRegeneration;
				if(Chosen->Params[ST_CURRENT_AP]>max_ap) Chosen->Params[ST_CURRENT_AP]=max_ap;
				Chosen->ApRegenerationTick=tick;
			}
		}
	}
	if(Chosen->GetAp()>Chosen->GetParam(ST_ACTION_POINTS)) Chosen->Params[ST_CURRENT_AP]=Chosen->GetParam(ST_ACTION_POINTS)*AP_DIVIDER;

	if(ChosenAction.empty()) return;

	if(!Chosen->IsLife() || (IsTurnBased && !IsTurnBasedMyTurn()))
	{
		ChosenAction.clear();
		Chosen->AnimateStay();
		return;
	}

	ActionEvent act=ChosenAction[0];
#define CHECK_NEED_AP(need_ap) {if(IsTurnBased && !IsTurnBasedMyTurn()) break; if(Chosen->GetParam(ST_ACTION_POINTS)<(int)(need_ap)){AddMess(FOMB_GAME,FmtCombatText(STR_COMBAT_NEED_AP,need_ap)); break;} if(Chosen->GetAp()<(int)(need_ap)){if(IsTurnBased){if(Chosen->GetAp()) AddMess(FOMB_GAME,FmtCombatText(STR_COMBAT_NEED_AP,need_ap)); break;} else return;}}
#define CHECK_NEED_REAL_AP(need_ap) {if(IsTurnBased && !IsTurnBasedMyTurn()) break; if(Chosen->GetParam(ST_ACTION_POINTS)*AP_DIVIDER<(int)(need_ap)){AddMess(FOMB_GAME,FmtCombatText(STR_COMBAT_NEED_AP,(need_ap)/AP_DIVIDER)); break;} if(Chosen->GetRealAp()<(int)(need_ap)){if(IsTurnBased){if(Chosen->GetRealAp()) AddMess(FOMB_GAME,FmtCombatText(STR_COMBAT_NEED_AP,(need_ap)/AP_DIVIDER)); break;} else return;}}

	// Force end move
	if(act.Type!=CHOSEN_MOVE && act.Type!=CHOSEN_MOVE_TO_CRIT && MoveDirs.size())
	{
		MoveLastHx=-1;
		MoveLastHy=-1;
		MoveDirs.clear();
		Chosen->MoveSteps.clear();
		if(!Chosen->IsAnim()) Chosen->AnimateStay();
		Net_SendMove(MoveDirs);
	}

	switch(act.Type)
	{
	case CHOSEN_NONE:
		break;
	case CHOSEN_MOVE_TO_CRIT:
	case CHOSEN_MOVE:
		{
			WORD hx=act.Param[0];
			WORD hy=act.Param[1];
			bool is_run=(act.Param[2]&0xFF)!=0;
			bool wait_click=(act.Param[2]>>8)!=0;
			int cut=act.Param[3];
			DWORD start_tick=act.Param[4];

			if(!HexMngr.IsMapLoaded()) break;

			if(act.Type==CHOSEN_MOVE_TO_CRIT)
			{
				CritterCl* cr=GetCritter(act.Param[0]);
				if(!cr) goto label_EndMove;
				hx=cr->GetHexX();
				hy=cr->GetHexY();
			}

			WORD from_hx=Chosen->GetHexX();
			WORD from_hy=Chosen->GetHexY();

			if(Chosen->IsDoubleOverweight())
			{
				AddMess(FOMB_GAME,MsgGame->GetStr(STR_OVERWEIGHT_TITLE));
				SetAction(CHOSEN_NONE);
				goto label_EndMove;
			}

			if(CheckDist(from_hx,from_hy,hx,hy,cut)) goto label_EndMove;

			if(!GameOpt.RunOnCombat && Chosen->GetTimeout(TO_BATTLE)) is_run=false;
			else if(!GameOpt.RunOnTransfer && Chosen->GetTimeout(TO_TRANSFER)) is_run=false;
			else if(IsTurnBased) is_run=false;
			else if(Chosen->IsDmgLeg() || Chosen->IsOverweight()) is_run=false;
			else if(wait_click && Timer::GameTick()-start_tick<GameOpt.DoubleClickTime) return;
			else if(is_run && !IsTurnBased && Chosen->GetApCostCritterMove(is_run)>0 && Chosen->GetRealAp()<(GameOpt.RunModMul*Chosen->GetParam(ST_ACTION_POINTS)*AP_DIVIDER)/GameOpt.RunModDiv+GameOpt.RunModAdd) is_run=false;
			if(is_run && !CritType::IsCanRun(Chosen->GetCrType())) is_run=false;
			if(!is_run && !CritType::IsCanWalk(Chosen->GetCrType()))
			{
				AddMess(FOMB_GAME,MsgGame->GetStr(STR_CRITTER_CANT_MOVE));
				SetAction(CHOSEN_NONE);
				goto label_EndMove;
			}
			int ap_cost_real=Chosen->GetApCostCritterMove(is_run);
			if(!(IsTurnBasedMyTurn() && Chosen->GetAllAp()>=ap_cost_real/AP_DIVIDER)) CHECK_NEED_REAL_AP(ap_cost_real);
			Chosen->IsRunning=is_run;

			bool skip_find=false;
			if(hx==MoveLastHx && hy==MoveLastHy && MoveDirs.size())
			{
				WORD hx_=from_hx;
				WORD hy_=from_hy;
				MoveHexByDir(hx_,hy_,MoveDirs[0],HexMngr.GetMaxHexX(),HexMngr.GetMaxHexY());
				if(!HexMngr.GetField(hx_,hy_).IsNotPassed) skip_find=true;
			}

			// Find steps
			if(!skip_find)
			{
				//MoveDirs.resize(min(MoveDirs.size(),4)); // TODO:
				MoveDirs.clear();
				if(!IsTurnBased && MoveDirs.size())
				{
					WORD hx_=from_hx;
					WORD hy_=from_hy;
					MoveHexByDir(hx_,hy_,MoveDirs[0],HexMngr.GetMaxHexX(),HexMngr.GetMaxHexY());
					if(!HexMngr.GetField(hx_,hy_).IsNotPassed)
					{
						for(int i=1;i<MoveDirs.size() && i<4;i++) MoveHexByDir(hx_,hy_,MoveDirs[i],HexMngr.GetMaxHexX(),HexMngr.GetMaxHexY());
						from_hx=hx_;
						from_hy=hy_;
					}
					else MoveDirs.clear();
				}
				else MoveDirs.clear();

				WORD hx_=hx;
				WORD hy_=hy;
				bool result=HexMngr.CutPath(Chosen,from_hx,from_hy,hx_,hy_,cut);
				if(!result)
				{
					DWORD max_cut=DistGame(from_hx,from_hy,hx_,hy_);
					if(max_cut>cut+1) result=HexMngr.CutPath(Chosen,from_hx,from_hy,hx_,hy_,++cut);
					if(!result) goto label_EndMove;
				}

				ChosenAction[0].Param[3]=cut;
				ByteVec steps;
				if(HexMngr.FindPath(Chosen,from_hx,from_hy,hx_,hy_,steps,-1))
				{
					for(int i=0,j=steps.size();i<j;i++) MoveDirs.push_back(steps[i]);
				}
			}

label_EndMove:
			if(IsTurnBased && ap_cost_real>0 && MoveDirs.size()/(ap_cost_real/AP_DIVIDER)>Chosen->GetAp()+Chosen->GetParam(ST_MOVE_AP)) MoveDirs.resize(Chosen->GetAp()+Chosen->GetParam(ST_MOVE_AP));
			if(!IsTurnBased && ap_cost_real>0 && MoveDirs.size()>Chosen->GetRealAp()/ap_cost_real) MoveDirs.resize(Chosen->GetRealAp()/ap_cost_real);
			if(MoveDirs.size()>1 && Chosen->IsDmgTwoLeg()) MoveDirs.resize(1);

			// Transit
			if(MoveDirs.size())
			{
				WORD hx_=Chosen->GetHexX();
				WORD hy_=Chosen->GetHexY();
				MoveHexByDir(hx_,hy_,MoveDirs[0],HexMngr.GetMaxHexX(),HexMngr.GetMaxHexY());
				HexMngr.TransitCritter(Chosen,hx_,hy_,true,false);
				if(IsTurnBased)
				{
					int ap_cost=ap_cost_real/AP_DIVIDER;
					int move_ap=Chosen->GetParam(ST_MOVE_AP);
					if(move_ap)
					{
						if(ap_cost>move_ap)
						{
							Chosen->SubMoveAp(move_ap);
							Chosen->SubAp(ap_cost-move_ap);
						}
						else Chosen->SubMoveAp(ap_cost);
					}
					else
					{
						Chosen->SubAp(ap_cost);
					}
				}
				else if(Chosen->GetTimeout(TO_BATTLE))
				{
					Chosen->Params[ST_CURRENT_AP]-=ap_cost_real;
				}
				Chosen->ApRegenerationTick=0;
				HexMngr.SetCursorPos(CurX,CurY,Keyb::CtrlDwn,true);
				RebuildLookBorders=true;
			}

			// Send about move
			Net_SendMove(MoveDirs);
			if(MoveDirs.size()) MoveDirs.erase(MoveDirs.begin());

			// End move
			if(MoveDirs.empty())
			{
				MoveLastHx=-1;
				MoveLastHy=-1;
				Chosen->MoveSteps.clear();
				if(!Chosen->IsAnim()) Chosen->AnimateStay();
			}
			// Continue move
			else
			{
				MoveLastHx=hx;
				MoveLastHy=hy;
				Chosen->MoveSteps.resize(1);
				return;
			}
		}
		break;
	case CHOSEN_DIR:
		{
			int cw=act.Param[0];

			if(!HexMngr.IsMapLoaded() || (IsTurnBased && !IsTurnBasedMyTurn())) break;

			int dir=Chosen->GetDir();
			if(cw==0)
			{
				dir++;
				if(dir>5) dir=0;
			}
			else
			{
				dir--;
				if(dir<0) dir=5;
			}
			Chosen->SetDir(dir);
			Net_SendDir();
		}
		break;
	case CHOSEN_USE_ITEM:
		{
			DWORD item_id=act.Param[0];
			WORD item_pid=act.Param[1];
			BYTE target_type=act.Param[2];
			DWORD target_id=act.Param[3];
			BYTE rate=act.Param[4];
			DWORD param=act.Param[5];
			BYTE use=(rate&0xF);
			BYTE aim=(rate>>4);

			// Find item
			Item* item=(item_id?Chosen->GetItem(item_id):&Chosen->DefItemSlotMain);
			if(!item) break;
			bool is_main_item=(item==Chosen->ItemSlotMain);

			// Check proto item
			if(!item_pid) item_pid=item->GetProtoId();
			ProtoItem* proto_item=ItemMngr.GetProtoItem(item_pid);
			if(!proto_item) break;
			if(item->GetProtoId()!=item_pid) // Unarmed proto
			{
				static Item temp_item;
				static ProtoItem temp_proto_item;
				temp_item=*item;
				temp_proto_item=*proto_item;
				item=&temp_item;
				proto_item=&temp_proto_item;
				item->Init(proto_item);
			}

			// Calculate ap cost
			int ap_cost=Chosen->GetUseApCost(item,rate);

			// Find target
			CritterCl* target_cr=NULL;
			ItemHex* target_item=NULL;
			Item* target_item_self=NULL;
			if(target_type==TARGET_SELF) target_cr=Chosen;
			else if(target_type==TARGET_SELF_ITEM) target_item_self=Chosen->GetItem(target_id);
			else if(target_type==TARGET_CRITTER) target_cr=GetCritter(target_id);
			else if(target_type==TARGET_ITEM) target_item=GetItem(target_id);
			else if(target_type==TARGET_SCENERY) target_item=GetItem(target_id);
			else break;
			if(target_type==TARGET_CRITTER && Chosen==target_cr) target_type=TARGET_SELF;
			if(target_type==TARGET_SELF) target_id=Chosen->GetId();

			// Check
			if(target_type==TARGET_CRITTER && !target_cr) break;
			else if(target_type==TARGET_ITEM && !target_item) break;
			else if(target_type==TARGET_SCENERY && !target_item) break;
			if(target_type!=TARGET_CRITTER && !item->GetId()) break;

			// Parse use
			bool is_attack=(target_type==TARGET_CRITTER && is_main_item && item->IsWeapon() && use<MAX_USES);
			bool is_reload=(target_type==TARGET_SELF_ITEM && use==USE_RELOAD && item->IsWeapon());
			bool is_self=(target_type==TARGET_SELF || target_type==TARGET_SELF_ITEM);
			if(!is_attack && !is_reload && (IsCurMode(CUR_USE_ITEM) || IsCurMode(CUR_USE_WEAPON))) SetCurMode(CUR_DEFAULT);

			// Check weapon
			if(is_attack)
			{
				if(!HexMngr.IsMapLoaded()) break;
				if(is_self) break;
				if(!item->IsWeapon()) break;
				if(target_cr->IsDead()) break;
				if(!Chosen->IsPerk(MODE_UNLIMITED_AMMO) && item->WeapGetMaxAmmoCount() && item->WeapIsEmpty())
				{
					SndMngr.PlaySoundType('W',SOUND_WEAPON_EMPTY,item->Proto->Weapon.SoundId[use],'1');
					break;
				}
				if(item->IsTwoHands() && Chosen->IsDmgArm())
				{
					AddMess(FOMB_GAME,FmtCombatText(STR_COMBAT_NEED_DMG_ARM));
					break;
				}
				if(Chosen->IsDmgTwoArm() && item->GetId())
				{
					AddMess(FOMB_GAME,FmtCombatText(STR_COMBAT_NEED_DMG_TWO_ARMS));
					break;
				}
				if(item->IsWeared() && item->IsBroken())
				{
					AddMess(FOMB_GAME,FmtGameText(STR_INV_WEAR_WEAPON_BROKEN));
					break;
				}
			}
			else if(is_reload)
			{
				if(!is_self) break;

				CHECK_NEED_AP(ap_cost);

				if(is_main_item)
				{
					item->SetRate(USE_PRIMARY);
					Net_SendRateItem();
					if(GetActiveScreen()==SCREEN_NONE) SetCurMode(CUR_USE_WEAPON);
				}

				if(!item->WeapGetMaxAmmoCount()) break; // No have holder

				if(target_id==-1) // Unload
				{
					if(item->WeapIsEmpty()) break; // Is empty
					target_id=0;
				}
				else if(!target_item_self) // Reload
				{
					if(item->WeapIsFull()) break; // Is full
					if(item->WeapGetAmmoCaliber())
					{
						target_item_self=Chosen->GetAmmoAvialble(item);
						if(!target_item_self) break;
						target_id=target_item_self->GetId();
					}
				}
				else // Load
				{
					if(item->WeapGetAmmoCaliber()!=target_item_self->AmmoGetCaliber()) break; // Different caliber
					if(item->WeapGetAmmoPid()==target_item_self->GetProtoId() && item->WeapIsFull()) break; // Is full
				}
			}
			else // Use
			{
				if(use!=USE_USE) break;
			}

			// Find Target
			if(!is_self && HexMngr.IsMapLoaded())
			{
				WORD hx,hy;
				if(target_cr)
				{
					hx=target_cr->GetHexX();
					hy=target_cr->GetHexY();
				}
				else
				{
					hx=target_item->GetHexX();
					hy=target_item->GetHexY();
				}

				DWORD max_dist=(is_attack?Chosen->GetAttackDist():Chosen->GetUseDist())+(target_cr?target_cr->GetMultihex():0);

				// Target find
				bool need_move=false;
				if(is_attack) need_move=!HexMngr.TraceBullet(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,max_dist,0.0f,target_cr,false,NULL,0,NULL,NULL,NULL,true);
				else need_move=!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,max_dist);
				if(need_move) 
				{
					// If target too far, then move to it	
					if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,max_dist))
					{
						if(IsTurnBased) break;
						if(target_cr) SetAction(CHOSEN_MOVE_TO_CRIT,target_cr->GetId(),0,0/*walk*/,max_dist);
						else SetAction(CHOSEN_MOVE,hx,hy,0/*walk*/,max_dist,0);
						if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,max_dist)) AddActionBack(act);
						return;
					}
					AddMess(FOMB_GAME,MsgGame->GetStr(STR_FINDPATH_AIMBLOCK));
					break;
				}

				// Refresh orientation
				CHECK_NEED_AP(ap_cost);
				BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy);
				if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy)>=1 && Chosen->GetDir()!=dir)
				{
					Chosen->SetDir(dir);
					Net_SendDir();
				}
			}

			// Use
			CHECK_NEED_AP(ap_cost);

			if(target_item && target_item->IsScenOrGrid())
				Net_SendUseItem(ap_cost,item_id,item_pid,rate,target_type,(target_item->GetHexX()<<16)|(target_item->GetHexY()&0xFFFF),target_item->GetProtoId(),param);
			else
				Net_SendUseItem(ap_cost,item_id,item_pid,rate,target_type,target_id,0,param); // Item or critter

			if(use>=USE_PRIMARY && use<=USE_THIRD) Chosen->Action(ACTION_USE_WEAPON,rate,item);
			else if(use==USE_RELOAD) Chosen->Action(ACTION_RELOAD_WEAPON,0,item);
			else Chosen->Action(ACTION_USE_ITEM,0,item);

			Chosen->SubAp(ap_cost);
			//WaitPing(); TODO: need???

			if(is_attack && !aim && Keyb::ShiftDwn) // Continue battle after attack
			{
				AddActionBack(act);
				return;
			}
		}
		break;
	case CHOSEN_MOVE_ITEM:
		{
			DWORD item_id=act.Param[0];
			DWORD item_count=act.Param[1];
			int to_slot=act.Param[2];
			bool is_barter_cont=(act.Param[3]!=0);

			Item* item=Chosen->GetItem(item_id);
			if(!item) break;

			BYTE from_slot=item->ACC_CRITTER.Slot;
			if(from_slot==to_slot) break;
			bool is_castling=(from_slot==SLOT_HAND1 && to_slot==SLOT_HAND2) || (from_slot==SLOT_HAND2 && to_slot==SLOT_HAND1);
			bool is_light=item->IsLight();

			if(to_slot!=SLOT_GROUND)
			{
				if(!CritterCl::SlotEnabled[to_slot]) break;
				if(to_slot>SLOT_ARMOR && to_slot!=item->Proto->Slot) break;
				if(to_slot==SLOT_HAND1 && item->IsWeapon() && !CritType::IsAnim1(Chosen->GetCrType(),item->Proto->Weapon.Anim1)) break;
				if(to_slot==SLOT_ARMOR && (!item->IsArmor() || item->Proto->Slot || !CritType::IsCanArmor(Chosen->GetCrType()))) break;
			}

			DWORD ap_cost=(is_castling?0:Chosen->GetApCostMoveItemInventory());
			if(to_slot==SLOT_GROUND) ap_cost=Chosen->GetApCostDropItem();
			CHECK_NEED_AP(ap_cost);

			Item* to_slot_item=Chosen->GetItemSlot(to_slot);
			if(!is_castling && to_slot!=SLOT_INV && to_slot!=SLOT_GROUND && to_slot_item)
			{
				AddActionFront(CHOSEN_MOVE_ITEM,to_slot_item->GetId(),to_slot_item->GetCount(),SLOT_INV);
			}
			else
			{
				if(!Chosen->MoveItem(item_id,to_slot,item_count)) break;

				if(to_slot==SLOT_GROUND && is_barter_cont && IsScreenPresent(SCREEN__BARTER))
				{
					ItemVecIt it=std::find(BarterCont1oInit.begin(),BarterCont1oInit.end(),item_id);
					if(it!=BarterCont1oInit.end())
					{
						Item& item_=*it;
						if(item_count>=item_.GetCount()) BarterCont1oInit.erase(it);
						else item_.Count_Sub(item_count);
					}
					CollectContItems();
				}

				if(to_slot==SLOT_HAND1 || from_slot==SLOT_HAND1) RebuildLookBorders=true;
				if(is_light && (to_slot==SLOT_INV || (from_slot==SLOT_INV && to_slot!=SLOT_GROUND))) HexMngr.RebuildLight();

				Net_SendChangeItem(ap_cost,item_id,from_slot,to_slot,item_count);
				Chosen->SubAp(ap_cost);
				break;
			}
			return;
		}
		break;
	case CHOSEN_MOVE_ITEM_CONT:
		{
			DWORD item_id=act.Param[0];
			DWORD cont=act.Param[1];
			DWORD count=act.Param[2];

			CHECK_NEED_AP(Chosen->GetApCostMoveItemContainer());

			Item* item=GetContainerItem(cont==IFACE_PUP_CONT1?InvContInit:PupCont2Init,item_id);
			if(!item) break;
			if(count>item->GetCount()) break;

			if(cont==IFACE_PUP_CONT2)
			{
				if(Chosen->GetFreeWeight()<(int)(item->GetWeight1st()*count))
				{
					AddMess(FOMB_GAME,MsgGame->GetStr(STR_OVERWEIGHT));
					break;
				}
				if(Chosen->GetFreeVolume()<(int)(item->GetVolume1st()*count))
				{
					AddMess(FOMB_GAME,MsgGame->GetStr(STR_OVERVOLUME));
					break;
				}
			}

			Chosen->Action(ACTION_OPERATE_CONTAINER,PupTransferType*10+(cont==IFACE_PUP_CONT2?0:2),item);
			PupTransfer(item_id,cont,count);
			Chosen->SubAp(Chosen->GetApCostMoveItemContainer());
		}
		break;
	case CHOSEN_TAKE_ALL:
		{
			CHECK_NEED_AP(Chosen->GetApCostMoveItemContainer());

			if(PupCont2Init.empty()) break;

			DWORD c,w,v;
			ContainerCalcInfo(PupCont2Init,c,w,v,MAX_INT,false);
			if(Chosen->GetFreeWeight()<(int)w) AddMess(FOMB_GAME,MsgGame->GetStr(STR_BARTER_OVERWEIGHT));
			else if(Chosen->GetFreeVolume()<(int)v) AddMess(FOMB_GAME,MsgGame->GetStr(STR_BARTER_OVERSIZE));
			else
			{
				PupCont2Init.clear();
				PupCount=0;
				Net_SendItemCont(PupTransferType,PupContId,PupHoldId,0,CONT_GETALL);
				Chosen->Action(ACTION_OPERATE_CONTAINER,PupTransferType*10+1,NULL);
				Chosen->SubAp(Chosen->GetApCostMoveItemContainer());
				CollectContItems();
			}
		}
		break;
	case CHOSEN_USE_SKL_ON_CRITTER:
		{
			WORD skill=act.Param[0];
			DWORD crid=act.Param[1];

			CritterCl* cr=(crid?GetCritter(crid):Chosen);
			if(!cr) break;

			if(skill==SK_STEAL && cr->IsPerk(MODE_NO_STEAL)) break;

			if(cr!=Chosen && HexMngr.IsMapLoaded())
			{
				// Check dist 1
				switch(skill)
				{
				case SK_LOCKPICK:
				case SK_STEAL:
				case SK_TRAPS:
				case SK_FIRST_AID:
				case SK_DOCTOR:
				case SK_SCIENCE:
				case SK_REPAIR:
					if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),Chosen->GetUseDist()+cr->GetMultihex()))
					{
						if(IsTurnBased) break;
						DWORD dist=Chosen->GetUseDist()+cr->GetMultihex();
						SetAction(CHOSEN_MOVE_TO_CRIT,cr->GetId(),0,0/*walk*/,dist);
						WORD hx=cr->GetHexX(),hy=cr->GetHexY();
						if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,dist)) AddActionBack(act);
						return;
					}
					break;
				default:
					break;
				}

				if(cr!=Chosen)
				{
					// Refresh orientation
					CHECK_NEED_AP(Chosen->GetApCostUseSkill());
					BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY());
					if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY())>=1 && Chosen->GetDir()!=dir)
					{
						Chosen->SetDir(dir);
						Net_SendDir();
					}
				}
			}

			CHECK_NEED_AP(Chosen->GetApCostUseSkill());
			Chosen->Action(ACTION_USE_SKILL,skill-(GameOpt.AbsoluteOffsets?0:SKILL_BEGIN),NULL);
			Net_SendUseSkill(skill,cr);
			Chosen->SubAp(Chosen->GetApCostUseSkill());
			WaitPing();
		}
		break;
	case CHOSEN_USE_SKL_ON_ITEM:
		{
			bool is_inv=act.Param[0]!=0;
			WORD skill=act.Param[1];
			DWORD item_id=act.Param[2];

			Item* item_action;
			if(is_inv)
			{
				Item* item=Chosen->GetItem(item_id);
				if(!item) break;
				item_action=item;

				if(skill==SK_SCIENCE && item->IsHolodisk())
				{
					ShowScreen(SCREEN__INPUT_BOX);
					IboxMode=IBOX_MODE_HOLO;
					IboxHolodiskId=item->GetId();
					IboxTitle=GetHoloText(STR_HOLO_INFO_NAME_(item->HolodiskGetNum()));
					IboxText=GetHoloText(STR_HOLO_INFO_DESC_(item->HolodiskGetNum()));
					if(IboxTitle.length()>USER_HOLO_MAX_TITLE_LEN) IboxTitle.resize(USER_HOLO_MAX_TITLE_LEN);
					if(IboxText.length()>USER_HOLO_MAX_LEN) IboxText.resize(USER_HOLO_MAX_LEN);
					break;
				}

				CHECK_NEED_AP(Chosen->GetApCostUseSkill());
				Net_SendUseSkill(skill,item);
			}
			else
			{
				ItemHex* item=HexMngr.GetItemById(item_id);
				if(!item) break;
				item_action=item;

				if(HexMngr.IsMapLoaded())
				{
					if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),item->GetHexX(),item->GetHexY(),Chosen->GetUseDist()))
					{
						if(IsTurnBased) break;
						SetAction(CHOSEN_MOVE,item->GetHexX(),item->GetHexY(),0/*walk*/,Chosen->GetUseDist(),0);
						WORD hx=item->GetHexX(),hy=item->GetHexY();
						if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,Chosen->GetUseDist())) AddActionBack(act);
						return;
					}

					// Refresh orientation
					CHECK_NEED_AP(Chosen->GetApCostUseSkill());
					BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),item->GetHexX(),item->GetHexY());
					if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),item->GetHexX(),item->GetHexY())>=1 && Chosen->GetDir()!=dir)
					{
						Chosen->SetDir(dir);
						Net_SendDir();
					}
				}

				CHECK_NEED_AP(Chosen->GetApCostUseSkill());
				Net_SendUseSkill(skill,item);
			}

			Chosen->Action(ACTION_USE_SKILL,skill-(GameOpt.AbsoluteOffsets?0:SKILL_BEGIN),item_action);
			Chosen->SubAp(Chosen->GetApCostUseSkill());
			WaitPing();
		}
		break;
	case CHOSEN_USE_SKL_ON_SCEN:
		{
			WORD skill=act.Param[0];
			WORD pid=act.Param[1];
			WORD hx=act.Param[2];
			WORD hy=act.Param[3];

			if(!HexMngr.IsMapLoaded()) break;

			ItemHex* item=HexMngr.GetItem(hx,hy,pid);
			if(!item) break;

			if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,Chosen->GetUseDist()))
			{
				if(IsTurnBased) break;
				SetAction(CHOSEN_MOVE,hx,hy,0/*walk*/,Chosen->GetUseDist(),0);
				if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,Chosen->GetUseDist())) AddActionBack(act);
				return;
			}

			CHECK_NEED_AP(Chosen->GetApCostUseSkill());

			// Refresh orientation
			BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),item->GetHexX(),item->GetHexY());
			if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),item->GetHexX(),item->GetHexY())>=1 && Chosen->GetDir()!=dir)
			{
				Chosen->SetDir(dir);
				Net_SendDir();
			}

			Chosen->Action(ACTION_USE_SKILL,skill-(GameOpt.AbsoluteOffsets?0:SKILL_BEGIN),item);
			Net_SendUseSkill(skill,item);
			Chosen->SubAp(Chosen->GetApCostUseSkill());
			//WaitPing();
		}
		break;
	case CHOSEN_TALK_NPC:
		{
			DWORD crid=act.Param[0];

			if(!HexMngr.IsMapLoaded()) break;

			CritterCl* cr=GetCritter(crid);
			if(!cr) break;

			if(IsTurnBased) break;

			if(cr->IsDead())
			{
				//TODO: AddMess Dead CritterCl
				break;
			}

			DWORD talk_distance=cr->GetTalkDistance()+Chosen->GetMultihex();
			if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),talk_distance))
			{
				if(IsTurnBased) break;
				SetAction(CHOSEN_MOVE_TO_CRIT,cr->GetId(),0,0/*walk*/,talk_distance);
				WORD hx=cr->GetHexX(),hy=cr->GetHexY();
				if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,talk_distance)) AddActionBack(act);
				return;
			}

			if(!HexMngr.TraceBullet(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),talk_distance+Chosen->GetMultihex(),0.0f,cr,false,NULL,0,NULL,NULL,NULL,true))
			{
				AddMess(FOMB_GAME,MsgGame->GetStr(STR_FINDPATH_AIMBLOCK));
				break;
			}

			// Refresh orientation
			BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY());
			if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY())>=1 && Chosen->GetDir()!=dir)
			{
				Chosen->SetDir(dir);
				Net_SendDir();
			}

			Net_SendTalk(true,cr->GetId(),ANSWER_BEGIN);
			WaitPing();
		}
		break;
	case CHOSEN_PICK_ITEM:
		{
			WORD pid=act.Param[0];
			WORD hx=act.Param[1];
			WORD hy=act.Param[2];

			if(!HexMngr.IsMapLoaded()) break;

			ItemHex* item=HexMngr.GetItem(hx,hy,pid);
			if(!item) break;

			if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,Chosen->GetUseDist()))
			{
				if(IsTurnBased) break;
				SetAction(CHOSEN_MOVE,hx,hy,0/*walk*/,Chosen->GetUseDist(),0);
				if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,Chosen->GetUseDist())) AddActionBack(act);
				AddActionBack(act);
				return;
			}

			CHECK_NEED_AP(Chosen->GetApCostPickItem());

			// Refresh orientation
			BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy);
			if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),hx,hy)>=1 && Chosen->GetDir()!=dir)
			{
				Chosen->SetDir(dir);
				Net_SendDir();
			}

			Net_SendPickItem(hx,hy,pid);
			Chosen->Action(ACTION_PICK_ITEM,0,item);
			Chosen->SubAp(Chosen->GetApCostPickItem());
			//WaitPing();
		}
		break;
	case CHOSEN_PICK_CRIT:
		{
			DWORD crid=act.Param[0];
			bool is_loot=(act.Param[1]==0);

			if(!HexMngr.IsMapLoaded()) break;

			CritterCl* cr=GetCritter(crid);
			if(!cr) break;

			if(is_loot && (!cr->IsDead() || cr->IsPerk(MODE_NO_LOOT))) break;
			if(!is_loot && (!cr->IsLife() || cr->IsPerk(MODE_NO_PUSH))) break;

			DWORD dist=Chosen->GetUseDist()+cr->GetMultihex();
			if(!CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),dist))
			{
				if(IsTurnBased) break;
				SetAction(CHOSEN_MOVE_TO_CRIT,cr->GetId(),0,0/*walk*/,dist);
				WORD hx=cr->GetHexX(),hy=cr->GetHexY();
				if(HexMngr.CutPath(Chosen,Chosen->GetHexX(),Chosen->GetHexY(),hx,hy,dist)) AddActionBack(act);
				return;
			}

			CHECK_NEED_AP(Chosen->GetApCostPickCritter());

			// Refresh orientation
			BYTE dir=GetFarDir(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY());
			if(DistGame(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY())>=1 && Chosen->GetDir()!=dir)
			{
				Chosen->SetDir(dir);
				Net_SendDir();
			}

			if(is_loot)
			{
				Net_SendPickCritter(cr->GetId(),PICK_CRIT_LOOT);
				Chosen->Action(ACTION_PICK_CRITTER,0,NULL);
			}
			else if(cr->IsLifeNone())
			{
				Net_SendPickCritter(cr->GetId(),PICK_CRIT_PUSH);
				Chosen->Action(ACTION_PICK_CRITTER,2,NULL);
			}
			Chosen->SubAp(Chosen->GetApCostPickCritter());
			WaitPing();
		}
		break;
	case CHOSEN_WRITE_HOLO:
		{
			DWORD holodisk_id=act.Param[0];
			if(holodisk_id!=IboxHolodiskId) break;
			Item* holo=Chosen->GetItem(IboxHolodiskId);
			if(!holo->IsHolodisk()) break;
			const char* old_title=GetHoloText(STR_HOLO_INFO_NAME_(holo->HolodiskGetNum()));
			const char* old_text=GetHoloText(STR_HOLO_INFO_DESC_(holo->HolodiskGetNum()));
			if(holo && IboxTitle.length() && IboxText.length() && (IboxTitle!=old_title || IboxText!=old_text))
			{
				Net_SendSetUserHoloStr(holo,IboxTitle.c_str(),IboxText.c_str());
				Chosen->Action(ACTION_USE_ITEM,0,holo);
			}
		}
		break;
	}

	EraseFrontAction();
}

void FOClient::TryPickItemOnGround()
{
	if(!Chosen) return;
	ItemHexVec items;
	HexMngr.GetItems(Chosen->GetHexX(),Chosen->GetHexY(),items);
	for(ItemHexVecIt it=items.begin();it!=items.end();)
	{
		ItemHex* item=*it;
		if(item->IsFinishing() || item->Proto->IsDoor() || !item->IsUsable()) it=items.erase(it);
		else ++it;
	}
	if(items.empty()) return;
	ItemHex* item=items[Random(0,items.size()-1)];
	SetAction(CHOSEN_PICK_ITEM,item->GetProtoId(),item->HexX,item->HexY);
}

void FOClient::TryExit()
{
	int active=GetActiveScreen();
	if(active!=SCREEN_NONE)
	{
		switch(active)
		{
		case SCREEN__TIMER: TimerClose(false); break;
		case SCREEN__SPLIT: SplitClose(false); break;
		case SCREEN__TOWN_VIEW: Net_SendRefereshMe(); break;
		case SCREEN__DIALOG: break; // Processed in DlgKeyDown
		case SCREEN__BARTER: break; // Processed in DlgKeyDown
		case SCREEN__DIALOGBOX:
			if(DlgboxType>=DIALOGBOX_ENCOUNTER_ANY && DlgboxType<=DIALOGBOX_ENCOUNTER_TB) Net_SendRuleGlobal(GM_CMD_ANSWER,-1);
		case SCREEN__PERK:
		case SCREEN__ELEVATOR:
		case SCREEN__SAY:
		case SCREEN__SKILLBOX:
		case SCREEN__USE:
		case SCREEN__AIM:
		case SCREEN__INVENTORY:
		case SCREEN__PICKUP:
		case SCREEN__MINI_MAP:
		case SCREEN__CHARACTER:
		case SCREEN__PIP_BOY:
		case SCREEN__FIX_BOY:	
		case SCREEN__MENU_OPTION:
		case SCREEN__SAVE_LOAD:
		default:
			ShowScreen(SCREEN_NONE);
			break;
		}
	}
	else
	{
		switch(GetMainScreen())
		{
		case SCREEN_LOGIN:
			GameOpt.Quit=true;
			break;
		case SCREEN_REGISTRATION:
		case SCREEN_CREDITS:
			ShowMainScreen(SCREEN_LOGIN);
			break;
		case SCREEN_WAIT:
			NetDisconnect();
			break;
		case SCREEN_GLOBAL_MAP:
			ShowScreen(SCREEN__MENU_OPTION);
			break;
		case SCREEN_GAME:
			ShowScreen(SCREEN__MENU_OPTION);
			break;
		default:
			break;
		}
	}
}

void FOClient::ProcessMouseScroll()
{
	if(IsLMenu() || !GameOpt.MouseScroll) return;

	if(CurX>=MODE_WIDTH-1)
		GameOpt.ScrollMouseRight=true;
	else
		GameOpt.ScrollMouseRight=false;

	if(CurX<=0)
		GameOpt.ScrollMouseLeft=true;
	else
		GameOpt.ScrollMouseLeft=false;

	if(CurY>=MODE_HEIGHT-1)
		GameOpt.ScrollMouseDown=true;
	else
		GameOpt.ScrollMouseDown=false;

	if(CurY<=0)
		GameOpt.ScrollMouseUp=true;
	else
		GameOpt.ScrollMouseUp=false;
}

void FOClient::ProcessKeybScroll(bool down, BYTE dik)
{
	if(down && IsMainScreen(SCREEN_GAME) && ConsoleEdit) return;

	switch(dik)
	{
	case DIK_LEFT: GameOpt.ScrollKeybLeft=down; break;
	case DIK_RIGHT: GameOpt.ScrollKeybRight=down; break;
	case DIK_UP: GameOpt.ScrollKeybUp=down; break;
	case DIK_DOWN: GameOpt.ScrollKeybDown=down; break;
	default: break;
	}
}

bool FOClient::IsCurInWindow()
{
	if(GetActiveWindow()!=Wnd) return false;

	if(!GameOpt.FullScreen)
	{
		if(IsLMenu()) return true;

		POINT p;
		GetCursorPos(&p);
		WINDOWINFO wi;
		wi.cbSize=sizeof(wi);
		GetWindowInfo(Wnd,&wi);
		return p.x>=wi.rcClient.left && p.x<=wi.rcClient.right && p.y>=wi.rcClient.top && p.y<=wi.rcClient.bottom;
	}
	return true;
}

const char* FOClient::GetHoloText(DWORD str_num)
{
	if(str_num/10>=USER_HOLO_START_NUM)
	{
		if(!MsgUserHolo->Count(str_num))
		{
			Net_SendGetUserHoloStr(str_num);
			static char wait[32]="...";
			return wait;
		}
		return MsgUserHolo->GetStr(str_num);
	}
	return MsgHolo->GetStr(str_num);
}

const char* FOClient::FmtGameText(DWORD str_num, ...)
{
//	return Str::Format(MsgGame->GetStr(str_num),(void*)(_ADDRESSOF(str_num)+_INTSIZEOF(str_num)));

	static char res[MAX_FOTEXT];
	static char str[MAX_FOTEXT];

	StringCopy(str,MsgGame->GetStr(str_num));
	Str::Replacement(str,'\\','n','\n');

	va_list list;
	va_start(list,str_num);
	vsprintf(res,str,list);
	va_end(list);

	return res;
}

const char* FOClient::FmtCombatText(DWORD str_num, ...)
{
//	return Str::Format(MsgCombat->GetStr(str_num),(void*)(_ADDRESSOF(str_num)+_INTSIZEOF(str_num)));

	static char res[MAX_FOTEXT];
	static char str[MAX_FOTEXT];

	StringCopy(str,MsgCombat->GetStr(str_num));

	va_list list;
	va_start(list,str_num);
	vsprintf(res,str,list);
	va_end(list);

	return res;
}

const char* FOClient::FmtGenericDesc(int desc_type, int& ox, int& oy)
{
	ox=0;
	oy=0;
	if(Script::PrepareContext(ClientFunctions.GenericDesc,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgDword(desc_type);
		Script::SetArgAddress(&ox);
		Script::SetArgAddress(&oy);
		if(Script::RunPrepared())
		{
			CScriptString* result=(CScriptString*)Script::GetReturnedObject();
			if(result)
			{
				static char str[MAX_FOTEXT];
				StringCopy(str,result->c_str());
				return str[0]?str:NULL;
			}
		}
	}

	return Str::Format("<error>");
}

const char* FOClient::FmtCritLook(CritterCl* cr, int look_type)
{
	if(Script::PrepareContext(ClientFunctions.CritterLook,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(cr);
		Script::SetArgDword(look_type);
		if(Script::RunPrepared())
		{
			CScriptString* result=(CScriptString*)Script::GetReturnedObject();
			if(result)
			{
				static char str[MAX_FOTEXT];
				StringCopy(str,MAX_FOTEXT,result->c_str());
				return str[0]?str:NULL;
			}
		}
	}

	return MsgGame->GetStr(STR_CRIT_LOOK_NOTHING);
}

const char* FOClient::FmtItemLook(Item* item, int look_type)
{
	if(Script::PrepareContext(ClientFunctions.ItemLook,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(item);
		Script::SetArgDword(look_type);
		if(Script::RunPrepared())
		{
			CScriptString* result=(CScriptString*)Script::GetReturnedObject();
			if(result)
			{
				static char str[MAX_FOTEXT];
				StringCopy(str,MAX_FOTEXT,result->c_str());
				return str[0]?str:NULL;
			}
		}
	}

	return MsgGame->GetStr(STR_ITEM_LOOK_NOTHING);
}

void FOClient::ParseIntellectWords(char* words, PCharPairVec& text)
{
	Str::SkipLine(words);

	bool parse_in=true;
	char in[128]={0};
	char out[128]={0};

	while(true)
	{
		if(*words==0) break;

		if(*words=='\n' || *words=='\r')
		{
			Str::SkipLine(words);
			parse_in=true;

			int in_len=strlen(in)+1;
			int out_len=strlen(out)+1;
			if(in_len<2) continue;

			char* in_=new char[in_len];
			char* out_=new char[out_len];
			StringCopy(in_,in_len,in);
			StringCopy(out_,out_len,out);

			text.push_back(PCharPairVecVal(in_,out_));
			in[0]=0;
			out[0]=0;
			continue;
		}

		if(*words==' ' && *(words+1)==' ' && parse_in)
		{
			if(!strlen(in))
			{
				Str::SkipLine(words);
			}
			else
			{
				words+=2;
				parse_in=false;
			}
			continue;
		}

		strncat(parse_in?in:out,words,1);
		words++;
	}
}

PCharPairVecIt FOClient::FindIntellectWord(const char* word, PCharPairVec& text)
{
	PCharPairVecIt it=text.begin();
	PCharPairVecIt end=text.end();
	for(;it!=end;++it)
	{
		if(!_stricmp(word,(*it).first)) break;
	}

	if(it!=end)
	{
		PCharPairVecIt it_=it;
		it++;
		int cnt=0;
		for(;it!=end;++it)
		{
			if(!_stricmp((*it_).first,(*it).first)) cnt++;
			else break;
		}
		it_+=Random(0,cnt);
		it=it_;
	}

	return it;
}

void FOClient::FmtTextIntellect(char* str, WORD intellect)
{
	static bool is_parsed=false;
	if(!is_parsed)
	{
		ParseIntellectWords((char*)MsgGame->GetStr(STR_INTELLECT_WORDS),IntellectWords);
		ParseIntellectWords((char*)MsgGame->GetStr(STR_INTELLECT_SYMBOLS),IntellectSymbols);
		is_parsed=true;
	}

	BYTE intellegence=intellect&0xF;
	if(!intellect || intellegence>=5) return;

	int word_proc;
	int symbol_proc;
	switch(intellegence)
	{
	default:
	case 1: word_proc=100; symbol_proc=20; break;
	case 2: word_proc=80; symbol_proc=15; break;
	case 3: word_proc=60; symbol_proc=10; break;
	case 4: word_proc=30; symbol_proc=5; break;
	}

	srand((intellect<<16)|intellect);

	char word[1024]={0};
	while(true)
	{
		if((*str>='a' && *str<='z') || (*str>='A' && *str<='Z') ||
		   (*str>='а' && *str<='я') || (*str>='А' && *str<='Я'))
		{
			strncat(word,str,1);
			str++;
			continue;
		}

		int len=strlen(word);
		if(len)
		{
			PCharPairVecIt it=FindIntellectWord(word,IntellectWords);
			if(it!=IntellectWords.end() && Random(1,100)<=word_proc)
			{
				Str::EraseInterval(str-len,len);
				Str::Insert(str-len,(*it).second);
				str=str-len+strlen((*it).second);
			}
			else
			{
				for(char* s=str-len;s<str;s++)
				{
					if(Random(1,100)>symbol_proc) continue;

					word[0]=*s;
					word[1]=0;

					it=FindIntellectWord(word,IntellectSymbols);
					if(it==IntellectSymbols.end()) continue;

					int f_len=strlen((*it).first);
					int s_len=strlen((*it).second);
					Str::EraseInterval(s,f_len);
					Str::Insert(s,(*it).second);
					s-=f_len;
					str-=f_len;
					s+=s_len;
					str+=s_len;
				}
			}
			word[0]=0;
		}

		if(*str==0) break;
		str++;
	}
}

bool FOClient::SaveLogFile()
{
	if(MessBox.empty()) return false;

	SYSTEMTIME sys_time;
	GetLocalTime(&sys_time);

	char log_path[512];
	sprintf(log_path,"%smessbox_%02d-%02d-%d_%02d-%02d-%02d.txt",PATH_LOG_FILE,
		sys_time.wDay,sys_time.wMonth,sys_time.wYear,
		sys_time.wHour,sys_time.wMinute,sys_time.wSecond);

	HANDLE save_file=CreateFile(log_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,NULL);
	if(save_file==INVALID_HANDLE_VALUE) return false;

	char cur_mess[MAX_FOTEXT];
	string fmt_log;
	for(int i=0;i<MessBox.size();++i)
	{
		MessBoxMessage& m=MessBox[i];
		// Skip
		if(IsMainScreen(SCREEN_GAME) && std::find(MessBoxFilters.begin(),MessBoxFilters.end(),m.Type)!=MessBoxFilters.end()) continue;
		// Concat
		StringCopy(cur_mess,m.Mess.c_str());
		Str::EraseWords(cur_mess,'|',' ');
		fmt_log+=MessBox[i].Time+string(cur_mess);
	}

	DWORD br;
	WriteFile(save_file,fmt_log.c_str(),fmt_log.length(),&br,NULL);
	CloseHandle(save_file);
	return true;
}

bool FOClient::SaveScreenshot()
{
	if(!SprMngr.IsInit()) return false;

	LPDIRECT3DSURFACE surf=NULL;
	if(FAILED(SprMngr.GetDevice()->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&surf))) return false;

	SYSTEMTIME sys_time;
	GetLocalTime(&sys_time);

	char screen_path[512];
	sprintf(screen_path,"%sscreen_%02d-%02d-%d_%02d-%02d-%02d.",PATH_SCREENS_FILE,
		sys_time.wDay,sys_time.wMonth,sys_time.wYear,
		sys_time.wHour,sys_time.wMinute,sys_time.wSecond);
	StringAppend(screen_path,"jpg");

	if(FAILED(D3DXSaveSurfaceToFile(screen_path,D3DXIFF_JPG,surf,NULL,NULL)))
	{
		surf->Release();
		return false;
	}

	surf->Release();
	return true;	
/*
	D3DSURFACE_DESC sDesc;
	if(FAILED(surf->GetDesc(&sDesc))) return FALSE;

	int s_width=sDesc.Width;
	int s_height=sDesc.Height;

	D3DLOCKED_RECT lr_dst;
	if(FAILED(surf->LockRect(&lr_dst,NULL,D3DLOCK_READONLY))) return FALSE;

	BYTE* p_dst=(BYTE*)lr_dst.pBits;

	// Create file
	SYSTEMTIME sys_time;
	GetSystemTime(&sys_time);
	
	char screen_path[512];
	sprintf(screen_path,"%sscreen_%02d-%02d-%d_%02d-%02d-%02d.bmp",PATH_SCREENS_FILE,
		sys_time.wDay,sys_time.wMonth,sys_time.wYear,
		sys_time.wHour,sys_time.wMinute,sys_time.wSecond);

	HANDLE save_file=CreateFile(screen_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,NULL);
	if(save_file==INVALID_HANDLE_VALUE)
	{
		surf->UnlockRect();
		return FALSE;
	}

	// Write Bmp
	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER bi;
	memset(&bf,0,sizeof(bf));
	memset(&bi,0,sizeof(bi));

	char* image=new char[s_width*s_height*4];
	if(!image)
	{
		surf->UnlockRect();
		CloseHandle(save_file);
		return FALSE;
	}

	memcpy(image,p_dst,s_width*s_height*4);

	bf.bfType='BM';
	bf.bfSize=sizeof(bf)+sizeof(bi)+s_width*s_height*4;
	bf.bfOffBits=sizeof(bf)+sizeof(bi);
	bi.biSize=sizeof(bi);
	bi.biWidth=s_width;
	bi.biHeight=s_height;
	bi.biPlanes=1;
	bi.biBitCount=32;
	bi.biSizeImage=s_width*s_height*4;

	// Save
	DWORD br;
	WriteFile(save_file,&bf,sizeof(bf),&br,NULL);
	WriteFile(save_file,&bi,sizeof(bi),&br,NULL);
	WriteFile(save_file,image,s_width*s_height*4,&br,NULL);

	// Finish
	CloseHandle(save_file);
	surf->UnlockRect();
	delete[] image;

	return TRUE;
*/
}

void FOClient::SoundProcess()
{
	// Manager
	SndMngr.Process();

	// Ambient
	static DWORD next_ambient=0;
	if(Timer::GameTick()>next_ambient)
	{
		if(IsMainScreen(SCREEN_GAME)) SndMngr.PlayAmbient(MsgGM->GetStr(STR_MAP_AMBIENT_(HexMngr.GetCurPidMap())));
		next_ambient=Timer::GameTick()+Random(AMBIENT_SOUND_TIME/2,AMBIENT_SOUND_TIME);
	}
}

void FOClient::AddVideo(const char* fname, const char* sound, bool can_stop, bool clear_sequence)
{
	if(clear_sequence && ShowVideos.size()) StopVideo();

	// Concat full path
	char full_path[MAX_FOPATH];
	FileMngr.GetFullPath(fname,PT_VIDEO,full_path);

	// Add video in sequence
	ShowVideo sw;
	sw.FileName=full_path;
	sw.CanStop=can_stop;
	sw.SoundName=(sound?sound:"");
	ShowVideos.push_back(sw);

	// Play
	if(ShowVideos.size()==1)
	{
		// Velosaped
		SprMngr.BeginScene(0xFF000000);
		SprMngr.EndScene();
		// Instant play
		PlayVideo(sw);
	}
}

#define CHECK_VIDEO_HR(expr) {HRESULT hr=expr; if(FAILED(hr)) {WriteLog(__FUNCTION__" - Error<%u>, line<%d>.\n",hr,__LINE__); ShowVideos.clear(); StopVideo(); return;}}
#define CHECK_VIDEO_NULLPTR(contrl) {if(!(contrl)) {WriteLog(__FUNCTION__" - Control nullptr, line<%d>.\n",__LINE__); ShowVideos.clear(); StopVideo(); return;}}
void FOClient::PlayVideo(ShowVideo& video)
{
	// Check file exiting
	WIN32_FIND_DATA fd;
	HANDLE f=FindFirstFile(video.FileName.c_str(),&fd);
	if(f==INVALID_HANDLE_VALUE)
	{
		FindClose(f);
		NextVideo();
		return;
	}
	FindClose(f);

	// Create direct show
	CHECK_VIDEO_HR(CoInitialize(NULL));

	// Create the filter graph manager and query for interfaces.
	CHECK_VIDEO_HR(CoCreateInstance(CLSID_FilterGraph,NULL,CLSCTX_INPROC,IID_IGraphBuilder,(void**)&GraphBuilder));
	CHECK_VIDEO_NULLPTR(GraphBuilder);
	CHECK_VIDEO_HR(GraphBuilder->QueryInterface(IID_IMediaControl,(void**)&MediaControl));
	CHECK_VIDEO_NULLPTR(MediaControl);
	CHECK_VIDEO_HR(GraphBuilder->QueryInterface(IID_IMediaSeeking,(void**)&MediaSeeking));
	CHECK_VIDEO_NULLPTR(MediaSeeking);
	GraphBuilder->QueryInterface(IID_IBasicAudio,(void**)&BasicAudio);

	// VMR Initialization
	// Create the VMR.
	CHECK_VIDEO_HR(CoCreateInstance(CLSID_VideoMixingRenderer,NULL,CLSCTX_INPROC,IID_IBaseFilter,(void**)&VMRFilter));
	CHECK_VIDEO_NULLPTR(VMRFilter);
	// Add the VMR to the filter graph.
	CHECK_VIDEO_HR(GraphBuilder->AddFilter(VMRFilter,L"Video Mixing Renderer"));
	// Set the rendering mode.
	CHECK_VIDEO_HR(VMRFilter->QueryInterface(IID_IVMRFilterConfig,(void**)&FilterConfig));
	CHECK_VIDEO_NULLPTR(FilterConfig);
	CHECK_VIDEO_HR(FilterConfig->SetRenderingMode(VMRMode_Windowless));
	CHECK_VIDEO_HR(VMRFilter->QueryInterface(IID_IVMRWindowlessControl,(void**)&WindowLess));
	CHECK_VIDEO_NULLPTR(WindowLess);
	CHECK_VIDEO_HR(WindowLess->SetVideoClippingWindow(Wnd));

	// Set the video window.
	RECT rsrc,rdest; // Source and destination rectangles.
	long w,h;
	WindowLess->GetNativeVideoSize(&w,&h,NULL,NULL);
	SetRect(&rsrc,0,0,w,h);
	GetClientRect(Wnd,&rdest);
	SetRect(&rdest,0,0,rdest.right,rdest.bottom);
	WindowLess->SetVideoPosition(&rsrc,&rdest);

	// Build the graph.
	wchar_t fname_[1024];
	mbstowcs(fname_,video.FileName.c_str(),1024); // Convert name
	CHECK_VIDEO_HR(GraphBuilder->RenderFile(fname_,NULL));

	// Set soundtrack
	/*IBaseFilter* splitter;
	if(SUCCEEDED(CoCreateInstance(CLSID_MMSPLITTER,NULL,CLSCTX_INPROC,IID_IBaseFilter,(void**)&splitter)))
	{
		if(SUCCEEDED(GraphBuilder->AddFilter(splitter,L"MPEG-2 Splitter")))
		{
			IAMStreamSelect* strm=NULL;
			if(SUCCEEDED(splitter->QueryInterface(IID_IAMStreamSelect,(void**)&strm)))
			{
				IPin* in,*out;
				HRESULT hr=splitter->FindPin(L"Output", &out);
				WriteLog("0hr<%u>\n",hr);
				hr=VMRFilter->FindPin(L"Input", &in);
				WriteLog("1hr<%u>\n",hr);
				hr=GraphBuilder->Connect(out,in);
				WriteLog("2hr<%u>\n",hr);

			//	strm->Enable(0,0);
			//	strm->Enable(1,0);
				DWORD count=9;
				hr=strm->Count(&count);
			//	if(count>1) strm->Enable(1,0);
				WriteLog("hr<%u> count<%u>\n",hr,count);
			//	SAFEREL(strm);
			}
		}
		//SAFEREL(mpeg1splitter);
	}*/

	// Run the graph.
	CHECK_VIDEO_HR(MediaControl->Run());

	// Start sound
	if(video.SoundName!="")
	{
		MusicVolumeRestore=SndMngr.GetMusicVolume();
		SndMngr.SetMusicVolume(100);
		SndMngr.PlayMusic(video.SoundName.c_str(),0,0);
	}
}

void FOClient::NextVideo()
{
	if(ShowVideos.size())
	{
		StopVideo();
		ShowVideos.erase(ShowVideos.begin());
		if(ShowVideos.size()) PlayVideo(ShowVideos[0]);
		else
		{
			ScreenFadeOut();
			if(MusicAfterVideo!="")
			{
				SndMngr.PlayMusic(MusicAfterVideo.c_str());
				MusicAfterVideo="";
			}
		}
	}
}

void FOClient::StopVideo()
{
	SAFEREL(BasicAudio);
	SAFEREL(MediaControl);
	SAFEREL(WindowLess);
	SAFEREL(MediaSeeking);
	SAFEREL(VMRFilter);
	SAFEREL(FilterConfig);
	SAFEREL(GraphBuilder);
	SndMngr.StopMusic();
	if(MusicVolumeRestore!=-1)
	{
		SndMngr.SetMusicVolume(MusicVolumeRestore);
		MusicVolumeRestore=-1;
	}
	//CoUninitialize(); // Crashed sometimes
}

DWORD FOClient::AnimLoad(DWORD name_hash, BYTE dir, int res_type)
{
	Self->SprMngr.SurfType=res_type;
	AnyFrames* frm=ResMngr.GetAnim(name_hash,dir);
	Self->SprMngr.SurfType=RES_NONE;
	if(!frm) return 0;
	IfaceAnim* ianim=new IfaceAnim(frm,res_type);
	if(!ianim) return 0;
	size_t index=1;
	for(size_t j=Animations.size();index<j;index++) if(!Animations[index]) break;
	if(index<Animations.size()) Animations[index]=ianim;
	else Animations.push_back(ianim);
	return index;
}

DWORD FOClient::AnimLoad(const char* fname, int path_type, int res_type)
{
	Self->SprMngr.SurfType=res_type;
	AnyFrames* frm=SprMngr.LoadAnyAnimation(fname,path_type,true,0);
	Self->SprMngr.SurfType=RES_NONE;
	if(!frm) return 0;
	IfaceAnim* ianim=new IfaceAnim(frm,res_type);
	if(!ianim) return 0;
	size_t index=1;
	for(size_t j=Animations.size();index<j;index++) if(!Animations[index]) break;
	if(index<Animations.size()) Animations[index]=ianim;
	else Animations.push_back(ianim);
	return index;
}

DWORD FOClient::AnimGetCurSpr(DWORD anim_id)
{
	if(anim_id>=Animations.size() || !Animations[anim_id]) return 0;
	return Animations[anim_id]->Frames->Ind[Animations[anim_id]->CurSpr];
}

DWORD FOClient::AnimGetCurSprCnt(DWORD anim_id)
{
	if(anim_id>=Animations.size() || !Animations[anim_id]) return 0;
	return Animations[anim_id]->CurSpr;
}

DWORD FOClient::AnimGetSprCount(DWORD anim_id)
{
	if(anim_id>=Animations.size() || !Animations[anim_id]) return 0;
	return Animations[anim_id]->Frames->CntFrm;
}

AnyFrames* FOClient::AnimGetFrames(DWORD anim_id)
{
	if(anim_id>=Animations.size() || !Animations[anim_id]) return 0;
	return Animations[anim_id]->Frames;
}

void FOClient::AnimRun(DWORD anim_id, DWORD flags)
{
	if(anim_id>=Animations.size() || !Animations[anim_id]) return;
	IfaceAnim* anim=Animations[anim_id];

	// Set flags
	anim->Flags=flags&0xFFFF;
	flags>>=16;

	// Set frm
	BYTE cur_frm=flags&0xFF;
	if(cur_frm>0)
	{
		cur_frm--;
		if(cur_frm>=anim->Frames->CntFrm) cur_frm=anim->Frames->CntFrm-1;
		anim->CurSpr=cur_frm;
	}
	//flags>>=8;
}

void FOClient::AnimProcess()
{
	for(IfaceAnimVecIt it=Animations.begin(),end=Animations.end();it!=end;++it)
	{
		IfaceAnim* anim=*it;
		if(!anim || !anim->Flags) continue;

		if(FLAG(anim->Flags,ANIMRUN_STOP))
		{
			anim->Flags=0;
			continue;
		}

		if(FLAG(anim->Flags,ANIMRUN_TO_END) || FLAG(anim->Flags,ANIMRUN_FROM_END))
		{
			DWORD cur_tick=Timer::GameTick();
			if(cur_tick-anim->LastTick<anim->Frames->Ticks/anim->Frames->CntFrm) continue;

			anim->LastTick=cur_tick;
			int end_spr=anim->Frames->CntFrm-1;
			if(FLAG(anim->Flags,ANIMRUN_FROM_END)) end_spr=0;

			if(anim->CurSpr<end_spr) anim->CurSpr++;
			else if(anim->CurSpr>end_spr) anim->CurSpr--;
			else
			{
				if(FLAG(anim->Flags,ANIMRUN_CYCLE))
				{
					if(FLAG(anim->Flags,ANIMRUN_TO_END)) anim->CurSpr=0;
					else anim->CurSpr=end_spr;
				}
				else
				{
					anim->Flags=0;
				}
			}
		}
	}
}

void FOClient::AnimFree(int res_type)
{
	ResMngr.FreeResources(res_type);
	for(IfaceAnimVecIt it=Animations.begin(),end=Animations.end();it!=end;++it)
	{
		IfaceAnim* anim=*it;
		if(anim && anim->ResType==res_type)
		{
			delete anim;
			(*it)=NULL;
		}
	}
}

/************************************************************************/
/* Scripts                                                              */
/************************************************************************/

StrSet ParametersAlready_;
DWORD ParametersIndex_=1; // 0 is ParamBase

bool FOClient::ReloadScripts()
{
	WriteLog("Load scripts...\n");

	FOMsg& msg_script=CurLang.Msg[TEXTMSG_INTERNAL];
	if(!msg_script.Count(STR_INTERNAL_SCRIPT_CONFIG) ||
		!msg_script.Count(STR_INTERNAL_SCRIPT_MODULES) ||
		!msg_script.Count(STR_INTERNAL_SCRIPT_MODULES+1))
	{
		WriteLog("Main script section not found in MSG.\n");
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_FAIL_RUN_START_SCRIPT));
		return false;
	}

	// Reinitialize engine
	Script::Finish();
	if(!Script::Init(false,PragmaCallbackCrData))
	{
		WriteLog("Unable to start script engine.\n");
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_FAIL_RUN_START_SCRIPT));
		return false;
	}

	// Bind stuff
#define BIND_CLIENT
#define BIND_CLASS FOClient::SScriptFunc::
#define BIND_ERROR do{WriteLog("Bind error, line<%d>.\n",__LINE__); bind_errors++;}while(0)

	asIScriptEngine* engine=Script::GetEngine();
	int bind_errors=0;
#include <ScriptBind.h>

	if(bind_errors)
	{
		WriteLog("Bind fail, errors<%d>.\n",bind_errors);
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_FAIL_RUN_START_SCRIPT));
		return false;
	}

	// Options
	Script::SetScriptsPath(PT_SCRIPTS);
	Script::Define("__CLIENT");
	ParametersAlready_.clear();
	ParametersIndex_=1;

	// Pragmas
	StrVec pragmas;
	for(int i=STR_INTERNAL_SCRIPT_PRAGMAS;;i+=2)
	{
		if(!msg_script.Count(i) || !msg_script.Count(i+1)) break;
		pragmas.push_back(msg_script.GetStr(i));
		pragmas.push_back(msg_script.GetStr(i+1));
	}
	Script::CallPragmas(pragmas);

	// Load modules
	int errors=0;
	for(int i=STR_INTERNAL_SCRIPT_MODULES;;i+=2)
	{
		if(!msg_script.Count(i) || !msg_script.Count(i+1)) break;

		const char* module_name=msg_script.GetStr(i);
		DWORD len;
		const BYTE* bytecode=msg_script.GetBinary(i+1,len);
		if(!bytecode) break;

		if(!Script::LoadScript(module_name,bytecode,len))
		{
			WriteLog("Load script<%s> fail.\n",module_name);
			errors++;
		}
	}

	// Bind functions
	errors+=Script::BindImportedFunctions();
	errors+=Script::RebindFunctions();

	// Bind reserved functions
	ReservedScriptFunction BindGameFunc[]={
		{&ClientFunctions.Start,"start","bool %s()"},
		{&ClientFunctions.Loop,"loop","uint %s()"},
		{&ClientFunctions.GetActiveScreens,"get_active_screens","void %s(int[]&)"},
		{&ClientFunctions.ScreenChange,"screen_change","void %s(bool,int,int,int,int)"},
		{&ClientFunctions.RenderIface,"render_iface","void %s(uint)"},
		{&ClientFunctions.RenderMap,"render_map","void %s()"},
		{&ClientFunctions.MouseDown,"mouse_down","bool %s(int)"},
		{&ClientFunctions.MouseUp,"mouse_up","bool %s(int)"},
		{&ClientFunctions.MouseMove,"mouse_move","void %s(int,int)"},
		{&ClientFunctions.KeyDown,"key_down","bool %s(uint8)"},
		{&ClientFunctions.KeyUp,"key_up","bool %s(uint8)"},
		{&ClientFunctions.InputLost,"input_lost","void %s()"},
		{&ClientFunctions.CritterIn,"critter_in","void %s(CritterCl&)"},
		{&ClientFunctions.CritterOut,"critter_out","void %s(CritterCl&)"},
		{&ClientFunctions.ItemMapIn,"item_map_in","void %s(ItemCl&)"},
		{&ClientFunctions.ItemMapChanged,"item_map_changed","void %s(ItemCl&,ItemCl&)"},
		{&ClientFunctions.ItemMapOut,"item_map_out","void %s(ItemCl&)"},
		{&ClientFunctions.ItemInvIn,"item_inv_in","void %s(ItemCl&)"},
		{&ClientFunctions.ItemInvOut,"item_inv_out","void %s(ItemCl&)"},
		{&ClientFunctions.MapMessage,"map_message","bool %s(string&,uint16&,uint16&,uint&,uint&)"},
		{&ClientFunctions.InMessage,"in_message","bool %s(string&,int&,uint&,uint&)"},
		{&ClientFunctions.OutMessage,"out_message","bool %s(string&,int&)"},
		{&ClientFunctions.ToHit,"to_hit","int %s(CritterCl&,CritterCl&,ProtoItem&,int)"},
		{&ClientFunctions.CombatResult,"combat_result","void %s(uint[]&)"},
		{&ClientFunctions.GenericDesc,"generic_description","string %s(int,int&,int&)"},
		{&ClientFunctions.ItemLook,"item_description","string %s(ItemCl&,int)"},
		{&ClientFunctions.CritterLook,"critter_description","string %s(CritterCl&,int)"},
		{&ClientFunctions.GetElevator,"get_elevator","bool %s(uint,uint[]&)"},
		{&ClientFunctions.ItemCost,"item_cost","uint %s(ItemCl&,CritterCl&,CritterCl&,bool)"},
		{&ClientFunctions.PerkCheck,"check_perk","bool %s(CritterCl&,uint)"},
		{&ClientFunctions.PlayerGeneration,"player_data_generate","void %s(int[]&)"},
		{&ClientFunctions.PlayerGenerationCheck,"player_data_check","bool %s(int[]&)"},
		{&ClientFunctions.CritterAction,"critter_action","void %s(bool,CritterCl&,int,int,ItemCl@)"},
		{&ClientFunctions.Animation2dProcess,"animation2d_process","void %s(CritterCl&,uint,uint,ItemCl@)"},
		{&ClientFunctions.Animation3dProcess,"animation3d_process","void %s(CritterCl&,uint,uint,ItemCl@)"},
		{&ClientFunctions.ItemsCollection,"items_collection","void %s(int,ItemCl@[]&)"},
	};
	const char* config=msg_script.GetStr(STR_INTERNAL_SCRIPT_CONFIG);
	if(!Script::BindReservedFunctions(config,"client",BindGameFunc,sizeof(BindGameFunc)/sizeof(BindGameFunc[0]))) errors++;

	if(errors)
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_FAIL_RUN_START_SCRIPT));
		return false;
	}

	AnimFree(RES_SCRIPT);

	if(!Script::PrepareContext(ClientFunctions.Start,CALL_FUNC_STR,"Game") || !Script::RunPrepared() || Script::GetReturnedBool()==false)
	{
		WriteLog("Execute start script fail.\n");
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_FAIL_RUN_START_SCRIPT));
		return false;
	}

	Crypt.SetCache("_user_interface",(BYTE*)GameOpt.UserInterface.c_str(),GameOpt.UserInterface.length()+1);
	WriteLog("Load scripts complete.\n");
	return true;
}

int FOClient::ScriptGetHitProc(CritterCl* cr, int hit_location)
{
	if(!Chosen) return 0;
	int use=Chosen->GetUse();
	ProtoItem* proto_item=Chosen->ItemSlotMain->Proto;
	if(!proto_item->IsWeapon() || !Chosen->ItemSlotMain->WeapIsUseAviable(use)) return 0;
	if(proto_item->Weapon.Weapon_CurrentUse!=use) proto_item->Weapon_SetUse(use);

	if(Script::PrepareContext(ClientFunctions.ToHit,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgObject(Chosen);
		Script::SetArgObject(cr);
		Script::SetArgObject(proto_item);
		Script::SetArgDword(hit_location);
		if(Script::RunPrepared()) return Script::GetReturnedDword();
	}
	return 0;
}

void FOClient::DrawIfaceLayer(DWORD layer)
{
	if(Script::PrepareContext(ClientFunctions.RenderIface,CALL_FUNC_STR,"Game"))
	{
		SpritesCanDraw=true;
		Script::SetArgDword(layer);
		Script::RunPrepared();
		SpritesCanDraw=false;
	}
}

bool FOClient::PragmaCallbackCrData(const char* text)
{
	string name;
	DWORD min,max;
	istrstream str(text);
	str >> name >> min >> max;
	if(str.fail()) return false;
	if(min>max || max>=MAX_PARAMS) return false;
	if(ParametersAlready_.count(name)) return true;
	if(ParametersIndex_>=MAX_PARAMETERS_ARRAYS) return false;

	asIScriptEngine* engine=Script::GetEngine();
	char decl[128];
	sprintf_s(decl,"DataVal %s",name.c_str());
	if(engine->RegisterObjectProperty("CritterCl",decl,offsetof(CritterCl,ThisPtr[ParametersIndex_]))<0) return false;
	sprintf_s(decl,"DataRef %sBase",name.c_str());
	if(engine->RegisterObjectProperty("CritterCl",decl,offsetof(CritterCl,ThisPtr[ParametersIndex_]))<0) return false;
	CritterCl::ParametersMin[ParametersIndex_]=min;
	CritterCl::ParametersMax[ParametersIndex_]=max;
	CritterCl::ParametersOffset[ParametersIndex_]=(strstr(text,"+")!=NULL);
	ParametersIndex_++;
	ParametersAlready_.insert(name);
	return true;
}

template<typename Type>
void AppendVectorToArray(vector<Type>& vec, asIScriptArray* arr)
{
	if(vec.empty()) return;
	int i=arr->GetElementCount();
	arr->Resize(arr->GetElementCount()+vec.size());
	for(int k=0,l=vec.size();k<l;k++,i++)
	{
		Type* p=(Type*)arr->GetElementPointer(i);
		*p=vec[k];
	}
}

template<typename Type>
void AppendVectorToArrayRef(vector<Type>& vec, asIScriptArray* arr)
{
	if(vec.empty()) return;
	int i=arr->GetElementCount();
	arr->Resize(arr->GetElementCount()+vec.size());
	for(int k=0,l=vec.size();k<l;k++,i++)
	{
		Type* p=(Type*)arr->GetElementPointer(i);
		*p=vec[k];
		(*p)->AddRef();
	}
}

template<typename Type>
void AssignScriptArrayInVector(vector<Type>& vec, asIScriptArray* arr)
{
	int cnt=arr->GetElementCount();
	if(!cnt) return;
	vec.resize(cnt);
	for(int i=0;i<cnt;i++)
	{
		Type* p=(Type*)arr->GetElementPointer(i);
		vec[i]=*p;
	}
}

int SortCritterHx_=0,SortCritterHy_=0;
bool SortCritterByDistPred(CritterCl* cr1, CritterCl* cr2)
{
	return DistGame(SortCritterHx_,SortCritterHy_,cr1->GetHexX(),cr1->GetHexY())<DistGame(SortCritterHx_,SortCritterHy_,cr2->GetHexX(),cr2->GetHexY());
}
void SortCritterByDist(CritterCl* cr, CritVec& critters)
{
	SortCritterHx_=cr->GetHexX();
	SortCritterHy_=cr->GetHexY();
	std::sort(critters.begin(),critters.end(),SortCritterByDistPred);
}
void SortCritterByDist(int hx, int hy, CritVec& critters)
{
	SortCritterHx_=hx;
	SortCritterHy_=hy;
	std::sort(critters.begin(),critters.end(),SortCritterByDistPred);
}

#define SCRIPT_ERROR(error) do{ScriptLastError=error; Script::LogError(__FUNCTION__", "error);}while(0)
#define SCRIPT_ERROR_RX(error,x) do{ScriptLastError=error; Script::LogError(__FUNCTION__", "error); return x;}while(0)
#define SCRIPT_ERROR_R(error) do{ScriptLastError=error; Script::LogError(__FUNCTION__", "error); return;}while(0)
#define SCRIPT_ERROR_R0(error) do{ScriptLastError=error; Script::LogError(__FUNCTION__", "error); return 0;}while(0)
static string ScriptLastError;

int FOClient::SScriptFunc::DataRef_Index(CritterClPtr& cr, DWORD index)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nulltptr.");
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	DWORD data_index=((DWORD)&cr-(DWORD)&cr->ThisPtr[0])/sizeof(cr->ThisPtr[0]);
	if(CritterCl::ParametersOffset[data_index]) index+=CritterCl::ParametersMin[data_index];
	if(index<CritterCl::ParametersMin[data_index]) SCRIPT_ERROR_R0("Index is less than minimum.");
	if(index>CritterCl::ParametersMax[data_index]) SCRIPT_ERROR_R0("Index is greather than maximum.");
	return cr->Params[index];
}

int FOClient::SScriptFunc::DataVal_Index(CritterClPtr& cr, DWORD index)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nulltptr.");
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	DWORD data_index=((DWORD)&cr-(DWORD)&cr->ThisPtr[0])/sizeof(cr->ThisPtr[0]);
	if(CritterCl::ParametersOffset[data_index]) index+=CritterCl::ParametersMin[data_index];
	if(index<CritterCl::ParametersMin[data_index]) SCRIPT_ERROR_R0("Index is less than minimum.");
	if(index>CritterCl::ParametersMax[data_index]) SCRIPT_ERROR_R0("Index is greather than maximum.");
	return cr->GetParam(index);
}

bool FOClient::SScriptFunc::Crit_IsChosen(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsChosen();
}

bool FOClient::SScriptFunc::Crit_IsPlayer(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsPlayer();
}

bool FOClient::SScriptFunc::Crit_IsNpc(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsNpc();
}

bool FOClient::SScriptFunc::Crit_IsLife(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsLife();
}

bool FOClient::SScriptFunc::Crit_IsKnockout(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsKnockout();
}

bool FOClient::SScriptFunc::Crit_IsDead(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsDead();
}

bool FOClient::SScriptFunc::Crit_IsFree(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsFree();
}

bool FOClient::SScriptFunc::Crit_IsBusy(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return !cr->IsFree();
}

bool FOClient::SScriptFunc::Crit_IsAnim3d(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->Anim3d!=NULL;
}

bool FOClient::SScriptFunc::Crit_IsAnimAviable(CritterCl* cr, DWORD anim1, DWORD anim2)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsAnimAviable(anim1,anim2);
}

bool FOClient::SScriptFunc::Crit_IsAnimPlaying(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsAnim();
}

DWORD FOClient::SScriptFunc::Crit_GetAnim1(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetAnim1();
}

void FOClient::SScriptFunc::Crit_Animate(CritterCl* cr, DWORD anim1, DWORD anim2)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->Animate(anim1,anim2,NULL);
}

void FOClient::SScriptFunc::Crit_AnimateEx(CritterCl* cr, DWORD anim1, DWORD anim2, Item* item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->Animate(anim1,anim2,item);
}

void FOClient::SScriptFunc::Crit_ClearAnim(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->ClearAnim();
}

void FOClient::SScriptFunc::Crit_Wait(CritterCl* cr, DWORD ms)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->TickStart(ms);
}

DWORD FOClient::SScriptFunc::Crit_ItemsCount(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetItemsCount();
}

DWORD FOClient::SScriptFunc::Crit_ItemsWeight(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetItemsWeight();
}

DWORD FOClient::SScriptFunc::Crit_ItemsVolume(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetItemsVolume();
}

DWORD FOClient::SScriptFunc::Crit_CountItem(CritterCl* cr, WORD proto_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->CountItemPid(proto_id);
}

DWORD FOClient::SScriptFunc::Crit_CountItemByType(CritterCl* cr, BYTE type)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->CountItemType(type);
}

Item* FOClient::SScriptFunc::Crit_GetItem(CritterCl* cr, WORD proto_id, int slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(proto_id) return cr->GetItemByPid(proto_id);
	return cr->GetItemSlot(slot);
}

DWORD FOClient::SScriptFunc::Crit_GetItems(CritterCl* cr, int slot, asIScriptArray* items)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ItemPtrVec items_;
	cr->GetItemsSlot(slot,items_);
	if(items) AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

DWORD FOClient::SScriptFunc::Crit_GetItemsByType(CritterCl* cr, int type, asIScriptArray* items)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ItemPtrVec items_;
	cr->GetItemsType(type,items_);
	if(items) AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

ProtoItem* FOClient::SScriptFunc::Crit_GetSlotProto(CritterCl* cr, int slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoItem* proto_item;
	switch(slot)
	{
	case SLOT_HAND1: proto_item=cr->ItemSlotMain->Proto; break;
	case SLOT_HAND2: proto_item=(cr->ItemSlotExt->GetId()?cr->ItemSlotExt->Proto:cr->DefItemSlotMain.Proto); break;
	case SLOT_ARMOR: proto_item=cr->ItemSlotArmor->Proto; break;
	default:
		{
			Item* item=cr->GetItemSlot(slot);
			if(item) proto_item=item->Proto;
		}
		break;
	}
	return proto_item;
}

void FOClient::SScriptFunc::Crit_SetVisible(CritterCl* cr, bool visible)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->Visible=visible;
	Self->HexMngr.RefreshMap();
}

bool FOClient::SScriptFunc::Crit_GetVisible(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->Visible;
}

void FOClient::SScriptFunc::Crit_set_ContourColor(CritterCl* cr, DWORD value)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->SprDrawValid) cr->SprDraw->ContourColor=value;
	cr->ContourColor=value;
}

DWORD FOClient::SScriptFunc::Crit_get_ContourColor(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->ContourColor;
}

DWORD FOClient::SScriptFunc::Crit_GetMultihex(CritterCl* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetMultihex();
}

bool FOClient::SScriptFunc::Item_IsGrouped(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->IsGrouped();
}

bool FOClient::SScriptFunc::Item_IsWeared(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->IsWeared();
}

DWORD FOClient::SScriptFunc::Item_GetScriptId(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->Data.ScriptId;
}

BYTE FOClient::SScriptFunc::Item_GetType(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetType();
}

WORD FOClient::SScriptFunc::Item_GetProtoId(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetProtoId();
}

DWORD FOClient::SScriptFunc::Item_GetCount(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetCount();
}

bool FOClient::SScriptFunc::Item_GetMapPosition(Item* item, WORD& hx, WORD& hy)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_R0("Map is not loaded.");
	switch(item->Accessory)
	{
	case ITEM_ACCESSORY_CRITTER:
		{
			CritterCl* cr=Self->GetCritter(item->ACC_CRITTER.Id);
			if(!cr) SCRIPT_ERROR_R0("CritterCl accessory, CritterCl not found.");
			hx=cr->GetHexX();
			hy=cr->GetHexY();
		}
		break;
	case ITEM_ACCESSORY_HEX:
		{
			hx=item->ACC_HEX.HexX;
			hy=item->ACC_HEX.HexY;
		}
		break;
	case ITEM_ACCESSORY_CONTAINER:
		{
			
			if(item->GetId()==item->ACC_CONTAINER.ContainerId) SCRIPT_ERROR_R0("Container accessory, Crosslinks.");
			Item* cont=Self->GetItem(item->ACC_CONTAINER.ContainerId);
			if(!cont) SCRIPT_ERROR_R0("Container accessory, Container not found.");
			return Item_GetMapPosition(cont,hx,hy); //Recursion
		}
		break;
	default:
		SCRIPT_ERROR_R0("Unknown accessory.");
		return false;
	}
	return true;
}

void FOClient::SScriptFunc::Item_Animate(Item* item, BYTE from_frame, BYTE to_frame)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
#pragma MESSAGE("Add item animation.")
}

bool FOClient::SScriptFunc::Item_IsCar(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->IsCar();
}

Item* FOClient::SScriptFunc::Item_CarGetBag(Item* item, int num_bag)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	//return item->CarGetBag(num_bag);
	// Need implement
	return NULL;
}

CritterCl* FOClient::SScriptFunc::Global_GetChosen()
{
	if(Self->Chosen && Self->Chosen->IsNotValid) return NULL;
	return Self->Chosen;
}

Item* FOClient::SScriptFunc::Global_GetItem(DWORD item_id)
{
	if(!item_id) SCRIPT_ERROR_R0("Item id arg is zero.");
	Item* item=Self->GetItem(item_id);
	if(!item || item->IsNotValid) return NULL;
	return item;
}

DWORD FOClient::SScriptFunc::Global_GetCrittersDistantion(CritterCl* cr1, CritterCl* cr2)
{
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_RX("Map is not loaded.",-1);
	if(cr1->IsNotValid) SCRIPT_ERROR_RX("Critter1 arg nullptr.",-1);
	if(cr2->IsNotValid) SCRIPT_ERROR_RX("Critter2 arg nullptr.",-1);
	return DistGame(cr1->GetHexX(),cr1->GetHexY(),cr2->GetHexX(),cr2->GetHexY());
}

CritterCl* FOClient::SScriptFunc::Global_GetCritter(DWORD critter_id)
{
	if(!critter_id) return NULL;//SCRIPT_ERROR_R0("CritterCl id arg is zero.");
	CritterCl* cr=Self->GetCritter(critter_id);
	if(!cr || cr->IsNotValid) return NULL;
	return cr;
}

DWORD FOClient::SScriptFunc::Global_GetCritters(WORD hx, WORD hy, DWORD radius, int find_type, asIScriptArray* critters)
{
	if(hx>=Self->HexMngr.GetMaxHexX() || hy>=Self->HexMngr.GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");

	CritMap& crits=Self->HexMngr.GetCritters();
	CritVec cr_vec;
	for(CritMapIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		CritterCl* cr=(*it).second;
		if(cr->CheckFind(find_type) && CheckDist(hx,hy,cr->GetHexX(),cr->GetHexY(),radius)) cr_vec.push_back(cr);
	}

	if(critters)
	{
		SortCritterByDist(hx,hy,cr_vec);
		AppendVectorToArrayRef<CritterCl*>(cr_vec,critters);
	}
	return cr_vec.size();
}

DWORD FOClient::SScriptFunc::Global_GetCrittersByPids(WORD pid, int find_type, asIScriptArray* critters)
{
	CritMap& crits=Self->HexMngr.GetCritters();
	CritVec cr_vec;
	if(!pid)
	{
		for(CritMapIt it=crits.begin(),end=crits.end();it!=end;++it)
		{
			CritterCl* cr=(*it).second;
			if(cr->CheckFind(find_type)) cr_vec.push_back(cr);
		}
	}
	else
	{
		for(CritMapIt it=crits.begin(),end=crits.end();it!=end;++it)
		{
			CritterCl* cr=(*it).second;
			if(cr->IsNpc() && cr->Pid==pid && cr->CheckFind(find_type)) cr_vec.push_back(cr);
		}
	}
	if(cr_vec.empty()) return 0;
	if(critters) AppendVectorToArrayRef<CritterCl*>(cr_vec,critters);
	return cr_vec.size();
}

DWORD FOClient::SScriptFunc::Global_GetCrittersInPath(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters)
{
	CritVec cr_vec;
	Self->HexMngr.TraceBullet(from_hx,from_hy,to_hx,to_hy,dist,angle,NULL,false,&cr_vec,FIND_LIFE|FIND_KO,NULL,NULL,NULL,true);
	if(critters) AppendVectorToArrayRef<CritterCl*>(cr_vec,critters);
	return cr_vec.size();
}

DWORD FOClient::SScriptFunc::Global_GetCrittersInPathBlock(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters, WORD& pre_block_hx, WORD& pre_block_hy, WORD& block_hx, WORD& block_hy)
{
	CritVec cr_vec;
	WordPair block,pre_block;
	Self->HexMngr.TraceBullet(from_hx,from_hy,to_hx,to_hy,dist,angle,NULL,false,&cr_vec,FIND_LIFE|FIND_KO,&block,&pre_block,NULL,true);
	if(critters) AppendVectorToArrayRef<CritterCl*>(cr_vec,critters);
	pre_block_hx=pre_block.first;
	pre_block_hy=pre_block.second;
	block_hx=block.first;
	block_hy=block.second;
	return cr_vec.size();
}

void FOClient::SScriptFunc::Global_GetHexInPath(WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist)
{
	WordPair pre_block,block;
	Self->HexMngr.TraceBullet(from_hx,from_hy,to_hx,to_hy,dist,angle,NULL,false,NULL,0,&block,&pre_block,NULL,true);
	to_hx=pre_block.first;
	to_hy=pre_block.second;
}

DWORD FOClient::SScriptFunc::Global_GetPathLengthHex(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD cut)
{
	if(from_hx>=Self->HexMngr.GetMaxHexX() || from_hy>=Self->HexMngr.GetMaxHexY()) SCRIPT_ERROR_R0("Invalid from hexes args.");
	if(to_hx>=Self->HexMngr.GetMaxHexX() || to_hy>=Self->HexMngr.GetMaxHexY()) SCRIPT_ERROR_R0("Invalid to hexes args.");

	if(cut>0 && !Self->HexMngr.CutPath(NULL,from_hx,from_hy,to_hx,to_hy,cut)) return 0;
	ByteVec steps;
	if(!Self->HexMngr.FindPath(NULL,from_hx,from_hy,to_hx,to_hy,steps,-1)) steps.clear();
	return steps.size();
}

DWORD FOClient::SScriptFunc::Global_GetPathLengthCr(CritterCl* cr, WORD to_hx, WORD to_hy, DWORD cut)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	if(to_hx>=Self->HexMngr.GetMaxHexX() || to_hy>=Self->HexMngr.GetMaxHexY()) SCRIPT_ERROR_R0("Invalid to hexes args.");

	if(cut>0 && !Self->HexMngr.CutPath(cr,cr->GetHexX(),cr->GetHexY(),to_hx,to_hy,cut)) return 0;
	ByteVec steps;
	if(!Self->HexMngr.FindPath(cr,cr->GetHexX(),cr->GetHexY(),to_hx,to_hy,steps,-1)) steps.clear();
	return steps.size();
}

void FOClient::SScriptFunc::Global_FlushScreen(DWORD from_color, DWORD to_color, DWORD ms)
{
	Self->ScreenFade(ms,from_color,to_color,true);
}

void FOClient::SScriptFunc::Global_QuakeScreen(DWORD noise, DWORD ms)
{
	Self->ScreenQuake(noise,ms);
}

void FOClient::SScriptFunc::Global_PlaySound(CScriptString& sound_name)
{
	SndMngr.PlaySound(sound_name.c_str());
}

void FOClient::SScriptFunc::Global_PlaySoundType(BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext)
{
	SndMngr.PlaySoundType(sound_type,sound_type_ext,sound_id,sound_id_ext);
}

void FOClient::SScriptFunc::Global_PlayMusic(CScriptString& music_name, DWORD pos, DWORD repeat)
{
	SndMngr.PlayMusic(music_name.c_str(),pos,repeat);
}

void FOClient::SScriptFunc::Global_PlayVideo(CScriptString& video_name, bool can_stop)
{
	SndMngr.StopMusic();
	Self->AddVideo(video_name.c_str(),NULL,can_stop,true);
}

bool FOClient::SScriptFunc::Global_IsTurnBased()
{
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_R0("Map is not loaded.");
	return Self->IsTurnBased;
}

WORD FOClient::SScriptFunc::Global_GetCurrentMapPid()
{
	if(!Self->HexMngr.IsMapLoaded()) return 0;
	return Self->HexMngr.GetCurPidMap();
}

DWORD FOClient::SScriptFunc::Global_GetMessageFilters(asIScriptArray* filters)
{
	if(filters) Script::AppendVectorToArray(Self->MessBoxFilters,filters);
	return Self->MessBoxFilters.size();
}

void FOClient::SScriptFunc::Global_SetMessageFilters(asIScriptArray* filters)
{
	Self->MessBoxFilters.clear();
	if(filters) Script::AssignScriptArrayInVector(Self->MessBoxFilters,filters);
}

void FOClient::SScriptFunc::Global_Message(CScriptString& msg)
{
	Self->AddMess(FOMB_GAME,msg.c_str());
}

void FOClient::SScriptFunc::Global_MessageType(CScriptString& msg, int type)
{
	if(type<FOMB_GAME || type>FOMB_VIEW) type=FOMB_GAME;
	Self->AddMess(type,msg.c_str());
}

void FOClient::SScriptFunc::Global_MessageMsg(int text_msg, DWORD str_num)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R("Invalid text msg arg.");
	Self->AddMess(FOMB_GAME,Self->CurLang.Msg[text_msg].GetStr(str_num));
}

void FOClient::SScriptFunc::Global_MessageMsgType(int text_msg, DWORD str_num, int type)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R("Invalid text msg arg.");
	if(type<FOMB_GAME || type>FOMB_VIEW) type=FOMB_GAME;
	Self->AddMess(type,Self->CurLang.Msg[text_msg].GetStr(str_num));
}

void FOClient::SScriptFunc::Global_MapMessage(CScriptString& text, WORD hx, WORD hy, DWORD ms, DWORD color, bool fade, int ox, int oy)
{
	FOClient::MapText t;
	t.HexX=hx;
	t.HexY=hy;
	t.Color=(color?color:COLOR_TEXT);
	t.Fade=fade;
	t.StartTick=Timer::GameTick();
	t.Tick=ms;
	t.Text=text.c_std_str();
	t.Rect=Self->HexMngr.GetRectForText(hx,hy);
	t.EndRect=INTRECT(t.Rect,ox,oy);
	MapTextVecIt it=std::find(Self->GameMapTexts.begin(),Self->GameMapTexts.end(),t);
	if(it!=Self->GameMapTexts.end()) Self->GameMapTexts.erase(it);
	Self->GameMapTexts.push_back(t);
}

CScriptString* FOClient::SScriptFunc::Global_GetMsgStr(int text_msg, DWORD str_num)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R0("Invalid text msg arg.");
	CScriptString* str=new CScriptString(text_msg==TEXTMSG_HOLO?Self->GetHoloText(str_num):Self->CurLang.Msg[text_msg].GetStr(str_num));
	return str;
}

CScriptString* FOClient::SScriptFunc::Global_GetMsgStrSkip(int text_msg, DWORD str_num, DWORD skip_count)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R0("Invalid text msg arg.");
	CScriptString* str=new CScriptString(text_msg==TEXTMSG_HOLO?Self->GetHoloText(str_num):Self->CurLang.Msg[text_msg].GetStr(str_num,skip_count));
	return str;
}

DWORD FOClient::SScriptFunc::Global_GetMsgStrNumUpper(int text_msg, DWORD str_num)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R0("Invalid text msg arg.");
	return Self->CurLang.Msg[text_msg].GetStrNumUpper(str_num);
}

DWORD FOClient::SScriptFunc::Global_GetMsgStrNumLower(int text_msg, DWORD str_num)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R0("Invalid text msg arg.");
	return Self->CurLang.Msg[text_msg].GetStrNumLower(str_num);
}

DWORD FOClient::SScriptFunc::Global_GetMsgStrCount(int text_msg, DWORD str_num)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R0("Invalid text msg arg.");
	return Self->CurLang.Msg[text_msg].Count(str_num);
}

bool FOClient::SScriptFunc::Global_IsMsgStr(int text_msg, DWORD str_num)
{
	if(text_msg>=TEXTMSG_COUNT) SCRIPT_ERROR_R0("Invalid text msg arg.");
	return Self->CurLang.Msg[text_msg].Count(str_num)>0;
}

CScriptString* FOClient::SScriptFunc::Global_ReplaceTextStr(CScriptString& text, CScriptString& replace, CScriptString& str)
{
	size_t pos=text.c_std_str().find(replace.c_std_str(),0);
	if(pos==std::string::npos) return new CScriptString(text);
	string result=text.c_std_str();
	return new CScriptString(result.replace(pos,replace.length(),str.c_std_str()));
}

CScriptString* FOClient::SScriptFunc::Global_ReplaceTextInt(CScriptString& text, CScriptString& replace, int i)
{
	size_t pos=text.c_std_str().find(replace.c_std_str(),0);
	if(pos==std::string::npos) return new CScriptString(text);
	char val[32];
	sprintf(val,"%d",i);
	string result=text.c_std_str();
	return new CScriptString(result.replace(pos,replace.length(),val));
}

CScriptString* FOClient::SScriptFunc::Global_FormatTags(CScriptString& text, CScriptString* lexems)
{
	static char* buf=NULL;
	static DWORD buf_len=0;
	if(!buf)
	{
		buf=new char[MAX_FOTEXT];
		if(!buf) SCRIPT_ERROR_R0("Allocation fail.");
		buf_len=MAX_FOTEXT;
	}

	if(buf_len<text.length()*2)
	{
		delete[] buf;
		buf=new char[text.length()*2];
		if(!buf) SCRIPT_ERROR_R0("Reallocation fail.");
		buf_len=text.length()*2;
	}

	StringCopy(buf,buf_len,text.c_str());
	Self->FormatTags(buf,buf_len,Self->Chosen,NULL,lexems?lexems->c_str():NULL);
	return new CScriptString(buf);
}

int FOClient::SScriptFunc::Global_GetSomeValue(int var)
{
	switch(var)
	{
	case 0:
	case 1:
		break;
	}
	return 0;
}

bool FOClient::SScriptFunc::Global_LoadDataFile(CScriptString& dat_name)
{
	if(FileManager::LoadDataFile(dat_name.c_str()))
	{
		ResMngr.Refresh(NULL);
		FONames::GenerateFoNames(PT_DATA);
		return true;
	}
	return false;
}

void FOClient::SScriptFunc::Global_MoveScreen(WORD hx, WORD hy, DWORD speed)
{
	if(hx>=Self->HexMngr.GetMaxHexX() || hy>=Self->HexMngr.GetMaxHexY()) SCRIPT_ERROR_R("Invalid hex args.");
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_R("Map is not loaded.");
	if(!speed) Self->HexMngr.FindSetCenter(hx,hy);
	else Self->HexMngr.ScrollToHex(hx,hy,double(speed)/1000.0,false);
}

void FOClient::SScriptFunc::Global_LockScreenScroll(CritterCl* cr)
{
	if(cr && cr->IsNotValid) SCRIPT_ERROR_R("CritterCl arg nullptr.");
	Self->HexMngr.AutoScroll.LockedCritter=(cr?cr->GetId():0);
}

int FOClient::SScriptFunc::Global_GetFog(WORD zone_x, WORD zone_y)
{
	if(!Self->Chosen || Self->Chosen->IsNotValid) SCRIPT_ERROR_R0("Chosen data not valid.");
	if(zone_x>=GameOpt.GlobalMapWidth || zone_y>=GameOpt.GlobalMapHeight) SCRIPT_ERROR_R0("Invalid world map pos arg.");
	return Self->GmapFog.Get2Bit(zone_x,zone_y);
}

void FOClient::SScriptFunc::Global_RefreshItemsCollection(int collection)
{
	switch(collection)
	{
	case ITEMS_INVENTORY: Self->ProcessItemsCollection(ITEMS_INVENTORY,Self->InvContInit,Self->InvCont); break;
	case ITEMS_USE: Self->ProcessItemsCollection(ITEMS_USE,Self->InvContInit,Self->UseCont); break;
	case ITEMS_BARTER: Self->ProcessItemsCollection(ITEMS_BARTER,Self->InvContInit,Self->BarterCont1); break;
	case ITEMS_BARTER_OFFER: Self->ProcessItemsCollection(ITEMS_BARTER_OFFER,Self->BarterCont1oInit,Self->BarterCont1o); break;
	case ITEMS_BARTER_OPPONENT: Self->ProcessItemsCollection(ITEMS_BARTER_OPPONENT,Self->BarterCont2Init,Self->BarterCont2); break;
	case ITEMS_BARTER_OPPONENT_OFFER: Self->ProcessItemsCollection(ITEMS_BARTER_OPPONENT_OFFER,Self->BarterCont2oInit,Self->BarterCont2o); break;
	case ITEMS_PICKUP: Self->ProcessItemsCollection(ITEMS_PICKUP,Self->InvContInit,Self->PupCont1); break;
	case ITEMS_PICKUP_FROM: Self->ProcessItemsCollection(ITEMS_PICKUP_FROM,Self->PupCont2Init,Self->PupCont2); break;
	default: SCRIPT_ERROR("Invalid items collection."); break;
	}
}

#define SCROLL_MESSBOX                (0)
#define SCROLL_INVENTORY              (1)
#define SCROLL_INVENTORY_ITEM_INFO    (2)
#define SCROLL_PICKUP                 (3)
#define SCROLL_PICKUP_FROM            (4)
#define SCROLL_USE                    (5)
#define SCROLL_BARTER                 (6)
#define SCROLL_BARTER_OFFER           (7)
#define SCROLL_BARTER_OPPONENT        (8)
#define SCROLL_BARTER_OPPONENT_OFFER  (9)
#define SCROLL_GLOBAL_MAP_CITIES_X    (10)
#define SCROLL_GLOBAL_MAP_CITIES_Y    (11)
#define SCROLL_SPLIT_VALUE            (12)
#define SCROLL_TIMER_VALUE            (13)
#define SCROLL_PERK                   (14)
#define SCROLL_DIALOG_TEXT            (15)
#define SCROLL_MAP_ZOOM_VALUE         (16)
#define SCROLL_CHARACTER_PERKS        (17)
#define SCROLL_CHARACTER_KARMA        (18)
#define SCROLL_CHARACTER_KILLS        (19)
#define SCROLL_PIPBOY_STATUS          (20)
#define SCROLL_PIPBOY_STATUS_QUESTS   (21)
#define SCROLL_PIPBOY_STATUS_SCORES   (22)
#define SCROLL_PIPBOY_AUTOMAPS        (23)
#define SCROLL_PIPBOY_ARCHIVES        (24)
#define SCROLL_PIPBOY_ARCHIVES_INFO   (25)

int FOClient::SScriptFunc::Global_GetScroll(int scroll_element)
{
	switch(scroll_element)
	{
	case SCROLL_MESSBOX: return Self->MessBoxScroll;
	case SCROLL_INVENTORY: return Self->InvScroll;
	case SCROLL_INVENTORY_ITEM_INFO: return Self->InvItemInfoScroll;
	case SCROLL_PICKUP: return Self->PupScroll1;
	case SCROLL_PICKUP_FROM: return Self->PupScroll2;
	case SCROLL_USE: return Self->UseScroll;
	case SCROLL_BARTER: return Self->BarterScroll1;
	case SCROLL_BARTER_OFFER: return Self->BarterScroll1o;
	case SCROLL_BARTER_OPPONENT: return Self->BarterScroll2;
	case SCROLL_BARTER_OPPONENT_OFFER: return Self->BarterScroll2o;
	case SCROLL_GLOBAL_MAP_CITIES_X: return Self->GmapTabsScrX;
	case SCROLL_GLOBAL_MAP_CITIES_Y: return Self->GmapTabsScrY;
	case SCROLL_SPLIT_VALUE: return Self->SplitValue;
	case SCROLL_TIMER_VALUE: return Self->TimerValue;
	case SCROLL_PERK: return Self->PerkScroll;
	case SCROLL_DIALOG_TEXT: return Self->DlgMainTextCur;
	case SCROLL_MAP_ZOOM_VALUE: return Self->LmapZoom;
	case SCROLL_CHARACTER_PERKS: return Self->ChaSwitchScroll[CHA_SWITCH_PERKS];
	case SCROLL_CHARACTER_KARMA: return Self->ChaSwitchScroll[CHA_SWITCH_KARMA];
	case SCROLL_CHARACTER_KILLS: return Self->ChaSwitchScroll[CHA_SWITCH_KILLS];
	case SCROLL_PIPBOY_STATUS: return Self->PipScroll[PIP__STATUS];
	case SCROLL_PIPBOY_STATUS_QUESTS: return Self->PipScroll[PIP__STATUS_QUESTS];
	case SCROLL_PIPBOY_STATUS_SCORES: return Self->PipScroll[PIP__STATUS_SCORES];
	case SCROLL_PIPBOY_AUTOMAPS: return Self->PipScroll[PIP__AUTOMAPS];
	case SCROLL_PIPBOY_ARCHIVES: return Self->PipScroll[PIP__ARCHIVES];
	case SCROLL_PIPBOY_ARCHIVES_INFO: return Self->PipScroll[PIP__ARCHIVES_INFO];
	default: break;
	}
	return 0;
}

int ContainerMaxScroll(int items_count, int cont_height, int item_height)
{
	int height_items=cont_height/item_height;
	if(items_count<=height_items) return 0;
	return items_count-height_items;
}

void FOClient::SScriptFunc::Global_SetScroll(int scroll_element, int value)
{
	int* scroll=NULL;
	int min_value=0;
	int max_value=1000000000;
	switch(scroll_element)
	{
	case SCROLL_MESSBOX: scroll=&Self->MessBoxScroll; max_value=Self->MessBoxMaxScroll; break;
	case SCROLL_INVENTORY: scroll=&Self->InvScroll; max_value=ContainerMaxScroll(Self->InvCont.size(),Self->InvWInv.H(),Self->InvHeightItem); break;
	case SCROLL_INVENTORY_ITEM_INFO: scroll=&Self->InvItemInfoScroll; max_value=Self->InvItemInfoMaxScroll; break;
	case SCROLL_PICKUP: scroll=&Self->PupScroll1; max_value=ContainerMaxScroll(Self->PupCont1.size(),Self->PupWCont1.H(),Self->PupHeightItem1); break;
	case SCROLL_PICKUP_FROM: scroll=&Self->PupScroll2; max_value=ContainerMaxScroll(Self->PupCont2.size(),Self->PupWCont2.H(),Self->PupHeightItem2); break;
	case SCROLL_USE: scroll=&Self->UseScroll; max_value=ContainerMaxScroll(Self->UseCont.size(),Self->UseWInv.H(),Self->UseHeightItem); break;
	case SCROLL_BARTER: scroll=&Self->BarterScroll1; max_value=ContainerMaxScroll(Self->BarterCont1.size(),Self->BarterWCont1.H(),Self->BarterCont1HeightItem); break;
	case SCROLL_BARTER_OFFER: scroll=&Self->BarterScroll1o; max_value=ContainerMaxScroll(Self->BarterCont1o.size(),Self->BarterWCont1o.H(),Self->BarterCont1oHeightItem); break;
	case SCROLL_BARTER_OPPONENT: scroll=&Self->BarterScroll2; max_value=ContainerMaxScroll(Self->BarterCont2.size(),Self->BarterWCont2.H(),Self->BarterCont2HeightItem); break;
	case SCROLL_BARTER_OPPONENT_OFFER: scroll=&Self->BarterScroll2o; max_value=ContainerMaxScroll(Self->BarterCont2o.size(),Self->BarterWCont2o.H(),Self->BarterCont2oHeightItem); break;
	case SCROLL_GLOBAL_MAP_CITIES_X: scroll=&Self->GmapTabsScrX; break;
	case SCROLL_GLOBAL_MAP_CITIES_Y: scroll=&Self->GmapTabsScrY; break;
	case SCROLL_SPLIT_VALUE: scroll=&Self->SplitValue; max_value=MAX_SPLIT_VALUE-1; break;
	case SCROLL_TIMER_VALUE: scroll=&Self->TimerValue; min_value=TIMER_MIN_VALUE; max_value=TIMER_MAX_VALUE; break;
	case SCROLL_PERK: scroll=&Self->PerkScroll; max_value=Self->PerkCollection.size()-1; break;
	case SCROLL_DIALOG_TEXT: scroll=&Self->DlgMainTextCur; max_value=Self->DlgMainTextLinesReal-Self->DlgMainTextLinesRect; break;
	case SCROLL_MAP_ZOOM_VALUE: scroll=&Self->LmapZoom; min_value=2; max_value=13; break;
	case SCROLL_CHARACTER_PERKS: scroll=&Self->ChaSwitchScroll[CHA_SWITCH_PERKS]; break;
	case SCROLL_CHARACTER_KARMA: scroll=&Self->ChaSwitchScroll[CHA_SWITCH_KARMA]; break;
	case SCROLL_CHARACTER_KILLS: scroll=&Self->ChaSwitchScroll[CHA_SWITCH_KILLS]; break;
	case SCROLL_PIPBOY_STATUS: scroll=&Self->PipScroll[PIP__STATUS]; break;
	case SCROLL_PIPBOY_STATUS_QUESTS: scroll=&Self->PipScroll[PIP__STATUS_QUESTS]; break;
	case SCROLL_PIPBOY_STATUS_SCORES: scroll=&Self->PipScroll[PIP__STATUS_SCORES]; break;
	case SCROLL_PIPBOY_AUTOMAPS: scroll=&Self->PipScroll[PIP__AUTOMAPS]; break;
	case SCROLL_PIPBOY_ARCHIVES: scroll=&Self->PipScroll[PIP__ARCHIVES]; break;
	case SCROLL_PIPBOY_ARCHIVES_INFO: scroll=&Self->PipScroll[PIP__ARCHIVES_INFO]; break;
	default: return;
	}
	*scroll=CLAMP(value,min_value,max_value);
}

DWORD FOClient::SScriptFunc::Global_GetDayTime(DWORD day_part)
{
	if(day_part>=4) SCRIPT_ERROR_R0("Invalid day part arg.");
	if(Self->HexMngr.IsMapLoaded()) return Self->HexMngr.GetMapDayTime()[day_part];
	return 0;
}

void FOClient::SScriptFunc::Global_GetDayColor(DWORD day_part, BYTE& r, BYTE& g, BYTE& b)
{
	r=g=b=0;
	if(day_part>=4) SCRIPT_ERROR_R("Invalid day part arg.");
	if(Self->HexMngr.IsMapLoaded())
	{
		BYTE* col=Self->HexMngr.GetMapDayColor();
		r=col[0+day_part];
		g=col[4+day_part];
		b=col[8+day_part];
	}
}

CScriptString* FOClient::SScriptFunc::Global_GetLastError()
{
	return new CScriptString(ScriptLastError);
}

void FOClient::SScriptFunc::Global_Log(CScriptString& text)
{
	Script::Log(text.c_str());
}

ProtoItem* FOClient::SScriptFunc::Global_GetProtoItem(WORD proto_id)
{
	ProtoItem* proto_item=ItemMngr.GetProtoItem(proto_id);
	//if(!proto_item) SCRIPT_ERROR_R0("Proto item not found.");
	return proto_item;
}

DWORD FOClient::SScriptFunc::Global_GetDistantion(WORD hex_x1, WORD hex_y1, WORD hex_x2, WORD hex_y2)
{
	return DistGame(hex_x1,hex_y1,hex_x2,hex_y2);
}

BYTE FOClient::SScriptFunc::Global_GetDirection(WORD from_x, WORD from_y, WORD to_x, WORD to_y)
{
	return GetFarDir(from_x,from_y,to_x,to_y);
}

BYTE FOClient::SScriptFunc::Global_GetOffsetDir(WORD hx, WORD hy, WORD tx, WORD ty, float offset)
{
	float nx=3.0f*(float(tx)-float(hx));
	float ny=SQRT3T2_FLOAT*(float(ty)-float(hy))-SQRT3_FLOAT*(float(tx&1)-float(hx&1));
	float dir=180.0f+RAD2DEG*atan2(ny,nx);
	dir+=offset;
	if(dir>360.0f) dir-=360.0f;
	else if(dir<0.0f) dir+=360.0f;
	if(dir>=60.0f  && dir<120.0f) return 5;
	if(dir>=120.0f && dir<180.0f) return 4;
	if(dir>=180.0f && dir<240.0f) return 3;
	if(dir>=240.0f && dir<300.0f) return 2;
	if(dir>=300.0f && dir<360.0f) return 1;
	return 0;
}

DWORD FOClient::SScriptFunc::Global_GetFullSecond(WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second)
{
	if(!year) year=GameOpt.Year;
	else year=CLAMP(year,GameOpt.YearStart,GameOpt.YearStart+130);
	if(!month) month=GameOpt.Month;
	else month=CLAMP(month,1,12);
	if(!day) day=GameOpt.Day;
	else
	{
		DWORD month_day=GameTimeMonthDay(year,month);
		day=CLAMP(day,1,month_day);
	}
	hour=CLAMP(hour,0,23);
	minute=CLAMP(minute,0,59);
	second=CLAMP(second,0,59);
	return GetFullSecond(year,month,day,hour,minute,second);
}

void FOClient::SScriptFunc::Global_GetGameTime(DWORD full_second, WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute, WORD& second)
{
	SYSTEMTIME st=GetGameTime(full_second);
	year=st.wYear;
	month=st.wMonth;
	day_of_week=st.wDayOfWeek;
	day=st.wDay;
	hour=st.wHour;
	minute=st.wMinute;
	second=st.wSecond;
}

bool FOClient::SScriptFunc::Global_StrToInt(CScriptString& text, int& result)
{
	if(!text.length()) return false;
	return Str::StrToInt(text.c_str(),result);
}

void FOClient::SScriptFunc::Global_MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, DWORD steps)
{
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_R("Map not loaded.");
	if(dir>5) SCRIPT_ERROR_R("Invalid dir arg.");
	if(!steps) SCRIPT_ERROR_R("Steps arg is zero.");
	if(steps>1)
	{
		for(int i=0;i<steps;i++) MoveHexByDir(hx,hy,dir,Self->HexMngr.GetMaxHexX(),Self->HexMngr.GetMaxHexY());
	}
	else
	{
		MoveHexByDir(hx,hy,dir,Self->HexMngr.GetMaxHexX(),Self->HexMngr.GetMaxHexY());
	}
}

CScriptString* FOClient::SScriptFunc::Global_GetIfaceIniStr(CScriptString& key)
{
	char* big_buf=Str::GetBigBuf();
	if(Self->IfaceIni.GetStr(key.c_str(),"",big_buf)) return new CScriptString(big_buf);
	return new CScriptString("");
}

bool FOClient::SScriptFunc::Global_Load3dFile(CScriptString& fname, int path_type)
{
	Animation3dEntity* entity=Animation3dEntity::GetEntity(fname.c_str(),path_type);
	return entity!=NULL;
}

void FOClient::SScriptFunc::Global_GetTime(WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute, WORD& second, WORD& milliseconds)
{
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);
	year=cur_time.wYear;
	month=cur_time.wMonth;
	day_of_week=cur_time.wDayOfWeek;
	day=cur_time.wDay;
	hour=cur_time.wHour;
	minute=cur_time.wMinute;
	second=cur_time.wSecond;
	milliseconds=cur_time.wMilliseconds;
}

bool FOClient::SScriptFunc::Global_SetParameterGetBehaviour(DWORD index, CScriptString& func_name)
{
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	CritterCl::ParamsGetScript[index]=0;
	if(func_name.length()>0)
	{
		int bind_id=Script::Bind(func_name.c_str(),"int %s(CritterCl&, uint)",false);
		if(bind_id<=0) SCRIPT_ERROR_R0("Function not found.");
		CritterCl::ParamsGetScript[index]=bind_id;
	}
	return true;
}

bool FOClient::SScriptFunc::Global_SetParameterChangeBehaviour(DWORD index, CScriptString& func_name)
{
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	CritterCl::ParamsChangeScript[index]=0;
	if(func_name.length()>0)
	{
		int bind_id=Script::Bind(func_name.c_str(),"void %s(CritterCl&, uint, int)",false);
		if(bind_id<=0) SCRIPT_ERROR_R0("Function not found.");
		CritterCl::ParamsChangeScript[index]=bind_id;
	}
	return true;
}

void FOClient::SScriptFunc::Global_AllowSlot(BYTE index, CScriptString& ini_option)
{
	if(index<=SLOT_ARMOR || index==SLOT_GROUND) SCRIPT_ERROR_R("Invalid index arg.");
	if(!ini_option.length()) SCRIPT_ERROR_R("Ini string is empty.");
	CritterCl::SlotEnabled[index]=true;
	SlotExt se;
	se.Index=index;
	se.IniName=Str::DuplicateString(ini_option.c_str());
	Self->IfaceLoadRect(se.Rect,se.IniName);
	Self->SlotsExt.push_back(se);
}

void FOClient::SScriptFunc::Global_SetRegistrationParam(DWORD index, bool enabled)
{
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R("Invalid index arg.");
	CritterCl::ParamsRegEnabled[index]=enabled;
}

DWORD FOClient::SScriptFunc::Global_GetAngelScriptProperty(int property)
{
	asIScriptEngine* engine=Script::GetEngine();
	if(!engine) SCRIPT_ERROR_R0("Can't get engine.");
	return engine->GetEngineProperty((asEEngineProp)property);
}

bool FOClient::SScriptFunc::Global_SetAngelScriptProperty(int property, DWORD value)
{
	asIScriptEngine* engine=Script::GetEngine();
	if(!engine) SCRIPT_ERROR_R0("Can't get engine.");
	int result=engine->SetEngineProperty((asEEngineProp)property,value);
	if(result<0) SCRIPT_ERROR_R0("Invalid data. Property not setted.");
	return true;
}

DWORD FOClient::SScriptFunc::Global_GetStrHash(CScriptString* str)
{
	if(str) return Str::GetHash(str->c_str());
	return 0;
}

bool FOClient::SScriptFunc::Global_IsCritterCanWalk(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanWalk(cr_type);
}

bool FOClient::SScriptFunc::Global_IsCritterCanRun(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanRun(cr_type);
}

bool FOClient::SScriptFunc::Global_IsCritterCanRotate(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanRotate(cr_type);
}

bool FOClient::SScriptFunc::Global_IsCritterCanAim(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanAim(cr_type);
}

bool FOClient::SScriptFunc::Global_IsCritterAnim1(DWORD cr_type, DWORD index)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsAnim1(cr_type,index);
}

bool FOClient::SScriptFunc::Global_IsCritterAnim3d(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsAnim3d(cr_type);
}

int FOClient::SScriptFunc::Global_GetGlobalMapRelief(DWORD x, DWORD y)
{
	if(x>=GM_MAXX || y>=GM_MAXY) SCRIPT_ERROR_R0("Invalid coord args.");
	return Self->GmapRelief->Get4Bit(x,y);
}

void FOClient::SScriptFunc::Global_RunServerScript(CScriptString& func_name, int p0, int p1, int p2, CScriptString* p3, asIScriptArray* p4)
{
	DwordVec dw;
	if(p4) AssignScriptArrayInVector<DWORD>(dw,p4);
	Self->Net_SendRunScript(false,func_name.c_str(),p0,p1,p2,p3?p3->c_str():NULL,dw);
}

void FOClient::SScriptFunc::Global_RunServerScriptUnsafe(CScriptString& func_name, int p0, int p1, int p2, CScriptString* p3, asIScriptArray* p4)
{
	DwordVec dw;
	if(p4) AssignScriptArrayInVector<DWORD>(dw,p4);
	Self->Net_SendRunScript(true,func_name.c_str(),p0,p1,p2,p3?p3->c_str():NULL,dw);
}

DWORD FOClient::SScriptFunc::Global_LoadSprite(CScriptString& spr_name, int path_index)
{
	if(path_index>=PATH_LIST_COUNT) SCRIPT_ERROR_R0("Invalid path index arg.");
	return Self->AnimLoad(spr_name.c_str(),path_index,RES_SCRIPT);
}

DWORD FOClient::SScriptFunc::Global_LoadSpriteHash(DWORD name_hash, BYTE dir)
{
	return Self->AnimLoad(name_hash,dir,RES_SCRIPT);
}

int FOClient::SScriptFunc::Global_GetSpriteWidth(DWORD spr_id, int spr_index)
{
	AnyFrames* anim=Self->AnimGetFrames(spr_id);
	if(!anim || spr_index>=anim->GetCnt()) return 0;
	SpriteInfo* si=Self->SprMngr.GetSpriteInfo(spr_index<0?anim->GetCurSprId():anim->GetSprId(spr_index));
	if(!si) return 0;
	return si->Width;
}

int FOClient::SScriptFunc::Global_GetSpriteHeight(DWORD spr_id, int spr_index)
{
	AnyFrames* anim=Self->AnimGetFrames(spr_id);
	if(!anim || spr_index>=anim->GetCnt()) return 0;
	SpriteInfo* si=Self->SprMngr.GetSpriteInfo(spr_index<0?anim->GetCurSprId():anim->GetSprId(spr_index));
	if(!si) return 0;
	return si->Height;
}

DWORD FOClient::SScriptFunc::Global_GetSpriteCount(DWORD spr_id)
{
	AnyFrames* anim=Self->AnimGetFrames(spr_id);
	return anim?anim->CntFrm:0;
}

void FOClient::SScriptFunc::Global_DrawSprite(DWORD spr_id, int spr_index, int x, int y, DWORD color)
{
	if(!SpritesCanDraw || !spr_id) return;
	AnyFrames* anim=Self->AnimGetFrames(spr_id);
	if(!anim || spr_index>=anim->GetCnt()) return;
	Self->SprMngr.DrawSprite(spr_index<0?anim->GetCurSprId():anim->GetSprId(spr_index),x,y,color);
}

void FOClient::SScriptFunc::Global_DrawSpriteSize(DWORD spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, DWORD color)
{
	if(!SpritesCanDraw || !spr_id) return;
	AnyFrames* anim=Self->AnimGetFrames(spr_id);
	if(!anim || spr_index>=anim->GetCnt()) return;
	Self->SprMngr.DrawSpriteSize(spr_index<0?anim->GetCurSprId():anim->GetSprId(spr_index),x,y,w,h,scratch,true,color);
}

void FOClient::SScriptFunc::Global_DrawText(CScriptString& text, int x, int y, int w, int h, DWORD color, int font, int flags)
{
	if(!SpritesCanDraw) return;
	Self->SprMngr.DrawStr(INTRECT(x,y,x+w,y+h),text.c_str(),flags,color,font);
}

void FOClient::SScriptFunc::Global_DrawPrimitive(int primitive_type, asIScriptArray& data)
{
	if(!SpritesCanDraw) return;

	D3DPRIMITIVETYPE prim;
	switch(primitive_type)
	{
	case 0: prim=D3DPT_POINTLIST; break;
	case 1: prim=D3DPT_LINELIST; break;
	case 2: prim=D3DPT_LINESTRIP; break;
	case 3: prim=D3DPT_TRIANGLELIST; break;
	case 4: prim=D3DPT_TRIANGLESTRIP; break;
	case 5: prim=D3DPT_TRIANGLEFAN; break;
	default: return;
	}

	int size=data.GetElementCount()/3;
	PointVec points;
	points.resize(size);

	for(int i=0;i<size;i++)
	{
		PrepPoint& pp=points[i];
		pp.PointX=*(int*)data.GetElementPointer(i*3);
		pp.PointY=*(int*)data.GetElementPointer(i*3+1);
		pp.PointColor=*(int*)data.GetElementPointer(i*3+2);
		//pp.PointOffsX=NULL;
		//pp.PointOffsY=NULL;
	}

	Self->SprMngr.DrawPoints(points,prim);
}

void FOClient::SScriptFunc::Global_DrawMapSprite(WORD hx, WORD hy, WORD proto_id, DWORD spr_id, int spr_index, int ox, int oy)
{
	if(!Self->HexMngr.SpritesCanDrawMap) return;
	if(!Self->HexMngr.GetHexToDraw(hx,hy)) return;

	AnyFrames* anim=Self->AnimGetFrames(spr_id);
	if(!anim || spr_index>=anim->GetCnt()) return;

	ProtoItem* proto_item=ItemMngr.GetProtoItem(proto_id);
	DWORD pos=HEX_POS(hx,hy);
	bool is_flat=(proto_item?FLAG(proto_item->Flags,ITEM_FLAT):false);
	bool is_scen=(proto_item?proto_item->IsScen() || proto_item->IsGrid():false);
	bool no_light=(is_flat && is_scen);

	Field& f=Self->HexMngr.GetField(hx,hy);
	Sprites& tree=Self->HexMngr.GetDrawTree();
	Sprite& spr=tree.InsertSprite(is_flat?DRAW_ORDER_ITEM_FLAT(is_scen):DRAW_ORDER_ITEM(pos),
		f.ScrX+16+ox,f.ScrY+6+oy,spr_index<0?anim->GetCurSprId():anim->GetSprId(spr_index),
		NULL,NULL,NULL,NULL,no_light?NULL:Self->HexMngr.GetLightHex(hx,hy),NULL);

	if(proto_item) // Copy from void ItemHex::SetEffects(Sprite* prep);
	{
		if(FLAG(proto_item->Flags,ITEM_COLORIZE))
		{
			spr.Alpha=((BYTE*)&proto_item->LightColor)+3;
			spr.Color=proto_item->LightColor&0xFFFFFF;
		}

		if(is_flat || proto_item->DisableEgg) spr.Egg=Sprite::EggNone;
		else
		{
			switch(proto_item->Corner)
			{
			case CORNER_SOUTH: spr.Egg=Sprite::EggXorY;
			case CORNER_NORTH: spr.Egg=Sprite::EggXandY;
			case CORNER_EAST_WEST:
			case CORNER_WEST: spr.Egg=Sprite::EggY;
			default: spr.Egg=Sprite::EggX; // CORNER_NORTH_SOUTH, CORNER_EAST
			}
		}

		if(proto_item->Flags&ITEM_BAD_ITEM) spr.Contour=Sprite::ContourRed;
	}
}

void FOClient::SScriptFunc::Global_DrawCritter2d(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir, int l, int t, int r, int b, bool scratch, bool center, DWORD color)
{
	if(CritType::IsEnabled(crtype))
	{
		AnyFrames* frm=CritterCl::LoadAnim(crtype,anim1,anim2,dir);
		if(frm) Self->SprMngr.DrawSpriteSize(frm->Ind[0],l,t,r-l,b-t,scratch,center,color?color:COLOR_IFACE);
	}
}

Animation3dVec DrawCritter3dAnim;
DwordVec DrawCritter3dCrType;
DwordVec DrawCritter3dFailToLoad;
int DrawCritter3dLayers[LAYERS3D_COUNT];
void FOClient::SScriptFunc::Global_DrawCritter3d(DWORD instance, DWORD crtype, DWORD anim1, DWORD anim2, asIScriptArray* layers, asIScriptArray* position, DWORD color)
{
	// x y
	// rx ry rz
	// sx sy sz
	// speed
	// stencil l t r b
	if(CritType::IsEnabled(crtype))
	{
		if(instance>=DrawCritter3dAnim.size())
		{
			DrawCritter3dAnim.resize(instance+1);
			DrawCritter3dCrType.resize(instance+1);
			DrawCritter3dFailToLoad.resize(instance+1);
		}

		if(DrawCritter3dFailToLoad[instance] && DrawCritter3dCrType[instance]==crtype) return;

		Animation3d*& anim=DrawCritter3dAnim[instance];
		if(!anim || DrawCritter3dCrType[instance]!=crtype)
		{
			SAFEDEL(anim);
			anim=Animation3d::GetAnimation(Str::Format("%s.fo3d",CritType::GetName(crtype)),PT_ART_CRITTERS,false);
			DrawCritter3dCrType[instance]=crtype;
			DrawCritter3dFailToLoad[instance]=0;

			if(!anim)
			{
				DrawCritter3dFailToLoad[instance]=1;
				return;
			}
			anim->EnableShadow(false);
		}

		if(anim)
		{
			DWORD count=(position?position->GetElementCount():0);
			float x=(count>0?*(float*)position->GetElementPointer(0):0.0f);
			float y=(count>1?*(float*)position->GetElementPointer(1):0.0f);
			float rx=(count>2?*(float*)position->GetElementPointer(2):0.0f);
			float ry=(count>3?*(float*)position->GetElementPointer(3):0.0f);
			float rz=(count>4?*(float*)position->GetElementPointer(4):0.0f);
			float sx=(count>5?*(float*)position->GetElementPointer(5):1.0f);
			float sy=(count>6?*(float*)position->GetElementPointer(6):1.0f);
			float sz=(count>7?*(float*)position->GetElementPointer(7):1.0f);
			float speed=(count>8?*(float*)position->GetElementPointer(8):1.0f);
			// 9 reserved
			float stl=(count>10?*(float*)position->GetElementPointer(10):0.0f);
			float stt=(count>11?*(float*)position->GetElementPointer(11):0.0f);
			float str=(count>12?*(float*)position->GetElementPointer(12):0.0f);
			float stb=(count>13?*(float*)position->GetElementPointer(13):0.0f);

			ZeroMemory(DrawCritter3dLayers,sizeof(DrawCritter3dLayers));
			for(DWORD i=0,j=(layers?layers->GetElementCount():0);i<j && i<LAYERS3D_COUNT;i++)
				DrawCritter3dLayers[i]=*(int*)layers->GetElementPointer(i);

			anim->SetRotation(rx*D3DX_PI/180.0f,ry*D3DX_PI/180.0f,rz*D3DX_PI/180.0f);
			anim->SetScale(sx,sy,sz);
			anim->SetSpeed(speed);
			anim->SetAnimation(anim1,anim2,DrawCritter3dLayers,0);
			Self->SprMngr.Draw3d(x,y,1.0f,anim,stl<str && stt<stb?&FLTRECT(stl,stt,str,stb):NULL,color?color:COLOR_IFACE);
		}
	}
}

void FOClient::SScriptFunc::Global_ShowScreen(int screen, int p0, int p1, int p2)
{
	Self->ShowScreen(screen,p0,p1,p2);
}

void FOClient::SScriptFunc::Global_HideScreen(int screen, int p0, int p1, int p2)
{
	if(screen) Self->HideScreen(screen,p0,p1,p2);
	else Self->ShowScreen(SCREEN_NONE,p0,p1,p2);
}

void FOClient::SScriptFunc::Global_GetHardcodedScreenPos(int screen, int& x, int& y)
{
	switch(screen)
	{
	case SCREEN_LOGIN: x=Self->LogX; y=Self->LogY; break;
	case SCREEN_REGISTRATION: x=Self->ChaX; y=Self->ChaY; break;
	case SCREEN_GAME: x=Self->IntX; y=Self->IntY; break;
	case SCREEN_GLOBAL_MAP: x=Self->GmapX; y=Self->GmapY; break;
	case SCREEN_WAIT: x=0; y=0; break;
	case SCREEN__INVENTORY: x=Self->InvX; y=Self->InvY; break;
	case SCREEN__PICKUP: x=Self->PupX; y=Self->PupY; break;
	case SCREEN__MINI_MAP: x=Self->LmapX; y=Self->LmapY; break;
	case SCREEN__CHARACTER: x=Self->ChaX; y=Self->ChaY; break;
	case SCREEN__DIALOG: x=Self->DlgX; y=Self->DlgY; break;
	case SCREEN__BARTER: x=Self->DlgX; y=Self->DlgY; break;
	case SCREEN__PIP_BOY: x=Self->PipX; y=Self->PipY; break;
	case SCREEN__FIX_BOY: x=Self->FixX; y=Self->FixY; break;
	case SCREEN__MENU_OPTION: x=Self->MoptX; y=Self->MoptY; break;
	case SCREEN__AIM: x=Self->AimX; y=Self->AimY; break;
	case SCREEN__SPLIT: x=Self->SplitX; y=Self->SplitY; break;
	case SCREEN__TIMER: x=Self->TimerX; y=Self->TimerY; break;
	case SCREEN__DIALOGBOX: x=Self->DlgboxX; y=Self->DlgboxY; break;
	case SCREEN__ELEVATOR: x=Self->ElevatorX; y=Self->ElevatorY; break;
	case SCREEN__SAY: x=Self->SayX; y=Self->SayY; break;
	case SCREEN__CHA_NAME: x=Self->ChaNameX; y=Self->ChaNameY; break;
	case SCREEN__CHA_AGE: x=Self->ChaAgeX; y=Self->ChaAgeY; break;
	case SCREEN__CHA_SEX: x=Self->ChaSexX; y=Self->ChaSexY; break;
	case SCREEN__GM_TOWN: x=0; y=0; break;
	case SCREEN__INPUT_BOX: x=Self->IboxX; y=Self->IboxY; break;
	case SCREEN__SKILLBOX: x=Self->SboxX; y=Self->SboxY; break;
	case SCREEN__USE: x=Self->UseX; y=Self->UseY; break;
	case SCREEN__PERK: x=Self->PerkX; y=Self->PerkY; break;
	case SCREEN__SAVE_LOAD: x=Self->SaveLoadX; y=Self->SaveLoadY; break;
	default: x=0; y=0; break;
	}
}

void FOClient::SScriptFunc::Global_DrawHardcodedScreen(int screen)
{
	switch(screen)
	{
	case SCREEN__INVENTORY:  Self->InvDraw(); break;
	case SCREEN__PICKUP:     Self->PupDraw(); break;
	case SCREEN__MINI_MAP:   Self->LmapDraw(); break;
	case SCREEN__DIALOG:     Self->DlgDraw(true); break;
	case SCREEN__BARTER:     Self->DlgDraw(false); break;
	case SCREEN__PIP_BOY:    Self->PipDraw(); break;
	case SCREEN__FIX_BOY:    Self->FixDraw(); break;
	case SCREEN__MENU_OPTION:Self->MoptDraw(); break;
	case SCREEN__CHARACTER:  Self->ChaDraw(false); break;
	case SCREEN__AIM:        Self->AimDraw(); break;
	case SCREEN__SPLIT:      Self->SplitDraw();break;
	case SCREEN__TIMER:      Self->TimerDraw();break;
	case SCREEN__DIALOGBOX:  Self->DlgboxDraw();break;
	case SCREEN__ELEVATOR:   Self->ElevatorDraw();break;
	case SCREEN__SAY:        Self->SayDraw();break;
	case SCREEN__CHA_NAME:   Self->ChaNameDraw();break;
	case SCREEN__CHA_AGE:    Self->ChaAgeDraw();break;
	case SCREEN__CHA_SEX:    Self->ChaSexDraw();break;
	case SCREEN__GM_TOWN:    Self->GmapTownDraw();break;
	case SCREEN__INPUT_BOX:  Self->IboxDraw(); break;
	case SCREEN__SKILLBOX:   Self->SboxDraw(); break;
	case SCREEN__USE:        Self->UseDraw(); break;
	case SCREEN__PERK:       Self->PerkDraw(); break;
	case SCREEN__TOWN_VIEW:  Self->TViewDraw(); break;
	case SCREEN__SAVE_LOAD:  Self->SaveLoadDraw(); break;
	default: break;
	}
}

bool FOClient::SScriptFunc::Global_GetHexPos(WORD hx, WORD hy, int& x, int& y)
{
	x=y=0;
	if(Self->HexMngr.IsMapLoaded() && hx<Self->HexMngr.GetMaxHexX() && hy<Self->HexMngr.GetMaxHexY())
	{
		Self->HexMngr.GetHexCurrentPosition(hx,hy,x,y);
		x+=GameOpt.ScrOx;
		y+=GameOpt.ScrOy;
		x/=GameOpt.SpritesZoom;
		y/=GameOpt.SpritesZoom;
		return true;
	}
	return false;
}

bool FOClient::SScriptFunc::Global_GetMonitorHex(int x, int y, WORD& hx, WORD& hy)
{
	if(Self->GetMouseHex())
	{
		hx=Self->TargetX;
		hy=Self->TargetY;
		return true;
	}
	return false;
}

void FOClient::SScriptFunc::Global_GetMousePosition(int& x, int& y)
{
	x=Self->CurX;
	y=Self->CurY;
}

Item* FOClient::SScriptFunc::Global_GetMonitorItem(int x, int y)
{
	if(Self->GetMouseHex())
	{
		ItemHex* item;
		CritterCl* cr;
		Self->HexMngr.GetSmthPixel(x,y,item,cr);
		return item;
	}
	return NULL;
}

CritterCl* FOClient::SScriptFunc::Global_GetMonitorCritter(int x, int y)
{
	if(Self->GetMouseHex())
	{
		ItemHex* item;
		CritterCl* cr;
		Self->HexMngr.GetSmthPixel(x,y,item,cr);
		return cr;
	}
	return NULL;
}

WORD FOClient::SScriptFunc::Global_GetMapWidth()
{
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_R0("Map is not loaded.");
	return Self->HexMngr.GetMaxHexX();
}

WORD FOClient::SScriptFunc::Global_GetMapHeight()
{
	if(!Self->HexMngr.IsMapLoaded()) SCRIPT_ERROR_R0("Map is not loaded.");
	return Self->HexMngr.GetMaxHexY();
}

int FOClient::SScriptFunc::Global_GetCurrentCursor()
{
	return Self->CurMode;
}

int FOClient::SScriptFunc::Global_GetLastCursor()
{
	return Self->CurModeLast;
}

void FOClient::SScriptFunc::Global_ChangeCursor(int cursor)
{
	Self->SetCurMode(cursor);
}

