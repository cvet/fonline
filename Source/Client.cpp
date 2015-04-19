#include "Common.h"
#include "Client.h"
#include "Access.h"
#include "Defence.h"
#include "ScriptFunctions.h"

// Check buffer for error
#define CHECK_IN_BUFF_ERROR                              \
    if( Bin.IsError() )                                  \
    {                                                    \
        WriteLogF( _FUNC_, " - Wrong network data!\n" ); \
        NetDisconnect();                                 \
        return;                                          \
    }

void* zlib_alloc_( void* opaque, unsigned int items, unsigned int size ) { return calloc( items, size ); }
void  zlib_free_( void* opaque, void* address )                          { free( address ); }

#define MOUSE_BUTTON_LEFT          ( 0 )
#define MOUSE_BUTTON_RIGHT         ( 1 )
#define MOUSE_BUTTON_MIDDLE        ( 2 )
#define MOUSE_BUTTON_WHEEL_UP      ( 3 )
#define MOUSE_BUTTON_WHEEL_DOWN    ( 4 )
#define MOUSE_BUTTON_EXT0          ( 5 )
#define MOUSE_BUTTON_EXT1          ( 6 )
#define MOUSE_BUTTON_EXT2          ( 7 )
#define MOUSE_BUTTON_EXT3          ( 8 )
#define MOUSE_BUTTON_EXT4          ( 9 )

#ifdef FO_OSX_IOS
int HandleAppEvents( void* userdata, SDL_Event* event )
{
    switch( event->type )
    {
    case SDL_APP_TERMINATING:
        // Terminate the app.
        // Shut everything down before returning from this function.
        return 0;
    case SDL_APP_LOWMEMORY:
        // You will get this when your app is paused and iOS wants more memory.
        // Release as much memory as possible.
        return 0;
    case SDL_APP_WILLENTERBACKGROUND:
        // Prepare your app to go into the background.  Stop loops, etc.
        // This gets called when the user hits the home button, or gets a call.
        return 0;
    case SDL_APP_DIDENTERBACKGROUND:
        // This will get called if the user accepted whatever sent your app to the background.
        // If the user got a phone call and canceled it, you'll instead get an SDL_APP_DIDENTERFOREGROUND event and restart your loops.
        // When you get this, you have 5 seconds to save all your state or the app will be terminated.
        // Your app is NOT active at this point.
        return 0;
    case SDL_APP_WILLENTERFOREGROUND:
        // This call happens when your app is coming back to the foreground.
        // Restore all your state here.
        return 0;
    case SDL_APP_DIDENTERFOREGROUND:
        // Restart your loops here.
        // Your app is interactive and getting CPU again.
        return 0;
    default:
        // No special processing, add it to the event queue.
        return 1;
    }
}
#endif

FOClient*    FOClient::Self = NULL;
int          FOClient::SpritesCanDraw = 0;
static uint* UID4 = NULL;
FOClient::FOClient()
{
    Self = this;

    Active = false;
    ComLen = 4096;
    ComBuf = new char[ ComLen ];
    ZStreamOk = false;
    Sock = INVALID_SOCKET;
    BytesReceive = 0;
    BytesRealReceive = 0;
    BytesSend = 0;
    IsConnected = false;
    InitNetReason = INIT_NET_REASON_NONE;

    UpdateFilesInProgress = false;
    UpdateFilesAborted = false;
    UpdateFilesFontLoaded = false;
    UpdateFilesText = "";
    UpdateFilesList = NULL;
    UpdateFilesWholeSize = 0;
    UpdateFileActive = false;
    UpdateFileTemp = NULL;

    Chosen = NULL;
    PingTick = 0;
    PingCallTick = 0;
    IsTurnBased = false;
    TurnBasedTime = 0;
    TurnBasedCurCritterId = 0;
    DaySumRGB = 0;
    CurMode = CUR_DEFAULT;
    CurModeLast = CUR_DEFAULT;
    LMenuActive = false;
    LMenuMode = LMENU_OFF;

    GmapCar.Car = NULL;
    Animations.resize( 10000 );

    CurVideo = NULL;
    MusicVolumeRestore = -1;

    UIDFail = false;
    MoveLastHx = -1;
    MoveLastHy = -1;

    CurMapPid = 0;
    CurMapLocPid = 0;
    CurMapIndexInLoc = 0;

    SomeItem = NULL;
}

uint* UID1;
bool FOClient::Init()
{
    WriteLog( "Engine initialization...\n" );

    // Check the sizes of base types
    STATIC_ASSERT( sizeof( char ) == 1 );
    STATIC_ASSERT( sizeof( short ) == 2 );
    STATIC_ASSERT( sizeof( int ) == 4 );
    STATIC_ASSERT( sizeof( int64 ) == 8 );
    STATIC_ASSERT( sizeof( uchar ) == 1 );
    STATIC_ASSERT( sizeof( ushort ) == 2 );
    STATIC_ASSERT( sizeof( uint ) == 4 );
    STATIC_ASSERT( sizeof( uint64 ) == 8 );
    STATIC_ASSERT( sizeof( bool ) == 1 );
    #if defined ( FO_X86 )
    STATIC_ASSERT( sizeof( size_t ) == 4 );
    STATIC_ASSERT( sizeof( void* ) == 4 );
    #elif defined ( FO_X64 )
    STATIC_ASSERT( sizeof( size_t ) == 8 );
    STATIC_ASSERT( sizeof( void* ) == 8 );
    #endif

    GET_UID0( UID0 );
    UID_PREPARE_UID4_0;

    // Another check for already runned window
    #ifndef DEV_VERSION
    if( !Singleplayer )
    {
        # ifdef FO_WINDOWS
        HANDLE h = CreateEvent( NULL, FALSE, FALSE, "_fosync_" );
        if( !h || h == INVALID_HANDLE_VALUE || GetLastError() == ERROR_ALREADY_EXISTS )
            memset( MulWndArray, 1, sizeof( MulWndArray ) );
        # else
        // Todo: Linux
        # endif
    }
    #endif

    // SDL
    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) )
    {
        WriteLogF( _FUNC_, " - SDL initialization fail, error<%s>.\n", SDL_GetError() );
        return false;
    }
    GET_UID1( UID1 );
    UID_PREPARE_UID4_1;

    // SDL events
    #ifdef FO_OSX_IOS
    SDL_SetEventFilter( HandleAppEvents, NULL );
    #endif

    // Register dll script data
    GameOpt.CritterTypes = &CritType::GetRealCritType( 0 );

    struct GetDrawingSprites_
    {
        static void* GetDrawingSprites( uint& count )
        {
            Sprites& tree = Self->HexMngr.GetDrawTree();
            count = tree.Size();
            if( !count ) return NULL;
            return &( *tree.Begin() );
        }
    };
    GameOpt.GetDrawingSprites = &GetDrawingSprites_::GetDrawingSprites;

    struct GetSpriteInfo_
    {
        static void* GetSpriteInfo( uint spr_id )
        {
            return SprMngr.GetSpriteInfo( spr_id );
        }
    };
    GameOpt.GetSpriteInfo = &GetSpriteInfo_::GetSpriteInfo;

    struct GetSpriteColor_
    {
        static uint GetSpriteColor( uint spr_id, int x, int y, bool with_zoom )
        {
            return SprMngr.GetPixColor( spr_id, x, y, with_zoom );
        }
    };
    GameOpt.GetSpriteColor = &GetSpriteColor_::GetSpriteColor;

    struct IsSpriteHit_
    {
        static bool IsSpriteHit( void* sprite, int x, int y, bool check_egg )
        {
            Sprite*     sprite_ = (Sprite*) sprite;
            if( !sprite_ || !sprite_->Valid ) return false;
            SpriteInfo* si = SprMngr.GetSpriteInfo( sprite_->PSprId ? *sprite_->PSprId : sprite_->SprId );
            if( !si ) return false;
            int         sx = sprite_->ScrX - si->Width / 2 + si->OffsX + GameOpt.ScrOx + ( sprite_->OffsX ? *sprite_->OffsX : 0 );
            int         sy = sprite_->ScrY - si->Height + si->OffsY + GameOpt.ScrOy + ( sprite_->OffsY ? *sprite_->OffsY : 0 );
            if( !( sprite_ = sprite_->GetIntersected( x - sx, y - sy ) ) ) return false;
            if( check_egg && SprMngr.CompareHexEgg( sprite_->HexX, sprite_->HexY, sprite_->EggType ) && SprMngr.IsEggTransp( x, y ) ) return false;
            return true;
        }
    };
    GameOpt.IsSpriteHit = &IsSpriteHit_::IsSpriteHit;

    struct GetNameByHash_
    {
        static const char* GetNameByHash( hash h ) { return Str::GetName( h ); }
    };
    GameOpt.GetNameByHash = &GetNameByHash_::GetNameByHash;
    struct GetHashByName_
    {
        static hash GetHashByName( const char* name ) { return Str::GetHash( name ); }
    };
    GameOpt.GetHashByName = &GetHashByName_::GetHashByName;

    // Input
    Keyb::Init();
    GET_UID2( UID2 );
    UID_PREPARE_UID4_2;

    // Cache
    if( !FileExist( FileManager::GetWritePath( "default.cache", PT_CACHE ) ) )
        FileManager::CopyFile( FileManager::GetReadPath( "default.cache", PT_DATA ), FileManager::GetWritePath( "default.cache", PT_CACHE ) );
    if( !Crypt.SetCacheTable( FileManager::GetWritePath( "default.cache", PT_CACHE ) ) )
    {
        WriteLogF( _FUNC_, " - Can't set default cache.\n" );
        return false;
    }
    UID_PREPARE_UID4_3;

    // Check password in config and command line
    char       pass[ MAX_FOTEXT ];
    IniParser& cfg = IniParser::GetClientConfig();     // Also used below
    cfg.GetStr( "UserPass", "", pass );
    char*      cmd_line_pass = Str::Substring( CommandLine, "-UserPass" );
    if( cmd_line_pass )
        sscanf( cmd_line_pass + Str::Length( "-UserPass" ) + 1, "%s", pass );
    Password = pass;
    GET_UID3( UID3 );
    UID_PREPARE_UID4_4;

    // User and password
    if( !GameOpt.Name->length() && Password.empty() && !Singleplayer )
    {
        bool  fail = false;

        uint  len;
        char* str = (char*) Crypt.GetCache( "__name", len );
        if( str && len <= UTF8_BUF_SIZE( MAX_NAME ) )
            *GameOpt.Name = str;
        else
            fail = true;
        delete[] str;

        str = (char*) Crypt.GetCache( "__pass", len );
        if( str && len <= UTF8_BUF_SIZE( MAX_NAME ) )
            Password = str;
        else
            fail = true;
        delete[] str;

        if( fail )
        {
            *GameOpt.Name = "login";
            Password = "password";
            Crypt.SetCache( "__name", (uchar*) GameOpt.Name->c_str(), (uint) GameOpt.Name->length() + 1 );
            Crypt.SetCache( "__pass", (uchar*) Password.c_str(), (uint) Password.length() + 1 );
        }
    }
    UID_PREPARE_UID4_5;

    // Sprite manager
    if( !SprMngr.Init() )
        return false;
    UID_PREPARE_UID4_6;
    GET_UID4( UID4 );

    // Sound manager
    SndMngr.Init();
    GameOpt.SoundVolume = cfg.GetInt( "SoundVolume", 100 );
    GameOpt.MusicVolume = cfg.GetInt( "MusicVolume", 100 );

    // Language Packs
    char lang_name[ MAX_FOTEXT ];
    cfg.GetStr( "Language", DEFAULT_LANGUAGE, lang_name );
    if( Str::Length( lang_name ) != 4 )
        Str::Copy( lang_name, DEFAULT_LANGUAGE );
    Str::Lower( lang_name );

    bool lang_ok = CurLang.LoadFromCache( lang_name );

    MsgText = &CurLang.Msg[ TEXTMSG_TEXT ];
    MsgDlg = &CurLang.Msg[ TEXTMSG_DLG ];
    MsgItem = &CurLang.Msg[ TEXTMSG_ITEM ];
    MsgGame = &CurLang.Msg[ TEXTMSG_GAME ];
    MsgLocations = &CurLang.Msg[ TEXTMSG_LOCATIONS ];
    MsgCombat = &CurLang.Msg[ TEXTMSG_COMBAT ];
    MsgQuest = &CurLang.Msg[ TEXTMSG_QUEST ];
    MsgHolo = &CurLang.Msg[ TEXTMSG_HOLO ];
    MsgCraft = &CurLang.Msg[ TEXTMSG_CRAFT ];
    MsgInternal = &CurLang.Msg[ TEXTMSG_INTERNAL ];
    MsgUserHolo = new FOMsg();
    MsgUserHolo->LoadFromFile( USER_HOLO_TEXTMSG_FILE, PT_TEXTS );

    // Update
    UpdateFiles( true );

    // Find valid cache
    if( !lang_ok )
    {
        StrVec processed_langs;
        processed_langs.push_back( CurLang.NameStr );
        StrVec msg_entires;
        Crypt.GetCacheNames( CACHE_MSG_PREFIX, msg_entires );
        for( size_t i = 0, j = msg_entires.size(); i < j; i++ )
        {
            string lang = msg_entires[ i ].substr( Str::Length( CACHE_MSG_PREFIX "\\" ), 4 );
            if( lang.length() == 4 && std::find( processed_langs.begin(), processed_langs.end(), lang ) == processed_langs.end() )
            {
                processed_langs.push_back( lang );
                lang_ok = CurLang.LoadFromCache( lang.c_str() );
                if( lang_ok )
                    break;
            }
        }
    }
    if( !lang_ok )
    {
        WriteLog( "Language packs not found!\n" );
        return false;
    }

    // Cursor position
    int sw = 0, sh = 0;
    SDL_GetWindowSize( MainWindow, &sw, &sh );
    int mx = 0, my = 0;
    SDL_GetMouseState( &mx, &my );
    GameOpt.MouseX = CLAMP( mx, 0, sw - 1 );
    GameOpt.MouseY = CLAMP( my, 0, sh - 1 );

    // CritterCl types
    CritType::InitFromMsg( MsgInternal );

    // Resource manager
    ResMngr.Refresh();

    // Wait screen
    ScreenModeMain = SCREEN_WAIT;
    CurMode = CUR_WAIT;
    WaitPic = ResMngr.GetRandomSplash();
    if( SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) ) )
    {
        WaitDraw();
        SprMngr.EndScene();
    }

    // Constants
    ConstantsManager::Initialize( PT_DATA );

    // Base ini options
    if( !AppendIfaceIni( NULL ) )
        return false;

    // Scripts
    if( !ReloadScripts() )
        return false;

    // Recreate static atlas
    ResMngr.FreeResources( RES_ATLAS_STATIC );
    SprMngr.AccumulateAtlasData();
    SprMngr.PushAtlasType( RES_ATLAS_STATIC );

    // Start script
    if( !Script::PrepareContext( ClientFunctions.Start, _FUNC_, "Game" ) || !Script::RunPrepared() || !Script::GetReturnedBool() )
    {
        WriteLog( "Execute start script fail.\n" );
        return false;
    }

    // 3d initialization
    WriteLog( "3d rendering is %s.\n", GameOpt.Enable3dRendering ? "enabled" : "disabled" );
    if( GameOpt.Enable3dRendering && !Animation3d::StartUp() )
    {
        WriteLog( "Can't initialize 3d rendering.\n" );
        return false;
    }

    // Initialize main interface
    int result = InitIface();
    if( result != 0 )
    {
        WriteLog( "Interface initialization fail on line<%d>.\n", result );
        return false;
    }

    // Load fonts
    if( !SprMngr.LoadFontFO( FONT_FO, "OldDefault", false ) ||
        !SprMngr.LoadFontFO( FONT_NUM, "Numbers", true ) ||
        !SprMngr.LoadFontFO( FONT_BIG_NUM, "BigNumbers", true ) ||
        !SprMngr.LoadFontFO( FONT_SAND_NUM, "SandNumbers", false ) ||
        !SprMngr.LoadFontFO( FONT_SPECIAL, "Special", false ) ||
        !SprMngr.LoadFontFO( FONT_DEFAULT, "Default", false ) ||
        !SprMngr.LoadFontFO( FONT_THIN, "Thin", false ) ||
        !SprMngr.LoadFontFO( FONT_FAT, "Fat", false ) ||
        !SprMngr.LoadFontFO( FONT_BIG, "Big", false ) )
    {
        WriteLog( "Fonts initialization fail.\n" );
        return false;
    }

    // Flush atlas data
    SprMngr.PopAtlasType();
    SprMngr.FlushAccumulatedAtlasData();

    // Finish fonts
    SprMngr.BuildFonts();
    SprMngr.SetDefaultFont( FONT_DEFAULT, COLOR_TEXT );

    // Preload 3d files
    if( GameOpt.Enable3dRendering && !Preload3dFiles.empty() )
    {
        WriteLog( "Preload 3d files...\n" );
        for( size_t i = 0, j = Preload3dFiles.size(); i < j; i++ )
            Animation3dEntity::GetEntity( Preload3dFiles[ i ].c_str() );
        WriteLog( "Preload 3d files complete.\n" );
    }

    // Quest Manager
    QuestMngr.Init( MsgQuest );

    // Item manager
    if( !ItemMngr.Init() )
        return false;

    // Item prototypes
    UCharVec protos_data;
    if( Crypt.GetCache( CACHE_ITEM_PROTOS, protos_data ) )
        ItemMngr.SetBinaryData( protos_data );

    // MrFixit
    MrFixit.LoadCrafts( *MsgCraft );
    MrFixit.GenerateNames( *MsgGame, *MsgItem );  // After Item manager init

    // Hex manager
    if( !HexMngr.Init() )
        return false;

    // Other
    SetGameColor( COLOR_IFACE );
    ScreenOffsX = 0;
    ScreenOffsY = 0;
    ScreenOffsXf = 0.0f;
    ScreenOffsYf = 0.0f;
    ScreenOffsStep = 0.0f;
    ScreenOffsNextTick = 0;
    ScreenMirrorTexture = NULL;
    ScreenMirrorEndTick = 0;
    ScreenMirrorStart = false;
    RebuildLookBorders = false;
    DrawLookBorders = false;
    DrawShootBorders = false;

    LookBorders.clear();
    ShootBorders.clear();

    WriteLog( "Engine initialization complete.\n" );
    Active = true;

    // Begin game
    if( Str::Substring( CommandLine, "-Start" ) && !Singleplayer )
    {
        ConnectToGame();
    }
    // Intro
    else if( !Str::Substring( CommandLine, "-SkipIntro" ) )
    {
        if( MsgGame->Count( STR_MUSIC_MAIN_THEME ) )
            MusicAfterVideo = MsgGame->GetStr( STR_MUSIC_MAIN_THEME );
        for( uint i = STR_VIDEO_INTRO_BEGIN; i < STR_VIDEO_INTRO_END; i++ )
            if( MsgGame->Count( i ) )
                AddVideo( MsgGame->GetStr( i, 0 ), true, false );

        if( !IsVideoPlayed() )
        {
            ScreenFadeOut();
            if( MusicAfterVideo != "" )
            {
                SndMngr.PlayMusic( MusicAfterVideo.c_str() );
                MusicAfterVideo = "";
            }
        }
    }
    else
    {
        ScreenFadeOut();
        if( MsgGame->Count( STR_MUSIC_MAIN_THEME ) )
            SndMngr.PlayMusic( MsgGame->GetStr( STR_MUSIC_MAIN_THEME ) );
    }

    // Disable dumps if multiple window detected
    if( MulWndArray[ 11 ] )
        CatchExceptions( NULL, 0 );

    return true;
}

void FOClient::UpdateFiles( bool early_call )
{
    // Load font
    SprMngr.PushAtlasType( RES_ATLAS_STATIC );
    UpdateFilesFontLoaded = SprMngr.LoadFontFO( FONT_DEFAULT, "Default", false );
    if( !UpdateFilesFontLoaded )
        UpdateFilesFontLoaded = SprMngr.LoadFontBMF( FONT_DEFAULT, "Default" );
    if( UpdateFilesFontLoaded )
        SprMngr.BuildFonts();
    SprMngr.PopAtlasType();

    // Update
    UpdateFilesAddText( STR_CHECK_UPDATES, "CHECK_UPDATES" );
    UpdateFilesInProgress = true;
    UpdateFilesText = "";
    bool cache_changed = false;
    bool files_changed = false;
    for( int t = 0; t < 5; t++ )
    {
        // Connect to server
        UpdateFilesAddText( STR_CONNECT_TO_SERVER, "CONNECT_TO_SERVER" );
        const char* host = ( GameOpt.UpdateServerHost->length() > 0 ? GameOpt.UpdateServerHost->c_str() : GameOpt.Host->c_str() );
        ushort      port = ( GameOpt.UpdateServerPort != 0 ? GameOpt.UpdateServerPort : GameOpt.Port );
        if( !NetConnect( host, port ) )
        {
            UpdateFilesAddText( STR_CANT_CONNECT_TO_SERVER, "CANT_CONNECT_TO_SERVER" );
            UpdateFilesWait( 10000 );
            continue;
        }
        UpdateFilesAddText( STR_CONNECTION_ESTABLISHED, "CONNECTION_ESTABLISHED" );

        // Data synchronization
        UpdateFilesAddText( STR_DATA_SYNCHRONIZATION, "DATA_SYNCHRONIZATION" );

        UpdateFilesAborted = false;
        SAFEDEL( UpdateFilesList );
        UpdateFileActive = false;
        FileManager::DeleteFile( FileManager::GetWritePath( "update.temp", PT_DATA ) );

        Net_SendUpdate();

        uint tick = Timer::FastTick();
        while( IsConnected )
        {
            if( !early_call && UpdateFilesList && !UpdateFilesList->empty() )
                UpdateFilesAbort( STR_CLIENT_DATA_OUTDATED, "STR_CLIENT_DATA_OUTDATED" );

            int    dots = ( Timer::FastTick() - tick ) / 100 % 50 + 1;
            string dots_str = "";
            for( int i = 0; i < dots; i++ )
                dots_str += ".";

            string progress = "";
            if( UpdateFilesList && !UpdateFilesList->empty() )
            {
                progress += "\n";
                for( size_t i = 0, j = UpdateFilesList->size(); i < j; i++ )
                {
                    UpdateFile& update_file = ( *UpdateFilesList )[ i ];
                    float       cur = (float) ( update_file.Size - update_file.RemaningSize ) / ( 1024.0f * 1024.0f );
                    float       max = MAX( (float) update_file.Size / ( 1024.0f * 1024.0f ), 0.01f );
                    char        buf[ MAX_FOTEXT ];
                    char        name[ MAX_FOPATH ];

                    Str::Copy( name, update_file.Name.c_str() );
                    FileManager::FormatPath( name );

                    progress += Str::Format( buf, "%s %.2f / %.2f MB\n", name, cur, max );
                }
                progress += "\n";
            }

            if( !Singleplayer )
            {
                SprMngr.BeginScene( COLOR_RGB( 50, 50, 50 ) );
                string update_text = UpdateFilesText + progress + dots_str;
                SprMngr.DrawStr( Rect( 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), update_text.c_str(), FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT );
                SprMngr.EndScene();
            }

            if( UpdateFilesList && !UpdateFileActive )
            {
                if( !UpdateFilesList->empty() )
                {
                    if( UpdateFileTemp )
                        FileClose( UpdateFileTemp );

                    UpdateFileTemp = FileOpen( FileManager::GetWritePath( "update.temp", PT_DATA ), true );
                    if( !UpdateFileTemp )
                    {
                        UpdateFilesAddText( STR_FILESYSTEM_ERROR, "FILESYSTEM_ERROR0" );
                        NetDisconnect();
                        UpdateFilesWait( 10000 );
                        continue;
                    }

                    if( UpdateFilesList->front().Name[ 0 ] == CACHE_MAGIC_CHAR[ 0 ] )
                        cache_changed = true;
                    else
                        files_changed = true;

                    UpdateFileActive = true;

                    Bout << NETMSG_GET_UPDATE_FILE;
                    Bout << UpdateFilesList->front().Index;
                }
                else
                {
                    // Done
                    SAFEDEL( UpdateFilesList );
                    UpdateFilesInProgress = false;
                    NetDisconnect();

                    // Reinitialize data
                    if( cache_changed )
                    {
                        CurLang.LoadFromCache( CurLang.NameStr );
                    }
                    if( files_changed )
                    {
                        FileManager::ClearDataFiles();
                        #ifdef FO_OSX_IOS
                        FileManager::InitDataFiles( "../../Documents/" );
                        #endif
                        FileManager::InitDataFiles( DIR_SLASH_SD "data" DIR_SLASH_S );
                    }

                    return;
                }
            }

            UpdateFilesWait( 0 );

            ParseSocket();
        }

        if( !UpdateFilesAborted )
            UpdateFilesAddText( STR_CONNECTION_FAILTURE, "CONNECTION_FAILTURE" );
        UpdateFilesWait( 10000 );
    }

    ExitProcess( 0 );
}

void FOClient::UpdateFilesAddText( uint num_str, const char* num_str_str )
{
    if( Singleplayer )
    {
        UpdateFilesText = "";
        num_str = STR_START_SINGLEPLAYER;
        num_str_str = "STR_START_SINGLEPLAYER";
    }

    if( !UpdateFilesFontLoaded )
    {
        WriteLog( "Update files message<%s>.\n", num_str_str );
        SprMngr.BeginScene( COLOR_RGB( Random( 0, 255 ), Random( 0, 255 ), Random( 0, 255 ) ) );
        SprMngr.EndScene();
    }
    else
    {
        const char* text = ( CurLang.Msg[ TEXTMSG_GAME ].Count( num_str ) ? CurLang.Msg[ TEXTMSG_GAME ].GetStr( num_str ) : num_str_str );
        UpdateFilesText += string( text ) + "\n";
        SprMngr.BeginScene( COLOR_RGB( 50, 50, 50 ) );
        SprMngr.DrawStr( Rect( 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), UpdateFilesText.c_str(), FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT );
        SprMngr.EndScene();
    }

    UpdateFilesWait( 0 );
}

void FOClient::UpdateFilesAbort( uint num_str, const char* num_str_str )
{
    UpdateFilesAddText( num_str, num_str_str );
    UpdateFilesAborted = true;
    NetDisconnect();
    if( UpdateFileTemp )
    {
        FileClose( UpdateFileTemp );
        UpdateFileTemp = NULL;
    }

    while( num_str == STR_CLIENT_OUTDATED || num_str == STR_CLIENT_OUTDATED_APP_STORE || num_str == STR_CLIENT_OUTDATED_GOOGLE_PLAY ||
           num_str == STR_CLIENT_UPDATED || num_str == STR_CLIENT_DATA_OUTDATED )
    {
        SprMngr.BeginScene( COLOR_RGB( 255, 0, 0 ) );
        SprMngr.DrawStr( Rect( 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), UpdateFilesText.c_str(), FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT );
        SprMngr.EndScene();

        SDL_Event event;
        while( SDL_PollEvent( &event ) )
            if( event.type == SDL_KEYDOWN || event.type == SDL_QUIT )
                ExitProcess( 0 );
    }
}

void FOClient::UpdateFilesWait( uint time )
{
    uint tick = Timer::FastTick();
    do
    {
        SDL_Event event;
        while( SDL_PollEvent( &event ) )
            if( ( event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE ) || event.type == SDL_QUIT )
                ExitProcess( 0 );
        if( time )
            Thread::Sleep( 1 );
    }
    while( Timer::FastTick() - tick < time );
}

void FOClient::Finish()
{
    WriteLog( "Engine finish...\n" );

    NetDisconnect();
    ResMngr.Finish();
    HexMngr.Finish();
    SprMngr.Finish();
    SndMngr.Finish();
    QuestMngr.Finish();
    MrFixit.Finish();
    Script::Finish();

    SAFEDELA( ComBuf );

    for( auto it = IntellectWords.begin(), end = IntellectWords.end(); it != end; ++it )
    {
        delete[] ( *it ).first;
        delete[] ( *it ).second;
    }
    IntellectWords.clear();
    for( auto it = IntellectSymbols.begin(), end = IntellectSymbols.end(); it != end; ++it )
    {
        delete[] ( *it ).first;
        delete[] ( *it ).second;
    }
    IntellectSymbols.clear();
    FileManager::ClearDataFiles();

    Active = false;
    WriteLog( "Engine finish complete.\n" );
}

void FOClient::ClearCritters()
{
    HexMngr.ClearCritters();
    Chosen = NULL;
    ChosenAction.clear();
    Item::ClearItems( InvContInit );
    Item::ClearItems( BarterCont1oInit );
    Item::ClearItems( BarterCont2Init );
    Item::ClearItems( BarterCont2oInit );
    Item::ClearItems( PupCont2Init );
    Item::ClearItems( PupCont2 );
    Item::ClearItems( InvCont );
    Item::ClearItems( BarterCont1 );
    Item::ClearItems( PupCont1 );
    Item::ClearItems( UseCont );
    Item::ClearItems( BarterCont1o );
    Item::ClearItems( BarterCont2 );
    Item::ClearItems( BarterCont2o );
    Item::ClearItems( PupCont2 );
}

void FOClient::AddCritter( CritterCl* cr )
{
    uint       fading = 0;
    CritterCl* cr_ = GetCritter( cr->GetId() );
    if( cr_ )
        fading = cr_->FadingTick;
    EraseCritter( cr->GetId() );
    if( HexMngr.IsMapLoaded() )
    {
        Field& f = HexMngr.GetField( cr->GetHexX(), cr->GetHexY() );
        if( f.Crit && f.Crit->IsFinishing() )
            EraseCritter( f.Crit->GetId() );
    }
    if( cr->IsChosen() )
        Chosen = cr;
    HexMngr.AddCrit( cr );
    cr->FadingTick = Timer::GameTick() + FADING_PERIOD - ( fading > Timer::GameTick() ? fading - Timer::GameTick() : 0 );
}

void FOClient::EraseCritter( uint remid )
{
    if( Chosen && Chosen->GetId() == remid )
        Chosen = NULL;
    HexMngr.EraseCrit( remid );
}

void FOClient::LookBordersPrepare()
{
    if( !DrawLookBorders && !DrawShootBorders )
        return;

    LookBorders.clear();
    ShootBorders.clear();
    if( HexMngr.IsMapLoaded() && Chosen )
    {
        uint   dist = Chosen->GetLook();
        ushort base_hx = Chosen->GetHexX();
        ushort base_hy = Chosen->GetHexY();
        int    hx = base_hx;
        int    hy = base_hy;
        int    chosen_dir = Chosen->GetDir();
        uint   dist_shoot = Chosen->GetAttackDist();
        ushort maxhx = HexMngr.GetMaxHexX();
        ushort maxhy = HexMngr.GetMaxHexY();
        bool   seek_start = true;
        for( int i = 0; i < ( GameOpt.MapHexagonal ? 6 : 4 ); i++ )
        {
            int dir = ( GameOpt.MapHexagonal ? ( i + 2 ) % 6 : ( ( i + 1 ) * 2 ) % 8 );

            for( uint j = 0, jj = ( GameOpt.MapHexagonal ? dist : dist * 2 ); j < jj; j++ )
            {
                if( seek_start )
                {
                    // Move to start position
                    for( uint l = 0; l < dist; l++ )
                        MoveHexByDirUnsafe( hx, hy, GameOpt.MapHexagonal ? 0 : 7 );
                    seek_start = false;
                    j = -1;
                }
                else
                {
                    // Move to next hex
                    MoveHexByDirUnsafe( hx, hy, dir );
                }

                ushort hx_ = CLAMP( hx, 0, maxhx - 1 );
                ushort hy_ = CLAMP( hy, 0, maxhy - 1 );
                if( FLAG( GameOpt.LookChecks, LOOK_CHECK_DIR ) )
                {
                    int dir_ = GetFarDir( base_hx, base_hy, hx_, hy_ );
                    int ii = ( chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir );
                    if( ii > DIRS_COUNT / 2 )
                        ii = DIRS_COUNT - ii;
                    uint       dist_ = dist - dist * GameOpt.LookDir[ ii ] / 100;
                    UShortPair block;
                    HexMngr.TraceBullet( base_hx, base_hy, hx_, hy_, dist_, 0.0f, NULL, false, NULL, 0, NULL, &block, NULL, false );
                    hx_ = block.first;
                    hy_ = block.second;
                }

                if( FLAG( GameOpt.LookChecks, LOOK_CHECK_TRACE ) )
                {
                    UShortPair block;
                    HexMngr.TraceBullet( base_hx, base_hy, hx_, hy_, 0, 0.0f, NULL, false, NULL, 0, NULL, &block, NULL, true );
                    hx_ = block.first;
                    hy_ = block.second;
                }

                ushort     hx__ = hx_;
                ushort     hy__ = hy_;
                uint       dist_look = DistGame( base_hx, base_hy, hx_, hy_ );
                UShortPair block;
                HexMngr.TraceBullet( base_hx, base_hy, hx_, hy_, MIN( dist_look, dist_shoot ), 0.0f, NULL, false, NULL, 0, NULL, &block, NULL, true );
                hx__ = block.first;
                hy__ = block.second;

                int x, y, x_, y_;
                HexMngr.GetHexCurrentPosition( hx_, hy_, x, y );
                HexMngr.GetHexCurrentPosition( hx__, hy__, x_, y_ );
                LookBorders.push_back( PrepPoint( x + HEX_OX, y + HEX_OY, COLOR_RGBA( 80, 0, 255, 0 ), (short*) &GameOpt.ScrOx, (short*) &GameOpt.ScrOy ) );
                ShootBorders.push_back( PrepPoint( x_ + HEX_OX, y_ + HEX_OY, COLOR_RGBA( 80, 255, 0, 0 ), (short*) &GameOpt.ScrOx, (short*) &GameOpt.ScrOy ) );
            }
        }

        if( LookBorders.size() < 2 )
            LookBorders.clear();
        else
            LookBorders.push_back( *LookBorders.begin() );
        if( ShootBorders.size() < 2 )
            ShootBorders.clear();
        else
            ShootBorders.push_back( *ShootBorders.begin() );
    }
}

void FOClient::LookBordersDraw()
{
    if( RebuildLookBorders )
    {
        LookBordersPrepare();
        RebuildLookBorders = false;
    }
    if( DrawLookBorders )
        SprMngr.DrawPoints( LookBorders, PRIMITIVE_LINESTRIP, &GameOpt.SpritesZoom );
    if( DrawShootBorders )
        SprMngr.DrawPoints( ShootBorders, PRIMITIVE_LINESTRIP, &GameOpt.SpritesZoom );
}

int FOClient::MainLoop()
{
    // Fixed FPS
    double start_loop = Timer::AccurateTick();

    // FPS counter
    static uint last_call = Timer::FastTick();
    static uint call_counter = 0;
    if( ( Timer::FastTick() - last_call ) >= 1000 )
    {
        GameOpt.FPS = call_counter;
        call_counter = 0;
        last_call = Timer::FastTick();
    }
    else
    {
        call_counter++;
    }

    // Input events
    SDL_Event event;
    while( SDL_PollEvent( &event ) )
    {
        if( event.type == SDL_MOUSEMOTION )
        {
            int sw = 0, sh = 0;
            SDL_GetWindowSize( MainWindow, &sw, &sh );
            int x = (int) ( event.motion.x / (float) sw * (float) GameOpt.ScreenWidth );
            int y = (int) ( event.motion.y / (float) sh * (float) GameOpt.ScreenHeight );
            GameOpt.MouseX = CLAMP( x, 0, GameOpt.ScreenWidth - 1 );
            GameOpt.MouseY = CLAMP( y, 0, GameOpt.ScreenHeight - 1 );
        }
        else if( event.type == SDL_KEYDOWN || event.type == SDL_KEYUP )
        {
            MainWindowKeyboardEvents.push_back( event.type );
            MainWindowKeyboardEvents.push_back( event.key.keysym.scancode );
            MainWindowKeyboardEventsText.push_back( "" );
        }
        else if( event.type == SDL_TEXTINPUT )
        {
            MainWindowKeyboardEvents.push_back( SDL_KEYDOWN );
            MainWindowKeyboardEvents.push_back( 510 );
            MainWindowKeyboardEventsText.push_back( event.text.text );
            MainWindowKeyboardEvents.push_back( SDL_KEYUP );
            MainWindowKeyboardEvents.push_back( 510 );
            MainWindowKeyboardEventsText.push_back( "" );
        }
        else if( event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP )
        {
            int button = -1;
            if( event.button.button == SDL_BUTTON_LEFT )
                button = MOUSE_BUTTON_LEFT;
            if( event.button.button == SDL_BUTTON_RIGHT )
                button = MOUSE_BUTTON_RIGHT;
            if( event.button.button == SDL_BUTTON_MIDDLE )
                button = MOUSE_BUTTON_MIDDLE;
            if( event.button.button == SDL_BUTTON( 4 ) )
                button = MOUSE_BUTTON_EXT0;
            if( event.button.button == SDL_BUTTON( 5 ) )
                button = MOUSE_BUTTON_EXT1;
            if( event.button.button == SDL_BUTTON( 6 ) )
                button = MOUSE_BUTTON_EXT2;
            if( event.button.button == SDL_BUTTON( 7 ) )
                button = MOUSE_BUTTON_EXT3;
            if( event.button.button == SDL_BUTTON( 8 ) )
                button = MOUSE_BUTTON_EXT4;
            if( button != -1 )
            {
                MainWindowMouseEvents.push_back( event.type );
                MainWindowMouseEvents.push_back( button );
            }
        }
        else if( event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP )
        {
            MainWindowMouseEvents.push_back( event.type == SDL_FINGERDOWN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP );
            MainWindowMouseEvents.push_back( MOUSE_BUTTON_LEFT );
            GameOpt.MouseX = (int) ( event.tfinger.x * (float) GameOpt.ScreenWidth );
            GameOpt.MouseY = (int) ( event.tfinger.y * (float) GameOpt.ScreenHeight );
        }
        else if( event.type == SDL_MOUSEWHEEL )
        {
            MainWindowMouseEvents.push_back( SDL_MOUSEBUTTONDOWN );
            MainWindowMouseEvents.push_back( event.wheel.y > 0 ? MOUSE_BUTTON_WHEEL_UP : MOUSE_BUTTON_WHEEL_DOWN );
            MainWindowMouseEvents.push_back( SDL_MOUSEBUTTONUP );
            MainWindowMouseEvents.push_back( event.wheel.y > 0 ? MOUSE_BUTTON_WHEEL_UP : MOUSE_BUTTON_WHEEL_DOWN );
        }
        else if( event.type == SDL_QUIT )
        {
            GameOpt.Quit = true;
        }
    }

    // Singleplayer data synchronization
    if( Singleplayer )
    {
        #ifdef FO_WINDOWS
        bool pause = SingleplayerData.Pause;
        if( !pause )
        {
            int main_screen = GetMainScreen();
            if( ( main_screen != SCREEN_GAME && main_screen != SCREEN_GLOBAL_MAP && main_screen != SCREEN_WAIT ) || IsScreenPresent( SCREEN__MENU_OPTION ) )
                pause = true;
        }

        SingleplayerData.Lock();         // Read data
        if( pause != SingleplayerData.Pause )
        {
            SingleplayerData.Pause = pause;
            Timer::SetGamePause( pause );
        }
        SingleplayerData.Unlock();         // Write data
        #endif
    }

    CHECK_MULTIPLY_WINDOWS0;

    // Network
    if( InitNetReason != INIT_NET_REASON_NONE )
    {
        // Reason
        int reason = InitNetReason;
        InitNetReason = INIT_NET_REASON_NONE;

        // Check updates
        if( reason != INIT_NET_REASON_LOGIN2 )
            UpdateFiles( false );

        // Wait screen
        ShowMainScreen( SCREEN_WAIT );

        // Connect to server
        if( !NetConnect( GameOpt.Host->c_str(), GameOpt.Port ) )
        {
            ShowMainScreen( SCREEN_LOGIN );
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_CONN_FAIL ) );
            return 1;
        }

        // After connect things
        if( reason == INIT_NET_REASON_LOGIN || reason == INIT_NET_REASON_LOGIN2 )
            Net_SendLogIn( GameOpt.Name->c_str(), Password.c_str() );
        else if( reason == INIT_NET_REASON_REG )
            Net_SendCreatePlayer();
        else if( reason == INIT_NET_REASON_LOAD )
            Net_SendSaveLoad( false, SaveLoadFileName.c_str(), NULL );
        else
            NetDisconnect();
    }

    // Parse Net
    if( IsConnected )
        ParseSocket();

    // Exit in Login screen if net disconnect
    if( !IsConnected && !IsMainScreen( SCREEN_LOGIN ) && !IsMainScreen( SCREEN_REGISTRATION ) && !IsMainScreen( SCREEN_CREDITS ) && !IsMainScreen( SCREEN_OPTIONS ) )
        ShowMainScreen( SCREEN_LOGIN );

    // Input
    ParseKeyboard();
    ParseMouse();

    // Process
    SoundProcess();
    AnimProcess();
    IboxProcess();
    CHECK_MULTIPLY_WINDOWS1;

    // Game time
    ushort full_second = GameOpt.FullSecond;
    Timer::ProcessGameTime();
    if( full_second != GameOpt.FullSecond )
        SetDayTime( false );

    if( IsMainScreen( SCREEN_GLOBAL_MAP ) )
    {
        CrittersProcess();
        GmapProcess();
        LMenuTryCreate();
    }
    else if( IsMainScreen( SCREEN_GAME ) && HexMngr.IsMapLoaded() )
    {
        int screen = GetActiveScreen();
        if( screen == SCREEN_NONE || screen == SCREEN__TOWN_VIEW || HexMngr.AutoScroll.Active )
        {
            if( HexMngr.Scroll() )
            {
                LMenuSet( LMENU_OFF );
                LookBordersPrepare();
            }
        }
        CrittersProcess();
        HexMngr.ProcessItems();
        HexMngr.ProcessRain();
        LMenuTryCreate();
    }
    else if( IsMainScreen( SCREEN_CREDITS ) )
    {
        // CreditsDraw();
    }

    if( IsScreenPresent( SCREEN__ELEVATOR ) )
        ElevatorProcess();

    CHECK_MULTIPLY_WINDOWS2;

    // Script loop
    static uint next_call = 0;
    if( Timer::FastTick() >= next_call )
    {
        uint wait_tick = 1000;
        if( Script::PrepareContext( ClientFunctions.Loop, _FUNC_, "Game" ) && Script::RunPrepared() )
            wait_tick = Script::GetReturnedUInt();
        next_call = Timer::FastTick() + wait_tick;
    }

    // Video
    if( IsVideoPlayed() )
    {
        RenderVideo();
        return 1;
    }

    CHECK_MULTIPLY_WINDOWS3;

    // Render
    if( !SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) ) )
        return 0;

    ProcessScreenEffectQuake();

    DrawIfaceLayer( 1 );

    if( GetMainScreen() == SCREEN_GAME && HexMngr.IsMapLoaded() )
    {
        GameDraw();
        if( SaveLoadProcessDraft )
            SaveLoadFillDraft();
        ProcessScreenEffectMirror();
    }

    DrawIfaceLayer( 2 );

    if( SaveLoadProcessDraft && GetMainScreen() == SCREEN_GLOBAL_MAP )
        SaveLoadFillDraft();

    CHECK_MULTIPLY_WINDOWS4;

    DrawIfaceLayer( 3 );
    DrawIfaceLayer( 4 );

    SprMngr.Flush();
    ProcessScreenEffectFading();

    CHECK_MULTIPLY_WINDOWS5;

    SprMngr.EndScene();

    // Fixed FPS
    if( !GameOpt.VSync && GameOpt.FixedFPS )
    {
        if( GameOpt.FixedFPS > 0 )
        {
            static double balance = 0.0;
            double        elapsed = Timer::AccurateTick() - start_loop;
            double        need_elapsed = 1000.0 / (double) GameOpt.FixedFPS;
            if( need_elapsed > elapsed )
            {
                double sleep = need_elapsed - elapsed + balance;
                balance = fmod ( sleep, 1.0 );
                Thread::Sleep( (uint) floor( sleep) );
            }
        }
        else
        {
            Thread::Sleep( -GameOpt.FixedFPS );
        }
    }

    return 1;
}

void FOClient::ScreenFade( uint time, uint from_color, uint to_color, bool push_back )
{
    if( !push_back || ScreenEffects.empty() )
    {
        ScreenEffects.push_back( ScreenEffect( Timer::FastTick(), time, from_color, to_color ) );
    }
    else
    {
        uint last_tick = 0;
        for( auto it = ScreenEffects.begin(); it != ScreenEffects.end(); ++it )
        {
            ScreenEffect& e = ( *it );
            if( e.BeginTick + e.Time > last_tick )
                last_tick = e.BeginTick + e.Time;
        }
        ScreenEffects.push_back( ScreenEffect( last_tick, time, from_color, to_color ) );
    }
}

void FOClient::ScreenQuake( int noise, uint time )
{
    GameOpt.ScrOx -= ScreenOffsX;
    GameOpt.ScrOy -= ScreenOffsY;
    ScreenOffsX = Random( 0, 1 ) ? noise : -noise;
    ScreenOffsY = Random( 0, 1 ) ? noise : -noise;
    ScreenOffsXf = (float) ScreenOffsX;
    ScreenOffsYf = (float) ScreenOffsY;
    ScreenOffsStep = fabs( ScreenOffsXf ) / ( time / 30 );
    GameOpt.ScrOx += ScreenOffsX;
    GameOpt.ScrOy += ScreenOffsY;
    ScreenOffsNextTick = Timer::GameTick() + 30;
}

void FOClient::ProcessScreenEffectFading()
{
    PointVec full_screen_quad;
    SprMngr.PrepareSquare( full_screen_quad, Rect( 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), 0 );

    for( auto it = ScreenEffects.begin(); it != ScreenEffects.end();)
    {
        ScreenEffect& e = ( *it );
        if( Timer::FastTick() >= e.BeginTick + e.Time )
        {
            it = ScreenEffects.erase( it );
            continue;
        }
        else if( Timer::FastTick() >= e.BeginTick )
        {
            int proc = Procent( e.Time, Timer::FastTick() - e.BeginTick ) + 1;
            int res[ 4 ];

            for( int i = 0; i < 4; i++ )
            {
                int sc = ( (uchar*) &e.StartColor )[ i ];
                int ec = ( (uchar*) &e.EndColor )[ i ];
                int dc = ec - sc;
                res[ i ] = sc + dc * proc / 100;
            }

            uint color = COLOR_RGBA( res[ 3 ], res[ 2 ], res[ 1 ], res[ 0 ] );
            for( int i = 0; i < 6; i++ )
                full_screen_quad[ i ].PointColor = color;

            SprMngr.DrawPoints( full_screen_quad, PRIMITIVE_TRIANGLELIST );
        }
        it++;
    }
}

void FOClient::ProcessScreenEffectQuake()
{
    if( ( ScreenOffsX || ScreenOffsY ) && Timer::GameTick() >= ScreenOffsNextTick )
    {
        GameOpt.ScrOx -= ScreenOffsX;
        GameOpt.ScrOy -= ScreenOffsY;
        if( ScreenOffsXf < 0.0f )
            ScreenOffsXf += ScreenOffsStep;
        else if( ScreenOffsXf > 0.0f )
            ScreenOffsXf -= ScreenOffsStep;
        if( ScreenOffsYf < 0.0f )
            ScreenOffsYf += ScreenOffsStep;
        else if( ScreenOffsYf > 0.0f )
            ScreenOffsYf -= ScreenOffsStep;
        ScreenOffsXf = -ScreenOffsXf;
        ScreenOffsYf = -ScreenOffsYf;
        ScreenOffsX = (int) ScreenOffsXf;
        ScreenOffsY = (int) ScreenOffsYf;
        GameOpt.ScrOx += ScreenOffsX;
        GameOpt.ScrOy += ScreenOffsY;
        ScreenOffsNextTick = Timer::GameTick() + 30;
    }
}

void FOClient::ProcessScreenEffectMirror()
{
/*
        if(ScreenMirrorStart)
        {
                ScreenQuake(10,1000);
                ScreenMirrorX=0;
                ScreenMirrorY=0;
                SAFEREL(ScreenMirrorTexture);
                ScreenMirrorEndTick=Timer::FastTick()+1000;
                ScreenMirrorStart=false;

                if(FAILED(SprMngr.GetDevice()->CreateTexture(MODE_WIDTH,MODE_HEIGHT,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&ScreenMirrorTexture))) return;
                LPDIRECT3DSURFACE8 mirror=NULL;
                LPDIRECT3DSURFACE8 back_buf=NULL;
                if(SUCCEEDED(ScreenMirrorTexture->GetSurfaceLevel(0,&mirror))
                && SUCCEEDED(SprMngr.GetDevice()->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back_buf)))
                        D3DXLoadSurfaceFromSurface(mirror,NULL,NULL,back_buf,NULL,NULL,D3DX_DEFAULT,0);
                SAFEREL(back_buf);
                SAFEREL(mirror);
        }
        else if(ScreenMirrorEndTick)
        {
                if(Timer::FastTick()>=ScreenMirrorEndTick)
                {
                        SAFEREL(ScreenMirrorTexture);
                        ScreenMirrorEndTick=0;
                }
                else
                {
                        MYVERTEX vb_[6];
                        vb_[0].x=0+ScreenMirrorX-0.5f;
                        vb_[0].y=MODE_HEIGHT+ScreenMirrorY-0.5f;
                        vb_[0].tu=0.0f;
                        vb_[0].tv=1.0f;
                        vb_[0].Diffuse=0x7F7F7F7F;

                        vb_[1].x=0+ScreenMirrorX-0.5f;
                        vb_[1].y=0+ScreenMirrorY-0.5f;
                        vb_[1].tu=0.0f;
                        vb_[1].tv=0.0f;
                        vb_[1].Diffuse=0x7F7F7F7F;
                        vb_[3].x=0+ScreenMirrorX-0.5f;
                        vb_[3].y=0+ScreenMirrorY-0.5f;
                        vb_[3].tu=0.0f;
                        vb_[3].tv=0.0f;
                        vb_[3].Diffuse=0x7F7F7F7F;

                        vb_[2].x=MODE_WIDTH+ScreenMirrorX-0.5f;
                        vb_[2].y=MODE_HEIGHT+ScreenMirrorY-0.5f;
                        vb_[2].tu=1.0f;
                        vb_[2].tv=1.0f;
                        vb_[2].Diffuse=0x7F7F7F7F;
                        vb_[5].x=MODE_WIDTH+ScreenMirrorX-0.5f;
                        vb_[5].y=MODE_HEIGHT+ScreenMirrorY-0.5f;
                        vb_[5].tu=1.0f;
                        vb_[5].tv=1.0f;
                        vb_[5].Diffuse=0x7F7F7F7F;

                        vb_[4].x=MODE_WIDTH+ScreenMirrorX-0.5f;
                        vb_[4].y=0+ScreenMirrorY-0.5f;
                        vb_[4].tu=1.0f;
                        vb_[4].tv=0.0f;
                        vb_[4].Diffuse=0x7F7F7F7F;

                        SprMngr.GetDevice()->SetTexture(0,ScreenMirrorTexture);
                        LPDIRECT3DVERTEXBUFFER vb;
                        SprMngr.GetDevice()->CreateVertexBuffer(6*sizeof(MYVERTEX),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFVF_MYVERTEX,D3DPOOL_DEFAULT,&vb);
                        void* vertices;
                        vb->Lock(0,6*sizeof(MYVERTEX),(uchar**)&vertices,D3DLOCK_DISCARD);
                        memcpy(vertices,vb_,6*sizeof(MYVERTEX));
                        vb->Unlock();
                        SprMngr.GetDevice()->SetStreamSource(0,vb,sizeof(MYVERTEX));
                        SprMngr.GetDevice()->SetVertexShader(D3DFVF_MYVERTEX);
                        SprMngr.GetDevice()->DrawPrimitive(PRIMITIVE_TRIANGLELIST,0,2);
                        SAFEREL(vb);
                        SprMngr.GetDevice()->SetStreamSource(0,SprMngr.GetVB(),sizeof(MYVERTEX));
                        SprMngr.GetDevice()->SetVertexShader(D3DFVF_MYVERTEX);
                }
        }
 */
}

void FOClient::ParseKeyboard()
{
    // Stop processing if window not active
    if( !( SDL_GetWindowFlags( MainWindow ) & SDL_WINDOW_INPUT_FOCUS ) )
    {
        MainWindowKeyboardEvents.clear();
        MainWindowKeyboardEventsText.clear();
        Keyb::Lost();
        Timer::StartAccelerator( ACCELERATE_NONE );
        if( Script::PrepareContext( ClientFunctions.InputLost, _FUNC_, "Game" ) )
            Script::RunPrepared();
        return;
    }

    // Accelerators
    if( !IsCurMode( CUR_WAIT ) )
    {
        if( Timer::ProcessAccelerator( ACCELERATE_PAGE_UP ) )
            ProcessMouseWheel( 1 );
        if( Timer::ProcessAccelerator( ACCELERATE_PAGE_DOWN ) )
            ProcessMouseWheel( -1 );
    }

    // Get buffered data
    if( MainWindowKeyboardEvents.empty() )
        return;
    IntVec events = MainWindowKeyboardEvents;
    StrVec events_text = MainWindowKeyboardEventsText;
    MainWindowKeyboardEvents.clear();
    MainWindowKeyboardEventsText.clear();

    // Process events
    for( uint i = 0; i < events.size(); i += 2 )
    {
        // Event data
        int         event = events[ i ];
        int         event_key = events[ i + 1 ];
        const char* event_text = events_text[ i / 2 ].c_str();

        // Keys codes mapping
        uchar dikdw = 0;
        uchar dikup = 0;
        if( event == SDL_KEYDOWN )
            dikdw = Keyb::MapKey( event_key );
        else if( event == SDL_KEYUP )
            dikup = Keyb::MapKey( event_key );
        if( !dikdw  && !dikup )
            continue;

        // Video
        if( IsVideoPlayed() )
        {
            if( IsCanStopVideo() && ( dikdw == DIK_ESCAPE || dikdw == DIK_SPACE || dikdw == DIK_RETURN || dikdw == DIK_NUMPADENTER ) )
            {
                NextVideo();
                return;
            }
            continue;
        }

        // Key script event
        if( dikdw && Script::PrepareContext( ClientFunctions.KeyDown, _FUNC_, "Game" ) )
        {
            ScriptString* event_text_script = NULL;
            if( dikdw == DIK_TEXT )
                event_text_script = ScriptString::Create( event_text );
            Script::SetArgUChar( dikdw );
            Script::SetArgObject( event_text_script );
            Script::RunPrepared();
            if( event_text_script )
                event_text_script->Release();
        }

        if( dikup && Script::PrepareContext( ClientFunctions.KeyUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUChar( dikup );
            Script::RunPrepared();
        }

        // Control keys
        if( dikdw == DIK_RCONTROL || dikdw == DIK_LCONTROL )
            Keyb::CtrlDwn = true;
        else if( dikdw == DIK_LMENU || dikdw == DIK_RMENU )
            Keyb::AltDwn = true;
        else if( dikdw == DIK_LSHIFT || dikdw == DIK_RSHIFT )
            Keyb::ShiftDwn = true;
        if( dikup == DIK_RCONTROL || dikup == DIK_LCONTROL )
            Keyb::CtrlDwn = false;
        else if( dikup == DIK_LMENU || dikup == DIK_RMENU )
            Keyb::AltDwn = false;
        else if( dikup == DIK_LSHIFT || dikup == DIK_RSHIFT )
            Keyb::ShiftDwn = false;
    }
}

void FOClient::ParseMouse()
{
    // Stop processing if window not active
    if( !( SDL_GetWindowFlags( MainWindow ) & SDL_WINDOW_INPUT_FOCUS ) )
    {
        MainWindowMouseEvents.clear();
        IfaceHold = IFACE_NONE;
        Timer::StartAccelerator( ACCELERATE_NONE );
        if( Script::PrepareContext( ClientFunctions.InputLost, _FUNC_, "Game" ) )
            Script::RunPrepared();
        return;
    }

    // Accelerators
    if( Timer::GetAcceleratorNum() != ACCELERATE_NONE && !IsCurMode( CUR_WAIT ) )
    {
        int iface_hold = IfaceHold;
        if( Timer::ProcessAccelerator( ACCELERATE_SPLIT_UP ) )
            SplitLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_SPLIT_DOWN ) )
            SplitLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_TIMER_UP ) )
            TimerLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_TIMER_DOWN ) )
            TimerLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_USE_SCRUP ) )
            UseLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_USE_SCRDOWN ) )
            UseLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_PUP_SCRUP1 ) )
            PupLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_PUP_SCRDOWN1 ) )
            PupLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_PUP_SCRUP2 ) )
            PupLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_PUP_SCRDOWN2 ) )
            PupLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT1SU ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT1SD ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT2SU ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT2SD ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT1OSU ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT1OSD ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT2OSU ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_BARTER_CONT2OSD ) )
            DlgLMouseUp( false );
        else if( Timer::ProcessAccelerator( ACCELERATE_PERK_SCRUP ) )
            PerkLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_PERK_SCRDOWN ) )
            PerkLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_DLG_TEXT_UP ) )
            DlgLMouseUp( true );
        else if( Timer::ProcessAccelerator( ACCELERATE_DLG_TEXT_DOWN ) )
            DlgLMouseUp( true );
        else if( Timer::ProcessAccelerator( ACCELERATE_SAVE_LOAD_SCR_UP ) )
            SaveLoadLMouseUp();
        else if( Timer::ProcessAccelerator( ACCELERATE_SAVE_LOAD_SCR_DN ) )
            SaveLoadLMouseUp();
        IfaceHold = iface_hold;
    }

    // Mouse move
    static int old_cur_x = GameOpt.MouseX;
    static int old_cur_y = GameOpt.MouseY;
    if( old_cur_x != GameOpt.MouseX || old_cur_y != GameOpt.MouseY )
    {
        // Script handler
        if( Script::PrepareContext( ClientFunctions.MouseMove, _FUNC_, "Game" ) )
            Script::RunPrepared();

        // Engine handlers
        LMenuMouseMove();

        // Store last position to avid redundant calls
        old_cur_x = GameOpt.MouseX;
        old_cur_y = GameOpt.MouseY;
    }

    // Get buffered data
    if( MainWindowMouseEvents.empty() )
        return;
    IntVec events = MainWindowMouseEvents;
    MainWindowMouseEvents.clear();

    // Process events
    for( uint i = 0; i < events.size(); i += 2 )
    {
        int event = events[ i ];
        int event_button = events[ i + 1 ];

        // Stop video
        if( IsVideoPlayed() )
        {
            if( IsCanStopVideo() && ( event == SDL_MOUSEBUTTONDOWN && ( event_button == MOUSE_BUTTON_LEFT || event_button == MOUSE_BUTTON_RIGHT ) ) )
            {
                NextVideo();
                return;
            }
            continue;
        }

        // Engine handlers
        if( event == SDL_MOUSEBUTTONUP )
            LMenuMouseUp();

        // Scripts
        if( event == SDL_MOUSEBUTTONDOWN && Script::PrepareContext( ClientFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( event_button );
            Script::RunPrepared();
        }
        else if( event == SDL_MOUSEBUTTONUP && Script::PrepareContext( ClientFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( event_button );
            Script::RunPrepared();
        }
    }
}

void ContainerWheelScroll( int items_count, int cont_height, int item_height, int& cont_scroll, int wheel_data )
{
    int height_items = cont_height / item_height;
    int scroll = 1;
    if( Keyb::ShiftDwn )
        scroll = height_items;
    if( wheel_data > 0 )
        scroll = -scroll;
    cont_scroll += scroll;
    if( cont_scroll < 0 )
        cont_scroll = 0;
    else if( cont_scroll > items_count - height_items )
        cont_scroll = MAX( 0, items_count - height_items );
}

void FOClient::ProcessMouseWheel( int data )
{
    int screen = GetActiveScreen();
    if( screen == SCREEN__SPLIT )
    {
        if( IsCurInRect( SplitWValue, SplitX, SplitY ) || IsCurInRect( SplitWItem, SplitX, SplitY ) )
        {
            if( data > 0 && SplitValue < SplitMaxValue )
                SplitValue++;
            else if( data < 0 && SplitValue > SplitMinValue )
                SplitValue--;
        }
    }
    else if( screen == SCREEN__TIMER )
    {
        if( IsCurInRect( TimerWValue, TimerX, TimerY ) || IsCurInRect( TimerWItem, TimerX, TimerY ) )
        {
            if( data > 0 && TimerValue < TIMER_MAX_VALUE )
                TimerValue++;
            else if( data < 0 && TimerValue > TIMER_MIN_VALUE )
                TimerValue--;
        }
    }
    else if( screen == SCREEN__USE )
    {
        if( IsCurInRect( UseWInv, UseX, UseY ) )
            ContainerWheelScroll( (int) UseCont.size(), UseWInv.H(), UseHeightItem, UseScroll, data );
    }
    else if( screen == SCREEN__PERK )
    {
        if( data > 0 && IsCurInRect( PerkWPerks, PerkX, PerkY ) && PerkScroll > 0 )
            PerkScroll--;
        else if( data < 0 && IsCurInRect( PerkWPerks, PerkX, PerkY ) && PerkScroll < (int) PerkCollection.size() - 1 )
            PerkScroll++;
    }
    else if( screen == SCREEN_NONE || screen == SCREEN__TOWN_VIEW )
    {
        if( IsMainScreen( SCREEN_GLOBAL_MAP ) )
        {
            GmapChangeZoom( (float) -data / 20.0f );
            if( IsCurInRect( GmapWTabs ) )
            {
                if( data > 0 )
                {
                    if( GmapTabNextX )
                        GmapTabsScrX -= 26;
                    if( GmapTabNextY )
                        GmapTabsScrY -= 26;
                    if( GmapTabsScrX < 0 )
                        GmapTabsScrX = 0;
                    if( GmapTabsScrY < 0 )
                        GmapTabsScrY = 0;
                }
                else
                {
                    if( GmapTabNextX )
                        GmapTabsScrX += 26;
                    if( GmapTabNextY )
                        GmapTabsScrY += 26;

                    int tabs_count = 0;
                    for( uint i = 0, j = (uint) GmapLoc.size(); i < j; i++ )
                    {
                        GmapLocation& loc = GmapLoc[ i ];
                        if( MsgLocations->Count( STR_LOC_LABEL_PIC( loc.LocPid ) ) && ResMngr.GetIfaceAnim( Str::GetHash( MsgLocations->GetStr( STR_LOC_LABEL_PIC( loc.LocPid ) ) ) ) )
                            tabs_count++;
                    }
                    if( GmapTabNextX && GmapTabsScrX > GmapWTab.W() * tabs_count )
                        GmapTabsScrX = GmapWTab.W() * tabs_count;
                    if( GmapTabNextY && GmapTabsScrY > GmapWTab.H() * tabs_count )
                        GmapTabsScrY = GmapWTab.H() * tabs_count;
                }
            }
        }
    }
    else if( screen == SCREEN__DIALOG )
    {
        if( IsCurInRect( DlgWText, DlgX, DlgY ) )
        {
            if( data > 0 && DlgMainTextCur > 0 )
                DlgMainTextCur--;
            else if( data < 0 && DlgMainTextCur < DlgMainTextLinesReal - DlgMainTextLinesRect )
                DlgMainTextCur++;
        }
        else if( IsCurInRect( DlgAnswText, DlgX, DlgY ) )
        {
            DlgCollectAnswers( data < 0 );
        }
    }
    else if( screen == SCREEN__BARTER )
    {
        if( IsCurInRect( BarterWCont1, DlgX, DlgY ) )
            ContainerWheelScroll( (int) BarterCont1.size(), BarterWCont1.H(), BarterCont1HeightItem, BarterScroll1, data );
        else if( IsCurInRect( BarterWCont2, DlgX, DlgY ) )
            ContainerWheelScroll( (int) BarterCont2.size(), BarterWCont2.H(), BarterCont2HeightItem, BarterScroll2, data );
        else if( IsCurInRect( BarterWCont1o, DlgX, DlgY ) )
            ContainerWheelScroll( (int) BarterCont1o.size(), BarterWCont1o.H(), BarterCont1oHeightItem, BarterScroll1o, data );
        else if( IsCurInRect( BarterWCont2o, DlgX, DlgY ) )
            ContainerWheelScroll( (int) BarterCont2o.size(), BarterWCont2o.H(), BarterCont2oHeightItem, BarterScroll2o, data );
        else if( IsCurInRect( DlgWText, DlgX, DlgY ) )
        {
            if( data > 0 && DlgMainTextCur > 0 )
                DlgMainTextCur--;
            else if( data < 0 && DlgMainTextCur < DlgMainTextLinesReal - DlgMainTextLinesRect )
                DlgMainTextCur++;
        }
    }
    else if( screen == SCREEN__PICKUP )
    {
        if( IsCurInRect( PupWCont1, PupX, PupY ) )
            ContainerWheelScroll( (int) PupCont1.size(), PupWCont1.H(), PupHeightItem1, PupScroll1, data );
        else if( IsCurInRect( PupWCont2, PupX, PupY ) )
            ContainerWheelScroll( (int) PupCont2.size(), PupWCont2.H(), PupHeightItem2, PupScroll2, data );
    }
    else if( screen == SCREEN__MINI_MAP )
    {
        if( IsCurInRect( LmapWMap, LmapX, LmapY ) )
        {
            if( data > 0 )
                LmapZoom++;
            else
                LmapZoom--;
            LmapZoom = CLAMP( LmapZoom, 2, 13 );
            LmapPrepareMap();
        }
    }
    else if( screen == SCREEN__PIP_BOY )
    {
        if( IsCurInRect( PipWMonitor, PipX, PipY ) )
        {
            if( PipMode != PIP__AUTOMAPS_MAP )
            {
                int scroll = 1;
                if( Keyb::ShiftDwn )
                    scroll = SprMngr.GetLinesCount( 0, PipWMonitor.H(), NULL, FONT_DEFAULT );
                if( data > 0 )
                    scroll = -scroll;
                PipScroll[ PipMode ] += scroll;
                if( PipScroll[ PipMode ] < 0 )
                    PipScroll[ PipMode ] = 0;
                #pragma MESSAGE("Check maximums in PipBoy scrolling.")
            }
            else
            {
                float scr_x = ( (float) ( PipWMonitor.CX() + PipX ) - AutomapScrX ) * AutomapZoom;
                float scr_y = ( (float) ( PipWMonitor.CY() + PipY ) - AutomapScrY ) * AutomapZoom;
                if( data > 0 )
                    AutomapZoom -= 0.1f;
                else
                    AutomapZoom += 0.1f;
                AutomapZoom = CLAMP( AutomapZoom, 0.1f, 10.0f );
                AutomapScrX = (float) ( PipWMonitor.CX() + PipX ) - scr_x / AutomapZoom;
                AutomapScrY = (float) ( PipWMonitor.CY() + PipY ) - scr_y / AutomapZoom;
            }
        }
    }
    else if( screen == SCREEN__SAVE_LOAD )
    {
        int ox = ( SaveLoadLoginScreen ? SaveLoadCX : SaveLoadX );
        int oy = ( SaveLoadLoginScreen ? SaveLoadCY : SaveLoadY );

        if( IsCurInRect( SaveLoadSlots, ox, oy ) )
        {
            if( data > 0 )
            {
                if( SaveLoadSlotScroll > 0 )
                    SaveLoadSlotScroll--;
            }
            else
            {
                int max = (int) SaveLoadDataSlots.size() - SaveLoadSlotsMax + ( SaveLoadSave ? 1 : 0 );
                if( SaveLoadSlotScroll < max )
                    SaveLoadSlotScroll++;
            }
        }
    }
}

bool FOClient::NetConnect( const char* host, ushort port )
{
    if( !Singleplayer )
        WriteLog( "Connecting to server<%s:%d>.\n", host, port );
    else
        WriteLog( "Connecting to server.\n" );

    IsConnected = false;

    #ifdef FO_WINDOWS
    WSADATA wsa;
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) )
    {
        WriteLog( "WSAStartup error<%s>.\n", GetLastSocketError() );
        return false;
    }
    #endif

    if( !Singleplayer )
    {
        if( !FillSockAddr( SockAddr, host, port ) )
            return false;
        if( GameOpt.ProxyType && !FillSockAddr( ProxyAddr, GameOpt.ProxyHost->c_str(), GameOpt.ProxyPort ) )
            return false;
    }
    else
    {
        #ifdef FO_WINDOWS
        for( int i = 0; i < 60; i++ )     // Wait 1 minute, then abort
        {
            if( !SingleplayerData.Refresh() )
                return false;
            if( SingleplayerData.NetPort )
                break;
            Thread::Sleep( 1000 );
        }
        if( !SingleplayerData.NetPort )
            return false;
        SockAddr.sin_family = AF_INET;
        SockAddr.sin_port = SingleplayerData.NetPort;
        SockAddr.sin_addr.s_addr = inet_addr( "127.0.0.1" );
        #endif
    }

    Bin.SetEncryptKey( 0 );
    Bout.SetEncryptKey( 0 );

    #ifdef FO_WINDOWS
    if( ( Sock = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0 ) ) == INVALID_SOCKET )
    #else
    if( ( Sock = socket( AF_INET, SOCK_STREAM, 0 ) ) == INVALID_SOCKET )
    #endif
    {
        WriteLog( "Create socket error<%s>.\n", GetLastSocketError() );
        return false;
    }

    // Nagle
    #ifdef FO_WINDOWS
    if( GameOpt.DisableTcpNagle )
    {
        int optval = 1;
        if( setsockopt( Sock, IPPROTO_TCP, TCP_NODELAY, (char*) &optval, sizeof( optval ) ) )
            WriteLog( "Can't set TCP_NODELAY (disable Nagle) to socket, error<%s>.\n", WSAGetLastError() );
    }
    #endif

    // Direct connect
    if( !GameOpt.ProxyType || Singleplayer )
    {
        if( connect( Sock, (sockaddr*) &SockAddr, sizeof( sockaddr_in ) ) )
        {
            WriteLog( "Can't connect to game server, error<%s>.\n", GetLastSocketError() );
            return false;
        }
    }
    // Proxy connect
    else
    {
        if( connect( Sock, (sockaddr*) &ProxyAddr, sizeof( sockaddr_in ) ) )
        {
            WriteLog( "Can't connect to proxy server, error<%s>.\n", GetLastSocketError() );
            return false;
        }

// ==========================================
        #define SEND_RECV                                      \
            do {                                               \
                if( !NetOutput() )                             \
                {                                              \
                    WriteLog( "Net output error.\n" );         \
                    return false;                              \
                }                                              \
                uint tick = Timer::FastTick();                 \
                while( true )                                  \
                {                                              \
                    int receive = NetInput( false );           \
                    if( receive > 0 )                          \
                        break;                                 \
                    if( receive < 0 )                          \
                    {                                          \
                        WriteLog( "Net input error.\n" );      \
                        return false;                          \
                    }                                          \
                    if( Timer::FastTick() - tick > 10000 )     \
                    {                                          \
                        WriteLog( "Proxy answer timeout.\n" ); \
                        return false;                          \
                    }                                          \
                    Thread::Sleep( 1 );                        \
                }                                              \
            }                                                  \
            while( 0 )
// ==========================================

        uchar b1, b2;
        Bin.Reset();
        Bout.Reset();
        // Authentication
        if( GameOpt.ProxyType == PROXY_SOCKS4 )
        {
            // Connect
            Bout << uchar( 4 );           // Socks version
            Bout << uchar( 1 );           // Connect command
            Bout << ushort( SockAddr.sin_port );
            Bout << uint( SockAddr.sin_addr.s_addr );
            Bout << uchar( 0 );
            SEND_RECV;
            Bin >> b1;             // Null byte
            Bin >> b2;             // Answer code
            if( b2 != 0x5A )
            {
                switch( b2 )
                {
                case 0x5B:
                    WriteLog( "Proxy connection error, request rejected or failed.\n" );
                    break;
                case 0x5C:
                    WriteLog( "Proxy connection error, request failed because client is not running identd (or not reachable from the server).\n" );
                    break;
                case 0x5D:
                    WriteLog( "Proxy connection error, request failed because client's identd could not confirm the user ID string in the request.\n" );
                    break;
                default:
                    WriteLog( "Proxy connection error, Unknown error<%u>.\n", b2 );
                    break;
                }
                return false;
            }
        }
        else if( GameOpt.ProxyType == PROXY_SOCKS5 )
        {
            Bout << uchar( 5 );                                                                // Socks version
            Bout << uchar( 1 );                                                                // Count methods
            Bout << uchar( 2 );                                                                // Method
            SEND_RECV;
            Bin >> b1;                                                                         // Socks version
            Bin >> b2;                                                                         // Method
            if( b2 == 2 )                                                                      // User/Password
            {
                Bout << uchar( 1 );                                                            // Subnegotiation version
                Bout << uchar( GameOpt.ProxyUser->length() );                                  // Name length
                Bout.Push( GameOpt.ProxyUser->c_str(), (uint) GameOpt.ProxyUser->length() );   // Name
                Bout << uchar( GameOpt.ProxyPass->length() );                                  // Pass length
                Bout.Push( GameOpt.ProxyPass->c_str(), (uint) GameOpt.ProxyPass->length() );   // Pass
                SEND_RECV;
                Bin >> b1;                                                                     // Subnegotiation version
                Bin >> b2;                                                                     // Status
                if( b2 != 0 )
                {
                    WriteLog( "Invalid proxy user or password.\n" );
                    return false;
                }
            }
            else if( b2 != 0 )         // Other authorization
            {
                WriteLog( "Socks server connect fail.\n" );
                return false;
            }
            // Connect
            Bout << uchar( 5 );           // Socks version
            Bout << uchar( 1 );           // Connect command
            Bout << uchar( 0 );           // Reserved
            Bout << uchar( 1 );           // IP v4 address
            Bout << uint( SockAddr.sin_addr.s_addr );
            Bout << ushort( SockAddr.sin_port );
            SEND_RECV;
            Bin >> b1;             // Socks version
            Bin >> b2;             // Answer code
            if( b2 != 0 )
            {
                switch( b2 )
                {
                case 1:
                    WriteLog( "Proxy connection error, SOCKS-server error.\n" );
                    break;
                case 2:
                    WriteLog( "Proxy connection error, connections fail by proxy rules.\n" );
                    break;
                case 3:
                    WriteLog( "Proxy connection error, network is not aviable.\n" );
                    break;
                case 4:
                    WriteLog( "Proxy connection error, host is not aviable.\n" );
                    break;
                case 5:
                    WriteLog( "Proxy connection error, connection denied.\n" );
                    break;
                case 6:
                    WriteLog( "Proxy connection error, TTL expired.\n" );
                    break;
                case 7:
                    WriteLog( "Proxy connection error, command not supported.\n" );
                    break;
                case 8:
                    WriteLog( "Proxy connection error, address type not supported.\n" );
                    break;
                default:
                    WriteLog( "Proxy connection error, unknown error<%u>.\n", b2 );
                    break;
                }
                return false;
            }
        }
        else if( GameOpt.ProxyType == PROXY_HTTP )
        {
            char* buf = (char*) Str::FormatBuf( "CONNECT %s:%d HTTP/1.0\r\n\r\n", inet_ntoa( SockAddr.sin_addr ), port );
            Bout.Push( buf, Str::Length( buf ) );
            SEND_RECV;
            buf = Bin.GetCurData();
            /*if(strstr(buf," 407 "))
               {
                    char* buf=(char*)Str::FormatBuf("CONNECT %s:%d HTTP/1.0\r\nProxy-authorization: basic \r\n\r\n",inet_ntoa(SockAddr.sin_addr),OptPort);
                    Bout.Push(buf,strlen(buf));
                    SEND_RECV;
                    buf=Bin.GetCurData();
               }*/
            if( !Str::Substring( buf, " 200 " ) )
            {
                WriteLog( "Proxy connection error, receive message<%s>.\n", buf );
                return false;
            }
        }
        else
        {
            WriteLog( "Unknown proxy type<%u>.\n", GameOpt.ProxyType );
            return false;
        }

        Bin.Reset();
        Bout.Reset();
    }

    ZStream.zalloc = zlib_alloc_;
    ZStream.zfree = zlib_free_;
    ZStream.opaque = NULL;
    if( inflateInit( &ZStream ) != Z_OK )
    {
        WriteLog( "ZStream InflateInit error.\n" );
        return false;
    }
    ZStreamOk = true;

    IsConnected = true;
    WriteLog( "Connection established.\n" );
    return true;
}

bool FOClient::FillSockAddr( sockaddr_in& saddr, const char* host, ushort port )
{
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons( port );
    if( ( saddr.sin_addr.s_addr = inet_addr( host ) ) == uint( -1 ) )
    {
        hostent* h = gethostbyname( host );
        if( !h )
        {
            WriteLogF( _FUNC_, " - Can't resolve remote host<%s>, error<%s>.", host, GetLastSocketError() );
            return false;
        }

        memcpy( &saddr.sin_addr, h->h_addr, sizeof( in_addr ) );
    }
    return true;
}

void FOClient::NetDisconnect()
{
    WriteLog( "Disconnect. Session traffic: send<%u>, receive<%u>, whole<%u>, receive real<%u>.\n",
              BytesSend, BytesReceive, BytesReceive + BytesSend, BytesRealReceive );

    if( ZStreamOk )
        inflateEnd( &ZStream );
    ZStreamOk = false;
    if( Sock != INVALID_SOCKET )
        closesocket( Sock );
    Sock = INVALID_SOCKET;
    IsConnected = false;

    SetCurMode( CUR_DEFAULT );
    HexMngr.UnloadMap();
    ClearCritters();
    QuestMngr.Finish();
    Bin.Reset();
    Bout.Reset();
    Bin.SetEncryptKey( 0 );
    Bout.SetEncryptKey( 0 );
}

void FOClient::ParseSocket()
{
    if( Sock == INVALID_SOCKET )
        return;

    if( NetInput( true ) < 0 )
    {
        NetDisconnect();
    }
    else
    {
        NetProcess();

        if( IsConnected && GameOpt.HelpInfo && Bout.IsEmpty() && !PingTick && GameOpt.PingPeriod && Timer::FastTick() >= PingCallTick )
        {
            Net_SendPing( PING_PING );
            PingTick = Timer::FastTick();
        }

        NetOutput();
    }
}

bool FOClient::NetOutput()
{
    if( !IsConnected )
        return false;
    if( Bout.IsEmpty() )
        return true;

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO( &SockSet );
    FD_ZERO( &SockSetErr );
    FD_SET( Sock, &SockSet );
    FD_SET( Sock, &SockSetErr );
    if( select( (int) Sock + 1, NULL, &SockSet, &SockSetErr, &tv ) == SOCKET_ERROR )
        WriteLogF( _FUNC_, " - Select error<%s>.\n", GetLastSocketError() );
    if( FD_ISSET( Sock, &SockSetErr ) )
        WriteLogF( _FUNC_, " - Socket error.\n" );
    if( !FD_ISSET( Sock, &SockSet ) )
        return true;

    int tosend = Bout.GetEndPos();
    int sendpos = 0;
    while( sendpos < tosend )
    {
        #ifdef FO_WINDOWS
        DWORD  len;
        WSABUF buf;
        buf.buf = Bout.GetData() + sendpos;
        buf.len = tosend - sendpos;
        if( WSASend( Sock, &buf, 1, &len, 0, NULL, NULL ) == SOCKET_ERROR || len == 0 )
        #else
        int len = (int) send( Sock, Bout.GetData() + sendpos, tosend - sendpos, 0 );
        if( len <= 0 )
        #endif
        {
            WriteLogF( _FUNC_, " - Socket error while send to server, error<%s>.\n", GetLastSocketError() );
            NetDisconnect();
            return false;
        }
        sendpos += len;
        BytesSend += len;
    }

    Bout.Reset();
    return true;
}

int FOClient::NetInput( bool unpack )
{
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO( &SockSet );
    FD_ZERO( &SockSetErr );
    FD_SET( Sock, &SockSet );
    FD_SET( Sock, &SockSetErr );
    if( select( (int) Sock + 1, &SockSet, NULL, &SockSetErr, &tv ) == SOCKET_ERROR )
        WriteLogF( _FUNC_, " - Select error<%s>.\n", GetLastSocketError() );
    if( FD_ISSET( Sock, &SockSetErr ) )
        WriteLogF( _FUNC_, " - Socket error.\n" );
    if( !FD_ISSET( Sock, &SockSet ) )
        return 0;

    #ifdef FO_WINDOWS
    DWORD  len;
    DWORD  flags = 0;
    WSABUF buf;
    buf.buf = ComBuf;
    buf.len = ComLen;
    if( WSARecv( Sock, &buf, 1, &len, &flags, NULL, NULL ) == SOCKET_ERROR )
    #else
    int len = (int) recv( Sock, ComBuf, ComLen, 0 );
    if( len < 0 )
    #endif
    {
        WriteLogF( _FUNC_, " - Socket error while receive from server, error<%s>.\n", GetLastSocketError() );
        return -1;
    }
    if( len == 0 )
    {
        WriteLogF( _FUNC_, " - Socket is closed.\n" );
        return -2;
    }

    uint pos = len;
    while( pos == ComLen )
    {
        uint  newcomlen = ( ComLen << 1 );
        char* combuf = new char[ newcomlen ];
        memcpy( combuf, ComBuf, ComLen );
        SAFEDELA( ComBuf );
        ComBuf = combuf;
        ComLen = newcomlen;

        #ifdef FO_WINDOWS
        flags = 0;
        buf.buf = ComBuf + pos;
        buf.len = ComLen - pos;
        if( WSARecv( Sock, &buf, 1, &len, &flags, NULL, NULL ) == SOCKET_ERROR )
        #else
        int len = (int) recv( Sock, ComBuf + pos, ComLen - pos, 0 );
        if( len < 0 )
        #endif
        {
            WriteLogF( _FUNC_, " - Socket error (2) while receive from server, error<%s>.\n", GetLastSocketError() );
            return -1;
        }
        if( len == 0 )
        {
            WriteLogF( _FUNC_, " - Socket is closed (2).\n" );
            return -2;
        }

        pos += len;
    }

    Bin.Refresh();
    uint old_pos = Bin.GetEndPos();     // Fix position

    if( unpack && !GameOpt.DisableZlibCompression )
    {
        ZStream.next_in = (uchar*) ComBuf;
        ZStream.avail_in = pos;
        ZStream.next_out = (uchar*) Bin.GetData() + Bin.GetEndPos();
        ZStream.avail_out = Bin.GetLen() - Bin.GetEndPos();

        if( inflate( &ZStream, Z_SYNC_FLUSH ) != Z_OK )
        {
            WriteLogF( _FUNC_, " - ZStream Inflate error.\n" );
            return -3;
        }

        Bin.SetEndPos( (uint) ( (size_t) ZStream.next_out - (size_t) Bin.GetData() ) );

        while( ZStream.avail_in )
        {
            Bin.GrowBuf( 2048 );

            ZStream.next_out = (uchar*) Bin.GetData() + Bin.GetEndPos();
            ZStream.avail_out = Bin.GetLen() - Bin.GetEndPos();

            if( inflate( &ZStream, Z_SYNC_FLUSH ) != Z_OK )
            {
                WriteLogF( _FUNC_, " - ZStream Inflate continue error.\n" );
                return -4;
            }

            Bin.SetEndPos( (uint) ( (size_t) ZStream.next_out - (size_t) Bin.GetData() ) );
        }
    }
    else
    {
        Bin.Push( ComBuf, pos, true );
    }

    BytesReceive += pos;
    BytesRealReceive += Bin.GetEndPos() - old_pos;
    return Bin.GetEndPos() - old_pos;
}

void FOClient::NetProcess()
{
    while( IsConnected && Bin.NeedProcess() )
    {
        uint msg = 0;
        Bin >> msg;

        CHECK_IN_BUFF_ERROR;

        if( GameOpt.DebugNet )
        {
            static uint count = 0;
            AddMess( FOMB_GAME, Str::FormatBuf( "%04u) Input net message<%u>.", count, ( msg >> 8 ) & 0xFF ) );
            WriteLog( "%04u) Input net message<%u>.\n", count, ( msg >> 8 ) & 0xFF );
            count++;
        }

        switch( msg )
        {
        case NETMSG_WRONG_NET_PROTO:
            Net_OnWrongNetProto();
            break;
        case NETMSG_LOGIN_SUCCESS:
            Net_OnLoginSuccess();
            break;
        case NETMSG_REGISTER_SUCCESS:
            if( !Singleplayer )
            {
                WriteLog( "Registration success.\n" );
                *GameOpt.Name = *GameOpt.RegName;
                Password = GameOpt.RegPassword->c_std_str();
                Crypt.SetCache( "__name", (uchar*) GameOpt.Name->c_str(), (uint) GameOpt.Name->length() + 1 );
                Crypt.SetCache( "__pass", (uchar*) Password.c_str(), (uint) Password.length() + 1 );
                *GameOpt.RegName = "";
                *GameOpt.RegPassword = "";
            }
            else
            {
                WriteLog( "World loaded, enter to it.\n" );
            }
            NetDisconnect();
            ConnectToGame();
            InitNetReason = INIT_NET_REASON_LOGIN2;
            break;

        case NETMSG_PING:
            Net_OnPing();
            break;
        case NETMSG_END_PARSE_TO_GAME:
            Net_OnEndParseToGame();
            break;
        case NETMSG_CHECK_UID0:
            Net_OnCheckUID0();
            break;

        case NETMSG_ADD_PLAYER:
            Net_OnAddCritter( false );
            break;
        case NETMSG_ADD_NPC:
            Net_OnAddCritter( true );
            break;
        case NETMSG_REMOVE_CRITTER:
            Net_OnRemoveCritter();
            break;
        case NETMSG_SOME_ITEM:
            Net_OnSomeItem();
            break;
        case NETMSG_CRITTER_ACTION:
            Net_OnCritterAction();
            break;
        case NETMSG_CRITTER_KNOCKOUT:
            Net_OnCritterKnockout();
            break;
        case NETMSG_CRITTER_MOVE_ITEM:
            Net_OnCritterMoveItem();
            break;
        case NETMSG_CRITTER_ANIMATE:
            Net_OnCritterAnimate();
            break;
        case NETMSG_CRITTER_SET_ANIMS:
            Net_OnCritterSetAnims();
            break;
        case NETMSG_CUSTOM_COMMAND:
            Net_OnCustomCommand();
            break;
        case NETMSG_CRITTER_MOVE:
            Net_OnCritterMove();
            break;
        case NETMSG_CRITTER_DIR:
            Net_OnCritterDir();
            break;
        case NETMSG_CRITTER_XY:
            Net_OnCritterXY();
            break;
        case NETMSG_CHECK_UID1:
            Net_OnCheckUID1();
            break;

        case NETMSG_CRITTER_ITEM_POD_PROPERTY( 1 ):
            Net_OnItemProperty( true, 1 );
            break;
        case NETMSG_CRITTER_ITEM_POD_PROPERTY( 2 ):
            Net_OnItemProperty( true, 2 );
            break;
        case NETMSG_CRITTER_ITEM_POD_PROPERTY( 4 ):
            Net_OnItemProperty( true, 4 );
            break;
        case NETMSG_CRITTER_ITEM_POD_PROPERTY( 8 ):
            Net_OnItemProperty( true, 8 );
            break;
        case NETMSG_CRITTER_ITEM_COMPLEX_PROPERTY:
            Net_OnItemProperty( true, 0 );
            break;
        case NETMSG_MAP_ITEM_POD_PROPERTY( 1 ):
            Net_OnItemProperty( false, 1 );
            break;
        case NETMSG_MAP_ITEM_POD_PROPERTY( 2 ):
            Net_OnItemProperty( false, 2 );
            break;
        case NETMSG_MAP_ITEM_POD_PROPERTY( 4 ):
            Net_OnItemProperty( false, 4 );
            break;
        case NETMSG_MAP_ITEM_POD_PROPERTY( 8 ):
            Net_OnItemProperty( false, 8 );
            break;
        case NETMSG_MAP_ITEM_COMPLEX_PROPERTY:
            Net_OnItemProperty( false, 0 );
            break;

        case NETMSG_CRITTER_TEXT:
            Net_OnText();
            break;
        case NETMSG_MSG:
            Net_OnTextMsg( false );
            break;
        case NETMSG_MSG_LEX:
            Net_OnTextMsg( true );
            break;
        case NETMSG_MAP_TEXT:
            Net_OnMapText();
            break;
        case NETMSG_MAP_TEXT_MSG:
            Net_OnMapTextMsg();
            break;
        case NETMSG_MAP_TEXT_MSG_LEX:
            Net_OnMapTextMsgLex();
            break;
        case NETMSG_CHECK_UID2:
            Net_OnCheckUID2();
            break;

        case NETMSG_ALL_PROPERTIES:
            Net_OnAllProperties();
            break;
        case NETMSG_CRITTER_POD_PROPERTY( 1 ):
            Net_OnCritterProperty( 1 );
            break;
        case NETMSG_CRITTER_POD_PROPERTY( 2 ):
            Net_OnCritterProperty( 2 );
            break;
        case NETMSG_CRITTER_POD_PROPERTY( 4 ):
            Net_OnCritterProperty( 4 );
            break;
        case NETMSG_CRITTER_POD_PROPERTY( 8 ):
            Net_OnCritterProperty( 8 );
            break;
        case NETMSG_CRITTER_COMPLEX_PROPERTY:
            Net_OnCritterProperty( 0 );
            break;

        case NETMSG_CRAFT_ASK:
            Net_OnCraftAsk();
            break;
        case NETMSG_CRAFT_RESULT:
            Net_OnCraftResult();
            break;
        case NETMSG_CLEAR_ITEMS:
            Net_OnChosenClearItems();
            break;
        case NETMSG_ADD_ITEM:
            Net_OnChosenAddItem();
            break;
        case NETMSG_REMOVE_ITEM:
            Net_OnChosenEraseItem();
            break;
        case NETMSG_ALL_ITEMS_SEND:
            Net_OnAllItemsSend();
            break;
        case NETMSG_CHECK_UID3:
            Net_OnCheckUID3();
            break;

        case NETMSG_TALK_NPC:
            Net_OnChosenTalk();
            break;

        case NETMSG_GAME_INFO:
            Net_OnGameInfo();
            break;

        case NETMSG_QUEST:
            Net_OnQuest( false );
            break;
        case NETMSG_QUESTS:
            Net_OnQuest( true );
            break;
        case NETMSG_HOLO_INFO:
            Net_OnHoloInfo();
            break;
        case NETMSG_SCORES:
            Net_OnScores();
            break;
        case NETMSG_USER_HOLO_STR:
            Net_OnUserHoloStr();
            break;
        case NETMSG_AUTOMAPS_INFO:
            Net_OnAutomapsInfo();
            break;
        case NETMSG_CHECK_UID4:
            Net_OnCheckUID4();
            break;
        case NETMSG_VIEW_MAP:
            Net_OnViewMap();
            break;

        case NETMSG_LOADMAP:
            Net_OnLoadMap();
            break;
        case NETMSG_MAP:
            Net_OnMap();
            break;
        case NETMSG_GLOBAL_INFO:
            Net_OnGlobalInfo();
            break;
        case NETMSG_GLOBAL_ENTRANCES:
            Net_OnGlobalEntrances();
            break;
        case NETMSG_CONTAINER_INFO:
            Net_OnContainerInfo();
            break;
        case NETMSG_FOLLOW:
            Net_OnFollow();
            break;
        case NETMSG_PLAYERS_BARTER:
            Net_OnPlayersBarter();
            break;
        case NETMSG_PLAYERS_BARTER_SET_HIDE:
            Net_OnPlayersBarterSetHide();
            break;
        case NETMSG_SHOW_SCREEN:
            Net_OnShowScreen();
            break;
        case NETMSG_RUN_CLIENT_SCRIPT:
            Net_OnRunClientScript();
            break;
        case NETMSG_DROP_TIMERS:
            Net_OnDropTimers();
            break;

        case NETMSG_ADD_ITEM_ON_MAP:
            Net_OnAddItemOnMap();
            break;
        case NETMSG_ERASE_ITEM_FROM_MAP:
            Net_OnEraseItemFromMap();
            break;
        case NETMSG_ANIMATE_ITEM:
            Net_OnAnimateItem();
            break;
        case NETMSG_COMBAT_RESULTS:
            Net_OnCombatResult();
            break;
        case NETMSG_EFFECT:
            Net_OnEffect();
            break;
        case NETMSG_FLY_EFFECT:
            Net_OnFlyEffect();
            break;
        case NETMSG_PLAY_SOUND:
            Net_OnPlaySound( false );
            break;
        case NETMSG_PLAY_SOUND_TYPE:
            Net_OnPlaySound( true );
            break;

        case NETMSG_UPDATE_FILES_LIST:
            Net_OnUpdateFilesList();
            break;
        case NETMSG_UPDATE_FILE_DATA:
            Net_OnUpdateFileData();
            break;

        default:
            Bin.SkipMsg( msg );
            break;
        }
    }

    if( Bin.IsError() )
    {
        if( GameOpt.DebugNet )
            AddMess( FOMB_GAME, "Invalid network message. Disconnect." );
        WriteLog( "Invalid network message. Disconnect.\n" );
        NetDisconnect();
    }
}

void FOClient::Net_SendUpdate()
{
    // Header
    Bout << NETMSG_UPDATE;

    // Protocol version
    Bout << (ushort) FONLINE_VERSION;

    // Data encrypting
    uint encrypt_key = *UID0;
    Bout << encrypt_key;
    Bout.SetEncryptKey( encrypt_key + 521 );
    Bin.SetEncryptKey( encrypt_key + 3491 );
}

void FOClient::Net_SendLogIn( const char* name, const char* pass )
{
    char name_[ UTF8_BUF_SIZE( MAX_NAME ) ] = { 0 };
    char pass_[ UTF8_BUF_SIZE( MAX_NAME ) ] = { 0 };
    if( name )
        Str::Copy( name_, name );
    if( pass )
        Str::Copy( pass_, pass );

    uint uid1 = *UID1;
    Bout << NETMSG_LOGIN;
    Bout << (ushort) FONLINE_VERSION;
    uint uid4 = *UID4;
    Bout << uid4;
    uid4 = uid1;                                                                                                                                                        // UID4

    // Begin data encrypting
    Bout.SetEncryptKey( *UID4 + 12345 );
    Bin.SetEncryptKey( *UID4 + 12345 );

    Bout.Push( name_, sizeof( name_ ) );
    Bout << uid1;
    uid4 ^= uid1 * Random( 0, 432157 ) + *UID3;                                                                                                 // UID1
    char pass_hash[ PASS_HASH_SIZE ];
    Crypt.ClientPassHash( name_, pass_, pass_hash );
    Bout.Push( pass_hash, PASS_HASH_SIZE );
    uint uid2 = *UID2;
    Bout << CurLang.Name;
    for( int i = 0; i < TEXTMSG_COUNT; i++ )
        Bout << (uint) i;
    Bout << UIDXOR;                                                                                                                                                             // UID xor
    uint uid3 = *UID3;
    Bout << uid3;
    uid3 ^= uid1;
    uid1 |= uid3 / Random( 2, 53 );                                                                                                             // UID3
    Bout << uid2;
    uid3 ^= uid2;
    uid1 |= uid4 + 222 - *UID2;                                                                                                                 // UID2
    Bout << UIDOR;                                                                                                                              // UID or
    for( int i = 0; i < ITEM_MAX_TYPES; i++ )
        Bout << (uint) i + 111;
    Bout << UIDCALC;                                                                                                                            // UID uidcalc
    Bout << GameOpt.DefaultCombatMode;
    uint uid0 = *UID0;
    Bout << uid0;
    uid3 ^= uid1 + Random( 0, 53245 );
    uid1 |= uid3 + *UID0;
    uid3 ^= uid1 + uid3;
    uid1 |= uid3;                                                                                       // UID0

    char dummy[ 100 ];
    Bout.Push( dummy, 100 );

    if( !Singleplayer && name )
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_CONN_SUCCESS ) );
}

void FOClient::Net_SendCreatePlayer()
{
    WriteLog( "Player registration..." );

    IntVec reg_props;
    for( size_t i = 0; i < RegProps.size() / 2; i++ )
    {
        if( CritterCl::RegProperties.count( RegProps[ i * 2 ] ) )
        {
            reg_props.push_back( RegProps[ i * 2 ] );
            reg_props.push_back( RegProps[ i * 2 + 1 ] );
        }
    }

    ushort count = (ushort) ( reg_props.size() / 2 );
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( ushort ) + UTF8_BUF_SIZE( MAX_NAME ) + PASS_HASH_SIZE + sizeof( count ) + ( sizeof( int ) + sizeof( int ) ) * count;

    Bout << NETMSG_CREATE_CLIENT;
    Bout << msg_len;

    Bout << (ushort) FONLINE_VERSION;

    // Begin data encrypting
    Bout.SetEncryptKey( 1234567890 );
    Bin.SetEncryptKey( 1234567890 );

    char buf[ UTF8_BUF_SIZE( MAX_NAME ) ];
    memzero( buf, sizeof( buf ) );
    Str::Copy( buf, GameOpt.RegName->c_str() );
    Bout.Push( buf, UTF8_BUF_SIZE( MAX_NAME ) );
    char pass_hash[ PASS_HASH_SIZE ];
    Crypt.ClientPassHash( GameOpt.RegName->c_str(), GameOpt.RegPassword->c_str(), pass_hash );
    Bout.Push( pass_hash, PASS_HASH_SIZE );

    Bout << count;
    for( size_t i = 0; i < reg_props.size(); i += 2 )
    {
        Bout << reg_props[ i ];
        Bout << reg_props[ i + 1 ];
    }

    WriteLog( "complete.\n" );
}

void FOClient::Net_SendSaveLoad( bool save, const char* fname, UCharVec* pic_data )
{
    if( save )
        WriteLog( "Request save to file<%s>...", fname );
    else
        WriteLog( "Request load from file<%s>...", fname );

    uint   msg = NETMSG_SINGLEPLAYER_SAVE_LOAD;
    ushort fname_len = Str::Length( fname );
    uint   msg_len = sizeof( msg ) + sizeof( save ) + sizeof( fname_len ) + fname_len;
    if( save )
        msg_len += sizeof( uint ) + (uint) pic_data->size();

    Bout << msg;
    Bout << msg_len;
    Bout << save;
    Bout << fname_len;
    Bout.Push( fname, fname_len );
    if( save )
    {
        Bout << (uint) pic_data->size();
        if( pic_data->size() )
            Bout.Push( (char*) &( *pic_data )[ 0 ], (uint) pic_data->size() );
    }

    WriteLog( "complete.\n" );
}

void FOClient::Net_SendText( const char* send_str, uchar how_say )
{
    if( !send_str || !send_str[ 0 ] )
        return;

    char  str_buf[ MAX_FOTEXT ];
    char* str = str_buf;
    Str::Copy( str_buf, send_str );

    bool result = false;
    if( Script::PrepareContext( ClientFunctions.OutMessage, _FUNC_, "Game" ) )
    {
        int           say_type = how_say;
        ScriptString* sstr = ScriptString::Create( str );
        Script::SetArgObject( sstr );
        Script::SetArgAddress( &say_type );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
        Str::Copy( str, MAX_FOTEXT, sstr->c_str() );
        sstr->Release();
        how_say = say_type;
    }

    if( !result || !str[ 0 ] )
        return;

    ushort len = Str::Length( str );
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( how_say ) + sizeof( len ) + len;

    Bout << NETMSG_SEND_TEXT;
    Bout << msg_len;
    Bout << how_say;
    Bout << len;
    Bout.Push( str, len );
}

void FOClient::Net_SendDir()
{
    if( !Chosen )
        return;

    Bout << NETMSG_DIR;
    Bout << (uchar) Chosen->GetDir();
}

void FOClient::Net_SendMove( UCharVec steps )
{
    if( !Chosen )
        return;

    uint move_params = 0;
    for( uint i = 0; i < MOVE_PARAM_STEP_COUNT; i++ )
    {
        if( i + 1 >= steps.size() )
            break;                           // Send next steps

        SETFLAG( move_params, steps[ i + 1 ] << ( i * MOVE_PARAM_STEP_BITS ) );
        SETFLAG( move_params, MOVE_PARAM_STEP_ALLOW << ( i * MOVE_PARAM_STEP_BITS ) );
    }

    if( Chosen->IsRunning )
        SETFLAG( move_params, MOVE_PARAM_RUN );
    if( steps.empty() )
        SETFLAG( move_params, MOVE_PARAM_STEP_DISALLOW );                // Inform about stopping

    Bout << ( Chosen->IsRunning ? NETMSG_SEND_MOVE_RUN : NETMSG_SEND_MOVE_WALK );
    Bout << move_params;
    Bout << Chosen->HexX;
    Bout << Chosen->HexY;
}

void FOClient::Net_SendUseSkill( int skill, CritterCl* cr )
{
    Bout << NETMSG_SEND_USE_SKILL;
    Bout << skill;
    Bout << (uchar) TARGET_CRITTER;
    Bout << cr->GetId();
    Bout << (hash) 0;
}

void FOClient::Net_SendUseSkill( int skill, ItemHex* item )
{
    Bout << NETMSG_SEND_USE_SKILL;
    Bout << skill;

    if( item->IsScenOrGrid() )
    {
        uint hex = ( item->GetHexX() << 16 ) | item->GetHexY();

        Bout << (uchar) TARGET_SCENERY;
        Bout << hex;
        Bout << item->GetProtoId();
    }
    else
    {
        Bout << (uchar) TARGET_ITEM;
        Bout << item->GetId();
        Bout << (hash) 0;
    }
}

void FOClient::Net_SendUseSkill( int skill, Item* item )
{
    Bout << NETMSG_SEND_USE_SKILL;
    Bout << skill;
    Bout << (uchar) TARGET_SELF_ITEM;
    Bout << item->GetId();
    Bout << (hash) 0;
}

void FOClient::Net_SendUseItem( uchar ap, uint item_id, hash item_pid, uchar rate, uchar target_type, uint target_id, hash target_pid, uint param )
{
    Bout << NETMSG_SEND_USE_ITEM;
    Bout << ap;
    Bout << item_id;
    Bout << item_pid;
    Bout << rate;
    Bout << target_type;
    Bout << target_id;
    Bout << target_pid;
    Bout << param;
}

void FOClient::Net_SendPickItem( ushort targ_x, ushort targ_y, hash pid )
{
    Bout << NETMSG_SEND_PICK_ITEM;
    Bout << targ_x;
    Bout << targ_y;
    Bout << pid;
}

void FOClient::Net_SendPickCritter( uint crid, uchar pick_type )
{
    Bout << NETMSG_SEND_PICK_CRITTER;
    Bout << crid;
    Bout << pick_type;
}

void FOClient::Net_SendChosenProperty( Property* prop )
{
    uint  data_size;
    void* data = prop->GetRawData( Chosen, data_size );

    if( prop->IsPOD() )
    {
        Bout << NETMSG_SEND_CRITTER_POD_PROPERTY( data_size );
        Bout << (ushort) prop->GetRegIndex();
        Bout.Push( (char*) data, data_size );
    }
    else
    {
        uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( ushort ) + data_size;

        Bout << NETMSG_SEND_CRITTER_COMPLEX_PROPERTY;
        Bout << msg_len;
        Bout << (ushort) prop->GetRegIndex();
        if( data_size )
            Bout.Push( (char*) data, data_size );
    }
}

void FOClient::Net_SendItemProperty( Item* item, Property* prop )
{
    uint  data_size;
    void* data = prop->GetRawData( item, data_size );

    if( prop->IsPOD() )
    {
        Bout << NETMSG_SEND_ITEM_POD_PROPERTY( data_size );
        Bout << item->GetId();
        Bout << (ushort) prop->GetRegIndex();
        Bout.Push( (char*) data, data_size );
    }
    else
    {
        uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( ushort ) + data_size;

        Bout << NETMSG_SEND_ITEM_COMPLEX_PROPERTY;
        Bout << msg_len;
        Bout << item->GetId();
        Bout << (ushort) prop->GetRegIndex();
        if( data_size )
            Bout.Push( (char*) data, data_size );
    }
}

void FOClient::Net_SendChangeItem( uchar ap, uint item_id, uchar from_slot, uchar to_slot, uint count )
{
    Bout << NETMSG_SEND_CHANGE_ITEM;
    Bout << ap;
    Bout << item_id;
    Bout << from_slot;
    Bout << to_slot;
    Bout << count;

    CollectContItems();
}

void FOClient::Net_SendItemCont( uchar transfer_type, uint cont_id, uint item_id, uint count, uchar take_flags )
{
    Bout << NETMSG_SEND_ITEM_CONT;
    Bout << transfer_type;
    Bout << cont_id;
    Bout << item_id;
    Bout << count;
    Bout << take_flags;

    CollectContItems();
}

void FOClient::Net_SendTalk( uchar is_npc, uint id_to_talk, uchar answer )
{
    Bout << NETMSG_SEND_TALK_NPC;
    Bout << is_npc;
    Bout << id_to_talk;
    Bout << answer;
}

void FOClient::Net_SendSayNpc( uchar is_npc, uint id_to_talk, const char* str )
{
    Bout << NETMSG_SEND_SAY_NPC;
    Bout << is_npc;
    Bout << id_to_talk;
    Bout.Push( str, MAX_SAY_NPC_TEXT );
}

void FOClient::Net_SendBarter( uint npc_id, ItemVec& cont_sale, ItemVec& cont_buy )
{
    ushort sale_count = (ushort) cont_sale.size();
    ushort buy_count = (ushort) cont_buy.size();
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( sale_count ) + sizeof( buy_count ) +
                     ( sizeof( uint ) + sizeof( uint ) ) * sale_count + ( sizeof( uint ) + sizeof( uint ) ) * buy_count;

    Bout << NETMSG_SEND_BARTER;
    Bout << msg_len;
    Bout << npc_id;

    Bout << sale_count;
    for( int i = 0; i < sale_count; ++i )
    {
        Bout << cont_sale[ i ]->GetId();
        Bout << cont_sale[ i ]->GetCount();
    }

    Bout << buy_count;
    for( int i = 0; i < buy_count; ++i )
    {
        Bout << cont_buy[ i ]->GetId();
        Bout << cont_buy[ i ]->GetCount();
    }
}

void FOClient::Net_SendGetGameInfo()
{
    Bout << NETMSG_SEND_GET_INFO;
}

void FOClient::Net_SendGiveMap( bool automap, hash map_pid, uint loc_id, uint tiles_hash, uint walls_hash, uint scen_hash )
{
    Bout << NETMSG_SEND_GIVE_MAP;
    Bout << automap;
    Bout << map_pid;
    Bout << loc_id;
    Bout << tiles_hash;
    Bout << walls_hash;
    Bout << scen_hash;
}

void FOClient::Net_SendLoadMapOk()
{
    Bout << NETMSG_SEND_LOAD_MAP_OK;
}

void FOClient::Net_SendRuleGlobal( uchar command, uint param1, uint param2 )
{
    Bout << NETMSG_SEND_RULE_GLOBAL;
    Bout << command;
    Bout << param1;
    Bout << param2;

    WaitPing();
}

void FOClient::Net_SendLevelUp( ushort perk_up, IntVec* props_data )
{
    if( !Chosen )
        return;

    ushort count = (ushort) ( props_data ? props_data->size() / 2 : 0 );
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( count ) + sizeof( int ) * 2 * count + sizeof( perk_up );

    Bout << NETMSG_SEND_LEVELUP;
    Bout << msg_len;

    // Skills
    Bout << count;
    for( size_t i = 0, j = ( props_data ? props_data->size() / 2 : 0 ); i < j; i++ )
    {
        Bout << props_data->at( i * 2 );
        Bout << props_data->at( i * 2 + 1 );
    }

    // Perks
    Bout << perk_up;
}

void FOClient::Net_SendCraftAsk( UIntVec numbers )
{
    ushort count = (ushort) numbers.size();
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( count ) + sizeof( uint ) * count;
    Bout << NETMSG_CRAFT_ASK;
    Bout << msg_len;
    Bout << count;
    for( int i = 0; i < count; i++ )
        Bout << numbers[ i ];
    FixNextShowCraftTick = Timer::FastTick() + CRAFT_SEND_TIME;
}

void FOClient::Net_SendCraft( uint craft_num )
{
    Bout << NETMSG_SEND_CRAFT;
    Bout << craft_num;
}

void FOClient::Net_SendPing( uchar ping )
{
    Bout << NETMSG_PING;
    Bout << ping;
}

void FOClient::Net_SendPlayersBarter( uchar barter, uint param, uint param_ext )
{
    Bout << NETMSG_PLAYERS_BARTER;
    Bout << barter;
    Bout << param;
    Bout << param_ext;
}

void FOClient::Net_SendScreenAnswer( uint answer_i, const char* answer_s )
{
    char answer_s_buf[ MAX_SAY_NPC_TEXT + 1 ];
    memzero( answer_s_buf, sizeof( answer_s_buf ) );
    Str::Copy( answer_s_buf, MAX_SAY_NPC_TEXT + 1, answer_s );

    Bout << NETMSG_SEND_SCREEN_ANSWER;
    Bout << answer_i;
    Bout.Push( answer_s_buf, MAX_SAY_NPC_TEXT );
}

void FOClient::Net_SendGetScores()
{
    Bout << NETMSG_SEND_GET_SCORES;
}

void FOClient::Net_SendSetUserHoloStr( Item* holodisk, const char* title, const char* text )
{
    if( !holodisk || !title || !text )
    {
        WriteLogF( _FUNC_, " - Null pointers arguments.\n" );
        return;
    }

    ushort title_len = ( ushort ) Str::Length( title );
    ushort text_len = ( ushort ) Str::Length( text );
    if( !title_len || !text_len || title_len > USER_HOLO_MAX_TITLE_LEN || text_len > USER_HOLO_MAX_LEN )
    {
        WriteLogF( _FUNC_, " - Length of texts is greather of maximum or zero, title cur<%u>, title max<%u>, text cur<%u>, text max<%u>.\n", title_len, USER_HOLO_MAX_TITLE_LEN, text_len, USER_HOLO_MAX_LEN );
        return;
    }

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( title_len ) + sizeof( text_len ) + title_len + text_len;
    Bout << NETMSG_SEND_SET_USER_HOLO_STR;
    Bout << msg_len;
    Bout << holodisk->GetId();
    Bout << title_len;
    Bout << text_len;
    Bout.Push( title, title_len );
    Bout.Push( text, text_len );
}

void FOClient::Net_SendGetUserHoloStr( uint str_num )
{
    static UIntSet already_send;
    if( already_send.count( str_num ) )
        return;
    already_send.insert( str_num );

    Bout << NETMSG_SEND_GET_USER_HOLO_STR;
    Bout << str_num;
}

void FOClient::Net_SendCombat( uchar type, int val )
{
    Bout << NETMSG_SEND_COMBAT;
    Bout << type;
    Bout << val;
}

void FOClient::Net_SendRunScript( bool unsafe, const char* func_name, int p0, int p1, int p2, const char* p3, UIntVec& p4 )
{
    ushort func_name_len = ( ushort ) Str::Length( func_name );
    ushort p3len = ( p3 ? ( ushort ) Str::Length( p3 ) : 0 );
    ushort p4size = (ushort) p4.size();
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( unsafe ) + sizeof( func_name_len ) + func_name_len +
                     sizeof( p0 ) + sizeof( p1 ) + sizeof( p2 ) + sizeof( p3len ) + p3len + sizeof( p4size ) + p4size * sizeof( uint );

    Bout << NETMSG_SEND_RUN_SERVER_SCRIPT;
    Bout << msg_len;
    Bout << unsafe;
    Bout << func_name_len;
    Bout.Push( func_name, func_name_len );
    Bout << p0;
    Bout << p1;
    Bout << p2;
    Bout << p3len;
    if( p3len )
        Bout.Push( p3, p3len );
    Bout << p4size;
    if( p4size )
        Bout.Push( (char*) &p4[ 0 ], p4size * sizeof( uint ) );
}

void FOClient::Net_SendKarmaVoting( uint crid, bool val_up )
{
    Bout << NETMSG_SEND_KARMA_VOTING;
    Bout << crid;
    Bout << val_up;
}

void FOClient::Net_SendRefereshMe()
{
    Bout << NETMSG_SEND_REFRESH_ME;

    WaitPing();
}

void FOClient::Net_OnWrongNetProto()
{
    if( UpdateFilesInProgress )
        UpdateFilesAbort( STR_CLIENT_OUTDATED, "CLIENT_OUTDATED" );
    else
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_CLIENT_OUTDATED ) );
}

void FOClient::Net_OnLoginSuccess()
{
    WriteLog( "Authentication success.\n" );

    if( !Singleplayer )
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_LOGINOK ) );

    // Set encrypt keys
    uint bin_seed, bout_seed;     // Server bin/bout == client bout/bin
    Bin >> bin_seed;
    Bin >> bout_seed;

    CHECK_IN_BUFF_ERROR;

    Bout.SetEncryptKey( bin_seed );
    Bin.SetEncryptKey( bout_seed );
}

void FOClient::Net_OnAddCritter( bool is_npc )
{
    uint msg_len;
    Bin >> msg_len;

    uint crid;
    uint base_type;
    Bin >> crid;
    Bin >> base_type;

    ushort hx, hy;
    uchar  dir;
    Bin >> hx;
    Bin >> hy;
    Bin >> dir;

    uchar cond;
    uint  anim1life, anim1ko, anim1dead;
    uint  anim2life, anim2ko, anim2dead;
    uint  flags;
    short multihex;
    Bin >> cond;
    Bin >> anim1life;
    Bin >> anim1ko;
    Bin >> anim1dead;
    Bin >> anim2life;
    Bin >> anim2ko;
    Bin >> anim2dead;
    Bin >> flags;
    Bin >> multihex;

    // Npc
    hash npc_pid;
    if( is_npc )
        Bin >> npc_pid;

    // Player
    char cl_name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    if( !is_npc )
    {
        Bin.Pop( cl_name, sizeof( cl_name ) );
        cl_name[ sizeof( cl_name ) - 1 ] = 0;
    }

    // Parameters
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    if( !CritType::IsEnabled( base_type ) )
        base_type = DEFAULT_CRTYPE;

    if( !crid )
        WriteLogF( _FUNC_, " - CritterCl id is zero.\n" );
    else if( HexMngr.IsMapLoaded() && ( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() || dir >= DIRS_COUNT ) )
        WriteLogF( _FUNC_, " - Invalid positions hx<%u>, hy<%u>, dir<%u>.\n", hx, hy, dir );
    else
    {
        CritterCl* cr = new CritterCl();
        cr->Id = crid;
        cr->HexX = hx;
        cr->HexY = hy;
        cr->CrDir = dir;
        cr->Cond = cond;
        cr->Anim1Life = anim1life;
        cr->Anim1Knockout = anim1ko;
        cr->Anim1Dead = anim1dead;
        cr->Anim2Life = anim2life;
        cr->Anim2Knockout = anim2ko;
        cr->Anim2Dead = anim2dead;
        cr->Flags = flags;
        cr->Multihex = multihex;
        cr->Props.RestoreData( TempPropertiesData );

        if( is_npc )
        {
            cr->Pid = npc_pid;
            *cr->Name = MsgDlg->GetStr( STR_NPC_NAME( cr->GetDialogId() ) );
            if( MsgDlg->Count( STR_NPC_AVATAR( cr->GetDialogId() ) ) )
                *cr->Avatar = MsgDlg->GetStr( STR_NPC_AVATAR( cr->GetDialogId() ) );
        }
        else
        {
            *cr->Name = cl_name;
        }

        cr->SetBaseType( base_type );
        cr->Init();

        if( FLAG( cr->Flags, FCRIT_CHOSEN ) )
            SetAction( CHOSEN_NONE );

        AddCritter( cr );

        const char* look = FmtCritLook( cr, CRITTER_ONLY_NAME );
        if( look )
            *cr->Name = look;

        if( Script::PrepareContext( ClientFunctions.CritterIn, _FUNC_, "Game" ) )
        {
            Script::SetArgObject( cr );
            Script::RunPrepared();
        }
    }
}

void FOClient::Net_OnRemoveCritter()
{
    uint remove_crid;
    Bin >> remove_crid;

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = GetCritter( remove_crid );
    if( !cr )
        return;

    cr->Finish();

    if( Script::PrepareContext( ClientFunctions.CritterOut, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( cr );
        Script::RunPrepared();
    }
}

void FOClient::Net_OnText()
{
    uint   msg_len;
    uint   crid;
    uchar  how_say;
    ushort intellect;
    bool   unsafe_text;
    ushort len;
    char   str[ MAX_FOTEXT + 1 ];

    Bin >> msg_len;
    Bin >> crid;
    Bin >> how_say;
    Bin >> intellect;
    Bin >> unsafe_text;

    Bin >> len;
    Bin.Pop( str, MIN( len, ushort( MAX_FOTEXT ) ) );
    if( len > MAX_FOTEXT )
        Bin.Pop( Str::GetBigBuf(), len - MAX_FOTEXT );
    str[ MIN( len, ushort( MAX_FOTEXT ) ) ] = 0;

    CHECK_IN_BUFF_ERROR;

    if( how_say == SAY_FLASH_WINDOW )
    {
        FlashGameWindow();
        return;
    }

    if( unsafe_text )
        Keyb::EraseInvalidChars( str, KIF_NO_SPEC_SYMBOLS );

    OnText( str, crid, how_say, intellect );
}

void FOClient::Net_OnTextMsg( bool with_lexems )
{
    uint   msg_len;
    uint   crid;
    uchar  how_say;
    ushort msg_num;
    uint   num_str;

    if( with_lexems )
        Bin >> msg_len;
    Bin >> crid;
    Bin >> how_say;
    Bin >> msg_num;
    Bin >> num_str;

    char lexems[ MAX_DLG_LEXEMS_TEXT + 1 ] = { 0 };
    if( with_lexems )
    {
        ushort lexems_len;
        Bin >> lexems_len;
        if( lexems_len && lexems_len <= MAX_DLG_LEXEMS_TEXT )
        {
            Bin.Pop( lexems, lexems_len );
            lexems[ lexems_len ] = '\0';
        }
    }

    CHECK_IN_BUFF_ERROR;

    if( how_say == SAY_FLASH_WINDOW )
    {
        FlashGameWindow();
        return;
    }

    if( msg_num >= TEXTMSG_COUNT )
    {
        WriteLogF( _FUNC_, " - Msg num invalid value<%u>.\n", msg_num );
        return;
    }

    FOMsg& msg = CurLang.Msg[ msg_num ];
    if( msg_num == TEXTMSG_HOLO )
    {
        char str[ MAX_FOTEXT ];
        Str::Copy( str, GetHoloText( num_str ) );
        OnText( str, crid, how_say, 10 );
    }
    else if( msg.Count( num_str ) )
    {
        char str[ MAX_FOTEXT ];
        Str::Copy( str, msg.GetStr( num_str ) );
        FormatTags( str, MAX_FOTEXT, Chosen, GetCritter( crid ), lexems );
        OnText( str, crid, how_say, 10 );
    }
}

void FOClient::OnText( const char* str, uint crid, int how_say, ushort intellect )
{
    char fstr[ MAX_FOTEXT ];
    Str::Copy( fstr, str );
    uint len = Str::Length( str );
    if( !len )
        return;

    uint text_delay = GameOpt.TextDelay + len * 100;
    if( Script::PrepareContext( ClientFunctions.InMessage, _FUNC_, "Game" ) )
    {
        ScriptString* sstr = ScriptString::Create( fstr );
        Script::SetArgObject( sstr );
        Script::SetArgAddress( &how_say );
        Script::SetArgAddress( &crid );
        Script::SetArgAddress( &text_delay );
        Script::RunPrepared();
        Str::Copy( fstr, sstr->c_str() );
        sstr->Release();

        if( !Script::GetReturnedBool() )
            return;

        len = Str::Length( fstr );
        if( !len )
            return;
    }

    // Intellect format
    if( how_say >= SAY_NORM && how_say <= SAY_RADIO )
        FmtTextIntellect( fstr, intellect );

    // Type stream
    uint fstr_cr = 0;
    uint fstr_mb = 0;
    int  mess_type = FOMB_TALK;

    switch( how_say )
    {
    case SAY_NORM:
        fstr_mb = STR_MBNORM;
    case SAY_NORM_ON_HEAD:
        fstr_cr = STR_CRNORM;
        break;
    case SAY_SHOUT:
        fstr_mb = STR_MBSHOUT;
    case SAY_SHOUT_ON_HEAD:
        fstr_cr = STR_CRSHOUT;
        Str::UpperUTF8( fstr );
        break;
    case SAY_EMOTE:
        fstr_mb = STR_MBEMOTE;
    case SAY_EMOTE_ON_HEAD:
        fstr_cr = STR_CREMOTE;
        break;
    case SAY_WHISP:
        fstr_mb = STR_MBWHISP;
    case SAY_WHISP_ON_HEAD:
        fstr_cr = STR_CRWHISP;
        Str::LowerUTF8( fstr );
        break;
    case SAY_SOCIAL:
        fstr_cr = STR_CRSOCIAL;
        fstr_mb = STR_MBSOCIAL;
        break;
    case SAY_RADIO:
        fstr_mb = STR_MBRADIO;
        break;
    case SAY_NETMSG:
        mess_type = FOMB_GAME;
        fstr_mb = STR_MBNET;
        break;
    default:
        break;
    }

    CritterCl* cr = NULL;
    if( how_say != SAY_RADIO )
        cr = GetCritter( crid );

    string crit_name = ( cr ? cr->GetName() : "?" );

    // CritterCl on head text
    if( fstr_cr && cr )
        cr->SetText( FmtGameText( fstr_cr, fstr ), COLOR_TEXT, text_delay );

    // Message box text
    if( fstr_mb )
    {
        if( how_say == SAY_NETMSG )
        {
            AddMess( mess_type, FmtGameText( fstr_mb, fstr ) );
        }
        else if( how_say == SAY_RADIO )
        {
            ushort channel = 0;
            if( Chosen )
            {
                Item* radio = Chosen->GetItem( crid );
                if( radio )
                    channel = radio->GetRadioChannel();
            }
            AddMess( mess_type, FmtGameText( fstr_mb, channel, fstr ) );
        }
        else
        {
            AddMess( mess_type, FmtGameText( fstr_mb, crit_name.c_str(), fstr ) );
        }

        if( IsScreenPlayersBarter() && Chosen && ( crid == BarterOpponentId || crid == Chosen->GetId() ) )
        {
            if( how_say == SAY_NETMSG || how_say == SAY_RADIO )
                BarterText += FmtGameText( fstr_mb, fstr );
            else
                BarterText += FmtGameText( fstr_mb, crit_name.c_str(), fstr );
            BarterText += "\n";
            BarterText += Str::FormatBuf( "|%u ", COLOR_TEXT );
            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
        }
    }

    // Dialog text
    bool is_dialog = IsScreenPresent( SCREEN__DIALOG );
    bool is_barter = IsScreenPresent( SCREEN__BARTER );
    if( ( how_say == SAY_DIALOG || how_say == SAY_APPEND ) && ( is_dialog || is_barter ) )
    {
        CritterCl* npc = GetCritter( DlgNpcId );
        FormatTags( fstr, MAX_FOTEXT, Chosen, npc, "" );

        if( is_dialog )
        {
            if( how_say == SAY_APPEND )
                DlgMainText += fstr;
            else
                DlgMainText = fstr;
            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, DlgMainText.c_str() );
        }
        else if( is_barter )
        {
            if( how_say == SAY_APPEND )
                BarterText += fstr;
            else
                BarterText = fstr;
            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
        }
        if( how_say != SAY_APPEND )
            DlgMainTextCur = 0;
    }

    // Encounter question
    if( how_say >= SAY_ENCOUNTER_ANY && how_say <= SAY_ENCOUNTER_TB && IsMainScreen( SCREEN_GLOBAL_MAP ) )
    {
        ShowScreen( SCREEN__DIALOGBOX );
        if( how_say == SAY_ENCOUNTER_ANY )
        {
            DlgboxType = DIALOGBOX_ENCOUNTER_ANY;
            DlgboxButtonText[ 0 ] = MsgGame->GetStr( STR_DIALOGBOX_ENCOUNTER_RT );
            DlgboxButtonText[ 1 ] = MsgGame->GetStr( STR_DIALOGBOX_ENCOUNTER_TB );
            DlgboxButtonText[ 2 ] = MsgGame->GetStr( STR_DIALOGBOX_CANCEL );
            DlgboxButtonsCount = 3;
        }
        else if( how_say == SAY_ENCOUNTER_RT )
        {
            DlgboxType = DIALOGBOX_ENCOUNTER_RT;
            DlgboxButtonText[ 0 ] = MsgGame->GetStr( STR_DIALOGBOX_ENCOUNTER_RT );
            DlgboxButtonText[ 1 ] = MsgGame->GetStr( STR_DIALOGBOX_CANCEL );
            DlgboxButtonsCount = 2;
        }
        else
        {
            DlgboxType = DIALOGBOX_ENCOUNTER_TB;
            DlgboxButtonText[ 0 ] = MsgGame->GetStr( STR_DIALOGBOX_ENCOUNTER_TB );
            DlgboxButtonText[ 1 ] = MsgGame->GetStr( STR_DIALOGBOX_CANCEL );
            DlgboxButtonsCount = 2;
        }
        DlgboxWait = Timer::GameTick() + GM_ANSWER_WAIT_TIME;
        Str::Copy( DlgboxText, fstr );
    }

    // FixBoy result
    if( how_say == SAY_FIX_RESULT )
        FixResultStr = fstr;

    // Dialogbox
    if( how_say == SAY_DIALOGBOX_TEXT )
        Str::Copy( DlgboxText, fstr );
    else if( how_say >= SAY_DIALOGBOX_BUTTON( 0 ) && how_say < SAY_DIALOGBOX_BUTTON( MAX_DLGBOX_BUTTONS ) )
        DlgboxButtonText[ how_say - SAY_DIALOGBOX_BUTTON( 0 ) ] = fstr;

    // Say box
    if( how_say == SAY_SAY_TITLE )
        SayTitle = fstr;
    else if( how_say == SAY_SAY_TEXT )
        SayText = fstr;

    FlashGameWindow();
}

void FOClient::OnMapText( const char* str, ushort hx, ushort hy, uint color, ushort intellect )
{
    uint          len = Str::Length( str );
    uint          text_delay = GameOpt.TextDelay + len * 100;
    ScriptString* sstr = ScriptString::Create( str );
    if( Script::PrepareContext( ClientFunctions.MapMessage, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( sstr );
        Script::SetArgAddress( &hx );
        Script::SetArgAddress( &hy );
        Script::SetArgAddress( &color );
        Script::SetArgAddress( &text_delay );
        Script::RunPrepared();
        if( !Script::GetReturnedBool() )
        {
            sstr->Release();
            return;
        }
    }

    char fstr[ MAX_FOTEXT ];
    Str::Copy( fstr, sstr->c_str() );
    sstr->Release();

    FmtTextIntellect( fstr, intellect );

    MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = ( color ? color : COLOR_TEXT );
    t.Fade = false;
    t.StartTick = Timer::GameTick();
    t.Tick = text_delay;
    t.Text = fstr;
    t.Pos = HexMngr.GetRectForText( hx, hy );
    t.EndPos = t.Pos;
    auto it = std::find( GameMapTexts.begin(), GameMapTexts.end(), t );
    if( it != GameMapTexts.end() )
        GameMapTexts.erase( it );
    GameMapTexts.push_back( t );

    FlashGameWindow();
}

void FOClient::Net_OnMapText()
{
    uint   msg_len;
    ushort hx, hy;
    uint   color;
    ushort len;
    char   str[ MAX_FOTEXT + 1 ];
    ushort intellect;
    bool   unsafe_text;

    Bin >> msg_len;
    Bin >> hx;
    Bin >> hy;
    Bin >> color;

    Bin >> len;
    Bin.Pop( str, MIN( len, ushort( MAX_FOTEXT ) ) );
    if( len > MAX_FOTEXT )
        Bin.Pop( Str::GetBigBuf(), len - MAX_FOTEXT );
    str[ MIN( len, ushort( MAX_FOTEXT ) ) ] = 0;

    Bin >> intellect;
    Bin >> unsafe_text;

    CHECK_IN_BUFF_ERROR;

    if( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
    {
        WriteLogF( _FUNC_, " - Invalid coords, hx<%u>, hy<%u>, text<%s>.\n", hx, hy, str );
        return;
    }

    if( unsafe_text )
        Keyb::EraseInvalidChars( str, KIF_NO_SPEC_SYMBOLS );

    OnMapText( str, hx, hy, color, intellect );
}

void FOClient::Net_OnMapTextMsg()
{
    ushort hx, hy;
    uint   color;
    ushort msg_num;
    uint   num_str;

    Bin >> hx;
    Bin >> hy;
    Bin >> color;
    Bin >> msg_num;
    Bin >> num_str;

    CHECK_IN_BUFF_ERROR;

    if( msg_num >= TEXTMSG_COUNT )
    {
        WriteLogF( _FUNC_, " - Msg num invalid value, num<%u>.\n", msg_num );
        return;
    }

    static char str[ MAX_FOTEXT ];
    Str::Copy( str, CurLang.Msg[ msg_num ].GetStr( num_str ) );
    FormatTags( str, MAX_FOTEXT, Chosen, NULL, "" );

    OnMapText( str, hx, hy, color, 0 );
}

void FOClient::Net_OnMapTextMsgLex()
{
    uint   msg_len;
    ushort hx, hy;
    uint   color;
    ushort msg_num;
    uint   num_str;
    ushort lexems_len;

    Bin >> msg_len;
    Bin >> hx;
    Bin >> hy;
    Bin >> color;
    Bin >> msg_num;
    Bin >> num_str;
    Bin >> lexems_len;

    char lexems[ MAX_DLG_LEXEMS_TEXT + 1 ] = { 0 };
    if( lexems_len && lexems_len <= MAX_DLG_LEXEMS_TEXT )
    {
        Bin.Pop( lexems, lexems_len );
        lexems[ lexems_len ] = '\0';
    }

    CHECK_IN_BUFF_ERROR;

    if( msg_num >= TEXTMSG_COUNT )
    {
        WriteLogF( _FUNC_, " - Msg num invalid value, num<%u>.\n", msg_num );
        return;
    }

    char str[ MAX_FOTEXT ];
    Str::Copy( str, CurLang.Msg[ msg_num ].GetStr( num_str ) );
    FormatTags( str, MAX_FOTEXT, Chosen, NULL, lexems );

    OnMapText( str, hx, hy, color, 0 );
}

void FOClient::Net_OnCritterDir()
{
    uint  crid;
    uchar dir;
    Bin >> crid;
    Bin >> dir;

    CHECK_IN_BUFF_ERROR;

    if( dir >= DIRS_COUNT )
    {
        WriteLogF( _FUNC_, " - Invalid dir<%u>.\n", dir );
        dir = 0;
    }

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;
    cr->SetDir( dir );
}

void FOClient::Net_OnCritterMove()
{
    uint   crid;
    uint   move_params;
    ushort new_hx;
    ushort new_hy;
    Bin >> crid;
    Bin >> move_params;
    Bin >> new_hx;
    Bin >> new_hy;

    CHECK_IN_BUFF_ERROR;

    if( new_hx >= HexMngr.GetMaxHexX() || new_hy >= HexMngr.GetMaxHexY() )
        return;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    cr->IsRunning = FLAG( move_params, MOVE_PARAM_RUN );

    if( cr != Chosen )
    {
        cr->MoveSteps.resize( cr->CurMoveStep > 0 ? cr->CurMoveStep : 0 );
        if( cr->CurMoveStep >= 0 )
            cr->MoveSteps.push_back( PAIR( new_hx, new_hy ) );
        for( int i = 0, j = cr->CurMoveStep + 1; i < MOVE_PARAM_STEP_COUNT; i++, j++ )
        {
            int  step = ( move_params >> ( i * MOVE_PARAM_STEP_BITS ) ) & ( MOVE_PARAM_STEP_DIR | MOVE_PARAM_STEP_ALLOW | MOVE_PARAM_STEP_DISALLOW );
            int  dir = step & MOVE_PARAM_STEP_DIR;
            bool disallow = ( !FLAG( step, MOVE_PARAM_STEP_ALLOW ) || FLAG( step, MOVE_PARAM_STEP_DISALLOW ) );
            if( disallow )
            {
                if( j <= 0 && ( cr->GetHexX() != new_hx || cr->GetHexY() != new_hy ) )
                {
                    HexMngr.TransitCritter( cr, new_hx, new_hy, true, true );
                    cr->CurMoveStep = -1;
                }
                break;
            }
            MoveHexByDir( new_hx, new_hy, dir, HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
            if( j < 0 )
                continue;
            cr->MoveSteps.push_back( PAIR( new_hx, new_hy ) );
        }
        cr->CurMoveStep++;

        /*if(HexMngr.IsShowTrack())
           {
                HexMngr.ClearHexTrack();
                for(int i=4;i<cr->MoveDirs.size();i++)
                {
                        UShortPair& step=cr->MoveDirs[i];
                        HexMngr.HexTrack[step.second][step.first]=1;
                }
                HexMngr.HexTrack[last_hy][last_hx]=2;
                HexMngr.RefreshMap();
           }*/
    }
    else
    {
        if( IsAction( ACTION_MOVE ) )
            EraseFrontAction();
        MoveDirs.clear();
        HexMngr.TransitCritter( cr, new_hx, new_hy, true, true );
    }
}

void FOClient::Net_OnSomeItem()
{
    uint  msg_len;
    uint  item_id;
    hash  item_pid;
    uchar slot;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> item_pid;
    Bin >> slot;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    SAFEREL( SomeItem );

    ProtoItem* proto_item = ItemMngr.GetProtoItem( item_pid );
    if( !proto_item )
    {
        WriteLogF( _FUNC_, " - Proto item<%u> not found.\n", item_pid );
        return;
    }

    SomeItem = new Item( item_id, proto_item );
    SomeItem->AccCritter.Slot = slot;
    SomeItem->Props.RestoreData( TempPropertiesData );
}

void FOClient::Net_OnCritterAction()
{
    uint crid;
    int  action;
    int  action_ext;
    bool is_item;
    Bin >> crid;
    Bin >> action;
    Bin >> action_ext;
    Bin >> is_item;

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    cr->Action( action, action_ext, is_item ? SomeItem : NULL, false );
}

void FOClient::Net_OnCritterKnockout()
{
    uint   crid;
    uint   anim2begin;
    uint   anim2idle;
    ushort knock_hx;
    ushort knock_hy;
    Bin >> crid;
    Bin >> anim2begin;
    Bin >> anim2idle;
    Bin >> knock_hx;
    Bin >> knock_hy;

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    cr->Action( ACTION_KNOCKOUT, anim2begin, NULL, false );
    cr->Anim2Knockout = anim2idle;

    if( cr->GetHexX() != knock_hx || cr->GetHexY() != knock_hy )
    {
        // TODO: offsets
        HexMngr.TransitCritter( cr, knock_hx, knock_hy, false, true );
    }
}

void FOClient::Net_OnCritterMoveItem()
{
    uint  msg_len;
    uint  crid;
    uchar action;
    uchar prev_slot;
    bool  is_item;
    Bin >> msg_len;
    Bin >> crid;
    Bin >> action;
    Bin >> prev_slot;
    Bin >> is_item;

    // Slot items
    UCharVec              slots_data_slot;
    UIntVec               slots_data_id;
    HashVec               slots_data_pid;
    vector< UCharVecVec > slots_data_data;
    ushort                slots_data_count;
    Bin >> slots_data_count;
    for( uchar i = 0; i < slots_data_count; i++ )
    {
        uchar slot;
        uint  id;
        hash  pid;
        Bin >> slot;
        Bin >> id;
        Bin >> pid;
        NET_READ_PROPERTIES( Bin, TempPropertiesData );

        slots_data_slot.push_back( slot );
        slots_data_id.push_back( id );
        slots_data_pid.push_back( pid );
        slots_data_data.push_back( TempPropertiesData );
    }

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    if( cr != Chosen )
    {
        int64 prev_hash_sum = 0;
        for( auto it = cr->InvItems.begin(), end = cr->InvItems.end(); it != end; ++it )
        {
            Item* item = *it;
            prev_hash_sum += item->LightGetHash();
        }

        cr->EraseAllItems();
        cr->DefItemSlotHand->SetProto( ItemMngr.GetProtoItem( ITEM_DEF_SLOT ) );
        cr->DefItemSlotArmor->SetProto( ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) );

        for( uchar i = 0; i < slots_data_count; i++ )
        {
            ProtoItem* proto_item = ItemMngr.GetProtoItem( slots_data_pid[ i ] );
            if( proto_item )
            {
                Item* item = new Item( slots_data_id[ i ], proto_item );
                item->Props.RestoreData( slots_data_data[ i ] );
                item->AccCritter.Slot = slots_data_slot[ i ];
                cr->AddItem( item );
            }
        }

        int64 hash_sum = 0;
        for( auto it = cr->InvItems.begin(), end = cr->InvItems.end(); it != end; ++it )
        {
            Item* item = *it;
            hash_sum += item->LightGetHash();
        }
        if( hash_sum != prev_hash_sum )
            HexMngr.RebuildLight();
    }

    cr->Action( action, prev_slot, is_item ? SomeItem : NULL, false );
}

void FOClient::Net_OnCritterAnimate()
{
    uint crid;
    uint anim1;
    uint anim2;
    bool is_item;
    bool clear_sequence;
    bool delay_play;
    Bin >> crid;
    Bin >> anim1;
    Bin >> anim2;
    Bin >> is_item;
    Bin >> clear_sequence;
    Bin >> delay_play;

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    if( clear_sequence )
        cr->ClearAnim();
    if( delay_play || !cr->IsAnim() )
        cr->Animate( anim1, anim2, is_item ? SomeItem : NULL );
}

void FOClient::Net_OnCritterSetAnims()
{
    uint crid;
    int  cond;
    uint anim1;
    uint anim2;
    Bin >> crid;
    Bin >> cond;
    Bin >> anim1;
    Bin >> anim2;

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = GetCritter( crid );
    if( cr )
    {
        if( cond == 0 || cond == COND_LIFE )
        {
            cr->Anim1Life = anim1;
            cr->Anim2Life = anim2;
        }
        else if( cond == 0 || cond == COND_KNOCKOUT )
        {
            cr->Anim1Knockout = anim1;
            cr->Anim2Knockout = anim2;
        }
        else if( cond == 0 || cond == COND_DEAD )
        {
            cr->Anim1Dead = anim1;
            cr->Anim2Dead = anim2;
        }
        if( !cr->IsAnim() )
            cr->AnimateStay();
    }
}

void FOClient::Net_OnCheckUID0()
{
    #define CHECKUIDBIN                       \
        uint uid[ 5 ];                        \
        uint  uidxor[ 5 ];                    \
        uchar rnd_count, rnd_count2;          \
        uchar dummy;                          \
        uint  msg_len;                        \
        Bin >> msg_len;                       \
        Bin >> uid[ 3 ];                      \
        Bin >> uidxor[ 0 ];                   \
        Bin >> rnd_count;                     \
        Bin >> uid[ 1 ];                      \
        Bin >> uidxor[ 2 ];                   \
        for( int i = 0; i < rnd_count; i++ )  \
            Bin >> dummy;                     \
        Bin >> uid[ 2 ];                      \
        Bin >> uidxor[ 1 ];                   \
        Bin >> uid[ 4 ];                      \
        Bin >> rnd_count2;                    \
        Bin >> uidxor[ 3 ];                   \
        Bin >> uidxor[ 4 ];                   \
        Bin >> uid[ 0 ];                      \
        for( int i = 0; i < 5; i++ )          \
            uid[ i ] ^= uidxor[ i ];          \
        for( int i = 0; i < rnd_count2; i++ ) \
            Bin >> dummy;                     \
        CHECK_IN_BUFF_ERROR

        CHECKUIDBIN;
    if( CHECK_UID0( uid ) )
        UIDFail = true;
}

void FOClient::Net_OnCustomCommand()
{
    uint   crid;
    ushort index;
    int    value;
    Bin >> crid;
    Bin >> index;
    Bin >> value;

    CHECK_IN_BUFF_ERROR;

    if( GameOpt.DebugNet )
        AddMess( FOMB_GAME, Str::FormatBuf( " - crid<%u> index<%u> value<%d>.", crid, index, value ) );

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    if( cr->IsChosen() )
    {
        if( GameOpt.DebugNet )
            AddMess( FOMB_GAME, Str::FormatBuf( " - index<%u> value<%d>.", index, value ) );

        switch( index )
        {
        case OTHER_BREAK_TIME:
        {
            if( value < 0 )
                value = 0;
            Chosen->TickStart( value );
            SetAction( CHOSEN_NONE );
            MoveDirs.clear();
            Chosen->MoveSteps.clear();
        }
        break;
        case OTHER_FLAGS:
        {
            Chosen->Flags = value;
        }
        break;
        case OTHER_BASE_TYPE:
        {
            if( Chosen->Multihex < 0 && CritType::GetMultihex( Chosen->GetCrType() ) != CritType::GetMultihex( value ) )
            {
                HexMngr.SetMultihex( Chosen->GetHexX(), Chosen->GetHexY(), CritType::GetMultihex( Chosen->GetCrType() ), false );
                HexMngr.SetMultihex( Chosen->GetHexX(), Chosen->GetHexY(), CritType::GetMultihex( value ), true );
            }

            Chosen->SetBaseType( value );
            if( !Chosen->IsAnim() )
                Chosen->Action( ACTION_REFRESH, 0, NULL, false );
        }
        break;
        case OTHER_MULTIHEX:
        {
            int old_mh = Chosen->GetMultihex();
            Chosen->Multihex = value;
            if( old_mh != (int) Chosen->GetMultihex() )
            {
                HexMngr.SetMultihex( Chosen->GetHexX(), Chosen->GetHexY(), old_mh, false );
                HexMngr.SetMultihex( Chosen->GetHexX(), Chosen->GetHexY(), Chosen->GetMultihex(), true );
            }
        }
        break;
        case OTHER_YOU_TURN:
        {
            if( value < 0 )
                value = 0;
            ChosenAction.clear();
            TurnBasedTime = Timer::GameTick() + value;
            TurnBasedCurCritterId = Chosen->GetId();
            HexMngr.SetCritterContour( 0, 0 );
            FlashGameWindow();
        }
        break;
        case OTHER_CLEAR_MAP:
        {
            CritMap crits = HexMngr.GetCritters();
            for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
            {
                CritterCl* cr = ( *it ).second;
                if( cr != Chosen )
                    EraseCritter( cr->GetId() );
            }
            ItemHexVec items = HexMngr.GetItems();
            for( auto it = items.begin(), end = items.end(); it != end; ++it )
            {
                ItemHex* item = *it;
                if( item->IsItem() )
                    HexMngr.DeleteItem( *it, true );
            }
        }
        break;
        case OTHER_TELEPORT:
        {
            ushort hx = ( value >> 16 ) & 0xFFFF;
            ushort hy = value & 0xFFFF;
            if( hx < HexMngr.GetMaxHexX() && hy < HexMngr.GetMaxHexY() )
            {
                CritterCl* cr = HexMngr.GetField( hx, hy ).Crit;
                if( Chosen == cr )
                    break;
                if( !Chosen->IsDead() && cr )
                    EraseCritter( cr->GetId() );
                HexMngr.RemoveCrit( Chosen );
                Chosen->HexX = hx;
                Chosen->HexY = hy;
                HexMngr.SetCrit( Chosen );
                HexMngr.ScrollToHex( Chosen->GetHexX(), Chosen->GetHexY(), 0.1, true );
            }
        }
        break;
        default:
            break;
        }

        // Maybe changed some parameter influencing on look borders
        RebuildLookBorders = true;
    }
    else
    {
        if( index == OTHER_FLAGS )
        {
            cr->Flags = value;
        }
        else if( index == OTHER_BASE_TYPE )
        {
            if( cr->Multihex < 0 && CritType::GetMultihex( cr->GetCrType() ) != CritType::GetMultihex( value ) )
            {
                HexMngr.SetMultihex( cr->GetHexX(), cr->GetHexY(), CritType::GetMultihex( cr->GetCrType() ), false );
                HexMngr.SetMultihex( cr->GetHexX(), cr->GetHexY(), CritType::GetMultihex( value ), true );
            }

            cr->SetBaseType( value );
            if( !cr->IsAnim() )
                cr->Action( ACTION_REFRESH, 0, NULL, false );
        }
        else if( index == OTHER_MULTIHEX )
        {
            int old_mh = cr->GetMultihex();
            cr->Multihex = value;
            if( old_mh != (int) cr->GetMultihex() )
            {
                HexMngr.SetMultihex( cr->GetHexX(), cr->GetHexY(), old_mh, false );
                HexMngr.SetMultihex( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), true );
            }
        }
        else if( index == OTHER_YOU_TURN )
        {
            TurnBasedTime = Timer::GameTick() + value;
            TurnBasedCurCritterId = cr->GetId();
            HexMngr.SetCritterContour( cr->GetId(), CONTOUR_CUSTOM );
        }
    }
}

void FOClient::Net_OnCheckUID1()
{
    CHECKUIDBIN;
    if( CHECK_UID1( uid ) )
        ExitProcess( 0 );
}

void FOClient::Net_OnCritterXY()
{
    uint   crid;
    ushort hx;
    ushort hy;
    uchar  dir;
    Bin >> crid;
    Bin >> hx;
    Bin >> hy;
    Bin >> dir;

    CHECK_IN_BUFF_ERROR;

    if( GameOpt.DebugNet )
        AddMess( FOMB_GAME, Str::FormatBuf( " - crid<%u> hx<%u> hy<%u> dir<%u>.", crid, hx, hy, dir ) );

    if( !HexMngr.IsMapLoaded() )
        return;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    if( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() || dir >= DIRS_COUNT )
    {
        WriteLogF( _FUNC_, " - Error data, hx<%d>, hy<%d>, dir<%d>.\n", hx, hy, dir );
        return;
    }

    if( cr->GetDir() != dir )
        cr->SetDir( dir );

    if( cr->GetHexX() != hx || cr->GetHexY() != hy )
    {
        Field& f = HexMngr.GetField( hx, hy );
        if( f.Crit && f.Crit->IsFinishing() )
            EraseCritter( f.Crit->GetId() );

        HexMngr.TransitCritter( cr, hx, hy, false, true );
        cr->AnimateStay();
        if( cr == Chosen )
        {
            // Chosen->TickStart(GameOpt.Breaktime);
            Chosen->TickStart( 200 );
            // SetAction(CHOSEN_NONE);
            MoveDirs.clear();
            Chosen->MoveSteps.clear();
            RebuildLookBorders = true;
        }
    }

    cr->ZeroSteps();
}

void FOClient::Net_OnAllProperties()
{
    WriteLog( "Chosen properties..." );

    uint msg_len = 0;
    Bin >> msg_len;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    if( !Chosen )
    {
        WriteLog( "chosen not created, skip.\n" );
        return;
    }

    Chosen->Props.RestoreData( TempPropertiesData );

    // Animate
    if( !Chosen->IsAnim() )
        Chosen->AnimateStay();

    // Refresh borders
    RebuildLookBorders = true;

    WriteLog( "complete.\n" );
}

void FOClient::Net_OnCritterProperty( uint data_size )
{
    uint   msg_len = 0;
    uint   crid = 0;
    ushort property_index;
    if( data_size == 0 )
        Bin >> msg_len;
    Bin >> crid;
    Bin >> property_index;

    if( data_size != 0 )
    {
        TempPropertyData.resize( data_size );
        Bin.Pop( (char*) &TempPropertyData[ 0 ], data_size );
    }
    else
    {
        uint len = msg_len - sizeof( uint ) - sizeof( msg_len ) - sizeof( uint ) - sizeof( ushort );
        TempPropertyData.resize( len );
        if( len )
            Bin.Pop( (char*) &TempPropertyData[ 0 ], len );
    }

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr = ( Chosen && Chosen->GetId() == crid ? Chosen : GetCritter( crid ) );
    if( !cr )
        return;

    Property* prop = CritterCl::PropertiesRegistrator->Get( property_index );
    if( !prop )
        return;

    prop->SetSendIgnore( cr );
    prop->SetData( cr, !TempPropertyData.empty() ? &TempPropertyData[ 0 ] : NULL, (uint) TempPropertyData.size() );
    prop->SetSendIgnore( NULL );
}

void FOClient::Net_OnChosenClearItems()
{
    InitialItemsSend = true;

    if( !Chosen )
    {
        WriteLogF( _FUNC_, " - Chosen is not created.\n" );
        return;
    }

    if( Chosen->IsHaveLightSources() )
        HexMngr.RebuildLight();
    Chosen->EraseAllItems();
    CollectContItems();
}

void FOClient::Net_OnChosenAddItem()
{
    uint  msg_len;
    uint  item_id;
    hash  pid;
    uchar slot;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> pid;
    Bin >> slot;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    if( !Chosen )
    {
        WriteLogF( _FUNC_, " - Chosen is not created.\n" );
        return;
    }

    Item* prev_item = Chosen->GetItem( item_id );
    uchar prev_slot = SLOT_INV;
    uint  prev_light_hash = 0;
    if( prev_item )
    {
        prev_slot = prev_item->AccCritter.Slot;
        prev_light_hash = prev_item->LightGetHash();
        Chosen->EraseItem( prev_item, false );
    }

    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item )
    {
        WriteLogF( _FUNC_, " - Proto item not found, pid<%u>.\n", pid );
        return;
    }

    Item* item = new Item( item_id, proto_item );
    item->Props.RestoreData( TempPropertiesData );
    item->Accessory = ITEM_ACCESSORY_CRITTER;
    item->AccCritter.Slot = slot;

    item->AddRef();

    Chosen->AddItem( item );

    if( slot == SLOT_HAND1 || prev_slot == SLOT_HAND1 )
        RebuildLookBorders = true;
    if( item->LightGetHash() != prev_light_hash && ( slot != SLOT_INV || prev_slot != SLOT_INV ) )
        HexMngr.RebuildLight();
    if( item->IsHidden() )
        Chosen->EraseItem( item, true );
    CollectContItems();

    if( !InitialItemsSend && Script::PrepareContext( ClientFunctions.ItemInvIn, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( item );
        Script::RunPrepared();
    }

    item->Release();
}

void FOClient::Net_OnChosenEraseItem()
{
    uint item_id;
    Bin >> item_id;

    CHECK_IN_BUFF_ERROR;

    if( !Chosen )
    {
        WriteLogF( _FUNC_, " - Chosen is not created.\n" );
        return;
    }

    Item* item = Chosen->GetItem( item_id );
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item not found, id<%u>.\n", item_id );
        return;
    }

    item->AddRef();

    if( item->IsLight() && item->AccCritter.Slot != SLOT_INV )
        HexMngr.RebuildLight();
    Chosen->EraseItem( item, true );
    CollectContItems();

    if( Script::PrepareContext( ClientFunctions.ItemInvOut, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( item );
        Script::RunPrepared();
    }

    item->Release();
}

void FOClient::Net_OnAllItemsSend()
{
    InitialItemsSend = false;

    if( !Chosen )
    {
        WriteLogF( _FUNC_, " - Chosen is not created.\n" );
        return;
    }

    if( Chosen->Anim3d )
        Chosen->Anim3d->StartMeshGeneration();

    if( Script::PrepareContext( ClientFunctions.ItemInvAllIn, _FUNC_, "Game" ) )
        Script::RunPrepared();
}

void FOClient::Net_OnAddItemOnMap()
{
    uint   msg_len;
    uint   item_id;
    hash   item_pid;
    ushort item_hx;
    ushort item_hy;
    bool   is_added;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> item_pid;
    Bin >> item_hx;
    Bin >> item_hy;
    Bin >> is_added;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    if( HexMngr.IsMapLoaded() )
    {
        HexMngr.AddItem( item_id, item_pid, item_hx, item_hy, is_added, &TempPropertiesData );
    }
    else     // Global map car
    {
        ProtoItem* proto_item = ItemMngr.GetProtoItem( item_pid );
        if( proto_item && proto_item->IsCar() )
        {
            SAFEREL( GmapCar.Car );
            GmapCar.Car = new Item( item_id, proto_item );
            GmapCar.Car->Props.RestoreData( TempPropertiesData );
        }
    }

    Item* item = HexMngr.GetItemById( item_id );
    if( item && Script::PrepareContext( ClientFunctions.ItemMapIn, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( item );
        Script::RunPrepared();
    }

    // Refresh borders
    if( item && !item->IsRaked() )
        RebuildLookBorders = true;
}

void FOClient::Net_OnEraseItemFromMap()
{
    uint item_id;
    bool is_deleted;
    Bin >> item_id;
    Bin >> is_deleted;

    CHECK_IN_BUFF_ERROR;

    Item* item = HexMngr.GetItemById( item_id );
    if( item && Script::PrepareContext( ClientFunctions.ItemMapOut, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( item );
        Script::RunPrepared();
    }

    HexMngr.FinishItem( item_id, is_deleted );

    // Refresh borders
    if( item && !item->IsRaked() )
        RebuildLookBorders = true;
}

void FOClient::Net_OnAnimateItem()
{
    uint  item_id;
    uchar from_frm;
    uchar to_frm;
    Bin >> item_id;
    Bin >> from_frm;
    Bin >> to_frm;

    CHECK_IN_BUFF_ERROR;

    ItemHex* item = HexMngr.GetItemById( item_id );
    if( item )
        item->SetAnim( from_frm, to_frm );
}

void FOClient::Net_OnCombatResult()
{
    uint    msg_len;
    uint    data_count;
    UIntVec data_vec;
    Bin >> msg_len;
    Bin >> data_count;
    if( data_count > GameOpt.FloodSize / sizeof( uint ) )
        return;                                               // Insurance
    if( data_count )
    {
        data_vec.resize( data_count );
        Bin.Pop( (char*) &data_vec[ 0 ], data_count * sizeof( uint ) );
    }

    CHECK_IN_BUFF_ERROR;

    ScriptArray* arr = Script::CreateArray( "uint[]" );
    if( !arr )
        return;
    arr->Resize( data_count );
    for( uint i = 0; i < data_count; i++ )
        *( (uint*) arr->At( i ) ) = data_vec[ i ];

    if( Script::PrepareContext( ClientFunctions.CombatResult, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( arr );
        Script::RunPrepared();
    }

    arr->Release();
}

void FOClient::Net_OnEffect()
{
    hash   eff_pid;
    ushort hx;
    ushort hy;
    ushort radius;
    Bin >> eff_pid;
    Bin >> hx;
    Bin >> hy;
    Bin >> radius;

    CHECK_IN_BUFF_ERROR;

    // Base hex effect
    HexMngr.RunEffect( eff_pid, hx, hy, hx, hy );

    // Radius hexes effect
    if( radius > MAX_HEX_OFFSET )
        radius = MAX_HEX_OFFSET;
    int    cnt = NumericalNumber( radius ) * DIRS_COUNT;
    short* sx, * sy;
    GetHexOffsets( hx & 1, sx, sy );
    int    maxhx = HexMngr.GetMaxHexX();
    int    maxhy = HexMngr.GetMaxHexY();

    for( int i = 0; i < cnt; i++ )
    {
        int ex = hx + sx[ i ];
        int ey = hy + sy[ i ];
        if( ex >= 0 && ey >= 0 && ex < maxhx && ey < maxhy )
            HexMngr.RunEffect( eff_pid, ex, ey, ex, ey );
    }
}

#pragma MESSAGE("Synchronize effects showing.")
void FOClient::Net_OnFlyEffect()
{
    hash   eff_pid;
    uint   eff_cr1_id;
    uint   eff_cr2_id;
    ushort eff_cr1_hx;
    ushort eff_cr1_hy;
    ushort eff_cr2_hx;
    ushort eff_cr2_hy;
    Bin >> eff_pid;
    Bin >> eff_cr1_id;
    Bin >> eff_cr2_id;
    Bin >> eff_cr1_hx;
    Bin >> eff_cr1_hy;
    Bin >> eff_cr2_hx;
    Bin >> eff_cr2_hy;

    CHECK_IN_BUFF_ERROR;

    CritterCl* cr1 = GetCritter( eff_cr1_id );
    CritterCl* cr2 = GetCritter( eff_cr2_id );

    if( cr1 )
    {
        eff_cr1_hx = cr1->GetHexX();
        eff_cr1_hy = cr1->GetHexY();
    }

    if( cr2 )
    {
        eff_cr2_hx = cr2->GetHexX();
        eff_cr2_hy = cr2->GetHexY();
    }

    if( !HexMngr.RunEffect( eff_pid, eff_cr1_hx, eff_cr1_hy, eff_cr2_hx, eff_cr2_hy ) )
    {
        WriteLogF( _FUNC_, " - Run effect fail, pid<%u>.\n", eff_pid );
        return;
    }
}

void FOClient::Net_OnPlaySound( bool by_type )
{
    if( !by_type )
    {
        uint synchronize_crid;
        char sound_name[ 101 ];
        Bin >> synchronize_crid;
        Bin.Pop( sound_name, 100 );
        sound_name[ 100 ] = 0;
        SndMngr.PlaySound( sound_name );
    }
    else
    {
        uint  synchronize_crid;
        uchar sound_type;
        uchar sound_type_ext;
        uchar sound_id;
        uchar sound_id_ext;
        Bin >> synchronize_crid;
        Bin >> sound_type;
        Bin >> sound_type_ext;
        Bin >> sound_id;
        Bin >> sound_id_ext;
        SndMngr.PlaySoundType( sound_type, sound_type_ext, sound_id, sound_id_ext );
    }

    CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnPing()
{
    uchar ping;
    Bin >> ping;

    CHECK_IN_BUFF_ERROR;

    if( ping == PING_WAIT )
    {
        if( GetActiveScreen() == SCREEN__BARTER )
            SetCurMode( CUR_HAND );
        else
            SetLastCurMode();
    }
    else if( ping == PING_CLIENT )
    {
        if( UIDFail )
            return;

        Bout << NETMSG_PING;
        Bout << (uchar) PING_CLIENT;
    }
    else if( ping == PING_PING )
    {
        GameOpt.Ping = Timer::FastTick() - PingTick;
        PingTick = 0;
        PingCallTick = Timer::FastTick() + GameOpt.PingPeriod;
    }

    CHECK_MULTIPLY_WINDOWS8;
    CHECK_MULTIPLY_WINDOWS9;
}

void FOClient::Net_OnEndParseToGame()
{
    if( !Chosen )
    {
        WriteLogF( _FUNC_, " - Chosen is not created.\n" );
        return;
    }

    FlashGameWindow();
    ScreenFadeOut();

    if( CurMapPid )
    {
        MoveDirs.clear();
        HexMngr.FindSetCenter( Chosen->HexX, Chosen->HexY );
        Chosen->AnimateStay();
        SndMngr.PlayAmbient( MsgLocations->GetStr( STR_LOC_MAP_AMBIENT( CurMapLocPid, CurMapIndexInLoc ) ) );
        ShowMainScreen( SCREEN_GAME );
        HexMngr.RebuildLight();
    }
    else
    {
        ShowMainScreen( SCREEN_GLOBAL_MAP );
    }

    if( IsVideoPlayed() )
        MusicAfterVideo = MsgLocations->GetStr( STR_LOC_MAP_MUSIC( CurMapLocPid, CurMapIndexInLoc ) );
    else
        SndMngr.PlayMusic( MsgLocations->GetStr( STR_LOC_MAP_MUSIC( CurMapLocPid, CurMapIndexInLoc ) ) );

    WriteLog( "Entering to game complete.\n" );
}

void FOClient::Net_OnItemProperty( bool is_critter, uint data_size )
{
    uint   msg_len = 0;
    uint   crid = 0;
    uint   item_id;
    ushort property_index;
    if( data_size == 0 )
        Bin >> msg_len;
    if( is_critter )
        Bin >> crid;
    Bin >> item_id;
    Bin >> property_index;

    if( data_size != 0 )
    {
        TempPropertyData.resize( data_size );
        Bin.Pop( (char*) &TempPropertyData[ 0 ], data_size );
    }
    else
    {
        uint len = msg_len - sizeof( uint ) - sizeof( msg_len ) - ( is_critter ? sizeof( uint ) : 0 ) - sizeof( uint ) - sizeof( ushort );
        TempPropertyData.resize( len );
        if( len )
            Bin.Pop( (char*) &TempPropertyData[ 0 ], len );
    }

    CHECK_IN_BUFF_ERROR;

    Item* item = NULL;
    if( is_critter )
    {
        CritterCl* cr = GetCritter( crid );
        if( !cr )
            return;

        item = cr->GetItem( item_id );
        if( !item )
            return;
    }
    else
    {
        item = HexMngr.GetItemById( item_id );
        if( !item )
            return;
    }

    Property* prop = Item::PropertiesRegistrator->Get( property_index );
    if( !prop )
        return;

    prop->SetSendIgnore( item );
    prop->SetData( item, !TempPropertyData.empty() ? &TempPropertyData[ 0 ] : NULL, (uint) TempPropertyData.size() );
    prop->SetSendIgnore( NULL );

    if( !is_critter && Script::PrepareContext( ClientFunctions.ItemMapChanged, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( item );
        Script::SetArgObject( item );
        Script::RunPrepared();
    }
}

void FOClient::Net_OnChosenTalk()
{
    uint  msg_len;
    uchar is_npc;
    uint  talk_id;
    uchar count_answ;
    uint  text_id;
    uint  talk_time;

    Bin >> msg_len;
    Bin >> is_npc;
    Bin >> talk_id;
    Bin >> count_answ;

    DlgCurAnswPage = 0;
    DlgMaxAnswPage = 0;
    DlgAllAnswers.clear();
    DlgAnswers.clear();

    if( !count_answ )
    {
        // End dialog or barter
        if( IsScreenPresent( SCREEN__DIALOG ) )
            HideScreen( SCREEN__DIALOG );
        if( IsScreenPresent( SCREEN__BARTER ) )
            HideScreen( SCREEN__BARTER );
        return;
    }

    // Text params
    ushort lexems_len;
    char   lexems[ MAX_DLG_LEXEMS_TEXT + 1 ] = { 0 };
    Bin >> lexems_len;
    if( lexems_len && lexems_len <= MAX_DLG_LEXEMS_TEXT )
    {
        Bin.Pop( lexems, lexems_len );
        lexems[ lexems_len ] = '\0';
    }

    // Find critter
    DlgIsNpc = is_npc;
    DlgNpcId = talk_id;
    CritterCl* npc = ( is_npc ? GetCritter( talk_id ) : NULL );

    // Avatar
    DlgAvatarPic = NULL;
    if( npc && npc->Avatar->length() )
        DlgAvatarPic = ResMngr.GetAnim( Str::GetHash( npc->Avatar->c_str() ), RES_ATLAS_DYNAMIC );

    // Main text
    Bin >> text_id;

    char str[ MAX_FOTEXT ];
    Str::Copy( str, MsgDlg->GetStr( text_id ) );
    FormatTags( str, MAX_FOTEXT, Chosen, npc, lexems );
    DlgMainText = str;
    DlgMainTextCur = 0;
    DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, str );

    // Answers
    UIntVec answers_texts;
    for( int i = 0; i < count_answ; i++ )
    {
        Bin >> text_id;
        answers_texts.push_back( text_id );
    }

    const char answ_beg[] = { ' ', ' ', (char) 0xE2, (char) 0x80, (char) 0xA2, ' ', 0 };
    const char page_up[] = { 24, 24, 24, 0 };
    const int  page_up_height = SprMngr.GetLinesHeight( DlgAnswText.W(), 0, page_up );
    const char page_down[] = { 25, 25, 25, 0 };
    const int  page_down_height = SprMngr.GetLinesHeight( DlgAnswText.W(), 0, page_down );

    int        line = 0, height = 0, page = 0, answ = 0;
    while( true )
    {
        Rect pos(
            DlgAnswText.L + DlgNextAnswX * line,
            DlgAnswText.T + DlgNextAnswY * line + height,
            DlgAnswText.R + DlgNextAnswX * line,
            DlgAnswText.T + DlgNextAnswY * line + height );

        // Up arrow
        if( page && !line )
        {
            height += page_up_height;
            pos.B += page_up_height;
            line++;
            DlgAllAnswers.push_back( Answer( page, pos, page_up, -1 ) );
            continue;
        }

        Str::Copy( str, MsgDlg->GetStr( answers_texts[ answ ] ) );
        FormatTags( str, MAX_FOTEXT, Chosen, npc, lexems );
        Str::Insert( str, answ_beg );      // TODO: GetStr

        height += SprMngr.GetLinesHeight( DlgAnswText.W(), 0, str );
        pos.B = DlgAnswText.T + DlgNextAnswY * line + height;

        if( pos.B >= DlgAnswText.B && line > 1 )
        {
            // Down arrow
            Answer& answ_prev = DlgAllAnswers.back();
            if( line > 2 && DlgAnswText.B - answ_prev.Position.B < page_down_height )     // Check free space
            {
                // Not enough space
                // Migrate last answer to next page
                DlgAllAnswers.pop_back();
                pos = answ_prev.Position;
                pos.B = pos.T + page_down_height;
                DlgAllAnswers.push_back( Answer( page, pos, page_down, -2 ) );
                answ--;
            }
            else
            {
                // Add down arrow
                pos.B = pos.T + page_down_height;
                DlgAllAnswers.push_back( Answer( page, pos, page_down, -2 ) );
            }

            page++;
            height = 0;
            line = 0;
            DlgMaxAnswPage = page;
        }
        else
        {
            // Answer
            DlgAllAnswers.push_back( Answer( page, pos, str, answ ) );
            line++;
            answ++;
            if( answ >= count_answ )
                break;
        }
    }

    Bin >> talk_time;

    CHECK_IN_BUFF_ERROR;

    DlgCollectAnswers( false );
    if( talk_time )
        DlgEndTick = Timer::GameTick() + talk_time;
    else
        DlgEndTick = 0;
    if( IsScreenPresent( SCREEN__BARTER ) )
        HideScreen( SCREEN__BARTER );
    ShowScreen( SCREEN__DIALOG );
}

void FOClient::Net_OnCheckUID2()
{
    CHECKUIDBIN;
    if( CHECK_UID2( uid ) )
        UIDFail = true;
    CHECK_MULTIPLY_WINDOWS6;
}

void FOClient::Net_OnGameInfo()
{
    int    time;
    uchar  rain;
    bool   turn_based;
    bool   no_log_out;
    int*   day_time = HexMngr.GetMapDayTime();
    uchar* day_color = HexMngr.GetMapDayColor();
    Bin >> GameOpt.YearStart;
    Bin >> GameOpt.Year;
    Bin >> GameOpt.Month;
    Bin >> GameOpt.Day;
    Bin >> GameOpt.Hour;
    Bin >> GameOpt.Minute;
    Bin >> GameOpt.Second;
    Bin >> GameOpt.TimeMultiplier;
    Bin >> time;
    Bin >> rain;
    Bin >> turn_based;
    Bin >> no_log_out;
    Bin.Pop( (char*) day_time, sizeof( int ) * 4 );
    Bin.Pop( (char*) day_color, sizeof( uchar ) * 12 );

    CHECK_IN_BUFF_ERROR;

    DateTimeStamp dt = { GameOpt.YearStart, 1, 0, 1, 0, 0, 0, 0 };
    uint64        start_ft;
    Timer::DateTimeToFullTime( dt, start_ft );
    GameOpt.YearStartFTHi = ( start_ft >> 32 ) & 0xFFFFFFFF;
    GameOpt.YearStartFTLo = start_ft & 0xFFFFFFFF;
    GameOpt.FullSecond = Timer::GetFullSecond( GameOpt.Year, GameOpt.Month, GameOpt.Day, GameOpt.Hour, GameOpt.Minute, GameOpt.Second );
    GameOpt.FullSecondStart = GameOpt.FullSecond;
    GameOpt.GameTimeTick = Timer::GameTick();

    HexMngr.SetWeather( time, rain );
    SetDayTime( true );
    IsTurnBased = turn_based;
    if( !IsTurnBased )
        HexMngr.SetCritterContour( 0, 0 );
    NoLogOut = no_log_out;
}

void FOClient::Net_OnLoadMap()
{
    WriteLog( "Change map...\n" );

    hash  map_pid;
    hash  loc_pid;
    uchar map_index_in_loc;
    int   map_time;
    uchar map_rain;
    uint  hash_tiles;
    uint  hash_walls;
    uint  hash_scen;
    Bin >> map_pid;
    Bin >> loc_pid;
    Bin >> map_index_in_loc;
    Bin >> map_time;
    Bin >> map_rain;
    Bin >> hash_tiles;
    Bin >> hash_walls;
    Bin >> hash_scen;

    CHECK_IN_BUFF_ERROR;

    GameOpt.SpritesZoom = 1.0f;
    GmapZoom = 1.0f;

    CurMapPid = map_pid;
    CurMapLocPid = loc_pid;
    CurMapIndexInLoc = map_index_in_loc;
    GameMapTexts.clear();
    HexMngr.UnloadMap();
    SndMngr.ClearSounds();
    ShowMainScreen( SCREEN_WAIT );
    ClearCritters();
    ResMngr.ReinitializeDynamicAtlas();
    for( size_t i = 0, j = GmapPic.size(); i < j; i++ )
        GmapPic[ i ] = NULL;

    DropScroll();
    IsTurnBased = false;

    // Global
    if( !map_pid )
    {
        GmapNullParams();
        WriteLog( "Global map loaded.\n" );
    }
    // Local
    else
    {
        uint hash_tiles_cl = 0;
        uint hash_walls_cl = 0;
        uint hash_scen_cl = 0;
        HexMngr.GetMapHash( map_pid, hash_tiles_cl, hash_walls_cl, hash_scen_cl );

        if( hash_tiles != hash_tiles_cl || hash_walls != hash_walls_cl || hash_scen != hash_scen_cl )
        {
            Net_SendGiveMap( false, map_pid, 0, hash_tiles_cl, hash_walls_cl, hash_scen_cl );
            return;
        }

        if( !HexMngr.LoadMap( map_pid ) )
        {
            WriteLog( "Map not loaded. Disconnect.\n" );
            NetDisconnect();
            return;
        }

        HexMngr.SetWeather( map_time, map_rain );
        SetDayTime( true );
        LookBorders.clear();
        ShootBorders.clear();

        WriteLog( "Local map loaded.\n" );
    }

    Net_SendLoadMapOk();
}

void FOClient::Net_OnMap()
{
    uint   msg_len;
    hash   map_pid;
    ushort maxhx, maxhy;
    uchar  send_info;
    Bin >> msg_len;
    Bin >> map_pid;
    Bin >> maxhx;
    Bin >> maxhy;
    Bin >> send_info;

    CHECK_IN_BUFF_ERROR;

    WriteLog( "Map<%u> received...\n", map_pid );

    char map_name[ 256 ];
    Str::Format( map_name, "map%u", map_pid );

    bool  tiles = false;
    char* tiles_data = NULL;
    uint  tiles_len = 0;
    bool  walls = false;
    char* walls_data = NULL;
    uint  walls_len = 0;
    bool  scen = false;
    char* scen_data = NULL;
    uint  scen_len = 0;

    if( FLAG( send_info, SENDMAP_TILES ) )
    {
        uint count_tiles;
        Bin >> count_tiles;
        if( count_tiles )
        {
            tiles_len = count_tiles * sizeof( ProtoMap::Tile );
            tiles_data = new char[ tiles_len ];
            Bin.Pop( tiles_data, tiles_len );
        }
        tiles = true;
    }

    if( FLAG( send_info, SENDMAP_WALLS ) )
    {
        uint count_walls = 0;
        Bin >> count_walls;
        if( count_walls )
        {
            walls_len = count_walls * sizeof( SceneryCl );
            walls_data = new char[ walls_len ];
            Bin.Pop( walls_data, walls_len );
        }
        walls = true;
    }

    if( FLAG( send_info, SENDMAP_SCENERY ) )
    {
        uint count_scen = 0;
        Bin >> count_scen;
        if( count_scen )
        {
            scen_len = count_scen * sizeof( SceneryCl );
            scen_data = new char[ scen_len ];
            Bin.Pop( scen_data, scen_len );
        }
        scen = true;
    }

    CHECK_IN_BUFF_ERROR;

    uint        cache_len;
    uchar*      cache = Crypt.GetCache( map_name, cache_len );
    FileManager fm;

    if( cache && fm.LoadStream( cache, cache_len ) )
    {
        uint   buf_len = fm.GetFsize();
        uchar* buf = Crypt.Uncompress( fm.GetBuf(), buf_len, 50 );
        if( buf )
        {
            fm.UnloadFile();
            fm.LoadStream( buf, buf_len );
            delete[] buf;

            if( fm.GetBEUInt() == CLIENT_MAP_FORMAT_VER )
            {
                fm.SetCurPos( 0x20 );
                uint old_tiles_len = fm.GetBEUInt();
                uint old_walls_len = fm.GetBEUInt();
                uint old_scen_len = fm.GetBEUInt();

                if( !tiles )
                {
                    tiles_len = old_tiles_len;
                    fm.SetCurPos( 0x2C );
                    tiles_data = new char[ tiles_len ];
                    fm.CopyMem( tiles_data, tiles_len );
                    tiles = true;
                }

                if( !walls )
                {
                    walls_len = old_walls_len;
                    fm.SetCurPos( 0x2C + old_tiles_len );
                    walls_data = new char[ walls_len ];
                    fm.CopyMem( walls_data, walls_len );
                    walls = true;
                }

                if( !scen )
                {
                    scen_len = old_scen_len;
                    fm.SetCurPos( 0x2C + old_tiles_len + old_walls_len );
                    scen_data = new char[ scen_len ];
                    fm.CopyMem( scen_data, scen_len );
                    scen = true;
                }

                fm.UnloadFile();
            }
        }
    }
    SAFEDELA( cache );

    if( tiles && walls && scen )
    {
        fm.ClearOutBuf();

        fm.SetBEUInt( CLIENT_MAP_FORMAT_VER );
        fm.SetBEUInt( map_pid );
        fm.SetBEUShort( maxhx );
        fm.SetBEUShort( maxhy );
        fm.SetBEUInt( 0 );
        fm.SetBEUInt( 0 );

        fm.SetBEUInt( tiles_len / sizeof( ProtoMap::Tile ) );
        fm.SetBEUInt( walls_len / sizeof( SceneryCl ) );
        fm.SetBEUInt( scen_len / sizeof( SceneryCl ) );
        fm.SetBEUInt( tiles_len );
        fm.SetBEUInt( walls_len );
        fm.SetBEUInt( scen_len );

        if( tiles_len )
            fm.SetData( tiles_data, tiles_len );
        if( walls_len )
            fm.SetData( walls_data, walls_len );
        if( scen_len )
            fm.SetData( scen_data, scen_len );

        uint   obuf_len = fm.GetOutBufLen();
        uchar* buf = Crypt.Compress( fm.GetOutBuf(), obuf_len );
        if( !buf )
        {
            WriteLog( "Failed to compress data<%s>, disconnect.\n", map_name );
            NetDisconnect();
            fm.ClearOutBuf();
            return;
        }

        fm.ClearOutBuf();
        fm.SetData( buf, obuf_len );
        delete[] buf;

        Crypt.SetCache( map_name, fm.GetOutBuf(), fm.GetOutBufLen() );
        fm.ClearOutBuf();
    }
    else
    {
        WriteLog( "Not for all data of map, disconnect.\n" );
        NetDisconnect();
        SAFEDELA( tiles_data );
        SAFEDELA( walls_data );
        SAFEDELA( scen_data );
        return;
    }

    SAFEDELA( tiles_data );
    SAFEDELA( walls_data );
    SAFEDELA( scen_data );

    AutomapWaitPids.erase( map_pid );
    AutomapReceivedPids.insert( map_pid );

    WriteLog( "Map saved.\n" );
}

void FOClient::Net_OnGlobalInfo()
{
    uint  msg_len;
    uchar info_flags;
    Bin >> msg_len;
    Bin >> info_flags;

    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
    {
        GmapLoc.clear();
        ushort count_loc;
        Bin >> count_loc;

        for( int i = 0; i < count_loc; i++ )
        {
            GmapLocation loc;
            Bin >> loc.LocId;
            Bin >> loc.LocPid;
            Bin >> loc.LocWx;
            Bin >> loc.LocWy;
            Bin >> loc.Radius;
            Bin >> loc.Color;
            Bin >> loc.Entrances;

            if( loc.LocId )
                GmapLoc.push_back( loc );
        }
    }

    if( FLAG( info_flags, GM_INFO_LOCATION ) )
    {
        GmapLocation loc;
        bool         add;
        Bin >> add;
        Bin >> loc.LocId;
        Bin >> loc.LocPid;
        Bin >> loc.LocWx;
        Bin >> loc.LocWy;
        Bin >> loc.Radius;
        Bin >> loc.Color;
        Bin >> loc.Entrances;

        auto it = std::find( GmapLoc.begin(), GmapLoc.end(), loc.LocId );
        if( add )
        {
            if( it != GmapLoc.end() )
                *it = loc;
            else
                GmapLoc.push_back( loc );
        }
        else
        {
            if( it != GmapLoc.end() )
                GmapLoc.erase( it );
        }
    }

    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) )
    {
        ushort cur_x, cur_y, to_x, to_y;
        uint   speed;
        Bin >> cur_x;
        Bin >> cur_y;
        Bin >> to_x;
        Bin >> to_y;
        Bin >> speed;
        Bin >> GmapWait;

        GmapGroupRealOldX = GmapGroupCurX;
        GmapGroupRealOldY = GmapGroupCurY;
        GmapGroupRealCurX = cur_x;
        GmapGroupRealCurY = cur_y;
        GmapGroupToX = to_x;
        GmapGroupToY = to_y;
        GmapGroupSpeed = (float) speed / 1000000.0f;
        GmapMoveTick = Timer::GameTick();

        if( !GmapActive )
        {
            GmapGroupRealOldX = GmapGroupRealCurX;
            GmapGroupRealOldY = GmapGroupRealCurY;
            GmapGroupCurX = GmapGroupRealCurX;
            GmapGroupCurY = GmapGroupRealCurY;
            GmapOffsetX = ( GmapWMap[ 2 ] - GmapWMap[ 0 ] ) / 2 + GmapWMap[ 0 ] - GmapGroupCurX;
            GmapOffsetY = ( GmapWMap[ 3 ] - GmapWMap[ 1 ] ) / 2 + GmapWMap[ 1 ] - GmapGroupCurY;
            GmapTrace.clear();

            int w = GM_MAXX;
            int h = GM_MAXY;
            if( GmapOffsetX > GmapWMap[ 0 ] )
                GmapOffsetX = GmapWMap[ 0 ];
            if( GmapOffsetY > GmapWMap[ 1 ] )
                GmapOffsetY = GmapWMap[ 1 ];
            if( GmapOffsetX < GmapWMap[ 2 ] - w )
                GmapOffsetX = GmapWMap[ 2 ] - w;
            if( GmapOffsetY < GmapWMap[ 3 ] - h )
                GmapOffsetY = GmapWMap[ 3 ] - h;

            SetCurMode( CUR_DEFAULT );
        }

        // Car master id
        Bin >> GmapCar.MasterId;

        SAFEREL( GmapCar.Car );

        GmapActive = true;
    }

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
    {
        Bin.Pop( (char*) GmapFog.GetData(), GM_ZONES_FOG_SIZE );
    }

    if( FLAG( info_flags, GM_INFO_FOG ) )
    {
        ushort zx, zy;
        uchar  fog;
        Bin >> zx;
        Bin >> zy;
        Bin >> fog;
        GmapFog.Set2Bit( zx, zy, fog );
    }

    if( FLAG( info_flags, GM_INFO_CRITTERS ) )
    {
        ClearCritters();
        // After wait AddCritters
    }

    CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnGlobalEntrances()
{
    uint  msg_len;
    uint  loc_id;
    uchar count;
    bool  entrances[ 0x100 ];
    memzero( entrances, sizeof( entrances ) );
    Bin >> msg_len;
    Bin >> loc_id;
    Bin >> count;

    for( int i = 0; i < count; i++ )
    {
        uchar e;
        Bin >> e;
        entrances[ e ] = true;
    }

    CHECK_IN_BUFF_ERROR;

    GmapShowEntrancesLocId = loc_id;
    memcpy( GmapShowEntrances, entrances, sizeof( entrances ) );
}

void FOClient::Net_OnContainerInfo()
{
    uint  msg_len;
    uchar transfer_type;
    uint  talk_time;
    uint  cont_id;
    max_t cont_pid;      // Or Barter K
    uint  items_count;
    Bin >> msg_len;
    Bin >> transfer_type;

    bool open_screen = ( FLAG( transfer_type, 0x80 ) ? true : false );
    UNSETFLAG( transfer_type, 0x80 );

    if( transfer_type == TRANSFER_CLOSE )
    {
        if( IsScreenPresent( SCREEN__BARTER ) )
            HideScreen( SCREEN__BARTER );
        if( IsScreenPresent( SCREEN__PICKUP ) )
            HideScreen( SCREEN__PICKUP );
        return;
    }

    Bin >> talk_time;
    Bin >> cont_id;
    Bin >> cont_pid;
    Bin >> items_count;

    ItemVec item_container;
    for( uint i = 0; i < items_count; i++ )
    {
        uint item_id;
        hash item_pid;
        Bin >> item_id;
        Bin >> item_pid;
        NET_READ_PROPERTIES( Bin, TempPropertiesData );

        ProtoItem* proto_item = ItemMngr.GetProtoItem( item_pid );
        if( item_id && proto_item )
        {
            Item* item = new Item( item_id, proto_item );
            item->Props.RestoreData( TempPropertiesData );
            item_container.push_back( item );
        }
    }

    CHECK_IN_BUFF_ERROR;

    if( !Chosen )
    {
        Item::ClearItems( item_container );
        return;
    }

    Item::SortItems( item_container );
    if( talk_time )
        DlgEndTick = Timer::GameTick() + talk_time;
    else
        DlgEndTick = 0;
    PupContId = cont_id;
    PupContPid = 0;

    if( transfer_type == TRANSFER_CRIT_BARTER )
    {
        PupTransferType = transfer_type;
        BarterK = (ushort) cont_pid;
        BarterCount = items_count;
        Item::ClearItems( BarterCont2Init );
        BarterCont2Init = item_container;
        item_container.clear();
        BarterScroll1o = 0;
        BarterScroll2o = 0;
        Item::ClearItems( BarterCont1oInit );
        Item::ClearItems( BarterCont2oInit );

        if( open_screen )
        {
            if( IsScreenPresent( SCREEN__DIALOG ) )
                HideScreen( SCREEN__DIALOG );
            if( !IsScreenPresent( SCREEN__BARTER ) )
                ShowScreen( SCREEN__BARTER );
            BarterScroll1 = 0;
            BarterScroll2 = 0;
            BarterText = "";
            DlgMainTextCur = 0;
            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
        }
    }
    else
    {
        PupTransferType = transfer_type;
        PupContPid = ( transfer_type == TRANSFER_HEX_CONT_UP || transfer_type == TRANSFER_HEX_CONT_DOWN || transfer_type == TRANSFER_FAR_CONT ? (hash) cont_pid : 0 );
        PupCount = items_count;
        Item::ClearItems( PupCont2Init );
        PupCont2Init = item_container;
        item_container.clear();
        PupScrollCrit = 0;
        PupLastPutId = 0;

        if( open_screen )
        {
            if( !IsScreenPresent( SCREEN__PICKUP ) )
                ShowScreen( SCREEN__PICKUP );
            PupScroll1 = 0;
            PupScroll2 = 0;
        }

        if( PupTransferType == TRANSFER_CRIT_LOOT )
        {
            CritVec& loot = PupGetLootCrits();
            for( uint i = 0, j = (uint) loot.size(); i < j; i++ )
            {
                if( loot[ i ]->GetId() == PupContId )
                {
                    PupScrollCrit = i;
                    break;
                }
            }
        }
    }

    CollectContItems();
}

void FOClient::Net_OnFollow()
{
    uint  rule;
    uchar follow_type;
    hash  map_pid;
    uint  wait_time;
    Bin >> rule;
    Bin >> follow_type;
    Bin >> map_pid;
    Bin >> wait_time;

    CHECK_IN_BUFF_ERROR;

    ShowScreen( SCREEN__DIALOGBOX );
    DlgboxType = DIALOGBOX_FOLLOW;
    FollowRuleId = rule;
    FollowType = follow_type;
    FollowMap = map_pid;
    DlgboxWait = Timer::GameTick() + wait_time;

    // Find rule
    char       cr_name[ 64 ];
    CritterCl* cr = GetCritter( rule );
    Str::Copy( cr_name, cr ? cr->GetName() : MsgGame->GetStr( STR_FOLLOW_UNKNOWN_CRNAME ) );
    // Find map
    char map_name[ 64 ];
    Str::Copy( map_name, map_pid ? "local map" : MsgGame->GetStr( STR_FOLLOW_GMNAME ) );

    switch( FollowType )
    {
    case FOLLOW_PREP:
        Str::Copy( DlgboxText, FmtGameText( STR_FOLLOW_PREP, cr_name, map_name ) );
        break;
    case FOLLOW_FORCE:
        Str::Copy( DlgboxText, FmtGameText( STR_FOLLOW_FORCE, cr_name, map_name ) );
        break;
    default:
        WriteLogF( _FUNC_, " - Error FollowType\n" );
        Str::Copy( DlgboxText, "ERROR!" );
        break;
    }

    DlgboxButtonText[ 0 ] = MsgGame->GetStr( STR_DIALOGBOX_FOLLOW );
    DlgboxButtonText[ 1 ] = MsgGame->GetStr( STR_DIALOGBOX_CANCEL );
    DlgboxButtonsCount = 2;
}

void FOClient::Net_OnPlayersBarter()
{
    uchar barter;
    uint  param;
    uint  param_ext;
    Bin >> barter;
    Bin >> param;
    Bin >> param_ext;

    CHECK_IN_BUFF_ERROR;

    switch( barter )
    {
    case BARTER_BEGIN:
    {
        CritterCl* cr = GetCritter( param );
        if( !cr )
            break;
        BarterText = FmtGameText( STR_BARTER_BEGIN, cr->GetName(),
                                  !FLAG( param_ext, 1 ) ? MsgGame->GetStr( STR_BARTER_OPEN_MODE ) : MsgGame->GetStr( STR_BARTER_HIDE_MODE ),
                                  !FLAG( param_ext, 2 ) ? MsgGame->GetStr( STR_BARTER_OPEN_MODE ) : MsgGame->GetStr( STR_BARTER_HIDE_MODE ) );
        BarterText += "\n";
        DlgMainTextCur = 0;
        DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
        param_ext = ( FLAG( param_ext, 2 ) ? true : false );
    }
    case BARTER_REFRESH:
    {
        CritterCl* cr = GetCritter( param );
        if( !cr )
            break;

        if( IsScreenPresent( SCREEN__DIALOG ) )
            HideScreen( SCREEN__DIALOG );
        if( !IsScreenPresent( SCREEN__BARTER ) )
            ShowScreen( SCREEN__BARTER );

        BarterIsPlayers = true;
        BarterOpponentId = param;
        BarterOpponentHide = ( param_ext != 0 );
        BarterOffer = false;
        BarterOpponentOffer = false;
        Item::ClearItems( BarterCont2Init );
        Item::ClearItems( BarterCont1oInit );
        Item::ClearItems( BarterCont2oInit );
        CollectContItems();
    }
    break;
    case BARTER_END:
    {
        if( IsScreenPlayersBarter() )
            ShowScreen( SCREEN_NONE );
    }
    break;
    case BARTER_TRY:
    {
        CritterCl* cr = GetCritter( param );
        if( !cr )
            break;
        ShowScreen( SCREEN__DIALOGBOX );
        DlgboxType = DIALOGBOX_BARTER;
        PBarterPlayerId = param;
        BarterOpponentHide = ( param_ext != 0 );
        PBarterHide = BarterOpponentHide;
        Str::Copy( DlgboxText, FmtGameText( STR_BARTER_DIALOGBOX, cr->GetName(), !BarterOpponentHide ? MsgGame->GetStr( STR_BARTER_OPEN_MODE ) : MsgGame->GetStr( STR_BARTER_HIDE_MODE ) ) );
        DlgboxWait = Timer::GameTick() + 20000;
        DlgboxButtonText[ 0 ] = MsgGame->GetStr( STR_DIALOGBOX_BARTER_OPEN );
        DlgboxButtonText[ 1 ] = MsgGame->GetStr( STR_DIALOGBOX_BARTER_HIDE );
        DlgboxButtonText[ 2 ] = MsgGame->GetStr( STR_DIALOGBOX_CANCEL );
        DlgboxButtonsCount = 3;
    }
    break;
    case BARTER_SET_SELF:
    case BARTER_SET_OPPONENT:
    {
        if( !IsScreenPlayersBarter() )
        {
            Net_SendPlayersBarter( BARTER_END, 0, 0 );
            break;
        }

        BarterOffer = false;
        BarterOpponentOffer = false;
        bool     is_hide = ( barter == BARTER_SET_OPPONENT && BarterOpponentHide );
        ItemVec& cont = ( barter == BARTER_SET_SELF ? InvContInit : BarterCont2Init );
        ItemVec& cont_o = ( barter == BARTER_SET_SELF ? BarterCont1oInit : BarterCont2oInit );

        if( !is_hide )
        {
            auto it = PtrCollectionFind( cont.begin(), cont.end(), param );
            if( it == cont.end() || param_ext > ( *it )->GetCount() )
            {
                Net_SendPlayersBarter( BARTER_REFRESH, 0, 0 );
                break;
            }
            Item* citem = *it;
            auto  it_ = PtrCollectionFind( cont_o.begin(), cont_o.end(), param );
            if( it_ == cont_o.end() )
            {
                citem->AddRef();
                cont_o.push_back( citem );
                it_ = cont_o.begin() + cont_o.size() - 1;
                ( *it_ )->SetCount( param_ext );
            }
            else
            {
                ( *it_ )->ChangeCount( param_ext );
            }
            citem->ChangeCount( -(int) param_ext );
            if( !citem->GetCount() || !citem->IsStackable() )
            {
                ( *it )->Release();
                cont.erase( it );
            }
        }
        else                 // Hide
        {
            WriteLogF( _FUNC_, " - Invalid argument.\n" );
            Net_SendPlayersBarter( BARTER_END, 0, 0 );
            // See void FOClient::Net_OnPlayersBarterSetHide()
        }
        CollectContItems();
    }
    break;
    case BARTER_UNSET_SELF:
    case BARTER_UNSET_OPPONENT:
    {
        if( !IsScreenPlayersBarter() )
        {
            Net_SendPlayersBarter( BARTER_END, 0, 0 );
            break;
        }

        BarterOffer = false;
        BarterOpponentOffer = false;
        bool     is_hide = ( barter == BARTER_UNSET_OPPONENT && BarterOpponentHide );
        ItemVec& cont = ( barter == BARTER_UNSET_SELF ? InvContInit : BarterCont2Init );
        ItemVec& cont_o = ( barter == BARTER_UNSET_SELF ? BarterCont1oInit : BarterCont2oInit );

        if( !is_hide )
        {
            auto it = PtrCollectionFind( cont_o.begin(), cont_o.end(), param );
            if( it == cont_o.end() || param_ext > ( *it )->GetCount() )
            {
                Net_SendPlayersBarter( BARTER_REFRESH, 0, 0 );
                break;
            }
            Item* citem = *it;
            auto  it_ = PtrCollectionFind( cont.begin(), cont.end(), param );
            if( it_ == cont.end() )
            {
                citem->AddRef();
                cont.push_back( citem );
                it_ = cont.begin() + cont.size() - 1;
                ( *it_ )->SetCount( param_ext );
            }
            else
            {
                ( *it_ )->ChangeCount( param_ext );
            }
            citem->ChangeCount( -(int) param_ext );
            if( !citem->GetCount() || !citem->IsStackable() )
            {
                ( *it )->Release();
                cont_o.erase( it );
            }
        }
        else                 // Hide
        {
            Item* citem = GetContainerItem( cont_o, param );
            if( !citem || param_ext > citem->GetCount() )
            {
                Net_SendPlayersBarter( BARTER_REFRESH, 0, 0 );
                break;
            }
            citem->ChangeCount( -(int) param_ext );
            if( !citem->GetCount() || !citem->IsStackable() )
            {
                auto it = PtrCollectionFind( cont_o.begin(), cont_o.end(), param );
                ( *it )->Release();
                cont_o.erase( it );
            }
        }
        CollectContItems();
    }
    break;
    case BARTER_OFFER:
    {
        if( !IsScreenPlayersBarter() )
        {
            Net_SendPlayersBarter( BARTER_END, 0, 0 );
            break;
        }

        if( !param_ext )
            BarterOffer = ( param != 0 );
        else
            BarterOpponentOffer = ( param != 0 );

        if( BarterOpponentOffer && !BarterOffer )
        {
            CritterCl* cr = GetCritter( BarterOpponentId );
            if( !cr )
                break;
            BarterText += FmtGameText( STR_BARTER_READY_OFFER, cr->GetName() );
            BarterText += "\n";
            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
        }
    }
    break;
    default:
        Net_SendPlayersBarter( BARTER_END, 0, 0 );
        break;
    }
}

void FOClient::Net_OnPlayersBarterSetHide()
{
    uint msg_len;
    uint item_id;
    hash pid;
    uint count;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> pid;
    Bin >> count;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    if( !IsScreenPlayersBarter() )
    {
        Net_SendPlayersBarter( BARTER_END, 0, 0 );
        return;
    }

    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item )
    {
        WriteLogF( _FUNC_, " - Proto target_item<%u> not found.\n", pid );
        Net_SendPlayersBarter( BARTER_REFRESH, 0, 0 );
        return;
    }

    Item* citem = GetContainerItem( BarterCont2oInit, item_id );
    if( !citem )
    {
        Item* item = new Item( item_id, proto_item );
        item->Props.RestoreData( TempPropertiesData );
        item->SetCount( count );
        BarterCont2oInit.push_back( item );
    }
    else
    {
        uint new_count = citem->GetCount() + count;
        citem->Props.RestoreData( TempPropertiesData );
        citem->SetCount( new_count );
    }
    CollectContItems();
}

void FOClient::Net_OnShowScreen()
{
    int  screen_type;
    uint param;
    bool need_answer;
    Bin >> screen_type;
    Bin >> param;
    Bin >> need_answer;

    CHECK_IN_BUFF_ERROR;

    // Close current
    switch( screen_type )
    {
    case SHOW_SCREEN_TIMER:
    case SHOW_SCREEN_DIALOGBOX:
    case SHOW_SCREEN_SKILLBOX:
    case SHOW_SCREEN_BAG:
    case SHOW_SCREEN_SAY:
        ShowScreen( SCREEN_NONE );
        break;
    default:
        ShowScreen( SCREEN_NONE );
        break;
    }

    // Open new
    switch( screen_type )
    {
    case SHOW_SCREEN_TIMER:
        TimerStart( 0, ResMngr.GetInvAnim( param ), 0 );
        break;
    case SHOW_SCREEN_DIALOGBOX:
        ShowScreen( SCREEN__DIALOGBOX );
        DlgboxWait = 0;
        DlgboxText[ 0 ] = 0;
        DlgboxType = DIALOGBOX_MANUAL;
        if( param >= MAX_DLGBOX_BUTTONS )
            param = MAX_DLGBOX_BUTTONS - 1;
        DlgboxButtonsCount = param + 1;
        for( uint i = 0; i < DlgboxButtonsCount; i++ )
            DlgboxButtonText[ i ] = "";
        DlgboxButtonText[ param ] = MsgGame->GetStr( STR_DIALOGBOX_CANCEL );
        break;
    case SHOW_SCREEN_SKILLBOX:
        ShowScreen( SCREEN__SKILLBOX );
        break;
    case SHOW_SCREEN_BAG:
        ShowScreen( SCREEN__USE );
        break;
    case SHOW_SCREEN_SAY:
        ShowScreen( SCREEN__SAY );
        SayType = DIALOGSAY_TEXT;
        SayText = "";
        if( param )
            SayOnlyNumbers = true;
        break;
    case SHOW_ELEVATOR:
        ElevatorGenerate( param );
        break;
    // Just open
    case SHOW_SCREEN_INVENTORY:
        ShowScreen( SCREEN__INVENTORY );
        return;
    case SHOW_SCREEN_CHARACTER:
        ShowScreen( SCREEN__CHARACTER );
        return;
    case SHOW_SCREEN_FIXBOY:
        ShowScreen( SCREEN__FIX_BOY );
        return;
    case SHOW_SCREEN_PIPBOY:
        ShowScreen( SCREEN__PIP_BOY );
        return;
    case SHOW_SCREEN_MINIMAP:
        ShowScreen( SCREEN__MINI_MAP );
        return;
    case SHOW_SCREEN_CLOSE:
        return;
    default:
        return;
    }

    ShowScreenType = screen_type;
    ShowScreenParam = param;
    ShowScreenNeedAnswer = need_answer;
}

void FOClient::Net_OnRunClientScript()
{
    char          str[ MAX_FOTEXT ];
    uint          msg_len;
    ushort        func_name_len;
    ScriptString* func_name = ScriptString::Create();
    int           p0, p1, p2;
    ushort        p3len;
    ScriptString* p3 = NULL;
    ushort        p4size;
    ScriptArray*  p4 = NULL;
    Bin >> msg_len;
    Bin >> func_name_len;
    if( func_name_len && func_name_len < MAX_FOTEXT )
    {
        Bin.Pop( str, func_name_len );
        str[ func_name_len ] = 0;
        *func_name = str;
    }
    Bin >> p0;
    Bin >> p1;
    Bin >> p2;
    Bin >> p3len;
    if( p3len && p3len )
    {
        Bin.Pop( str, p3len );
        str[ p3len ] = 0;
        p3 = ScriptString::Create( str );
    }
    Bin >> p4size;
    if( p4size )
    {
        p4 = Script::CreateArray( "int[]" );
        if( p4 )
        {
            p4->Resize( p4size );
            Bin.Pop( (char*) p4->At( 0 ), p4size * sizeof( uint ) );
        }
    }

    CHECK_IN_BUFF_ERROR;

    // Reparse module
    int bind_id;
    if( Str::Substring( func_name->c_str(), "@" ) )
        bind_id = Script::Bind( func_name->c_str(), "void %s(int,int,int,string@,int[]@)", true );
    else
        bind_id = Script::Bind( "client_main", func_name->c_str(), "void %s(int,int,int,string@,int[]@)", true );

    if( bind_id > 0 && Script::PrepareContext( bind_id, _FUNC_, "Game" ) )
    {
        Script::SetArgUInt( p0 );
        Script::SetArgUInt( p1 );
        Script::SetArgUInt( p2 );
        Script::SetArgObject( p3 );
        Script::SetArgObject( p4 );
        Script::RunPrepared();
    }

    func_name->Release();
    if( p3 )
        p3->Release();
    if( p4 )
        p4->Release();
}

void FOClient::Net_OnDropTimers()
{
    ScoresNextUploadTick = 0;
    FixNextShowCraftTick = 0;
    GmapNextShowEntrancesTick = 0;
    GmapShowEntrancesLocId = 0;
}

void FOClient::Net_OnCheckUID3()
{
    CHECKUIDBIN;
    if( CHECK_UID1( uid ) )
        Net_SendPing( PING_UID_FAIL );
    CHECK_MULTIPLY_WINDOWS7;
}

void FOClient::Net_OnUpdateFilesList()
{
    uint     msg_len;
    UCharVec data;
    Bin >> msg_len;
    data.resize( msg_len - ( sizeof( uint ) + sizeof( msg_len ) ) );
    Bin.Pop( (char*) &data[ 0 ], (uint) data.size() );

    CHECK_IN_BUFF_ERROR;

    FileManager fm;
    fm.LoadStream( &data[ 0 ], (uint) data.size() );

    SAFEDEL( UpdateFilesList );
    UpdateFilesList = new UpdateFileVec();
    UpdateFilesWholeSize = 0;

    for( uint file_index = 0; ; file_index++ )
    {
        short name_len = fm.GetLEShort();
        if( name_len == -1 )
            break;
        if( name_len < 0 || name_len >= MAX_FOPATH )
        {
            NetDisconnect();
            break;
        }

        char name[ MAX_FOPATH ];
        fm.CopyMem( name, name_len );
        name[ name_len ] = 0;

        uint size = fm.GetLEUInt();
        uint hash = fm.GetLEUInt();

        if( name[ 0 ] == '*' )
        {
            uint  cur_hash_len;
            uint* cur_hash = (uint*) Crypt.GetCache( ( string( name ) + CACHE_HASH_APPENDIX ).c_str(), cur_hash_len );
            if( cur_hash && cur_hash_len == sizeof( hash ) && *cur_hash == hash )
                continue;
        }
        else
        {
            FileManager file;
            if( file.LoadFile( name, PT_DATA, true ) && file.GetFsize() == size )
            {
                if( hash == 0 )
                    continue;

                file.UnloadFile();
                if( file.LoadFile( name, PT_DATA ) && file.GetFsize() > 0 && hash == Crypt.Crc32( file.GetBuf(), file.GetFsize() ) )
                    continue;
            }
        }

        UpdateFile update_file;
        update_file.Index = file_index;
        update_file.Name = name;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        UpdateFilesList->push_back( update_file );
        UpdateFilesWholeSize += size;
    }
}

void FOClient::Net_OnUpdateFileData()
{
    // Get portion
    uchar data[ FILE_UPDATE_PORTION ];
    Bin.Pop( (char*) data, sizeof( data ) );

    CHECK_IN_BUFF_ERROR;

    UpdateFile& update_file = UpdateFilesList->front();

    // Write data to temp file
    if( !FileWrite( UpdateFileTemp, data, MIN( update_file.RemaningSize, sizeof( data ) ) ) )
    {
        UpdateFilesAbort( STR_FILESYSTEM_ERROR, "FILESYSTEM_ERROR1" );
        return;
    }

    // Get next portion or finalize data
    update_file.RemaningSize -= MIN( update_file.RemaningSize, sizeof( data ) );
    if( update_file.RemaningSize > 0 )
    {
        // Request next portion
        Bout << NETMSG_GET_UPDATE_FILE_DATA;
    }
    else
    {
        // Finalize received data
        FileClose( UpdateFileTemp );
        UpdateFileTemp = NULL;

        // Cache
        if( update_file.Name[ 0 ] == CACHE_MAGIC_CHAR[ 0 ] )
        {
            FileManager cache_data;
            if( !cache_data.LoadFile( FileManager::GetWritePath( "update.temp", PT_DATA ), PT_ROOT ) )
            {
                UpdateFilesAbort( STR_FILESYSTEM_ERROR, "FILESYSTEM_ERROR3" );
                return;
            }

            Crypt.SetCache( update_file.Name.c_str(), cache_data.GetBuf(), cache_data.GetFsize() );
            Crypt.SetCache( ( update_file.Name + CACHE_HASH_APPENDIX ).c_str(), (uchar*) &update_file.Hash, sizeof( update_file.Hash ) );
        }
        // File
        else
        {
            char to_path[ MAX_FOPATH ];
            FileManager::GetWritePath( update_file.Name.c_str(), PT_DATA, to_path );
            FileManager::FormatPath( to_path );
            if( !FileManager::CopyFile( FileManager::GetWritePath( "update.temp", PT_DATA ), to_path ) )
            {
                UpdateFilesAbort( STR_FILESYSTEM_ERROR, "FILESYSTEM_ERROR4" );
                return;
            }
        }

        FileManager::DeleteFile( FileManager::GetWritePath( "update.temp", PT_DATA ) );
        UpdateFilesList->erase( UpdateFilesList->begin() );
        UpdateFileActive = false;
    }
}

void FOClient::Net_OnQuest( bool many )
{
    many ? \
    WriteLog( "Quests..." ) :
    WriteLog( "Quest..." );

    if( many )
    {
        uint msg_len;
        uint q_count;
        Bin >> msg_len;
        Bin >> q_count;

        QuestMngr.Finish();
        if( q_count )
        {
            UIntVec quests;
            quests.resize( q_count );
            Bin.Pop( (char*) &quests[ 0 ], q_count * sizeof( uint ) );
            for( uint i = 0; i < quests.size(); ++i )
                QuestMngr.OnQuest( quests[ i ] );
        }
    }
    else
    {
        uint q_num;
        Bin >> q_num;
        QuestMngr.OnQuest( q_num );

        // Inform player
        Quest* quest = QuestMngr.GetQuest( q_num );
        if( quest )
            AddMess( FOMB_GAME, quest->str.c_str() );
    }

    CHECK_IN_BUFF_ERROR;

    WriteLog( "Complete.\n" );
}

void FOClient::Net_OnHoloInfo()
{
    uint   msg_len;
    bool   clear;
    ushort offset;
    ushort count;
    Bin >> msg_len;
    Bin >> clear;
    Bin >> offset;
    Bin >> count;

    if( clear )
        memzero( HoloInfo, sizeof( HoloInfo ) );
    if( count )
        Bin.Pop( (char*) &HoloInfo[ offset ], count * sizeof( uint ) );

    CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnScores()
{
    Bin.Pop( &BestScores[ 0 ][ 0 ], SCORE_NAME_LEN * SCORES_MAX );
    for( int i = 0; i < SCORES_MAX; i++ )
        BestScores[ i ][ SCORE_NAME_LEN - 1 ] = '\0';
}

void FOClient::Net_OnUserHoloStr()
{
    uint   msg_len;
    uint   str_num;
    ushort text_len;
    char   text[ USER_HOLO_MAX_LEN + 1 ];
    Bin >> msg_len;
    Bin >> str_num;
    Bin >> text_len;

    if( text_len > USER_HOLO_MAX_LEN )
    {
        WriteLogF( _FUNC_, " - Text length greater than maximum, cur<%u>, max<%u>. Disconnect.\n", text_len, USER_HOLO_MAX_LEN );
        NetDisconnect();
        return;
    }

    Bin.Pop( text, text_len );
    text[ text_len ] = '\0';

    CHECK_IN_BUFF_ERROR;

    if( MsgUserHolo->Count( str_num ) )
        MsgUserHolo->EraseStr( str_num );
    MsgUserHolo->AddStr( str_num, text );
    MsgUserHolo->SaveToFile( USER_HOLO_TEXTMSG_FILE, PT_TEXTS );
}

void FOClient::Net_OnAutomapsInfo()
{
    uint   msg_len;
    bool   clear;
    ushort locs_count;
    Bin >> msg_len;
    Bin >> clear;

    if( clear )
    {
        Automaps.clear();
        AutomapWaitPids.clear();
        AutomapReceivedPids.clear();
        AutomapPoints.clear();
        AutomapCurMapPid = 0;
        AutomapScrX = 0.0f;
        AutomapScrY = 0.0f;
        AutomapZoom = 1.0f;
    }

    Bin >> locs_count;
    for( ushort i = 0; i < locs_count; i++ )
    {
        uint   loc_id;
        hash   loc_pid;
        ushort maps_count;
        Bin >> loc_id;
        Bin >> loc_pid;
        Bin >> maps_count;

        auto it = std::find( Automaps.begin(), Automaps.end(), loc_id );

        // Delete from collection
        if( !maps_count )
        {
            if( it != Automaps.end() )
                Automaps.erase( it );
        }
        // Add or modify
        else
        {
            Automap amap;
            amap.LocId = loc_id;
            amap.LocPid = loc_pid;
            amap.LocName = MsgLocations->GetStr( STR_LOC_NAME( loc_pid ) );

            for( ushort j = 0; j < maps_count; j++ )
            {
                hash  map_pid;
                uchar map_index_in_loc;
                Bin >> map_pid;
                Bin >> map_index_in_loc;

                amap.MapPids.push_back( map_pid );
                amap.MapNames.push_back( MsgLocations->GetStr( STR_LOC_MAP_NAME( loc_pid, map_index_in_loc ) ) );
            }

            if( it != Automaps.end() )
                *it = amap;
            else
                Automaps.push_back( amap );
        }
    }

    CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnCheckUID4()
{
    CHECKUIDBIN;
    if( CHECK_UID2( uid ) )
        Net_SendPing( PING_UID_FAIL );
}

void FOClient::Net_OnViewMap()
{
    ushort hx, hy;
    uint   loc_id, loc_ent;
    Bin >> hx;
    Bin >> hy;
    Bin >> loc_id;
    Bin >> loc_ent;

    CHECK_IN_BUFF_ERROR;

    if( !HexMngr.IsMapLoaded() )
        return;

    HexMngr.FindSetCenter( hx, hy );
    SndMngr.PlayAmbient( MsgLocations->GetStr( STR_LOC_MAP_AMBIENT( CurMapLocPid, CurMapIndexInLoc ) ) );
    ShowMainScreen( SCREEN_GAME );
    ScreenFadeOut();
    HexMngr.RebuildLight();
    ShowScreen( SCREEN__TOWN_VIEW );

    if( loc_id )
    {
        TViewType = TOWN_VIEW_FROM_GLOBAL;
        TViewGmapLocId = loc_id;
        TViewGmapLocEntrance = loc_ent;
    }
}

void FOClient::Net_OnCraftAsk()
{
    uint   msg_len;
    ushort count;
    Bin >> msg_len;
    Bin >> count;

    FixShowCraft.clear();
    for( int i = 0; i < count; i++ )
    {
        uint craft_num;
        Bin >> craft_num;
        FixShowCraft.insert( craft_num );
    }

    CHECK_IN_BUFF_ERROR;

    if( IsScreenPresent( SCREEN__FIX_BOY ) && FixMode == FIX_MODE_LIST )
        FixGenerate( FIX_MODE_LIST );
}

void FOClient::Net_OnCraftResult()
{
    uchar craft_result;
    Bin >> craft_result;

    CHECK_IN_BUFF_ERROR;

    if( craft_result != CRAFT_RESULT_NONE )
    {
        FixResult = craft_result;
        FixGenerate( FIX_MODE_RESULT );
    }
}

void FOClient::SetGameColor( uint color )
{
    SprMngr.SetSpritesColor( color );
    HexMngr.RefreshMap();
}

bool FOClient::IsCurInInterface( int x, int y )
{
    if( ClientFunctions.CheckInterfaceHit && Script::PrepareContext( ClientFunctions.CheckInterfaceHit, _FUNC_, "Game" ) )
    {
        Script::SetArgUInt( x );
        Script::SetArgUInt( y );
        if( Script::RunPrepared() )
            return Script::GetReturnedBool();
    }
    return false;
}

bool FOClient::GetCurHex( ushort& hx, ushort& hy, bool ignore_interface )
{
    hx = hy = 0;
    if( !ignore_interface && IsCurInInterface( GameOpt.MouseX, GameOpt.MouseY ) )
        return false;
    return HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy );
}

bool FOClient::RegCheckData()
{
    // Name
    uint name_len_utf8 = Str::LengthUTF8( GameOpt.RegName->c_str() );
    if( name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength ||
        name_len_utf8 > MAX_NAME || name_len_utf8 > GameOpt.MaxNameLength )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_WRONG_NAME ) );
        return false;
    }

    if( !Str::IsValidUTF8( GameOpt.RegName->c_str() ) || Str::Substring( GameOpt.RegName->c_str(), "*" ) )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_NAME_WRONG_CHARS ) );
        return false;
    }

    // Password
    if( !Singleplayer )
    {
        uint pass_len_utf8 = Str::LengthUTF8( GameOpt.RegPassword->c_str() );
        if( pass_len_utf8 < MIN_NAME || pass_len_utf8 < GameOpt.MinNameLength ||
            pass_len_utf8 > MAX_NAME || pass_len_utf8 > GameOpt.MaxNameLength )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_WRONG_PASS ) );
            return false;
        }

        if( !Str::IsValidUTF8( GameOpt.RegPassword->c_str() ) || Str::Substring( GameOpt.RegPassword->c_str(), "*" ) )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_PASS_WRONG_CHARS ) );
            return false;
        }
    }

    return true;
}

void FOClient::SetDayTime( bool refresh )
{
    if( !HexMngr.IsMapLoaded() )
        return;

    static uint old_color = -1;
    uint        color = GetColorDay( HexMngr.GetMapDayTime(), HexMngr.GetMapDayColor(), HexMngr.GetMapTime(), NULL );
    color = COLOR_GAME_RGB( ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF );

    if( refresh )
        old_color = -1;
    if( old_color != color )
    {
        old_color = color;
        SetGameColor( old_color );
    }
}

Item* FOClient::GetTargetContItem()
{
    if( !TargetSmth.IsContItem() )
        return NULL;

    // Script stuff
    if( TargetSmth.GetParam() > 1 )
    {
        uint  item_id = TargetSmth.GetId();
        Item* item = NULL;
        if( TargetSmth.GetParam() == 2 )
        {
            if( Chosen )
                item = Chosen->GetItem( item_id );
        }
        else
        {
            CritMap& critters = HexMngr.GetCritters();
            for( auto it = critters.begin(), end = critters.end(); it != end && !item; ++it )
                item = ( *it ).second->GetItem( item_id );
        }
        if( !item )
            TargetSmth.Clear();
        return item;
    }

    // Hardcoded stuff
    #define TRY_SEARCH_IN_CONT( cont )                                        \
        do {                                                                  \
            auto it = PtrCollectionFind( cont.begin(), cont.end(), item_id ); \
            if( it != cont.end() )                                            \
                return *it;                                                   \
        }                                                                     \
        while( 0 )

    uint item_id = TargetSmth.GetId();
    if( GetActiveScreen() != SCREEN_NONE )
    {
        switch( GetActiveScreen() )
        {
        case SCREEN__USE:
            TRY_SEARCH_IN_CONT( UseCont );
            break;
        case SCREEN__PICKUP:
            TRY_SEARCH_IN_CONT( PupCont1 );
            TRY_SEARCH_IN_CONT( PupCont2 );
            break;
        case SCREEN__BARTER:
            if( TargetSmth.GetParam() == 0 )
                TRY_SEARCH_IN_CONT( BarterCont1 );
            if( TargetSmth.GetParam() == 0 )
                TRY_SEARCH_IN_CONT( BarterCont2 );
            if( TargetSmth.GetParam() == 1 )
                TRY_SEARCH_IN_CONT( BarterCont1o );
            if( TargetSmth.GetParam() == 1 )
                TRY_SEARCH_IN_CONT( BarterCont2o );
            break;
        default:
            break;
        }
    }
    TargetSmth.Clear();
    return NULL;
}

void FOClient::AddAction( bool to_front, ActionEvent act )
{
    for( auto it = ChosenAction.begin(); it != ChosenAction.end();)
    {
        ActionEvent& a = *it;
        if( a == act )
            it = ChosenAction.erase( it );
        else
            ++it;
    }

    if( to_front )
        ChosenAction.insert( ChosenAction.begin(), act );
    else
        ChosenAction.push_back( act );
}

void FOClient::SetAction( max_t type_action, max_t param0, max_t param1, max_t param2, max_t param3, max_t param4, max_t param5 )
{
    SetAction( ActionEvent( type_action, param0, param1, param2, param3, param4, param5 ) );
}

void FOClient::SetAction( ActionEvent act )
{
    ChosenAction.clear();
    AddActionBack( act );
}

void FOClient::AddActionBack( max_t type_action, max_t param0, max_t param1, max_t param2, max_t param3, max_t param4, max_t param5 )
{
    AddActionBack( ActionEvent( type_action, param0, param1, param2, param3, param4, param5 ) );
}

void FOClient::AddActionBack( ActionEvent act )
{
    AddAction( false, act );
}

void FOClient::AddActionFront( max_t type_action, max_t param0, max_t param1, max_t param2, max_t param3, max_t param4, max_t param5 )
{
    AddAction( true, ActionEvent( type_action, param0, param1, param2, param3, param4, param5 ) );
}

void FOClient::EraseFrontAction()
{
    if( ChosenAction.size() )
        ChosenAction.erase( ChosenAction.begin() );
}

void FOClient::EraseBackAction()
{
    if( ChosenAction.size() )
        ChosenAction.pop_back();
}

bool FOClient::IsAction( max_t type_action )
{
    return ChosenAction.size() && ( *ChosenAction.begin() ).Type == type_action;
}

void FOClient::ChosenChangeSlot()
{
    if( !Chosen )
        return;

    if( Chosen->ItemSlotExt->GetId() )
        AddActionBack( CHOSEN_MOVE_ITEM, Chosen->ItemSlotExt->GetId(), Chosen->ItemSlotExt->GetCount(), SLOT_HAND1 );
    else if( Chosen->ItemSlotMain->GetId() )
        AddActionBack( CHOSEN_MOVE_ITEM, Chosen->ItemSlotMain->GetId(), Chosen->ItemSlotMain->GetCount(), SLOT_HAND2 );
    else
    {
        uchar      tree = Chosen->DefItemSlotHand->Proto->GetWeapon_UnarmedTree() + 1;
        ProtoItem* unarmed = Chosen->GetUnarmedItem( tree, 0 );
        if( !unarmed )
            unarmed = Chosen->GetUnarmedItem( 0, 0 );
        Chosen->DefItemSlotHand->SetProto( unarmed );
    }
}

void FOClient::WaitPing()
{
    SetCurMode( CUR_WAIT );
    Net_SendPing( PING_WAIT );
}

void FOClient::CrittersProcess()
{
/************************************************************************/
/* All critters                                                         */
/************************************************************************/
    for( auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end();)
    {
        CritterCl* crit = ( *it ).second;
        ++it;

        crit->Process();

        if( crit->IsNeedReSet() )
        {
            HexMngr.RemoveCrit( crit );
            HexMngr.SetCrit( crit );
            crit->ReSetOk();
        }

        if( !crit->IsChosen() && crit->IsNeedMove() && HexMngr.TransitCritter( crit, crit->MoveSteps[ 0 ].first, crit->MoveSteps[ 0 ].second, true, crit->CurMoveStep > 0 ) )
        {
            crit->MoveSteps.erase( crit->MoveSteps.begin() );
            crit->CurMoveStep--;
        }

        if( crit->IsFinish() )
        {
            EraseCritter( crit->GetId() );
        }
    }

/************************************************************************/
/* Chosen                                                               */
/************************************************************************/
    if( !Chosen )
        return;

    if( IsMainScreen( SCREEN_GAME ) )
    {
        // Roof Visible
        static int last_hx = 0;
        static int last_hy = 0;

        if( last_hx != Chosen->GetHexX() || last_hy != Chosen->GetHexY() )
        {
            last_hx = Chosen->GetHexX();
            last_hy = Chosen->GetHexY();
            HexMngr.SetSkipRoof( last_hx, last_hy );
        }

        // Hide Mode
        if( Chosen->GetIsHide() )
            Chosen->Alpha = 0x82;
        else
            Chosen->Alpha = 0xFF;
    }

    // Actions
    if( !Chosen->IsFree() )
        return;

    // Game pause
    if( Timer::IsGamePaused() )
        return;

    // Ap regeneration
    if( Chosen->GetCurrentAp() / AP_DIVIDER < Chosen->GetActionPoints() && !IsTurnBased )
    {
        uint tick = Timer::GameTick();
        if( !Chosen->ApRegenerationTick )
            Chosen->ApRegenerationTick = tick;
        else
        {
            uint delta = tick - Chosen->ApRegenerationTick;
            if( delta >= 500 )
            {
                int max_ap = Chosen->GetActionPoints() * AP_DIVIDER;
                Chosen->SetCurrentAp( Chosen->GetCurrentAp() + max_ap * delta / GameOpt.ApRegeneration );
                if( Chosen->GetCurrentAp() > max_ap )
                    Chosen->SetCurrentAp( max_ap );
                Chosen->ApRegenerationTick = tick;
            }
        }
    }
    if( Chosen->GetCurrentAp() / AP_DIVIDER > Chosen->GetActionPoints() )
        Chosen->SetCurrentAp( Chosen->GetActionPoints() * AP_DIVIDER );

    if( ChosenAction.empty() )
        return;

    if( !Chosen->IsLife() || ( IsTurnBased && !IsTurnBasedMyTurn() ) )
    {
        ChosenAction.clear();
        Chosen->AnimateStay();
        return;
    }

    ActionEvent act = ChosenAction[ 0 ];
    #define CHECK_NEED_AP( need_ap )                                                         \
        {                                                                                    \
            if( IsTurnBased && !IsTurnBasedMyTurn() )                                        \
                break;                                                                       \
            if( Chosen->GetActionPoints() < (int) ( need_ap ) )                              \
            {                                                                                \
                AddMess( FOMB_GAME, FmtCombatText( STR_COMBAT_NEED_AP, need_ap ) );          \
                break;                                                                       \
            }                                                                                \
            if( Chosen->GetCurrentAp() / AP_DIVIDER < (int) ( need_ap ) )                    \
            {                                                                                \
                if( IsTurnBased )                                                            \
                {                                                                            \
                    if( Chosen->GetCurrentAp() / AP_DIVIDER )                                \
                        AddMess( FOMB_GAME, FmtCombatText( STR_COMBAT_NEED_AP, need_ap ) );  \
                    break;                                                                   \
                }                                                                            \
                else                                                                         \
                {                                                                            \
                    return;                                                                  \
                }                                                                            \
            }                                                                                \
        }
    #define CHECK_NEED_REAL_AP( need_ap )                                                                     \
        {                                                                                                     \
            if( IsTurnBased && !IsTurnBasedMyTurn() )                                                         \
                break;                                                                                        \
            if( Chosen->GetActionPoints() * AP_DIVIDER < (int) ( need_ap ) )                                  \
            {                                                                                                 \
                AddMess( FOMB_GAME, FmtCombatText( STR_COMBAT_NEED_AP, ( need_ap ) / AP_DIVIDER ) );          \
                break;                                                                                        \
            }                                                                                                 \
            if( Chosen->GetRealAp() < (int) ( need_ap ) )                                                     \
            {                                                                                                 \
                if( IsTurnBased )                                                                             \
                {                                                                                             \
                    if( Chosen->GetRealAp() )                                                                 \
                        AddMess( FOMB_GAME, FmtCombatText( STR_COMBAT_NEED_AP, ( need_ap ) / AP_DIVIDER ) );  \
                    break;                                                                                    \
                }                                                                                             \
                else                                                                                          \
                {                                                                                             \
                    return;                                                                                   \
                }                                                                                             \
            }                                                                                                 \
        }

    // Force end move
    if( act.Type != CHOSEN_MOVE && act.Type != CHOSEN_MOVE_TO_CRIT && MoveDirs.size() )
    {
        MoveLastHx = -1;
        MoveLastHy = -1;
        MoveDirs.clear();
        Chosen->MoveSteps.clear();
        if( !Chosen->IsAnim() )
            Chosen->AnimateStay();
        Net_SendMove( MoveDirs );
    }

    switch( act.Type )
    {
    case CHOSEN_NONE:
        break;
    case CHOSEN_MOVE_TO_CRIT:
    case CHOSEN_MOVE:
    {
        ushort hx = (ushort) act.Param[ 0 ];
        ushort hy = (ushort) act.Param[ 1 ];
        bool   is_run = ( act.Param[ 2 ] != 0 );
        int    cut = (int) act.Param[ 3 ];
        bool   wait_click = ( act.Param[ 4 ] != 0 );
        uint   start_tick = (uint) act.Param[ 5 ];

        ushort from_hx = 0;
        ushort from_hy = 0;
        int    ap_cost_real = 0;
        bool   skip_find = false;

        if( !HexMngr.IsMapLoaded() )
            break;

        if( act.Type == CHOSEN_MOVE_TO_CRIT )
        {
            CritterCl* cr = GetCritter( (uint) act.Param[ 0 ] );
            if( !cr )
                goto label_EndMove;
            hx = cr->GetHexX();
            hy = cr->GetHexY();
        }

        from_hx = Chosen->GetHexX();
        from_hy = Chosen->GetHexY();

        if( Chosen->IsDoubleOverweight() )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_OVERWEIGHT_TITLE ) );
            SetAction( CHOSEN_NONE );
            goto label_EndMove;
        }

        if( CheckDist( from_hx, from_hy, hx, hy, cut ) )
            goto label_EndMove;

        if( !GameOpt.RunOnCombat && IS_TIMEOUT( Chosen->GetTimeoutBattle() ) )
            is_run = false;
        else if( !GameOpt.RunOnTransfer && IS_TIMEOUT( Chosen->GetTimeoutTransfer() ) )
            is_run = false;
        else if( !GameOpt.RunOnTurnBased && IsTurnBased )
            is_run = false;
        else if( Chosen->IsDmgLeg() || Chosen->IsOverweight() )
            is_run = false;
        else if( wait_click && Timer::GameTick() - start_tick < ( GameOpt.DoubleClickTime ? GameOpt.DoubleClickTime : GetDoubleClickTicks() ) )
            return;
        else if( is_run && !IsTurnBased && Chosen->GetApCostCritterMove( is_run ) > 0 && Chosen->GetRealAp() < ( GameOpt.RunModMul * Chosen->GetActionPoints() * AP_DIVIDER ) / GameOpt.RunModDiv + GameOpt.RunModAdd )
            is_run = false;

        if( is_run && !CritType::IsCanRun( Chosen->GetCrType() ) )
            is_run = false;
        if( is_run && Chosen->GetIsNoRun() )
            is_run = false;

        if( !is_run && ( !CritType::IsCanWalk( Chosen->GetCrType() ) || Chosen->GetIsNoWalk() ) )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_CRITTER_CANT_MOVE ) );
            SetAction( CHOSEN_NONE );
            goto label_EndMove;
        }

        ap_cost_real = Chosen->GetApCostCritterMove( is_run );
        if( !( IsTurnBasedMyTurn() && Chosen->GetAllAp() >= ap_cost_real / AP_DIVIDER ) )
            CHECK_NEED_REAL_AP( ap_cost_real );
        Chosen->IsRunning = is_run;

        if( hx == MoveLastHx && hy == MoveLastHy && MoveDirs.size() )
        {
            ushort hx_ = from_hx;
            ushort hy_ = from_hy;
            MoveHexByDir( hx_, hy_, MoveDirs[ 0 ], HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
            if( !HexMngr.GetField( hx_, hy_ ).Flags.IsNotPassed )
                skip_find = true;
        }

        // Find steps
        if( !skip_find )
        {
            // MoveDirs.resize(MIN(MoveDirs.size(),4)); // TODO:
            MoveDirs.clear();
            if( !IsTurnBased && MoveDirs.size() )
            {
                ushort hx_ = from_hx;
                ushort hy_ = from_hy;
                MoveHexByDir( hx_, hy_, MoveDirs[ 0 ], HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
                if( !HexMngr.GetField( hx_, hy_ ).Flags.IsNotPassed )
                {
                    for( uint i = 1; i < MoveDirs.size() && i < 4; i++ )
                        MoveHexByDir( hx_, hy_, MoveDirs[ i ], HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
                    from_hx = hx_;
                    from_hy = hy_;
                }
                else
                    MoveDirs.clear();
            }
            else
                MoveDirs.clear();

            ushort hx_ = hx;
            ushort hy_ = hy;
            bool   result = HexMngr.CutPath( Chosen, from_hx, from_hy, hx_, hy_, cut );
            if( !result )
            {
                int max_cut = (int) DistGame( from_hx, from_hy, hx_, hy_ );
                if( max_cut > cut + 1 )
                    result = HexMngr.CutPath( Chosen, from_hx, from_hy, hx_, hy_, ++cut );
                if( !result )
                    goto label_EndMove;
            }

            ChosenAction[ 0 ].Param[ 3 ] = cut;
            UCharVec steps;
            if( HexMngr.FindPath( Chosen, from_hx, from_hy, hx_, hy_, steps, -1 ) )
            {
                for( uint i = 0, j = (uint) steps.size(); i < j; i++ )
                    MoveDirs.push_back( steps[ i ] );
            }
        }

label_EndMove:
        if( IsTurnBased && ap_cost_real > 0 && (int) MoveDirs.size() / ( ap_cost_real / AP_DIVIDER ) > Chosen->GetCurrentAp() / AP_DIVIDER + Chosen->GetMoveAp() )
            MoveDirs.resize( Chosen->GetCurrentAp() / AP_DIVIDER + Chosen->GetMoveAp() );
        if( !IsTurnBased && ap_cost_real > 0 && (int) MoveDirs.size() > Chosen->GetRealAp() / ap_cost_real )
            MoveDirs.resize( Chosen->GetRealAp() / ap_cost_real );
        if( MoveDirs.size() > 1 && Chosen->IsDmgTwoLeg() )
            MoveDirs.resize( 1 );

        // Transit
        if( MoveDirs.size() )
        {
            ushort hx_ = Chosen->GetHexX();
            ushort hy_ = Chosen->GetHexY();
            MoveHexByDir( hx_, hy_, MoveDirs[ 0 ], HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
            HexMngr.TransitCritter( Chosen, hx_, hy_, true, false );
            if( IsTurnBased )
            {
                int ap_cost = ap_cost_real / AP_DIVIDER;
                int move_ap = Chosen->GetMoveAp();
                if( move_ap )
                {
                    if( ap_cost > move_ap )
                    {
                        Chosen->SubMoveAp( move_ap );
                        Chosen->SubAp( ap_cost - move_ap );
                    }
                    else
                    {
                        Chosen->SubMoveAp( ap_cost );
                    }
                }
                else
                {
                    Chosen->SubAp( ap_cost );
                }
            }
            else if( IS_TIMEOUT( Chosen->GetTimeoutBattle() ) )
            {
                Chosen->SetCurrentAp( Chosen->GetCurrentAp() - ap_cost_real );
            }
            Chosen->ApRegenerationTick = 0;
            HexMngr.SetCursorPos( GameOpt.MouseX, GameOpt.MouseY, Keyb::CtrlDwn, true );
            RebuildLookBorders = true;
        }

        // Send about move
        Net_SendMove( MoveDirs );
        if( MoveDirs.size() )
            MoveDirs.erase( MoveDirs.begin() );

        // End move
        if( MoveDirs.empty() )
        {
            MoveLastHx = -1;
            MoveLastHy = -1;
            Chosen->MoveSteps.clear();
            if( !Chosen->IsAnim() )
                Chosen->AnimateStay();
        }
        // Continue move
        else
        {
            MoveLastHx = hx;
            MoveLastHy = hy;
            Chosen->MoveSteps.resize( 1 );
            return;
        }
    }
    break;
    case CHOSEN_DIR:
    {
        int cw = (int) act.Param[ 0 ];

        if( !HexMngr.IsMapLoaded() || ( IsTurnBased && !IsTurnBasedMyTurn() ) )
            break;

        int dir = Chosen->GetDir();
        if( cw == 0 )
        {
            dir++;
            if( dir >= DIRS_COUNT )
                dir = 0;
        }
        else
        {
            dir--;
            if( dir < 0 )
                dir = DIRS_COUNT - 1;
        }
        Chosen->SetDir( dir );
        Net_SendDir();
    }
    break;
    case CHOSEN_USE_ITEM:
    {
        uint  item_id = (uint) act.Param[ 0 ];
        hash  item_pid = (hash) act.Param[ 1 ];
        uchar target_type = (uchar) act.Param[ 2 ];
        uint  target_id = (uint) act.Param[ 3 ];
        uchar rate = (uchar) act.Param[ 4 ];
        uint  param = (uint) act.Param[ 5 ];
        uchar use = ( rate & 0xF );
        uchar aim = ( rate >> 4 );

        // Find item
        Item* item = ( item_id ? Chosen->GetItem( item_id ) : Chosen->DefItemSlotHand );
        if( !item )
            break;
        bool is_main_item = ( item == Chosen->ItemSlotMain );

        // Check proto item
        if( !item_pid )
            item_pid = item->GetProtoId();
        ProtoItem* proto_item = ItemMngr.GetProtoItem( item_pid );
        if( !proto_item )
            break;
        if( item->GetProtoId() != item_pid )             // Unarmed proto
            item->SetProto( proto_item );

        // Find target
        CritterCl* target_cr = NULL;
        ItemHex*   target_item = NULL;
        Item*      target_item_self = NULL;
        if( target_type == TARGET_SELF )
            target_cr = Chosen;
        else if( target_type == TARGET_SELF_ITEM )
            target_item_self = Chosen->GetItem( target_id );
        else if( target_type == TARGET_CRITTER )
            target_cr = GetCritter( target_id );
        else if( target_type == TARGET_ITEM )
            target_item = GetItem( target_id );
        else if( target_type == TARGET_SCENERY )
            target_item = GetItem( target_id );
        else
            break;
        if( target_type == TARGET_CRITTER && Chosen == target_cr )
            target_type = TARGET_SELF;
        if( target_type == TARGET_SELF )
            target_id = Chosen->GetId();

        // Check
        if( target_type == TARGET_CRITTER && !target_cr )
            break;
        else if( target_type == TARGET_ITEM && !target_item )
            break;
        else if( target_type == TARGET_SCENERY && !target_item )
            break;
        if( target_type != TARGET_CRITTER && !item->GetId() )
            break;

        // Parse use
        bool is_attack = ( target_type == TARGET_CRITTER && is_main_item && item->IsWeapon() && use < MAX_USES );
        bool is_reload = ( target_type == TARGET_SELF_ITEM && use == USE_RELOAD && item->IsWeapon() );
        bool is_self = ( target_type == TARGET_SELF || target_type == TARGET_SELF_ITEM );
        if( !is_attack && !is_reload && ( IsCurMode( CUR_USE_ITEM ) || IsCurMode( CUR_USE_WEAPON ) ) )
            SetCurMode( CUR_DEFAULT );

        // Aim overriding
        if( is_attack && ClientFunctions.HitAim && Script::PrepareContext( ClientFunctions.HitAim, _FUNC_, "Game" ) )
        {
            uchar new_aim = aim;
            Script::SetArgAddress( &new_aim );
            if( Script::RunPrepared() && new_aim != aim )
            {
                aim = new_aim;
                rate = MAKE_ITEM_MODE( use, aim );
            }
        }

        // Calculate ap cost
        int ap_cost = Chosen->GetUseApCost( item, rate );

        // Check weapon
        if( is_attack )
        {
            if( !HexMngr.IsMapLoaded() )
                break;
            if( is_self )
                break;
            if( !item->IsWeapon() )
                break;
            if( target_cr->IsDead() )
                break;
            if( !Chosen->GetIsUnlimitedAmmo() && item->WeapGetMaxAmmoCount() && item->WeapIsEmpty() )
            {
                SndMngr.PlaySoundType( 'W', 'O', use == 0 ? item->Proto->GetWeapon_SoundId_0() : ( use == 1 ? item->Proto->GetWeapon_SoundId_1() : item->Proto->GetWeapon_SoundId_2() ), '1' );
                break;
            }
            if( item->IsTwoHands() && Chosen->IsDmgArm() )
            {
                AddMess( FOMB_GAME, FmtCombatText( STR_COMBAT_NEED_DMG_ARM ) );
                break;
            }
            if( Chosen->IsDmgTwoArm() && item->GetId() )
            {
                AddMess( FOMB_GAME, FmtCombatText( STR_COMBAT_NEED_DMG_TWO_ARMS ) );
                break;
            }
            if( item->IsDeteriorable() && item->IsBroken() )
            {
                AddMess( FOMB_GAME, FmtGameText( STR_INV_WEAR_WEAPON_BROKEN ) );
                break;
            }
        }
        else if( is_reload )
        {
            if( !is_self )
                break;

            CHECK_NEED_AP( ap_cost );

            if( is_main_item )
            {
                item->SetWeaponMode( USE_PRIMARY );
                if( GetActiveScreen() == SCREEN_NONE )
                    SetCurMode( CUR_USE_WEAPON );
            }

            if( !item->WeapGetMaxAmmoCount() )
                break;                            // No have holder

            if( target_id == uint( -1 ) )         // Unload
            {
                if( item->WeapIsEmpty() )
                    break;                        // Is empty
                target_id = 0;
            }
            else if( !target_item_self )          // Reload
            {
                if( item->WeapIsFull() )
                    break;                        // Is full
                if( item->WeapGetAmmoCaliber() )
                {
                    target_item_self = Chosen->GetAmmoAvialble( item );
                    if( !target_item_self )
                        break;
                    target_id = target_item_self->GetId();
                }
            }
            else                // Load
            {
                if( item->WeapGetAmmoCaliber() != target_item_self->AmmoGetCaliber() )
                    break;      // Different caliber
                if( item->WeapGetAmmoPid() == target_item_self->GetProtoId() && item->WeapIsFull() )
                    break;      // Is full
            }
        }
        else                    // Use
        {
            if( use != USE_USE )
                break;
        }

        // Find Target
        if( !is_self && HexMngr.IsMapLoaded() )
        {
            ushort hx, hy;
            if( target_cr )
            {
                hx = target_cr->GetHexX();
                hy = target_cr->GetHexY();
            }
            else
            {
                hx = target_item->GetHexX();
                hy = target_item->GetHexY();
            }

            uint max_dist = ( is_attack ? Chosen->GetAttackDist() : Chosen->GetUseDist() ) + ( target_cr ? target_cr->GetMultihex() : 0 );

            // Target find
            bool need_move = false;
            if( is_attack )
                need_move = !HexMngr.TraceBullet( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, max_dist, 0.0f, target_cr, false, NULL, 0, NULL, NULL, NULL, true );
            else
                need_move = !CheckDist( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, max_dist );
            if( need_move )
            {
                // If target too far, then move to it
                uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy );
                if( dist > max_dist )
                {
                    if( IsTurnBased )
                        break;
                    bool is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
                    if( target_cr )
                        SetAction( CHOSEN_MOVE_TO_CRIT, target_cr->GetId(), 0, is_run, max_dist );
                    else
                        SetAction( CHOSEN_MOVE, hx, hy, is_run, max_dist, 0 );
                    if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, max_dist ) )
                        AddActionBack( act );
                    return;
                }
                AddMess( FOMB_GAME, MsgGame->GetStr( STR_FINDPATH_AIMBLOCK ) );
                break;
            }

            // Refresh orientation
            CHECK_NEED_AP( ap_cost );
            uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy );
            if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy ) >= 1 && Chosen->GetDir() != dir )
            {
                Chosen->SetDir( dir );
                Net_SendDir();
            }
        }

        // Use
        CHECK_NEED_AP( ap_cost );

        if( target_item && target_item->IsScenOrGrid() )
            Net_SendUseItem( ap_cost, item_id, item_pid, rate, target_type, ( target_item->GetHexX() << 16 ) | ( target_item->GetHexY() & 0xFFFF ), target_item->GetProtoId(), param );
        else
            Net_SendUseItem( ap_cost, item_id, item_pid, rate, target_type, target_id, 0, param );  // Item or critter

        int usei = use;                                                                             // Avoid 'warning: comparison is always true due to limited range of data type' for GCC
        if( usei >= USE_PRIMARY && use <= USE_THIRD )
            Chosen->Action( ACTION_USE_WEAPON, rate, item );
        else if( use == USE_RELOAD )
            Chosen->Action( ACTION_RELOAD_WEAPON, 0, item );
        else
            Chosen->Action( ACTION_USE_ITEM, 0, item );

        Chosen->SubAp( ap_cost );

        if( is_attack && !aim && Keyb::ShiftDwn )               // Continue battle after attack
        {
            AddActionBack( act );
            return;
        }
    }
    break;
    case CHOSEN_MOVE_ITEM:
    {
        uint  item_id = (uint) act.Param[ 0 ];
        uint  item_count = (uint) act.Param[ 1 ];
        int   to_slot = (int) act.Param[ 2 ];
        bool  is_barter_cont = ( act.Param[ 3 ] != 0 );
        bool  is_second_try = ( act.Param[ 4 ] != 0 );

        Item* item = Chosen->GetItem( item_id );
        if( !item )
            break;

        uchar from_slot = item->AccCritter.Slot;
        if( from_slot == to_slot )
            break;
        if( to_slot != SLOT_GROUND && !CritterCl::SlotEnabled[ to_slot ] )
            break;

        Item* item_swap = ( ( to_slot != SLOT_INV && to_slot != SLOT_GROUND ) ? Chosen->GetItemSlot( to_slot ) : NULL );
        bool  allow = false;
        if( Script::PrepareContext( ClientFunctions.CritterCheckMoveItem, _FUNC_, "Game" ) )
        {
            Script::SetArgObject( Chosen );
            Script::SetArgObject( item );
            Script::SetArgUChar( to_slot );
            Script::SetArgObject( item_swap );
            if( Script::RunPrepared() )
                allow = Script::GetReturnedBool();
        }
        if( !allow )
        {
            // Gameplay swap workaround
            if( item_swap && !is_second_try )
            {
                // Hindering item to inventory
                bool allow1 = false;
                if( Script::PrepareContext( ClientFunctions.CritterCheckMoveItem, _FUNC_, "Game" ) )
                {
                    Script::SetArgObject( Chosen );
                    Script::SetArgObject( item_swap );
                    Script::SetArgUChar( SLOT_INV );
                    Script::SetArgObject( NULL );
                    if( Script::RunPrepared() )
                        allow1 = Script::GetReturnedBool();
                }

                // Current item to empty slot
                bool allow2 = false;
                if( allow1 && Script::PrepareContext( ClientFunctions.CritterCheckMoveItem, _FUNC_, "Game" ) )
                {
                    Script::SetArgObject( Chosen );
                    Script::SetArgObject( item );
                    Script::SetArgUChar( to_slot );
                    Script::SetArgObject( NULL );
                    if( Script::RunPrepared() )
                        allow2 = Script::GetReturnedBool();
                }

                // Add actions
                if( allow1 && allow2 )
                {
                    EraseFrontAction();
                    AddActionFront( CHOSEN_MOVE_ITEM, item_id, item_count, to_slot, is_barter_cont, true );                        // Second
                    AddActionFront( CHOSEN_MOVE_ITEM, item_swap->GetId(), item_swap->GetCount(), SLOT_INV, is_barter_cont, true ); // First
                    return;
                }
            }
            break;
        }

        // Action points
        bool is_castling = ( ( from_slot == SLOT_HAND1 && to_slot == SLOT_HAND2 ) || ( from_slot == SLOT_HAND2 && to_slot == SLOT_HAND1 ) );
        uint ap_cost = ( is_castling ? 0 : Chosen->GetApCostMoveItemInventory() );
        if( to_slot == SLOT_GROUND )
            ap_cost = Chosen->GetApCostDropItem();
        CHECK_NEED_AP( ap_cost );

        // Store old copy
        Item* old_item = item->Clone();

        // Move
        if( to_slot == SLOT_GROUND )
        {
            Chosen->Action( ACTION_DROP_ITEM, from_slot, item );
            if( item_count < item->GetCount() )
                item->ChangeCount( -(int) item_count );
            else
                Chosen->EraseItem( item, true );
        }
        else
        {
            if( from_slot == SLOT_HAND1 || to_slot == SLOT_HAND1 )
                Chosen->ItemSlotMain = Chosen->DefItemSlotHand;
            if( from_slot == SLOT_HAND2 || to_slot == SLOT_HAND2 )
                Chosen->ItemSlotExt = Chosen->DefItemSlotHand;
            if( from_slot == SLOT_ARMOR || to_slot == SLOT_ARMOR )
                Chosen->ItemSlotArmor = Chosen->DefItemSlotArmor;

            if( to_slot == SLOT_HAND1 )
                Chosen->ItemSlotMain = item;
            else if( to_slot == SLOT_HAND2 )
                Chosen->ItemSlotExt = item;
            else if( to_slot == SLOT_ARMOR )
                Chosen->ItemSlotArmor = item;
            if( item_swap )
            {
                if( from_slot == SLOT_HAND1 )
                    Chosen->ItemSlotMain = item_swap;
                else if( from_slot == SLOT_HAND2 )
                    Chosen->ItemSlotExt = item_swap;
                else if( from_slot == SLOT_ARMOR )
                    Chosen->ItemSlotArmor = item_swap;
            }

            item->AccCritter.Slot = to_slot;
            if( item_swap )
                item_swap->AccCritter.Slot = from_slot;

            Chosen->Action( ACTION_MOVE_ITEM, from_slot, item );
            if( item_swap )
                Chosen->Action( ACTION_MOVE_ITEM_SWAP, to_slot, item_swap );
        }

        // Affect barter screen
        if( to_slot == SLOT_GROUND && is_barter_cont && IsScreenPresent( SCREEN__BARTER ) )
        {
            auto it = PtrCollectionFind( BarterCont1oInit.begin(), BarterCont1oInit.end(), item_id );
            if( it != BarterCont1oInit.end() )
            {
                Item* item_ = *it;
                if( item_count >= item_->GetCount() )
                {
                    ( *it )->Release();
                    BarterCont1oInit.erase( it );
                }
                else
                {
                    item_->ChangeCount( -(int) item_count );
                }
            }
            CollectContItems();
        }

        // Light
        if( to_slot == SLOT_HAND1 || from_slot == SLOT_HAND1 )
            RebuildLookBorders = true;
        if( item->IsLight() && ( to_slot == SLOT_INV || ( from_slot == SLOT_INV && to_slot != SLOT_GROUND ) ) )
            HexMngr.RebuildLight();

        // Notice server
        Net_SendChangeItem( ap_cost, item_id, from_slot, to_slot, item_count );

        // Spend AP
        Chosen->SubAp( ap_cost );

        // Notify scripts about item changing
        OnItemInvChanged( old_item, item );
    }
    break;
    case CHOSEN_MOVE_ITEM_CONT:
    {
        uint item_id = (uint) act.Param[ 0 ];
        uint cont = (uint) act.Param[ 1 ];
        uint count = (uint) act.Param[ 2 ];

        CHECK_NEED_AP( Chosen->GetApCostMoveItemContainer() );

        Item* item = GetContainerItem( cont == IFACE_PUP_CONT1 ? InvContInit : PupCont2Init, item_id );
        if( !item )
            break;
        if( count > item->GetCount() )
            break;

        if( cont == IFACE_PUP_CONT2 )
        {
            if( Chosen->GetFreeWeight() < (int) ( item->GetWeight1st() * count ) )
            {
                AddMess( FOMB_GAME, MsgGame->GetStr( STR_OVERWEIGHT ) );
                break;
            }
            if( Chosen->GetFreeVolume() < (int) ( item->GetVolume1st() * count ) )
            {
                AddMess( FOMB_GAME, MsgGame->GetStr( STR_OVERVOLUME ) );
                break;
            }
        }

        Chosen->Action( ACTION_OPERATE_CONTAINER, PupTransferType * 10 + ( cont == IFACE_PUP_CONT2 ? 0 : 2 ), item );
        PupTransfer( item_id, cont, count );
        Chosen->SubAp( Chosen->GetApCostMoveItemContainer() );
    }
    break;
    case CHOSEN_TAKE_ALL:
    {
        CHECK_NEED_AP( Chosen->GetApCostMoveItemContainer() );

        if( PupCont2Init.empty() )
            break;

        uint c, w, v;
        ContainerCalcInfo( PupCont2Init, c, w, v, MAX_INT, false );
        if( Chosen->GetFreeWeight() < (int) w )
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_BARTER_OVERWEIGHT ) );
        else if( Chosen->GetFreeVolume() < (int) v )
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_BARTER_OVERSIZE ) );
        else
        {
            Item::ClearItems( PupCont2Init );
            PupCount = 0;
            Net_SendItemCont( PupTransferType, PupContId, PupHoldId, 0, CONT_GETALL );
            Chosen->Action( ACTION_OPERATE_CONTAINER, PupTransferType * 10 + 1, NULL );
            Chosen->SubAp( Chosen->GetApCostMoveItemContainer() );
            CollectContItems();
        }
    }
    break;
    case CHOSEN_USE_SKL_ON_CRITTER:
    {
        int        skill = (int) act.Param[ 0 ];
        uint       crid = (uint) act.Param[ 1 ];

        CritterCl* cr = ( crid ? GetCritter( crid ) : Chosen );
        if( !cr )
            break;

        if( skill == CritterCl::PropertySkillSteal->GetEnumValue() && cr->GetIsNoSteal() )
            break;

        if( cr != Chosen && HexMngr.IsMapLoaded() )
        {
            // Check distantance
            uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() );
            if( skill == CritterCl::PropertySkillLockpick->GetEnumValue() || skill == CritterCl::PropertySkillSteal->GetEnumValue() ||
                skill == CritterCl::PropertySkillTraps->GetEnumValue() || skill == CritterCl::PropertySkillFirstAid->GetEnumValue() ||
                skill == CritterCl::PropertySkillDoctor->GetEnumValue() || skill == CritterCl::PropertySkillScience->GetEnumValue() ||
                skill == CritterCl::PropertySkillRepair->GetEnumValue() )
            {
                if( dist > Chosen->GetUseDist() + cr->GetMultihex() )
                {
                    if( IsTurnBased )
                        break;
                    bool   is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
                    uint   cut_dist = Chosen->GetUseDist() + cr->GetMultihex();
                    SetAction( CHOSEN_MOVE_TO_CRIT, cr->GetId(), 0, is_run, cut_dist );
                    ushort hx = cr->GetHexX(), hy = cr->GetHexY();
                    if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, cut_dist ) )
                        AddActionBack( act );
                    return;
                }
            }

            if( cr != Chosen )
            {
                // Refresh orientation
                CHECK_NEED_AP( Chosen->GetApCostUseSkill() );
                uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() );
                if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() ) >= 1 && Chosen->GetDir() != dir )
                {
                    Chosen->SetDir( dir );
                    Net_SendDir();
                }
            }
        }

        CHECK_NEED_AP( Chosen->GetApCostUseSkill() );
        Chosen->Action( ACTION_USE_SKILL, skill, NULL );
        Net_SendUseSkill( skill, cr );
        Chosen->SubAp( Chosen->GetApCostUseSkill() );
        WaitPing();
    }
    break;
    case CHOSEN_USE_SKL_ON_ITEM:
    {
        bool  is_inv = ( act.Param[ 0 ] != 0 );
        int   skill = (int) act.Param[ 1 ];
        uint  item_id = (uint) act.Param[ 2 ];

        Item* item_action;
        if( is_inv )
        {
            Item* item = Chosen->GetItem( item_id );
            if( !item )
                break;
            item_action = item;

            if( skill == CritterCl::PropertySkillScience->GetEnumValue() && item->IsHolodisk() )
            {
                ShowScreen( SCREEN__INPUT_BOX );
                IboxMode = IBOX_MODE_HOLO;
                IboxHolodiskId = item->GetId();
                IboxTitle = GetHoloText( STR_HOLO_INFO_NAME( item->GetHolodiskNumber() ) );
                IboxText = GetHoloText( STR_HOLO_INFO_DESC( item->GetHolodiskNumber() ) );
                if( IboxTitle.length() > USER_HOLO_MAX_TITLE_LEN )
                    IboxTitle.resize( USER_HOLO_MAX_TITLE_LEN );
                if( IboxText.length() > USER_HOLO_MAX_LEN )
                    IboxText.resize( USER_HOLO_MAX_LEN );
                break;
            }

            CHECK_NEED_AP( Chosen->GetApCostUseSkill() );
            Net_SendUseSkill( skill, item );
        }
        else
        {
            ItemHex* item = HexMngr.GetItemById( item_id );
            if( !item )
                break;
            item_action = item;

            if( HexMngr.IsMapLoaded() )
            {
                uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), item->GetHexX(), item->GetHexY() );
                if( dist > Chosen->GetUseDist() )
                {
                    if( IsTurnBased )
                        break;
                    bool   is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
                    SetAction( CHOSEN_MOVE, item->GetHexX(), item->GetHexY(), is_run, Chosen->GetUseDist(), 0 );
                    ushort hx = item->GetHexX(), hy = item->GetHexY();
                    if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, Chosen->GetUseDist() ) )
                        AddActionBack( act );
                    return;
                }

                // Refresh orientation
                CHECK_NEED_AP( Chosen->GetApCostUseSkill() );
                uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), item->GetHexX(), item->GetHexY() );
                if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), item->GetHexX(), item->GetHexY() ) >= 1 && Chosen->GetDir() != dir )
                {
                    Chosen->SetDir( dir );
                    Net_SendDir();
                }
            }

            CHECK_NEED_AP( Chosen->GetApCostUseSkill() );
            Net_SendUseSkill( skill, item );
        }

        Chosen->Action( ACTION_USE_SKILL, skill, item_action );
        Chosen->SubAp( Chosen->GetApCostUseSkill() );
        WaitPing();
    }
    break;
    case CHOSEN_USE_SKL_ON_SCEN:
    {
        int    skill = (int) act.Param[ 0 ];
        hash   pid = (hash) act.Param[ 1 ];
        ushort hx = (ushort) act.Param[ 2 ];
        ushort hy = (ushort) act.Param[ 3 ];

        if( !HexMngr.IsMapLoaded() )
            break;

        ItemHex* item = HexMngr.GetItem( hx, hy, pid );
        if( !item )
            break;

        uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy );
        if( dist > Chosen->GetUseDist() )
        {
            if( IsTurnBased )
                break;
            bool is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
            SetAction( CHOSEN_MOVE, hx, hy, is_run, Chosen->GetUseDist(), 0 );
            if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, Chosen->GetUseDist() ) )
                AddActionBack( act );
            return;
        }

        CHECK_NEED_AP( Chosen->GetApCostUseSkill() );

        // Refresh orientation
        uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), item->GetHexX(), item->GetHexY() );
        if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), item->GetHexX(), item->GetHexY() ) >= 1 && Chosen->GetDir() != dir )
        {
            Chosen->SetDir( dir );
            Net_SendDir();
        }

        Chosen->Action( ACTION_USE_SKILL, skill, item );
        Net_SendUseSkill( skill, item );
        Chosen->SubAp( Chosen->GetApCostUseSkill() );
        // WaitPing();
    }
    break;
    case CHOSEN_TALK_NPC:
    {
        uint crid = (uint) act.Param[ 0 ];

        if( !HexMngr.IsMapLoaded() )
            break;

        CritterCl* cr = GetCritter( crid );
        if( !cr )
            break;

        if( IsTurnBased )
            break;

        if( cr->IsDead() )
        {
            // TODO: AddMess Dead CritterCl
            break;
        }

        uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() );
        uint talk_distance = cr->GetTalkDist() + Chosen->GetMultihex();
        if( dist > talk_distance )
        {
            if( IsTurnBased )
                break;
            bool   is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
            SetAction( CHOSEN_MOVE_TO_CRIT, cr->GetId(), 0, is_run, talk_distance );
            ushort hx = cr->GetHexX(), hy = cr->GetHexY();
            if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, talk_distance ) )
                AddActionBack( act );
            return;
        }

        if( !HexMngr.TraceBullet( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY(), talk_distance + Chosen->GetMultihex(), 0.0f, cr, false, NULL, 0, NULL, NULL, NULL, true ) )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_FINDPATH_AIMBLOCK ) );
            break;
        }

        // Refresh orientation
        uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() );
        if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() ) >= 1 && Chosen->GetDir() != dir )
        {
            Chosen->SetDir( dir );
            Net_SendDir();
        }

        Net_SendTalk( true, cr->GetId(), ANSWER_BEGIN );
        WaitPing();
    }
    break;
    case CHOSEN_PICK_ITEM:
    {
        hash   pid = (hash) act.Param[ 0 ];
        ushort hx = (ushort) act.Param[ 1 ];
        ushort hy = (ushort) act.Param[ 2 ];

        if( !HexMngr.IsMapLoaded() )
            break;

        ItemHex* item = HexMngr.GetItem( hx, hy, pid );
        if( !item )
            break;

        uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy );
        if( dist > Chosen->GetUseDist() )
        {
            if( IsTurnBased )
                break;
            bool is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
            SetAction( CHOSEN_MOVE, hx, hy, is_run, Chosen->GetUseDist(), 0 );
            if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, Chosen->GetUseDist() ) )
                AddActionBack( act );
            AddActionBack( act );
            return;
        }

        CHECK_NEED_AP( Chosen->GetApCostPickItem() );

        // Refresh orientation
        uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy );
        if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy ) >= 1 && Chosen->GetDir() != dir )
        {
            Chosen->SetDir( dir );
            Net_SendDir();
        }

        Net_SendPickItem( hx, hy, pid );
        Chosen->Action( ACTION_PICK_ITEM, 0, item );
        Chosen->SubAp( Chosen->GetApCostPickItem() );
        // WaitPing();
    }
    break;
    case CHOSEN_PICK_CRIT:
    {
        uint crid = (uint) act.Param[ 0 ];
        bool is_loot = ( act.Param[ 1 ] == 0 );

        if( !HexMngr.IsMapLoaded() )
            break;

        CritterCl* cr = GetCritter( crid );
        if( !cr )
            break;

        if( is_loot && ( !cr->IsDead() || cr->GetIsNoLoot() ) )
            break;
        if( !is_loot && ( !cr->IsLife() || cr->GetIsNoPush() ) )
            break;

        uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() );
        uint pick_dist = Chosen->GetUseDist() + cr->GetMultihex();
        if( dist > pick_dist )
        {
            if( IsTurnBased )
                break;
            bool   is_run = ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunUseDist );
            SetAction( CHOSEN_MOVE_TO_CRIT, cr->GetId(), 0, is_run, pick_dist );
            ushort hx = cr->GetHexX(), hy = cr->GetHexY();
            if( HexMngr.CutPath( Chosen, Chosen->GetHexX(), Chosen->GetHexY(), hx, hy, pick_dist ) )
                AddActionBack( act );
            return;
        }

        CHECK_NEED_AP( Chosen->GetApCostPickCritter() );

        // Refresh orientation
        uchar dir = GetFarDir( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() );
        if( DistGame( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY() ) >= 1 && Chosen->GetDir() != dir )
        {
            Chosen->SetDir( dir );
            Net_SendDir();
        }

        if( is_loot )
        {
            Net_SendPickCritter( cr->GetId(), PICK_CRIT_LOOT );
            Chosen->Action( ACTION_PICK_CRITTER, 0, NULL );
        }
        else if( cr->IsLife() )
        {
            Net_SendPickCritter( cr->GetId(), PICK_CRIT_PUSH );
            Chosen->Action( ACTION_PICK_CRITTER, 2, NULL );
        }
        Chosen->SubAp( Chosen->GetApCostPickCritter() );
        WaitPing();
    }
    break;
    case CHOSEN_WRITE_HOLO:
    {
        uint holodisk_id = (uint) act.Param[ 0 ];
        if( holodisk_id != IboxHolodiskId )
            break;
        Item* holo = Chosen->GetItem( IboxHolodiskId );
        if( !holo->IsHolodisk() )
            break;
        const char* old_title = GetHoloText( STR_HOLO_INFO_NAME( holo->GetHolodiskNumber() ) );
        const char* old_text = GetHoloText( STR_HOLO_INFO_DESC( holo->GetHolodiskNumber() ) );
        if( holo && IboxTitle.length() && IboxText.length() && ( IboxTitle != old_title || IboxText != old_text ) )
        {
            Net_SendSetUserHoloStr( holo, IboxTitle.c_str(), IboxText.c_str() );
            Chosen->Action( ACTION_USE_ITEM, 0, holo );
        }
    }
    break;
    }

    EraseFrontAction();
}

void FOClient::TryPickItemOnGround()
{
    if( !Chosen )
        return;
    ItemHexVec items;
    HexMngr.GetItems( Chosen->GetHexX(), Chosen->GetHexY(), items );
    for( auto it = items.begin(); it != items.end();)
    {
        ItemHex* item = *it;
        if( item->IsFinishing() || item->Proto->IsDoor() || !item->IsUsable() )
            it = items.erase( it );
        else
            ++it;
    }
    if( items.empty() )
        return;
    ItemHex* item = items[ Random( 0, (int) items.size() - 1 ) ];
    SetAction( CHOSEN_PICK_ITEM, item->GetProtoId(), item->HexX, item->HexY );
}

void FOClient::TryExit()
{
    int active = GetActiveScreen();
    if( active != SCREEN_NONE )
    {
        switch( active )
        {
        case SCREEN__TIMER:
            TimerClose( false );
            break;
        case SCREEN__SPLIT:
            SplitClose( false );
            break;
        case SCREEN__TOWN_VIEW:
            Net_SendRefereshMe();
            break;
        case SCREEN__DIALOG:
            DlgKeyDown( true, DIK_ESCAPE, "" );
            break;
        case SCREEN__BARTER:
            DlgKeyDown( false, DIK_ESCAPE, "" );
            break;
        case SCREEN__DIALOGBOX:
            if( DlgboxType >= DIALOGBOX_ENCOUNTER_ANY && DlgboxType <= DIALOGBOX_ENCOUNTER_TB )
                Net_SendRuleGlobal( GM_CMD_ANSWER, -1 );
        case SCREEN__PERK:
        case SCREEN__ELEVATOR:
        case SCREEN__SAY:
        case SCREEN__SKILLBOX:
        case SCREEN__USE:
        case SCREEN__AIM:
        case SCREEN__INVENTORY:
        case SCREEN__PICKUP:
        case SCREEN__MINI_MAP:
        case SCREEN__CHARACTER:
        case SCREEN__PIP_BOY:
        case SCREEN__FIX_BOY:
        case SCREEN__MENU_OPTION:
        case SCREEN__SAVE_LOAD:
        default:
            ShowScreen( SCREEN_NONE );
            break;
        }
    }
    else
    {
        switch( GetMainScreen() )
        {
        case SCREEN_LOGIN:
            GameOpt.Quit = true;
            break;
        case SCREEN_REGISTRATION:
        case SCREEN_CREDITS:
            ShowMainScreen( SCREEN_LOGIN );
            break;
        case SCREEN_WAIT:
            NetDisconnect();
            break;
        case SCREEN_GLOBAL_MAP:
            ShowScreen( SCREEN__MENU_OPTION );
            break;
        case SCREEN_GAME:
            ShowScreen( SCREEN__MENU_OPTION );
            break;
        default:
            break;
        }
    }
}

void FOClient::ProcessMouseScroll()
{
    if( IsLMenu() || !GameOpt.MouseScroll )
        return;

    if( GameOpt.MouseX >= GameOpt.ScreenWidth - 1 )
        GameOpt.ScrollMouseRight = true;
    else
        GameOpt.ScrollMouseRight = false;

    if( GameOpt.MouseX <= 0 )
        GameOpt.ScrollMouseLeft = true;
    else
        GameOpt.ScrollMouseLeft = false;

    if( GameOpt.MouseY >= GameOpt.ScreenHeight - 1 )
        GameOpt.ScrollMouseDown = true;
    else
        GameOpt.ScrollMouseDown = false;

    if( GameOpt.MouseY <= 0 )
        GameOpt.ScrollMouseUp = true;
    else
        GameOpt.ScrollMouseUp = false;
}

void FOClient::DropScroll()
{
    GameOpt.ScrollMouseUp = false;
    GameOpt.ScrollMouseRight = false;
    GameOpt.ScrollMouseDown = false;
    GameOpt.ScrollMouseLeft = false;
    GameOpt.ScrollKeybUp = false;
    GameOpt.ScrollKeybRight = false;
    GameOpt.ScrollKeybDown = false;
    GameOpt.ScrollKeybLeft = false;
}

bool FOClient::IsCurInWindow()
{
    if( !( SDL_GetWindowFlags( MainWindow ) & SDL_WINDOW_INPUT_FOCUS ) )
        return false;
    return true;
}

void FOClient::FlashGameWindow()
{
    if( SDL_GetWindowFlags( MainWindow ) & SDL_WINDOW_INPUT_FOCUS )
        return;

    #ifdef FO_WINDOWS
    SDL_SysWMinfo info;
    if( GameOpt.MessNotify && SDL_GetWindowWMInfo( MainWindow, &info ) )
        FlashWindow( info.info.win.window, true );
    if( GameOpt.SoundNotify )
        Beep( 100, 200 );
    #else
    // Todo: Linux
    #endif
}

const char* FOClient::GetHoloText( uint str_num )
{
    if( str_num / 10 >= USER_HOLO_START_NUM )
    {
        if( !MsgUserHolo->Count( str_num ) )
        {
            Net_SendGetUserHoloStr( str_num );
            static char wait[ 32 ] = "...";
            return wait;
        }
        return MsgUserHolo->GetStr( str_num );
    }
    return MsgHolo->GetStr( str_num );
}

const char* FOClient::FmtGameText( uint str_num, ... )
{
//	return Str::FormatBuf(MsgGame->GetStr(str_num),(void*)(_ADDRESSOF(str_num)+_INTSIZEOF(str_num)));

    static char res[ MAX_FOTEXT ];
    static char str[ MAX_FOTEXT ];

    Str::Copy( str, MsgGame->GetStr( str_num ) );
    Str::Replacement( str, '\\', 'n', '\n' );

    va_list list;
    va_start( list, str_num );
    vsprintf( res, str, list );
    va_end( list );

    return res;
}

const char* FOClient::FmtCombatText( uint str_num, ... )
{
    static char res[ MAX_FOTEXT ];
    static char str[ MAX_FOTEXT ];

    Str::Copy( str, MsgCombat->GetStr( str_num ) );

    va_list list;
    va_start( list, str_num );
    vsprintf( res, str, list );
    va_end( list );

    return res;
}

const char* FOClient::FmtGenericDesc( int desc_type, int& ox, int& oy )
{
    ox = 0;
    oy = 0;
    if( Script::PrepareContext( ClientFunctions.GenericDesc, _FUNC_, "Game" ) )
    {
        Script::SetArgUInt( desc_type );
        Script::SetArgAddress( &ox );
        Script::SetArgAddress( &oy );
        if( Script::RunPrepared() )
        {
            ScriptString* result = (ScriptString*) Script::GetReturnedObject();
            if( result )
            {
                static char str[ MAX_FOTEXT ];
                Str::Copy( str, result->c_str() );
                return str[ 0 ] ? str : NULL;
            }
        }
    }

    return Str::FormatBuf( "<error>" );
}

const char* FOClient::FmtCritLook( CritterCl* cr, int look_type )
{
    if( Script::PrepareContext( ClientFunctions.CritterLook, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( cr );
        Script::SetArgUInt( look_type );
        if( Script::RunPrepared() )
        {
            ScriptString* result = (ScriptString*) Script::GetReturnedObject();
            if( result )
            {
                static char str[ MAX_FOTEXT ];
                Str::Copy( str, MAX_FOTEXT, result->c_str() );
                return str[ 0 ] ? str : NULL;
            }
        }
    }

    return MsgGame->GetStr( STR_CRIT_LOOK_NOTHING );
}

const char* FOClient::FmtItemLook( Item* item, int look_type )
{
    if( Script::PrepareContext( ClientFunctions.ItemLook, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( item );
        Script::SetArgUInt( look_type );
        if( Script::RunPrepared() )
        {
            ScriptString* result = (ScriptString*) Script::GetReturnedObject();
            if( result )
            {
                static char str[ MAX_FOTEXT ];
                Str::Copy( str, MAX_FOTEXT, result->c_str() );
                return str[ 0 ] ? str : NULL;
            }
        }
    }

    return MsgGame->GetStr( STR_ITEM_LOOK_NOTHING );
}

void FOClient::ParseIntellectWords( const char* words, PCharPairVec& text )
{
    char* w = (char*) words;
    Str::SkipLine( w );

    bool parse_in = true;
    char in[ MAX_FOTEXT ] = { 0 };
    char out[ MAX_FOTEXT ] = { 0 };

    while( true )
    {
        if( *w == 0 )
            break;

        if( *w == '\n' || *w == '\r' )
        {
            Str::SkipLine( w );
            parse_in = true;

            uint in_len = Str::Length( in ) + 1;
            uint out_len = Str::Length( out ) + 1;
            if( in_len < 2 )
                continue;

            char* in_ = new char[ in_len ];
            char* out_ = new char[ out_len ];
            Str::Copy( in_, in_len, in );
            Str::Copy( out_, out_len, out );

            text.push_back( PAIR( in_, out_ ) );
            in[ 0 ] = 0;
            out[ 0 ] = 0;
            continue;
        }

        if( *w == ' ' && *( w + 1 ) == ' ' && parse_in )
        {
            if( !Str::Length( in ) )
            {
                Str::SkipLine( w );
            }
            else
            {
                w += 2;
                parse_in = false;
            }
            continue;
        }

        strncat( parse_in ? in : out, w, 1 );
        w++;
    }
}

PCharPairVec::iterator FOClient::FindIntellectWord( const char* word, PCharPairVec& text, Randomizer& rnd )
{
    auto it = text.begin();
    auto end = text.end();
    for( ; it != end; ++it )
    {
        if( Str::CompareCaseUTF8( word, ( *it ).first ) )
            break;
    }

    if( it != end )
    {
        auto it_ = it;
        it++;
        int  cnt = 0;
        for( ; it != end; ++it )
        {
            if( Str::CompareCaseUTF8( ( *it_ ).first, ( *it ).first ) )
                cnt++;
            else
                break;
        }
        it_ += rnd.Random( 0, cnt );
        it = it_;
    }

    return it;
}

void FOClient::FmtTextIntellect( char* str, ushort intellect )
{
    static bool is_parsed = false;
    if( !is_parsed )
    {
        ParseIntellectWords( MsgGame->GetStr( STR_INTELLECT_WORDS ), IntellectWords );
        ParseIntellectWords( MsgGame->GetStr( STR_INTELLECT_SYMBOLS ), IntellectSymbols );
        is_parsed = true;
    }

    if( IntellectWords.empty() && IntellectSymbols.empty() )
        return;

    uchar intellegence = intellect & 0xF;
    if( !intellect || intellegence >= 5 )
        return;

    int word_proc;
    int symbol_proc;
    switch( intellegence )
    {
    default:
    case 1:
        word_proc = 100;
        symbol_proc = 20;
        break;
    case 2:
        word_proc = 80;
        symbol_proc = 15;
        break;
    case 3:
        word_proc = 60;
        symbol_proc = 10;
        break;
    case 4:
        word_proc = 30;
        symbol_proc = 5;
        break;
    }

    Randomizer rnd( ( intellect << 16 ) | intellect );

    char       word[ 1024 ] = { 0 };
    while( true )
    {
        while( *str )
        {
            uint length;
            Str::DecodeUTF8( str, &length );
            if( length == 1 && !( ( *str >= 'a' && *str <= 'z' ) || ( *str >= 'A' && *str <= 'Z' ) ) )
                break;
            strncat( word, str, length );
            str += length;
        }

        uint len = Str::Length( word );
        if( len )
        {
            auto it = FindIntellectWord( word, IntellectWords, rnd );
            if( it != IntellectWords.end() && rnd.Random( 1, 100 ) <= word_proc )
            {
                Str::EraseInterval( str - len, len );
                Str::Insert( str - len, ( *it ).second );
                str = str - len + Str::Length( ( *it ).second );
            }
            else
            {
                for( char* s = str - len; s < str;)
                {
                    if( rnd.Random( 1, 100 ) > symbol_proc )
                        continue;

                    uint length;
                    Str::DecodeUTF8( s, &length );
                    strncpy( word, s, length );
                    word[ length ] = 0;

                    it = FindIntellectWord( word, IntellectSymbols, rnd );
                    if( it != IntellectSymbols.end() )
                    {
                        uint f_len = Str::Length( ( *it ).first );
                        uint s_len = Str::Length( ( *it ).second );
                        Str::EraseInterval( s, f_len );
                        Str::Insert( s, ( *it ).second );
                        str -= f_len;
                        str += s_len;
                    }

                    s += length;
                }
            }
            word[ 0 ] = 0;
        }

        if( *str == 0 )
            break;

        uint length;
        Str::DecodeUTF8( str, &length );
        str += length;
    }
}

void FOClient::SoundProcess()
{
    // Ambient
    static uint next_ambient = 0;
    if( Timer::GameTick() > next_ambient )
    {
        if( IsMainScreen( SCREEN_GAME ) )
            SndMngr.PlayAmbient( MsgLocations->GetStr( STR_LOC_MAP_AMBIENT( CurMapLocPid, CurMapIndexInLoc ) ) );
        next_ambient = Timer::GameTick() + Random( AMBIENT_SOUND_TIME / 2, AMBIENT_SOUND_TIME );
    }
}

void FOClient::AddVideo( const char* video_name, bool can_stop, bool clear_sequence )
{
    // Stop current
    if( clear_sequence && ShowVideos.size() )
        StopVideo();

    // Show parameters
    ShowVideo sw;

    // Paths
    char  str[ MAX_FOPATH ];
    Str::Copy( str, video_name );
    char* sound = Str::Substring( str, "|" );
    if( sound )
    {
        *sound = 0;
        sound++;
        if( !Str::Substring( sound, "/" ) )
            sw.SoundName = FileManager::GetDataPath( "", PT_VIDEO );
        sw.SoundName += sound;
    }
    if( !Str::Substring( str, "/" ) )
        sw.FileName = FileManager::GetDataPath( "", PT_VIDEO );
    sw.FileName += str;

    // Add video in sequence
    sw.CanStop = can_stop;
    ShowVideos.push_back( sw );

    // Instant play
    if( ShowVideos.size() == 1 )
    {
        // Clear screen
        if( SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) ) )
            SprMngr.EndScene();

        // Play
        PlayVideo();
    }
}

void FOClient::PlayVideo()
{
    // Start new context
    ShowVideo& video = ShowVideos[ 0 ];
    CurVideo = new VideoContext();
    CurVideo->CurFrame = 0;
    CurVideo->StartTime = Timer::AccurateTick();
    CurVideo->AverageRenderTime = 0.0;

    // Open file
    if( !CurVideo->RawData.LoadFile( video.FileName.c_str(), PT_DATA ) )
    {
        WriteLogF( _FUNC_, " - Video file<%s> not found.\n", video.FileName.c_str() );
        SAFEDEL( CurVideo );
        NextVideo();
        return;
    }

    // Initialize Theora stuff
    ogg_sync_init( &CurVideo->SyncState );

    for( uint i = 0; i < CurVideo->SS.COUNT; i++ )
        CurVideo->SS.StreamsState[ i ] = false;
    CurVideo->SS.MainIndex = -1;

    th_info_init( &CurVideo->VideoInfo );
    th_comment_init( &CurVideo->Comment );
    CurVideo->SetupInfo = NULL;
    while( true )
    {
        int stream_index = VideoDecodePacket();
        if( stream_index < 0 )
        {
            WriteLogF( _FUNC_, " - Decode header packet fail.\n" );
            SAFEDEL( CurVideo );
            NextVideo();
            return;
        }
        int r = th_decode_headerin( &CurVideo->VideoInfo, &CurVideo->Comment, &CurVideo->SetupInfo, &CurVideo->Packet );
        if( !r )
        {
            if( stream_index != CurVideo->SS.MainIndex )
            {
                while( true )
                {
                    stream_index = VideoDecodePacket();
                    if( stream_index == CurVideo->SS.MainIndex )
                        break;
                    if( stream_index < 0 )
                    {
                        WriteLogF( _FUNC_, " - Seek first data packet fail.\n" );
                        SAFEDEL( CurVideo );
                        NextVideo();
                        return;
                    }
                }
            }
            break;
        }
        else
        {
            CurVideo->SS.MainIndex = stream_index;
        }
    }

    CurVideo->Context = th_decode_alloc( &CurVideo->VideoInfo, CurVideo->SetupInfo );

    // Create texture
    CurVideo->RT = SprMngr.CreateRenderTarget( false, false, CurVideo->VideoInfo.pic_width, CurVideo->VideoInfo.pic_height, true );
    if( !CurVideo->RT )
    {
        WriteLogF( _FUNC_, " - Can't create render target.\n" );
        SAFEDEL( CurVideo );
        NextVideo();
        return;
    }
    CurVideo->TextureData = new uchar[ CurVideo->RT->TargetTexture->Width * CurVideo->RT->TargetTexture->Height * 4 ];

    // Start sound
    if( video.SoundName != "" )
    {
        MusicVolumeRestore = GameOpt.MusicVolume;
        GameOpt.MusicVolume = 100;
        SndMngr.PlayMusic( video.SoundName.c_str(), 0, 0 );
    }
}

int FOClient::VideoDecodePacket()
{
    ogg_stream_state* streams = CurVideo->SS.Streams;
    bool*             initiated_stream = CurVideo->SS.StreamsState;
    uint              num_streams = CurVideo->SS.COUNT;
    ogg_packet*       packet = &CurVideo->Packet;

    int               b = 0;
    int               rv = 0;
    for( uint i = 0; initiated_stream[ i ] && i < num_streams; i++ )
    {
        int a = ogg_stream_packetout( &streams[ i ], packet );
        switch( a )
        {
        case  1:
            b = i + 1;
        case  0:
        case -1:
            break;
        }
    }
    if( b )
        return rv = b - 1;

    do
    {
        ogg_page op;
        while( ogg_sync_pageout( &CurVideo->SyncState, &op ) != 1 )
        {
            int left = CurVideo->RawData.GetFsize() - CurVideo->RawData.GetCurPos();
            int bytes = MIN( 1024, left );
            if( !bytes )
                return -2;
            char* buffer = ogg_sync_buffer( &CurVideo->SyncState, bytes );
            CurVideo->RawData.CopyMem( buffer, bytes );
            ogg_sync_wrote( &CurVideo->SyncState, bytes );
        }

        if( ogg_page_bos( &op ) && rv != 1 )
        {
            uint i = 0;
            while( initiated_stream[ i ] && i < num_streams )
                i++;
            if( !initiated_stream[ i ] )
            {
                int a = ogg_stream_init( &streams[ i ], ogg_page_serialno( &op ) );
                initiated_stream[ i ] = true;
                if( a )
                    return -1;
                else
                    rv = 1;
            }
        }

        for( uint i = 0; initiated_stream[ i ] && i < num_streams; i++ )
        {
            ogg_stream_pagein( &streams[ i ], &op );
            int a = ogg_stream_packetout( &streams[ i ], packet );
            switch( a )
            {
            case  1:
                rv = i;
                b = i + 1;
            case  0:
            case -1:
                break;
            }
        }
    }
    while( !b );

    return rv;
}

void FOClient::RenderVideo()
{
    // Count function call time to interpolate output time
    double render_time = Timer::AccurateTick();

    // Calculate next frame
    double cur_second = ( render_time - CurVideo->StartTime + CurVideo->AverageRenderTime ) / 1000.0;
    uint   new_frame = (uint) floor( cur_second * (double) CurVideo->VideoInfo.fps_numerator / (double) CurVideo->VideoInfo.fps_denominator + 0.5 );
    uint   skip_frames = new_frame - CurVideo->CurFrame;
    if( !skip_frames )
        return;
    bool last_frame = false;
    CurVideo->CurFrame = new_frame;

    // Process frames
    for( uint f = 0; f < skip_frames; f++ )
    {
        // Decode frame
        int r = th_decode_packetin( CurVideo->Context, &CurVideo->Packet, NULL );
        if( r != TH_DUPFRAME )
        {
            if( r )
            {
                WriteLogF( _FUNC_, " - Frame does not contain encoded video data, error<%d>.\n", r );
                NextVideo();
                return;
            }

            // Decode color
            r = th_decode_ycbcr_out( CurVideo->Context, CurVideo->ColorBuffer );
            if( r )
            {
                WriteLogF( _FUNC_, " - th_decode_ycbcr_out() fail, error<%d>\n.", r );
                NextVideo();
                return;
            }
        }

        // Seek next packet
        do
        {
            r = VideoDecodePacket();
            if( r == -2 )
            {
                last_frame = true;
                break;
            }
        }
        while( r != CurVideo->SS.MainIndex );
        if( last_frame )
            break;
    }

    // Data offsets
    char di, dj;
    switch( CurVideo->VideoInfo.pixel_fmt )
    {
    case TH_PF_420:
        di = 2;
        dj = 2;
        break;
    case TH_PF_422:
        di = 2;
        dj = 1;
        break;
    case TH_PF_444:
        di = 1;
        dj = 1;
        break;
    default:
        WriteLogF( _FUNC_, " - Wrong pixel format.\n" );
        NextVideo();
        return;
    }

    // Fill render texture
    uint w = CurVideo->VideoInfo.pic_width;
    uint h = CurVideo->VideoInfo.pic_height;
    for( uint y = 0; y < h; y++ )
    {
        for( uint x = 0; x < w; x++ )
        {
            // Get YUV
            th_ycbcr_buffer& cbuf = CurVideo->ColorBuffer;
            uchar            cy = *( cbuf[ 0 ].data + y * cbuf[ 0 ].stride + x );
            uchar            cu = *( cbuf[ 1 ].data + y / dj * cbuf[ 1 ].stride + x / di );
            uchar            cv = *( cbuf[ 2 ].data + y / dj * cbuf[ 2 ].stride + x / di );

            // Convert YUV to RGB
            float cr = cy + 1.402f * ( cv - 127 );
            float cg = cy - 0.344f * ( cu - 127 ) - 0.714f * ( cv - 127 );
            float cb = cy + 1.722f * ( cu - 127 );

            // Set on texture
            uchar* data = CurVideo->TextureData + ( ( h - y - 1 ) * w * 4 + x * 4 );
            data[ 0 ] = (uchar) cr;
            data[ 1 ] = (uchar) cg;
            data[ 2 ] = (uchar) cb;
            data[ 3 ] = 0xFF;
        }
    }

    // Update texture and draw it
    CurVideo->RT->TargetTexture->UpdateRegion( Rect( 0, 0, CurVideo->RT->TargetTexture->Width - 1, CurVideo->RT->TargetTexture->Height - 1 ), CurVideo->TextureData );
    SprMngr.DrawRenderTarget( CurVideo->RT, false );

    // Render to window
    float mw = (float) GameOpt.ScreenWidth;
    float mh = (float) GameOpt.ScreenHeight;
    float k = MIN( mw / w, mh / h );
    w = (uint) ( (float) w * k );
    h = (uint) ( (float) h * k );
    int x = ( GameOpt.ScreenWidth - w ) / 2;
    int y = ( GameOpt.ScreenHeight - h ) / 2;
    if( SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) ) )
    {
        Rect r = Rect( x, y, x + w, y + h );
        SprMngr.DrawRenderTarget( CurVideo->RT, false, NULL, &r );
        SprMngr.EndScene();
    }

    // Store render time
    render_time = Timer::AccurateTick() - render_time;
    if( CurVideo->AverageRenderTime > 0.0 )
        CurVideo->AverageRenderTime = ( CurVideo->AverageRenderTime + render_time ) / 2.0;
    else
        CurVideo->AverageRenderTime = render_time;

    // Last frame
    if( last_frame )
        NextVideo();
}

void FOClient::NextVideo()
{
    if( ShowVideos.size() )
    {
        // Clear screen
        if( SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) ) )
            SprMngr.EndScene();

        // Stop current
        StopVideo();
        ShowVideos.erase( ShowVideos.begin() );

        // Manage next
        if( ShowVideos.size() )
        {
            PlayVideo();
        }
        else
        {
            ScreenFadeOut();
            if( MusicAfterVideo != "" )
            {
                SndMngr.PlayMusic( MusicAfterVideo.c_str() );
                MusicAfterVideo = "";
            }
        }
    }
}

void FOClient::StopVideo()
{
    // Video
    if( CurVideo )
    {
        ogg_sync_clear( &CurVideo->SyncState );
        th_info_clear( &CurVideo->VideoInfo );
        th_setup_free( CurVideo->SetupInfo );
        th_decode_free( CurVideo->Context );
        SprMngr.DeleteRenderTarget( CurVideo->RT );
        SAFEDELA( CurVideo->TextureData );
        SAFEDEL( CurVideo );
    }

    // Music
    SndMngr.StopMusic();
    if( MusicVolumeRestore != -1 )
    {
        GameOpt.MusicVolume = MusicVolumeRestore;
        MusicVolumeRestore = -1;
    }
}

uint FOClient::AnimLoad( uint name_hash, int res_type )
{
    AnyFrames* anim = ResMngr.GetAnim( name_hash, res_type );
    if( !anim )
        return 0;
    IfaceAnim* ianim = new IfaceAnim( anim, res_type );
    if( !ianim )
        return 0;

    uint index = 1;
    for( uint j = (uint) Animations.size(); index < j; index++ )
        if( !Animations[ index ] )
            break;
    if( index < (uint) Animations.size() )
        Animations[ index ] = ianim;
    else
        Animations.push_back( ianim );
    return index;
}

uint FOClient::AnimLoad( const char* fname, int path_type, int res_type )
{
    char full_name[ MAX_FOPATH ];
    FileManager::GetDataPath( fname, path_type, full_name );

    AnyFrames* anim = ResMngr.GetAnim( Str::GetHash( full_name ), res_type );
    if( !anim )
        return 0;
    IfaceAnim* ianim = new IfaceAnim( anim, res_type );
    if( !ianim )
        return 0;

    uint index = 1;
    for( uint j = (uint) Animations.size(); index < j; index++ )
        if( !Animations[ index ] )
            break;
    if( index < (uint) Animations.size() )
        Animations[ index ] = ianim;
    else
        Animations.push_back( ianim );
    return index;
}

uint FOClient::AnimGetCurSpr( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->Frames->Ind[ Animations[ anim_id ]->CurSpr ];
}

uint FOClient::AnimGetCurSprCnt( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->CurSpr;
}

uint FOClient::AnimGetSprCount( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->Frames->CntFrm;
}

AnyFrames* FOClient::AnimGetFrames( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->Frames;
}

void FOClient::AnimRun( uint anim_id, uint flags )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return;
    IfaceAnim* anim = Animations[ anim_id ];

    // Set flags
    anim->Flags = flags & 0xFFFF;
    flags >>= 16;

    // Set frm
    uchar cur_frm = flags & 0xFF;
    if( cur_frm > 0 )
    {
        cur_frm--;
        if( cur_frm >= anim->Frames->CntFrm )
            cur_frm = anim->Frames->CntFrm - 1;
        anim->CurSpr = cur_frm;
    }
    // flags>>=8;
}

void FOClient::AnimProcess()
{
    uint cur_tick = Timer::GameTick();
    for( auto it = Animations.begin(), end = Animations.end(); it != end; ++it )
    {
        IfaceAnim* anim = *it;
        if( !anim || !anim->Flags )
            continue;

        if( FLAG( anim->Flags, ANIMRUN_STOP ) )
        {
            anim->Flags = 0;
            continue;
        }

        if( FLAG( anim->Flags, ANIMRUN_TO_END ) || FLAG( anim->Flags, ANIMRUN_FROM_END ) )
        {
            if( cur_tick - anim->LastTick < anim->Frames->Ticks / anim->Frames->CntFrm )
                continue;

            anim->LastTick = cur_tick;
            uint end_spr = anim->Frames->CntFrm - 1;
            if( FLAG( anim->Flags, ANIMRUN_FROM_END ) )
                end_spr = 0;

            if( anim->CurSpr < end_spr )
                anim->CurSpr++;
            else if( anim->CurSpr > end_spr )
                anim->CurSpr--;
            else
            {
                if( FLAG( anim->Flags, ANIMRUN_CYCLE ) )
                {
                    if( FLAG( anim->Flags, ANIMRUN_TO_END ) )
                        anim->CurSpr = 0;
                    else
                        anim->CurSpr = end_spr;
                }
                else
                {
                    anim->Flags = 0;
                }
            }
        }
    }
}

void FOClient::OnSendChosenValue( void* obj, Property* prop, void* cur_value, void* old_value )
{
    CritterCl* cr = (CritterCl*) obj;
    if( cr->IsChosen() )
        Self->Net_SendChosenProperty( prop );
}

void FOClient::OnSendItemValue( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    #pragma MESSAGE( "Clean up client 0 and -1 item ids" )
    if( item->Id && item->Id != uint( -1 ) && Self->Chosen && Self->Chosen->GetItem( item->Id ) )
        Self->Net_SendItemProperty( item, prop );
}

void FOClient::OnSetItemFlags( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    uint  cur = *(uint*) cur_value;
    uint  old = *(uint*) old_value;

    if( FLAG( cur, ITEM_LIGHT ) != FLAG( old, ITEM_LIGHT ) )
        Self->HexMngr.RebuildLight();

    if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        ItemHex* hex_item = (ItemHex*) item;
        bool     rebuild_cache = false;
        if( FLAG( cur, ITEM_COLORIZE ) != FLAG( old, ITEM_COLORIZE ) )
            hex_item->RefreshAlpha();
        if( FLAG( cur, ITEM_BAD_ITEM ) != FLAG( old, ITEM_BAD_ITEM ) )
            hex_item->SetSprite( NULL );
        if( FLAG( cur, ITEM_SHOOT_THRU ) != FLAG( old, ITEM_SHOOT_THRU ) )
            Self->RebuildLookBorders = true, rebuild_cache = true;
        if( FLAG( cur, ITEM_LIGHT_THRU ) != FLAG( old, ITEM_LIGHT_THRU ) )
            Self->HexMngr.RebuildLight(), rebuild_cache = true;
        if( FLAG( cur, ITEM_NO_BLOCK ) != FLAG( old, ITEM_NO_BLOCK ) )
            rebuild_cache = true;
        if( rebuild_cache )
            Self->HexMngr.GetField( hex_item->GetHexX(), hex_item->GetHexY() ).ProcessCache();
    }
}

void FOClient::OnSetItemSomeLight( void* obj, Property* prop, void* cur_value, void* old_value )
{
    // LightIntensity, LightDistance, LightFlags, LightColor

    Self->HexMngr.RebuildLight();
}

void FOClient::OnSetItemPicMap( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;

    if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        ItemHex* hex_item = (ItemHex*) item;
        hex_item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetXY( void* obj, Property* prop, void* cur_value, void* old_value )
{
    // OffsetX, OffsetY

    Item* item = (Item*) obj;

    if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        ItemHex* hex_item = (ItemHex*) item;
        hex_item->SetAnimOffs();
        Self->HexMngr.ProcessHexBorders( hex_item );
    }
}

void FOClient::OnSetItemLockerCondition( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    uint  cur = *(uint*) cur_value;
    uint  old = *(uint*) old_value;

    if( item->IsDoor() || item->IsContainer() )
    {
        ItemHex* hex_item = (ItemHex*) item;
        if( !FLAG( old, LOCKER_ISOPEN ) && FLAG( cur, LOCKER_ISOPEN ) )
            hex_item->SetAnimFromStart();
        if( FLAG( old, LOCKER_ISOPEN ) && !FLAG( cur, LOCKER_ISOPEN ) )
            hex_item->SetAnimFromEnd();
    }
}

/************************************************************************/
/* Scripts                                                              */
/************************************************************************/

bool FOClient::ReloadScripts()
{
    WriteLog( "Load scripts...\n" );

    FOMsg& msg_script = CurLang.Msg[ TEXTMSG_INTERNAL ];
    if( !msg_script.Count( STR_INTERNAL_SCRIPT_MODULES ) ||
        !msg_script.Count( STR_INTERNAL_SCRIPT_MODULES + 1 ) )
    {
        WriteLog( "Main script section not found in MSG.\n" );
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    // Properties
    PropertyRegistrator* registrators[ 3 ] =
    {
        new PropertyRegistrator( false, "CritterCl" ),
        new PropertyRegistrator( false, "ItemCl" ),
        new PropertyRegistrator( false, "ProtoItem" ),
    };

    // Reinitialize engine
    Script::Finish();
    if( !Script::Init( new ScriptPragmaCallback( PRAGMA_CLIENT, registrators ), "CLIENT", true ) )
    {
        WriteLog( "Unable to start script engine.\n" );
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    // Bind stuff
    #define BIND_CLIENT
    #define BIND_CLASS    FOClient::SScriptFunc::
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line<%d>.\n", __LINE__ ); bind_errors++; }
    asIScriptEngine* engine = Script::GetEngine();
    int              bind_errors = 0;
    #include "ScriptBind.h"

    if( bind_errors )
    {
        WriteLog( "Bind fail, errors<%d>.\n", bind_errors );
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    // Options
    Script::SetScriptsPath( PT_CACHE );
    Script::Undef( NULL );
    Script::Define( "__CLIENT" );
    Script::Define( "__VERSION %d", FONLINE_VERSION );

    // Store dlls
    for( int i = STR_INTERNAL_SCRIPT_DLLS; ; i += 2 )
    {
        if( !msg_script.Count( i ) || !msg_script.Count( i + 1 ) )
            break;

        const char*  dll_name = msg_script.GetStr( i );
        uint         len;
        const uchar* dll_binary = msg_script.GetBinary( i + 1, len );
        if( !dll_binary )
            break;

        // Fix slashes
        char dll_name_[ MAX_FOPATH ];
        Str::Copy( dll_name_, dll_name );
        Str::Replacement( dll_name_, '\\', '.' );
        Str::Replacement( dll_name_, '/', '.' );
        dll_name = dll_name_;

        // Save to cache
        FileManager dll;
        if( dll.LoadStream( dll_binary, len ) )
        {
            dll.SwitchToWrite();
            dll.SaveOutBufToFile( dll_name, PT_CACHE );
        }
    }

    // Pragmas
    Pragmas pragmas;
    for( int i = STR_INTERNAL_SCRIPT_PRAGMAS; ; i += 2 )
    {
        if( !msg_script.Count( i ) || !msg_script.Count( i + 1 ) )
            break;
        Preprocessor::PragmaInstance pragma;
        pragma.Name = msg_script.GetStr( i );
        pragma.Text = msg_script.GetStr( i + 1 );
        pragma.CurrentFile = "main";
        pragma.CurrentFileLine = i;
        pragmas.push_back( pragma );
    }
    Script::CallPragmas( pragmas );

    // Load modules
    int errors = 0;
    for( int i = STR_INTERNAL_SCRIPT_MODULES; ; i += 2 )
    {
        if( !msg_script.Count( i ) || !msg_script.Count( i + 1 ) )
            break;

        const char*  module_name = msg_script.GetStr( i );
        uint         len;
        const uchar* bytecode = msg_script.GetBinary( i + 1, len );
        if( !bytecode )
            break;

        if( !Script::LoadScript( module_name, bytecode, len ) )
        {
            WriteLog( "Load script<%s> fail.\n", module_name );
            errors++;
        }
    }

    // Bind functions
    errors += Script::BindImportedFunctions();
    errors += Script::RebindFunctions();

    // Bind reserved functions
    ReservedScriptFunction BindGameFunc[] =
    {
        { &ClientFunctions.Start, "start", "bool %s()" },
        { &ClientFunctions.Finish, "finish", "void %s()" },
        { &ClientFunctions.Loop, "loop", "uint %s()" },
        { &ClientFunctions.GetActiveScreens, "get_active_screens", "void %s(int[]&)" },
        { &ClientFunctions.ScreenChange, "screen_change", "void %s(bool,int,dictionary@)" },
        { &ClientFunctions.RenderIface, "render_iface", "void %s(uint)" },
        { &ClientFunctions.RenderMap, "render_map", "void %s()" },
        { &ClientFunctions.MouseDown, "mouse_down", "void %s(int)" },
        { &ClientFunctions.MouseUp, "mouse_up", "void %s(int)" },
        { &ClientFunctions.MouseMove, "mouse_move", "void %s()" },
        { &ClientFunctions.KeyDown, "key_down", "void %s(uint8,string@)" },
        { &ClientFunctions.KeyUp, "key_up", "void %s(uint8)" },
        { &ClientFunctions.InputLost, "input_lost", "void %s()" },
        { &ClientFunctions.CritterIn, "critter_in", "void %s(CritterCl&)" },
        { &ClientFunctions.CritterOut, "critter_out", "void %s(CritterCl&)" },
        { &ClientFunctions.ItemMapIn, "item_map_in", "void %s(ItemCl&)" },
        { &ClientFunctions.ItemMapChanged, "item_map_changed", "void %s(ItemCl&,ItemCl&)" },
        { &ClientFunctions.ItemMapOut, "item_map_out", "void %s(ItemCl&)" },
        { &ClientFunctions.ItemInvAllIn, "item_inv_all_in", "void %s()" },
        { &ClientFunctions.ItemInvIn, "item_inv_in", "void %s(ItemCl&)" },
        { &ClientFunctions.ItemInvChanged, "item_inv_changed", "void %s(ItemCl&,ItemCl&)" },
        { &ClientFunctions.ItemInvOut, "item_inv_out", "void %s(ItemCl&)" },
        { &ClientFunctions.MapMessage, "map_message", "bool %s(string&,uint16&,uint16&,uint&,uint&)" },
        { &ClientFunctions.InMessage, "in_message", "bool %s(string&,int&,uint&,uint&)" },
        { &ClientFunctions.OutMessage, "out_message", "bool %s(string&,int&)" },
        { &ClientFunctions.MessageBox, "message_box", "void %s(string&,int,bool)" },
        { &ClientFunctions.ToHit, "to_hit", "int %s(CritterCl&,CritterCl&,ProtoItem&,uint8)" },
        { &ClientFunctions.HitAim, "hit_aim", "void %s(uint8&)" },
        { &ClientFunctions.CombatResult, "combat_result", "void %s(uint[]&)" },
        { &ClientFunctions.GenericDesc, "generic_description", "string %s(int,int&,int&)" },
        { &ClientFunctions.ItemLook, "item_description", "string %s(ItemCl&,int)" },
        { &ClientFunctions.CritterLook, "critter_description", "string %s(CritterCl&,int)" },
        { &ClientFunctions.GetElevator, "get_elevator", "bool %s(uint,uint[]&)" },
        { &ClientFunctions.ItemCost, "item_cost", "uint %s(ItemCl&,CritterCl&,CritterCl&,bool)" },
        { &ClientFunctions.PerksCheck, "get_available_perks", "void %s(CritterProperty[]&)" },
        { &ClientFunctions.GetTimeouts, "get_available_timeouts", "void %s(CritterProperty[]&)" },
        { &ClientFunctions.CritterAction, "critter_action", "void %s(bool,CritterCl&,int,int,ItemCl@)" },
        { &ClientFunctions.Animation2dProcess, "animation2d_process", "void %s(bool,CritterCl&,uint,uint,ItemCl@)" },
        { &ClientFunctions.Animation3dProcess, "animation3d_process", "void %s(bool,CritterCl&,uint,uint,ItemCl@)" },
        { &ClientFunctions.ItemsCollection, "items_collection", "void %s(int,ItemCl@[]&)" },
        { &ClientFunctions.CritterAnimation, "critter_animation", "string@ %s(int,uint,uint,uint,uint&,uint&,int&,int&)" },
        { &ClientFunctions.CritterAnimationSubstitute, "critter_animation_substitute", "bool %s(int,uint,uint,uint,uint&,uint&,uint&)" },
        { &ClientFunctions.CritterAnimationFallout, "critter_animation_fallout", "bool %s(uint,uint&,uint&,uint&,uint&,uint&)" },
        { &ClientFunctions.CritterCheckMoveItem, "critter_check_move_item", "bool %s(CritterCl&,ItemCl&,uint8,ItemCl@)" },
        { &ClientFunctions.GetUseApCost, "get_use_ap_cost", "uint %s(CritterCl&,ItemCl&,uint8)" },
        { &ClientFunctions.GetAttackDistantion, "get_attack_distantion", "uint %s(CritterCl&,ItemCl&,uint8)" },
        { &ClientFunctions.CheckInterfaceHit, "check_interface_hit", "bool %s(int,int)" },
        { &ClientFunctions.GetContItem, "get_cont_item", "bool %s(uint&,bool&)" },
    };
    if( !Script::BindReservedFunctions( BindGameFunc, sizeof( BindGameFunc ) / sizeof( BindGameFunc[ 0 ] ) ) )
        errors++;

    CritterCl::SetPropertyRegistrator( registrators[ 0 ] );
    CritterCl::PropertiesRegistrator->SetNativeSendCallback( OnSendChosenValue );
    Item::SetPropertyRegistrator( registrators[ 1 ] );
    Item::PropertiesRegistrator->SetNativeSendCallback( OnSendItemValue );
    Item::PropertiesRegistrator->SetNativeSetCallback( "Flags", OnSetItemFlags );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightIntensity", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightDistance", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightFlags", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightColor", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "PicMap", OnSetItemPicMap );
    Item::PropertiesRegistrator->SetNativeSetCallback( "OffsetX", OnSetItemOffsetXY );
    Item::PropertiesRegistrator->SetNativeSetCallback( "OffsetY", OnSetItemOffsetXY );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LockerCondition", OnSetItemLockerCondition );
    ProtoItem::SetPropertyRegistrator( registrators[ 2 ] );

    if( errors || !Script::RunModuleInitFunctions() )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    WriteLog( "Load scripts complete.\n" );
    return true;
}

int FOClient::ScriptGetHitProc( CritterCl* cr, int hit_location )
{
    if( !Chosen )
        return 0;
    int        use = Chosen->GetUse();
    ProtoItem* proto_item = Chosen->ItemSlotMain->Proto;
    if( !proto_item->IsWeapon() || !Chosen->ItemSlotMain->WeapIsUseAviable( use ) )
        return 0;

    if( Script::PrepareContext( ClientFunctions.ToHit, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( Chosen );
        Script::SetArgObject( cr );
        Script::SetArgObject( proto_item );
        Script::SetArgUChar( MAKE_ITEM_MODE( use, hit_location ) );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return 0;
}

void FOClient::DrawIfaceLayer( uint layer )
{
    if( Script::PrepareContext( ClientFunctions.RenderIface, _FUNC_, "Game" ) )
    {
        SpritesCanDraw++;
        Script::SetArgUInt( layer );
        Script::RunPrepared();
        SpritesCanDraw--;
    }
}

void FOClient::OnItemInvChanged( Item* old_item, Item* item )
{
    if( Script::PrepareContext( ClientFunctions.ItemInvChanged, _FUNC_, "Game" ) )
    {
        Script::SetArgObject( old_item );
        Script::SetArgObject( item );
        Script::RunPrepared();
    }
    old_item->Release();
}

int SortCritterHx_ = 0, SortCritterHy_ = 0;
bool SortCritterByDistPred( CritterCl* cr1, CritterCl* cr2 )
{
    return DistGame( SortCritterHx_, SortCritterHy_, cr1->GetHexX(), cr1->GetHexY() ) < DistGame( SortCritterHx_, SortCritterHy_, cr2->GetHexX(), cr2->GetHexY() );
}
void SortCritterByDist( CritterCl* cr, CritVec& critters )
{
    SortCritterHx_ = cr->GetHexX();
    SortCritterHy_ = cr->GetHexY();
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}
void SortCritterByDist( int hx, int hy, CritVec& critters )
{
    SortCritterHx_ = hx;
    SortCritterHy_ = hy;
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}

bool FOClient::SScriptFunc::Crit_IsChosen( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsChosen();
}

bool FOClient::SScriptFunc::Crit_IsPlayer( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsPlayer();
}

bool FOClient::SScriptFunc::Crit_IsNpc( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsNpc();
}

bool FOClient::SScriptFunc::Crit_IsLife( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsLife();
}

bool FOClient::SScriptFunc::Crit_IsKnockout( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsKnockout();
}

bool FOClient::SScriptFunc::Crit_IsDead( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsDead();
}

bool FOClient::SScriptFunc::Crit_IsFree( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsFree();
}

bool FOClient::SScriptFunc::Crit_IsBusy( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return !cr->IsFree();
}

bool FOClient::SScriptFunc::Crit_IsAnim3d( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->Anim3d != NULL;
}

bool FOClient::SScriptFunc::Crit_IsAnimAviable( CritterCl* cr, uint anim1, uint anim2 )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsAnimAviable( anim1, anim2 );
}

bool FOClient::SScriptFunc::Crit_IsAnimPlaying( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->IsAnim();
}

uint FOClient::SScriptFunc::Crit_GetAnim1( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->GetAnim1();
}

void FOClient::SScriptFunc::Crit_Animate( CritterCl* cr, uint anim1, uint anim2 )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    cr->Animate( anim1, anim2, NULL );
}

void FOClient::SScriptFunc::Crit_AnimateEx( CritterCl* cr, uint anim1, uint anim2, Item* item )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    cr->Animate( anim1, anim2, item );
}

void FOClient::SScriptFunc::Crit_ClearAnim( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    cr->ClearAnim();
}

void FOClient::SScriptFunc::Crit_Wait( CritterCl* cr, uint ms )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    cr->TickStart( ms );
}

uint FOClient::SScriptFunc::Crit_ItemsCount( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->GetItemsCount();
}

uint FOClient::SScriptFunc::Crit_ItemsWeight( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->GetItemsWeight();
}

uint FOClient::SScriptFunc::Crit_ItemsVolume( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->GetItemsVolume();
}

uint FOClient::SScriptFunc::Crit_CountItem( CritterCl* cr, hash proto_id )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->CountItemPid( proto_id );
}

uint FOClient::SScriptFunc::Crit_CountItemByType( CritterCl* cr, uchar type )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->CountItemType( type );
}

Item* FOClient::SScriptFunc::Crit_GetItem( CritterCl* cr, hash proto_id, int slot )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    if( proto_id && slot >= 0 && slot < SLOT_GROUND )
        return cr->GetItemByPidSlot( proto_id, slot );
    else if( proto_id )
        return cr->GetItemByPidInvPriority( proto_id );
    else if( slot >= 0 && slot < SLOT_GROUND )
        return cr->GetItemSlot( slot );
    return NULL;
}

Item* FOClient::SScriptFunc::Crit_GetItemById( CritterCl* cr, uint item_id )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->GetItem( item_id );
}

uint FOClient::SScriptFunc::Crit_GetItems( CritterCl* cr, int slot, ScriptArray* items )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    ItemVec items_;
    cr->GetItemsSlot( slot, items_ );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

uint FOClient::SScriptFunc::Crit_GetItemsByType( CritterCl* cr, int type, ScriptArray* items )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    ItemVec items_;
    cr->GetItemsType( type, items_ );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

ProtoItem* FOClient::SScriptFunc::Crit_GetSlotProto( CritterCl* cr, int slot, uchar& mode )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );

    Item* item = NULL;
    switch( slot )
    {
    case SLOT_HAND1:
        item = cr->ItemSlotMain;
        break;
    case SLOT_HAND2:
        item = ( cr->ItemSlotExt->GetId() ? cr->ItemSlotExt : cr->DefItemSlotHand );
        break;
    case SLOT_ARMOR:
        item = cr->ItemSlotArmor;
        break;
    default:
        item = cr->GetItemSlot( slot );
        break;
    }
    if( !item )
        return NULL;

    mode = item->GetMode();
    return item->Proto;
}

void FOClient::SScriptFunc::Crit_SetVisible( CritterCl* cr, bool visible )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    cr->Visible = visible;
    Self->HexMngr.RefreshMap();
}

bool FOClient::SScriptFunc::Crit_GetVisible( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->Visible;
}

void FOClient::SScriptFunc::Crit_set_ContourColor( CritterCl* cr, uint value )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    if( cr->SprDrawValid )
        cr->SprDraw->SetContour( cr->SprDraw->ContourType, value );
    cr->ContourColor = value;
}

uint FOClient::SScriptFunc::Crit_get_ContourColor( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->ContourColor;
}

uint FOClient::SScriptFunc::Crit_GetMultihex( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return cr->GetMultihex();
}

bool FOClient::SScriptFunc::Crit_IsTurnBasedTurn( CritterCl* cr )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return Self->IsTurnBased && cr->GetId() == Self->TurnBasedCurCritterId;
}

void FOClient::SScriptFunc::Crit_GetNameTextInfo( CritterCl* cr, bool& nameVisible, int& x, int& y, int& w, int& h, int& lines )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );

    cr->GetNameTextInfo( nameVisible, x, y, w, h, lines );
}

bool FOClient::SScriptFunc::Item_IsStackable( Item* item )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return item->IsStackable();
}

bool FOClient::SScriptFunc::Item_IsDeteriorable( Item* item )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return item->IsDeteriorable();
}

uchar FOClient::SScriptFunc::Item_GetType( Item* item )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return item->GetType();
}

hash FOClient::SScriptFunc::Item_GetProtoId( Item* item )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return item->GetProtoId();
}

uint FOClient::SScriptFunc::Item_GetCount( Item* item )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    return item->GetCount();
}

bool FOClient::SScriptFunc::Item_GetMapPosition( Item* item, ushort& hx, ushort& hy )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    switch( item->Accessory )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        CritterCl* cr = Self->GetCritter( item->AccCritter.Id );
        if( !cr )
            SCRIPT_ERROR_R0( "CritterCl accessory, CritterCl not found." );
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        hx = item->AccHex.HexX;
        hy = item->AccHex.HexY;
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
    {

        if( item->GetId() == item->AccContainer.ContainerId )
            SCRIPT_ERROR_R0( "Container accessory, Crosslinks." );
        Item* cont = Self->GetItem( item->AccContainer.ContainerId );
        if( !cont )
            SCRIPT_ERROR_R0( "Container accessory, Container not found." );
        return Item_GetMapPosition( cont, hx, hy );             // Recursion
    }
    break;
    default:
        SCRIPT_ERROR_R0( "Unknown accessory." );
        return false;
    }
    return true;
}

void FOClient::SScriptFunc::Item_Animate( Item* item, uchar from_frame, uchar to_frame )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R( "This nullptr." );
    #pragma MESSAGE("Implement Item_Animate.")
}

Item* FOClient::SScriptFunc::Item_GetChild( Item* item, uint childIndex )
{
    if( item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
    // Need implement
    #pragma MESSAGE("Implement Item_GetChild.")
    return NULL;
}

ScriptString* FOClient::SScriptFunc::Global_CustomCall( ScriptString& command, ScriptString& separator )
{
    // Parse command
    vector< string >  args;
    std::stringstream ss( command.c_std_str() );
    if( separator.length() > 0 )
    {
        string arg;
        while( getline( ss, arg, *separator.c_str() ) )
            args.push_back( arg );
    }
    else
    {
        args.push_back( command.c_std_str() );
    }
    if( args.size() < 1 )
        SCRIPT_ERROR_R0( "Empty custom call command." );

    // Execute
    string cmd = args[ 0 ];
    if( cmd == "Login" && args.size() >= 3 )
    {
        *GameOpt.Name = args[ 1 ];
        Self->Password = args[ 2 ];
        Self->ConnectToGame();
    }
    else if( cmd == "Register" )
    {
        if( Self->RegCheckData() )
        {
            Self->RegProps.clear();
            for( size_t i = 1; i < args.size(); i += 2 )
            {
                Self->RegProps.push_back( Str::AtoI( args[ i ].c_str() ) );
                Self->RegProps.push_back( Str::AtoI( args[ i + 1 ].c_str() ) );
            }
            Self->InitNetReason = INIT_NET_REASON_REG;
            Self->SetCurMode( CUR_WAIT );
        }
    }
    else if( cmd == "GetPassword" )
    {
        return ScriptString::Create( Self->Password );
    }
    else if( cmd == "DumpAtlases" )
    {
        SprMngr.DumpAtlases();
    }
    else if( cmd == "SwitchShowTrack" )
    {
        Self->HexMngr.SwitchShowTrack();
    }
    else if( cmd == "SwitchShowHex" )
    {
        Self->HexMngr.SwitchShowHex();
    }
    else if( cmd == "SwitchFullscreen" )
    {
        if( !GameOpt.FullScreen )
        {
            if( !SDL_SetWindowFullscreen( MainWindow, SDL_WINDOW_FULLSCREEN_DESKTOP ) )
                GameOpt.FullScreen = true;
        }
        else
        {
            if( !SDL_SetWindowFullscreen( MainWindow, 0 ) )
                GameOpt.FullScreen = false;
        }
        SprMngr.RefreshViewport();
    }
    else if( cmd == "MinimizeWindow" )
    {
        SDL_MinimizeWindow( MainWindow );
    }
    else if( cmd == "SwitchLookBorders" )
    {
        Self->DrawLookBorders = !Self->DrawLookBorders;
        Self->RebuildLookBorders = true;
    }
    else if( cmd == "SwitchShootBorders" )
    {
        Self->DrawShootBorders = !Self->DrawShootBorders;
        Self->RebuildLookBorders = true;
    }
    else if( cmd == "SwitchSingleplayerPause" )
    {
        SingleplayerData.Pause = !SingleplayerData.Pause;
    }
    else if( cmd == "SetCursorPos" )
    {
        if( Self->HexMngr.IsMapLoaded() )
            Self->HexMngr.SetCursorPos( GameOpt.MouseX, GameOpt.MouseY, Keyb::CtrlDwn, true );
    }
    else if( cmd == "NetDisconnect" )
    {
        Self->NetDisconnect();
    }
    else if( cmd == "TryExit" )
    {
        Self->TryExit();
    }
    else if( cmd == "Version" )
    {
        char buf[ 1024 ];
        return ScriptString::Create( Str::Format( buf, "%d", FONLINE_VERSION ) );
    }
    else if( cmd == "BytesSend" )
    {
        return ScriptString::Create( Str::ItoA( Self->BytesSend ) );
    }
    else if( cmd == "BytesReceive" )
    {
        return ScriptString::Create( Str::ItoA( Self->BytesReceive ) );
    }
    else if( cmd == "GetLanguage" )
    {
        return ScriptString::Create( Self->CurLang.NameStr );
    }
    else if( cmd == "SetLanguage" && args.size() >= 2 )
    {
        if( args[ 1 ].length() == 4 )
            Self->CurLang.LoadFromCache( args[ 1 ].c_str() );
    }
    else if( cmd == "SetResolution" && args.size() >= 3 )
    {
        int w = Str::AtoI( args[ 1 ].c_str() );
        int h = Str::AtoI( args[ 2 ].c_str() );
        int diff_w = w - GameOpt.ScreenWidth;
        int diff_h = h - GameOpt.ScreenHeight;

        GameOpt.ScreenWidth = w;
        GameOpt.ScreenHeight = h;
        SDL_SetWindowSize( MainWindow, w, h );

        int x, y;
        SDL_GetWindowPosition( MainWindow, &x, &y );
        SDL_SetWindowPosition( MainWindow, x - diff_w / 2, y - diff_h / 2 );

        SprMngr.OnResolutionChanged();
        if( Self->HexMngr.IsMapLoaded() )
            Self->HexMngr.OnResolutionChanged();
    }
    else if( cmd == "Command" && args.size() >= 2 )
    {
        char str[ MAX_FOTEXT ] = { 0 };

        for( uint i = 1, j = (uint) args.size(); i < j; i++ )
        {
            if( i > 1 )
                Str::Append( str, " " );
            Str::Append( str, args[ i ].c_str() );
        }

        static char buf[ MAX_FOTEXT ];
        static char buf_separator[ MAX_FOTEXT ];
        struct LogCB
        {
            static void Message( const char* msg )
            {
                if( Str::Length( buf ) > 0 && Str::Length( buf_separator ) > 0 )
                    Str::Append( buf, buf_separator );

                Str::Append( buf, msg );
            }
        };

        Str::Copy( buf, "" );
        Str::Copy( buf_separator, separator.c_str() );

        if( !PackCommand( str, Self->Bout, LogCB::Message, Self->Chosen->Name->c_str() ) )
            return ScriptString::Create( "UNKNOWN" );

        return ScriptString::Create( buf );
    }
    else if( cmd == "ConsoleMessage" && args.size() >= 2 )
    {
        Self->Net_SendText( args[ 1 ].c_str(), SAY_NORM );
    }
    else if( cmd == "ChangeSlot" )
    {
        Self->ChosenChangeSlot();
    }
    else if( cmd == "UseMainItem" )
    {
        if( Self->Chosen->GetUse() == USE_RELOAD )
        {
            Self->SetAction( CHOSEN_USE_ITEM, Self->Chosen->ItemSlotMain->GetId(), 0, TARGET_SELF_ITEM, 0, USE_RELOAD );
        }
        else if( Self->Chosen->GetUse() < MAX_USES && Self->Chosen->ItemSlotMain->IsWeapon() )
        {
            Self->SetCurMode( CUR_USE_WEAPON );
        }
        else if( Self->Chosen->GetUse() == USE_USE && Self->Chosen->ItemSlotMain->IsCanUseOnSmth() )
        {
            Self->SetCurMode( CUR_USE_ITEM );
        }
        else if( Self->Chosen->GetUse() == USE_USE && Self->Chosen->ItemSlotMain->IsCanUse() )
        {
            if( !Self->Chosen->ItemSlotMain->IsHasTimer() )
                Self->SetAction( CHOSEN_USE_ITEM, Self->Chosen->ItemSlotMain->GetId(), 0, TARGET_SELF, 0, USE_USE );
            else
                Self->TimerStart( Self->Chosen->ItemSlotMain->GetId(), ResMngr.GetInvAnim( Self->Chosen->ItemSlotMain->GetActualPicInv() ), Self->Chosen->ItemSlotMain->GetInvColor() );
        }
    }
    else if( cmd == "IsTurnBasedMyTurn" )
    {
        return ScriptString::Create( Self->IsTurnBasedMyTurn() ? "true" : "false" );
    }
    else if( cmd == "EndTurn" )
    {
        if( Self->IsTurnBasedMyTurn() )
        {
            Self->Net_SendCombat( COMBAT_TB_END_TURN, 1 );
            Self->TurnBasedTime = 0;
            Self->TurnBasedCurCritterId = 0;
        }
    }
    else if( cmd == "EndCombat" )
    {
        if( Self->IsTurnBased )
        {
            if( Self->Chosen->GetIsEndCombat() )
                Self->Net_SendCombat( COMBAT_TB_END_COMBAT, 0 );
            else
                Self->Net_SendCombat( COMBAT_TB_END_COMBAT, 1 );
            Self->Chosen->SetIsEndCombat( !Self->Chosen->GetIsEndCombat() );
        }
    }
    else if( cmd == "NextItemMode" )
    {
        Self->Chosen->NextRateItem( args.size() >= 2 && args[ 1 ] == "Prev" ? true : false );
    }
    else if( cmd == "GameLMouseDown" )
    {
        Self->GameLMouseDown();
    }
    else if( cmd == "GameLMouseUp" )
    {
        Self->GameLMouseUp();
    }
    else if( cmd == "NextCursor" )
    {
        switch( Self->GetCurMode() )
        {
        case CUR_DEFAULT:
            Self->SetCurMode( CUR_MOVE );
            break;
        case CUR_MOVE:
            if( IS_TIMEOUT( Self->Chosen->GetTimeoutBattle() ) && Self->Chosen->ItemSlotMain->IsWeapon() )
                Self->SetCurMode( CUR_USE_WEAPON );
            else
                Self->SetCurMode( CUR_DEFAULT );
            break;
        case CUR_USE_ITEM:
            Self->SetCurMode( CUR_DEFAULT );
            break;
        case CUR_USE_WEAPON:
            Self->SetCurMode( CUR_DEFAULT );
            break;
        case CUR_USE_SKILL:
            Self->SetCurMode( CUR_MOVE );
            break;
        default:
            Self->SetCurMode( CUR_DEFAULT );
            break;
        }
    }
    else if( cmd == "TryPickItemOnGround" )
    {
        Self->TryPickItemOnGround();
    }
    else if( cmd == "ProcessMouseScroll" )
    {
        Self->ProcessMouseScroll();
    }
    else if( cmd == "LMenuDraw" )
    {
        Self->LMenuDraw();
    }
    else if( cmd == "CurDrawHand" )
    {
        Self->CurDrawHand();
    }
    else if( cmd == "SetMousePos" )
    {
        int x = Str::AtoI( args.size() >= 2 ? args[ 1 ].c_str() : "0" );
        int y = Str::AtoI( args.size() >= 3 ? args[ 2 ].c_str() : "0" );
        Self->SetCurPos( x, y );
    }
    else if( cmd == "SplitDrop" )
    {
        uint  item_id = Str::AtoI( args.size() >= 2 ? args[ 1 ].c_str() : "0" );
        Item* item = Self->Chosen->GetItem( item_id );
        Self->SplitStart( item, SLOT_GROUND );
    }
    else if( cmd == "IsLMenu" )
    {
        return ScriptString::Create( Self->IsLMenu() ? "true" : "false" );
    }
    else if( cmd == "LMenuTryActivate" )
    {
        Self->LMenuTryActivate();
    }
    else if( cmd == "SaveGame" )
    {
        Self->SaveLoadLoginScreen = false;
        Self->SaveLoadSave = true;
        Self->ShowScreen( SCREEN__SAVE_LOAD );
    }
    else if( cmd == "LoadGame" )
    {
        Self->SaveLoadLoginScreen = false;
        Self->SaveLoadSave = false;
        Self->ShowScreen( SCREEN__SAVE_LOAD );
    }
    else if( cmd == "AssignSkillPoints" )
    {
        IntVec props_data;
        for( size_t i = 1; i < args.size(); i += 2 )
        {
            props_data.push_back( Str::AtoI( args[ i ].c_str() ) );
            props_data.push_back( Str::AtoI( args[ i + 1 ].c_str() ) );
        }
        Self->Net_SendLevelUp( 0xFFFF, &props_data );
    }
    else if( cmd == "GmapHome" )
    {
        GmapOffsetX = Self->GmapWMap.W() / 2 + Self->GmapWMap[ 0 ] - (int) ( GmapGroupCurX / GmapZoom );
        GmapOffsetY = Self->GmapWMap.H() / 2 + Self->GmapWMap[ 1 ] - (int) ( GmapGroupCurY / GmapZoom );
        if( GmapOffsetX > Self->GmapWMap[ 0 ] )
            GmapOffsetX = Self->GmapWMap[ 0 ];
        if( GmapOffsetY > Self->GmapWMap[ 1 ] )
            GmapOffsetY = Self->GmapWMap[ 1 ];
        if( GmapOffsetX < Self->GmapWMap[ 2 ] - (int) ( GM_MAXX / GmapZoom ) )
            GmapOffsetX = Self->GmapWMap[ 2 ] - (int) ( GM_MAXX / GmapZoom );
        if( GmapOffsetY < Self->GmapWMap[ 3 ] - (int) ( GM_MAXY / GmapZoom ) )
            GmapOffsetY = Self->GmapWMap[ 3 ] - (int) ( GM_MAXY / GmapZoom );
    }
    else if( cmd == "SaveLog" && args.size() == 3 )
    {
//              if( file_name == "" )
//              {
//                      DateTime dt;
//                      Timer::GetCurrentDateTime(dt);
//                      char     log_path[MAX_FOPATH];
//                      Str::Format(log_path, "messbox_%04d.%02d.%02d_%02d-%02d-%02d.txt",
//                              dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
//              }

//              for (uint i = 0; i < MessBox.size(); ++i)
//              {
//                      MessBoxMessage& m = MessBox[i];
//                      // Skip
//                      if (IsMainScreen(SCREEN_GAME) && std::find(MessBoxFilters.begin(), MessBoxFilters.end(), m.Type) != MessBoxFilters.end())
//                              continue;
//                      // Concat
//                      Str::Copy(cur_mess, m.Mess.c_str());
//                      Str::EraseWords(cur_mess, '|', ' ');
//                      fmt_log += MessBox[i].Time + string(cur_mess);
//              }


    }
    else
    {
        SCRIPT_ERROR_R0( "Invalid custom call command." );
    }
    return ScriptString::Create();
}

CritterCl* FOClient::SScriptFunc::Global_GetChosen()
{
    if( Self->Chosen && Self->Chosen->IsNotValid )
        return NULL;
    return Self->Chosen;
}

uint FOClient::SScriptFunc::Global_GetChosenActions( ScriptArray* actions )
{
    if( actions )
        actions->Resize( 0 );

    if( Self->Chosen )
    {
        actions->Resize( (uint) Self->ChosenAction.size() * 7 );
        for( uint i = 0, j = (uint) Self->ChosenAction.size(); i < j; i++ )
        {
            ActionEvent& act = Self->ChosenAction[ i ];
            *(uint*) actions->At( i * 7 + 0 ) = act.Type;
            *(uint*) actions->At( i * 7 + 1 ) = act.Param[ 0 ];
            *(uint*) actions->At( i * 7 + 2 ) = act.Param[ 1 ];
            *(uint*) actions->At( i * 7 + 3 ) = act.Param[ 2 ];
            *(uint*) actions->At( i * 7 + 4 ) = act.Param[ 3 ];
            *(uint*) actions->At( i * 7 + 5 ) = act.Param[ 4 ];
            *(uint*) actions->At( i * 7 + 6 ) = act.Param[ 5 ];
        }
        return (uint) Self->ChosenAction.size();
    }
    return 0;
}

void FOClient::SScriptFunc::Global_SetChosenActions( ScriptArray* actions )
{
    if( actions && actions->GetSize() % 7 )
        SCRIPT_ERROR_R( "Wrong action array size." );
    Self->ChosenAction.clear();

    if( Self->Chosen && actions )
    {
        Self->ChosenAction.resize( actions->GetSize() / 7 );
        for( uint i = 0, j = actions->GetSize() / 7; i < j; i++ )
        {
            ActionEvent& act = Self->ChosenAction[ i ];
            act.Type = *(uint*) actions->At( i * 7 + 0 );
            act.Param[ 0 ] = *(uint*) actions->At( i * 7 + 1 );
            act.Param[ 1 ] = *(uint*) actions->At( i * 7 + 2 );
            act.Param[ 2 ] = *(uint*) actions->At( i * 7 + 3 );
            act.Param[ 3 ] = *(uint*) actions->At( i * 7 + 4 );
            act.Param[ 4 ] = *(uint*) actions->At( i * 7 + 5 );
            act.Param[ 5 ] = *(uint*) actions->At( i * 7 + 6 );
        }
    }
}

Item* FOClient::SScriptFunc::Global_GetItem( uint item_id )
{
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );
    Item* item = Self->GetItem( item_id );
    if( !item || item->IsNotValid )
        return NULL;
    return item;
}

uint FOClient::SScriptFunc::Global_GetCrittersDistantion( CritterCl* cr1, CritterCl* cr2 )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    if( cr1->IsNotValid )
        SCRIPT_ERROR_R0( "Critter1 arg nullptr." );
    if( cr2->IsNotValid )
        SCRIPT_ERROR_R0( "Critter2 arg nullptr." );
    return DistGame( cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY() );
}

CritterCl* FOClient::SScriptFunc::Global_GetCritter( uint critter_id )
{
    if( !critter_id )
        return NULL;                // SCRIPT_ERROR_R0("CritterCl id arg is zero.");
    CritterCl* cr = Self->GetCritter( critter_id );
    if( !cr || cr->IsNotValid )
        return NULL;
    return cr;
}

uint FOClient::SScriptFunc::Global_GetCritters( ushort hx, ushort hy, uint radius, int find_type, ScriptArray* critters )
{
    if( hx >= Self->HexMngr.GetMaxHexX() || hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    CritMap& crits = Self->HexMngr.GetCritters();
    CritVec  cr_vec;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        CritterCl* cr = ( *it ).second;
        if( cr->CheckFind( find_type ) && CheckDist( hx, hy, cr->GetHexX(), cr->GetHexY(), radius ) )
            cr_vec.push_back( cr );
    }

    if( critters )
    {
        SortCritterByDist( hx, hy, cr_vec );
        Script::AppendVectorToArrayRef< CritterCl* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

uint FOClient::SScriptFunc::Global_GetCrittersByPids( hash pid, int find_type, ScriptArray* critters )
{
    CritMap& crits = Self->HexMngr.GetCritters();
    CritVec  cr_vec;
    if( !pid )
    {
        for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
        {
            CritterCl* cr = ( *it ).second;
            if( cr->CheckFind( find_type ) )
                cr_vec.push_back( cr );
        }
    }
    else
    {
        for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
        {
            CritterCl* cr = ( *it ).second;
            if( cr->IsNpc() && cr->Pid == pid && cr->CheckFind( find_type ) )
                cr_vec.push_back( cr );
        }
    }
    if( cr_vec.empty() )
        return 0;
    if( critters )
        Script::AppendVectorToArrayRef< CritterCl* >( cr_vec, critters );
    return (uint) cr_vec.size();
}

uint FOClient::SScriptFunc::Global_GetCrittersInPath( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters )
{
    CritVec cr_vec;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, NULL, false, &cr_vec, find_type, NULL, NULL, NULL, true );
    if( critters )
        Script::AppendVectorToArrayRef< CritterCl* >( cr_vec, critters );
    return (uint) cr_vec.size();
}

uint FOClient::SScriptFunc::Global_GetCrittersInPathBlock( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy )
{
    CritVec    cr_vec;
    UShortPair block, pre_block;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, NULL, false, &cr_vec, find_type, &block, &pre_block, NULL, true );
    if( critters )
        Script::AppendVectorToArrayRef< CritterCl* >( cr_vec, critters );
    pre_block_hx = pre_block.first;
    pre_block_hy = pre_block.second;
    block_hx = block.first;
    block_hy = block.second;
    return (uint) cr_vec.size();
}

void FOClient::SScriptFunc::Global_GetHexInPath( ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    UShortPair pre_block, block;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, NULL, false, NULL, 0, &block, &pre_block, NULL, true );
    to_hx = pre_block.first;
    to_hy = pre_block.second;
}

uint FOClient::SScriptFunc::Global_GetPathLengthHex( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut )
{
    if( from_hx >= Self->HexMngr.GetMaxHexX() || from_hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid from hexes args." );
    if( to_hx >= Self->HexMngr.GetMaxHexX() || to_hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    if( cut > 0 && !Self->HexMngr.CutPath( NULL, from_hx, from_hy, to_hx, to_hy, cut ) )
        return 0;
    UCharVec steps;
    if( !Self->HexMngr.FindPath( NULL, from_hx, from_hy, to_hx, to_hy, steps, -1 ) )
        steps.clear();
    return (uint) steps.size();
}

uint FOClient::SScriptFunc::Global_GetPathLengthCr( CritterCl* cr, ushort to_hx, ushort to_hy, uint cut )
{
    if( cr->IsNotValid )
        SCRIPT_ERROR_R0( "Critter arg nullptr." );
    if( to_hx >= Self->HexMngr.GetMaxHexX() || to_hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    if( cut > 0 && !Self->HexMngr.CutPath( cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, cut ) )
        return 0;
    UCharVec steps;
    if( !Self->HexMngr.FindPath( cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, steps, -1 ) )
        steps.clear();
    return (uint) steps.size();
}

void FOClient::SScriptFunc::Global_FlushScreen( uint from_color, uint to_color, uint ms )
{
    Self->ScreenFade( ms, from_color, to_color, true );
}

void FOClient::SScriptFunc::Global_QuakeScreen( uint noise, uint ms )
{
    Self->ScreenQuake( noise, ms );
}

bool FOClient::SScriptFunc::Global_PlaySound( ScriptString& sound_name )
{
    return SndMngr.PlaySound( sound_name.c_str() );
}

bool FOClient::SScriptFunc::Global_PlaySoundType( uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext )
{
    return SndMngr.PlaySoundType( sound_type, sound_type_ext, sound_id, sound_id_ext );
}

bool FOClient::SScriptFunc::Global_PlayMusic( ScriptString& music_name, uint pos, uint repeat )
{
    return SndMngr.PlayMusic( music_name.c_str(), pos, repeat );
}

void FOClient::SScriptFunc::Global_PlayVideo( ScriptString& video_name, bool can_stop )
{
    SndMngr.StopMusic();
    Self->AddVideo( video_name.c_str(), can_stop, true );
}

bool FOClient::SScriptFunc::Global_IsTurnBased()
{
    return Self->HexMngr.IsMapLoaded() && Self->IsTurnBased;
}

uint FOClient::SScriptFunc::Global_GetTurnBasedTime()
{
    if( Self->HexMngr.IsMapLoaded() && Self->IsTurnBased && Self->TurnBasedTime > Timer::GameTick() )
        return Self->TurnBasedTime - Timer::GameTick();
    return 0;
}

hash FOClient::SScriptFunc::Global_GetCurrentMapPid()
{
    if( !Self->HexMngr.IsMapLoaded() )
        return 0;
    return Self->CurMapPid;
}

void FOClient::SScriptFunc::Global_Message( ScriptString& msg )
{
    Self->AddMess( FOMB_GAME, msg.c_str(), true );
}

void FOClient::SScriptFunc::Global_MessageType( ScriptString& msg, int type )
{
    if( type < FOMB_GAME || type > FOMB_VIEW )
        type = FOMB_GAME;
    Self->AddMess( type, msg.c_str(), true );
}

void FOClient::SScriptFunc::Global_MessageMsg( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R( "Invalid text msg arg." );
    Self->AddMess( FOMB_GAME, Self->CurLang.Msg[ text_msg ].GetStr( str_num ), true );
}

void FOClient::SScriptFunc::Global_MessageMsgType( int text_msg, uint str_num, int type )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R( "Invalid text msg arg." );
    if( type < FOMB_GAME || type > FOMB_VIEW )
        type = FOMB_GAME;
    Self->AddMess( type, Self->CurLang.Msg[ text_msg ].GetStr( str_num ), true );
}

void FOClient::SScriptFunc::Global_MapMessage( ScriptString& text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy )
{
    FOClient::MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = ( color ? color : COLOR_TEXT );
    t.Fade = fade;
    t.StartTick = Timer::GameTick();
    t.Tick = ms;
    t.Text = text.c_std_str();
    t.Pos = Self->HexMngr.GetRectForText( hx, hy );
    t.EndPos = Rect( t.Pos, ox, oy );
    auto it = std::find( Self->GameMapTexts.begin(), Self->GameMapTexts.end(), t );
    if( it != Self->GameMapTexts.end() )
        Self->GameMapTexts.erase( it );
    Self->GameMapTexts.push_back( t );
}

ScriptString* FOClient::SScriptFunc::Global_GetMsgStr( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return ScriptString::Create( text_msg == TEXTMSG_HOLO ? Self->GetHoloText( str_num ) : Self->CurLang.Msg[ text_msg ].GetStr( str_num ) );
}

ScriptString* FOClient::SScriptFunc::Global_GetMsgStrSkip( int text_msg, uint str_num, uint skip_count )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return ScriptString::Create( text_msg == TEXTMSG_HOLO ? Self->GetHoloText( str_num ) : Self->CurLang.Msg[ text_msg ].GetStr( str_num, skip_count ) );
}

uint FOClient::SScriptFunc::Global_GetMsgStrNumUpper( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].GetStrNumUpper( str_num );
}

uint FOClient::SScriptFunc::Global_GetMsgStrNumLower( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].GetStrNumLower( str_num );
}

uint FOClient::SScriptFunc::Global_GetMsgStrCount( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].Count( str_num );
}

bool FOClient::SScriptFunc::Global_IsMsgStr( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].Count( str_num ) > 0;
}

ScriptString* FOClient::SScriptFunc::Global_ReplaceTextStr( ScriptString& text, ScriptString& replace, ScriptString& str )
{
    size_t pos = text.c_std_str().find( replace.c_std_str(), 0 );
    if( pos == std::string::npos )
        return ScriptString::Create( text );
    string result = text.c_std_str();
    return ScriptString::Create( result.replace( pos, replace.length(), str.c_std_str() ) );
}

ScriptString* FOClient::SScriptFunc::Global_ReplaceTextInt( ScriptString& text, ScriptString& replace, int i )
{
    size_t pos = text.c_std_str().find( replace.c_std_str(), 0 );
    if( pos == std::string::npos )
        return ScriptString::Create( text );
    char   val[ 32 ];
    Str::Format( val, "%d", i );
    string result = text.c_std_str();
    return ScriptString::Create( result.replace( pos, replace.length(), val ) );
}

ScriptString* FOClient::SScriptFunc::Global_FormatTags( ScriptString& text, ScriptString* lexems )
{
    static char* buf = NULL;
    static uint  buf_len = 0;
    if( !buf )
    {
        buf = new char[ MAX_FOTEXT ];
        if( !buf )
            SCRIPT_ERROR_R0( "Allocation fail." );
        buf_len = MAX_FOTEXT;
    }

    if( buf_len < text.length() * 2 )
    {
        delete[] buf;
        buf = new char[ text.length() * 2 ];
        if( !buf )
            SCRIPT_ERROR_R0( "Reallocation fail." );
        buf_len = (uint) text.length() * 2;
    }

    Str::Copy( buf, buf_len, text.c_str() );
    Self->FormatTags( buf, buf_len, Self->Chosen, NULL, lexems ? lexems->c_str() : NULL );
    return ScriptString::Create( buf );
}

int FOClient::SScriptFunc::Global_GetSomeValue( int var )
{
    switch( var )
    {
    case 0:
    case 1:
        break;
    }
    return 0;
}

void FOClient::SScriptFunc::Global_MoveScreen( ushort hx, ushort hy, uint speed, bool can_stop )
{
    if( hx >= Self->HexMngr.GetMaxHexX() || hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hex args." );
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R( "Map is not loaded." );
    if( !speed )
        Self->HexMngr.FindSetCenter( hx, hy );
    else
        Self->HexMngr.ScrollToHex( hx, hy, double(speed) / 1000.0, can_stop );
}

void FOClient::SScriptFunc::Global_LockScreenScroll( CritterCl* cr, bool unlock_if_same )
{
    if( cr && cr->IsNotValid )
        SCRIPT_ERROR_R( "CritterCl arg nullptr." );

    uint id = ( cr ? cr->GetId() : 0 );
    if( unlock_if_same && id == Self->HexMngr.AutoScroll.LockedCritter )
        Self->HexMngr.AutoScroll.LockedCritter = 0;
    else
        Self->HexMngr.AutoScroll.LockedCritter = id;

}

int FOClient::SScriptFunc::Global_GetFog( ushort zone_x, ushort zone_y )
{
    if( !Self->Chosen || Self->Chosen->IsNotValid )
        SCRIPT_ERROR_R0( "Chosen data not valid." );
    if( zone_x >= GameOpt.GlobalMapWidth || zone_y >= GameOpt.GlobalMapHeight )
        SCRIPT_ERROR_R0( "Invalid world map pos arg." );
    return Self->GmapFog.Get2Bit( zone_x, zone_y );
}

void FOClient::SScriptFunc::Global_RefreshItemsCollection( int collection )
{
    switch( collection )
    {
    case ITEMS_INVENTORY:
        Self->ProcessItemsCollection( ITEMS_INVENTORY, Self->InvContInit, Self->InvCont );
        break;
    case ITEMS_USE:
        Self->ProcessItemsCollection( ITEMS_USE, Self->InvContInit, Self->UseCont );
        break;
    case ITEMS_BARTER:
        Self->ProcessItemsCollection( ITEMS_BARTER, Self->InvContInit, Self->BarterCont1 );
        break;
    case ITEMS_BARTER_OFFER:
        Self->ProcessItemsCollection( ITEMS_BARTER_OFFER, Self->BarterCont1oInit, Self->BarterCont1o );
        break;
    case ITEMS_BARTER_OPPONENT:
        Self->ProcessItemsCollection( ITEMS_BARTER_OPPONENT, Self->BarterCont2Init, Self->BarterCont2 );
        break;
    case ITEMS_BARTER_OPPONENT_OFFER:
        Self->ProcessItemsCollection( ITEMS_BARTER_OPPONENT_OFFER, Self->BarterCont2oInit, Self->BarterCont2o );
        break;
    case ITEMS_PICKUP:
        Self->ProcessItemsCollection( ITEMS_PICKUP, Self->InvContInit, Self->PupCont1 );
        break;
    case ITEMS_PICKUP_FROM:
        Self->ProcessItemsCollection( ITEMS_PICKUP_FROM, Self->PupCont2Init, Self->PupCont2 );
        break;
    default:
        SCRIPT_ERROR_R( "Invalid items collection." );
        break;
    }
}

uint FOClient::SScriptFunc::Global_GetDayTime( uint day_part )
{
    if( day_part >= 4 )
        SCRIPT_ERROR_R0( "Invalid day part arg." );
    if( Self->HexMngr.IsMapLoaded() )
        return Self->HexMngr.GetMapDayTime()[ day_part ];
    return 0;
}

void FOClient::SScriptFunc::Global_GetDayColor( uint day_part, uchar& r, uchar& g, uchar& b )
{
    r = g = b = 0;
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );
    if( Self->HexMngr.IsMapLoaded() )
    {
        uchar* col = Self->HexMngr.GetMapDayColor();
        r = col[ 0 + day_part ];
        g = col[ 4 + day_part ];
        b = col[ 8 + day_part ];
    }
}

ProtoItem* FOClient::SScriptFunc::Global_GetProtoItem( hash proto_id )
{
    return ItemMngr.GetProtoItem( proto_id );
}

uint FOClient::SScriptFunc::Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    if( !year )
        year = GameOpt.Year;
    else
        year = CLAMP( year, GameOpt.YearStart, GameOpt.YearStart + 130 );
    if( !month )
        month = GameOpt.Month;
    else
        month = CLAMP( month, 1, 12 );
    if( !day )
        day = GameOpt.Day;
    else
    {
        uint month_day = Timer::GameTimeMonthDay( year, month );
        day = CLAMP( day, 1, month_day );
    }
    if( hour > 23 )
        hour = 23;
    if( minute > 59 )
        minute = 59;
    if( second > 59 )
        second = 59;
    return Timer::GetFullSecond( year, month, day, hour, minute, second );
}

void FOClient::SScriptFunc::Global_GetGameTime( uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second )
{
    DateTimeStamp dt = Timer::GetGameTime( full_second );
    year = dt.Year;
    month = dt.Month;
    day_of_week = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

void FOClient::SScriptFunc::Global_MoveHexByDir( ushort& hx, ushort& hy, uchar dir, uint steps )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R( "Map not loaded." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid dir arg." );
    if( !steps )
        SCRIPT_ERROR_R( "Steps arg is zero." );
    if( steps > 1 )
    {
        for( uint i = 0; i < steps; i++ )
            MoveHexByDir( hx, hy, dir, Self->HexMngr.GetMaxHexX(), Self->HexMngr.GetMaxHexY() );
    }
    else
    {
        MoveHexByDir( hx, hy, dir, Self->HexMngr.GetMaxHexX(), Self->HexMngr.GetMaxHexY() );
    }
}

bool FOClient::SScriptFunc::Global_AppendIfaceIni( ScriptString& ini_name )
{
    return Self->AppendIfaceIni( ini_name.c_str() );
}

ScriptString* FOClient::SScriptFunc::Global_GetIfaceIniStr( ScriptString& key )
{
    char* big_buf = Str::GetBigBuf();
    if( Self->IfaceIni.GetStr( key.c_str(), "", big_buf ) )
        return ScriptString::Create( big_buf );
    return ScriptString::Create();
}

void FOClient::SScriptFunc::Global_Preload3dFiles( ScriptArray& fnames, int path_type )
{
    size_t k = Self->Preload3dFiles.size();

    for( asUINT i = 0; i < fnames.GetSize(); i++ )
    {
        ScriptString* s = (ScriptString*) fnames.At( i );
        Self->Preload3dFiles.push_back( s->c_std_str() );
    }

    for( ; k < Self->Preload3dFiles.size(); k++ )
        Self->Preload3dFiles[ k ] = FileManager::GetDataPath( Self->Preload3dFiles[ k ].c_str(), path_type );
}

void FOClient::SScriptFunc::Global_WaitPing()
{
    Self->WaitPing();
}

bool FOClient::SScriptFunc::Global_LoadFont( int font_index, ScriptString& font_fname )
{
    SprMngr.PushAtlasType( RES_ATLAS_STATIC );
    bool result;
    if( font_fname.length() > 0 && font_fname.c_str()[ 0 ] == '*' )
        result = SprMngr.LoadFontFO( font_index, font_fname.c_str() + 1, false, false );
    else
        result = SprMngr.LoadFontBMF( font_index, font_fname.c_str() );
    if( result && !SprMngr.IsAccumulateAtlasActive() )
        SprMngr.BuildFonts();
    SprMngr.PopAtlasType();
    return result;
}

void FOClient::SScriptFunc::Global_SetDefaultFont( int font, uint color )
{
    SprMngr.SetDefaultFont( font, color );
}

bool FOClient::SScriptFunc::Global_SetEffect( int effect_type, int effect_subtype, ScriptString* effect_name, ScriptString* effect_defines )
{
    // Effect types
    #define EFFECT_2D_GENERIC                ( 0x00000001 ) // Subtype can be item id, zero for all items
    #define EFFECT_2D_CRITTER                ( 0x00000002 ) // Subtype can be critter id, zero for all critters
    #define EFFECT_2D_TILE                   ( 0x00000004 )
    #define EFFECT_2D_ROOF                   ( 0x00000008 )
    #define EFFECT_2D_RAIN                   ( 0x00000010 )
    #define EFFECT_3D_SKINNED                ( 0x00000400 )
    #define EFFECT_3D_SKINNED_SHADOW         ( 0x00000800 )
    #define EFFECT_INTERFACE_BASE            ( 0x00001000 )
    #define EFFECT_INTERFACE_CONTOUR         ( 0x00002000 )
    #define EFFECT_FONT                      ( 0x00010000 ) // Subtype is FONT_*, -1 default for all fonts
    #define EFFECT_PRIMITIVE_GENERIC         ( 0x00100000 )
    #define EFFECT_PRIMITIVE_LIGHT           ( 0x00200000 )
    #define EFFECT_FLUSH_RENDER_TARGET       ( 0x01000000 )
    #define EFFECT_FLUSH_RENDER_TARGET_MS    ( 0x02000000 ) // Multisample
    #define EFFECT_FLUSH_PRIMITIVE           ( 0x04000000 )
    #define EFFECT_FLUSH_MAP                 ( 0x08000000 )

    Effect* effect = NULL;
    if( effect_name && effect_name->length() )
    {
        bool use_in_2d = !( effect_type & ( EFFECT_3D_SKINNED | EFFECT_3D_SKINNED_SHADOW ) );
        effect = GraphicLoader::LoadEffect( effect_name->c_str(), use_in_2d, effect_defines ? effect_defines->c_str() : NULL );
        if( !effect )
            SCRIPT_ERROR_R0( "Effect not found or have some errors, see log file." );
    }

    if( effect_type & EFFECT_2D_GENERIC && effect_subtype != 0 )
    {
        ItemHex* item = Self->GetItem( (uint) effect_subtype );
        if( item )
            item->DrawEffect = ( effect ? effect : Effect::Generic );
    }
    if( effect_type & EFFECT_2D_CRITTER && effect_subtype != 0 )
    {
        CritterCl* cr = Self->GetCritter( (uint) effect_subtype );
        if( cr )
            cr->DrawEffect = ( effect ? effect : Effect::Critter );
    }

    if( effect_type & EFFECT_2D_GENERIC && effect_subtype == 0 )
        *Effect::Generic = ( effect ? *effect : *Effect::GenericDefault );
    if( effect_type & EFFECT_2D_CRITTER && effect_subtype == 0 )
        *Effect::Critter = ( effect ? *effect : *Effect::CritterDefault );
    if( effect_type & EFFECT_2D_TILE )
        *Effect::Tile = ( effect ? *effect : *Effect::TileDefault );
    if( effect_type & EFFECT_2D_ROOF )
        *Effect::Roof = ( effect ? *effect : *Effect::RoofDefault );
    if( effect_type & EFFECT_2D_RAIN )
        *Effect::Rain = ( effect ? *effect : *Effect::RainDefault );

    if( effect_type & EFFECT_3D_SKINNED )
        *Effect::Skinned3d = ( effect ? *effect : *Effect::Skinned3dDefault );
    if( effect_type & EFFECT_3D_SKINNED_SHADOW )
        *Effect::Skinned3dShadow = ( effect ? *effect : *Effect::Skinned3dShadowDefault );

    if( effect_type & EFFECT_INTERFACE_BASE )
        *Effect::Iface = ( effect ? *effect : *Effect::IfaceDefault );
    if( effect_type & EFFECT_INTERFACE_CONTOUR )
        *Effect::Contour = ( effect ? *effect : *Effect::ContourDefault );

    if( effect_type & EFFECT_FONT && effect_subtype == -1 )
        *Effect::Font = ( effect ? *effect : *Effect::ContourDefault );
    if( effect_type & EFFECT_FONT && effect_subtype >= 0 )
        SprMngr.SetFontEffect( effect_subtype, effect );

    if( effect_type & EFFECT_PRIMITIVE_GENERIC )
        *Effect::Primitive = ( effect ? *effect : *Effect::PrimitiveDefault );
    if( effect_type & EFFECT_PRIMITIVE_LIGHT )
        *Effect::Light = ( effect ? *effect : *Effect::LightDefault );

    if( effect_type & EFFECT_FLUSH_RENDER_TARGET )
        *Effect::FlushRenderTarget = ( effect ? *effect : *Effect::FlushRenderTargetDefault );
    if( effect_type & EFFECT_FLUSH_RENDER_TARGET_MS )
        *Effect::FlushRenderTargetMS = ( effect ? *effect : *Effect::FlushRenderTargetMSDefault );
    if( effect_type & EFFECT_FLUSH_PRIMITIVE )
        *Effect::FlushPrimitive = ( effect ? *effect : *Effect::FlushPrimitiveDefault );
    if( effect_type & EFFECT_FLUSH_MAP )
        *Effect::FlushMap = ( effect ? *effect : *Effect::FlushMapDefault );

    return true;
}

void FOClient::SScriptFunc::Global_RefreshMap( bool only_tiles, bool only_roof, bool only_light )
{
    if( Self->HexMngr.IsMapLoaded() )
    {
        if( only_tiles )
            Self->HexMngr.RebuildTiles();
        else if( only_roof )
            Self->HexMngr.RebuildRoof();
        else if( only_light )
            Self->HexMngr.RebuildLight();
        else
            Self->HexMngr.RefreshMap();
    }
}

void FOClient::SScriptFunc::Global_MouseClick( int x, int y, int button, int cursor )
{
    IntVec prev_events = MainWindowMouseEvents;
    MainWindowMouseEvents.clear();
    int    prev_x = GameOpt.MouseX;
    int    prev_y = GameOpt.MouseY;
    int    prev_cursor = Self->CurMode;
    GameOpt.MouseX = x;
    GameOpt.MouseY = y;
    if( cursor != -1 )
        Self->CurMode = cursor;
    MainWindowMouseEvents.push_back( SDL_MOUSEBUTTONDOWN );
    MainWindowMouseEvents.push_back( button );
    MainWindowMouseEvents.push_back( SDL_MOUSEBUTTONUP );
    MainWindowMouseEvents.push_back( button );
    Self->ParseMouse();
    MainWindowMouseEvents = prev_events;
    GameOpt.MouseX = prev_x;
    GameOpt.MouseY = prev_y;
    if( cursor != -1 )
        Self->CurMode = prev_cursor;
}

void FOClient::SScriptFunc::Global_KeyboardPress( uchar key1, uchar key2, ScriptString* key1_text, ScriptString* key2_text )
{
    if( !key1 && !key2 )
        return;

    IntVec prev_events = MainWindowKeyboardEvents;
    StrVec prev_events_text = MainWindowKeyboardEventsText;
    MainWindowKeyboardEvents.clear();
    if( key1 )
    {
        MainWindowKeyboardEvents.push_back( SDL_KEYDOWN );
        MainWindowKeyboardEvents.push_back( Keyb::UnmapKey( key1 ) );
        MainWindowKeyboardEventsText.push_back( key1_text ? key1_text->c_std_str() : "" );
    }
    if( key2 )
    {
        MainWindowKeyboardEvents.push_back( SDL_KEYDOWN );
        MainWindowKeyboardEvents.push_back( Keyb::UnmapKey( key2 ) );
        MainWindowKeyboardEventsText.push_back( key2_text ? key2_text->c_std_str() : "" );
        MainWindowKeyboardEvents.push_back( SDL_KEYUP );
        MainWindowKeyboardEvents.push_back( Keyb::UnmapKey( key2 ) );
        MainWindowKeyboardEventsText.push_back( "" );
    }
    if( key1 )
    {
        MainWindowKeyboardEvents.push_back( SDL_KEYUP );
        MainWindowKeyboardEvents.push_back( Keyb::UnmapKey( key1 ) );
        MainWindowKeyboardEventsText.push_back( "" );
    }
    Self->ParseKeyboard();
    MainWindowKeyboardEvents = prev_events;
    MainWindowKeyboardEventsText = prev_events_text;
}

void FOClient::SScriptFunc::Global_SetRainAnimation( ScriptString* fall_anim_name, ScriptString* drop_anim_name )
{
    Self->HexMngr.SetRainAnimation( fall_anim_name ? fall_anim_name->c_str() : NULL, drop_anim_name ? drop_anim_name->c_str() : NULL );
}

void FOClient::SScriptFunc::Global_ChangeZoom( float target_zoom )
{
    if( target_zoom == GameOpt.SpritesZoom )
        return;

    float init_zoom = GameOpt.SpritesZoom;
    if( target_zoom == 1.0f )
    {
        Self->HexMngr.ChangeZoom( 0 );
    }
    else if( target_zoom > GameOpt.SpritesZoom )
    {
        while( target_zoom > GameOpt.SpritesZoom )
        {
            float old_zoom = GameOpt.SpritesZoom;
            Self->HexMngr.ChangeZoom( 1 );
            if( GameOpt.SpritesZoom == old_zoom )
                break;
        }
    }
    else if( target_zoom < GameOpt.SpritesZoom )
    {
        while( target_zoom < GameOpt.SpritesZoom )
        {
            float old_zoom = GameOpt.SpritesZoom;
            Self->HexMngr.ChangeZoom( -1 );
            if( GameOpt.SpritesZoom == old_zoom )
                break;
        }
    }

    if( init_zoom != GameOpt.SpritesZoom )
        Self->RebuildLookBorders = true;
}

void FOClient::SScriptFunc::Global_GetTime( ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds )
{
    DateTimeStamp dt;
    Timer::GetCurrentDateTime( dt );
    year = dt.Year;
    month = dt.Month;
    day_of_week = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
    milliseconds = dt.Milliseconds;
}

bool FOClient::SScriptFunc::Global_SetPropertyGetCallback( int prop_enum_value, ScriptString& script_func )
{
    Property* prop = CritterCl::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R0( "Property not found." );

    string result = prop->SetGetCallback( script_func.c_str() );
    if( result != "" )
        SCRIPT_ERROR_R0( result.c_str() );
    return true;
}

bool FOClient::SScriptFunc::Global_AddPropertySetCallback( int prop_enum_value, ScriptString& script_func )
{
    Property* prop = CritterCl::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R0( "Property not found." );

    string result = prop->AddSetCallback( script_func.c_str() );
    if( result != "" )
        SCRIPT_ERROR_R0( result.c_str() );
    return true;
}

void FOClient::SScriptFunc::Global_AllowSlot( uchar index, bool enable_send )
{
    CritterCl::SlotEnabled[ index ] = true;
}

void FOClient::SScriptFunc::Global_AddRegistrationProperty( int cr_prop )
{
    CritterCl::RegProperties.insert( cr_prop );

    ScriptArray* props_array;
    int          props_array_index = Script::GetEngine()->GetGlobalPropertyIndexByName( "CritterPropertyRegProperties" );
    Script::GetEngine()->GetGlobalPropertyByIndex( props_array_index, NULL, NULL, NULL, NULL, NULL, (void**) &props_array );
    props_array->Resize( 0 );
    for( auto it = CritterCl::RegProperties.begin(); it != CritterCl::RegProperties.end(); ++it )
        props_array->InsertLast( (void*) &( *it ) );
}

bool FOClient::SScriptFunc::Global_LoadDataFile( ScriptString& dat_name )
{
    if( FileManager::LoadDataFile( dat_name.c_str() ) )
    {
        ResMngr.Refresh();
        ConstantsManager::Initialize( PT_DATA );
        return true;
    }
    return false;
}

int FOClient::SScriptFunc::Global_GetConstantValue( int const_collection, ScriptString* name )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    if( !name || !name->length() )
        SCRIPT_ERROR_R0( "Invalid name arg." );
    return ConstantsManager::GetValue( const_collection, name->c_str() );
}

ScriptString* FOClient::SScriptFunc::Global_GetConstantName( int const_collection, int value )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    const char* name = ConstantsManager::GetName( const_collection, value );
    return ScriptString::Create( name ? name : "" );
}

void FOClient::SScriptFunc::Global_AddConstant( int const_collection, ScriptString* name, int value )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R( "Invalid namesFile arg." );
    if( !name || !name->length() )
        SCRIPT_ERROR_R( "Invalid name arg." );
    ConstantsManager::AddConstant( const_collection, name->c_str(), value );
}

bool FOClient::SScriptFunc::Global_LoadConstants( int const_collection, ScriptString* file_name, int path_type )
{
    if( const_collection < 0 || const_collection > 1000 )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    if( !file_name || !file_name->length() )
        SCRIPT_ERROR_R0( "Invalid fileName arg." );
    return ConstantsManager::AddCollection( const_collection, file_name->c_str(), path_type );
}

bool FOClient::SScriptFunc::Global_IsCritterCanWalk( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanWalk( cr_type );
}

bool FOClient::SScriptFunc::Global_IsCritterCanRun( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanRun( cr_type );
}

bool FOClient::SScriptFunc::Global_IsCritterCanRotate( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanRotate( cr_type );
}

bool FOClient::SScriptFunc::Global_IsCritterCanAim( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanAim( cr_type );
}

bool FOClient::SScriptFunc::Global_IsCritterCanArmor( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanArmor( cr_type );
}

bool FOClient::SScriptFunc::Global_IsCritterAnim1( uint cr_type, uint index )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsAnim1( cr_type, index );
}

int FOClient::SScriptFunc::Global_GetCritterAnimType( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::GetAnimType( cr_type );
}

uint FOClient::SScriptFunc::Global_GetCritterAlias( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::GetAlias( cr_type );
}

ScriptString* FOClient::SScriptFunc::Global_GetCritterTypeName( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return ScriptString::Create( CritType::GetCritType( cr_type ).Name );
}

ScriptString* FOClient::SScriptFunc::Global_GetCritterSoundName( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return ScriptString::Create( CritType::GetSoundName( cr_type ) );
}

void FOClient::SScriptFunc::Global_RunServerScript( ScriptString& func_name, int p0, int p1, int p2, ScriptString* p3, ScriptArray* p4 )
{
    UIntVec dw;
    if( p4 )
        Script::AssignScriptArrayInVector< uint >( dw, p4 );
    Self->Net_SendRunScript( false, func_name.c_str(), p0, p1, p2, p3 ? p3->c_str() : NULL, dw );
}

void FOClient::SScriptFunc::Global_RunServerScriptUnsafe( ScriptString& func_name, int p0, int p1, int p2, ScriptString* p3, ScriptArray* p4 )
{
    UIntVec dw;
    if( p4 )
        Script::AssignScriptArrayInVector< uint >( dw, p4 );
    Self->Net_SendRunScript( true, func_name.c_str(), p0, p1, p2, p3 ? p3->c_str() : NULL, dw );
}

uint FOClient::SScriptFunc::Global_LoadSprite( ScriptString& spr_name, int path_index )
{
    if( path_index >= PATH_LIST_COUNT )
        SCRIPT_ERROR_R0( "Invalid path index arg." );
    return Self->AnimLoad( spr_name.c_str(), path_index, RES_ATLAS_STATIC );
}

uint FOClient::SScriptFunc::Global_LoadSpriteHash( uint name_hash )
{
    return Self->AnimLoad( name_hash, RES_ATLAS_STATIC );
}

int FOClient::SScriptFunc::Global_GetSpriteWidth( uint spr_id, int frame_index )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return 0;
    SpriteInfo* si = SprMngr.GetSpriteInfo( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ) );
    if( !si )
        return 0;
    return si->Width;
}

int FOClient::SScriptFunc::Global_GetSpriteHeight( uint spr_id, int frame_index )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return 0;
    SpriteInfo* si = SprMngr.GetSpriteInfo( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ) );
    if( !si )
        return 0;
    return si->Height;
}

uint FOClient::SScriptFunc::Global_GetSpriteCount( uint spr_id )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    return anim ? anim->CntFrm : 0;
}

uint FOClient::SScriptFunc::Global_GetSpriteTicks( uint spr_id )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    return anim ? anim->Ticks : 0;
}

uint FOClient::SScriptFunc::Global_GetPixelColor( uint spr_id, int frame_index, int x, int y )
{
    if( !spr_id )
        return 0;

    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return 0;

    uint spr_id_ = ( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ) );
    return SprMngr.GetPixColor( spr_id_, x, y, false );
}

void FOClient::SScriptFunc::Global_GetTextInfo( ScriptString* text, int w, int h, int font, int flags, int& tw, int& th, int& lines )
{
    SprMngr.GetTextInfo( w, h, text ? text->c_str() : NULL, font, flags, tw, th, lines );
}

void FOClient::SScriptFunc::Global_DrawSprite( uint spr_id, int frame_index, int x, int y, uint color, bool offs )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return;
    uint spr_id_ = ( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ) );
    if( offs )
    {
        SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id_ );
        if( !si )
            return;
        x += -si->Width / 2 + si->OffsX;
        y += -si->Height + si->OffsY;
    }
    SprMngr.DrawSprite( spr_id_, x, y, COLOR_SCRIPT_SPRITE( color ) );
}

void FOClient::SScriptFunc::Global_DrawSpriteSize( uint spr_id, int frame_index, int x, int y, int w, int h, bool zoom, uint color, bool offs )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return;
    uint spr_id_ = ( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ) );
    if( offs )
    {
        SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id_ );
        if( !si )
            return;
        x += si->OffsX;
        y += si->OffsY;
    }
    SprMngr.DrawSpriteSizeExt( spr_id_, x, y, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE( color ) );
}

void FOClient::SScriptFunc::Global_DrawSpritePattern( uint spr_id, int frame_index, int x, int y, int w, int h, int spr_width, int spr_height, uint color )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return;
    SprMngr.DrawSpritePattern( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ), x, y, w, h, spr_width, spr_height, COLOR_SCRIPT_SPRITE( color ) );
}

void FOClient::SScriptFunc::Global_DrawText( ScriptString& text, int x, int y, int w, int h, uint color, int font, int flags )
{
    if( !SpritesCanDraw )
        return;
    if( text.length() == 0 )
        return;
    if( w < 0 )
        w = -w, x -= w;
    if( h < 0 )
        h = -h, y -= h;
    Rect r = Rect( x, y, x + w, y + h );
    SprMngr.DrawStr( r, text.c_str(), flags, COLOR_SCRIPT_TEXT( color ), font );
}

void FOClient::SScriptFunc::Global_DrawPrimitive( int primitive_type, ScriptArray& data )
{
    if( !SpritesCanDraw || data.GetSize() == 0 )
        return;

    int prim;
    switch( primitive_type )
    {
    case 0:
        prim = PRIMITIVE_POINTLIST;
        break;
    case 1:
        prim = PRIMITIVE_LINELIST;
        break;
    case 2:
        prim = PRIMITIVE_LINESTRIP;
        break;
    case 3:
        prim = PRIMITIVE_TRIANGLELIST;
        break;
    case 4:
        prim = PRIMITIVE_TRIANGLESTRIP;
        break;
    case 5:
        prim = PRIMITIVE_TRIANGLEFAN;
        break;
    default:
        return;
    }

    static PointVec points;
    int             size = data.GetSize() / 3;
    points.resize( size );

    for( int i = 0; i < size; i++ )
    {
        PrepPoint& pp = points[ i ];
        pp.PointX = *(int*) data.At( i * 3 );
        pp.PointY = *(int*) data.At( i * 3 + 1 );
        pp.PointColor = *(int*) data.At( i * 3 + 2 );
        pp.PointOffsX = NULL;
        pp.PointOffsY = NULL;
    }

    SprMngr.DrawPoints( points, prim );
}

void FOClient::SScriptFunc::Global_DrawMapSprite( ushort hx, ushort hy, hash proto_id, uint spr_id, int frame_index, int ox, int oy )
{
    if( !Self->HexMngr.SpritesCanDrawMap )
        return;
    if( !Self->HexMngr.GetHexToDraw( hx, hy ) )
        return;

    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return;

    ProtoItem* proto_item = ItemMngr.GetProtoItem( proto_id );
    bool       is_flat = ( proto_item ? FLAG( proto_item->GetFlags(), ITEM_FLAT ) : false );
    bool       is_item = ( proto_item ? proto_item->IsItem() : false );
    bool       no_light = ( is_flat && !is_item );

    Field&     f = Self->HexMngr.GetField( hx, hy );
    Sprites&   tree = Self->HexMngr.GetDrawTree();
    Sprite&    spr = tree.InsertSprite( is_flat ? ( is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY ) : ( is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY ),
                                        hx, hy + ( proto_item ? proto_item->GetDrawOrderOffsetHexY() : 0 ), 0,
                                        f.ScrX + HEX_OX + ox, f.ScrY + HEX_OY + oy, frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ), NULL, NULL, NULL, NULL, NULL, NULL );
    if( !no_light )
        spr.SetLight( Self->HexMngr.GetLightHex( 0, 0 ), Self->HexMngr.GetMaxHexX(), Self->HexMngr.GetMaxHexY() );

    if( proto_item )
    {
        if( !is_flat && !proto_item->GetDisableEgg() )
        {
            int egg_type = 0;
            switch( proto_item->GetCorner() )
            {
            case CORNER_SOUTH:
                egg_type = EGG_X_OR_Y;
                break;
            case CORNER_NORTH:
                egg_type = EGG_X_AND_Y;
                break;
            case CORNER_EAST_WEST:
            case CORNER_WEST:
                egg_type = EGG_Y;
                break;
            default:
                egg_type = EGG_X;
                break;                                  // CORNER_NORTH_SOUTH, CORNER_EAST
            }
            spr.SetEgg( egg_type );
        }

        if( FLAG( proto_item->GetFlags(), ITEM_COLORIZE ) )
        {
            uint data_size;
            spr.SetAlpha( ProtoItem::PropertyLightColor->GetRawData( proto_item, data_size ) + 3 );
            spr.SetColor( proto_item->GetLightColor() & 0xFFFFFF );
        }

        if( FLAG( proto_item->GetFlags(), ITEM_BAD_ITEM ) )
            spr.SetContour( CONTOUR_RED );
    }
}

void FOClient::SScriptFunc::Global_DrawCritter2d( uint crtype, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color )
{
    if( CritType::IsEnabled( crtype ) )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, dir );
        if( anim )
            SprMngr.DrawSpriteSize( anim->Ind[ 0 ], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE( color ) );
    }
}

Animation3dVec DrawCritter3dAnim;
UIntVec        DrawCritter3dCrType;
BoolVec        DrawCritter3dFailToLoad;
int            DrawCritter3dLayers[ LAYERS3D_COUNT ];
void FOClient::SScriptFunc::Global_DrawCritter3d( uint instance, uint crtype, uint anim1, uint anim2, ScriptArray* layers, ScriptArray* position, uint color )
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b
    if( CritType::IsEnabled( crtype ) )
    {
        if( instance >= DrawCritter3dAnim.size() )
        {
            DrawCritter3dAnim.resize( instance + 1 );
            DrawCritter3dCrType.resize( instance + 1 );
            DrawCritter3dFailToLoad.resize( instance + 1 );
        }

        if( DrawCritter3dFailToLoad[ instance ] && DrawCritter3dCrType[ instance ] == crtype )
            return;

        Animation3d*& anim3d = DrawCritter3dAnim[ instance ];
        if( !anim3d || DrawCritter3dCrType[ instance ] != crtype )
        {
            if( anim3d )
                SprMngr.FreePure3dAnimation( anim3d );
            char fname[ MAX_FOPATH ];
            Str::Format( fname, "%s.fo3d", CritType::GetName( crtype ) );
            SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
            anim3d = SprMngr.LoadPure3dAnimation( fname, PT_ART_CRITTERS, false );
            SprMngr.PopAtlasType();
            DrawCritter3dCrType[ instance ] = crtype;
            DrawCritter3dFailToLoad[ instance ] = false;

            if( !anim3d )
            {
                DrawCritter3dFailToLoad[ instance ] = true;
                return;
            }
            anim3d->EnableShadow( false );
            anim3d->SetTimer( false );
        }

        uint  count = ( position ? position->GetSize() : 0 );
        float x = ( count > 0 ? *(float*) position->At( 0 ) : 0.0f );
        float y = ( count > 1 ? *(float*) position->At( 1 ) : 0.0f );
        float rx = ( count > 2 ? *(float*) position->At( 2 ) : 0.0f );
        float ry = ( count > 3 ? *(float*) position->At( 3 ) : 0.0f );
        float rz = ( count > 4 ? *(float*) position->At( 4 ) : 0.0f );
        float sx = ( count > 5 ? *(float*) position->At( 5 ) : 1.0f );
        float sy = ( count > 6 ? *(float*) position->At( 6 ) : 1.0f );
        float sz = ( count > 7 ? *(float*) position->At( 7 ) : 1.0f );
        float speed = ( count > 8 ? *(float*) position->At( 8 ) : 1.0f );
        // 9 reserved
        float stl = ( count > 10 ? *(float*) position->At( 10 ) : 0.0f );
        float stt = ( count > 11 ? *(float*) position->At( 11 ) : 0.0f );
        float str = ( count > 12 ? *(float*) position->At( 12 ) : 0.0f );
        float stb = ( count > 13 ? *(float*) position->At( 13 ) : 0.0f );
        RectF scissor = RectF( stl, stt, str, stb );

        memzero( DrawCritter3dLayers, sizeof( DrawCritter3dLayers ) );
        for( uint i = 0, j = ( layers ? layers->GetSize() : 0 ); i < j && i < LAYERS3D_COUNT; i++ )
            DrawCritter3dLayers[ i ] = *(int*) layers->At( i );

        anim3d->SetDirAngle( 0 );
        anim3d->SetRotation( rx * PI_VALUE / 180.0f, ry * PI_VALUE / 180.0f, rz * PI_VALUE / 180.0f );
        anim3d->SetScale( sx, sy, sz );
        anim3d->SetSpeed( speed );
        anim3d->SetAnimation( anim1, anim2, DrawCritter3dLayers, 0 );

        SprMngr.Draw3d( (int) x, (int) y, anim3d, COLOR_SCRIPT_SPRITE( color ), !scissor.IsZero() ? &scissor : NULL );
    }
}

void FOClient::SScriptFunc::Global_ShowScreen( int screen, ScriptDictionary* params )
{
    if( screen >= SCREEN_LOGIN && screen <= 9 )
        Self->ShowMainScreen( screen, params );
    else
        Self->ShowScreen( screen, params );
}

void FOClient::SScriptFunc::Global_HideScreen( int screen )
{
    Self->HideScreen( screen );
}

void FOClient::SScriptFunc::Global_GetHardcodedScreenPos( int screen, int& x, int& y )
{
    switch( screen )
    {
    case SCREEN_GLOBAL_MAP:
        x = Self->GmapX;
        y = Self->GmapY;
        break;
    case SCREEN_WAIT:
        x = 0;
        y = 0;
        break;
    case SCREEN__PICKUP:
        x = Self->PupX;
        y = Self->PupY;
        break;
    case SCREEN__MINI_MAP:
        x = Self->LmapX;
        y = Self->LmapY;
        break;
    case SCREEN__DIALOG:
        x = Self->DlgX;
        y = Self->DlgY;
        break;
    case SCREEN__BARTER:
        x = Self->DlgX;
        y = Self->DlgY;
        break;
    case SCREEN__PIP_BOY:
        x = Self->PipX;
        y = Self->PipY;
        break;
    case SCREEN__FIX_BOY:
        x = Self->FixX;
        y = Self->FixY;
        break;
    case SCREEN__AIM:
        x = Self->AimX;
        y = Self->AimY;
        break;
    case SCREEN__SPLIT:
        x = Self->SplitX;
        y = Self->SplitY;
        break;
    case SCREEN__TIMER:
        x = Self->TimerX;
        y = Self->TimerY;
        break;
    case SCREEN__DIALOGBOX:
        x = Self->DlgboxX;
        y = Self->DlgboxY;
        break;
    case SCREEN__ELEVATOR:
        x = Self->ElevatorX;
        y = Self->ElevatorY;
        break;
    case SCREEN__SAY:
        x = Self->SayX;
        y = Self->SayY;
        break;
    case SCREEN__GM_TOWN:
        x = 0;
        y = 0;
        break;
    case SCREEN__INPUT_BOX:
        x = Self->IboxX;
        y = Self->IboxY;
        break;
    case SCREEN__SKILLBOX:
        x = Self->SboxX;
        y = Self->SboxY;
        break;
    case SCREEN__USE:
        x = Self->UseX;
        y = Self->UseY;
        break;
    case SCREEN__PERK:
        x = Self->PerkX;
        y = Self->PerkY;
        break;
    case SCREEN__SAVE_LOAD:
        x = Self->SaveLoadX;
        y = Self->SaveLoadY;
        break;
    default:
        x = 0;
        y = 0;
        break;
    }
}

void FOClient::SScriptFunc::Global_DrawHardcodedScreen( int screen )
{
    switch( screen )
    {
    case SCREEN_CREDITS:
        Self->CreditsDraw();
        break;
    case SCREEN_GLOBAL_MAP:
        Self->GmapDraw();
        break;
    case SCREEN_WAIT:
        Self->WaitDraw();
        break;
    case SCREEN__PICKUP:
        Self->PupDraw();
        break;
    case SCREEN__MINI_MAP:
        Self->LmapDraw();
        break;
    case SCREEN__DIALOG:
        Self->DlgDraw( true );
        break;
    case SCREEN__BARTER:
        Self->DlgDraw( false );
        break;
    case SCREEN__PIP_BOY:
        Self->PipDraw();
        break;
    case SCREEN__FIX_BOY:
        Self->FixDraw();
        break;
    case SCREEN__AIM:
        Self->AimDraw();
        break;
    case SCREEN__SPLIT:
        Self->SplitDraw();
        break;
    case SCREEN__TIMER:
        Self->TimerDraw();
        break;
    case SCREEN__DIALOGBOX:
        Self->DlgboxDraw();
        break;
    case SCREEN__ELEVATOR:
        Self->ElevatorDraw();
        break;
    case SCREEN__SAY:
        Self->SayDraw();
        break;
    case SCREEN__GM_TOWN:
        Self->GmapTownDraw();
        break;
    case SCREEN__INPUT_BOX:
        Self->IboxDraw();
        break;
    case SCREEN__SKILLBOX:
        Self->SboxDraw();
        break;
    case SCREEN__USE:
        Self->UseDraw();
        break;
    case SCREEN__PERK:
        Self->PerkDraw();
        break;
    case SCREEN__TOWN_VIEW:
        Self->TViewDraw();
        break;
    case SCREEN__SAVE_LOAD:
        Self->SaveLoadDraw();
        break;
    default:
        break;
    }
}

void FOClient::SScriptFunc::Global_HandleHardcodedScreenMouse( int screen, int button, bool down, bool move )
{
    if( move )
        button = -1;

    switch( screen )
    {
    case SCREEN_CREDITS:
        Self->TryExit();
        break;
    case SCREEN_GAME:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->GameLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down && Self->IfaceHold == IFACE_NONE )
            Self->GameLMouseUp();
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->GameRMouseDown();
        else if( button == MOUSE_BUTTON_RIGHT && !down )
            Self->GameRMouseUp();
        else if( button == MOUSE_BUTTON_MIDDLE && GameOpt.MapZooming && GameOpt.SpritesZoomMin != GameOpt.SpritesZoomMax )
            Self->HexMngr.ChangeZoom( 0 ), Self->RebuildLookBorders = true;
        break;
    case SCREEN_GLOBAL_MAP:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->GmapLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->GmapLMouseUp();
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->GmapRMouseDown();
        else if( button == MOUSE_BUTTON_RIGHT && !down )
            Self->GmapRMouseUp();
        else if( move )
            Self->GmapMouseMove();
        break;
    case SCREEN__PICKUP:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->PupLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->PupLMouseUp();
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->PupRMouseDown();
        else if( move )
            Self->PupMouseMove();
        break;
    case SCREEN__MINI_MAP:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->LmapLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->LmapLMouseUp();
        else if( move )
            Self->LmapMouseMove();
        break;
    case SCREEN__DIALOG:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->DlgLMouseDown( true );
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->DlgLMouseUp( true );
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->DlgRMouseDown( true );
        else if( move )
            Self->DlgMouseMove( true );
        break;
    case SCREEN__BARTER:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->DlgLMouseDown( false );
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->DlgLMouseUp( false );
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->DlgRMouseDown( false );
        else if( move )
            Self->DlgMouseMove( false );
        break;
    case SCREEN__PIP_BOY:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->PipLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->PipLMouseUp();
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->PipRMouseDown();
        else if( move )
            Self->PipMouseMove();
        break;
    case SCREEN__FIX_BOY:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->FixLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->FixLMouseUp();
        else if( move )
            Self->FixMouseMove();
        break;
    case SCREEN__AIM:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->AimLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->AimLMouseUp();
        else if( move )
            Self->AimMouseMove();
        break;
    case SCREEN__SPLIT:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->SplitLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->SplitLMouseUp();
        else if( move )
            Self->SplitMouseMove();
        break;
    case SCREEN__TIMER:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->TimerLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->TimerLMouseUp();
        else if( move )
            Self->TimerMouseMove();
        break;
    case SCREEN__DIALOGBOX:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->DlgboxLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->DlgboxLMouseUp();
        else if( move )
            Self->DlgboxMouseMove();
        break;
    case SCREEN__ELEVATOR:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->ElevatorLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->ElevatorLMouseUp();
        else if( move )
            Self->ElevatorMouseMove();
        break;
    case SCREEN__SAY:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->SayLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->SayLMouseUp();
        else if( move )
            Self->SayMouseMove();
        break;
    case SCREEN__GM_TOWN:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->GmapLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->GmapLMouseUp();
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->GmapRMouseDown();
        else if( button == MOUSE_BUTTON_RIGHT && !down )
            Self->GmapRMouseUp();
        else if( move )
            Self->GmapMouseMove();
        break;
    case SCREEN__INPUT_BOX:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->IboxLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->IboxLMouseUp();
        break;
    case SCREEN__SKILLBOX:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->SboxLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->SboxLMouseUp();
        else if( move )
            Self->SboxMouseMove();
        break;
    case SCREEN__USE:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->UseLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->UseLMouseUp();
        else if( button == MOUSE_BUTTON_RIGHT && down )
            Self->UseRMouseDown();
        else if( move )
            Self->UseMouseMove();
        break;
    case SCREEN__PERK:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->PerkLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->PerkLMouseUp();
        else if( move )
            Self->PerkMouseMove();
        break;
    case SCREEN__TOWN_VIEW:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->TViewLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->TViewLMouseUp();
        else if( button == MOUSE_BUTTON_MIDDLE && GameOpt.MapZooming && GameOpt.SpritesZoomMin != GameOpt.SpritesZoomMax )
            Self->HexMngr.ChangeZoom( 0 ), Self->RebuildLookBorders = true;
        else if( move )
            Self->TViewMouseMove();
        break;
    case SCREEN__SAVE_LOAD:
        if( button == MOUSE_BUTTON_LEFT && down )
            Self->SaveLoadLMouseDown();
        else if( button == MOUSE_BUTTON_LEFT && !down )
            Self->SaveLoadLMouseUp();
        else if( move )
            Self->SaveLoadMouseMove();
        break;
    default:
        break;
    }

    if( button == MOUSE_BUTTON_LEFT && !down )
        Timer::StartAccelerator( ACCELERATE_NONE );
    else if( ( button == MOUSE_BUTTON_WHEEL_DOWN || button == MOUSE_BUTTON_WHEEL_UP ) && down )
        Self->ProcessMouseWheel( button == MOUSE_BUTTON_WHEEL_DOWN ? -1 : 1 );
    else if( move )
        Self->ProcessMouseScroll();
}

void FOClient::SScriptFunc::Global_HandleHardcodedScreenKey( int screen, uchar key, ScriptString* text, bool down )
{
    switch( screen )
    {
    case SCREEN_CREDITS:
        Self->TryExit();
        break;
    case SCREEN_GAME:
        if( down )
            Self->GameKeyDown( key, text ? text->c_str() : "" );
        break;
    case SCREEN_WAIT:
        break;
    case SCREEN__DIALOG:
        if( down )
            Self->DlgKeyDown( true, key, text ? text->c_str() : "" );
        break;
    case SCREEN__BARTER:
        if( down )
            Self->DlgKeyDown( false, key, text ? text->c_str() : "" );
        break;
    case SCREEN__SPLIT:
        if( down )
            Self->SplitKeyDown( key, text ? text->c_str() : "" );
        break;
    case SCREEN__SAY:
        if( down )
            Self->SayKeyDown( key, text ? text->c_str() : "" );
        break;
    case SCREEN__INPUT_BOX:
        if( down )
            Self->IboxKeyDown( key, text ? text->c_str() : "" );
        else if( !down )
            Self->IboxKeyUp( key );
        break;
    case SCREEN__SAVE_LOAD:
        break;
    default:
        break;
    }

    if( !down )
        Timer::StartAccelerator( ACCELERATE_NONE );
}

bool FOClient::SScriptFunc::Global_GetHexPos( ushort hx, ushort hy, int& x, int& y )
{
    x = y = 0;
    if( Self->HexMngr.IsMapLoaded() && hx < Self->HexMngr.GetMaxHexX() && hy < Self->HexMngr.GetMaxHexY() )
    {
        Self->HexMngr.GetHexCurrentPosition( hx, hy, x, y );
        x += GameOpt.ScrOx + HEX_OX;
        y += GameOpt.ScrOy + HEX_OY;
        x = (int) ( x / GameOpt.SpritesZoom );
        y = (int) ( y / GameOpt.SpritesZoom );
        return true;
    }
    return false;
}

bool FOClient::SScriptFunc::Global_GetMonitorHex( int x, int y, ushort& hx, ushort& hy, bool ignore_interface )
{
    ushort hx_, hy_;
    int    old_x = GameOpt.MouseX;
    int    old_y = GameOpt.MouseY;
    GameOpt.MouseX = x;
    GameOpt.MouseY = y;
    bool result = Self->GetCurHex( hx_, hy_, ignore_interface );
    GameOpt.MouseX = old_x;
    GameOpt.MouseY = old_y;
    if( result )
    {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

Item* FOClient::SScriptFunc::Global_GetMonitorItem( int x, int y, bool ignore_interface )
{
    if( !ignore_interface && Self->IsCurInInterface( x, y ) )
        return NULL;

    ItemHex*   item;
    CritterCl* cr;
    Self->HexMngr.GetSmthPixel( x, y, item, cr );
    return item;
}

CritterCl* FOClient::SScriptFunc::Global_GetMonitorCritter( int x, int y, bool ignore_interface )
{
    if( !ignore_interface && Self->IsCurInInterface( x, y ) )
        return NULL;

    ItemHex*   item;
    CritterCl* cr;
    Self->HexMngr.GetSmthPixel( x, y, item, cr );
    return cr;
}

ushort FOClient::SScriptFunc::Global_GetMapWidth()
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    return Self->HexMngr.GetMaxHexX();
}

ushort FOClient::SScriptFunc::Global_GetMapHeight()
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    return Self->HexMngr.GetMaxHexY();
}

int FOClient::SScriptFunc::Global_GetCurrentCursor()
{
    return Self->CurMode;
}

int FOClient::SScriptFunc::Global_GetLastCursor()
{
    return Self->CurModeLast;
}

void FOClient::SScriptFunc::Global_ChangeCursor( int cursor, uint context_id )
{
    if( cursor == CUR_USE_SKILL )
        Self->CurSkill = (ushort) context_id;

    Self->SetCurMode( cursor );
}

bool FOClient::SScriptFunc::Global_SaveScreenshot( ScriptString& file_path )
{
    char screen_path[ MAX_FOPATH ];
    Str::Copy( screen_path, file_path.c_str() );
    FileManager::FormatPath( screen_path );

    SprMngr.SaveTexture( NULL, screen_path, true );
    return true;
}

bool FOClient::SScriptFunc::Global_SaveText( ScriptString& file_path, ScriptString& text )
{
    char text_path[ MAX_FOPATH ];
    Str::Copy( text_path, file_path.c_str() );
    FileManager::FormatPath( text_path );

    void* f = FileOpen( text_path, true );
    if( !f )
        return false;

    if( text.length() > 0 )
        FileWrite( f, text.c_str(), (uint) text.length() );
    FileClose( f );
    return true;
}

void FOClient::SScriptFunc::Global_SetCacheData( const ScriptString& name, const ScriptArray& data )
{
    UCharVec data_vec;
    Script::AssignScriptArrayInVector( data_vec, &data );
    Crypt.SetCache( name.c_str(), data_vec );
}

void FOClient::SScriptFunc::Global_SetCacheDataSize( const ScriptString& name, const ScriptArray& data, uint data_size )
{
    UCharVec data_vec;
    Script::AssignScriptArrayInVector( data_vec, &data );
    data_vec.resize( data_size );
    Crypt.SetCache( name.c_str(), data_vec );
}

bool FOClient::SScriptFunc::Global_GetCacheData( const ScriptString& name, ScriptArray& data )
{
    UCharVec data_vec;
    if( !Crypt.GetCache( name.c_str(), data_vec ) )
        return false;
    Script::AppendVectorToArray( data_vec, &data );
    return true;
}

void FOClient::SScriptFunc::Global_SetCacheDataStr( const ScriptString& name, const ScriptString& str )
{
    Crypt.SetCache( name.c_str(), str.c_std_str() );
}

ScriptString* FOClient::SScriptFunc::Global_GetCacheDataStr( const ScriptString& name )
{
    return ScriptString::Create( Crypt.GetCache( name.c_str() ) );
}

bool FOClient::SScriptFunc::Global_IsCacheData( const ScriptString& name )
{
    return Crypt.IsCache( name.c_str() );
}

void FOClient::SScriptFunc::Global_EraseCacheData( const ScriptString& name )
{
    Crypt.EraseCache( name.c_str() );
}

void FOClient::SScriptFunc::Global_SetUserConfig( ScriptArray& key_values )
{
    FileManager cfg_user;
    for( int i = 0, j = (int) key_values.GetSize(); i < j - 1; i += 2 )
    {
        ScriptString& key = *(ScriptString*) key_values.At( i );
        ScriptString& value = *(ScriptString*) key_values.At( i + 1 );
        cfg_user.SetStr( key.c_str() );
        cfg_user.SetStr( " = " );
        cfg_user.SetStr( value.c_str() );
        cfg_user.SetStr( "\n" );
    }
    char cfg_name[ MAX_FOPATH ];
    Str::Format( cfg_name, "%s", IniParser::GetConfigFileName() );
    cfg_user.SaveOutBufToFile( cfg_name, PT_CACHE );
}

bool&  FOClient::SScriptFunc::GmapActive = FOClient::GmapActive;
bool&  FOClient::SScriptFunc::GmapWait = FOClient::GmapWait;
float& FOClient::SScriptFunc::GmapZoom = FOClient::GmapZoom;
int&   FOClient::SScriptFunc::GmapOffsetX = FOClient::GmapOffsetX;
int&   FOClient::SScriptFunc::GmapOffsetY = FOClient::GmapOffsetY;
int&   FOClient::SScriptFunc::GmapGroupCurX = FOClient::GmapGroupCurX;
int&   FOClient::SScriptFunc::GmapGroupCurY = FOClient::GmapGroupCurY;
int&   FOClient::SScriptFunc::GmapGroupToX = FOClient::GmapGroupToX;
int&   FOClient::SScriptFunc::GmapGroupToY = FOClient::GmapGroupToY;
float& FOClient::SScriptFunc::GmapGroupSpeed = FOClient::GmapGroupSpeed;
