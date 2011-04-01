#include "StdAfx.h"
#include "Server.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>
#include "FL/Fl.H"
#include "FL/Fl_Window.H"
#include "FL/Fl_Box.H"
#include "FL/Fl_Text_Display.H"
#include "FL/Fl_Button.H"
#include "FL/Fl_Check_Button.H"
#include "FL/Fl_File_Icon.H"

void GUIInit(IniParser& cfg);
void GUICallback(Fl_Widget* widget, void* data);
void UpdateInfo();
void UpdateLog();
void CheckTextBoxSize(bool force);
void* GameLoopThread(void*);
INTRECT MainInitRect,LogInitRect,InfoInitRect;
int SplitProcent=90;
Thread LoopThread;
MutexEvent GameInitEvent;
FOServer Serv;
string UpdateLogName;

// GUI
Fl_Window* GuiWindow;
Fl_Box* GuiLabelGameTime,*GuiLabelClients,*GuiLabelIngame,*GuiLabelNPC,*GuiLabelLocCount,
	*GuiLabelItemsCount,*GuiLabelVarsCount,*GuiLabelAnyDataCount,*GuiLabelTECount,
	*GuiLabelFPS,*GuiLabelDelta,*GuiLabelUptime,*GuiLabelSend,*GuiLabelRecv,*GuiLabelCompress;
Fl_Button* GuiBtnRlClScript,*GuiBtnSaveWorld,*GuiBtnSaveLog,*GuiBtnSaveInfo,
	*GuiBtnCreateDump,*GuiBtnMemory,*GuiBtnPlayers,*GuiBtnLocsMaps,*GuiBtnTimeEvents,
	*GuiBtnAnyData,*GuiBtnItemsCount,*GuiBtnStartStop,*GuiBtnSplitUp,*GuiBtnSplitDown;
Fl_Check_Button* GuiCBtnScriptDebug,*GuiCBtnLogging,*GuiCBtnLoggingTime,
	*GuiCBtnLoggingThread,*GuiCBtnAutoUpdate;
Fl_Text_Display *GuiLog,*GuiInfo;
int GUISizeMod=0;

#define GUI_SIZE1(x)           ((int)(x)*175*(100+GUISizeMod)/100/100)
#define GUI_SIZE2(x1,x2)       GUI_SIZE1(x1),GUI_SIZE1(x2)
#define GUI_SIZE4(x1,x2,x3,x4) GUI_SIZE1(x1),GUI_SIZE1(x2),GUI_SIZE1(x3),GUI_SIZE1(x4)
#define GUI_LABEL_BUF_SIZE     (128)

// Windows service
#if defined(FO_WINDOWS)
	void ServiceMain(bool as_service);
#endif

// Main
int main(int argc, char** argv)
{
	setlocale(LC_ALL,"Russian");
	RestoreMainDirectory();

	// Pthreads
#if defined(FO_WINDOWS)
	pthread_win32_process_attach_np();
#endif

	// Exceptions catcher
	CatchExceptions("FOnlineServer",SERVER_VERSION);

	// Timer
	Timer::Init();

	// Config
	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);

	// Make command line
	SetCommandLine(argc,argv);

	// Logging
	LogSetThreadName("GUI");
	LogWithTime(cfg.GetInt("LoggingTime",1)==0?false:true);
	LogWithThread(cfg.GetInt("LoggingThread",1)==0?false:true);
	if(strstr(CommandLine,"-logdebugoutput") || cfg.GetInt("LoggingDebugOutput",0)!=0) LogToDebugOutput();

	// Init event
	GameInitEvent.Disallow();

	// Service
	if(strstr(CommandLine,"-service"))
	{
#if defined(FO_WINDOWS)
		ServiceMain(strstr(CommandLine,"--service")!=NULL);
#endif
		return 0;
	}

	// Check single player parameters
	if(strstr(CommandLine,"-singleplayer "))
	{
#if defined(FO_WINDOWS)
		Singleplayer=true;
		Timer::SetGamePause(true);

		// Logging
		char log_path[MAX_FOPATH]={0};
		if(!strstr(CommandLine,"-nologpath") && strstr(CommandLine,"-logpath "))
		{
			const char* ptr=strstr(CommandLine,"-logpath ")+Str::Length("-logpath ");
			Str::Copy(log_path,ptr);
		}
		Str::Append(log_path,"FOnlineServer.log");
		LogToFile(log_path);

		WriteLog("Singleplayer mode.\n");

		// Shared data
		const char* ptr=strstr(CommandLine,"-singleplayer ")+Str::Length("-singleplayer ");
		HANDLE map_file=NULL;
		if(sscanf(ptr,"%p%p",&map_file,&SingleplayerClientProcess)!=2 || !SingleplayerData.Attach(map_file))
		{
			WriteLog("Can't attach to mapped file<%p>.\n",map_file);
			return 0;
		}
#else
		return 0;
#endif
	}

	// GUI
	if(!Singleplayer || strstr(CommandLine,"-showgui"))
	{
		Fl::lock(); // Begin GUI multi threading
		GUIInit(cfg);
		LogFinish(LOG_FILE);
		LogToBuffer(&GuiMsg_UpdateLog);
	}

	WriteLog("FOnline server, version %04X-%02X.\n",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);

	FOQuit=true;

	MemoryDebugLevel=cfg.GetInt("MemoryDebugLevel",0);
	Script::SetLogDebugInfo(true);

	if(GuiWindow)
	{
		GuiCBtnAutoUpdate->value(0);
		GuiCBtnLogging->value(cfg.GetInt("Logging",1)!=0?1:0);
		GuiCBtnLoggingTime->value(cfg.GetInt("LoggingTime",1)!=0?1:0);
		GuiCBtnLoggingThread->value(cfg.GetInt("LoggingThread",1)!=0?1:0);
		GuiCBtnScriptDebug->value(1);
	}

	// Command line
	if(CommandLineArgCount>1) WriteLog("Command line<%s>.\n",CommandLine);

	// Autostart
	if(strstr(CommandLine,"-start") || Singleplayer)
	{
		if(GuiWindow)
		{
			GuiBtnStartStop->do_callback();
		}
		else
		{
			FOQuit=false;
			LoopThread.Start(GameLoopThread);
		}
	}

	// Loop
	SyncManager* sync_mngr=SyncManager::GetForCurThread();
	sync_mngr->UnlockAll();
	if(GuiWindow)
	{
 		while(Fl::wait())
 		{
			void* pmsg=Fl::thread_message();
			if(pmsg)
			{
				int msg=*(int*)pmsg;
				if(msg==GuiMsg_UpdateInfo) UpdateInfo();
				else if(msg==GuiMsg_UpdateLog) UpdateLog();
			}
			CheckTextBoxSize(false);
 		}
	}
	else
	{
		while(!FOQuit) Sleep(100);
	}

	return 0;
}

void GUIInit(IniParser& cfg)
{
	// Setup
	struct
	{
		int FontType;
		int FontSize;

		void Setup(Fl_Box* widget)
		{
			widget->labelfont(FontType);
			widget->labelsize(FontSize);
			widget->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
			widget->box(FL_NO_BOX);
			widget->label(new char[GUI_LABEL_BUF_SIZE]);
			*(char*)widget->label()=0;
		}

		void Setup(Fl_Button* widget)
		{
			widget->labelfont(FontType);
			widget->labelsize(FontSize);
			widget->callback(GUICallback);
		}

		void Setup(Fl_Check_Button* widget)
		{
			widget->labelfont(FontType);
			widget->labelsize(FontSize);
			widget->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
			widget->callback(GUICallback);
		}

		void Setup(Fl_Text_Display* widget)
		{
			widget->labelfont(FontType);
			widget->labelsize(FontSize);
			widget->buffer(new Fl_Text_Buffer());
			widget->textfont(FontType);
			widget->textsize(FontSize);
		}
	} GUISetup;

	GUISizeMod=cfg.GetInt("GUISize",0);
	GUISetup.FontType=FL_COURIER;
	GUISetup.FontSize=11;

	// Main window
	int wx=cfg.GetInt("PositionX",0);
	int wy=cfg.GetInt("PositionY",0);
	if(!wx && !wy) wx=(Fl::w()-GUI_SIZE1(496))/2,wy=(Fl::h()-GUI_SIZE1(396))/2;
	GuiWindow=new Fl_Window(wx,wy,GUI_SIZE2(496,396),"FOnline Server");
	GuiWindow->labelfont(GUISetup.FontType);
	GuiWindow->labelsize(GUISetup.FontSize);
	GuiWindow->callback(GUICallback);
	GuiWindow->size_range(GUI_SIZE2(129,129));

	// Icon
#if defined(FO_WINDOWS)
	GuiWindow->icon((char*)LoadIcon(fl_display,MAKEINTRESOURCE(101)));
#else // FO_LINUX
	fl_open_display();
	// Todo: linux
#endif

	// Labels
	GUISetup.Setup(GuiLabelGameTime    =new Fl_Box(GUI_SIZE4(5,6,128,8),"Time:"));
	GUISetup.Setup(GuiLabelClients     =new Fl_Box(GUI_SIZE4(5,14,124,8),"Connections:"));
	GUISetup.Setup(GuiLabelIngame      =new Fl_Box(GUI_SIZE4(5,22,124,8),"Players in game:"));
	GUISetup.Setup(GuiLabelNPC         =new Fl_Box(GUI_SIZE4(5,30,124,8),"NPC in game:"));
	GUISetup.Setup(GuiLabelLocCount    =new Fl_Box(GUI_SIZE4(5,38,124,8),"Locations:"));
	GUISetup.Setup(GuiLabelItemsCount  =new Fl_Box(GUI_SIZE4(5,46,124,8),"Items:"));
	GUISetup.Setup(GuiLabelVarsCount   =new Fl_Box(GUI_SIZE4(5,54,124,8),"Vars:"));
	GUISetup.Setup(GuiLabelAnyDataCount=new Fl_Box(GUI_SIZE4(5,62,124,8),"Any data:"));
	GUISetup.Setup(GuiLabelTECount     =new Fl_Box(GUI_SIZE4(5,70,124,8),"Time events:"));
	GUISetup.Setup(GuiLabelFPS         =new Fl_Box(GUI_SIZE4(5,78,124,8),"Cycles per second:"));
	GUISetup.Setup(GuiLabelDelta       =new Fl_Box(GUI_SIZE4(5,86,124,8),"Cycle time:"));
	GUISetup.Setup(GuiLabelUptime      =new Fl_Box(GUI_SIZE4(5,94,124,8),"Uptime:"));
	GUISetup.Setup(GuiLabelSend        =new Fl_Box(GUI_SIZE4(5,102,124,8),"KBytes send:"));
	GUISetup.Setup(GuiLabelRecv        =new Fl_Box(GUI_SIZE4(5,110,124,8),"KBytes recv:"));
	GUISetup.Setup(GuiLabelCompress    =new Fl_Box(GUI_SIZE4(5,118,124,8),"Compress ratio:"));

	// Buttons
	GUISetup.Setup(GuiBtnRlClScript=new Fl_Button(GUI_SIZE4(5,128,124,14),"Reload client scripts"));
	GUISetup.Setup(GuiBtnSaveWorld =new Fl_Button(GUI_SIZE4(5,144,124,14),"Save world"));
	GUISetup.Setup(GuiBtnSaveLog   =new Fl_Button(GUI_SIZE4(5,160,124,14),"Save log"));
	GUISetup.Setup(GuiBtnSaveInfo  =new Fl_Button(GUI_SIZE4(5,176,124,14),"Save info"));
	GUISetup.Setup(GuiBtnCreateDump=new Fl_Button(GUI_SIZE4(5,192,124,14),"Create dump"));
	GUISetup.Setup(GuiBtnMemory    =new Fl_Button(GUI_SIZE4(5,219,124,14),"Memory usage"));
	GUISetup.Setup(GuiBtnPlayers   =new Fl_Button(GUI_SIZE4(5,235,124,14),"Players"));
	GUISetup.Setup(GuiBtnLocsMaps  =new Fl_Button(GUI_SIZE4(5,251,124,14),"Locations and maps"));
	GUISetup.Setup(GuiBtnTimeEvents=new Fl_Button(GUI_SIZE4(5,267,124,14),"Time events"));
	GUISetup.Setup(GuiBtnAnyData   =new Fl_Button(GUI_SIZE4(5,283,124,14),"Any data"));
	GUISetup.Setup(GuiBtnItemsCount=new Fl_Button(GUI_SIZE4(5,299,124,14),"Items count"));
	GUISetup.Setup(GuiBtnStartStop =new Fl_Button(GUI_SIZE4(5,377,124,14),"Start server"));
	GUISetup.Setup(GuiBtnSplitUp   =new Fl_Button(GUI_SIZE4(117,341,12,9),""));
	GUISetup.Setup(GuiBtnSplitDown =new Fl_Button(GUI_SIZE4(117,352,12,9),""));

	// Check buttons
	GUISetup.Setup(GuiCBtnScriptDebug  =new Fl_Check_Button(GUI_SIZE4(5,361,110,10),"Update info every second"));
	GUISetup.Setup(GuiCBtnLogging      =new Fl_Check_Button(GUI_SIZE4(5,333,110,10),"Logging"));
	GUISetup.Setup(GuiCBtnLoggingTime  =new Fl_Check_Button(GUI_SIZE4(5,342,110,10),"Logging with time"));
	GUISetup.Setup(GuiCBtnLoggingThread=new Fl_Check_Button(GUI_SIZE4(5,352,110,10),"Logging with thread"));
	GUISetup.Setup(GuiCBtnAutoUpdate   =new Fl_Check_Button(GUI_SIZE4(5,323,110,10),"Script debug info"));

	// Text boxes
	GUISetup.Setup(GuiLog =new Fl_Text_Display(GUI_SIZE4(133,7,358,191)));
	GUISetup.Setup(GuiInfo=new Fl_Text_Display(GUI_SIZE4(133,200,358,191)));

	// Disable buttons
	GuiBtnRlClScript->deactivate();
	GuiBtnSaveWorld->deactivate();
	GuiBtnPlayers->deactivate();
	GuiBtnLocsMaps->deactivate();
	GuiBtnTimeEvents->deactivate();
	GuiBtnAnyData->deactivate();
	GuiBtnItemsCount->deactivate();
	GuiBtnSaveInfo->deactivate();

	// Give initial focus to Start / Stop
	GuiBtnStartStop->take_focus();

	// Info
	MainInitRect(GuiWindow->x(),GuiWindow->y(),GuiWindow->x()+GuiWindow->w(),GuiWindow->y()+GuiWindow->h());
	LogInitRect(GuiLog->x(),GuiLog->y(),GuiLog->x()+GuiLog->w(),GuiLog->y()+GuiLog->h());
	InfoInitRect(GuiInfo->x(),GuiInfo->y(),GuiInfo->x()+GuiInfo->w(),GuiInfo->y()+GuiInfo->h());
	UpdateInfo();

	// Show window
	GuiWindow->show(CommandLineArgCount,CommandLineArgValues);
}

void GUICallback(Fl_Widget* widget, void* data)
{
	if(widget==GuiWindow)
	{
		ExitProcess(0);
	}
	else if(widget==GuiBtnRlClScript)
	{
		if(Serv.IsInit()) Serv.RequestReloadClientScripts=true;
	}
	else if(widget==GuiBtnSaveWorld)
	{
		if(Serv.IsInit()) Serv.SaveWorldNextTick=Timer::FastTick(); // Force saving time
	}
	else if(widget==GuiBtnSaveLog || widget==GuiBtnSaveInfo)
	{
		DateTime dt;
		Timer::GetCurrentDateTime(dt);
		char log_name[MAX_FOTEXT];
		Fl_Text_Display* log=(widget==GuiBtnSaveLog?GuiLog:GuiInfo);
		log->buffer()->savefile(Str::Format(log_name,"FOnlineServer_%s_%02u.%02u.%04u_%02u-%02u-%02u.log",
			log==GuiInfo?UpdateLogName.c_str():"Log",dt.Day,dt.Month,dt.Year,dt.Hour,dt.Minute,dt.Second));
	}
	else if(widget==GuiBtnCreateDump)
	{
		CreateDump("manual");
	}
	else if(widget==GuiBtnMemory)
	{
		FOServer::UpdateIndex=0;
		FOServer::UpdateLastIndex=0;
		if(!Serv.IsInit()) UpdateInfo();
	}
	else if(widget==GuiBtnPlayers)
	{
		FOServer::UpdateIndex=1;
		FOServer::UpdateLastIndex=1;
	}
	else if(widget==GuiBtnLocsMaps)
	{
		FOServer::UpdateIndex=2;
		FOServer::UpdateLastIndex=2;
	}
	else if(widget==GuiBtnTimeEvents)
	{
		FOServer::UpdateIndex=3;
		FOServer::UpdateLastIndex=3;
	}
	else if(widget==GuiBtnAnyData)
	{
		FOServer::UpdateIndex=4;
		FOServer::UpdateLastIndex=4;
	}
	else if(widget==GuiBtnItemsCount)
	{
		FOServer::UpdateIndex=5;
		FOServer::UpdateLastIndex=5;
	}
	else if(widget==GuiBtnStartStop)
	{
		if(!FOQuit) // End of work
		{
			FOQuit=true;
			GuiBtnStartStop->copy_label("Start server");
			GuiBtnStartStop->deactivate();

			// Disable buttons
			GuiBtnRlClScript->deactivate();
			GuiBtnSaveWorld->deactivate();
			GuiBtnPlayers->deactivate();
			GuiBtnLocsMaps->deactivate();
			GuiBtnTimeEvents->deactivate();
			GuiBtnAnyData->deactivate();
			GuiBtnItemsCount->deactivate();
		}
		else // Begin work
		{
			GuiBtnStartStop->copy_label("Stop server");
			GuiBtnStartStop->deactivate();

			FOQuit=false;
			LoopThread.Start(GameLoopThread);
		}
	}
	else if(widget==GuiBtnSplitUp)
	{
		if(SplitProcent>=50) SplitProcent-=40;
		CheckTextBoxSize(true);
		GuiLog->scroll(MAX_INT,0);
	}
	else if(widget==GuiBtnSplitDown)
	{
		if(SplitProcent<=50) SplitProcent+=40;
		CheckTextBoxSize(true);
		GuiLog->scroll(MAX_INT,0);
	}
	else if(widget==GuiCBtnScriptDebug)
	{
		Script::SetLogDebugInfo(GuiCBtnScriptDebug->value()?true:false);
	}
	else if(widget==GuiCBtnLogging)
	{
		if(GuiCBtnLogging->value())
			LogToBuffer(&GuiMsg_UpdateLog);
		else
			LogFinish(LOG_BUFFER);
	}
	else if(widget==GuiCBtnLoggingTime)
	{
		LogWithTime(GuiCBtnLogging->value()?true:false);
	}
	else if(widget==GuiCBtnLoggingThread)
	{
		LogWithThread(GuiCBtnLoggingThread->value()?true:false);
	}
	else if(widget==GuiCBtnAutoUpdate)
	{
		if(GuiCBtnAutoUpdate->value())
			FOServer::UpdateLastTick=Timer::FastTick();
		else
			FOServer::UpdateLastTick=0;
	}
}

void UpdateInfo()
{
	static char str[MAX_FOTEXT];
	static string std_str;

	struct Label
	{
		static void Update(Fl_Box* label, char* text)
		{
			if(!Str::Compare(text,(char*)label->label()))
			{
				Str::Copy((char*)label->label(),GUI_LABEL_BUF_SIZE,text);
				label->redraw_label();
			}
		}
	};

	if(Serv.IsInit())
	{
		DateTime st=Timer::GetGameTime(GameOpt.FullSecond);
		Label::Update(GuiLabelGameTime,Str::Format(str,"Time: %02u.%02u.%04u %02u:%02u:%02u x%u",st.Day,st.Month,st.Year,st.Hour,st.Minute,st.Second,GameOpt.TimeMultiplier));
		Label::Update(GuiLabelClients,Str::Format(str,"Connections: %u",Serv.Statistics.CurOnline));
		Label::Update(GuiLabelIngame,Str::Format(str,"Players in game: %u",Serv.PlayersInGame()));
		Label::Update(GuiLabelNPC,Str::Format(str,"NPC in game: %u",Serv.NpcInGame()));
		Label::Update(GuiLabelLocCount,Str::Format(str,"Locations: %u (%u)",MapMngr.GetLocationsCount(),MapMngr.GetMapsCount()));
		Label::Update(GuiLabelItemsCount,Str::Format(str,"Items: %u",ItemMngr.GetItemsCount()));
		Label::Update(GuiLabelVarsCount,Str::Format(str,"Vars: %u",VarMngr.GetVarsCount()));
		Label::Update(GuiLabelAnyDataCount,Str::Format(str,"Any data: %u",Serv.AnyData.size()));
		Label::Update(GuiLabelTECount,Str::Format(str,"Time events: %u",Serv.GetTimeEventsCount()));
		Label::Update(GuiLabelFPS,Str::Format(str,"Cycles per second: %u",Serv.Statistics.FPS));
		Label::Update(GuiLabelDelta,Str::Format(str,"Cycle time: %d",Serv.Statistics.CycleTime));
	}
	else
	{
		Label::Update(GuiLabelGameTime,Str::Format(str,"Time: n/a"));
		Label::Update(GuiLabelClients,Str::Format(str,"Connections: n/a"));
		Label::Update(GuiLabelIngame,Str::Format(str,"Players in game: n/a"));
		Label::Update(GuiLabelNPC,Str::Format(str,"NPC in game: n/a"));
		Label::Update(GuiLabelLocCount,Str::Format(str,"Locations: n/a"));
		Label::Update(GuiLabelItemsCount,Str::Format(str,"Items: n/a"));
		Label::Update(GuiLabelVarsCount,Str::Format(str,"Vars: n/a"));
		Label::Update(GuiLabelAnyDataCount,Str::Format(str,"Any data: n/a"));
		Label::Update(GuiLabelTECount,Str::Format(str,"Time events: n/a"));
		Label::Update(GuiLabelFPS,Str::Format(str,"Cycles per second: n/a"));
		Label::Update(GuiLabelDelta,Str::Format(str,"Cycle time: n/a"));
	}

	uint seconds=Serv.Statistics.Uptime;
	Label::Update(GuiLabelUptime,Str::Format(str,"Uptime: %2u:%2u:%2u",seconds/60/60,seconds/60%60,seconds%60));
	Label::Update(GuiLabelSend,Str::Format(str,"KBytes Send: %u",Serv.Statistics.BytesSend/1024));
	Label::Update(GuiLabelRecv,Str::Format(str,"KBytes Recv: %u",Serv.Statistics.BytesRecv/1024));
	Label::Update(GuiLabelCompress,Str::Format(str,"Compress ratio: %g",(double)Serv.Statistics.DataReal/(Serv.Statistics.DataCompressed?Serv.Statistics.DataCompressed:1)));

	if(FOServer::UpdateIndex==-1 && FOServer::UpdateLastTick && FOServer::UpdateLastTick+1000<Timer::FastTick())
	{
		FOServer::UpdateIndex=FOServer::UpdateLastIndex;
		FOServer::UpdateLastTick=Timer::FastTick();
	}

	if(FOServer::UpdateIndex!=-1)
	{
		switch(FOServer::UpdateIndex)
		{
		case 0: // Memory
			std_str=Debugger::GetMemoryStatistics();
			UpdateLogName="Memory";
			break;
		case 1: // Players
			if(!Serv.IsInit()) break;
			std_str=Serv.GetIngamePlayersStatistics();
			UpdateLogName="Players";
			break;
		case 2: // Locations and maps
			if(!Serv.IsInit()) break;
			std_str=MapMngr.GetLocationsMapsStatistics();
			UpdateLogName="LocationsAndMaps";
			break;
		case 3: // Time events
			if(!Serv.IsInit()) break;
			std_str=Serv.GetTimeEventsStatistics();
			UpdateLogName="TimeEvents";
			break;
		case 4: // Any data
			if(!Serv.IsInit()) break;
			std_str=Serv.GetAnyDataStatistics();
			UpdateLogName="AnyData";
			break;
		case 5: // Items count
			if(!Serv.IsInit()) break;
			std_str=ItemMngr.GetItemsStatistics();
			UpdateLogName="ItemsCount";
			break;
		default:
			UpdateLogName="";
			break;
		}
		GuiInfo->buffer()->text(std_str.c_str());
		if(!GuiBtnSaveInfo->active()) GuiBtnSaveInfo->activate();
		FOServer::UpdateIndex=-1;
	}
}

void UpdateLog()
{
	string str;
	LogGetBuffer(str);
	if(str.length())
	{
		GuiLog->buffer()->append(str.c_str());
		if(Fl::focus()!=GuiLog) GuiLog->scroll(MAX_INT,0);
	}
}

void CheckTextBoxSize(bool force)
{
	static INTRECT last_rmain;
	if(force || GuiWindow->x()!=last_rmain[0] || GuiWindow->y()!=last_rmain[1] ||
		GuiWindow->x()+GuiWindow->w()!=last_rmain[2] || GuiWindow->y()+GuiWindow->h()!=last_rmain[3])
	{
		INTRECT rmain(GuiWindow->x(),GuiWindow->y(),GuiWindow->x()+GuiWindow->w(),GuiWindow->y()+GuiWindow->h());
		if(rmain.W()>0 && rmain.H()>0)
		{
			int wdiff=rmain.W()-MainInitRect.W();
			int hdiff=rmain.H()-MainInitRect.H();

			INTRECT rlog(GuiLog->x(),GuiLog->y(),GuiLog->x()+GuiLog->w(),GuiLog->y()+GuiLog->h());
			INTRECT rinfo(GuiInfo->x(),GuiInfo->y(),GuiInfo->x()+GuiInfo->w(),GuiInfo->y()+GuiInfo->h());

			int hall=LogInitRect.H()+InfoInitRect.H()+hdiff;
			int wlog=LogInitRect.W()+wdiff;
			int hlog=hall*SplitProcent/100;
			int winfo=InfoInitRect.W()+wdiff;
			int hinfo=hall*(100-SplitProcent)/100;
			int yinfo=hlog-LogInitRect.H();

			GuiLog->position(LogInitRect.L,LogInitRect.T);
			GuiLog->size(wlog,hlog);
			GuiInfo->position(InfoInitRect.L,InfoInitRect.T+yinfo);
			GuiInfo->size(winfo,hinfo);
			GuiLog->redraw();
			GuiInfo->redraw();
			GuiWindow->redraw();
		}
		last_rmain=rmain;
	}
}

void* GameLoopThread(void*)
{
	LogSetThreadName("Main");

	GetServerOptions();

	if(Serv.Init())
	{
		if(GuiWindow)
		{
			if(GuiCBtnLogging->value()==0) LogFinish(LOG_TEXT_BOX);

			// Enable buttons
			GuiBtnRlClScript->activate();
			GuiBtnSaveWorld->activate();
			GuiBtnPlayers->activate();
			GuiBtnLocsMaps->activate();
			GuiBtnTimeEvents->activate();
			GuiBtnAnyData->activate();
			GuiBtnItemsCount->activate();
			GuiBtnStartStop->activate();
		}

		GameInitEvent.Allow();

		Serv.MainLoop();
		Serv.Finish();
		UpdateInfo();
	}
	else
	{
		WriteLog("Initialization fail!\n");
		GameInitEvent.Allow();
	}

	if(GuiWindow) UpdateLog();
	LogFinish(-1);
	if(Singleplayer) ExitProcess(0);
	return NULL;
}

/************************************************************************/
/* Windows service                                                      */
/************************************************************************/
#ifdef FO_WINDOWS

SERVICE_STATUS_HANDLE FOServiceStatusHandle;
VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv);
VOID WINAPI FOServiceCtrlHandler(DWORD opcode);
void SetFOServiceStatus(uint state);

void ServiceMain(bool as_service)
{
	// Binary started as service
	if(as_service)
	{
		// Start
		SERVICE_TABLE_ENTRY dispatch_table[]={{Str::Duplicate("FOnlineServer"),FOServiceStart},{NULL,NULL}};
		StartServiceCtrlDispatcher(dispatch_table);
		return;
	}

	// Open service manager
	SC_HANDLE manager=OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if(!manager)
	{
		MessageBox(NULL,"Can't open service manager.","FOnlineServer",MB_OK|MB_ICONHAND);
		return;
	}

	// Delete service
	if(strstr(CommandLine,"-delete"))
	{
		SC_HANDLE service=OpenService(manager,"FOnlineServer",DELETE);

		if(service && DeleteService(service))
			MessageBox(NULL,"Service deleted.","FOnlineServer",MB_OK|MB_ICONASTERISK);
		else
			MessageBox(NULL,"Can't delete service.","FOnlineServer",MB_OK|MB_ICONHAND);

		CloseServiceHandle(service);
		CloseServiceHandle(manager);
		return;
	}

	// Manage service
	SC_HANDLE service=OpenService(manager,"FOnlineServer",SERVICE_QUERY_CONFIG|SERVICE_CHANGE_CONFIG|SERVICE_QUERY_STATUS|SERVICE_START);

	// Compile service path
	char path1[MAX_FOPATH];
	GetModuleFileName(GetModuleHandle(NULL),path1,MAX_FOPATH);
	char path2[MAX_FOPATH];
	Str::Format(path2,"\"%s\" --service",path1);

	// Change executable path, if changed
	if(service)
	{
		LPQUERY_SERVICE_CONFIG service_cfg=(LPQUERY_SERVICE_CONFIG)calloc(8192,1);
		DWORD dw;
		if(QueryServiceConfig(service,service_cfg,8192,&dw) && !Str::CompareCase(service_cfg->lpBinaryPathName,path2))
			ChangeServiceConfig(service,SERVICE_NO_CHANGE,SERVICE_NO_CHANGE,SERVICE_NO_CHANGE,path2,NULL,NULL,NULL,NULL,NULL,NULL);
		free(service_cfg);
	}

	// Register service
	if(!service)
	{
		service=CreateService(manager,"FOnlineServer","FOnlineServer",SERVICE_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,path2,NULL,NULL,NULL,NULL,NULL);

		if(service)
			MessageBox(NULL,"\'FOnlineServer\' service registered.","FOnlineServer",MB_OK|MB_ICONASTERISK);
		else
			MessageBox(NULL,"Can't register \'FOnlineServer\' service.","FOnlineServer",MB_OK|MB_ICONHAND);
	}
	// Start service
	else
	{
		SERVICE_STATUS status;
		if(service && QueryServiceStatus(service,&status) && status.dwCurrentState!=SERVICE_STOPPED)
			MessageBox(NULL,"Service already running.","FOnlineServer",MB_OK|MB_ICONASTERISK);
		else if(service && !StartService(service,0,NULL))
			MessageBox(NULL,"Can't start service.","FOnlineServer",MB_OK|MB_ICONHAND);
	}

	// Close handles
	if(service) CloseServiceHandle(service);
	if(manager) CloseServiceHandle(manager);
}

VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv)
{
	LogToFile("FOnlineServer.log");
	LogSetThreadName("Service");
	WriteLog("FOnline server service, version %04X-%02X.\n",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);

	FOServiceStatusHandle=RegisterServiceCtrlHandler("FOnlineServer",FOServiceCtrlHandler);
	if(!FOServiceStatusHandle) return;

	// Start game
	SetFOServiceStatus(SERVICE_START_PENDING);

	FOQuit=false;
	LoopThread.Start(GameLoopThread);
	GameInitEvent.Wait();

	if(Serv.IsInit()) SetFOServiceStatus(SERVICE_RUNNING);
	else SetFOServiceStatus(SERVICE_STOPPED);
}

VOID WINAPI FOServiceCtrlHandler(DWORD opcode)
{
	switch(opcode)
	{
	case SERVICE_CONTROL_STOP:
		SetFOServiceStatus(SERVICE_STOP_PENDING);
		FOQuit=true;

		LoopThread.Wait();
		SetFOServiceStatus(SERVICE_STOPPED);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		// Fall through to send current status
		break;
	default:
		break;
	}

	// Send current status
	SetFOServiceStatus(0);
}

void SetFOServiceStatus(uint state)
{
	static uint last_state=0;
	static uint check_point=0;

	if(!state) state=last_state;
	else last_state=state;

	SERVICE_STATUS srv_status;
	srv_status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
	srv_status.dwCurrentState=state;
	srv_status.dwWin32ExitCode=0;
	srv_status.dwServiceSpecificExitCode=0;
	srv_status.dwWaitHint=0;
	srv_status.dwCheckPoint=0;
	srv_status.dwControlsAccepted=0;

	if(state==SERVICE_RUNNING) srv_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	if(!(state==SERVICE_RUNNING || state==SERVICE_STOPPED)) srv_status.dwCheckPoint=++check_point;

	SetServiceStatus(FOServiceStatusHandle,&srv_status);
}

#endif // FO_WINDOWS
