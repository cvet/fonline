#include "StdAfx.h"
#include "Client.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

// #define GAME_THREAD

LRESULT APIENTRY WndProc( HWND wnd, UINT message, WPARAM wparam, LPARAM lparam );
HWND      Wnd = NULL;
FOClient* FOEngine = NULL;

#ifdef GAME_THREAD
uint WINAPI GameLoopThread( void* );
HANDLE GameLoopThreadHandle = NULL;
#endif

int APIENTRY WinMain( HINSTANCE cur_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show )
{
    setlocale( LC_ALL, "Russian" );
    RestoreMainDirectory();

    // Pthreads
    #if defined ( FO_WINDOWS )
    pthread_win32_process_attach_np();
    #endif

    // Exception
    CatchExceptions( "FOnline", CLIENT_VERSION );

    // Register window
    WNDCLASS wnd_class;
    MSG      msg;
    wnd_class.style = CS_HREDRAW | CS_VREDRAW;
    wnd_class.lpfnWndProc = WndProc;
    wnd_class.cbClsExtra = 0;
    wnd_class.cbWndExtra = 0;
    wnd_class.hInstance = cur_instance;
    wnd_class.hIcon = LoadIcon( cur_instance, MAKEINTRESOURCE( IDI_ICON ) );
    wnd_class.hCursor = LoadCursor( NULL, IDC_ARROW );
    wnd_class.hbrBackground = (HBRUSH) GetStockObject( LTGRAY_BRUSH );
    wnd_class.lpszMenuName = NULL;
    wnd_class.lpszClassName = GetWindowName();
    RegisterClass( &wnd_class );

    // Stuff
    Timer::Init();
    LogToFile( "FOnline.log" );

    // Singleplayer mode initialization
    char full_path[ MAX_FOPATH ] = { 0 };
    char path[ MAX_FOPATH ] = { 0 };
    char name[ MAX_FOPATH ] = { 0 };
    GetModuleFileName( NULL, full_path, MAX_FOPATH );
    FileManager::ExtractPath( full_path, path );
    FileManager::ExtractFileName( full_path, name );
    if( strstr( name, "Singleplayer" ) || strstr( cmd_line, "Singleplayer" ) )
    {
        WriteLog( "Singleplayer mode.\n" );
        Singleplayer = true;
        Timer::SetGamePause( true );

        // Create interprocess shared data
        HANDLE map_file = SingleplayerData.Init();
        if( !map_file )
        {
            WriteLog( "Can't map shared data to memory.\n" );
            return 0;
        }

        // Fill interprocess initial data
        if( SingleplayerData.Lock() )
        {
            // Initialize other data
            SingleplayerData.NetPort = 0;
            SingleplayerData.Pause = true;

            SingleplayerData.Unlock();
        }
        else
        {
            WriteLog( "Can't lock mapped file.\n" );
            return 0;
        }

        // Config parsing
        IniParser cfg;
        char      server_exe[ MAX_FOPATH ] = { 0 };
        char      server_path[ MAX_FOPATH ] = { 0 };
        char      server_cmdline[ MAX_FOPATH ] = { 0 };
        cfg.LoadFile( GetConfigFileName(), PT_ROOT );
        cfg.GetStr( CLIENT_CONFIG_APP, "ServerAppName", "FOserv.exe", server_exe );
        cfg.GetStr( CLIENT_CONFIG_APP, "ServerPath", "..\\server\\", server_path );
        cfg.GetStr( CLIENT_CONFIG_APP, "ServerCommandLine", "", server_cmdline );

        // Process attributes
        PROCESS_INFORMATION server;
        memzero( &server, sizeof( server ) );
        STARTUPINFOA        sui;
        memzero( &sui, sizeof( sui ) );
        sui.cb = sizeof( sui );
        HANDLE client_process = OpenProcess( SYNCHRONIZE, TRUE, GetCurrentProcessId() );
        char   command_line[ 2048 ];

        // Start server
        sprintf( command_line, "\"%s%s\" -singleplayer %p %p %s -logpath %s", server_path, server_exe, map_file, client_process, server_cmdline, path );
        if( !CreateProcess( NULL, command_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, server_path, &sui, &server ) )
        {
            WriteLog( "Can't start server process, error<%u>.\n", GetLastError() );
            return 0;
        }
        CloseHandle( server.hProcess );
        CloseHandle( server.hThread );
    }

    // Check for already runned window
    #ifndef DEV_VESRION
    if( !Singleplayer && FindWindow( GetWindowName(), GetWindowName() ) != NULL )
    {
        MessageBox( NULL, "FOnline is running already.", "FOnline", MB_OK );
        return 0;
    }
    #endif

    // Options
    GetClientOptions();

    Wnd = CreateWindow( GetWindowName(), GetWindowName(), WS_OVERLAPPEDWINDOW & ( ~WS_MAXIMIZEBOX ) & ( ~WS_SIZEBOX ) & ( ~WS_SYSMENU ), -101, -101, 100, 100, NULL, NULL, cur_instance, NULL );

    HDC dcscreen = GetDC( NULL );
    int sw = GetDeviceCaps( dcscreen, HORZRES );
    int sh = GetDeviceCaps( dcscreen, VERTRES );
    ReleaseDC( NULL, dcscreen );

    WINDOWINFO wi;
    wi.cbSize = sizeof( wi );
    GetWindowInfo( Wnd, &wi );
    INTRECT wborders( wi.rcClient.left - wi.rcWindow.left, wi.rcClient.top - wi.rcWindow.top, wi.rcWindow.right - wi.rcClient.right, wi.rcWindow.bottom - wi.rcClient.bottom );
    SetWindowPos( Wnd, NULL, ( sw - MODE_WIDTH - wborders.L - wborders.R ) / 2, ( sh - MODE_HEIGHT - wborders.T - wborders.B ) / 2,
                  MODE_WIDTH + wborders.L + wborders.R, MODE_HEIGHT + wborders.T + wborders.B, 0 );

    ShowWindow( Wnd, SW_SHOWNORMAL );
    UpdateWindow( Wnd );

    // Start FOnline
    WriteLog( "Starting FOnline (version %04X-%02X)...\n\n", CLIENT_VERSION, FO_PROTOCOL_VERSION & 0xFF );

    FOEngine = new FOClient();
    if( !FOEngine || !FOEngine->Init( Wnd ) )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        return 0;
    }

    #ifdef GAME_THREAD
    GameLoopThreadHandle = CreateThread( NULL, 0, GameLoopThread, NULL, 0, NULL );
    #endif

    // Windows messages
    while( !GameOpt.Quit )
    {
        if( PeekMessage( &msg, NULL, NULL, NULL, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            #ifdef GAME_THREAD
            Sleep( 100 );
            #else
            if( !FOEngine->MainLoop() )
                Sleep( 100 );
            else if( GameOpt.Sleep >= 0 )
                Sleep( GameOpt.Sleep );
            #endif
        }
    }

    // Finishing FOnline
    #ifdef GAME_THREAD
    WaitForSingleObject( GameLoopThreadHandle, INFINITE );
    #endif
    FOEngine->Finish();
    delete FOEngine;
    if( Singleplayer )
        SingleplayerData.Finish();
    WriteLog( "FOnline finished.\n" );
    LogFinish( -1 );

    // _CrtDumpMemoryLeaks();
    return 0;
}

#ifdef GAME_THREAD
uint WINAPI GameLoopThread( void* )
{
    while( !GameOpt.Quit )
    {
        if( !FOEngine->MainLoop() )
            Sleep( 100 );
        else if( GameOpt.Sleep >= 0 )
            Sleep( GameOpt.Sleep );
    }

    CloseHandle( GameLoopThreadHandle );
    GameLoopThreadHandle = NULL;
    ExitThread( 0 );
    return 0;
}
#endif

LRESULT APIENTRY WndProc( HWND wnd, UINT message, WPARAM wparam, LPARAM lparam )
{
    switch( message )
    {
    case WM_DESTROY:
        GameOpt.Quit = true;
        break;
    case WM_KEYDOWN:
        if( wparam == VK_F12 )
        {
            ShowWindow( Wnd, SW_MINIMIZE );
            return 0;
        }
        break;
    case WM_SHOWWINDOW:
        if( GameOpt.AlwaysOnTop )
            SetWindowPos( Wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
        break;
    case WM_SIZE:
        if( !GameOpt.GlobalSound && FOEngine && FOEngine->BasicAudio )
        {
            if( wparam == SIZE_MINIMIZED )
                FOEngine->BasicAudio->put_Volume( -10000 );
            else if( wparam == SIZE_RESTORED )
                FOEngine->BasicAudio->put_Volume( 0 );
        }
        break;
    case WM_ACTIVATE:
        if( !GameOpt.GlobalSound && FOEngine && FOEngine->BasicAudio )
        {
            if( LOWORD( wparam ) == WA_INACTIVE && !HIWORD( wparam ) )
                FOEngine->BasicAudio->put_Volume( -10000 );
            else if( LOWORD( wparam ) == WA_ACTIVE || LOWORD( wparam ) == WA_CLICKACTIVE )
                FOEngine->BasicAudio->put_Volume( 0 );
        }
        break;
    case WM_FLASH_WINDOW:
        if( wnd != GetActiveWindow() )
        {
            if( GameOpt.MessNotify )
                FlashWindow( Wnd, true );
            if( GameOpt.SoundNotify )
                Beep( 100, 200 );
        }
        return 0;
//      case WM_ERASEBKGND:
//              return 0;
        #ifndef GAME_THREAD
    case WM_DISPLAYCHANGE:
        if( FOEngine && FOEngine->WindowLess )
            FOEngine->WindowLess->DisplayModeChanged();
        break;
    case WM_MOVING:
        if( FOEngine )
            FOEngine->MainLoop();
        return TRUE;
    case WM_PAINT:
        if( FOEngine )
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint( Wnd, &ps );
            FOEngine->MainLoop();
            if( FOEngine->WindowLess )
                FOEngine->WindowLess->RepaintVideo( wnd, hdc );
            EndPaint( Wnd, &ps );
            return 0;
        }

        /*if(FOEngine && FOEngine->WindowLess)
           {
                PAINTSTRUCT ps;
                HDC         hdc;
                RECT        rcClient;
                GetClientRect(wnd, &rcClient);
                hdc = BeginPaint(wnd, &ps);

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
                HRESULT hr = FOEngine->WindowLess->RepaintVideo(wnd, hdc);
                EndPaint(wnd, &ps);
           }*/
        break;
    case WM_WINDOWPOSCHANGING:
        // Allow size greather than monitor resolution
        // Used in video playing workaround
        return 0;
        #endif
/*	case WM_SETCURSOR:
                // Turn off window cursor
            SetCursor( NULL );
            return TRUE; // prevent Windows from setting cursor to window class cursor
        break;*/
    }

    return DefWindowProc( wnd, message, wparam, lparam );
}
