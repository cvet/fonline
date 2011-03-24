#include "StdAfx.h"
#include "ResourceServer.h"
#include "Server.h"
#include "Exception.h"
#include "Version.h"
#include <iostream>
#include <process.h>
#include <locale.h>

unsigned int __stdcall GameLoopThread(void*);
int CALLBACK DlgProc(HWND dlg, UINT msg,WPARAM wparam, LPARAM lparam);
void UpdateInfo();
void UpdateLog();
void ResizeDialog();
void ServiceMain(bool as_service);
HINSTANCE Instance=NULL;
HWND Dlg=NULL;
RECT MainInitRect,LogInitRect,InfoInitRect;
int SplitProcent=50;
HANDLE LoopThreadHandle=NULL;
HANDLE GameInitEvent=NULL;
FOServer Serv;

int APIENTRY WinMain(HINSTANCE cur_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
	RestoreMainDirectory();
	setlocale(LC_ALL,"Russian");

	// Exceptions catcher
	CatchExceptions("FOnlineServer",SERVER_VERSION);

	// Timer
	Timer::Init();

	// Config
	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);

	// Logging
	LogSetThreadName("GUI");
	LogWithTime(cfg.GetInt("LoggingTime",1)==0?false:true);
	LogWithThread(cfg.GetInt("LoggingThread",1)==0?false:true);
	if(strstr(cmd_line,"-logdebugoutput") || cfg.GetInt("LoggingDebugOutput",0)!=0) LogToDebugOutput();

	// Init events
	GameInitEvent=CreateEvent(NULL,TRUE,FALSE,NULL);
	UpdateEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	LogEvent=CreateEvent(NULL,FALSE,FALSE,NULL);

	// Service
	if(strstr(cmd_line,"-service"))
	{
		ServiceMain(strstr(cmd_line,"--service")!=NULL);
		return 0;
	}

	// Check single player parameters
	if(strstr(cmd_line,"-singleplayer "))
	{
		Singleplayer=true;
		Timer::SetGamePause(true);

		// Logging
		char log_path[MAX_FOPATH]={0};
		if(!strstr(cmd_line,"-nologpath") && strstr(cmd_line,"-logpath "))
		{
			const char* ptr=strstr(cmd_line,"-logpath ")+strlen("-logpath ");
			StringCopy(log_path,ptr);
		}
		StringAppend(log_path,"FOnlineServer.log");
		LogToFile(log_path);

		WriteLog(NULL,"Singleplayer mode.\n");

		// Shared data
		const char* ptr=strstr(cmd_line,"-singleplayer ")+strlen("-singleplayer ");
		HANDLE map_file=NULL;
		if(sscanf(ptr,"%p%p",&map_file,&SingleplayerClientProcess)!=2 || !SingleplayerData.Attach(map_file))
		{
			WriteLog(NULL,"Can't attach to mapped file<%p>.\n",map_file);
			return 0;
		}
	}

	// Singleplayer
	if(!Singleplayer || strstr(cmd_line,"-showgui"))
	{
		LoadLibrary("RICHED32.dll");

		Dlg=CreateDialog(Instance,MAKEINTRESOURCE(IDD_DLG),NULL,DlgProc);

		LogFinish(LOG_FILE);
		LogToBuffer(&LogEvent);

		int wx=cfg.GetInt("PositionX",0);
		int wy=cfg.GetInt("PositionY",0);
		if(wx || wy) SetWindowPos(Dlg,0,wx,wy,0,0,SWP_NOZORDER|SWP_NOSIZE);

		char win_title[256];
		sprintf(win_title,"FOnline Server");
		SetWindowText(Dlg,win_title);

		// Disable buttons
		EnableWindow(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPTS),0);
		EnableWindow(GetDlgItem(Dlg,IDC_SAVE_WORLD),0);
		EnableWindow(GetDlgItem(Dlg,IDC_CLIENTS),0);
		EnableWindow(GetDlgItem(Dlg,IDC_MAPS),0);
		EnableWindow(GetDlgItem(Dlg,IDC_TIME_EVENTS),0);
		EnableWindow(GetDlgItem(Dlg,IDC_ANY_DATA),0);
		EnableWindow(GetDlgItem(Dlg,IDC_ITEMS_STAT),0);

		// Set font
		CHARFORMAT fmt;
		ZeroMemory(&fmt,sizeof(fmt));
		fmt.cbSize=sizeof(fmt);
		SendDlgItemMessage(Dlg,IDC_LOG,EM_GETCHARFORMAT,SCF_DEFAULT,(LPARAM)&fmt);
		cfg.GetStr("TextFont","Courier New",fmt.szFaceName);
		SendDlgItemMessage(Dlg,IDC_LOG,EM_SETCHARFORMAT,SCF_DEFAULT,(LPARAM)&fmt);
		SendDlgItemMessage(Dlg,IDC_INFO,EM_SETCHARFORMAT,SCF_DEFAULT,(LPARAM)&fmt);
		HFONT font=CreateFont(14,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,fmt.szFaceName);
		SendMessage(GetDlgItem(Dlg,IDC_GAMETIME),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_INGAME),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_NPC),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_ITEMS_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_LOC_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_VARS_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_ANYDATA_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_TE_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_FPS),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_DELTA),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_UPTIME),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_SEND),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_RECV),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_COMPRESS),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPTS),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_SAVE_WORLD),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_SAVELOG),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_SAVEINFO),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_DUMP),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_MEMORY),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_CLIENTS),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_MAPS),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_TIME_EVENTS),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_ANY_DATA),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_ITEMS_STAT),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_AUTOUPDATE),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING_TIME),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING_THREAD),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_SCRIPT_DEBUG),WM_SETFONT,(LPARAM)font,TRUE);
		SendMessage(GetDlgItem(Dlg,IDC_STOP),WM_SETFONT,(LPARAM)font,TRUE);

		WINDOWINFO wi;
		wi.cbSize=sizeof(wi);
		GetWindowInfo(Dlg,&wi);
		MainInitRect=wi.rcClient;
		GetWindowInfo(GetDlgItem(Dlg,IDC_LOG),&wi);
		LogInitRect=wi.rcClient;
		GetWindowInfo(GetDlgItem(Dlg,IDC_INFO),&wi);
		InfoInitRect=wi.rcClient;
		ResizeDialog();

		UpdateInfo();
		SetDlgItemText(Dlg,IDC_RELOAD_CLIENT_SCRIPTS,"Reload client scripts");
		SetDlgItemText(Dlg,IDC_SAVE_WORLD,"Save world");
		SetDlgItemText(Dlg,IDC_SAVELOG,"Save log");
		SetDlgItemText(Dlg,IDC_SAVEINFO,"Save info");
		SetDlgItemText(Dlg,IDC_DUMP,"Create dump");
		SetDlgItemText(Dlg,IDC_MEMORY,"Memory usage");
		SetDlgItemText(Dlg,IDC_CLIENTS,"Players");
		SetDlgItemText(Dlg,IDC_MAPS,"Locations and maps");
		SetDlgItemText(Dlg,IDC_TIME_EVENTS,"Time events");
		SetDlgItemText(Dlg,IDC_ANY_DATA,"Any data");
		SetDlgItemText(Dlg,IDC_ITEMS_STAT,"Items count");
		SetDlgItemText(Dlg,IDC_AUTOUPDATE,"Update info every second");
		SetDlgItemText(Dlg,IDC_LOGGING,"Logging");
		SetDlgItemText(Dlg,IDC_LOGGING_TIME,"Logging with time");
		SetDlgItemText(Dlg,IDC_LOGGING_THREAD,"Logging with thread");
		SetDlgItemText(Dlg,IDC_SCRIPT_DEBUG,"Script debug info");
		SetDlgItemText(Dlg,IDC_STOP,"Start server");
	}

	WriteLog(NULL,"FOnline server, version %04X-%02X.\n",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);

	FOQuit=true;
	Serv.ServerWindow=Dlg;

	MemoryDebugLevel=cfg.GetInt("MemoryDebugLevel",0);
	Script::SetLogDebugInfo(true);

	if(Dlg)
	{
		SendMessage(GetDlgItem(Dlg,IDC_AUTOUPDATE),BM_SETCHECK,BST_UNCHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING),BM_SETCHECK,cfg.GetInt("Logging",1)==0?BST_UNCHECKED:BST_CHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING_TIME),BM_SETCHECK,cfg.GetInt("LoggingTime",1)==0?BST_UNCHECKED:BST_CHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING_THREAD),BM_SETCHECK,cfg.GetInt("LoggingThread",1)==0?BST_UNCHECKED:BST_CHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_SCRIPT_DEBUG),BM_SETCHECK,BST_CHECKED,0);
	}

	// Command line
	if(cmd_line[0]) WriteLog(NULL,"Command line<%s>.\n",cmd_line);

	// Autostart
	if(strstr(cmd_line,"-start") || Singleplayer)
	{
		if(Dlg)
		{
			SendMessage(Dlg,WM_COMMAND,IDC_STOP,0);
		}
		else
		{
			FOQuit=false;
			LoopThreadHandle=(HANDLE)_beginthreadex(NULL,0,GameLoopThread,NULL,0,NULL);
		}
	}

	// Loop
	MSG msg;
	HANDLE events[2]={UpdateEvent,LogEvent};
	SyncManager* sync_mngr=SyncManager::GetForCurThread();
	sync_mngr->UnlockAll();
	while(!FOAppQuit)
	{
		uint result=MsgWaitForMultipleObjects(2,events,FALSE,10000,QS_ALLINPUT);
		if(result==WAIT_OBJECT_0) UpdateInfo();
		else if(result==WAIT_OBJECT_0+1) UpdateLog();
		else if(result==WAIT_OBJECT_0+2)
		{
			while(PeekMessage(&msg,NULL,NULL,NULL,PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if(result==WAIT_TIMEOUT)
		{
			UpdateInfo();
			UpdateLog();
		}
		else if(result==WAIT_FAILED)
		{
			WriteLog(NULL,"Wait failed on MsgWaitForMultipleObjects, error<%u>.\b",GetLastError());
		}

		//sync_mngr->UnlockAll();
	}

	//SAFEDEL(serv);
	//_CrtDumpMemoryLeaks();
	return 0;
}

void UpdateInfo()
{
	static char str[512];
	static string str_;

	if(Serv.IsInit())
	{
		SYSTEMTIME st=GetGameTime(GameOpt.FullSecond);
		sprintf(str,"Time: %02u.%02u.%04u %02u:%02u:%02u x%u",st.wDay,st.wMonth,st.wYear,st.wHour,st.wMinute,st.wSecond,GameOpt.TimeMultiplier);
		SetDlgItemText(Dlg,IDC_GAMETIME,str);
		sprintf(str,"Connections: %u",Serv.Statistics.CurOnline);
		SetDlgItemText(Dlg,IDC_COUNT,str);
		sprintf(str,"Players in game: %u",Serv.PlayersInGame());
		SetDlgItemText(Dlg,IDC_INGAME,str);
		sprintf(str,"NPC in game: %u",Serv.NpcInGame());
		SetDlgItemText(Dlg,IDC_NPC,str);
		sprintf(str,"Items: %u",ItemMngr.GetItemsCount());
		SetDlgItemText(Dlg,IDC_ITEMS_COUNT,str);
		sprintf(str,"Locations: %u (%u)",MapMngr.GetLocationsCount(),MapMngr.GetMapsCount());
		SetDlgItemText(Dlg,IDC_LOC_COUNT,str);
		sprintf(str,"Vars: %u",VarMngr.GetVarsCount());
		SetDlgItemText(Dlg,IDC_VARS_COUNT,str);
		sprintf(str,"Any data: %u",Serv.AnyData.size());
		SetDlgItemText(Dlg,IDC_ANYDATA_COUNT,str);
		sprintf(str,"Time events: %u",Serv.GetTimeEventsCount());
		SetDlgItemText(Dlg,IDC_TE_COUNT,str);
		sprintf(str,"Cycles per second: %u",Serv.Statistics.FPS);
		SetDlgItemText(Dlg,IDC_FPS,str);
		sprintf(str,"Cycle time: %d",Serv.Statistics.CycleTime);
		SetDlgItemText(Dlg,IDC_DELTA,str);
	}
	else
	{
		SetDlgItemText(Dlg,IDC_GAMETIME,"Time: n/a");
		SetDlgItemText(Dlg,IDC_COUNT,"Connections: n/a");
		SetDlgItemText(Dlg,IDC_INGAME,"Players in game: n/a");
		SetDlgItemText(Dlg,IDC_NPC,"NPC in game: n/a");
		SetDlgItemText(Dlg,IDC_ITEMS_COUNT,"Items: n/a");
		SetDlgItemText(Dlg,IDC_LOC_COUNT,"Locations: n/a");
		SetDlgItemText(Dlg,IDC_VARS_COUNT,"Vars: n/a");
		SetDlgItemText(Dlg,IDC_ANYDATA_COUNT,"Any data: n/a");
		SetDlgItemText(Dlg,IDC_TE_COUNT,"Time events: n/a");
		SetDlgItemText(Dlg,IDC_FPS,"Cycles per second: n/a");
		SetDlgItemText(Dlg,IDC_DELTA,"Cycle time: n/a");
	}

	uint seconds=Serv.Statistics.Uptime;
	sprintf(str,"Uptime: %2u:%2u:%2u",seconds/60/60,seconds/60%60,seconds%60);
	SetDlgItemText(Dlg,IDC_UPTIME,str);
	sprintf(str,"KBytes Send: %u",Serv.Statistics.BytesSend/1024);
	SetDlgItemText(Dlg,IDC_SEND,str);
	sprintf(str,"KBytes Recv: %u",Serv.Statistics.BytesRecv/1024);
	SetDlgItemText(Dlg,IDC_RECV,str);
	sprintf(str,"Compress ratio: %g",(double)Serv.Statistics.DataReal/(Serv.Statistics.DataCompressed?Serv.Statistics.DataCompressed:1));
	SetDlgItemText(Dlg,IDC_COMPRESS,str);

	if(FOServer::UpdateIndex==-1 && FOServer::UpdateLastTick && FOServer::UpdateLastTick+1000<Timer::FastTick())
	{
		FOServer::UpdateIndex=FOServer::UpdateLastIndex;
		FOServer::UpdateLastTick=Timer::FastTick();
	}

	switch(FOServer::UpdateIndex)
	{
	case 0: // Memory
		{
			str_=Debugger::GetMemoryStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 1: // Players
		if(Serv.IsInit())
		{
			str_=Serv.GetIngamePlayersStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 2: // Locations and maps
		if(Serv.IsInit())
		{
			str_=MapMngr.GetLocationsMapsStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 3: // Time events
		if(Serv.IsInit())
		{
			str_=Serv.GetTimeEventsStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 4: // Any data
		if(Serv.IsInit())
		{
			str_=Serv.GetAnyDataStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 5: // Items count
		if(Serv.IsInit())
		{
			str_=ItemMngr.GetItemsStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	default:
		break;
	}
	FOServer::UpdateIndex=-1;
}

void UpdateLog()
{
	static string str;
	LogGetBuffer(str);
	if(str.length())
	{
		SendDlgItemMessage(Dlg,IDC_LOG,EM_SETSEL,-1,-1);
		SendDlgItemMessage(Dlg,IDC_LOG,EM_REPLACESEL,0,(LPARAM)str.c_str());
	}
}

void ResizeDialog()
{
	WINDOWINFO wi;
	wi.cbSize=sizeof(wi);
	GetWindowInfo(Dlg,&wi);
	RECT rmain=wi.rcClient;

	if(rmain.right-rmain.left>0 && rmain.bottom-rmain.top>0)
	{
		int wdiff=(rmain.right-rmain.left)-(MainInitRect.right-MainInitRect.left);
		int hdiff=(rmain.bottom-rmain.top)-(MainInitRect.bottom-MainInitRect.top);

		GetWindowInfo(GetDlgItem(Dlg,IDC_LOG),&wi);
		RECT rlog=wi.rcClient;
		GetWindowInfo(GetDlgItem(Dlg,IDC_INFO),&wi);
		RECT rinfo=wi.rcClient;

		int hall=(LogInitRect.bottom-LogInitRect.top)+(InfoInitRect.bottom-InfoInitRect.top)+hdiff;
		int wlog=(LogInitRect.right-LogInitRect.left)+wdiff;
		int hlog=hall*SplitProcent/100;
		int winfo=(InfoInitRect.right-InfoInitRect.left)+wdiff;
		int hinfo=hall*(100-SplitProcent)/100;
		int yinfo=hlog-(LogInitRect.bottom-LogInitRect.top);

		SetWindowPos(GetDlgItem(Dlg,IDC_LOG),NULL,LogInitRect.left-MainInitRect.left,LogInitRect.top-MainInitRect.top,wlog,hlog,0);
		SetWindowPos(GetDlgItem(Dlg,IDC_INFO),NULL,InfoInitRect.left-MainInitRect.left,InfoInitRect.top-MainInitRect.top+yinfo,winfo,hinfo,0);
	}
}

int CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
	case WM_CLOSE:
		ExitProcess(0);
		return 0;
	case WM_INITDIALOG:
		PostMessage(Dlg,WM_SIZE,0,0);
		return 1;
	case WM_SIZE:
		ResizeDialog();
		return 0;
	case WM_LBUTTONDBLCLK:
		FOServer::UpdateIndex=FOServer::UpdateLastIndex;
		UpdateInfo();
		UpdateLog();
		break;
	case WM_COMMAND:
		switch(LOWORD(wparam))
		{
		case IDCANCEL:
			//FOQuit=true;
			FOAppQuit=true;
			CloseHandle(UpdateEvent);
			UpdateEvent=NULL;
			CloseHandle(LogEvent);
			LogEvent=NULL;
			CloseWindow(Dlg);
			break;
		case IDC_STOP:
			if(!FOQuit) // End of work
			{
				FOQuit=true;
				SetDlgItemText(Dlg,IDC_STOP,"Start server");
				EnableWindow(GetDlgItem(Dlg,IDC_STOP),0);
				SendMessage(GetDlgItem(Dlg,IDC_LOG),WM_SETFOCUS,0,0);

				// Disable buttons
				EnableWindow(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPTS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_SAVE_WORLD),0);
				EnableWindow(GetDlgItem(Dlg,IDC_CLIENTS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_MAPS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_TIME_EVENTS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_ANY_DATA),0);
				EnableWindow(GetDlgItem(Dlg,IDC_ITEMS_STAT),0);
			}
			else // Begin work
			{
				SetDlgItemText(Dlg,IDC_STOP,"Stop server");
				EnableWindow(GetDlgItem(Dlg,IDC_STOP),0);
				SendMessage(GetDlgItem(Dlg,IDC_LOG),WM_SETFOCUS,0,0);

				FOQuit=false;
				LoopThreadHandle=(HANDLE)_beginthreadex(NULL,0,GameLoopThread,NULL,0,NULL);
			}
			break;
		case IDC_MEMORY:
			FOServer::UpdateIndex=0;
			FOServer::UpdateLastIndex=0;
			if(!Serv.IsInit()) UpdateInfo();
			break;
		case IDC_CLIENTS:
			FOServer::UpdateIndex=1;
			FOServer::UpdateLastIndex=1;
			break;
		case IDC_MAPS:
			FOServer::UpdateIndex=2;
			FOServer::UpdateLastIndex=2;
			break;
		case IDC_TIME_EVENTS:
			FOServer::UpdateIndex=3;
			FOServer::UpdateLastIndex=3;
			break;
		case IDC_ANY_DATA:
			FOServer::UpdateIndex=4;
			FOServer::UpdateLastIndex=4;
			break;
		case IDC_ITEMS_STAT:
			FOServer::UpdateIndex=5;
			FOServer::UpdateLastIndex=5;
			break;
		case IDC_SAVELOG:
		case IDC_SAVEINFO:
			{
				ushort idc=(LOWORD(wparam)==IDC_SAVELOG?IDC_LOG:IDC_INFO);
				void* f=FileOpen(idc==IDC_LOG?"FOnlineServer.log":"FOnlineInfo.txt",true);
				if(f)
				{
					int len=GetWindowTextLength(GetDlgItem(Dlg,idc));
					char* str=new char[len+1];
					SendDlgItemMessage(Dlg,idc,EM_SETSEL,0,len);
					SendDlgItemMessage(Dlg,idc,EM_GETSELTEXT,0,(LPARAM)str);
					str[len]='\0';
					FileWrite(f,str,strlen(str));
					FileClose(f);
					delete[] str;
				}
			}
			break;
		case IDC_DUMP:
			CreateDump("manual");
			break;
		case IDC_AUTOUPDATE:
			if(SendMessage(GetDlgItem(Dlg,IDC_AUTOUPDATE),BM_GETCHECK,0,0)==BST_CHECKED)
				FOServer::UpdateLastTick=Timer::FastTick();
			else
				FOServer::UpdateLastTick=0;
			break;
		case IDC_LOGGING:
			if(SendMessage(GetDlgItem(Dlg,IDC_LOGGING),BM_GETCHECK,0,0)==BST_CHECKED)
				LogToBuffer(&LogEvent);
			else
				LogFinish(LOG_BUFFER);
			break;
		case IDC_LOGGING_TIME:
			LogWithTime(SendMessage(GetDlgItem(Dlg,IDC_LOGGING_TIME),BM_GETCHECK,0,0)==BST_CHECKED?true:false);
			break;
		case IDC_LOGGING_THREAD:
			LogWithThread(SendMessage(GetDlgItem(Dlg,IDC_LOGGING_THREAD),BM_GETCHECK,0,0)==BST_CHECKED?true:false);
			break;
		case IDC_SCRIPT_DEBUG:
			Script::SetLogDebugInfo(SendMessage((HWND)lparam,BM_GETCHECK,0,0)==BST_CHECKED);
			break;
		case IDC_SAVE_WORLD:
			if(Serv.IsInit()) Serv.SaveWorldNextTick=Timer::FastTick(); // Force saving time
			break;
		case IDC_RELOAD_CLIENT_SCRIPTS:
			if(Serv.IsInit()) Serv.RequestReloadClientScripts=true;
			break;
		case IDC_SPLIT_UP:
			if(SplitProcent>=50) SplitProcent-=40;
			ResizeDialog();
			break;
		case IDC_SPLIT_DOWN:
			if(SplitProcent<=50) SplitProcent+=40;
			ResizeDialog();
			break;
		default:
			break;
		}
		return 0;
	default:
		break;
	}
	return 0;
}

unsigned int __stdcall GameLoopThread(void*)
{
	LogSetThreadName("Main");

	GetServerOptions();

	if(Serv.Init())
	{
		if(Dlg)
		{
			if(SendMessage(GetDlgItem(Dlg,IDC_LOGGING),BM_GETCHECK,0,0)==BST_UNCHECKED) LogFinish(LOG_DLG);

			// Enable buttons
			EnableWindow(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPTS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_SAVE_WORLD),1);
			EnableWindow(GetDlgItem(Dlg,IDC_CLIENTS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_MAPS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_TIME_EVENTS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_ANY_DATA),1);
			EnableWindow(GetDlgItem(Dlg,IDC_ITEMS_STAT),1);
			EnableWindow(GetDlgItem(Dlg,IDC_STOP),1);
		}

		SetEvent(GameInitEvent);

		Serv.MainLoop();
		Serv.Finish();
		UpdateInfo();
	}
	else
	{
		WriteLog(NULL,"Initialization fail!\n");
		SetEvent(GameInitEvent);
	}

	if(Dlg)
	{
		string str;
		LogFinish(LOG_BUFFER);
		LogGetBuffer(str);
		HWND dlg_log=GetDlgItem(Dlg,IDC_LOG);
		LogToDlg(&dlg_log);
		if(str.length()) WriteLog(NULL,"%s",str.c_str());
	}

	LogFinish(-1);
	if(Singleplayer) FOAppQuit=true;
	return 0;
}

/************************************************************************/
/* Service                                                              */
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
		SERVICE_TABLE_ENTRY dispatch_table[]={{StringDuplicate("FOnlineServer"),FOServiceStart},{NULL,NULL}};
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
	if(strstr(GetCommandLine(),"-delete"))
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
	sprintf(path2,"\"%s\" --service",path1);

	// Change executable path, if changed
	if(service)
	{
		LPQUERY_SERVICE_CONFIG service_cfg=(LPQUERY_SERVICE_CONFIG)calloc(8192,1);
		DWORD dw;
		if(QueryServiceConfig(service,service_cfg,8192,&dw) && _stricmp(service_cfg->lpBinaryPathName,path2))
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
	WriteLog(NULL,"FOnline server service, version %04X-%02X.\n",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);

	FOServiceStatusHandle=RegisterServiceCtrlHandler("FOnlineServer",FOServiceCtrlHandler);
	if(!FOServiceStatusHandle) return;

	// Start game
	SetFOServiceStatus(SERVICE_START_PENDING);

	FOQuit=false;
	LoopThreadHandle=(HANDLE)_beginthreadex(NULL,0,GameLoopThread,NULL,0,NULL);
	WaitForSingleObject(GameInitEvent,INFINITE);

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
		WaitForSingleObject(LoopThreadHandle,INFINITE);
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
