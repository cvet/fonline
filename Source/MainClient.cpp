#include "StdAfx.h"
#include "Client.h"
#include "Exception.h"
#include "Version.h"

//#define GAME_THREAD

LRESULT APIENTRY WndProc(HWND hWndProc, UINT message, WPARAM wParam, LPARAM lParam);
HWND hWnd=NULL;
FOClient* FOEngine;

#ifdef GAME_THREAD
HANDLE hGameThread=NULL;
DWORD dwGameThreadID=0;
DWORD WINAPI GameLoopThread(void *);
#endif

#ifdef FONLINE_SINGLE
#include "Server.h"
FOServer Serv;

void ServerLoopThread(void*);
bool StartSinglePlayer();
#endif

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpCmdLine, int nCmdShow)
{
	setlocale(LC_ALL,"Russian");

	// Check for run window
#if !defined(DEV_VESRION) && !defined(FONLINE_SINGLE)
	if(FindWindow(WINDOW_CLASS_NAME,WINDOW_NAME)!=NULL)
	{
		MessageBox(NULL,"FOnline already run.","FOnline",MB_OK);
		return 0;
	}
#endif

	// Exception
#ifdef FONLINE_SINGLE
	CatchExceptions("FOnlineSP",SINGLE_VERSION);
#else
	CatchExceptions("FOnline",CLIENT_VERSION);
#endif

	// Register window
	WNDCLASS wnd_class;//»спользуетс€ дл€ регистрации класса окна
	MSG msg;//сообщени€
	wnd_class.style=CS_HREDRAW|CS_VREDRAW;//определ€ет свойства окна
	wnd_class.lpfnWndProc=WndProc;//определ€ет адрес функции окна
	wnd_class.cbClsExtra=0;//число байт, которое необходимо запросить у Windows. ќбычно равна 0
	wnd_class.cbWndExtra=0;//число байт, которое необходимо запросить у Windows. ќбычно равна 0
	wnd_class.hInstance=hCurrentInst;//сообщает Windows о том, кто создает определение класса
	wnd_class.hIcon=LoadIcon(hCurrentInst,MAKEINTRESOURCE(IDI_ICON));//загружает иконку, в данном случае ее нет
	wnd_class.hCursor=LoadCursor(NULL,IDC_ARROW);//стандартный курсор
	wnd_class.hbrBackground=(HBRUSH)GetStockObject(LTGRAY_BRUSH);//фон приложени€
	wnd_class.lpszMenuName=NULL;//определ€ет меню. ¬ данной ситуации меню отсутствует
	wnd_class.lpszClassName=WINDOW_CLASS_NAME;//указатель на строку, содержащую им€ класса
	RegisterClass(&wnd_class);//регистраци€ окна

	// Timer
	Timer::Init();

#ifdef FONLINE_SINGLE
	LogToFile("FOnlineSP.log");
#else
	LogToFile("FOnline.log");
#endif

	GetClientOptions();

	hWnd=CreateWindow(WINDOW_CLASS_NAME,WINDOW_NAME,WS_OVERLAPPEDWINDOW&(~WS_MAXIMIZEBOX)&(~WS_SIZEBOX)&(~WS_SYSMENU),-101,-101,100,100,NULL,NULL,hCurrentInst,NULL);

	HDC dcscreen=GetDC(NULL);
	int sw=GetDeviceCaps(dcscreen,HORZRES);
	int sh=GetDeviceCaps(dcscreen,VERTRES);
	ReleaseDC(NULL,dcscreen);

	WINDOWINFO wi;
	wi.cbSize=sizeof(wi);
	GetWindowInfo(hWnd,&wi);
	INTRECT wborders(wi.rcClient.left-wi.rcWindow.left,wi.rcClient.top-wi.rcWindow.top,wi.rcWindow.right-wi.rcClient.right,wi.rcWindow.bottom-wi.rcClient.bottom);
	SetWindowPos(hWnd,NULL,(sw-MODE_WIDTH-wborders.L-wborders.R)/2,(sh-MODE_HEIGHT-wborders.T-wborders.B)/2,
		MODE_WIDTH+wborders.L+wborders.R,MODE_HEIGHT+wborders.T+wborders.B,0);

	ShowWindow(hWnd,SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	// Start FOnline
#ifdef FONLINE_SINGLE
	WriteLog("Starting FOnline (version %04X)...\n\n",SINGLE_VERSION);
#else
	WriteLog("Starting FOnline (version %04X-%02X)...\n\n",CLIENT_VERSION,FO_PROTOCOL_VERSION&0xFF);
#endif

	FOEngine=new FOClient();
	if(!FOEngine || !FOEngine->Init(hWnd))
	{
		WriteLog("FOnline engine initialization fail.\n");
		return 0;
	}

#ifdef GAME_THREAD
	hGameThread=CreateThread(NULL,0,GameLoopThread,NULL,0,&dwGameThreadID);
#endif

#ifdef FONLINE_SINGLE
	if(!StartSinglePlayer())
	{
		WriteLog("FOnline single-player initialization fail.\n");
		return 0;
	}
#endif

	// Windows messages
	while(!CmnQuit)
	{
		if(PeekMessage(&msg,NULL,NULL,NULL,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
#ifdef GAME_THREAD
			Sleep(100);
#else
			if(!FOEngine->MainLoop()) Sleep(100);
			else if(OptSleep>=0) Sleep(OptSleep);
#endif
		}
	}

	// Finishing FOnline
#ifdef GAME_THREAD
	WaitForSingleObject(hGameThread,INFINITE);
#endif
	FOEngine->Clear();
	delete FOEngine;
	WriteLog("FOnline finished.\n");
	LogFinish(-1);

	//_CrtDumpMemoryLeaks();
	return 0;
}

#ifdef GAME_THREAD
DWORD WINAPI GameLoopThread(void *)
{
	while(!CmnQuit)
	{
		if(!FOEngine->MainLoop()) Sleep(100);
		else if(OptSleep>=0) Sleep(OptSleep);
	}

	CloseHandle(hGameThread);
	hGameThread=NULL;
	ExitThread(0);
	return 0;
}
#endif

LRESULT APIENTRY WndProc(HWND hWndProc, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_DESTROY:
		CmnQuit=true;
		break;
	case WM_KEYDOWN:
		if(wParam==VK_F12)
		{
			ShowWindow(hWnd,SW_MINIMIZE);
			/*if(OptFullScr) SendMessage(hWnd,WM_ACTIVATE,WA_INACTIVE,NULL);
			else
			{
				FOEngine->DoLost();
				ShowWindow(hWnd,SW_MINIMIZE);
				UpdateWindow(hWnd);
			}*/
			return 0;
		}
		break;
	case WM_SHOWWINDOW:
		if(OptAlwaysOnTop) SetWindowPos(hWnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
		break;
	/*case WM_ACTIVATE:
		if(LOWORD(wParam)==WA_INACTIVE && !HIWORD(wParam))
		{
			if(!FOEngine) break;
			if(!OptFullScr) break;
			FOEngine->DoLost();
			ShowWindow(hWnd,SW_MINIMIZE);
			UpdateWindow(hWnd);
		}
		else if((LOWORD(wParam)==WA_ACTIVE || LOWORD(wParam)==WA_CLICKACTIVE))
		{
			if(!FOEngine) break;
			FOEngine->Restore();
			FOEngine->RestoreDI();
		}
		break;*/

	case WM_ACTIVATE:
		if(!OptGlobalSound && FOEngine && FOEngine->BasicAudio)
		{
			if(LOWORD(wParam)==WA_INACTIVE && !HIWORD(wParam)) FOEngine->BasicAudio->put_Volume(-10000);
			else if((LOWORD(wParam)==WA_ACTIVE || LOWORD(wParam)==WA_CLICKACTIVE)) FOEngine->BasicAudio->put_Volume(0);
		}
		break;
	case WM_FLASH_WINDOW:
		if(hWndProc!=GetActiveWindow())
		{
			if(OptMessNotify) FlashWindow(hWnd,true);
			if(OptSoundNotify) Beep(100,200);
		}
		return 0;
// 	case WM_ERASEBKGND:
// 		return 0;
#ifndef GAME_THREAD
	case WM_DISPLAYCHANGE:
		if(FOEngine && FOEngine->WindowLess) FOEngine->WindowLess->DisplayModeChanged();
		break;
	case WM_MOVING:
		if(FOEngine) FOEngine->MainLoop();
		return TRUE;
	case WM_PAINT:
		if(FOEngine)
		{
			PAINTSTRUCT ps;
			HDC hdc=BeginPaint(hWnd,&ps);
			FOEngine->MainLoop();
			if(FOEngine->WindowLess) FOEngine->WindowLess->RepaintVideo(hWndProc,hdc);
			EndPaint(hWnd,&ps);
			return 0;
		}

		/*if(FOEngine && FOEngine->WindowLess) 
		{ 
			PAINTSTRUCT ps; 
			HDC         hdc; 
			RECT        rcClient; 
			GetClientRect(hWndProc, &rcClient); 
			hdc = BeginPaint(hWndProc, &ps); 

			/ *HRGN rgnClient = CreateRectRgnIndirect(&rcClient); 
			HRGN rgnVideo  = CreateRectRgnIndirect(&FOEngine->WindowLessRectDest);  // Saved from earlier.
			//FOEngine->WindowLessRectDest=rcClient;
			CombineRgn(rgnClient, rgnClient, rgnVideo, RGN_DIFF);  // Paint on this region.

			HBRUSH hbr = GetSysColorBrush(COLOR_BTNFACE); 
			FillRgn(hdc, rgnClient, hbr); 
			DeleteObject(hbr); 
			DeleteObject(rgnClient); 
			DeleteObject(rgnVideo); * /

			// Request the VMR to paint the video.
			HRESULT hr = FOEngine->WindowLess->RepaintVideo(hWndProc, hdc);
			EndPaint(hWndProc, &ps);
		}*/
		break;
#endif
/*	case WM_SETCURSOR:
		// Turn off window cursor 
	    SetCursor( NULL );
	    return TRUE; // prevent Windows from setting cursor to window class cursor
	break;*/
	}

	return DefWindowProc(hWndProc,message,wParam,lParam);
}

#ifdef FONLINE_SINGLE
bool StartSinglePlayer()
{
	GetServerOptions();
	if(Serv.Init())
	{
		_beginthread(ServerLoopThread,0,NULL);
		return true;
	}
	return false;
}

void ServerLoopThread(void*)
{
	Serv.RunGameLoop();
	Serv.Finish();
	_endthread();
}
#endif