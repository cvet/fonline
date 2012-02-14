#include "StdAfx.h"
#include "Server.h"
#include "Exception.h"
#include "Version.h"
#include "Access.h"
#include "BufferManager.h"
#include <locale.h>
#ifdef FO_LINUX
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
#endif

void InitAdminManager( IniParser* cfg );

/************************************************************************/
/* GUI & Windows service version                                        */
/************************************************************************/

#ifndef SERVER_DAEMON
void GUIInit( IniParser& cfg );
void GUICallback( Fl_Widget* widget, void* data );
void UpdateInfo();
void UpdateLog();
void CheckTextBoxSize( bool force );
void GameLoopThread( void* );
void GUIUpdate( void* );
Rect       MainInitRect, LogInitRect, InfoInitRect;
int        SplitProcent = 90;
Thread     LoopThread;
MutexEvent GameInitEvent;
FOServer   Server;
string     UpdateLogName;
Thread     GUIUpdateThread;

// GUI
Fl_Window* GuiWindow;
Fl_Box*    GuiLabelGameTime, * GuiLabelClients, * GuiLabelIngame, * GuiLabelNPC, * GuiLabelLocCount,
* GuiLabelItemsCount, * GuiLabelVarsCount, * GuiLabelAnyDataCount, * GuiLabelTECount,
* GuiLabelFPS, * GuiLabelDelta, * GuiLabelUptime, * GuiLabelSend, * GuiLabelRecv, * GuiLabelCompress;
Fl_Button* GuiBtnRlClScript, * GuiBtnSaveWorld, * GuiBtnSaveLog, * GuiBtnSaveInfo,
* GuiBtnCreateDump, * GuiBtnMemory, * GuiBtnPlayers, * GuiBtnLocsMaps, * GuiBtnTimeEvents,
* GuiBtnAnyData, * GuiBtnItemsCount, * GuiBtnProfiler, * GuiBtnStartStop, * GuiBtnSplitUp, * GuiBtnSplitDown;
Fl_Check_Button* GuiCBtnScriptDebug, * GuiCBtnLogging, * GuiCBtnLoggingTime,
* GuiCBtnLoggingThread, * GuiCBtnAutoUpdate;
Fl_Text_Display* GuiLog, * GuiInfo;
int              GUISizeMod = 0;

# define GUI_SIZE1( x )                 ( (int) ( x ) * 175 * ( 100 + GUISizeMod ) / 100 / 100 )
# define GUI_SIZE2( x1, x2 )            GUI_SIZE1( x1 ), GUI_SIZE1( x2 )
# define GUI_SIZE4( x1, x2, x3, x4 )    GUI_SIZE1( x1 ), GUI_SIZE1( x2 ), GUI_SIZE1( x3 ), GUI_SIZE1( x4 )
# define GUI_LABEL_BUF_SIZE    ( 128 )

// Windows service
# ifdef FO_WINDOWS
void ServiceMain( bool as_service );
# endif

// Main
int main( int argc, char** argv )
{
    setlocale( LC_ALL, "Russian" );
    RestoreMainDirectory();

    // Threading
    # ifdef FO_WINDOWS
    pthread_win32_process_attach_np();
    # endif
    Thread::SetCurrentName( "GUI" );

    // Disable SIGPIPE signal
    # ifdef FO_LINUX
    signal( SIGPIPE, SIG_IGN );
    # endif

    // Exceptions catcher
    CatchExceptions( "FOnlineServer", SERVER_VERSION );

    // Timer
    Timer::Init();

    // Config
    IniParser cfg;
    cfg.LoadFile( GetConfigFileName(), PT_SERVER_ROOT );

    // Memory debugging
    MemoryDebugLevel = cfg.GetInt( "MemoryDebugLevel", 0 );
    if( MemoryDebugLevel >= 3 )
        Debugger::StartTraceMemory();

    // Make command line
    SetCommandLine( argc, argv );

    // Logging
    LogWithTime( cfg.GetInt( "LoggingTime", 1 ) == 0 ? false : true );
    LogWithThread( cfg.GetInt( "LoggingThread", 1 ) == 0 ? false : true );
    if( strstr( CommandLine, "-logdebugoutput" ) || cfg.GetInt( "LoggingDebugOutput", 0 ) != 0 )
        LogToDebugOutput();

    // Init event
    GameInitEvent.Disallow();

    // Service
    if( strstr( CommandLine, "-service" ) )
    {
        # ifdef FO_WINDOWS
        ServiceMain( strstr( CommandLine, "--service" ) != NULL );
        # endif
        return 0;
    }

    // Check single player parameters
    if( strstr( CommandLine, "-singleplayer " ) )
    {
        # ifdef FO_WINDOWS
        Singleplayer = true;
        Timer::SetGamePause( true );

        // Logging
        char log_path[ MAX_FOPATH ] = { 0 };
        if( !strstr( CommandLine, "-nologpath" ) && strstr( CommandLine, "-logpath " ) )
        {
            const char* ptr = strstr( CommandLine, "-logpath " ) + Str::Length( "-logpath " );
            Str::Copy( log_path, ptr );
        }
        Str::EraseFrontBackSpecificChars( log_path );
        Str::Append( log_path, "FOnlineServer.log" );
        LogToFile( log_path );

        WriteLog( "Singleplayer mode.\n" );

        // Shared data
        const char* ptr = strstr( CommandLine, "-singleplayer " ) + Str::Length( "-singleplayer " );
        HANDLE      map_file = NULL;
        if( sscanf( ptr, "%p%p", &map_file, &SingleplayerClientProcess ) != 2 || !SingleplayerData.Attach( map_file ) )
        {
            WriteLog( "Can't attach to mapped file<%p>.\n", map_file );
            return 0;
        }
        # else
        return 0;
        # endif
    }

    // GUI
    if( !Singleplayer || strstr( CommandLine, "-showgui" ) )
    {
        Fl::lock();         // Begin GUI multi threading
        GUIInit( cfg );
        LogFinish( LOG_FILE );
        LogToBuffer();
    }

    WriteLog( "FOnline server, version %04X-%02X.\n", SERVER_VERSION, FO_PROTOCOL_VERSION & 0xFF );

    FOQuit = true;
    Script::SetLogDebugInfo( true );

    if( GuiWindow )
    {
        GuiCBtnAutoUpdate->value( 0 );
        GuiCBtnLogging->value( cfg.GetInt( "Logging", 1 ) != 0 ? 1 : 0 );
        GuiCBtnLoggingTime->value( cfg.GetInt( "LoggingTime", 1 ) != 0 ? 1 : 0 );
        GuiCBtnLoggingThread->value( cfg.GetInt( "LoggingThread", 1 ) != 0 ? 1 : 0 );
        GuiCBtnScriptDebug->value( 1 );
    }

    // Command line
    if( CommandLineArgCount > 1 )
        WriteLog( "Command line<%s>.\n", CommandLine );

    // Autostart
    if( strstr( CommandLine, "-start" ) || Singleplayer )
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
    InitAdminManager( &cfg );

    // Loop
    SyncManager::GetForCurThread()->UnlockAll();
    if( GuiWindow )
    {
        GUIUpdateThread.Start( GUIUpdate, "GUIUpdate" );
        while( Fl::wait() )
        {
            void* pmsg = Fl::thread_message();
            if( pmsg )
            {
                UpdateLog();
                UpdateInfo();
                CheckTextBoxSize( false );
            }
        }
        Fl::unlock();
        GUIUpdateThread.Finish();
    }
    else
    {
        while( !FOQuit )
            Sleep( 100 );
    }

    return 0;
}

void GUIInit( IniParser& cfg )
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

    GUISizeMod = cfg.GetInt( "GUISize", 0 );
    GUISetup.FontType = FL_COURIER;
    GUISetup.FontSize = 11;

    // Main window
    int wx = cfg.GetInt( "PositionX", 0 );
    int wy = cfg.GetInt( "PositionY", 0 );
    if( !wx && !wy )
        wx = ( Fl::w() - GUI_SIZE1( 496 ) ) / 2, wy = ( Fl::h() - GUI_SIZE1( 412 ) ) / 2;
    GuiWindow = new Fl_Window( wx, wy, GUI_SIZE2( 496, 412 ), "FOnline Server" );
    GuiWindow->labelfont( GUISetup.FontType );
    GuiWindow->labelsize( GUISetup.FontSize );
    GuiWindow->callback( GUICallback );
    GuiWindow->size_range( GUI_SIZE2( 129, 129 ) );

    // Name
    GuiWindow->label( GetWindowName() );

    // Icon
    # ifdef FO_WINDOWS
    GuiWindow->icon( (char*) LoadIcon( fl_display, MAKEINTRESOURCE( 101 ) ) );
    # else // FO_LINUX
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
    GUISetup.Setup( GuiLabelVarsCount   = new Fl_Box( GUI_SIZE4( 5, 54, 124, 8 ), "Vars:" ) );
    GUISetup.Setup( GuiLabelAnyDataCount = new Fl_Box( GUI_SIZE4( 5, 62, 124, 8 ), "Any data:" ) );
    GUISetup.Setup( GuiLabelTECount     = new Fl_Box( GUI_SIZE4( 5, 70, 124, 8 ), "Time events:" ) );
    GUISetup.Setup( GuiLabelFPS         = new Fl_Box( GUI_SIZE4( 5, 78, 124, 8 ), "Cycles per second:" ) );
    GUISetup.Setup( GuiLabelDelta       = new Fl_Box( GUI_SIZE4( 5, 86, 124, 8 ), "Cycle time:" ) );
    GUISetup.Setup( GuiLabelUptime      = new Fl_Box( GUI_SIZE4( 5, 94, 124, 8 ), "Uptime:" ) );
    GUISetup.Setup( GuiLabelSend        = new Fl_Box( GUI_SIZE4( 5, 102, 124, 8 ), "KBytes send:" ) );
    GUISetup.Setup( GuiLabelRecv        = new Fl_Box( GUI_SIZE4( 5, 110, 124, 8 ), "KBytes recv:" ) );
    GUISetup.Setup( GuiLabelCompress    = new Fl_Box( GUI_SIZE4( 5, 118, 124, 8 ), "Compress ratio:" ) );

    // Buttons
    GUISetup.Setup( GuiBtnRlClScript = new Fl_Button( GUI_SIZE4( 5, 128, 124, 14 ), "Reload client scripts" ) );
    GUISetup.Setup( GuiBtnSaveWorld = new Fl_Button( GUI_SIZE4( 5, 144, 124, 14 ), "Save world" ) );
    GUISetup.Setup( GuiBtnSaveLog   = new Fl_Button( GUI_SIZE4( 5, 160, 124, 14 ), "Save log" ) );
    GUISetup.Setup( GuiBtnSaveInfo  = new Fl_Button( GUI_SIZE4( 5, 176, 124, 14 ), "Save info" ) );
    GUISetup.Setup( GuiBtnCreateDump = new Fl_Button( GUI_SIZE4( 5, 192, 124, 14 ), "Create dump" ) );
    GUISetup.Setup( GuiBtnMemory    = new Fl_Button( GUI_SIZE4( 5, 219, 124, 14 ), "Memory usage" ) );
    GUISetup.Setup( GuiBtnPlayers   = new Fl_Button( GUI_SIZE4( 5, 235, 124, 14 ), "Players" ) );
    GUISetup.Setup( GuiBtnLocsMaps  = new Fl_Button( GUI_SIZE4( 5, 251, 124, 14 ), "Locations and maps" ) );
    GUISetup.Setup( GuiBtnTimeEvents = new Fl_Button( GUI_SIZE4( 5, 267, 124, 14 ), "Time events" ) );
    GUISetup.Setup( GuiBtnAnyData   = new Fl_Button( GUI_SIZE4( 5, 283, 124, 14 ), "Any data" ) );
    GUISetup.Setup( GuiBtnItemsCount = new Fl_Button( GUI_SIZE4( 5, 299, 124, 14 ), "Items count" ) );
    GUISetup.Setup( GuiBtnProfiler = new Fl_Button( GUI_SIZE4( 5, 315, 124, 14 ), "Profiler" ) );
    GUISetup.Setup( GuiBtnStartStop = new Fl_Button( GUI_SIZE4( 5, 393, 124, 14 ), "Start server" ) );
    GUISetup.Setup( GuiBtnSplitUp   = new Fl_Button( GUI_SIZE4( 117, 357, 12, 9 ), "" ) );
    GUISetup.Setup( GuiBtnSplitDown = new Fl_Button( GUI_SIZE4( 117, 368, 12, 9 ), "" ) );

    // Check buttons
    GUISetup.Setup( GuiCBtnAutoUpdate   = new Fl_Check_Button( GUI_SIZE4( 5, 339, 110, 10 ), "Update info every second" ) );
    GUISetup.Setup( GuiCBtnLogging      = new Fl_Check_Button( GUI_SIZE4( 5, 349, 110, 10 ), "Logging" ) );
    GUISetup.Setup( GuiCBtnLoggingTime  = new Fl_Check_Button( GUI_SIZE4( 5, 359, 110, 10 ), "Logging with time" ) );
    GUISetup.Setup( GuiCBtnLoggingThread = new Fl_Check_Button( GUI_SIZE4( 5, 369, 110, 10 ), "Logging with thread" ) );
    GUISetup.Setup( GuiCBtnScriptDebug  = new Fl_Check_Button( GUI_SIZE4( 5, 379, 110, 10 ), "Script debug info" ) );

    // Text boxes
    GUISetup.Setup( GuiLog = new Fl_Text_Display( GUI_SIZE4( 133, 7, 358, 195 ) ) );
    GUISetup.Setup( GuiInfo = new Fl_Text_Display( GUI_SIZE4( 133, 204, 358, 203 ) ) );

    // Disable buttons
    GuiBtnRlClScript->deactivate();
    GuiBtnSaveWorld->deactivate();
    GuiBtnPlayers->deactivate();
    GuiBtnLocsMaps->deactivate();
    GuiBtnTimeEvents->deactivate();
    GuiBtnAnyData->deactivate();
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

void GUICallback( Fl_Widget* widget, void* data )
{
    if( widget == GuiWindow )
    {
        ExitProcess( 0 );
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
        DateTime         dt;
        Timer::GetCurrentDateTime( dt );
        char             log_name[ MAX_FOTEXT ];
        Fl_Text_Display* log = ( widget == GuiBtnSaveLog ? GuiLog : GuiInfo );
        log->buffer()->savefile( Str::Format( log_name, "FOnlineServer_%s_%02u.%02u.%04u_%02u-%02u-%02u.log",
                                              log == GuiInfo ? UpdateLogName.c_str() : "Log", dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second ) );
    }
    else if( widget == GuiBtnCreateDump )
    {
        CreateDump( "ManualDump" );
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
    else if( widget == GuiBtnTimeEvents )
    {
        FOServer::UpdateIndex = 3;
        FOServer::UpdateLastIndex = 3;
    }
    else if( widget == GuiBtnAnyData )
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
            GuiBtnTimeEvents->deactivate();
            GuiBtnAnyData->deactivate();
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
    else if( widget == GuiCBtnScriptDebug )
    {
        Script::SetLogDebugInfo( GuiCBtnScriptDebug->value() ? true : false );
    }
    else if( widget == GuiCBtnLogging )
    {
        if( GuiCBtnLogging->value() )
            LogToBuffer();
        else
            LogFinish( LOG_BUFFER );
    }
    else if( widget == GuiCBtnLoggingTime )
    {
        LogWithTime( GuiCBtnLogging->value() ? true : false );
    }
    else if( widget == GuiCBtnLoggingThread )
    {
        LogWithThread( GuiCBtnLoggingThread->value() ? true : false );
    }
    else if( widget == GuiCBtnAutoUpdate )
    {
        if( GuiCBtnAutoUpdate->value() )
            FOServer::UpdateLastTick = Timer::FastTick();
        else
            FOServer::UpdateLastTick = 0;
    }
}

void GUIUpdate( void* )
{
    while( true )
    {
        static int dummy = 0;
        Fl::awake( &dummy );
        Sleep( 50 );
    }
}

void UpdateInfo()
{
    static char   str[ MAX_FOTEXT ];
    static string std_str;

    struct Label
    {
        static void Update( Fl_Box* label, char* text )
        {
            if( !Str::Compare( text, (char*) label->label() ) )
            {
                Str::Copy( (char*) label->label(), GUI_LABEL_BUF_SIZE, text );
                label->redraw_label();
            }
        }
    };

    if( Server.Started() )
    {
        DateTime st = Timer::GetGameTime( GameOpt.FullSecond );
        Label::Update( GuiLabelGameTime, Str::Format( str, "Time: %02u.%02u.%04u %02u:%02u:%02u x%u", st.Day, st.Month, st.Year, st.Hour, st.Minute, st.Second, GameOpt.TimeMultiplier ) );
        Label::Update( GuiLabelClients, Str::Format( str, "Connections: %u", Server.Statistics.CurOnline ) );
        Label::Update( GuiLabelIngame, Str::Format( str, "Players in game: %u", Server.PlayersInGame() ) );
        Label::Update( GuiLabelNPC, Str::Format( str, "NPC in game: %u", Server.NpcInGame() ) );
        Label::Update( GuiLabelLocCount, Str::Format( str, "Locations: %u (%u)", MapMngr.GetLocationsCount(), MapMngr.GetMapsCount() ) );
        Label::Update( GuiLabelItemsCount, Str::Format( str, "Items: %u", ItemMngr.GetItemsCount() ) );
        Label::Update( GuiLabelVarsCount, Str::Format( str, "Vars: %u", VarMngr.GetVarsCount() ) );
        Label::Update( GuiLabelAnyDataCount, Str::Format( str, "Any data: %u", Server.AnyData.size() ) );
        Label::Update( GuiLabelTECount, Str::Format( str, "Time events: %u", Server.GetTimeEventsCount() ) );
        Label::Update( GuiLabelFPS, Str::Format( str, "Cycles per second: %u", Server.Statistics.FPS ) );
        Label::Update( GuiLabelDelta, Str::Format( str, "Cycle time: %d", Server.Statistics.CycleTime ) );
    }
    else
    {
        Label::Update( GuiLabelGameTime, Str::Format( str, "Time: n/a" ) );
        Label::Update( GuiLabelClients, Str::Format( str, "Connections: n/a" ) );
        Label::Update( GuiLabelIngame, Str::Format( str, "Players in game: n/a" ) );
        Label::Update( GuiLabelNPC, Str::Format( str, "NPC in game: n/a" ) );
        Label::Update( GuiLabelLocCount, Str::Format( str, "Locations: n/a" ) );
        Label::Update( GuiLabelItemsCount, Str::Format( str, "Items: n/a" ) );
        Label::Update( GuiLabelVarsCount, Str::Format( str, "Vars: n/a" ) );
        Label::Update( GuiLabelAnyDataCount, Str::Format( str, "Any data: n/a" ) );
        Label::Update( GuiLabelTECount, Str::Format( str, "Time events: n/a" ) );
        Label::Update( GuiLabelFPS, Str::Format( str, "Cycles per second: n/a" ) );
        Label::Update( GuiLabelDelta, Str::Format( str, "Cycle time: n/a" ) );
    }

    uint seconds = Server.Statistics.Uptime;
    Label::Update( GuiLabelUptime, Str::Format( str, "Uptime: %2u:%2u:%2u", seconds / 60 / 60, seconds / 60 % 60, seconds % 60 ) );
    Label::Update( GuiLabelSend, Str::Format( str, "KBytes Send: %u", Server.Statistics.BytesSend / 1024 ) );
    Label::Update( GuiLabelRecv, Str::Format( str, "KBytes Recv: %u", Server.Statistics.BytesRecv / 1024 ) );
    Label::Update( GuiLabelCompress, Str::Format( str, "Compress ratio: %g", (double) Server.Statistics.DataReal / ( Server.Statistics.DataCompressed ? Server.Statistics.DataCompressed : 1 ) ) );

    if( FOServer::UpdateIndex == -1 && FOServer::UpdateLastTick && FOServer::UpdateLastTick + 1000 < Timer::FastTick() )
    {
        FOServer::UpdateIndex = FOServer::UpdateLastIndex;
        FOServer::UpdateLastTick = Timer::FastTick();
    }

    if( FOServer::UpdateIndex != -1 )
    {
        switch( FOServer::UpdateIndex )
        {
        case 0:         // Memory
            std_str = Debugger::GetMemoryStatistics();
            UpdateLogName = "Memory";
            break;
        case 1:         // Players
            if( !Server.Started() )
                break;
            std_str = Server.GetIngamePlayersStatistics();
            UpdateLogName = "Players";
            break;
        case 2:         // Locations and maps
            if( !Server.Started() )
                break;
            std_str = MapMngr.GetLocationsMapsStatistics();
            UpdateLogName = "LocationsAndMaps";
            break;
        case 3:         // Time events
            if( !Server.Started() )
                break;
            std_str = Server.GetTimeEventsStatistics();
            UpdateLogName = "TimeEvents";
            break;
        case 4:         // Any data
            if( !Server.Started() )
                break;
            std_str = Server.GetAnyDataStatistics();
            UpdateLogName = "AnyData";
            break;
        case 5:         // Items count
            if( !Server.Started() )
                break;
            std_str = ItemMngr.GetItemsStatistics();
            UpdateLogName = "ItemsCount";
            break;
        case 6:         // Profiler
            std_str = Script::Profiler::GetStatistics();
            UpdateLogName = "Profiler";
            break;
        default:
            UpdateLogName = "";
            break;
        }
        # ifdef FO_WINDOWS
        GuiInfo->buffer()->text( fl_locale_to_utf8( std_str.c_str(), (int) std_str.length(), GetACP() ) );
        # else
        GuiInfo->buffer()->text( std_str.c_str() );
        # endif
        if( !GuiBtnSaveInfo->active() )
            GuiBtnSaveInfo->activate();
        FOServer::UpdateIndex = -1;
    }
}

void UpdateLog()
{
    string str;
    LogGetBuffer( str );
    if( str.length() )
    {
        # ifdef FO_WINDOWS
        GuiLog->buffer()->append( fl_locale_to_utf8( str.c_str(), (int) str.length(), GetACP() ) );
        # else // FO_LINUX
        GuiLog->buffer()->append( str.c_str() );
        # endif
        if( Fl::focus() != GuiLog )
            GuiLog->scroll( MAX_INT, 0 );
    }
}

void CheckTextBoxSize( bool force )
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

void GameLoopThread( void* )
{
    GetServerOptions();

    if( Server.Init() )
    {
        if( GuiWindow )
        {
            if( GuiCBtnLogging->value() == 0 )
                LogFinish( LOG_TEXT_BOX );

            // Enable buttons
            GuiBtnRlClScript->activate();
            GuiBtnSaveWorld->activate();
            GuiBtnPlayers->activate();
            GuiBtnLocsMaps->activate();
            GuiBtnTimeEvents->activate();
            GuiBtnAnyData->activate();
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
    LogFinish( -1 );
    if( Singleplayer )
        ExitProcess( 0 );
}

#endif // !SERVER_DAEMON

/************************************************************************/
/* Windows service                                                      */
/************************************************************************/
#ifdef FO_WINDOWS

SERVICE_STATUS_HANDLE FOServiceStatusHandle;
VOID WINAPI FOServiceStart( DWORD argc, LPTSTR* argv );
VOID WINAPI FOServiceCtrlHandler( DWORD opcode );
void        SetFOServiceStatus( uint state );

void ServiceMain( bool as_service )
{
    // Binary started as service
    if( as_service )
    {
        // Start
        SERVICE_TABLE_ENTRY dispatch_table[] = { { Str::Duplicate( "FOnlineServer" ), FOServiceStart }, { NULL, NULL } };
        StartServiceCtrlDispatcher( dispatch_table );
        return;
    }

    // Open service manager
    SC_HANDLE manager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if( !manager )
    {
        MessageBox( NULL, "Can't open service manager.", "FOnlineServer", MB_OK | MB_ICONHAND );
        return;
    }

    // Delete service
    if( strstr( CommandLine, "-delete" ) )
    {
        SC_HANDLE service = OpenService( manager, "FOnlineServer", DELETE );

        if( service && DeleteService( service ) )
            MessageBox( NULL, "Service deleted.", "FOnlineServer", MB_OK | MB_ICONASTERISK );
        else
            MessageBox( NULL, "Can't delete service.", "FOnlineServer", MB_OK | MB_ICONHAND );

        CloseServiceHandle( service );
        CloseServiceHandle( manager );
        return;
    }

    // Manage service
    SC_HANDLE service = OpenService( manager, "FOnlineServer", SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START );

    // Compile service path
    char path1[ MAX_FOPATH ];
    GetModuleFileName( GetModuleHandle( NULL ), path1, MAX_FOPATH );
    char path2[ MAX_FOPATH ];
    Str::Format( path2, "\"%s\" --service", path1 );

    // Change executable path, if changed
    if( service )
    {
        LPQUERY_SERVICE_CONFIG service_cfg = (LPQUERY_SERVICE_CONFIG) calloc( 8192, 1 );
        DWORD                  dw;
        if( QueryServiceConfig( service, service_cfg, 8192, &dw ) && !Str::CompareCase( service_cfg->lpBinaryPathName, path2 ) )
            ChangeServiceConfig( service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, path2, NULL, NULL, NULL, NULL, NULL, NULL );
        free( service_cfg );
    }

    // Register service
    if( !service )
    {
        service = CreateService( manager, "FOnlineServer", "FOnlineServer", SERVICE_ALL_ACCESS,
                                 SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path2, NULL, NULL, NULL, NULL, NULL );

        if( service )
            MessageBox( NULL, "\'FOnlineServer\' service registered.", "FOnlineServer", MB_OK | MB_ICONASTERISK );
        else
            MessageBox( NULL, "Can't register \'FOnlineServer\' service.", "FOnlineServer", MB_OK | MB_ICONHAND );
    }
    // Start service
    else
    {
        SERVICE_STATUS status;
        if( service && QueryServiceStatus( service, &status ) && status.dwCurrentState != SERVICE_STOPPED )
            MessageBox( NULL, "Service already running.", "FOnlineServer", MB_OK | MB_ICONASTERISK );
        else if( service && !StartService( service, 0, NULL ) )
            MessageBox( NULL, "Can't start service.", "FOnlineServer", MB_OK | MB_ICONHAND );
    }

    // Close handles
    if( service )
        CloseServiceHandle( service );
    if( manager )
        CloseServiceHandle( manager );
}

VOID WINAPI FOServiceStart( DWORD argc, LPTSTR* argv )
{
    Thread::SetCurrentName( "Service" );
    LogToFile( "FOnlineServer.log" );
    WriteLog( "FOnline server service, version %04X-%02X.\n", SERVER_VERSION, FO_PROTOCOL_VERSION & 0xFF );

    FOServiceStatusHandle = RegisterServiceCtrlHandler( "FOnlineServer", FOServiceCtrlHandler );
    if( !FOServiceStatusHandle )
        return;

    // Start admin manager
    InitAdminManager( NULL );

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

VOID WINAPI FOServiceCtrlHandler( DWORD opcode )
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

void SetFOServiceStatus( uint state )
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

void DaemonLoop();
void GameLoopThread( void* );
FOServer Server;
Thread   LoopThread;

int main( int argc, char** argv )
{
    // Stuff
    setlocale( LC_ALL, "Russian" );
    SetCommandLine( argc, argv );
    RestoreMainDirectory();
    CatchExceptions( "FOnlineServer", SERVER_VERSION );
    Timer::Init();
    Thread::SetCurrentName( "Daemon" );
    LogToFile( "./FOnlineServerDaemon.log" );

    // Config
    IniParser cfg;
    cfg.LoadFile( GetConfigFileName(), PT_SERVER_ROOT );

    // Logging
    LogWithTime( cfg.GetInt( "LoggingTime", 1 ) == 0 ? false : true );
    LogWithThread( cfg.GetInt( "LoggingThread", 1 ) == 0 ? false : true );
    if( strstr( CommandLine, "-logdebugoutput" ) || cfg.GetInt( "LoggingDebugOutput", 0 ) != 0 )
        LogToDebugOutput();

    // Log version
    WriteLog( "FOnline server daemon, version %04X-%02X.\n", SERVER_VERSION, FO_PROTOCOL_VERSION & 0xFF );
    if( CommandLineArgCount > 1 )
        WriteLog( "Command line<%s>.\n", CommandLine );

    // Start daemon
    pid_t parpid = fork();
    if( parpid < 0 )
    {
        WriteLog( "Create child process (fork) fail, error<%s>.", ERRORSTR );
        return 1;
    }
    else if( parpid != 0 )
    {
        // Close parent process
        return 0;
    }

    umask( 0 );

    if( setsid() < 0 )
    {
        WriteLog( "Generate process index (setsid) fail, error<%s>.\n", ERRORSTR );
        return 1;
    }

    close( STDIN_FILENO );
    close( STDOUT_FILENO );
    close( STDERR_FILENO );

    DaemonLoop();
    return 0;
}

void DaemonLoop()
{
    // Autostart server
    LoopThread.Start( GameLoopThread, "Main" );

    // Start admin manager
    InitAdminManager( NULL );

    // Daemon loop
    while( true )
        Sleep( 1000 );
}

void GameLoopThread( void* )
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
    int         RefCount;
    SOCKET      Sock;
    sockaddr_in From;
    Thread      WorkThread;
    DateTime    StartWork;
    bool        Authorized;
};
typedef vector< Session* > SessionVec;

void AdminWork( void* );
void AdminManager( void* );
Thread AdminManagerThread;

void InitAdminManager( IniParser* cfg )
{
    uint port = 0;
    if( !cfg )
    {
        IniParser cfg_;
        if( !cfg_.LoadFile( GetConfigFileName(), PT_SERVER_ROOT ) )
        {
            WriteLogF( _FUNC_, "Can't access to config file.\n" );
            return;
        }
        port = cfg_.GetInt( "AdminPanelPort", 0 );
    }
    else
    {
        port = cfg->GetInt( "AdminPanelPort", 0 );
    }

    if( port )
    {
        AdminManagerThread.Finish();
        AdminManagerThread.Start( AdminManager, "AdminPanelManager", (void*) port );
    }
}

void AdminManager( void* port_ )
{
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
    sin.sin_port = htons( (ushort) (size_t) port_ );
    sin.sin_addr.s_addr = INADDR_ANY;
    if( bind( listen_sock, (sockaddr*) &sin, sizeof( sin ) ) == SOCKET_ERROR )
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
        if( select( listen_sock + 1, &sock_set, NULL, NULL, &tv ) > 0 )
        {
            sockaddr_in from;
            socklen_t   len = sizeof( from );
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
            DateTime cur_dt;
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
        Sleep( 1000 );
    }
}

#define ADMIN_PREFIX    "Admin panel (%s): "
#define ADMIN_LOG( format, ... )                                                  \
    do {                                                                          \
        WriteLog( ADMIN_PREFIX format, admin_name, ## __VA_ARGS__ );              \
        char buf[ MAX_FOTEXT ];                                                   \
        Str::Format( buf, format, ## __VA_ARGS__ );                               \
        uint buf_len = Str::Length( buf ) + 1;                                    \
        if( send( s->Sock, buf, buf_len, 0 ) != (int) buf_len )                   \
        {                                                                         \
            WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name ); \
            goto label_Finish;                                                    \
        }                                                                         \
    } while( 0 )

void AdminWork( void* session_ )
{
    // Data
    Session* s = (Session*) session_;
    char     admin_name[ MAX_FOTEXT ] = { "Not authorized" };

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
        char cmd[ MAX_FOTEXT ];
        memzero( cmd, sizeof( cmd ) );
        int  len = recv( s->Sock, cmd, sizeof( cmd ), 0 );
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
        cmd[ len ] = 0;
        Str::EraseFrontBackSpecificChars( cmd );

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
                    Str::Copy( admin_name, admin_names[ pos ].c_str() );
                else
                    Str::Format( admin_name, "%d", pos );

                s->Authorized = true;
                ADMIN_LOG( "Authorized for admin '%s', IP '%s'.\n", admin_name, inet_ntoa( s->From.sin_addr ) );
                continue;
            }
            else
            {
                WriteLog( "Wrong access key entered in admin panel from IP '%s', disconnect.\n", inet_ntoa( s->From.sin_addr ) );
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
                ADMIN_LOG( "Logging to file '%s'.\n", &cmd[ 4 ] );
            }
            else
            {
                LogFinish( LOG_FILE );
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
        else if( cmd[ 0 ] == '~' )
        {
            if( Server.Started() )
            {
                static THREAD char*  admin_name_ptr;
                static THREAD SOCKET sock;
                static THREAD bool   send_fail;
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
                            WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name_ptr );
                            send_fail = true;
                        }
                    }
                };
                admin_name_ptr = admin_name;
                sock = s->Sock;
                send_fail = false;

                BufferManager buf;
                PackCommand( &cmd[ 1 ], buf, LogCB::Message, NULL );
                if( !buf.IsEmpty() )
                {
                    if( Script::InitThread() )
                    {
                        uint msg;
                        buf >> msg;
                        WriteLog( ADMIN_PREFIX "Execute command '%s'.\n", admin_name, cmd );
                        Server.Process_Command( buf, LogCB::Message, NULL, admin_name );
                        Script::FinishThread();
                    }
                    else
                    {
                        ADMIN_LOG( "Can't initialize script stuff for thread." );
                    }
                }

                if( send_fail )
                    goto label_Finish;
            }
            else
            {
                ADMIN_LOG( "Can't run command for not started server.\n" );
            }
        }
        else if( Str::Length( cmd ) > 0 )
        {
            ADMIN_LOG( "Unknown command '%s'.\n", cmd );
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
