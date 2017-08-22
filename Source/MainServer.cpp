#include "Common.h"
#include "Server.h"
#include "Exception.h"
#include "Access.h"
#include "BufferManager.h"
#include <locale.h>
#ifndef FO_WINDOWS
# include <signal.h>
#endif
#ifndef SERVER_DAEMON
# include "FL/Fl.H"
# include "FL/Fl_Window.H"
# include "FL/Fl_Box.H"
# include "FL/Fl_Text_Display.H"
# include "FL/Fl_Button.H"
# include "FL/Fl_Check_Button.H"
# include "FL/Fl_File_Icon.H"
# ifdef FO_MSVC
#  pragma comment( lib, "fltk.lib" )
# endif
#endif

static void InitAdminManager();

/************************************************************************/
/* GUI & Windows service version                                        */
/************************************************************************/

#ifndef SERVER_DAEMON
static void GUIInit();
static void GUICallback( Fl_Widget* widget, void* data );
static void UpdateInfo();
static void UpdateLog();
static void CheckTextBoxSize( bool force );
static void GameLoopThread( void* );
static void GUIUpdate( void* );
static Rect       MainInitRect, LogInitRect, InfoInitRect;
static int        SplitProcent = 90;
static Thread     LoopThread;
static MutexEvent GameInitEvent;
static FOServer   Server;
static string     UpdateLogName;
static Thread     GUIUpdateThread;

// GUI
static Fl_Window* GuiWindow;
static Fl_Box*    GuiLabelGameTime, * GuiLabelClients, * GuiLabelIngame, * GuiLabelNPC, * GuiLabelLocCount,
* GuiLabelItemsCount, * GuiLabelFPS, * GuiLabelDelta, * GuiLabelUptime, * GuiLabelSend, * GuiLabelRecv, * GuiLabelCompress;
static Fl_Button* GuiBtnRlClScript, * GuiBtnSaveWorld, * GuiBtnSaveLog, * GuiBtnSaveInfo,
* GuiBtnCreateDump, * GuiBtnMemory, * GuiBtnPlayers, * GuiBtnLocsMaps, * GuiBtnDeferredCalls,
* GuiBtnProperties, * GuiBtnItemsCount, * GuiBtnProfiler, * GuiBtnStartStop, * GuiBtnSplitUp, * GuiBtnSplitDown;
static Fl_Check_Button* GuiCBtnAutoUpdate;
static Fl_Text_Display* GuiLog, * GuiInfo;
static int              GUISizeMod = 0;

static bool             GracefulExit = false;

static int              GUIMessageUpdate = 0;
static int              GUIMessageExit = 1;

# define GUI_SIZE1( x )                 ( (int) ( x ) * 175 * ( 100 + GUISizeMod ) / 100 / 100 )
# define GUI_SIZE2( x1, x2 )            GUI_SIZE1( x1 ), GUI_SIZE1( x2 )
# define GUI_SIZE4( x1, x2, x3, x4 )    GUI_SIZE1( x1 ), GUI_SIZE1( x2 ), GUI_SIZE1( x3 ), GUI_SIZE1( x4 )
# define GUI_LABEL_BUF_SIZE    ( 128 )

// Windows service
# ifdef FO_WINDOWS
static void ServiceMain( bool as_service );
# endif

// Main
int main( int argc, char** argv )
{
    InitialSetup( argc, argv );

    // Threading
    Thread::SetCurrentName( "GUI" );

    // Disable SIGPIPE signal
    # ifndef FO_WINDOWS
    signal( SIGPIPE, SIG_IGN );
    # endif

    // Exceptions catcher
    CatchExceptions( "FOnlineServer", FONLINE_VERSION );

    // Timer
    Timer::Init();

    // Memory debugging
    MemoryDebugLevel = MainConfig->GetInt( "", "MemoryDebugLevel", 0 );
    if( MemoryDebugLevel >= 3 )
        Debugger::StartTraceMemory();

    // Update stuff
    if( !Singleplayer && MainConfig->IsKey( "", "GameServer" ) )
        GameOpt.GameServer = true, GameOpt.UpdateServer = false;
    else if( !Singleplayer && MainConfig->IsKey( "", "UpdateServer" ) )
        GameOpt.GameServer = false, GameOpt.UpdateServer = true;
    else
        GameOpt.GameServer = true, GameOpt.UpdateServer = true;

    // Init event
    GameInitEvent.Disallow();

    // Service
    if( MainConfig->IsKey( "", "Service" ) )
    {
        # ifdef FO_WINDOWS
        ServiceMain( MainConfig->IsKey( "", "StartService" ) );
        # endif
        return 0;
    }

    // Check single player parameters
    if( MainConfig->IsKey( "", "Singleplayer" ) )
    {
        # ifdef FO_WINDOWS
        Singleplayer = true;
        GameOpt.Singleplayer = true;
        Timer::SetGamePause( true );

        // Logging
        string log_path;
        if( !MainConfig->IsKey( "", "NoLogPath" ) && MainConfig->IsKey( "", "LogPath" ) )
            log_path = MainConfig->GetStr( "", "LogPath" );
        log_path = _str( log_path ).trim() + "FOnlineServer.log";
        LogToFile( log_path );

        WriteLog( "Singleplayer mode.\n" );

        // Shared data
        const char* ptr = MainConfig->GetStr( "", "Singleplayer" );
        HANDLE      map_file = nullptr;
        if( sscanf( ptr, "%p%p", &map_file, &SingleplayerClientProcess ) != 2 || !SingleplayerData.Attach( map_file ) )
        {
            WriteLog( "Can't attach to mapped file {}.\n", map_file );
            return 0;
        }
        # else
        return 0;
        # endif
    }

    // GUI
    if( !Singleplayer || MainConfig->IsKey( "", "ShowGui" ) )
    {
        Fl::lock();         // Begin GUI multi threading
        GUIInit();
        LogToFile( "" );
        LogToBuffer( true );
    }

    FOQuit = true;

    if( GuiWindow )
    {
        LogToBuffer( true );
        GuiCBtnAutoUpdate->value( 0 );
    }

    WriteLog( "FOnline server, version {}.\n", FONLINE_VERSION );

    // Autostart
    if( !MainConfig->IsKey( "", "NoStart" ) )
    {
        if( GuiWindow )
        {
            GuiBtnStartStop->do_callback();
        }
        else
        {
            FOQuit = false;
            LoopThread.Start( GameLoopThread, "Main" );
        }
    }

    // Start admin manager
    InitAdminManager();

    // Loop
    if( GuiWindow )
    {
        GUIUpdateThread.Start( GUIUpdate, "GUIUpdate" );
        while( Fl::wait() )
        {
            int* msg = (int*) Fl::thread_message();
            if( msg == &GUIMessageUpdate )
            {
                UpdateLog();
                UpdateInfo();
                CheckTextBoxSize( false );
            }
            else if( msg == &GUIMessageExit )
            {
                // Disable buttons
                GuiBtnRlClScript->deactivate();
                GuiBtnSaveWorld->deactivate();
                GuiBtnPlayers->deactivate();
                GuiBtnLocsMaps->deactivate();
                GuiBtnDeferredCalls->deactivate();
                GuiBtnProperties->deactivate();
                GuiBtnItemsCount->deactivate();
                GuiBtnProfiler->deactivate();
                GuiBtnSaveInfo->deactivate();
                break;
            }
        }
        Fl::unlock();
    }
    else
    {
        while( !FOQuit )
            Thread_Sleep( 100 );
    }

    return 0;
}

static void GUIInit()
{
    // Setup
    struct
    {
        int  FontType;
        int  FontSize;

        void Setup( Fl_Box* widget )
        {
            widget->labelfont( FontType );
            widget->labelsize( FontSize );
            widget->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
            widget->box( FL_NO_BOX );
            widget->label( new char[ GUI_LABEL_BUF_SIZE ] );
            *(char*) widget->label() = 0;
        }

        void Setup( Fl_Button* widget )
        {
            widget->labelfont( FontType );
            widget->labelsize( FontSize );
            widget->callback( GUICallback );
        }

        void Setup( Fl_Check_Button* widget )
        {
            widget->labelfont( FontType );
            widget->labelsize( FontSize );
            widget->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
            widget->callback( GUICallback );
        }

        void Setup( Fl_Text_Display* widget )
        {
            widget->labelfont( FontType );
            widget->labelsize( FontSize );
            widget->buffer( new Fl_Text_Buffer() );
            widget->textfont( FontType );
            widget->textsize( FontSize );
        }
    } GUISetup;

    GUISizeMod = MainConfig->GetInt( "", "GUISize", 0 );
    GUISetup.FontType = FL_COURIER;
    GUISetup.FontSize = 11;

    // Main window
    int wx = MainConfig->GetInt( "", "PositionX", 0 );
    int wy = MainConfig->GetInt( "", "PositionY", 0 );
    if( !wx && !wy )
        wx = ( Fl::w() - GUI_SIZE1( 496 ) ) / 2, wy = ( Fl::h() - GUI_SIZE1( 412 ) ) / 2;
    GuiWindow = new Fl_Window( wx, wy, GUI_SIZE2( 496, 412 ), "FOnline Server" );
    GuiWindow->labelfont( GUISetup.FontType );
    GuiWindow->labelsize( GUISetup.FontSize );
    GuiWindow->callback( GUICallback );
    GuiWindow->size_range( GUI_SIZE2( 129, 129 ) );

    // Name
    string title = MainConfig->GetStr( "", "WindowName", "FOnline" );
    if( GameOpt.GameServer && !GameOpt.UpdateServer )
        title += " GAME";
    else if( !GameOpt.GameServer && GameOpt.UpdateServer )
        title += " UPDATE";
    GuiWindow->label( title.c_str() );

    // Icon
    # ifdef FO_WINDOWS
    GuiWindow->icon( (char*) LoadIcon( fl_display, MAKEINTRESOURCE( 101 ) ) );
    # else
    fl_open_display();
    // Todo: linux
    # endif

    // Labels
    GUISetup.Setup( GuiLabelGameTime    = new Fl_Box( GUI_SIZE4( 5, 6, 128, 8 ), "Time:" ) );
    GUISetup.Setup( GuiLabelClients     = new Fl_Box( GUI_SIZE4( 5, 14, 124, 8 ), "Connections:" ) );
    GUISetup.Setup( GuiLabelIngame      = new Fl_Box( GUI_SIZE4( 5, 22, 124, 8 ), "Players in game:" ) );
    GUISetup.Setup( GuiLabelNPC         = new Fl_Box( GUI_SIZE4( 5, 30, 124, 8 ), "NPC in game:" ) );
    GUISetup.Setup( GuiLabelLocCount    = new Fl_Box( GUI_SIZE4( 5, 38, 124, 8 ), "Locations:" ) );
    GUISetup.Setup( GuiLabelItemsCount  = new Fl_Box( GUI_SIZE4( 5, 46, 124, 8 ), "Items:" ) );
    GUISetup.Setup( GuiLabelFPS         = new Fl_Box( GUI_SIZE4( 5, 54, 124, 8 ), "Cycles per second:" ) );
    GUISetup.Setup( GuiLabelDelta       = new Fl_Box( GUI_SIZE4( 5, 62, 124, 8 ), "Cycle time:" ) );
    GUISetup.Setup( GuiLabelUptime      = new Fl_Box( GUI_SIZE4( 5, 70, 124, 8 ), "Uptime:" ) );
    GUISetup.Setup( GuiLabelSend        = new Fl_Box( GUI_SIZE4( 5, 78, 124, 8 ), "KBytes send:" ) );
    GUISetup.Setup( GuiLabelRecv        = new Fl_Box( GUI_SIZE4( 5, 86, 124, 8 ), "KBytes recv:" ) );
    GUISetup.Setup( GuiLabelCompress    = new Fl_Box( GUI_SIZE4( 5, 94, 124, 8 ), "Compress ratio:" ) );

    // Buttons
    GUISetup.Setup( GuiBtnRlClScript = new Fl_Button( GUI_SIZE4( 5, 128, 124, 14 ), "Reload client scripts" ) );
    GUISetup.Setup( GuiBtnSaveWorld = new Fl_Button( GUI_SIZE4( 5, 144, 124, 14 ), "Save world" ) );
    GUISetup.Setup( GuiBtnSaveLog   = new Fl_Button( GUI_SIZE4( 5, 160, 124, 14 ), "Save log" ) );
    GUISetup.Setup( GuiBtnSaveInfo  = new Fl_Button( GUI_SIZE4( 5, 176, 124, 14 ), "Save info" ) );
    GUISetup.Setup( GuiBtnCreateDump = new Fl_Button( GUI_SIZE4( 5, 192, 124, 14 ), "Create dump" ) );
    GUISetup.Setup( GuiBtnMemory    = new Fl_Button( GUI_SIZE4( 5, 219, 124, 14 ), "Memory usage" ) );
    GUISetup.Setup( GuiBtnPlayers   = new Fl_Button( GUI_SIZE4( 5, 235, 124, 14 ), "Players" ) );
    GUISetup.Setup( GuiBtnLocsMaps  = new Fl_Button( GUI_SIZE4( 5, 251, 124, 14 ), "Locations and maps" ) );
    GUISetup.Setup( GuiBtnDeferredCalls = new Fl_Button( GUI_SIZE4( 5, 267, 124, 14 ), "Deferred calls" ) );
    GUISetup.Setup( GuiBtnProperties   = new Fl_Button( GUI_SIZE4( 5, 283, 124, 14 ), "Properties" ) );
    GUISetup.Setup( GuiBtnItemsCount = new Fl_Button( GUI_SIZE4( 5, 299, 124, 14 ), "Items count" ) );
    GUISetup.Setup( GuiBtnProfiler = new Fl_Button( GUI_SIZE4( 5, 315, 124, 14 ), "Profiler" ) );
    GUISetup.Setup( GuiBtnStartStop = new Fl_Button( GUI_SIZE4( 5, 393, 124, 14 ), "Start server" ) );
    GUISetup.Setup( GuiBtnSplitUp   = new Fl_Button( GUI_SIZE4( 117, 357, 12, 9 ), "" ) );
    GUISetup.Setup( GuiBtnSplitDown = new Fl_Button( GUI_SIZE4( 117, 368, 12, 9 ), "" ) );

    // Check buttons
    GUISetup.Setup( GuiCBtnAutoUpdate   = new Fl_Check_Button( GUI_SIZE4( 5, 339, 110, 10 ), "Update info every second" ) );
    // GUISetup.Setup( GuiCBtnLogging      = new Fl_Check_Button( GUI_SIZE4( 5, 349, 110, 10 ), "Logging" ) );
    // GUISetup.Setup( GuiCBtnLoggingTime  = new Fl_Check_Button( GUI_SIZE4( 5, 359, 110, 10 ), "Logging with time" ) );
    // GUISetup.Setup( GuiCBtnLoggingThread = new Fl_Check_Button( GUI_SIZE4( 5, 369, 110, 10 ), "Logging with thread" ) );
    // GUISetup.Setup( GuiCBtnScriptDebug  = new Fl_Check_Button( GUI_SIZE4( 5, 379, 110, 10 ), "Script debug info" ) );

    // Text boxes
    GUISetup.Setup( GuiLog = new Fl_Text_Display( GUI_SIZE4( 133, 7, 358, 195 ) ) );
    GUISetup.Setup( GuiInfo = new Fl_Text_Display( GUI_SIZE4( 133, 204, 358, 203 ) ) );

    // Disable buttons
    GuiBtnRlClScript->deactivate();
    GuiBtnSaveWorld->deactivate();
    GuiBtnPlayers->deactivate();
    GuiBtnLocsMaps->deactivate();
    GuiBtnDeferredCalls->deactivate();
    GuiBtnProperties->deactivate();
    GuiBtnItemsCount->deactivate();
    GuiBtnProfiler->deactivate();
    GuiBtnSaveInfo->deactivate();

    // Give initial focus to Start / Stop
    GuiBtnStartStop->take_focus();

    // Info
    MainInitRect( GuiWindow->x(), GuiWindow->y(), GuiWindow->x() + GuiWindow->w(), GuiWindow->y() + GuiWindow->h() );
    LogInitRect( GuiLog->x(), GuiLog->y(), GuiLog->x() + GuiLog->w(), GuiLog->y() + GuiLog->h() );
    InfoInitRect( GuiInfo->x(), GuiInfo->y(), GuiInfo->x() + GuiInfo->w(), GuiInfo->y() + GuiInfo->h() );
    UpdateInfo();

    // Show window
    char  dummy_argv0[ 2 ] = "";
    char* dummy_argv[] = { dummy_argv0 };
    int   dummy_argc = 1;
    GuiWindow->show( dummy_argc, dummy_argv );
}

static void GUICallback( Fl_Widget* widget, void* data )
{
    if( widget == GuiWindow )
    {
        if( !GracefulExit )
        {
            if( !Server.Started() || Fl::get_key( FL_Shift_L ) )
            {
                ExitProcess( 0 );
            }
            else
            {
                GuiWindow->label( "Graceful exit. Please wait..." );
                GracefulExit = true;
                FOQuit = true;
            }
        }
    }
    else if( widget == GuiBtnRlClScript )
    {
        if( Server.Started() )
            Server.RequestReloadClientScripts = true;
    }
    else if( widget == GuiBtnSaveWorld )
    {
        if( Server.Started() )
            Server.SaveWorldNextTick = Timer::FastTick();                       // Force saving time
    }
    else if( widget == GuiBtnSaveLog || widget == GuiBtnSaveInfo )
    {
        DateTimeStamp    dt;
        Timer::GetCurrentDateTime( dt );
        Fl_Text_Display* log = ( widget == GuiBtnSaveLog ? GuiLog : GuiInfo );
        string           log_name_dir = FileManager::GetWritePath( "Logs/" );
        string           log_name = _str( "{}FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log",
                                          log_name_dir, log == GuiInfo ? UpdateLogName : "Log", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );
        FileManager::CreateDirectoryTree( log_name );
        log->buffer()->savefile( log_name.c_str() );
    }
    else if( widget == GuiBtnCreateDump )
    {
        CreateDump( "ManualDump", "Manual" );
    }
    else if( widget == GuiBtnMemory )
    {
        FOServer::UpdateIndex = 0;
        FOServer::UpdateLastIndex = 0;
        if( !Server.Started() )
            UpdateInfo();
    }
    else if( widget == GuiBtnPlayers )
    {
        FOServer::UpdateIndex = 1;
        FOServer::UpdateLastIndex = 1;
    }
    else if( widget == GuiBtnLocsMaps )
    {
        FOServer::UpdateIndex = 2;
        FOServer::UpdateLastIndex = 2;
    }
    else if( widget == GuiBtnDeferredCalls )
    {
        FOServer::UpdateIndex = 3;
        FOServer::UpdateLastIndex = 3;
    }
    else if( widget == GuiBtnProperties )
    {
        FOServer::UpdateIndex = 4;
        FOServer::UpdateLastIndex = 4;
    }
    else if( widget == GuiBtnItemsCount )
    {
        FOServer::UpdateIndex = 5;
        FOServer::UpdateLastIndex = 5;
    }
    else if( widget == GuiBtnProfiler )
    {
        FOServer::UpdateIndex = 6;
        FOServer::UpdateLastIndex = 6;
    }
    else if( widget == GuiBtnStartStop )
    {
        if( !FOQuit )       // End of work
        {
            FOQuit = true;
            GuiBtnStartStop->copy_label( "Start server" );
            GuiBtnStartStop->deactivate();

            // Disable buttons
            GuiBtnRlClScript->deactivate();
            GuiBtnSaveWorld->deactivate();
            GuiBtnPlayers->deactivate();
            GuiBtnLocsMaps->deactivate();
            GuiBtnDeferredCalls->deactivate();
            GuiBtnProperties->deactivate();
            GuiBtnItemsCount->deactivate();
        }
        else         // Begin work
        {
            GuiBtnStartStop->copy_label( "Stop server" );
            GuiBtnStartStop->deactivate();

            FOQuit = false;
            LoopThread.Start( GameLoopThread, "Main" );
        }
    }
    else if( widget == GuiBtnSplitUp )
    {
        if( SplitProcent >= 50 )
            SplitProcent -= 40;
        CheckTextBoxSize( true );
        GuiLog->scroll( MAX_INT, 0 );
    }
    else if( widget == GuiBtnSplitDown )
    {
        if( SplitProcent <= 50 )
            SplitProcent += 40;
        CheckTextBoxSize( true );
        GuiLog->scroll( MAX_INT, 0 );
    }
    else if( widget == GuiCBtnAutoUpdate )
    {
        if( GuiCBtnAutoUpdate->value() )
            FOServer::UpdateLastTick = Timer::FastTick();
        else
            FOServer::UpdateLastTick = 0;
    }
}

static void GUIUpdate( void* )
{
    while( true )
    {
        Fl::awake( &GUIMessageUpdate );
        Thread_Sleep( 50 );
    }
}

static void UpdateInfo()
{
    struct Label
    {
        static void Update( Fl_Box* label, const string& text )
        {
            if( !Str::Compare( text, (char*) label->label() ) )
            {
                Str::Copy( (char*) label->label(), GUI_LABEL_BUF_SIZE, text.c_str() );
                label->redraw_label();
            }
        }
    };

    if( Server.Started() )
    {
        DateTimeStamp st = Timer::GetGameTime( GameOpt.FullSecond );
        Label::Update( GuiLabelGameTime, _str( "Time: {:02}.{:02}.{:04} {:02}:{:02}:{:02} x{}", st.Day, st.Month, st.Year, st.Hour, st.Minute, st.Second, GameOpt.TimeMultiplier ) );
        Label::Update( GuiLabelClients, _str( "Connections: {}", Server.Statistics.CurOnline ) );
        Label::Update( GuiLabelIngame, _str( "Players in game: {}", CrMngr.PlayersInGame() ) );
        Label::Update( GuiLabelNPC, _str( "NPC in game: {}", CrMngr.NpcInGame() ) );
        Label::Update( GuiLabelLocCount, _str( "Locations: {} ({})", MapMngr.GetLocationsCount(), MapMngr.GetMapsCount() ) );
        Label::Update( GuiLabelItemsCount, _str( "Items: {}", ItemMngr.GetItemsCount() ) );
        Label::Update( GuiLabelFPS, _str( "Cycles per second: {}", Server.Statistics.FPS ) );
        Label::Update( GuiLabelDelta, _str( "Cycle time: {}", Server.Statistics.CycleTime ) );
    }
    else
    {
        Label::Update( GuiLabelGameTime, _str( "Time: n/a" ) );
        Label::Update( GuiLabelClients, _str( "Connections: n/a" ) );
        Label::Update( GuiLabelIngame, _str( "Players in game: n/a" ) );
        Label::Update( GuiLabelNPC, _str( "NPC in game: n/a" ) );
        Label::Update( GuiLabelLocCount, _str( "Locations: n/a" ) );
        Label::Update( GuiLabelItemsCount, _str( "Items: n/a" ) );
        Label::Update( GuiLabelFPS, _str( "Cycles per second: n/a" ) );
        Label::Update( GuiLabelDelta, _str( "Cycle time: n/a" ) );
    }

    uint seconds = Server.Statistics.Uptime;
    Label::Update( GuiLabelUptime, _str( "Uptime: {:02}:{:02}:{:02}", seconds / 60 / 60, seconds / 60 % 60, seconds % 60 ) );
    Label::Update( GuiLabelSend, _str( "KBytes Send: {}", Server.Statistics.BytesSend / 1024 ) );
    Label::Update( GuiLabelRecv, _str( "KBytes Recv: {}", Server.Statistics.BytesRecv / 1024 ) );
    Label::Update( GuiLabelCompress, _str( "Compress ratio: {}", (double) Server.Statistics.DataReal / ( Server.Statistics.DataCompressed ? Server.Statistics.DataCompressed : 1 ) ) );

    if( FOServer::UpdateIndex == -1 && FOServer::UpdateLastTick && FOServer::UpdateLastTick + 1000 < Timer::FastTick() )
    {
        FOServer::UpdateIndex = FOServer::UpdateLastIndex;
        FOServer::UpdateLastTick = Timer::FastTick();
    }

    if( FOServer::UpdateIndex != -1 )
    {
        string info;
        switch( FOServer::UpdateIndex )
        {
        case 0:         // Memory
            info = Debugger::GetMemoryStatistics();
            UpdateLogName = "Memory";
            break;
        case 1:         // Players
            if( !Server.Started() )
                break;
            info = Server.GetIngamePlayersStatistics();
            UpdateLogName = "Players";
            break;
        case 2:         // Locations and maps
            if( !Server.Started() )
                break;
            info = MapMngr.GetLocationsMapsStatistics();
            UpdateLogName = "LocationsAndMaps";
            break;
        case 3:         // Deferred calls
            if( !Server.Started() )
                break;
            info = Script::GetDeferredCallsStatistics();
            UpdateLogName = "DeferredCalls";
            break;
        case 4:         // Properties
            if( !Server.Started() )
                break;
            info = "WIP";
            UpdateLogName = "Properties";
            break;
        case 5:         // Items count
            if( !Server.Started() )
                break;
            info = ItemMngr.GetItemsStatistics();
            UpdateLogName = "ItemsCount";
            break;
        case 6:         // Profiler
            info = Script::GetProfilerStatistics();
            UpdateLogName = "Profiler";
            break;
        default:
            UpdateLogName = "";
            break;
        }
        GuiInfo->buffer()->text( info.c_str() );
        if( !GuiBtnSaveInfo->active() )
            GuiBtnSaveInfo->activate();
        FOServer::UpdateIndex = -1;
    }
}

static void UpdateLog()
{
    string str;
    LogGetBuffer( str );
    if( str.length() )
    {
        GuiLog->buffer()->append( str.c_str() );
        if( Fl::focus() != GuiLog )
            GuiLog->scroll( MAX_INT, 0 );
    }
}

static void CheckTextBoxSize( bool force )
{
    static Rect last_rmain;
    if( force || GuiWindow->x() != last_rmain[ 0 ] || GuiWindow->y() != last_rmain[ 1 ] ||
        GuiWindow->x() + GuiWindow->w() != last_rmain[ 2 ] || GuiWindow->y() + GuiWindow->h() != last_rmain[ 3 ] )
    {
        Rect rmain( GuiWindow->x(), GuiWindow->y(), GuiWindow->x() + GuiWindow->w(), GuiWindow->y() + GuiWindow->h() );
        if( rmain.W() > 0 && rmain.H() > 0 )
        {
            int  wdiff = rmain.W() - MainInitRect.W();
            int  hdiff = rmain.H() - MainInitRect.H();

            Rect rlog( GuiLog->x(), GuiLog->y(), GuiLog->x() + GuiLog->w(), GuiLog->y() + GuiLog->h() );
            Rect rinfo( GuiInfo->x(), GuiInfo->y(), GuiInfo->x() + GuiInfo->w(), GuiInfo->y() + GuiInfo->h() );

            int  hall = LogInitRect.H() + InfoInitRect.H() + hdiff;
            int  wlog = LogInitRect.W() + wdiff;
            int  hlog = hall * SplitProcent / 100;
            int  winfo = InfoInitRect.W() + wdiff;
            int  hinfo = hall * ( 100 - SplitProcent ) / 100;
            int  yinfo = hlog - LogInitRect.H();

            GuiLog->position( LogInitRect.L, LogInitRect.T );
            GuiLog->size( wlog, hlog );
            GuiInfo->position( InfoInitRect.L, InfoInitRect.T + yinfo );
            GuiInfo->size( winfo, hinfo );
            GuiLog->redraw();
            GuiInfo->redraw();
            GuiWindow->redraw();
        }
        last_rmain = rmain;
    }
}

static void GameLoopThread( void* )
{
    GetServerOptions();

    if( Server.Init() )
    {
        if( GuiWindow )
        {
            // Enable buttons
            GuiBtnRlClScript->activate();
            GuiBtnSaveWorld->activate();
            GuiBtnPlayers->activate();
            GuiBtnLocsMaps->activate();
            GuiBtnDeferredCalls->activate();
            GuiBtnProperties->activate();
            GuiBtnItemsCount->activate();
            GuiBtnStartStop->activate();
            GuiBtnProfiler->activate();
        }

        GameInitEvent.Allow();
        Server.MainLoop();
        Server.Finish();
        UpdateInfo();
    }
    else
    {
        WriteLog( "Initialization fail!\n" );
        GameInitEvent.Allow();
    }

    if( GuiWindow )
        UpdateLog();
    if( Singleplayer )
        ExitProcess( 0 );
    if( GuiWindow && GracefulExit )
        Fl::awake( &GUIMessageExit );
}

#endif // !SERVER_DAEMON

/************************************************************************/
/* Windows service                                                      */
/************************************************************************/
#ifdef FO_WINDOWS

static SERVICE_STATUS_HANDLE FOServiceStatusHandle;
static VOID WINAPI FOServiceStart( DWORD argc, LPTSTR* argv );
static VOID WINAPI FOServiceCtrlHandler( DWORD opcode );
static void        SetFOServiceStatus( uint state );

static void ServiceMain( bool as_service )
{
    // Binary started as service
    if( as_service )
    {
        // Start
        SERVICE_TABLE_ENTRY dispatch_table[] = { { Str::Duplicate( "FOnlineServer" ), FOServiceStart }, { nullptr, nullptr } };
        StartServiceCtrlDispatcher( dispatch_table );
        return;
    }

    // Open service manager
    SC_HANDLE manager = OpenSCManager( nullptr, nullptr, SC_MANAGER_ALL_ACCESS );
    if( !manager )
    {
        MessageBox( nullptr, "Can't open service manager.", "FOnlineServer", MB_OK | MB_ICONHAND );
        return;
    }

    // Delete service
    if( MainConfig->IsKey( "", "Delete" ) )
    {
        SC_HANDLE service = OpenService( manager, "FOnlineServer", DELETE );

        if( service && DeleteService( service ) )
            MessageBox( nullptr, "Service deleted.", "FOnlineServer", MB_OK | MB_ICONASTERISK );
        else
            MessageBox( nullptr, "Can't delete service.", "FOnlineServer", MB_OK | MB_ICONHAND );

        CloseServiceHandle( service );
        CloseServiceHandle( manager );
        return;
    }

    // Manage service
    SC_HANDLE service = OpenService( manager, "FOnlineServer", SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START );

    // Compile service path
    char   exe_path[ MAX_FOPATH ];
    GetModuleFileName( GetModuleHandle( nullptr ), exe_path, MAX_FOPATH );
    string path = _str( "\"{}\" --service {}", exe_path, GameOpt.CommandLine );

    // Change executable path, if changed
    if( service )
    {
        LPQUERY_SERVICE_CONFIG service_cfg = (LPQUERY_SERVICE_CONFIG) calloc( 8192, 1 );
        DWORD                  dw;
        if( QueryServiceConfig( service, service_cfg, 8192, &dw ) && !Str::CompareCase( service_cfg->lpBinaryPathName, path ) )
            ChangeServiceConfig( service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
        free( service_cfg );
    }

    // Register service
    if( !service )
    {
        service = CreateService( manager, "FOnlineServer", "FOnlineServer", SERVICE_ALL_ACCESS,
                                 SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr );

        if( service )
            MessageBox( nullptr, "\'FOnlineServer\' service registered.", "FOnlineServer", MB_OK | MB_ICONASTERISK );
        else
            MessageBox( nullptr, "Can't register \'FOnlineServer\' service.", "FOnlineServer", MB_OK | MB_ICONHAND );
    }

    // Close handles
    if( service )
        CloseServiceHandle( service );
    if( manager )
        CloseServiceHandle( manager );
}

static VOID WINAPI FOServiceStart( DWORD argc, LPTSTR* argv )
{
    Thread::SetCurrentName( "Service" );
    LogToFile( "FOnlineServer.log" );
    WriteLog( "FOnline server service, version {}.\n", FONLINE_VERSION );

    FOServiceStatusHandle = RegisterServiceCtrlHandler( "FOnlineServer", FOServiceCtrlHandler );
    if( !FOServiceStatusHandle )
        return;

    // Start admin manager
    InitAdminManager();

    // Start game
    SetFOServiceStatus( SERVICE_START_PENDING );

    FOQuit = false;
    LoopThread.Start( GameLoopThread, "Main" );
    GameInitEvent.Wait();

    if( Server.Started() )
        SetFOServiceStatus( SERVICE_RUNNING );
    else
        SetFOServiceStatus( SERVICE_STOPPED );
}

static VOID WINAPI FOServiceCtrlHandler( DWORD opcode )
{
    switch( opcode )
    {
    case SERVICE_CONTROL_STOP:
        SetFOServiceStatus( SERVICE_STOP_PENDING );
        FOQuit = true;

        LoopThread.Wait();
        SetFOServiceStatus( SERVICE_STOPPED );
        return;
    case SERVICE_CONTROL_INTERROGATE:
        // Fall through to send current status
        break;
    default:
        break;
    }

    // Send current status
    SetFOServiceStatus( 0 );
}

static void SetFOServiceStatus( uint state )
{
    static uint last_state = 0;
    static uint check_point = 0;

    if( !state )
        state = last_state;
    else
        last_state = state;

    SERVICE_STATUS srv_status;
    srv_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    srv_status.dwCurrentState = state;
    srv_status.dwWin32ExitCode = 0;
    srv_status.dwServiceSpecificExitCode = 0;
    srv_status.dwWaitHint = 0;
    srv_status.dwCheckPoint = 0;
    srv_status.dwControlsAccepted = 0;

    if( state == SERVICE_RUNNING )
        srv_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    if( !( state == SERVICE_RUNNING || state == SERVICE_STOPPED ) )
        srv_status.dwCheckPoint = ++check_point;

    SetServiceStatus( FOServiceStatusHandle, &srv_status );
}

#endif // FO_WINDOWS

/************************************************************************/
/* Linux daemon                                                         */
/************************************************************************/
#ifdef SERVER_DAEMON

# include <sys/stat.h>

static void DaemonLoop();
static void GameLoopThread( void* );
static FOServer Server;
static Thread   LoopThread;

int main( int argc, char** argv )
{
    InitialSetup( argc, argv );

    if( !Str::Substring( GameOpt.CommandLine.c_str(), "-nodetach" ) )
    {
        // Start daemon
        pid_t parpid = fork();
        if( parpid < 0 )
        {
            WriteLog( "Create child process (fork) fail, error '{}'.", ERRORSTR );
            return 1;
        }
        else if( parpid != 0 )
        {
            // Close parent process
            return 0;
        }

        close( STDIN_FILENO );
        close( STDOUT_FILENO );
        close( STDERR_FILENO );

        if( setsid() < 0 )
        {
            WriteLog( "Generate process index (setsid) fail, error '{}'.\n", ERRORSTR );
            return 1;
        }
    }

    umask( 0 );

    // Stuff
    setlocale( LC_ALL, "Russian" );

    // Threading
    Thread::SetCurrentName( "Daemon" );

    // Disable SIGPIPE signal
    # ifndef FO_WINDOWS
    signal( SIGPIPE, SIG_IGN );
    # endif

    // Exceptions catcher
    CatchExceptions( "FOnlineServer", FONLINE_VERSION );

    // Timer
    Timer::Init();

    // Memory debugging
    MemoryDebugLevel = MainConfig->GetInt( "", "MemoryDebugLevel", 0 );
    if( MemoryDebugLevel >= 3 )
        Debugger::StartTraceMemory();

    // Logging
    LogToFile( "./FOnlineServerDaemon.log" );

    // Log version
    WriteLog( "FOnline server daemon, version {}.\n", FONLINE_VERSION );
    if( !GameOpt.CommandLine.empty() )
        WriteLog( "Command line '{}'.\n", GameOpt.CommandLine );

    // Update stuff
    if( !Singleplayer && Str::Substring( GameOpt.CommandLine.c_str(), "-game" ) )
        GameOpt.GameServer = true, GameOpt.UpdateServer = false;
    else if( !Singleplayer && Str::Substring( GameOpt.CommandLine.c_str(), "-update" ) )
        GameOpt.GameServer = false, GameOpt.UpdateServer = true;
    else
        GameOpt.GameServer = true, GameOpt.UpdateServer = true;

    DaemonLoop(); // Never out from here
    return 0;
}

static void DaemonLoop()
{
    // Autostart server
    LoopThread.Start( GameLoopThread, "Main" );

    // Start admin manager
    InitAdminManager();

    // Daemon loop
    while( true )
        Thread_Sleep( 1000 );
}

static void GameLoopThread( void* )
{
    GetServerOptions();

    if( Server.Init() )
    {
        FOQuit = false;
        Server.MainLoop();
        Server.Finish();
    }
    else
    {
        WriteLog( "Initialization fail!\n" );
    }
}

#endif // SERVER_DAEMON

/************************************************************************/
/* Admin panel                                                          */
/************************************************************************/

#define MAX_SESSIONS    ( 10 )

struct Session
{
    int           RefCount;
    SOCKET        Sock;
    sockaddr_in   From;
    Thread        WorkThread;
    DateTimeStamp StartWork;
    bool          Authorized;
};
typedef vector< Session* > SessionVec;

static void AdminWork( void* );
static void AdminManager( void* );
static Thread AdminManagerThread;

static void InitAdminManager()
{
    ushort port = MainConfig->GetInt( "", "AdminPanelPort", 0 );
    if( port )
        AdminManagerThread.Start( AdminManager, "AdminPanelManager", new ushort( port ) );
}

static void AdminManager( void* port_ )
{
    ushort port = *(ushort*) port_;
    delete (ushort*) port_;

    // Listen socket
    #ifdef FO_WINDOWS
    WSADATA wsa;
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) )
    {
        WriteLog( "WSAStartup fail on creation listen socket for admin manager.\n" );
        return;
    }
    #endif
    SOCKET listen_sock = socket( AF_INET, SOCK_STREAM, 0 );
    if( listen_sock == INVALID_SOCKET )
    {
        WriteLog( "Can't create listen socket for admin manager.\n" );
        return;
    }
    const int   opt = 1;
    setsockopt( listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof( opt ) );
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons( port );
    sin.sin_addr.s_addr = INADDR_ANY;
    if( ::bind( listen_sock, (sockaddr*) &sin, sizeof( sin ) ) == SOCKET_ERROR )
    {
        WriteLog( "Can't bind listen socket for admin manager.\n" );
        closesocket( listen_sock );
        return;
    }
    if( listen( listen_sock, SOMAXCONN ) == SOCKET_ERROR )
    {
        WriteLog( "Can't listen listen socket for admin manager.\n" );
        closesocket( listen_sock );
        return;
    }

    // Accept clients
    SessionVec sessions;
    while( true )
    {
        // Wait connection
        timeval tv = { 1, 0 };
        fd_set  sock_set;
        FD_ZERO( &sock_set );
        FD_SET( listen_sock, &sock_set );
        if( select( (int) listen_sock + 1, &sock_set, nullptr, nullptr, &tv ) > 0 )
        {
            sockaddr_in from;
            #ifdef FO_WINDOWS
            int         len = sizeof( from );
            #else
            socklen_t   len = sizeof( from );
            #endif
            SOCKET      sock = accept( listen_sock, (sockaddr*) &from, &len );
            if( sock != INVALID_SOCKET )
            {
                // Found already connected from this IP
                bool refuse = false;
                for( auto it = sessions.begin(); it != sessions.end(); ++it )
                {
                    Session* s = *it;
                    if( s->From.sin_addr.s_addr == from.sin_addr.s_addr )
                    {
                        refuse = true;
                        break;
                    }
                }
                if( refuse || sessions.size() > MAX_SESSIONS )
                {
                    shutdown( sock, SD_BOTH );
                    closesocket( sock );
                }

                // Add new session
                if( !refuse )
                {
                    Session* s = new Session();
                    s->RefCount = 2;
                    s->Sock = sock;
                    s->From = from;
                    Timer::GetCurrentDateTime( s->StartWork );
                    s->Authorized = false;
                    s->WorkThread.Start( AdminWork, "AdminPanel", (void*) s );
                    sessions.push_back( s );
                }
            }
        }

        // Manage sessions
        if( !sessions.empty() )
        {
            DateTimeStamp cur_dt;
            Timer::GetCurrentDateTime( cur_dt );
            for( auto it = sessions.begin(); it != sessions.end();)
            {
                Session* s = *it;
                bool     erase = false;

                // Erase closed sessions
                if( s->Sock == INVALID_SOCKET )
                    erase = true;

                // Drop long not authorized connections
                if( !s->Authorized && Timer::GetTimeDifference( cur_dt, s->StartWork ) > 60 ) // 1 minute
                    erase = true;

                // Erase
                if( erase )
                {
                    if( s->Sock != INVALID_SOCKET )
                        shutdown( s->Sock, SD_BOTH );
                    if( --s->RefCount == 0 )
                        delete s;
                    it = sessions.erase( it );
                }
                else
                {
                    ++it;
                }
            }
        }

        // Sleep to prevent panel DDOS or keys brute force
        Thread_Sleep( 1000 );
    }
}

#define ADMIN_PREFIX    "Admin panel ({}): "
#define ADMIN_LOG( format, ... )                                     \
    do {                                                             \
        WriteLog( ADMIN_PREFIX format, admin_name, ## __VA_ARGS__ ); \
    } while( 0 )

/*
   string buf = _str(format, ## __VA_ARGS__);                       \
        uint   buf_len = (uint) buf.length() + 1;                                 \
        if( send( s->Sock, buf.c_str(), buf_len, 0 ) != (int) buf_len )           \
        {                                                                         \
            WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name ); \
            goto label_Finish;                                                    \
        }                                                                         \
 */

static void AdminWork( void* session_ )
{
    // Data
    Session* s = (Session*) session_;
    string   admin_name = "Not authorized";

    // Welcome string
    char welcome[] = { "Welcome to FOnline admin panel.\nEnter access key: " };
    uint welcome_len = Str::Length( welcome ) + 1;
    if( send( s->Sock, welcome, welcome_len, 0 ) != (int) welcome_len )
    {
        WriteLog( "Admin connection first send fail, disconnect.\n" );
        goto label_Finish;
    }

    // Commands loop
    while( true )
    {
        // Get command
        char cmd_raw[ MAX_FOTEXT ];
        memzero( cmd_raw, sizeof( cmd_raw ) );
        int  len = recv( s->Sock, cmd_raw, sizeof( cmd_raw ), 0 );
        if( len <= 0 || len == MAX_FOTEXT )
        {
            if( !len )
                WriteLog( ADMIN_PREFIX "Socket closed, disconnect.\n", admin_name );
            else
                WriteLog( ADMIN_PREFIX "Socket error, disconnect.\n", admin_name );
            goto label_Finish;
        }
        if( len > 200 )
            len = 200;
        cmd_raw[ len ] = 0;
        string cmd = _str( cmd_raw ).trim();

        // Authorization
        if( !s->Authorized )
        {
            StrVec client, tester, moder, admin, admin_names;
            FOServer::GetAccesses( client, tester, moder, admin, admin_names );
            int    pos = -1;
            for( size_t i = 0, j = admin.size(); i < j; i++ )
            {
                if( Str::Compare( admin[ i ].c_str(), cmd ) )
                {
                    pos = (int) i;
                    break;
                }
            }
            if( pos != -1 )
            {
                if( pos < (int) admin_names.size() )
                    admin_name = admin_names[ pos ];
                else
                    admin_name = _str( "{}", pos );

                s->Authorized = true;
                ADMIN_LOG( "Authorized for admin '{}', IP '{}'.\n", admin_name, inet_ntoa( s->From.sin_addr ) );
                continue;
            }
            else
            {
                WriteLog( "Wrong access key entered in admin panel from IP '{}', disconnect.\n", inet_ntoa( s->From.sin_addr ) );
                char failstr[] = { "Wrong access key!\n" };
                send( s->Sock, failstr, Str::Length( failstr ) + 1, 0 );
                goto label_Finish;
            }
        }

        // Process commands
        if( Str::CompareCase( cmd, "exit" ) )
        {
            ADMIN_LOG( "Disconnect from admin panel.\n" );
            goto label_Finish;
        }
        else if( Str::CompareCase( cmd, "kill" ) )
        {
            ADMIN_LOG( "Kill whole process.\n" );
            ExitProcess( 0 );
        }
        else if( Str::CompareCaseCount( cmd, "log ", 4 ) )
        {
            if( !Str::CompareCase( &cmd[ 4 ], "disable" ) )
            {
                LogToFile( &cmd[ 4 ] );
                ADMIN_LOG( "Logging to file '{}'.\n", &cmd[ 4 ] );
            }
            else
            {
                LogToFile( nullptr );
                ADMIN_LOG( "Logging disabled.\n" );
            }
        }
        else if( Str::CompareCase( cmd, "start" ) )
        {
            if( Server.Starting() )
                ADMIN_LOG( "Server already starting.\n" );
            else if( Server.Started() )
                ADMIN_LOG( "Server already started.\n" );
            else if( Server.Stopping() )
                ADMIN_LOG( "Server stopping, wait.\n" );
            else if( Server.Stopped() )
            {
                if( !Server.ActiveOnce )
                {
                    ADMIN_LOG( "Starting server.\n" );
                    #ifndef SERVER_DAEMON
                    if( GuiWindow )
                    {
                        GuiBtnStartStop->do_callback();
                    }
                    else
                    #endif
                    {
                        LoopThread.Start( GameLoopThread, "Main" );
                    }
                }
                else
                {
                    ADMIN_LOG( "Can't start server more than one time. Restart server process.\n" );
                    #pragma MESSAGE( "Allow multiple server starting in one process session." )
                }
            }
        }
        else if( Str::CompareCase( cmd, "stop" ) )
        {
            if( Server.Starting() )
                ADMIN_LOG( "Server starting, wait.\n" );
            else if( Server.Stopped() )
                ADMIN_LOG( "Server already stopped.\n" );
            else if( Server.Stopping() )
                ADMIN_LOG( "Server already stopping.\n" );
            else if( Server.Started() )
            {
                ADMIN_LOG( "Stopping server.\n" );
                #ifndef SERVER_DAEMON
                if( GuiWindow )
                {
                    GuiBtnStartStop->do_callback();
                }
                else
                #endif
                {
                    FOQuit = true;
                }
            }
        }
        else if( Str::CompareCase( cmd, "state" ) )
        {
            if( Server.Starting() )
                ADMIN_LOG( "Server starting.\n" );
            else if( Server.Started() )
                ADMIN_LOG( "Server started.\n" );
            else if( Server.Stopping() )
                ADMIN_LOG( "Server stopping.\n" );
            else if( Server.Stopped() )
                ADMIN_LOG( "Server stopped.\n" );
            else
                ADMIN_LOG( "Unknown state.\n" );
        }
        else if( !cmd.empty() && cmd[ 0 ] == '~' )
        {
            if( Server.Started() )
            {
                static THREAD string* admin_name_ptr;
                static THREAD SOCKET  sock;
                static THREAD bool    send_fail;
                struct LogCB
                {
                    static void Message( const char* str )
                    {
                        char buf[ MAX_FOTEXT ];
                        Str::Copy( buf, str );
                        uint buf_len = Str::Length( buf );
                        if( !buf_len || buf[ buf_len - 1 ] != '\n' )
                        {
                            buf[ buf_len ] = '\n';
                            buf[ buf_len + 1 ] = 0;
                            buf_len++;
                        }
                        buf_len++;

                        if( !send_fail && send( sock, buf, buf_len, 0 ) != (int) buf_len )
                        {
                            WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", *admin_name_ptr );
                            send_fail = true;
                        }
                    }
                };
                admin_name_ptr = &admin_name;
                sock = s->Sock;
                send_fail = false;

                BufferManager buf;
                PackCommand( &cmd[ 1 ], buf, LogCB::Message, nullptr );
                if( !buf.IsEmpty() )
                {
                    uint msg;
                    buf >> msg;
                    WriteLog( ADMIN_PREFIX "Execute command '{}'.\n", admin_name, cmd );
                    Server.Process_Command( buf, LogCB::Message, nullptr, admin_name.c_str() );
                }

                if( send_fail )
                    goto label_Finish;
            }
            else
            {
                ADMIN_LOG( "Can't run command for not started server.\n" );
            }
        }
        else if( cmd.length() > 0 )
        {
            ADMIN_LOG( "Unknown command '{}'.\n", cmd );
        }
    }

label_Finish:
    shutdown( s->Sock, SD_BOTH );
    closesocket( s->Sock );
    s->Sock = INVALID_SOCKET;
    if( --s->RefCount == 0 )
        delete s;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
