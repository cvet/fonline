#include "StdAfx.h"
#include "Client.h"
#include "Exception.h"
#include "Version.h"
#include "Keyboard.h"
#include <locale.h>
#ifdef FO_LINUX
# include <signal.h>
#endif

FOWindow* MainWindow = NULL;
FOClient* FOEngine = NULL;
Thread    Game;
void GameThread( void* );

int main( int argc, char** argv )
{
    setlocale( LC_ALL, "Russian" );
    RestoreMainDirectory();

    // Threading
    #ifdef FO_WINDOWS
    pthread_win32_process_attach_np();
    #endif
    Thread::SetCurrentName( "GUI" );

    // Disable SIGPIPE signal
    #ifdef FO_LINUX
    signal( SIGPIPE, SIG_IGN );
    #endif

    // Exception
    CatchExceptions( "FOnline", CLIENT_VERSION );

    // Make command line
    SetCommandLine( argc, argv );

    // Stuff
    Timer::Init();
    LogToFile( "FOnline.log" );

    // Singleplayer mode initialization
    #ifdef FO_WINDOWS
    char full_path[ MAX_FOPATH ] = { 0 };
    char path[ MAX_FOPATH ] = { 0 };
    char name[ MAX_FOPATH ] = { 0 };
    GetModuleFileName( NULL, full_path, MAX_FOPATH );
    FileManager::ExtractPath( full_path, path );
    FileManager::ExtractFileName( full_path, name );
    if( Str::Substring( name, "Singleplayer" ) || Str::Substring( CommandLine, "Singleplayer" ) )
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
        Str::Format( command_line, "\"%s%s\" -singleplayer %p %p %s -logpath %s", server_path, server_exe, map_file, client_process, server_cmdline, path );
        if( !CreateProcess( NULL, command_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, server_path, &sui, &server ) )
        {
            WriteLog( "Can't start server process, error<%u>.\n", GetLastError() );
            return 0;
        }
        CloseHandle( server.hProcess );
        CloseHandle( server.hThread );
    }
    #endif

    // Init window threading
    #ifdef FO_LINUX
    XInitThreads();
    #endif
    Fl::lock();

    // Check for already runned window
    #ifndef DEV_VESRION
    # ifdef FO_WINDOWS
    if( !Singleplayer && FindWindow( GetWindowName(), GetWindowName() ) != NULL )
    {
        ShowMessage( "FOnline is running already." );
        return 0;
    }
    # else // FO_LINUX
    // Todo: Linux
    # endif
    #endif

    // Options
    GetClientOptions();

    // Create window
    MainWindow = new FOWindow();
    MainWindow->label( GetWindowName() );
    MainWindow->position( ( Fl::w() - MODE_WIDTH ) / 2, ( Fl::h() - MODE_HEIGHT ) / 2 );
    MainWindow->size( MODE_WIDTH, MODE_HEIGHT );
    // MainWindow->size_range( 100, 100 );

    // Icon
    #ifdef FO_WINDOWS
    MainWindow->icon( (char*) LoadIcon( fl_display, MAKEINTRESOURCE( 101 ) ) );
    #else // FO_LINUX
    // Todo: Linux
    #endif

    // OpenGL parameters
    #ifndef FO_D3D
    Fl::gl_visual( FL_RGB | FL_RGB8 | FL_DOUBLE | FL_DEPTH | FL_STENCIL );
    #endif

    // Show window
    MainWindow->show();

    // Hide cursor
    #ifdef FO_WINDOWS
    ShowCursor( FALSE );
    #else
    char   data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    XColor black;
    black.red = black.green = black.blue = 0;
    Pixmap nodata = XCreateBitmapFromData( fl_display, fl_xid( MainWindow ), data, 8, 8 );
    Cursor cur = XCreatePixmapCursor( fl_display, nodata, nodata, &black, &black, 0, 0 );
    XDefineCursor( fl_display, fl_xid( MainWindow ), cur );
    XFreeCursor( fl_display, cur );
    #endif

    // Fullscreen
    #ifndef FO_D3D
    if( GameOpt.FullScreen )
    {
        int sx, sy, sw, sh;
        Fl::screen_xywh( sx, sy, sw, sh );
        MainWindow->border( 0 );
        MainWindow->size( sw, sh );
        MainWindow->position( 0, 0 );
    }
    #endif

    // Hide menu
    #ifdef FO_WINDOWS
    SetWindowLong( fl_xid( MainWindow ), GWL_STYLE, GetWindowLong( fl_xid( MainWindow ), GWL_STYLE ) & ( ~WS_SYSMENU ) );
    #endif

    // Place on top
    #ifdef FO_WINDOWS
    if( GameOpt.AlwaysOnTop )
        SetWindowPos( fl_xid( MainWindow ), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
    #endif

    // Start
    WriteLog( "Starting FOnline (version %04X-%02X)...\n", CLIENT_VERSION, FO_PROTOCOL_VERSION & 0xFF );
    Game.Start( GameThread, "Main" );

    // Loop
    while( !GameOpt.Quit && Fl::wait() )
        ;
    Fl::unlock();
    GameOpt.Quit = true;
    Game.Wait();

    // Finish
    #ifdef FO_WINDOWS
    if( Singleplayer )
        SingleplayerData.Finish();
    #endif
    WriteLog( "FOnline finished.\n" );
    LogFinish( -1 );
    Timer::Finish();

    return 0;
}

void GameThread( void* )
{
    // Start
    FOEngine = new FOClient();
    if( !FOEngine || !FOEngine->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        GameOpt.Quit = true;
        return;
    }

    // Loop
    while( !GameOpt.Quit )
    {
        if( !FOEngine->MainLoop() )
            Sleep( 100 );
    }

    // Finish
    FOEngine->Finish();
    delete FOEngine;
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

    // Focus
    if( event == FL_FOCUS )
        MainWindow->focused = true;
    if( event == FL_UNFOCUS )
        MainWindow->focused = false;

    return 0;
}
