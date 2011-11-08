#include "StdAfx.h"
#include "Client.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

FOWindow* MainWindow = NULL;
FOClient* FOEngine = NULL;
Thread    Game;
void* GameThread( void* );

int APIENTRY WinMain( HINSTANCE cur_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show )
{
    setlocale( LC_ALL, "Russian" );
    RestoreMainDirectory();

    // Pthreads
    #ifdef FO_WINDOWS
    pthread_win32_process_attach_np();
    #endif

    // Exception
    CatchExceptions( "FOnline", CLIENT_VERSION );

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
        ShowMessage( "FOnline is running already." );
        return 0;
    }
    #endif

    // Options
    GetClientOptions();

    // Hide cursor
    #ifdef FO_WINDOWS
    ShowCursor( FALSE );
    #else
    // Todo: Linux
    #endif

    // Create window
    Fl::lock();
    MainWindow = new FOWindow();
    MainWindow->label( GetWindowName() );
    MainWindow->position( ( Fl::w() - MODE_WIDTH ) / 2, ( Fl::h() - MODE_HEIGHT ) / 2 );
    MainWindow->size( MODE_WIDTH, MODE_HEIGHT );
    MainWindow->set_modal();
    // MainWindow->size_range( 100, 100 );

    // Icon
    #ifdef FO_WINDOWS
    MainWindow->icon( (char*) LoadIcon( fl_display, MAKEINTRESOURCE( 101 ) ) );
    #else // FO_LINUX
    fl_open_display();
    // Todo: Linux
    #endif

    // Show window
    char  dummy_argv0[ 2 ] = "";
    char* dummy_argv[] = { dummy_argv0 };
    int   dummy_argc = 1;
    MainWindow->show( dummy_argc, dummy_argv );

    // Start
    WriteLog( "Starting FOnline (version %04X-%02X)...\n\n", CLIENT_VERSION, FO_PROTOCOL_VERSION & 0xFF );
    Game.Start( GameThread );

    // Loop
    while( !GameOpt.Quit )
        Fl::wait();
    Game.Wait();

    // Finish
    if( Singleplayer )
        SingleplayerData.Finish();
    WriteLog( "FOnline finished.\n" );
    LogFinish( -1 );

    return 0;
}

void* GameThread( void* )
{
    // Start
    FOEngine = new (nothrow) FOClient();
    if( !FOEngine || !FOEngine->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        GameOpt.Quit = true;
        return NULL;
    }

    // Loop
    while( !GameOpt.Quit )
    {
        if( !FOEngine->MainLoop() )
            Sleep( 100 );
        else if( GameOpt.Sleep >= 0 )
            Sleep( GameOpt.Sleep );
    }

    // Finish
    FOEngine->Finish();
    delete FOEngine;
    return NULL;
}

int FOWindow::handle( int event )
{
    if( !FOEngine || GameOpt.Quit )
        return 0;

    // Keyboard
    if( event == FL_KEYDOWN || event == FL_KEYUP )
    {
        int event_key = Fl::event_key();
        FOEngine->KeyboardEventsLocker.Lock();
        FOEngine->KeyboardEvents.push_back( event );
        FOEngine->KeyboardEvents.push_back( event_key );
        FOEngine->KeyboardEventsLocker.Unlock();
        return 1;
    }
    // Mouse
    else if( event == FL_PUSH || event == FL_RELEASE || ( event == FL_MOUSEWHEEL && Fl::event_dy() != 0 ) )
    {
        int event_button = Fl::event_button();
        int event_dy = Fl::event_dy();
        FOEngine->MouseEventsLocker.Lock();
        FOEngine->MouseEvents.push_back( event );
        FOEngine->MouseEvents.push_back( event_button );
        FOEngine->MouseEvents.push_back( event_dy );
        FOEngine->MouseEventsLocker.Unlock();
        return 1;
    }

    if( event == FL_FOCUS )
        MainWindow->focused = true;
    if( event == FL_UNFOCUS )
        MainWindow->focused = false;

    return 0;
}

/*
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
   #ifdef FO_D3D
            ShowWindow( Wnd, SW_MINIMIZE );
   #endif
            return 0;
        }
        break;
    case WM_SHOWWINDOW:
   #ifdef FO_D3D
        if( GameOpt.AlwaysOnTop )
            SetWindowPos( Wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
   #endif
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
   #ifdef FO_D3D
            if( GameOpt.MessNotify )
                FlashWindow( Wnd, true );
            if( GameOpt.SoundNotify )
                Beep( 100, 200 );
   #endif
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
   #ifdef FO_D3D
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint( Wnd, &ps );
            FOEngine->MainLoop();
            if( FOEngine->WindowLess )
                FOEngine->WindowLess->RepaintVideo( wnd, hdc );
            EndPaint( Wnd, &ps );
   #endif
            return 0;
        }

        / *if(FOEngine && FOEngine->WindowLess)
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
           }* /
        break;
    case WM_WINDOWPOSCHANGING:
        // Allow size greather than monitor resolution
        // Used in video playing workaround
        return 0;
   #endif
   / *	case WM_SETCURSOR:
                // Turn off window cursor
            SetCursor( NULL );
            return TRUE; // prevent Windows from setting cursor to window class cursor
        break;* /
    }

    return DefWindowProc( wnd, message, wparam, lparam );
   }
 */
