#include "StdAfx.h"
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
HINSTANCE hInstance=NULL;
HWND hDlg=NULL;
RECT MainInitRect,LogInitRect,InfoInitRect;
int SplitProcent=50;
FOServer Serv;

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpCmdLine, int nCmdShow)
{
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
	setlocale(LC_ALL,"Russian");

#ifndef SERVER_LITE
	// Exceptions catcher
	CatchExceptions("FOserv",SERVER_VERSION);
#endif
	Timer::Init(); // Timer
	LoadLibrary("RICHED32.dll"); // Riched20.dll

	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);

	hDlg=CreateDialog(hInstance,MAKEINTRESOURCE(IDD_DLG),NULL,DlgProc);
	int wx=cfg.GetInt("PositionX",0);
	int wy=cfg.GetInt("PositionY",0);
	if(wx || wy) SetWindowPos(hDlg,0,wx,wy,0,0,SWP_NOZORDER|SWP_NOSIZE);

	char win_title[256];
	sprintf(win_title,"FOnline Server               version %04X-%02X",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);
#ifdef SERVER_LITE
	strcat(win_title," Lite Edition");
#endif
	SetWindowText(hDlg,win_title);

	// Disable buttons
	EnableWindow(GetDlgItem(hDlg,IDC_RELOAD_CLIENT_SCRIPT),0);
	EnableWindow(GetDlgItem(hDlg,IDC_SAVE_WORLD),0);
	EnableWindow(GetDlgItem(hDlg,IDC_CLIENTS),0);
	EnableWindow(GetDlgItem(hDlg,IDC_MAPS),0);
	EnableWindow(GetDlgItem(hDlg,IDC_TIME_EVENTS),0);
	EnableWindow(GetDlgItem(hDlg,IDC_ANY_DATA),0);
	EnableWindow(GetDlgItem(hDlg,IDC_ITEMS_STAT),0);

	// Set font
	CHARFORMAT fmt;
	ZeroMemory(&fmt,sizeof(fmt));
	fmt.cbSize=sizeof(fmt);
	SendDlgItemMessage(hDlg,IDC_LOG,EM_GETCHARFORMAT,SCF_DEFAULT,(LPARAM)&fmt);
	cfg.GetStr("TextFont","Courier New",fmt.szFaceName);
	SendDlgItemMessage(hDlg,IDC_LOG,EM_SETCHARFORMAT,SCF_DEFAULT,(LPARAM)&fmt);
	SendDlgItemMessage(hDlg,IDC_INFO,EM_SETCHARFORMAT,SCF_DEFAULT,(LPARAM)&fmt);
	HFONT font=CreateFont(14,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,fmt.szFaceName);
	SendMessage(GetDlgItem(hDlg,IDC_GAMETIME),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_INGAME),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_NPC),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_ITEMS_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_LOC_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_VARS_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_ANYDATA_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_TE_COUNT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_FPS),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_DELTA),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_UPTIME),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_SEND),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_RECV),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_COMPRESS),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_RELOAD_CLIENT_SCRIPT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_SAVE_WORLD),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_SAVELOG),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_SAVEINFO),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_DUMP),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_MEMORY),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_CLIENTS),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_MAPS),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_TIME_EVENTS),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_ANY_DATA),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_ITEMS_STAT),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_AUTOUPDATE),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_LOGGING),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_LOGGING_TIME),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_SCRIPT_DEBUG),WM_SETFONT,(LPARAM)font,TRUE);
	SendMessage(GetDlgItem(hDlg,IDC_STOP),WM_SETFONT,(LPARAM)font,TRUE);

	LogToDlg(GetDlgItem(hDlg,IDC_LOG));
	UpdateEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!UpdateEvent) WriteLog("Create update event fail, error<%u>.\n",GetLastError());
	LogEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!LogEvent) WriteLog("Create log event fail, error<%u>.\n",GetLastError());
	FOQuit=true;
	Serv.ServerWindow=hDlg;

	WINDOWINFO wi;
	wi.cbSize=sizeof(wi);
	GetWindowInfo(hDlg,&wi);
	MainInitRect=wi.rcClient;
	GetWindowInfo(GetDlgItem(hDlg,IDC_LOG),&wi);
	LogInitRect=wi.rcClient;
	GetWindowInfo(GetDlgItem(hDlg,IDC_INFO),&wi);
	InfoInitRect=wi.rcClient;
	ResizeDialog();

	WriteLog("FOnline server, version %04X-%02X.\n",SERVER_VERSION,FO_PROTOCOL_VERSION&0xFF);

	FOServer::Statistics.DataReal=1;
	FOServer::Statistics.DataCompressed=1;
	MemoryDebugLevel=cfg.GetInt("MemoryDebugLevel",0);

	UpdateInfo();
	SetDlgItemText(hDlg,IDC_RELOAD_CLIENT_SCRIPT,"Reload client script");
	SetDlgItemText(hDlg,IDC_SAVE_WORLD,"Save world");
	SetDlgItemText(hDlg,IDC_SAVELOG,"Save log");
	SetDlgItemText(hDlg,IDC_SAVEINFO,"Save info");
	SetDlgItemText(hDlg,IDC_DUMP,"Create dump");
	SetDlgItemText(hDlg,IDC_MEMORY,"Memory usage");
	SetDlgItemText(hDlg,IDC_CLIENTS,"Players");
	SetDlgItemText(hDlg,IDC_MAPS,"Locations and maps");
	SetDlgItemText(hDlg,IDC_TIME_EVENTS,"Time events");
	SetDlgItemText(hDlg,IDC_ANY_DATA,"Any data");
	SetDlgItemText(hDlg,IDC_ITEMS_STAT,"Items count");
	SetDlgItemText(hDlg,IDC_AUTOUPDATE,"Update info every second");
	SetDlgItemText(hDlg,IDC_LOGGING,"Logging");
	SetDlgItemText(hDlg,IDC_LOGGING_TIME,"Logging with time");
	SetDlgItemText(hDlg,IDC_SCRIPT_DEBUG,"Script debug info");
	SetDlgItemText(hDlg,IDC_STOP,"Start server");

	ServerScript.SetLogDebugInfo(true);
	LogWithTime(cfg.GetInt("LoggingTime",1)==0?false:true);

	SendMessage(GetDlgItem(hDlg,IDC_AUTOUPDATE),BM_SETCHECK,BST_UNCHECKED,0);
	SendMessage(GetDlgItem(hDlg,IDC_LOGGING),BM_SETCHECK,cfg.GetInt("Logging",1)==0?BST_UNCHECKED:BST_CHECKED,0);
	SendMessage(GetDlgItem(hDlg,IDC_LOGGING_TIME),BM_SETCHECK,cfg.GetInt("LoggingTime",1)==0?BST_UNCHECKED:BST_CHECKED,0);
	SendMessage(GetDlgItem(hDlg,IDC_SCRIPT_DEBUG),BM_SETCHECK,BST_CHECKED,0);

	// Command line
	if(lpCmdLine[0])
	{
		WriteLog("Command line <%s>.\n",lpCmdLine);
		if(strstr(lpCmdLine,"-start")) SendMessage(hDlg,WM_COMMAND,IDC_STOP,0);
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
		SetDlgItemText(hDlg,IDC_GAMETIME,str);
		sprintf(str,"Connections: %u",Serv.Statistics.CurOnline);
		SetDlgItemText(hDlg,IDC_COUNT,str);
		sprintf(str,"Players in game: %u",Serv.PlayersInGame());
		SetDlgItemText(hDlg,IDC_INGAME,str);
		sprintf(str,"NPC in game: %u",Serv.NpcInGame());
		SetDlgItemText(hDlg,IDC_NPC,str);
		sprintf(str,"Items: %u",ItemMngr.GetItemsCount());
		SetDlgItemText(hDlg,IDC_ITEMS_COUNT,str);
		sprintf(str,"Locations: %u (%u)",MapMngr.GetLocations().size(),MapMngr.GetAllMaps().size());
		SetDlgItemText(hDlg,IDC_LOC_COUNT,str);
		sprintf(str,"Vars: %u",VarMngr.GetVarsCount());
		SetDlgItemText(hDlg,IDC_VARS_COUNT,str);
		sprintf(str,"Any data: %u",Serv.AnyData.size());
		SetDlgItemText(hDlg,IDC_ANYDATA_COUNT,str);
		sprintf(str,"Time events: %u",Serv.GetTimeEventsCount());
		SetDlgItemText(hDlg,IDC_TE_COUNT,str);
		sprintf(str,"Cycles per second: %u",Serv.Statistics.FPS);
		SetDlgItemText(hDlg,IDC_FPS,str);
		sprintf(str,"Cycle time: %d",Serv.Statistics.CycleTime);
		SetDlgItemText(hDlg,IDC_DELTA,str);
	}
	else
	{
		SetDlgItemText(hDlg,IDC_GAMETIME,"Game time: n/a");
		SetDlgItemText(hDlg,IDC_COUNT,"Connections: n/a");
		SetDlgItemText(hDlg,IDC_INGAME,"Players in game: n/a");
		SetDlgItemText(hDlg,IDC_NPC,"NPC in game: n/a");
		SetDlgItemText(hDlg,IDC_ITEMS_COUNT,"Items: n/a");
		SetDlgItemText(hDlg,IDC_LOC_COUNT,"Locations: n/a");
		SetDlgItemText(hDlg,IDC_VARS_COUNT,"Vars: n/a");
		SetDlgItemText(hDlg,IDC_ANYDATA_COUNT,"Any data: n/a");
		SetDlgItemText(hDlg,IDC_TE_COUNT,"Time events: n/a");
		SetDlgItemText(hDlg,IDC_FPS,"Cycles per second: n/a");
		SetDlgItemText(hDlg,IDC_DELTA,"Cycle time: n/a");
	}

	DWORD seconds=Serv.Statistics.Uptime;
	sprintf(str,"Uptime: %2u:%2u:%2u",seconds/60/60,seconds/60%60,seconds%60);
	SetDlgItemText(hDlg,IDC_UPTIME,str);
	sprintf(str,"KBytes Send: %u",Serv.Statistics.BytesSend/1024);
	SetDlgItemText(hDlg,IDC_SEND,str);
	sprintf(str,"KBytes Recv: %u",Serv.Statistics.BytesRecv/1024);
	SetDlgItemText(hDlg,IDC_RECV,str);
	sprintf(str,"Compress ratio: %g",(double)Serv.Statistics.DataReal/Serv.Statistics.DataCompressed);
	SetDlgItemText(hDlg,IDC_COMPRESS,str);

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
			SendDlgItemMessage(hDlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 1: // Players
		if(Serv.IsInit())
		{
			str_=Serv.GetIngamePlayersStatistics();
			SendDlgItemMessage(hDlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 2: // Locations and maps
		if(Serv.IsInit())
		{
			str_=MapMngr.GetLocationsMapsStatistics();
			SendDlgItemMessage(hDlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 3: // Time events
		if(Serv.IsInit())
		{
			str_=Serv.GetTimeEventsStatistics();
			SendDlgItemMessage(hDlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 4: // Any data
		if(Serv.IsInit())
		{
			str_=Serv.GetAnyDataStatistics();
			SendDlgItemMessage(hDlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
		}
		break;
	case 5: // Items count
#ifdef ITEMS_STATISTICS
		if(Serv.IsInit())
		{
			str_=ItemMngr.GetItemsStatistics();
			SendDlgItemMessage(hDlg,IDC_INFO,WM_SETTEXT,0,(LPARAM)str_.c_str());
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
		SendDlgItemMessage(hDlg,IDC_LOG,EM_SETSEL,-1,-1);
		SendDlgItemMessage(hDlg,IDC_LOG,EM_REPLACESEL,0,(LPARAM)str.c_str());
	}
}

void ResizeDialog()
{
	WINDOWINFO wi;
	wi.cbSize=sizeof(wi);
	GetWindowInfo(hDlg,&wi);
	RECT rmain=wi.rcClient;

	if(rmain.right-rmain.left>0 && rmain.bottom-rmain.top>0)
	{
		int wdiff=(rmain.right-rmain.left)-(MainInitRect.right-MainInitRect.left);
		int hdiff=(rmain.bottom-rmain.top)-(MainInitRect.bottom-MainInitRect.top);

		GetWindowInfo(GetDlgItem(hDlg,IDC_LOG),&wi);
		RECT rlog=wi.rcClient;
		GetWindowInfo(GetDlgItem(hDlg,IDC_INFO),&wi);
		RECT rinfo=wi.rcClient;

		int hall=(LogInitRect.bottom-LogInitRect.top)+(InfoInitRect.bottom-InfoInitRect.top)+hdiff;
		int wlog=(LogInitRect.right-LogInitRect.left)+wdiff;
		int hlog=hall*SplitProcent/100;
		int winfo=(InfoInitRect.right-InfoInitRect.left)+wdiff;
		int hinfo=hall*(100-SplitProcent)/100;
		int yinfo=hlog-(LogInitRect.bottom-LogInitRect.top);

		SetWindowPos(GetDlgItem(hDlg,IDC_LOG),NULL,LogInitRect.left-MainInitRect.left,LogInitRect.top-MainInitRect.top,wlog,hlog,0);
		SetWindowPos(GetDlgItem(hDlg,IDC_INFO),NULL,InfoInitRect.left-MainInitRect.left,InfoInitRect.top-MainInitRect.top+yinfo,winfo,hinfo,0);
	}
}

int CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		PostMessage(hDlg,WM_SIZE,0,0);
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
			CloseWindow(hDlg);
			break;
		case IDC_STOP:
			if(!FOQuit) // End of work
			{
				FOQuit=true;
				SetDlgItemText(hDlg,IDC_STOP,"Start server");
				EnableWindow(GetDlgItem(hDlg,IDC_STOP),0);
				SendMessage(GetDlgItem(hDlg,IDC_LOG),WM_SETFOCUS,0,0);

				// Disable buttons
				EnableWindow(GetDlgItem(hDlg,IDC_RELOAD_CLIENT_SCRIPT),0);
				EnableWindow(GetDlgItem(hDlg,IDC_SAVE_WORLD),0);
				EnableWindow(GetDlgItem(hDlg,IDC_CLIENTS),0);
				EnableWindow(GetDlgItem(hDlg,IDC_MAPS),0);
				EnableWindow(GetDlgItem(hDlg,IDC_TIME_EVENTS),0);
				EnableWindow(GetDlgItem(hDlg,IDC_ANY_DATA),0);
				EnableWindow(GetDlgItem(hDlg,IDC_ITEMS_STAT),0);
			}
			else // Begin work
			{
				FOQuit=false;
				SetDlgItemText(hDlg,IDC_STOP,"Stop server");
				EnableWindow(GetDlgItem(hDlg,IDC_STOP),0);
				SendMessage(GetDlgItem(hDlg,IDC_LOG),WM_SETFOCUS,0,0);
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
					int len=GetWindowTextLength(GetDlgItem(hDlg,idc));
					char* str=new char[len+1];
					SendDlgItemMessage(hDlg,idc,EM_SETSEL,0,len);
					SendDlgItemMessage(hDlg,idc,EM_GETSELTEXT,0,(LPARAM)str);
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
			if(SendMessage(GetDlgItem(hDlg,IDC_AUTOUPDATE),BM_GETCHECK,0,0)==BST_CHECKED)
				FOServer::UpdateLastTick=Timer::FastTick();
			else
				FOServer::UpdateLastTick=0;
			break;
		case IDC_LOGGING:
			if(SendMessage(GetDlgItem(hDlg,IDC_LOGGING),BM_GETCHECK,0,0)==BST_CHECKED)
				LogToBuffer(LogEvent);
			else
				LogFinish(LOG_BUFFER);
			break;
		case IDC_LOGGING_TIME:
			LogWithTime(SendMessage(GetDlgItem(hDlg,IDC_LOGGING_TIME),BM_GETCHECK,0,0)==BST_CHECKED?true:false);
			break;
		case IDC_SCRIPT_DEBUG:
			ServerScript.SetLogDebugInfo(SendMessage((HWND)lParam,BM_GETCHECK,0,0)==BST_CHECKED);
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
	}
	return 0;
}

void GameLoopThread(void*)
{
	LogFinish(-1);
	LogToBuffer(LogEvent);
	GetServerOptions();

	if(Serv.Init())
	{
		if(SendMessage(GetDlgItem(hDlg,IDC_LOGGING),BM_GETCHECK,0,0)==BST_UNCHECKED) LogFinish(-1);

		// Enable buttons
		EnableWindow(GetDlgItem(hDlg,IDC_RELOAD_CLIENT_SCRIPT),1);
		EnableWindow(GetDlgItem(hDlg,IDC_SAVE_WORLD),1);
		EnableWindow(GetDlgItem(hDlg,IDC_CLIENTS),1);
		EnableWindow(GetDlgItem(hDlg,IDC_MAPS),1);
		EnableWindow(GetDlgItem(hDlg,IDC_TIME_EVENTS),1);
		EnableWindow(GetDlgItem(hDlg,IDC_ANY_DATA),1);
		EnableWindow(GetDlgItem(hDlg,IDC_ITEMS_STAT),1);
		EnableWindow(GetDlgItem(hDlg,IDC_STOP),1);

		Serv.RunGameLoop();
		Serv.Finish();
		UpdateInfo();
	}

	string str;
	LogGetBuffer(str);
	LogFinish(LOG_BUFFER);
	LogToDlg(GetDlgItem(hDlg,IDC_LOG));
	if(str.length()) WriteLog("%s",str.c_str());

	LogFinish(-1);
	_endthread();
}
