#include "StdAfx.h"
#include "ResourceServer.h"
#include "Server.h"
#include "Exception.h"
#include "Version.h"
#include <iostream>
#include <process.h>

void GameLoopThread(void*);
int CALLBACK DlgProc(HWND hwndDlg,UINT msg,WPARAM wParam,LPARAM lParam);
void UpdateInfo();
void UpdateLog();
void ResizeDialog();
HINSTANCE Instance=NULL;
HWND Dlg=NULL;
RECT MainInitRect,LogInitRect,InfoInitRect;
int SplitProcent=50;
FOServer Serv;

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpCmdLine, int nCmdShow)
{
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
	setlocale(LC_ALL,"Russian");

	// Exceptions catcher
	CatchExceptions("FOserv",SERVER_VERSION);

	Timer::Init(); // Timer
	LoadLibrary("RICHED32.dll"); // Riched20.dll

	// Check single player parameters
	if(strstr(lpCmdLine,"-singleplayer "))
	{
		Singleplayer=true;
		Timer::SetGamePause(true);

		// Logging
		char log_path[MAX_FOPATH]={0};
		if(!strstr(lpCmdLine,"-nologpath ") && strstr(lpCmdLine,"-logpath "))
		{
			const char* ptr=strstr(lpCmdLine,"-logpath ")+strlen("-logpath ");
			StringCopy(log_path,ptr);
		}
		StringAppend(log_path,"FOserv.log");
		LogToFile(log_path);

		WriteLog("Singleplayer mode.\n");

		// Shared data
		const char* ptr=strstr(lpCmdLine,"-singleplayer ")+strlen("-singleplayer ");
		HANDLE map_file=NULL;
		if(sscanf_s(ptr,"%p%p",&map_file,&SingleplayerClientProcess)!=2 || !SingleplayerData.Attach(map_file))
		{
			WriteLog("Can't attach to mapped file<%p>.\n",map_file);
			return 0;
		}
	}

	// Configuration
	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);

	if(!Singleplayer || strstr(lpCmdLine,"-showgui "))
	{
		LogFinish(-1);
		LogToDlg(GetDlgItem(Dlg,IDC_LOG));
		Dlg=CreateDialog(Instance,MAKEINTRESOURCE(IDD_DLG),NULL,DlgProc);

		int wx=cfg.GetInt("PositionX",0);
		int wy=cfg.GetInt("PositionY",0);
		if(wx || wy) SetWindowPos(Dlg,0,wx,wy,0,0,SWP_NOZORDER|SWP_NOSIZE);

		char win_title[256];
		sprintf(win_title,"FOnline Server               version %04X-%02X",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);
		SetWindowText(Dlg,win_title);

		// Disable buttons
		EnableWindow(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPT),0);
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
		SendMessage(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPT),WM_SETFONT,(LPARAM)font,TRUE);
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
		SetDlgItemText(Dlg,IDC_RELOAD_CLIENT_SCRIPT,"Reload client script");
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
		SetDlgItemText(Dlg,IDC_SCRIPT_DEBUG,"Script debug info");
		SetDlgItemText(Dlg,IDC_STOP,"Start server");
	}

	UpdateEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!UpdateEvent) WriteLog("Create update event fail, error<%u>.\n",GetLastError());
	LogEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!LogEvent) WriteLog("Create log event fail, error<%u>.\n",GetLastError());
	FOQuit=true;
	Serv.ServerWindow=Dlg;

	WriteLog("FOnline server, version %04X-%02X.\n",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);

	MemoryDebugLevel=cfg.GetInt("MemoryDebugLevel",0);
	Script::SetLogDebugInfo(true);
	LogWithTime(cfg.GetInt("LoggingTime",1)==0?false:true);

	if(Dlg)
	{
		SendMessage(GetDlgItem(Dlg,IDC_AUTOUPDATE),BM_SETCHECK,BST_UNCHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING),BM_SETCHECK,cfg.GetInt("Logging",1)==0?BST_UNCHECKED:BST_CHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_LOGGING_TIME),BM_SETCHECK,cfg.GetInt("LoggingTime",1)==0?BST_UNCHECKED:BST_CHECKED,0);
		SendMessage(GetDlgItem(Dlg,IDC_SCRIPT_DEBUG),BM_SETCHECK,BST_CHECKED,0);
	}

	// Command line
	if(lpCmdLine[0]) WriteLog("Command line<%s>.\n",lpCmdLine);

	// Autostart
	if(strstr(lpCmdLine,"-start ") || Singleplayer)
	{
		if(Dlg)
		{
			SendMessage(Dlg,WM_COMMAND,IDC_STOP,0);
		}
		else
		{
			FOQuit=false;
			_beginthread(GameLoopThread,0,NULL);
		}
	}

	// Loop
	MSG msg;
	HANDLE events[2]={UpdateEvent,LogEvent};
	while(!FOAppQuit)
	{
		DWORD result=MsgWaitForMultipleObjects(2,events,FALSE,10000,QS_ALLINPUT);
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
			WriteLog("Wait failed on MsgWaitForMultipleObjects, error<%u>.\b",GetLastError());
		}
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
		SYSTEMTIME st=GetGameTime(GameOpt.FullMinute);
		sprintf(str,"Game time: %02u.%02u.%04u %02u:%02u",st.wDay,st.wMonth,st.wYear,st.wHour,st.wMinute);
		SetDlgItemText(Dlg,IDC_GAMETIME,str);
		sprintf(str,"Connections: %u",Serv.Statistics.CurOnline);
		SetDlgItemText(Dlg,IDC_COUNT,str);
		sprintf(str,"Players in game: %u",Serv.PlayersInGame());
		SetDlgItemText(Dlg,IDC_INGAME,str);
		sprintf(str,"NPC in game: %u",Serv.NpcInGame());
		SetDlgItemText(Dlg,IDC_NPC,str);
		sprintf(str,"Items: %u",ItemMngr.GetItemsCount());
		SetDlgItemText(Dlg,IDC_ITEMS_COUNT,str);
		sprintf(str,"Locations: %u (%u)",MapMngr.GetLocations().size(),MapMngr.GetAllMaps().size());
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
		SetDlgItemText(Dlg,IDC_GAMETIME,"Game time: n/a");
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

	DWORD seconds=Serv.Statistics.Uptime;
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
#ifdef ITEMS_STATISTICS
		if(Serv.IsInit())
		{
			str_=ItemMngr.GetItemsStatistics();
			SendDlgItemMessage(Dlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
#endif
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

int CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
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
		switch(LOWORD(wParam))
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
				EnableWindow(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPT),0);
				EnableWindow(GetDlgItem(Dlg,IDC_SAVE_WORLD),0);
				EnableWindow(GetDlgItem(Dlg,IDC_CLIENTS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_MAPS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_TIME_EVENTS),0);
				EnableWindow(GetDlgItem(Dlg,IDC_ANY_DATA),0);
				EnableWindow(GetDlgItem(Dlg,IDC_ITEMS_STAT),0);
			}
			else // Begin work
			{
				FOQuit=false;
				SetDlgItemText(Dlg,IDC_STOP,"Stop server");
				EnableWindow(GetDlgItem(Dlg,IDC_STOP),0);
				SendMessage(GetDlgItem(Dlg,IDC_LOG),WM_SETFOCUS,0,0);
				_beginthread(GameLoopThread,0,NULL);
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
				WORD idc=(LOWORD(wParam)==IDC_SAVELOG?IDC_LOG:IDC_INFO);
				HANDLE hlog=CreateFile(idc==IDC_LOG?"FOserv.log":"FOinfo.txt",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
				if(hlog!=INVALID_HANDLE_VALUE)
				{
					int len=GetWindowTextLength(GetDlgItem(Dlg,idc));
					char* str=new char[len+1];
					SendDlgItemMessage(Dlg,idc,EM_SETSEL,0,len);
					SendDlgItemMessage(Dlg,idc,EM_GETSELTEXT,0,(LPARAM)str);
					str[len]='\0';
					DWORD br;
					WriteFile(hlog,str,strlen(str),&br,NULL);
					CloseHandle(hlog);
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
				LogToBuffer(LogEvent);
			else
				LogFinish(LOG_BUFFER);
			break;
		case IDC_LOGGING_TIME:
			LogWithTime(SendMessage(GetDlgItem(Dlg,IDC_LOGGING_TIME),BM_GETCHECK,0,0)==BST_CHECKED?true:false);
			break;
		case IDC_SCRIPT_DEBUG:
			Script::SetLogDebugInfo(SendMessage((HWND)lParam,BM_GETCHECK,0,0)==BST_CHECKED);
			break;
		case IDC_SAVE_WORLD:
			if(Serv.IsInit()) Serv.SaveWorldNextTick=Timer::FastTick();
			break;
		case IDC_RELOAD_CLIENT_SCRIPT:
			if(Serv.IsInit()) Serv.ReloadLangScript();
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

void GameLoopThread(void*)
{
	if(Dlg)
	{
		LogFinish(-1);
		LogToBuffer(LogEvent);
	}
	GetServerOptions();

	if(Serv.Init())
	{
		if(Dlg)
		{
			if(SendMessage(GetDlgItem(Dlg,IDC_LOGGING),BM_GETCHECK,0,0)==BST_UNCHECKED) LogFinish(-1);

			// Enable buttons
			EnableWindow(GetDlgItem(Dlg,IDC_RELOAD_CLIENT_SCRIPT),1);
			EnableWindow(GetDlgItem(Dlg,IDC_SAVE_WORLD),1);
			EnableWindow(GetDlgItem(Dlg,IDC_CLIENTS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_MAPS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_TIME_EVENTS),1);
			EnableWindow(GetDlgItem(Dlg,IDC_ANY_DATA),1);
			EnableWindow(GetDlgItem(Dlg,IDC_ITEMS_STAT),1);
			EnableWindow(GetDlgItem(Dlg,IDC_STOP),1);
		}

		Serv.RunGameLoop();
		Serv.Finish();
		UpdateInfo();
	}

	if(Dlg)
	{
		string str;
		LogGetBuffer(str);
		LogFinish(LOG_BUFFER);
		LogToDlg(GetDlgItem(Dlg,IDC_LOG));
		if(str.length()) WriteLog("%s",str.c_str());
	}

	LogFinish(-1);
	if(Singleplayer) FOAppQuit=true;
	_endthread();
}
