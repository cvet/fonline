#include "stdafx.h"
#include "Mapper.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

FOWindow* MainWindow = NULL;
FOMapper* Mapper = NULL;
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

    // Exceptions
    CatchExceptions( "FOnlineMapper", MAPPER_VERSION );

    // Send about new instance to already runned mapper
    if( FindWindow( GetWindowName(), GetWindowName() ) != NULL )
    {
        MessageBox( NULL, "FOnline already run.", "FOnline", MB_OK );
        return 0;
    }

    // Timer
    Timer::Init();

    LogToFile( "FOMapper.log" );

    GetClientOptions();

    WriteLog( "Starting Mapper (%s)...\n", MAPPER_VERSION_STR );

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

    // Fullscreen
    #ifndef FO_D3D
    if( GameOpt.FullScreen )
    {
        # ifdef FO_WINDOWS
        HDC dcscreen = GetDC( NULL );
        int sw = GetDeviceCaps( dcscreen, HORZRES );
        int sh = GetDeviceCaps( dcscreen, VERTRES );
        ReleaseDC( NULL, dcscreen );
        # endif
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
    Game.Start( GameThread );

    // Loop
    while( !GameOpt.Quit && Fl::wait() )
        ;
    GameOpt.Quit = true;
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
    Mapper = new (nothrow) FOMapper();
    if( !Mapper || !Mapper->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        GameOpt.Quit = true;
        return NULL;
    }

    // Loop
    while( !GameOpt.Quit )
    {
        Mapper->MainLoop();
        if( GameOpt.Sleep >= 0 )
            Sleep( GameOpt.Sleep );
    }

    // Finish
    Mapper->Finish();
    delete Mapper;
    return NULL;
}

int FOWindow::handle( int event )
{
    if( !Mapper || GameOpt.Quit )
        return 0;

    // Keyboard
    if( event == FL_KEYDOWN || event == FL_KEYUP )
    {
        int event_key = Fl::event_key();
        Mapper->KeyboardEventsLocker.Lock();
        Mapper->KeyboardEvents.push_back( event );
        Mapper->KeyboardEvents.push_back( event_key );
        Mapper->KeyboardEventsLocker.Unlock();
        return 1;
    }
    // Mouse
    else if( event == FL_PUSH || event == FL_RELEASE || ( event == FL_MOUSEWHEEL && Fl::event_dy() != 0 ) )
    {
        int event_button = Fl::event_button();
        int event_dy = Fl::event_dy();
        Mapper->MouseEventsLocker.Lock();
        Mapper->MouseEvents.push_back( event );
        Mapper->MouseEvents.push_back( event_button );
        Mapper->MouseEvents.push_back( event_dy );
        Mapper->MouseEventsLocker.Unlock();
        return 1;
    }

    if( event == FL_FOCUS )
        MainWindow->focused = true;
    if( event == FL_UNFOCUS )
        MainWindow->focused = false;

    return 0;
}
