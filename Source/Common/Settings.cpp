#include "Settings.h"
#include "StringUtils.h"
#include "IniFile.h"

Settings GameOpt;
IniFile* MainConfig;
StrVec   ProjectFiles;

Settings::Settings()
{
    WorkDir = "";
    CommandLine = "";

    FullSecondStart = 0;
    FullSecond = 0;
    GameTimeTick = 0;
    YearStartFTHi = 0;
    YearStartFTLo = 0;

    DisableTcpNagle = false;
    DisableZlibCompression = false;
    FloodSize = 2048;
    NoAnswerShuffle = false;
    DialogDemandRecheck = false;
    SneakDivider = 6;
    LookMinimum = 6;
    DeadHitPoints = -6;

    Breaktime = 1200;
    TimeoutTransfer = 3;
    TimeoutBattle = 10;
    RunOnCombat = false;
    RunOnTransfer = false;
    GlobalMapWidth = 28;
    GlobalMapHeight = 30;
    GlobalMapZoneLength = 50;
    BagRefreshTime = 60;
    WhisperDist = 2;
    ShoutDist = 200;
    LookChecks = 0;
    LookDir[ 0 ] = 0;
    LookDir[ 1 ] = 20;
    LookDir[ 2 ] = 40;
    LookDir[ 3 ] = 60;
    LookDir[ 4 ] = 60;
    LookSneakDir[ 0 ] = 90;
    LookSneakDir[ 1 ] = 60;
    LookSneakDir[ 2 ] = 30;
    LookSneakDir[ 3 ] = 0;
    LookSneakDir[ 4 ] = 0;
    RegistrationTimeout = 5;
    AccountPlayTime = 0;
    ScriptRunSuspendTimeout = 30000;
    ScriptRunMessageTimeout = 10000;
    TalkDistance = 3;
    NpcMaxTalkers = 1;
    MinNameLength = 4;
    MaxNameLength = 12;
    DlgTalkMinTime = 0;
    DlgBarterMinTime = 0;
    MinimumOfflineTime = 180000;
    ForceRebuildResources = false;

    MapHexagonal = true;
    MapHexWidth = 32;
    MapHexHeight = 16;
    MapHexLineHeight = 12;
    MapTileOffsX = -8;
    MapTileOffsY = 32;
    MapTileStep = 2;
    MapRoofOffsX = -8;
    MapRoofOffsY = -66;
    MapRoofSkipSize = 2;
    MapCameraAngle = 25.7f;
    MapSmoothPath = true;
    MapDataPrefix = "art/geometry/fallout_";

    #ifdef FO_WEB
    WebBuild = true;
    #else
    WebBuild = false;
    #endif
    #ifdef FO_WINDOWS
    WindowsBuild = true;
    #else
    WindowsBuild = false;
    #endif
    #ifdef FO_LINUX
    LinuxBuild = true;
    #else
    LinuxBuild = false;
    #endif
    #ifdef FO_MAC
    MacOsBuild = true;
    #else
    MacOsBuild = false;
    #endif
    #ifdef FO_ANDROID
    AndroidBuild = true;
    #else
    AndroidBuild = false;
    #endif
    #ifdef FO_IOS
    IOsBuild = true;
    #else
    IOsBuild = false;
    #endif

    DesktopBuild = WindowsBuild || LinuxBuild || MacOsBuild;
    TabletBuild = AndroidBuild || IOsBuild;

    #ifdef FO_WINDOWS
    if( GetSystemMetrics( SM_TABLETPC ) != 0 )
    {
        DesktopBuild = false;
        TabletBuild = true;
    }
    #endif

    // Client and Mapper
    Quit = false;
    WaitPing = false;
    OpenGLRendering = true;
    OpenGLDebug = false;
    AssimpLogging = false;
    MouseX = 0;
    MouseY = 0;
    LastMouseX = 0;
    LastMouseY = 0;
    ScrOx = 0;
    ScrOy = 0;
    ShowTile = true;
    ShowRoof = true;
    ShowItem = true;
    ShowScen = true;
    ShowWall = true;
    ShowCrit = true;
    ShowFast = true;
    ShowPlayerNames = false;
    ShowNpcNames = false;
    ShowCritId = false;
    ScrollKeybLeft = false;
    ScrollKeybRight = false;
    ScrollKeybUp = false;
    ScrollKeybDown = false;
    ScrollMouseLeft = false;
    ScrollMouseRight = false;
    ScrollMouseUp = false;
    ScrollMouseDown = false;
    ShowGroups = true;
    HelpInfo = false;
    DebugInfo = false;
    DebugNet = false;
    Enable3dRendering = false;
    FullScreen = false;
    VSync = false;
    Light = 0;
    Host = "localhost";
    Port = 4000;
    ProxyType = 0;
    ProxyHost = "";
    ProxyPort = 0;
    ProxyUser = "";
    ProxyPass = "";
    ScrollDelay = 10;
    ScrollStep = 1;
    ScrollCheck = true;
    FixedFPS = 100;
    FPS = 0;
    PingPeriod = 2000;
    Ping = 0;
    MsgboxInvert = false;
    MessNotify = true;
    SoundNotify = true;
    AlwaysOnTop = false;
    TextDelay = 3000;
    DamageHitDelay = 0;
    ScreenWidth = 1024;
    ScreenHeight = 768;
    MultiSampling = -1;
    MouseScroll = true;
    DoubleClickTime = 0;
    RoofAlpha = 200;
    HideCursor = false;
    ShowMoveCursor = false;
    DisableMouseEvents = false;
    DisableKeyboardEvents = false;
    HidePassword = true;
    PlayerOffAppendix = "_off";
    Animation3dSmoothTime = 150;
    Animation3dFPS = 30;
    RunModMul = 1;
    RunModDiv = 3;
    RunModAdd = 0;
    MapZooming = false;
    SpritesZoom = 1.0f;
    SpritesZoomMax = MAX_ZOOM;
    SpritesZoomMin = MIN_ZOOM;
    memzero( EffectValues, sizeof( EffectValues ) );
    KeyboardRemap = "";
    CritterFidgetTime = 50000;
    Anim2CombatBegin = 0;
    Anim2CombatIdle = 0;
    Anim2CombatEnd = 0;
    RainTick = 60;
    RainSpeedX = 0;
    RainSpeedY = 15;
    ConsoleHistorySize = 20;
    SoundVolume = 100;
    MusicVolume = 100;
    ChosenLightColor = 0;
    ChosenLightDistance = 4;
    ChosenLightIntensity = 2500;
    ChosenLightFlags = 0;

    // Mapper
    ServerDir = "";
    ShowCorners = false;
    ShowSpriteCuts = false;
    ShowDrawOrder = false;
    SplitTilesCollection = true;
}

void Settings::Init( int argc, char** argv )
{
    // Parse command line args
    StrVec configs;
    for( int i = 0; i < argc; i++ )
    {
        // Skip path
        if( i == 0 && argv[ 0 ][ 0 ] != '-' )
            continue;

        // Find work path entry
        if( Str::Compare( argv[ i ], "-WorkDir" ) && i < argc - 1 )
            WorkDir = argv[ i + 1 ];
        if( Str::Compare( argv[ i ], "-ServerDir" ) && i < argc - 1 )
            ServerDir = argv[ i + 1 ];

        // Find config path entry
        if( Str::Compare( argv[ i ], "-AddConfig" ) && i < argc - 1 )
            configs.push_back( argv[ i + 1 ] );

        // Make single line
        CommandLine += argv[ i ];
        if( i < argc - 1 )
            CommandLine += " ";
    }

    // Store start directory
    if( !WorkDir.empty() )
    {
        #ifdef FO_WINDOWS
        SetCurrentDirectoryW( _str( WorkDir ).toWideChar().c_str() );
        #else
        int r = chdir( WorkDir.c_str() );
        UNUSED_VARIABLE( r );
        #endif
    }
    #ifdef FO_WINDOWS
    wchar_t dir_buf[ TEMP_BUF_SIZE ];
    GetCurrentDirectoryW( TEMP_BUF_SIZE, dir_buf );
    WorkDir = _str().parseWideChar( dir_buf );
    #else
    char  dir_buf[ TEMP_BUF_SIZE ];
    char* r = getcwd( dir_buf, sizeof( dir_buf ) );
    UNUSED_VARIABLE( r );
    WorkDir = dir_buf;
    #endif

    // Injected config
    static char InternalConfig[ 5022 ] =
    {
        "###InternalConfig###\0"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    };
    MainConfig = new IniFile();
    MainConfig->AppendStr( InternalConfig );

    // File configs
    string config_dir;
    MainConfig->AppendFile( CONFIG_NAME );
    for( string& config : configs )
    {
        config_dir = _str( config ).extractDir();
        MainConfig->AppendFile( config );
    }

    // Command line config
    for( int i = 0; i < argc; i++ )
    {
        string arg = argv[ i ];
        if( arg.length() < 2 || arg[ 0 ] != '-' )
            continue;

        string arg_value;
        while( i < argc - 1 && argv[ i + 1 ][ 0 ] != '-' )
        {
            if( arg_value.length() > 0 )
                arg_value += " ";
            arg_value += argv[ i + 1 ];
            i++;
        }

        MainConfig->SetStr( "", arg.substr( 1 ), arg_value );
    }

    // Cache project files
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    string project_files = MainConfig->GetStr( "", "ProjectFiles" );
    for( string project_path : _str( project_files ).split( ';' ) )
    {
        project_path = _str( config_dir ).combinePath( project_path ).normalizePathSlashes().resolvePath();
        if( !project_path.empty() && project_path.back() != '/' && project_path.back() != '\\' )
            project_path += "/";
        ProjectFiles.push_back( _str( project_path ).normalizePathSlashes() );
    }
    #endif

    // Defines
    #define GETOPTIONS_CMD_LINE_INT( opt, str_id )                             \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = MainConfig->GetInt( "", str_id ); } while( 0 )
    #define GETOPTIONS_CMD_LINE_BOOL( opt, str_id )                            \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = MainConfig->GetInt( "", str_id ) != 0; } while( 0 )
    #define GETOPTIONS_CMD_LINE_BOOL_ON( opt, str_id )                         \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = true; } while( 0 )
    #define GETOPTIONS_CMD_LINE_STR( opt, str_id )                             \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = str; } while( 0 )
    #define GETOPTIONS_CHECK( val_, min_, max_, def_ )                  \
        do { int val__ = (int) val_; if( val__ < min_ || val__ > max_ ) \
                 val_ = def_; } while( 0 )

    string buf;
    #define READ_CFG_STR_DEF( cfg, key, def_val )    buf = MainConfig->GetStr( "", key, def_val )

    // Cached configuration
    MainConfig->AppendFile( CONFIG_NAME );

    // Language
    READ_CFG_STR_DEF( *MainConfig, "Language", "russ" );
    GETOPTIONS_CMD_LINE_STR( buf, "Language" );

    // Int / Bool
    OpenGLDebug = MainConfig->GetInt( "", "OpenGLDebug", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( OpenGLDebug, "OpenGLDebug" );
    AssimpLogging = MainConfig->GetInt( "", "AssimpLogging", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( AssimpLogging, "AssimpLogging" );
    FullScreen = MainConfig->GetInt( "", "FullScreen", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( FullScreen, "FullScreen" );
    VSync = MainConfig->GetInt( "", "VSync", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( VSync, "VSync" );
    Light = MainConfig->GetInt( "", "Light", 20 );
    GETOPTIONS_CMD_LINE_INT( Light, "Light" );
    GETOPTIONS_CHECK( Light, 0, 100, 20 );
    ScrollDelay = MainConfig->GetInt( "", "ScrollDelay", 10 );
    GETOPTIONS_CMD_LINE_INT( ScrollDelay, "ScrollDelay" );
    GETOPTIONS_CHECK( ScrollDelay, 0, 100, 10 );
    ScrollStep = MainConfig->GetInt( "", "ScrollStep", 12 );
    GETOPTIONS_CMD_LINE_INT( ScrollStep, "ScrollStep" );
    GETOPTIONS_CHECK( ScrollStep, 4, 32, 12 );
    TextDelay = MainConfig->GetInt( "", "TextDelay", 3000 );
    GETOPTIONS_CMD_LINE_INT( TextDelay, "TextDelay" );
    GETOPTIONS_CHECK( TextDelay, 1000, 30000, 3000 );
    DamageHitDelay = MainConfig->GetInt( "", "DamageHitDelay", 0 );
    GETOPTIONS_CMD_LINE_INT( DamageHitDelay, "OptDamageHitDelay" );
    GETOPTIONS_CHECK( DamageHitDelay, 0, 30000, 0 );
    ScreenWidth = MainConfig->GetInt( "", "ScreenWidth", 0 );
    GETOPTIONS_CMD_LINE_INT( ScreenWidth, "ScreenWidth" );
    GETOPTIONS_CHECK( ScreenWidth, 100, 30000, 800 );
    ScreenHeight = MainConfig->GetInt( "", "ScreenHeight", 0 );
    GETOPTIONS_CMD_LINE_INT( ScreenHeight, "ScreenHeight" );
    GETOPTIONS_CHECK( ScreenHeight, 100, 30000, 600 );
    AlwaysOnTop = MainConfig->GetInt( "", "AlwaysOnTop", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( AlwaysOnTop, "AlwaysOnTop" );
    FixedFPS = MainConfig->GetInt( "", "FixedFPS", 100 );
    GETOPTIONS_CMD_LINE_INT( FixedFPS, "FixedFPS" );
    GETOPTIONS_CHECK( FixedFPS, -10000, 10000, 100 );
    MsgboxInvert = MainConfig->GetInt( "", "InvertMessBox", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( MsgboxInvert, "InvertMessBox" );
    MessNotify = MainConfig->GetInt( "", "WinNotify", true ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( MessNotify, "WinNotify" );
    SoundNotify = MainConfig->GetInt( "", "SoundNotify", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( SoundNotify, "SoundNotify" );
    Port = MainConfig->GetInt( "", "RemotePort", 4010 );
    GETOPTIONS_CMD_LINE_INT( Port, "RemotePort" );
    GETOPTIONS_CHECK( Port, 0, 0xFFFF, 4000 );
    ProxyType = MainConfig->GetInt( "", "ProxyType", 0 );
    GETOPTIONS_CMD_LINE_INT( ProxyType, "ProxyType" );
    GETOPTIONS_CHECK( ProxyType, 0, 3, 0 );
    ProxyPort = MainConfig->GetInt( "", "ProxyPort", 8080 );
    GETOPTIONS_CMD_LINE_INT( ProxyPort, "ProxyPort" );
    GETOPTIONS_CHECK( ProxyPort, 0, 0xFFFF, 1080 );

    uint dct = 500;
    #ifdef FO_WINDOWS
    dct = (uint) GetDoubleClickTime();
    #endif
    DoubleClickTime = MainConfig->GetInt( "", "DoubleClickTime", dct );
    GETOPTIONS_CMD_LINE_INT( DoubleClickTime, "DoubleClickTime" );
    GETOPTIONS_CHECK( DoubleClickTime, 0, 1000, dct );

    GETOPTIONS_CMD_LINE_BOOL_ON( HelpInfo, "HelpInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( DebugInfo, "DebugInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( DebugNet, "DebugNet" );

    // Str
    READ_CFG_STR_DEF( *MainConfig, "RemoteHost", "localhost" );
    GETOPTIONS_CMD_LINE_STR( buf, "RemoteHost" );
    Host = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyHost", "localhost" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyHost" );
    ProxyHost = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyUser", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyUser" );
    ProxyUser = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyPass", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyPass" );
    ProxyPass = buf;
    READ_CFG_STR_DEF( *MainConfig, "KeyboardRemap", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "KeyboardRemap" );
    KeyboardRemap = buf;
}
