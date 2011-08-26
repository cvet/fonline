#include "stdafx.h"
#include "Mapper.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

LRESULT APIENTRY WndProc(HWND wnd,UINT message,WPARAM wparam,LPARAM lparam);
HINSTANCE Instance=NULL;
HWND Wnd=NULL;
FOMapper* Mapper=NULL;

int APIENTRY WinMain(HINSTANCE cur_instance, HINSTANCE prev_instance,LPSTR cmd_line,int cmd_show)
{
	setlocale(LC_ALL,"Russian");
	RestoreMainDirectory();

	// Pthreads
#if defined(FO_WINDOWS)
	pthread_win32_process_attach_np();
#endif

	// Exceptions
	CatchExceptions("FOnlineMapper",MAPPER_VERSION);

	// Send about new instance to already runned mapper
	if(FindWindow(GetWindowName(),GetWindowName())!=NULL)
	{
		MessageBox(NULL,"FOnline already run.","FOnline",MB_OK);
		return 0;
	}

	// Timer
	Timer::Init();

	// Register window
	WNDCLASS wndClass;
	MSG msg;
	wndClass.style=CS_HREDRAW|CS_VREDRAW;
	wndClass.lpfnWndProc=WndProc;
	wndClass.cbClsExtra=0;
	wndClass.cbWndExtra=0;
	wndClass.hInstance=cur_instance;
	wndClass.hIcon=LoadIcon(cur_instance,MAKEINTRESOURCE(IDI_ICON));
	wndClass.hCursor=LoadCursor(NULL,IDC_ARROW);
	wndClass.hbrBackground=(HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wndClass.lpszMenuName=NULL;
	wndClass.lpszClassName=GetWindowName();
	RegisterClass(&wndClass);
	Instance=cur_instance;

	LogToFile("FOMapper.log");

	GetClientOptions();

	Wnd=CreateWindow(
		GetWindowName(),
		GetWindowName(),
		WS_OVERLAPPEDWINDOW&(~WS_MAXIMIZEBOX)&(~WS_SIZEBOX)&(~WS_SYSMENU), //(~WS_SYSMENU)
		CW_USEDEFAULT,CW_USEDEFAULT,100,100,
		NULL,
		NULL,
		cur_instance,
		NULL);

	HDC dcscreen=GetDC(NULL);
	int sw=GetDeviceCaps(dcscreen,HORZRES);
	int sh=GetDeviceCaps(dcscreen,VERTRES);
	ReleaseDC(NULL,dcscreen);

	WINDOWINFO wi;
	wi.cbSize=sizeof(wi);
	GetWindowInfo(Wnd,&wi);
	INTRECT wborders(wi.rcClient.left-wi.rcWindow.left,wi.rcClient.top-wi.rcWindow.top,wi.rcWindow.right-wi.rcClient.right,wi.rcWindow.bottom-wi.rcClient.bottom);
	SetWindowPos(Wnd,NULL,(sw-MODE_WIDTH-wborders.L-wborders.R)/2,(sh-MODE_HEIGHT-wborders.T-wborders.B)/2,MODE_WIDTH+wborders.L+wborders.R,MODE_HEIGHT+wborders.T+wborders.B,0);

	ShowWindow(Wnd,SW_SHOWNORMAL);
	UpdateWindow(Wnd);

	WriteLog("Starting Mapper...\n");

	Mapper=new FOMapper();
	if(!Mapper->Init(Wnd))
	{
		WriteLog("Mapper initialization fail.\n");
		DestroyWindow(Wnd);
		return 0;
	}

	while(!GameOpt.Quit)
	{
		if(PeekMessage(&msg,NULL,NULL,NULL,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// Main Loop
			Mapper->MainLoop();

			// Sleep
			if(GameOpt.Sleep>=0) Sleep(GameOpt.Sleep);
		}
	}

	Mapper->Finish();

	WriteLog("Mapper closed.\n");
	LogFinish(-1);

	delete Mapper;
	return 0;
}

LRESULT APIENTRY WndProc(HWND wnd, UINT message,WPARAM wparam,LPARAM lparam)
{
	switch(message)
	{
	case WM_DESTROY:
		GameOpt.Quit=true;
		return 0;
	case WM_KEYDOWN:
		if(wparam==VK_F12) ShowWindow(wnd,SW_MINIMIZE);
		return 0;
	case WM_ACTIVATE:
		break;
	case WM_FLASH_WINDOW:
		if(wnd!=GetActiveWindow()) FlashWindow(wnd,true);
		break;
	}
	return DefWindowProc(wnd,message,wparam,lparam);
}