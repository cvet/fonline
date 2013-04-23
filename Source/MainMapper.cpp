#include "StdAfx.h"
#include "Mapper.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

FOWindow* MainWindow = NULL;
FOMapper* Mapper = NULL;

int main( int argc, char** argv )
{
    setlocale( LC_ALL, "Russian" );
    RestoreMainDirectory();

    // Threading
    #ifdef FO_WINDOWS
    pthread_win32_process_attach_np();
    #endif
    Thread::SetCurrentName( "GUI" );

    // Exceptions
    CatchExceptions( "FOnlineMapper", MAPPER_VERSION );

    // Make command line
    SetCommandLine( argc, argv );

    // Timer
    Timer::Init();

    LogToFile( "FOMapper.log" );

    GetClientOptions();

    WriteLog( "Starting Mapper (%s)...\n", MAPPER_VERSION_STR );

    // Create window
    MainWindow = new FOWindow();
    MainWindow->label( GetWindowName() );
    MainWindow->position( ( Fl::w() - MODE_WIDTH ) / 2, ( Fl::h() - MODE_HEIGHT ) / 2 );
    MainWindow->size( MODE_WIDTH, MODE_HEIGHT );

    // Icon
    #ifdef FO_WINDOWS
    MainWindow->icon( (char*) LoadIcon( fl_display, MAKEINTRESOURCE( 101 ) ) );
    #else
    // Todo: Linux
    #endif

    // OpenGL parameters
    #ifndef FO_D3D
    Fl::gl_visual( FL_RGB | FL_RGB8 | FL_DOUBLE | FL_DEPTH | FL_STENCIL  );
    #endif

    // Show window
    MainWindow->show();
    MainWindow->make_current();

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

    // Create engine
    Mapper = new FOMapper();
    if( !Mapper || !Mapper->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        GameOpt.Quit = true;
        return 0;
    }

    // Loop
    while( !GameOpt.Quit && Fl::check() )
        Mapper->MainLoop();
    GameOpt.Quit = true;

    // Destroy engine
    Mapper->Finish();
    SAFEDEL( Mapper );

    // Finish
    #ifdef FO_WINDOWS
    if( Singleplayer )
        SingleplayerData.Finish();
    #endif
    WriteLog( "FOnline finished.\n" );
    LogFinish();

    return 0;
}
