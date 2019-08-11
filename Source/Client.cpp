#include "Common.h"
#include "Client.h"
#include "Access.h"
#include "FileSystem.h"
#include <fcntl.h>
#include "SHA/sha1.h"
#include "SHA/sha2.h"
#include <time.h>

#ifdef MEMORY_DEBUG
static bool                 ASDbgMemoryCanWork = false;
static THREAD bool          ASDbgMemoryInUse = false;
static map< void*, string > ASDbgMemoryPtr;

static void* ASDeepDebugMalloc( size_t size )
{
    size += sizeof( size_t );
    size_t* ptr = (size_t*) malloc( size );
    *ptr = size;

    if( ASDbgMemoryCanWork && !ASDbgMemoryInUse )
    {
        ASDbgMemoryInUse = true;
        string func = Script::GetActiveFuncName();
        string buf = _str( "AS : {}", !func.empty() ? func : "<nullptr>" );
        MEMORY_PROCESS_STR( buf.c_str(), (int) size );
        ASDbgMemoryPtr.insert( std::make_pair( ptr, buf ) );
        ASDbgMemoryInUse = false;
    }
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, (int) size );

    return ++ptr;
}

static void ASDeepDebugFree( void* ptr )
{
    size_t* ptr_ = (size_t*) ptr;
    size_t  size = *( --ptr_ );

    if( ASDbgMemoryCanWork )
    {
        auto it = ASDbgMemoryPtr.find( ptr_ );
        if( it != ASDbgMemoryPtr.end() )
        {
            MEMORY_PROCESS_STR( it->second.c_str(), -(int) size );
            ASDbgMemoryPtr.erase( it );
        }
    }
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, -(int) size );

    free( ptr_ );
}
#endif

// Check buffer for error
#define CHECK_IN_BUFF_ERROR                  \
    if( Bin.IsError() )                      \
    {                                        \
        WriteLog( "Wrong network data!\n" ); \
        NetDisconnect();                     \
        return;                              \
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

#if defined ( FO_ANDROID ) || defined ( FO_IOS )
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

FOClient* FOClient::Self = nullptr;

FOClient::FOClient()
{
    WriteLog( "Engine initialization...\n" );

    RUNTIME_ASSERT( !Self );
    Self = this;

    InitCalls = 0;
    DoRestart = false;
    ComLen = BufferManager::DefaultBufSize;
    ComBuf = new uchar[ ComLen ];
    ZStreamOk = false;
    Sock = INVALID_SOCKET;
    BytesReceive = 0;
    BytesRealReceive = 0;
    BytesSend = 0;
    IsConnecting = false;
    IsConnected = false;
    InitNetReason = INIT_NET_REASON_NONE;
    WindowResolutionDiffX = 0;
    WindowResolutionDiffY = 0;

    UpdateFilesInProgress = false;
    UpdateFilesClientOutdated = false;
    UpdateFilesCacheChanged = false;
    UpdateFilesFilesChanged = false;
    UpdateFilesConnection = false;
    UpdateFilesConnectTimeout = 0;
    UpdateFilesTick = 0;
    UpdateFilesAborted = false;
    UpdateFilesFontLoaded = false;
    UpdateFilesText = "";
    UpdateFilesProgress = "";
    UpdateFilesList = nullptr;
    UpdateFilesWholeSize = 0;
    UpdateFileDownloading = false;
    UpdateFileTemp = nullptr;

    Chosen = nullptr;
    PingTick = 0;
    PingCallTick = 0;
    DaySumRGB = 0;
    Animations.resize( 10000 );

    CurVideo = nullptr;
    MusicVolumeRestore = -1;

    CurMapPid = 0;
    CurMapLocPid = 0;
    CurMapIndexInLoc = 0;

    SomeItem = nullptr;
    GmapFog = nullptr;
    CanDrawInScripts = false;
    IsAutoLogin = false;
}

bool FOClient::PreInit()
{
    // SDL
    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) )
    {
        WriteLog( "SDL initialization fail, error '{}'.\n", SDL_GetError() );
        return false;
    }

    // SDL events
    #if defined ( FO_ANDROID ) || defined ( FO_IOS )
    SDL_SetEventFilter( HandleAppEvents, nullptr );
    #endif

    // Register dll script data
    struct GetDrawingSprites_
    {
        static void* GetDrawingSprites( uint& count )
        {
            Sprites& tree = Self->HexMngr.GetDrawTree();
            count = tree.Size();
            if( !count )
                return nullptr;
            return tree.RootSprite();
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
            int         sx = sprite_->ScrX - si->Width / 2 + si->OffsX + GameOpt.ScrOx + ( sprite_->OffsX ? *sprite_->OffsX : 0 ) + *sprite_->PScrX;
            int         sy = sprite_->ScrY - si->Height + si->OffsY + GameOpt.ScrOy + ( sprite_->OffsY ? *sprite_->OffsY : 0 ) + *sprite_->PScrY;
            if( !( sprite_ = sprite_->GetIntersected( x - sx, y - sy ) ) ) return false;
            if( check_egg && SprMngr.CompareHexEgg( sprite_->HexX, sprite_->HexY, sprite_->EggType ) && SprMngr.IsEggTransp( x, y ) ) return false;
            return true;
        }
    };
    GameOpt.IsSpriteHit = &IsSpriteHit_::IsSpriteHit;

    // Input
    Keyb::Init();

    // Cache
    if( !Crypt.InitCache() )
    {
        WriteLog( "Can't create cache file.\n" );
        return false;
    }

    // Sprite manager
    if( !SprMngr.Init() )
        return false;

    // Cursor position
    int sw = 0, sh = 0;
    SprMngr.GetWindowSize( sw, sh );
    int mx = 0, my = 0;
    SprMngr.GetMousePosition( mx, my );
    GameOpt.MouseX = GameOpt.LastMouseX = CLAMP( mx, 0, sw - 1 );
    GameOpt.MouseY = GameOpt.LastMouseY = CLAMP( my, 0, sh - 1 );

    // Sound manager
    SndMngr.Init();
    GameOpt.SoundVolume = MainConfig->GetInt( "", "SoundVolume", 100 );
    GameOpt.MusicVolume = MainConfig->GetInt( "", "MusicVolume", 100 );

    // Language Packs
    string lang_name = MainConfig->GetStr( "", "Language", DEFAULT_LANGUAGE );
    CurLang.LoadFromCache( lang_name );

    return true;
}

bool FOClient::PostInit()
{
    // Reload cache
    if( !CurLang.IsAllMsgLoaded )
        CurLang.LoadFromCache( CurLang.NameStr );
    if( !CurLang.IsAllMsgLoaded )
    {
        WriteLog( "Language packs not found!\n" );
        return false;
    }

    // Basic effects
    if( !GraphicLoader::LoadDefaultEffects() )
        return false;

    // Resource manager
    ResMngr.Refresh();

    // Wait screen
    ScreenModeMain = SCREEN_WAIT;
    WaitPic = ResMngr.GetRandomSplash();
    SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) );
    WaitDraw();
    SprMngr.EndScene();

    // Clean up previous data
    SAFEREL( SomeItem );
    SAFEREL( Globals );
    SAFEREL( ScriptFunc.ClientCurMap );
    SAFEREL( ScriptFunc.ClientCurLocation );
    ProtoMngr.ClearProtos();

    // Scripts
    if( !ReloadScripts() )
        return false;

    // Recreate static atlas
    ResMngr.FreeResources( RES_ATLAS_STATIC );
    SprMngr.AccumulateAtlasData();
    SprMngr.PushAtlasType( RES_ATLAS_STATIC );

    // Modules initialization
    if( !Script::RunModuleInitFunctions() )
    {
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    // Start script
    if( !Script::RaiseInternalEvent( ClientFunctions.Start ) )
    {
        WriteLog( "Execute start script fail.\n" );
        return false;
    }

    // 3d initialization
    WriteLog( "3d rendering is {}.\n", GameOpt.Enable3dRendering ? "enabled" : "disabled" );
    if( GameOpt.Enable3dRendering && !Animation3d::StartUp() )
    {
        WriteLog( "Can't initialize 3d rendering.\n" );
        return false;
    }

    // Minimap
    LmapZoom = 2;
    LmapSwitchHi = false;
    LmapPrepareNextTick = 0;

    // Global map
    SAFEDEL( GmapFog );
    GmapFog = new TwoBitMask( GM__MAXZONEX, GM__MAXZONEY, nullptr );

    // PickUp
    PupTransferType = 0;
    PupContId = 0;
    PupContPid = 0;

    // Pipboy
    Automaps.clear();

    // Hex field sprites
    HexMngr.ReloadSprites();

    // Load fonts
    if( !SprMngr.LoadFontFO( FONT_DEFAULT, "Default", false ) )
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
            Animation3dEntity::GetEntity( Preload3dFiles[ i ] );
        WriteLog( "Preload 3d files complete.\n" );
    }

    // Item prototypes
    UCharVec protos_data;
    Crypt.GetCache( "$protos.cache", protos_data );
    ProtoMngr.LoadProtosFromBinaryData( protos_data );

    // Hex manager
    HexMngr.Finish();
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
    ScreenMirrorTexture = nullptr;
    ScreenMirrorEndTick = 0;
    ScreenMirrorStart = false;
    RebuildLookBorders = false;
    DrawLookBorders = true;
    DrawShootBorders = false;

    LookBorders.clear();
    ShootBorders.clear();

    // Auto login
    ProcessAutoLogin();

    WriteLog( "Engine initialization complete.\n" );
    return true;
}

void FOClient::ProcessAutoLogin()
{
    if( !ClientFunctions.AutoLogin )
        return;

    string auto_login = MainConfig->GetStr( "", "AutoLogin" );

    #ifdef FO_WEB
    char* auto_login_web = (char*) EM_ASM_INT( {
                                                   if( 'foAutoLogin' in Module )
                                                   {
                                                       var len = lengthBytesUTF8( Module.foAutoLogin ) + 1;
                                                       var str = _malloc( len );
                                                       stringToUTF8( Module.foAutoLogin, str, len + 1 );
                                                       return str;
                                                   }
                                                   return null;
                                               } );
    if( auto_login_web )
    {
        auto_login = auto_login_web;
        free( auto_login_web );
        auto_login_web = nullptr;
    }
    #endif

    if( auto_login.empty() )
        return;

    StrVec auto_login_args = _str( auto_login ).split( ' ' );
    if( auto_login_args.size() != 2 )
        return;

    IsAutoLogin = true;

    if( !Script::RaiseInternalEvent( ClientFunctions.AutoLogin, &auto_login_args[ 0 ], &auto_login_args[ 1 ] ) ||
        InitNetReason == INIT_NET_REASON_NONE )
    {
        LoginName = auto_login_args[ 0 ];
        LoginPassword = auto_login_args[ 1 ];
        InitNetReason = INIT_NET_REASON_LOGIN;
    }
}

void FOClient::Finish()
{
    RUNTIME_ASSERT( !"Not used" );

    WriteLog( "Engine finish...\n" );

    NetDisconnect();
    ResMngr.Finish();
    HexMngr.Finish();
    SprMngr.Finish();
    SndMngr.Finish();
    Script::Finish();

    SAFEDELA( ComBuf );
    SAFEDEL( GmapFog );

    FileManager::ClearDataFiles();

    Self = nullptr;

    WriteLog( "Engine finish complete.\n" );
}

void FOClient::Restart()
{
    WriteLog( "Engine reinitialization.\n" );

    NetDisconnect();

    if( PostInit() )
        ScreenFadeOut();
    else
        GameOpt.Quit = true;
}

void FOClient::UpdateBinary()
{
    #ifdef FO_WINDOWS
    // Copy binaries
    string exe_path = FileManager::GetExePath();
    string to_dir = _str( exe_path ).extractDir();
    string base_name = _str( exe_path ).extractFileName().eraseFileExtension();
    string from_dir = FileManager::GetWritePath( "" );
    bool   copy_to_new = FileManager::CopyFile( from_dir + base_name + ".exe", to_dir + base_name + ".new.exe" );
    RUNTIME_ASSERT( copy_to_new );
    FileManager::DeleteDir( to_dir + base_name + ".old.exe" );
    bool rename_to_old = FileManager::RenameFile( to_dir + base_name + ".exe", to_dir + base_name + ".old.exe" );
    RUNTIME_ASSERT( rename_to_old );
    bool rename_to_new = FileManager::RenameFile( to_dir + base_name + ".new.exe", to_dir + base_name + ".exe" );
    RUNTIME_ASSERT( rename_to_new );
    if( FileManager::CopyFile( from_dir + base_name + ".pdb", to_dir + base_name + ".new.pdb" ) )
    {
        FileManager::DeleteFile( to_dir + base_name + ".old.pdb" );
        FileManager::RenameFile( to_dir + base_name + ".pdb", to_dir + base_name + ".old.pdb" );
        FileManager::RenameFile( to_dir + base_name + ".new.pdb", to_dir + base_name + ".pdb" );
    }

    // Restart client
    PROCESS_INFORMATION pi;
    memzero( &pi, sizeof( pi ) );
    STARTUPINFOW        sui;
    memzero( &sui, sizeof( sui ) );
    sui.cb = sizeof( sui );
    wchar_t command_line[ TEMP_BUF_SIZE ];
    wcscpy( command_line, GetCommandLineW() );
    wcscat( command_line, L" --restart" );
    BOOL create_process = CreateProcessW( _str( exe_path ).toWideChar().c_str(), command_line, nullptr, nullptr, FALSE,
                                          NORMAL_PRIORITY_CLASS, nullptr, nullptr, &sui, &pi );
    RUNTIME_ASSERT( create_process );

    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    ExitProcess( 0 );
    #else
    RUNTIME_ASSERT( !"Invalid platform for binary updating" );
    #endif
}

void FOClient::UpdateFilesStart()
{
    UpdateFilesInProgress = true;
    UpdateFilesClientOutdated = false;
    UpdateFilesCacheChanged = false;
    UpdateFilesFilesChanged = false;
    UpdateFilesConnection = false;
    UpdateFilesConnectTimeout = 0;
    UpdateFilesTick = 0;
    UpdateFilesAborted = false;
    UpdateFilesFontLoaded = false;
    UpdateFilesText = "";
    UpdateFilesProgress = "";
    UpdateFilesList = nullptr;
    UpdateFilesWholeSize = 0;
    UpdateFileDownloading = false;
    UpdateFileTemp = nullptr;
}

void FOClient::UpdateFilesLoop()
{
    // Was aborted
    if( UpdateFilesAborted )
        return;

    // Update indication
    if( InitCalls < 2 )
    {
        // Load font
        if( !UpdateFilesFontLoaded )
        {
            SprMngr.PushAtlasType( RES_ATLAS_STATIC );
            UpdateFilesFontLoaded = SprMngr.LoadFontFO( FONT_DEFAULT, "Default", false );
            if( !UpdateFilesFontLoaded )
                UpdateFilesFontLoaded = SprMngr.LoadFontBMF( FONT_DEFAULT, "Default" );
            if( UpdateFilesFontLoaded )
                SprMngr.BuildFonts();
            SprMngr.PopAtlasType();
        }

        // Running dots
        int    dots = ( Timer::FastTick() - UpdateFilesTick ) / 100 % 50 + 1;
        string dots_str = "";
        for( int i = 0; i < dots; i++ )
            dots_str += ".";

        // State
        SprMngr.BeginScene( COLOR_RGB( 50, 50, 50 ) );
        string update_text = UpdateFilesText + UpdateFilesProgress + dots_str;
        SprMngr.DrawStr( Rect( 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), update_text.c_str(),
                         FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT );
        SprMngr.EndScene();
    }
    else
    {
        // Wait screen
        SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) );
        DrawIface();
        SprMngr.EndScene();
    }

    // Logic
    if( !IsConnected )
    {
        if( UpdateFilesConnection )
        {
            UpdateFilesConnection = false;
            UpdateFilesAddText( STR_CANT_CONNECT_TO_SERVER, "Can't connect to server!" );
        }

        if( Timer::FastTick() < UpdateFilesConnectTimeout )
            return;
        UpdateFilesConnectTimeout = Timer::FastTick() + 5000;

        // Connect to server
        UpdateFilesAddText( STR_CONNECT_TO_SERVER, "Connect to server..." );
        NetConnect( GameOpt.Host.c_str(), GameOpt.Port );
        UpdateFilesConnection = true;
    }
    else
    {
        if( UpdateFilesConnection )
        {
            UpdateFilesConnection = false;
            UpdateFilesAddText( STR_CONNECTION_ESTABLISHED, "Connection established." );

            // Update
            UpdateFilesAddText( STR_CHECK_UPDATES, "Check updates..." );
            UpdateFilesText = "";

            // Data synchronization
            UpdateFilesAddText( STR_DATA_SYNCHRONIZATION, "Data synchronization..." );

            // Clean up
            UpdateFilesClientOutdated = false;
            UpdateFilesCacheChanged = false;
            UpdateFilesFilesChanged = false;
            SAFEDEL( UpdateFilesList );
            UpdateFileDownloading = false;
            FileManager::DeleteFile( FileManager::GetWritePath( "Update.bin" ) );
            UpdateFilesTick = Timer::FastTick();

            Net_SendUpdate();
        }

        UpdateFilesProgress = "";
        if( UpdateFilesList && !UpdateFilesList->empty() )
        {
            UpdateFilesProgress += "\n";
            for( size_t i = 0, j = UpdateFilesList->size(); i < j; i++ )
            {
                UpdateFile& update_file = ( *UpdateFilesList )[ i ];
                float       cur = (float) ( update_file.Size - update_file.RemaningSize ) / ( 1024.0f * 1024.0f );
                float       max = MAX( (float) update_file.Size / ( 1024.0f * 1024.0f ), 0.01f );
                string      name = _str( update_file.Name ).formatPath();
                UpdateFilesProgress += _str( "{} {:.2f} / {:.2f} MB\n", name, cur, max );
            }
            UpdateFilesProgress += "\n";
        }

        if( UpdateFilesList && !UpdateFileDownloading )
        {
            if( !UpdateFilesList->empty() )
            {
                UpdateFile& update_file = UpdateFilesList->front();

                if( UpdateFileTemp )
                {
                    FileClose( UpdateFileTemp );
                    UpdateFileTemp = nullptr;
                }

                if( update_file.Name[ 0 ] == '$' )
                {
                    UpdateFileTemp = FileOpen( FileManager::GetWritePath( "Update.bin" ), true );
                    UpdateFilesCacheChanged = true;
                }
                else
                {
                    // Web client can receive only cache updates
                    // Resources must be packed in main bundle
                    #ifdef FO_WEB
                    UpdateFilesAddText( STR_CLIENT_OUTDATED, "Client outdated!" );
                    SAFEDEL( UpdateFilesList );
                    NetDisconnect();
                    return;
                    #endif

                    FileManager::DeleteFile( FileManager::GetWritePath( update_file.Name ) );
                    UpdateFileTemp = FileOpen( FileManager::GetWritePath( "Update.bin" ), true );
                    UpdateFilesFilesChanged = true;
                }

                if( !UpdateFileTemp )
                {
                    UpdateFilesAddText( STR_FILESYSTEM_ERROR, "File system error!" );
                    SAFEDEL( UpdateFilesList );
                    NetDisconnect();
                    return;
                }

                UpdateFileDownloading = true;

                Bout << NETMSG_GET_UPDATE_FILE;
                Bout << UpdateFilesList->front().Index;
            }
            else
            {
                // Done
                UpdateFilesInProgress = false;
                SAFEDEL( UpdateFilesList );

                // Disconnect
                if( InitCalls < 2 )
                    NetDisconnect();

                // Update binaries
                if( UpdateFilesClientOutdated )
                    UpdateBinary();

                // Reinitialize data
                if( UpdateFilesCacheChanged )
                    CurLang.LoadFromCache( CurLang.NameStr );
                if( UpdateFilesFilesChanged )
                    GetClientOptions();
                if( InitCalls >= 2 && ( UpdateFilesCacheChanged || UpdateFilesFilesChanged ) )
                    DoRestart = true;

                return;
            }
        }

        ParseSocket();

        if( !IsConnected && !UpdateFilesAborted )
            UpdateFilesAddText( STR_CONNECTION_FAILTURE, "Connection failure!" );
    }
}

void FOClient::UpdateFilesAddText( uint num_str, const string& num_str_str )
{
    if( UpdateFilesFontLoaded )
    {
        string text = ( CurLang.Msg[ TEXTMSG_GAME ].Count( num_str ) ? CurLang.Msg[ TEXTMSG_GAME ].GetStr( num_str ) : num_str_str );
        UpdateFilesText += text + "\n";
    }
}

void FOClient::UpdateFilesAbort( uint num_str, const string& num_str_str )
{
    UpdateFilesAborted = true;

    UpdateFilesAddText( num_str, num_str_str );
    NetDisconnect();

    if( UpdateFileTemp )
    {
        FileClose( UpdateFileTemp );
        UpdateFileTemp = nullptr;
    }

    SprMngr.BeginScene( COLOR_RGB( 255, 0, 0 ) );
    SprMngr.DrawStr( Rect( 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), UpdateFilesText, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT );
    SprMngr.EndScene();
}

void FOClient::DeleteCritters()
{
    HexMngr.DeleteCritters();
    Chosen = nullptr;
}

void FOClient::AddCritter( CritterCl* cr )
{
    uint       fading = 0;
    CritterCl* cr_ = GetCritter( cr->GetId() );
    if( cr_ )
        fading = cr_->FadingTick;
    DeleteCritter( cr->GetId() );
    if( HexMngr.IsMapLoaded() )
    {
        Field& f = HexMngr.GetField( cr->GetHexX(), cr->GetHexY() );
        if( f.Crit && f.Crit->IsFinishing() )
            DeleteCritter( f.Crit->GetId() );
    }
    if( cr->IsChosen() )
        Chosen = cr;
    HexMngr.AddCritter( cr );
    cr->FadingTick = Timer::GameTick() + FADING_PERIOD - ( fading > Timer::GameTick() ? fading - Timer::GameTick() : 0 );
}

void FOClient::DeleteCritter( uint remid )
{
    if( Chosen && Chosen->GetId() == remid )
        Chosen = nullptr;
    HexMngr.DeleteCritter( remid );
}

void FOClient::LookBordersPrepare()
{
    LookBorders.clear();
    ShootBorders.clear();

    if( !Chosen || !HexMngr.IsMapLoaded() || ( !DrawLookBorders && !DrawShootBorders ) )
    {
        HexMngr.SetFog( LookBorders, ShootBorders, nullptr, nullptr );
        return;
    }

    uint   dist = Chosen->GetLookDistance();
    ushort base_hx = Chosen->GetHexX();
    ushort base_hy = Chosen->GetHexY();
    int    hx = base_hx;
    int    hy = base_hy;
    int    chosen_dir = Chosen->GetDir();
    uint   dist_shoot = Chosen->GetAttackDist();
    ushort maxhx = HexMngr.GetWidth();
    ushort maxhy = HexMngr.GetHeight();
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
                HexMngr.TraceBullet( base_hx, base_hy, hx_, hy_, dist_, 0.0f, nullptr, false, nullptr, 0, nullptr, &block, nullptr, false );
                hx_ = block.first;
                hy_ = block.second;
            }

            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_TRACE ) )
            {
                UShortPair block;
                HexMngr.TraceBullet( base_hx, base_hy, hx_, hy_, 0, 0.0f, nullptr, false, nullptr, 0, nullptr, &block, nullptr, true );
                hx_ = block.first;
                hy_ = block.second;
            }

            uint dist_look = DistGame( base_hx, base_hy, hx_, hy_ );
            if( DrawLookBorders )
            {
                int    x, y;
                HexMngr.GetHexCurrentPosition( hx_, hy_, x, y );
                short* ox = ( dist_look == dist ? &Chosen->SprOx : nullptr );
                short* oy = ( dist_look == dist ? &Chosen->SprOy : nullptr );
                LookBorders.push_back( PrepPoint( x + HEX_OX, y + HEX_OY, COLOR_RGBA( 0, 255, dist_look * 255 / dist, 0 ), ox, oy ) );
            }

            if( DrawShootBorders )
            {
                ushort     hx__ = hx_;
                ushort     hy__ = hy_;
                UShortPair block;
                uint       max_shoot_dist = MAX( MIN( dist_look, dist_shoot ), 0 ) + 1;
                HexMngr.TraceBullet( base_hx, base_hy, hx_, hy_, max_shoot_dist, 0.0f, nullptr, false, nullptr, 0, nullptr, &block, nullptr, true );
                hx__ = block.first;
                hy__ = block.second;

                int    x_, y_;
                HexMngr.GetHexCurrentPosition( hx__, hy__, x_, y_ );
                uint   result_shoot_dist = DistGame( base_hx, base_hy, hx__, hy__ );
                short* ox = ( result_shoot_dist == max_shoot_dist ? &Chosen->SprOx : nullptr );
                short* oy = ( result_shoot_dist == max_shoot_dist ? &Chosen->SprOy : nullptr );
                ShootBorders.push_back( PrepPoint( x_ + HEX_OX, y_ + HEX_OY, COLOR_RGBA( 255, 255, result_shoot_dist * 255 / max_shoot_dist, 0 ), ox, oy ) );
            }
        }
    }

    int base_x, base_y;
    HexMngr.GetHexCurrentPosition( base_hx, base_hy, base_x, base_y );
    if( !LookBorders.empty() )
    {
        LookBorders.push_back( *LookBorders.begin() );
        LookBorders.insert( LookBorders.begin(), PrepPoint( base_x + HEX_OX, base_y + HEX_OY, COLOR_RGBA( 0, 0, 0, 0 ), &Chosen->SprOx, &Chosen->SprOy ) );
    }
    if( !ShootBorders.empty() )
    {
        ShootBorders.push_back( *ShootBorders.begin() );
        ShootBorders.insert( ShootBorders.begin(), PrepPoint( base_x + HEX_OX, base_y + HEX_OY, COLOR_RGBA( 255, 0, 0, 0 ), &Chosen->SprOx, &Chosen->SprOy ) );
    }

    HexMngr.SetFog( LookBorders, ShootBorders, &Chosen->SprOx, &Chosen->SprOy );
}

void FOClient::MainLoop()
{
    Timer::UpdateTick();

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

    // Check for quit, poll pending events
    if( InitCalls < 2 )
    {
        SDL_Event event;
        while( SDL_PollEvent( &event ) )
            if( ( event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE ) || event.type == SDL_QUIT )
                GameOpt.Quit = true;
    }

    // Game end
    if( GameOpt.Quit )
        return;

    // Restart logic
    if( DoRestart )
    {
        DoRestart = false;
        Restart();
    }

    // Network connection
    if( IsConnecting )
    {
        if( !CheckSocketStatus( true ) )
            return;
    }

    if( UpdateFilesInProgress )
    {
        UpdateFilesLoop();
        return;
    }

    if( InitCalls < 2 )
    {
        if( ( InitCalls == 0 && !PreInit() ) || ( InitCalls == 1 && !PostInit() ) )
        {
            WriteLog( "FOnline engine initialization failed.\n" );
            GameOpt.Quit = true;
            return;
        }

        InitCalls++;
        if( InitCalls == 1 )
        {
            UpdateFilesStart();
        }
        else if( InitCalls == 2 )
        {
            if( !IsVideoPlayed() )
                ScreenFadeOut();
        }

        return;
    }

    // Input events
    SDL_Event event;
    while( SDL_PollEvent( &event ) )
    {
        if( event.type == SDL_MOUSEMOTION )
        {
            int sw = 0, sh = 0;
            SprMngr.GetWindowSize( sw, sh );
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

    // Network
    if( InitNetReason != INIT_NET_REASON_NONE && !UpdateFilesInProgress )
    {
        // Connect to server
        if( !IsConnected )
        {
            if( !NetConnect( GameOpt.Host.c_str(), GameOpt.Port ) )
            {
                ShowMainScreen( SCREEN_LOGIN );
                AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_CONN_FAIL ) );
            }
        }
        else
        {
            // Reason
            int reason = InitNetReason;
            InitNetReason = INIT_NET_REASON_NONE;

            // After connect things
            if( reason == INIT_NET_REASON_LOGIN )
                Net_SendLogIn();
            else if( reason == INIT_NET_REASON_REG )
                Net_SendCreatePlayer();
            // else if( reason == INIT_NET_REASON_LOAD )
            //    Net_SendSaveLoad( false, SaveLoadFileName.c_str(), nullptr );
            else if( reason != INIT_NET_REASON_CUSTOM )
                RUNTIME_ASSERT( !"Unreachable place" );
        }
    }

    // Parse Net
    if( IsConnected )
        ParseSocket();

    // Exit in Login screen if net disconnect
    if( !IsConnected && !IsMainScreen( SCREEN_LOGIN ) )
        ShowMainScreen( SCREEN_LOGIN );

    // Input
    ParseKeyboard();
    ParseMouse();

    // Process
    AnimProcess();

    // Game time
    if( Timer::ProcessGameTime() )
        SetDayTime( false );

    if( IsMainScreen( SCREEN_GLOBAL_MAP ) )
    {
        CrittersProcess();
    }
    else if( IsMainScreen( SCREEN_GAME ) && HexMngr.IsMapLoaded() )
    {
        if( HexMngr.Scroll() )
            LookBordersPrepare();
        CrittersProcess();
        HexMngr.ProcessItems();
        HexMngr.ProcessRain();
    }

    // Video
    if( IsVideoPlayed() )
    {
        RenderVideo();
        return;
    }

    // Start render
    SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) );

    // Process pending invocations
    Script::ProcessDeferredCalls();

    // Script loop
    Script::RaiseInternalEvent( ClientFunctions.Loop );

    // Suspended contexts
    Script::RunSuspended();

    // Quake effect
    ProcessScreenEffectQuake();

    // Render
    if( GetMainScreen() == SCREEN_GAME && HexMngr.IsMapLoaded() )
        GameDraw();

    DrawIface();

    ProcessScreenEffectFading();

    SprMngr.EndScene();
}

void FOClient::DrawIface()
{
    // Make dirty offscreen surfaces
    if( !PreDirtyOffscreenSurfaces.empty() )
    {
        DirtyOffscreenSurfaces.insert( DirtyOffscreenSurfaces.end(), Self->PreDirtyOffscreenSurfaces.begin(), Self->PreDirtyOffscreenSurfaces.end() );
        PreDirtyOffscreenSurfaces.clear();
    }

    CanDrawInScripts = true;
    Script::RaiseInternalEvent( ClientFunctions.RenderIface );
    CanDrawInScripts = false;
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
    SprMngr.Flush();

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

void FOClient::ParseKeyboard()
{
    // Stop processing if window not active
    if( !SprMngr.IsWindowFocused() )
    {
        MainWindowKeyboardEvents.clear();
        MainWindowKeyboardEventsText.clear();
        Keyb::Lost();
        Script::RaiseInternalEvent( ClientFunctions.InputLost );
        return;
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
        if( dikdw )
        {
            string s = event_text;
            Script::RaiseInternalEvent( ClientFunctions.KeyDown, dikdw, &s );
        }

        if( dikup )
            Script::RaiseInternalEvent( ClientFunctions.KeyUp, dikup );

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
    if( !SprMngr.IsWindowFocused() )
    {
        MainWindowMouseEvents.clear();
        Script::RaiseInternalEvent( ClientFunctions.InputLost );
        return;
    }

    // Mouse move
    if( GameOpt.LastMouseX != GameOpt.MouseX || GameOpt.LastMouseY != GameOpt.MouseY )
    {
        int ox = GameOpt.MouseX - GameOpt.LastMouseX;
        int oy = GameOpt.MouseY - GameOpt.LastMouseY;
        GameOpt.LastMouseX = GameOpt.MouseX;
        GameOpt.LastMouseY = GameOpt.MouseY;

        Script::RaiseInternalEvent( ClientFunctions.MouseMove, ox, oy );
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

        // Scripts
        if( event == SDL_MOUSEBUTTONDOWN )
            Script::RaiseInternalEvent( ClientFunctions.MouseDown, event_button );
        else if( event == SDL_MOUSEBUTTONUP )
            Script::RaiseInternalEvent( ClientFunctions.MouseUp, event_button );
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

bool FOClient::CheckSocketStatus( bool for_write )
{
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO( &SockSet );
    FD_SET( Sock, &SockSet );
    int r = select( (int) Sock + 1, for_write ? nullptr : &SockSet, for_write ? &SockSet : nullptr, nullptr, &tv );
    if( r == 1 )
    {
        // Ready
        if( IsConnecting )
        {
            WriteLog( "Connection established.\n" );
            IsConnecting = false;
            IsConnected = true;
        }
        return true;
    }
    else if( r == 0 )
    {
        // Not ready
        int       error = 0;
        #ifdef FO_WINDOWS
        int       len = sizeof( error );
        #else
        socklen_t len = sizeof( error );
        #endif
        if( getsockopt( Sock, SOL_SOCKET, SO_ERROR, (char*) &error, &len ) != SOCKET_ERROR && !error )
            return false;

        WriteLog( "Socket error '{}'.\n", GetLastSocketError() );
    }
    else
    {
        // Error
        WriteLog( "Socket select error '{}'.\n", GetLastSocketError() );
    }

    if( IsConnecting )
    {
        WriteLog( "Can't connect to server.\n" );
        IsConnecting = false;
    }

    NetDisconnect();
    return false;
}

bool FOClient::NetConnect( const char* host, ushort port )
{
    #ifdef FO_WEB
    port++;
    string wss_credentials = MainConfig->GetStr( "", "WssCredentials", "" );
    if( wss_credentials.empty() )
    {
        EM_ASM( Module[ 'websocket' ][ 'url' ] = 'ws://' );
        WriteLog( "Connecting to server 'ws://{}:{}'.\n", host, port );
    }
    else
    {
        EM_ASM( Module[ 'websocket' ][ 'url' ] = 'wss://' );
        WriteLog( "Connecting to server 'wss://{}:{}'.\n", host, port );
    }
    #else
    WriteLog( "Connecting to server '{}:{}'.\n", host, port );
    #endif

    IsConnecting = false;
    IsConnected = false;

    if( !ZStreamOk )
    {
        ZStream.zalloc = zlib_alloc_;
        ZStream.zfree = zlib_free_;
        ZStream.opaque = nullptr;
        RUNTIME_ASSERT( inflateInit( &ZStream ) == Z_OK );
        ZStreamOk = true;
    }

    #ifdef FO_WINDOWS
    WSADATA wsa;
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) )
    {
        WriteLog( "WSAStartup error '{}'.\n", GetLastSocketError() );
        return false;
    }
    #endif

    if( !FillSockAddr( SockAddr, host, port ) )
        return false;
    if( GameOpt.ProxyType && !FillSockAddr( ProxyAddr, GameOpt.ProxyHost.c_str(), GameOpt.ProxyPort ) )
        return false;

    Bin.SetEncryptKey( 0 );
    Bout.SetEncryptKey( 0 );

    if( ( Sock = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == INVALID_SOCKET )
    {
        WriteLog( "Create socket error '{}'.\n", GetLastSocketError() );
        return false;
    }

    // Nagle
    #ifndef FO_WEB
    if( GameOpt.DisableTcpNagle )
    {
        int optval = 1;
        if( setsockopt( Sock, IPPROTO_TCP, TCP_NODELAY, (char*) &optval, sizeof( optval ) ) )
            WriteLog( "Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'.\n", GetLastSocketError() );
    }
    #endif

    // Direct connect
    if( !GameOpt.ProxyType )
    {
        // Set non blocking mode
        #ifdef FO_WINDOWS
        unsigned long mode = 1;
        if( ioctlsocket( Sock, FIONBIO, &mode ) )
        #else
        int flags = fcntl( Sock, F_GETFL, 0 );
        RUNTIME_ASSERT( flags >= 0 );
        if( fcntl( Sock, F_SETFL, flags | O_NONBLOCK ) )
        #endif
        {
            WriteLog( "Can't set non-blocking mode to socket, error '{}'.\n", GetLastSocketError() );
            return false;
        }

        int r = connect( Sock, (sockaddr*) &SockAddr, sizeof( sockaddr_in ) );
        #ifdef FO_WINDOWS
        if( r == INVALID_SOCKET && WSAGetLastError() != WSAEWOULDBLOCK )
        #else
        if( r == INVALID_SOCKET && errno != EINPROGRESS )
        #endif
        {
            WriteLog( "Can't connect to game server, error '{}'.\n", GetLastSocketError() );
            return false;
        }
    }
    // Proxy connect
    #if !defined ( FO_IOS ) && !defined ( FO_ANDROID ) && !defined ( FO_WEB )
    else
    {
        if( connect( Sock, (sockaddr*) &ProxyAddr, sizeof( sockaddr_in ) ) )
        {
            WriteLog( "Can't connect to proxy server, error '{}'.\n", GetLastSocketError() );
            return false;
        }

// ==========================================
        # define SEND_RECV                                     \
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
                    Thread_Sleep( 1 );                         \
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
                    WriteLog( "Proxy connection error, Unknown error {}.\n", b2 );
                    break;
                }
                return false;
            }
        }
        else if( GameOpt.ProxyType == PROXY_SOCKS5 )
        {
            Bout << uchar( 5 );                                                              // Socks version
            Bout << uchar( 1 );                                                              // Count methods
            Bout << uchar( 2 );                                                              // Method
            SEND_RECV;
            Bin >> b1;                                                                       // Socks version
            Bin >> b2;                                                                       // Method
            if( b2 == 2 )                                                                    // User/Password
            {
                Bout << uchar( 1 );                                                          // Subnegotiation version
                Bout << uchar( GameOpt.ProxyUser.length() );                                 // Name length
                Bout.Push( GameOpt.ProxyUser.c_str(), (uint) GameOpt.ProxyUser.length() );   // Name
                Bout << uchar( GameOpt.ProxyPass.length() );                                 // Pass length
                Bout.Push( GameOpt.ProxyPass.c_str(), (uint) GameOpt.ProxyPass.length() );   // Pass
                SEND_RECV;
                Bin >> b1;                                                                   // Subnegotiation version
                Bin >> b2;                                                                   // Status
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
                    WriteLog( "Proxy connection error, unknown error {}.\n", b2 );
                    break;
                }
                return false;
            }
        }
        else if( GameOpt.ProxyType == PROXY_HTTP )
        {
            string buf = _str( "CONNECT {}:{} HTTP/1.0\r\n\r\n", inet_ntoa( SockAddr.sin_addr ), port );
            Bout.Push( buf.c_str(), (uint) buf.length() );
            SEND_RECV;
            buf = (const char*) Bin.GetCurData();
            if( buf.find( " 200 " ) == string::npos )
            {
                WriteLog( "Proxy connection error, receive message '{}'.\n", buf );
                return false;
            }
        }
        else
        {
            WriteLog( "Unknown proxy type {}.\n", GameOpt.ProxyType );
            return false;
        }

        # undef SEND_RECV

        Bin.Reset();
        Bout.Reset();

        IsConnected = true;
    }
    #endif

    IsConnecting = true;
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
            WriteLog( "Can't resolve remote host '{}', error '{}'.", host, GetLastSocketError() );
            return false;
        }

        memcpy( &saddr.sin_addr, h->h_addr, sizeof( in_addr ) );
    }
    return true;
}

void FOClient::NetDisconnect()
{
    if( IsConnecting )
    {
        IsConnecting = false;
        if( Sock != INVALID_SOCKET )
            closesocket( Sock );
        Sock = INVALID_SOCKET;
    }

    if( !IsConnected )
        return;

    WriteLog( "Disconnect. Session traffic: send {}, receive {}, whole {}, receive real {}.\n",
              BytesSend, BytesReceive, BytesReceive + BytesSend, BytesRealReceive );

    IsConnected = false;

    if( ZStreamOk )
        inflateEnd( &ZStream );
    ZStreamOk = false;
    if( Sock != INVALID_SOCKET )
        closesocket( Sock );
    Sock = INVALID_SOCKET;

    HexMngr.UnloadMap();
    DeleteCritters();
    Bin.Reset();
    Bout.Reset();
    Bin.SetEncryptKey( 0 );
    Bout.SetEncryptKey( 0 );
    Bin.SetError( false );
    Bout.SetError( false );

    ProcessAutoLogin();
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
    if( !CheckSocketStatus( true ) )
        return IsConnected;

    int tosend = Bout.GetEndPos();
    int sendpos = 0;
    while( sendpos < tosend )
    {
        #ifdef FO_WINDOWS
        DWORD  len;
        WSABUF buf;
        buf.buf = (char*) Bout.GetData() + sendpos;
        buf.len = tosend - sendpos;
        if( WSASend( Sock, &buf, 1, &len, 0, nullptr, nullptr ) == SOCKET_ERROR || len == 0 )
        #else
        int len = (int) send( Sock, Bout.GetData() + sendpos, tosend - sendpos, 0 );
        if( len <= 0 )
        #endif
        {
            WriteLog( "Socket error while send to server, error '{}'.\n", GetLastSocketError() );
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
    if( !CheckSocketStatus( false ) )
        return 0;

    #ifdef FO_WINDOWS
    DWORD  len;
    DWORD  flags = 0;
    WSABUF buf;
    buf.buf = (char*) ComBuf;
    buf.len = ComLen;
    if( WSARecv( Sock, &buf, 1, &len, &flags, nullptr, nullptr ) == SOCKET_ERROR )
    #else
    int len = recv( Sock, ComBuf, ComLen, 0 );
    if( len == SOCKET_ERROR )
    #endif
    {
        WriteLog( "Socket error while receive from server, error '{}'.\n", GetLastSocketError() );
        return -1;
    }
    if( len == 0 )
    {
        WriteLog( "Socket is closed.\n" );
        return -2;
    }

    uint whole_len = (uint) len;
    while( whole_len == ComLen )
    {
        uint   newcomlen = ( ComLen << 1 );
        uchar* combuf = new uchar[ newcomlen ];
        memcpy( combuf, ComBuf, ComLen );
        SAFEDELA( ComBuf );
        ComBuf = combuf;
        ComLen = newcomlen;

        #ifdef FO_WINDOWS
        flags = 0;
        buf.buf = (char*) ComBuf + whole_len;
        buf.len = ComLen - whole_len;
        if( WSARecv( Sock, &buf, 1, &len, &flags, nullptr, nullptr ) == SOCKET_ERROR )
        #else
        len = recv( Sock, ComBuf + whole_len, ComLen - whole_len, 0 );
        if( len == SOCKET_ERROR )
        #endif
        {
            #ifdef FO_WINDOWS
            if( WSAGetLastError() == WSAEWOULDBLOCK )
            #else
            if( errno == EINPROGRESS )
            #endif
                break;

            WriteLog( "Socket error (2) while receive from server, error '{}'.\n", GetLastSocketError() );
            return -1;
        }
        if( len == 0 )
        {
            WriteLog( "Socket is closed (2).\n" );
            return -2;
        }

        whole_len += len;
    }

    Bin.Refresh();
    uint old_pos = Bin.GetEndPos();

    if( unpack && !GameOpt.DisableZlibCompression )
    {
        ZStream.next_in = ComBuf;
        ZStream.avail_in = whole_len;
        ZStream.next_out = Bin.GetData() + Bin.GetEndPos();
        ZStream.avail_out = Bin.GetLen() - Bin.GetEndPos();

        int first_inflate = inflate( &ZStream, Z_SYNC_FLUSH );
        RUNTIME_ASSERT( first_inflate == Z_OK );

        uint uncompr = (uint) ( (size_t) ZStream.next_out - (size_t) Bin.GetData() );
        Bin.SetEndPos( uncompr );

        while( ZStream.avail_in )
        {
            Bin.GrowBuf( BufferManager::DefaultBufSize );

            ZStream.next_out = (uchar*) Bin.GetData() + Bin.GetEndPos();
            ZStream.avail_out = Bin.GetLen() - Bin.GetEndPos();

            int next_inflate = inflate( &ZStream, Z_SYNC_FLUSH );
            RUNTIME_ASSERT( next_inflate == Z_OK );

            uncompr = (uint) ( (size_t) ZStream.next_out - (size_t) Bin.GetData() );
            Bin.SetEndPos( uncompr );
        }
    }
    else
    {
        Bin.Push( ComBuf, whole_len, true );
    }

    BytesReceive += whole_len;
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
            AddMess( FOMB_GAME, _str( "{:04}) Input net message {}.", count, ( msg >> 8 ) & 0xFF ) );
            WriteLog( "{}) Input net message {}.\n", count, ( msg >> 8 ) & 0xFF );
            count++;
        }

        switch( msg )
        {
        case NETMSG_DISCONNECT:
            NetDisconnect();
            break;
        case NETMSG_WRONG_NET_PROTO:
            Net_OnWrongNetProto();
            break;
        case NETMSG_LOGIN_SUCCESS:
            Net_OnLoginSuccess();
            break;
        case NETMSG_REGISTER_SUCCESS:
            WriteLog( "Registration success.\n" );
            break;

        case NETMSG_PING:
            Net_OnPing();
            break;
        case NETMSG_END_PARSE_TO_GAME:
            Net_OnEndParseToGame();
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

        case NETMSG_POD_PROPERTY( 1, 0 ):
        case NETMSG_POD_PROPERTY( 1, 1 ):
        case NETMSG_POD_PROPERTY( 1, 2 ):
            Net_OnProperty( 1 );
            break;
        case NETMSG_POD_PROPERTY( 2, 0 ):
        case NETMSG_POD_PROPERTY( 2, 1 ):
        case NETMSG_POD_PROPERTY( 2, 2 ):
            Net_OnProperty( 2 );
            break;
        case NETMSG_POD_PROPERTY( 4, 0 ):
        case NETMSG_POD_PROPERTY( 4, 1 ):
        case NETMSG_POD_PROPERTY( 4, 2 ):
            Net_OnProperty( 4 );
            break;
        case NETMSG_POD_PROPERTY( 8, 0 ):
        case NETMSG_POD_PROPERTY( 8, 1 ):
        case NETMSG_POD_PROPERTY( 8, 2 ):
            Net_OnProperty( 8 );
            break;
        case NETMSG_COMPLEX_PROPERTY:
            Net_OnProperty( 0 );
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

        case NETMSG_ALL_PROPERTIES:
            Net_OnAllProperties();
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

        case NETMSG_TALK_NPC:
            Net_OnChosenTalk();
            break;

        case NETMSG_GAME_INFO:
            Net_OnGameInfo();
            break;

        case NETMSG_AUTOMAPS_INFO:
            Net_OnAutomapsInfo();
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
        case NETMSG_SOME_ITEMS:
            Net_OnSomeItems();
            break;
        case NETMSG_RPC:
            Script::HandleRpc( &Bin );
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
            Net_OnPlaySound();
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
    uint encrypt_key = 0x00420042;
    Bout << encrypt_key;
    Bout.SetEncryptKey( encrypt_key + 521 );
    Bin.SetEncryptKey( encrypt_key + 3491 );
}

void FOClient::Net_SendLogIn()
{
    char name_[ UTF8_BUF_SIZE( MAX_NAME ) ] = { 0 };
    char pass_[ UTF8_BUF_SIZE( MAX_NAME ) ] = { 0 };
    memzero( name_, sizeof( name_ ) );
    Str::Copy( name_, LoginName.c_str() );
    memzero( pass_, sizeof( pass_ ) );
    Str::Copy( pass_, LoginPassword.c_str() );

    Bout << NETMSG_LOGIN;
    Bout << (ushort) FONLINE_VERSION;

    // Begin data encrypting
    Bout.SetEncryptKey( 12345 );
    Bin.SetEncryptKey( 12345 );

    Bout.Push( name_, sizeof( name_ ) );
    Bout.Push( pass_, sizeof( pass_ ) );
    Bout << CurLang.Name;

    AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_CONN_SUCCESS ) );
}

void FOClient::Net_SendCreatePlayer()
{
    WriteLog( "Player registration..." );

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( ushort ) + UTF8_BUF_SIZE( MAX_NAME ) * 2;

    Bout << NETMSG_CREATE_CLIENT;
    Bout << msg_len;

    Bout << (ushort) FONLINE_VERSION;

    // Begin data encrypting
    Bout.SetEncryptKey( 1234567890 );
    Bin.SetEncryptKey( 1234567890 );

    char buf[ UTF8_BUF_SIZE( MAX_NAME ) ];
    memzero( buf, sizeof( buf ) );
    Str::Copy( buf, LoginName.c_str() );
    Bout.Push( buf, UTF8_BUF_SIZE( MAX_NAME ) );
    memzero( buf, sizeof( buf ) );
    Str::Copy( buf, LoginPassword.c_str() );
    Bout.Push( buf, UTF8_BUF_SIZE( MAX_NAME ) );

    WriteLog( "complete.\n" );
}

void FOClient::Net_SendText( const char* send_str, uchar how_say )
{
    if( !send_str || !send_str[ 0 ] )
        return;

    bool   result = false;
    int    say_type = how_say;
    string str = send_str;
    result = Script::RaiseInternalEvent( ClientFunctions.OutMessage, &str, &say_type );

    how_say = say_type;

    if( !result || str.empty() )
        return;

    ushort len = (ushort) str.length();
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( how_say ) + sizeof( len ) + len;

    Bout << NETMSG_SEND_TEXT;
    Bout << msg_len;
    Bout << how_say;
    Bout << len;
    Bout.Push( str.c_str(), len );
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
    Bout << Chosen->GetHexX();
    Bout << Chosen->GetHexY();
}

void FOClient::Net_SendProperty( NetProperty::Type type, Property* prop, Entity* entity )
{
    RUNTIME_ASSERT( entity );

    uint additional_args = 0;
    switch( type )
    {
    case NetProperty::Critter:
        additional_args = 1;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        break;
    case NetProperty::CritterItem:
        additional_args = 2;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        break;
    default:
        break;
    }

    uint  data_size;
    void* data = prop->GetRawData( entity, data_size );

    bool  is_pod = prop->IsPOD();
    if( is_pod )
    {
        Bout << NETMSG_SEND_POD_PROPERTY( data_size, additional_args );
    }
    else
    {
        uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( char ) + additional_args * sizeof( uint ) + sizeof( ushort ) + data_size;
        Bout << NETMSG_SEND_COMPLEX_PROPERTY;
        Bout << msg_len;
    }

    Bout << (char) type;

    switch( type )
    {
    case NetProperty::CritterItem:
        Bout << ( (Item*) entity )->GetCritId();
        Bout << entity->Id;
        break;
    case NetProperty::Critter:
        Bout << entity->Id;
        break;
    case NetProperty::MapItem:
        Bout << entity->Id;
        break;
    case NetProperty::ChosenItem:
        Bout << entity->Id;
        break;
    default:
        break;
    }

    if( is_pod )
    {
        Bout << (ushort) prop->GetRegIndex();
        Bout.Push( data, data_size );
    }
    else
    {
        Bout << (ushort) prop->GetRegIndex();
        if( data_size )
            Bout.Push( data, data_size );
    }
}

void FOClient::Net_SendTalk( uchar is_npc, uint id_to_talk, uchar answer )
{
    Bout << NETMSG_SEND_TALK_NPC;
    Bout << is_npc;
    Bout << id_to_talk;
    Bout << answer;
}

void FOClient::Net_SendGetGameInfo()
{
    Bout << NETMSG_SEND_GET_INFO;
}

void FOClient::Net_SendGiveMap( bool automap, hash map_pid, uint loc_id, hash tiles_hash, hash scen_hash )
{
    Bout << NETMSG_SEND_GIVE_MAP;
    Bout << automap;
    Bout << map_pid;
    Bout << loc_id;
    Bout << tiles_hash;
    Bout << scen_hash;
}

void FOClient::Net_SendLoadMapOk()
{
    Bout << NETMSG_SEND_LOAD_MAP_OK;
}

void FOClient::Net_SendPing( uchar ping )
{
    Bout << NETMSG_PING;
    Bout << ping;
}

void FOClient::Net_SendRefereshMe()
{
    Bout << NETMSG_SEND_REFRESH_ME;

    WaitPing();
}

void FOClient::Net_OnWrongNetProto()
{
    if( UpdateFilesInProgress )
        UpdateFilesAbort( STR_CLIENT_OUTDATED, "Client outdated!" );
    else
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_CLIENT_OUTDATED ) );
}

void FOClient::Net_OnLoginSuccess()
{
    WriteLog( "Authentication success.\n" );

    AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_LOGINOK ) );

    // Set encrypt keys
    uint msg_len;
    uint bin_seed, bout_seed;         // Server bin/bout == client bout/bin

    Bin >> msg_len;
    Bin >> bin_seed;
    Bin >> bout_seed;
    NET_READ_PROPERTIES( Bin, GlovalVarsPropertiesData );

    CHECK_IN_BUFF_ERROR;

    Bout.SetEncryptKey( bin_seed );
    Bin.SetEncryptKey( bout_seed );
    Globals->Props.RestoreData( GlovalVarsPropertiesData );
}

void FOClient::Net_OnAddCritter( bool is_npc )
{
    uint msg_len;
    Bin >> msg_len;

    uint   crid;
    ushort hx, hy;
    uchar  dir;
    Bin >> crid;
    Bin >> hx;
    Bin >> hy;
    Bin >> dir;

    int  cond;
    uint anim1life, anim1ko, anim1dead;
    uint anim2life, anim2ko, anim2dead;
    uint flags;
    Bin >> cond;
    Bin >> anim1life;
    Bin >> anim1ko;
    Bin >> anim1dead;
    Bin >> anim2life;
    Bin >> anim2ko;
    Bin >> anim2dead;
    Bin >> flags;

    // Npc
    hash npc_pid = 0;
    if( is_npc )
        Bin >> npc_pid;

    // Player
    char cl_name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    if( !is_npc )
    {
        Bin.Pop( cl_name, sizeof( cl_name ) );
        cl_name[ sizeof( cl_name ) - 1 ] = 0;
    }

    // Properties
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    if( !crid )
    {
        WriteLog( "Critter id is zero.\n" );
    }
    else if( HexMngr.IsMapLoaded() && ( hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight() || dir >= DIRS_COUNT ) )
    {
        WriteLog( "Invalid positions hx {}, hy {}, dir {}.\n", hx, hy, dir );
    }
    else
    {
        ProtoCritter* proto = ProtoMngr.GetProtoCritter( is_npc ? npc_pid : _str( "Player" ).toHash() );
        RUNTIME_ASSERT( proto );
        CritterCl*    cr = new CritterCl( crid, proto );
        cr->Props.RestoreData( TempPropertiesData );
        cr->SetHexX( hx );
        cr->SetHexY( hy );
        cr->SetDir( dir );
        cr->SetCond( cond );
        cr->SetAnim1Life( anim1life );
        cr->SetAnim1Knockout( anim1ko );
        cr->SetAnim1Dead( anim1dead );
        cr->SetAnim2Life( anim2life );
        cr->SetAnim2Knockout( anim2ko );
        cr->SetAnim2Dead( anim2dead );
        cr->Flags = flags;

        if( is_npc )
        {
            if( cr->GetDialogId() && CurLang.Msg[ TEXTMSG_DLG ].Count( STR_NPC_NAME( cr->GetDialogId() ) ) )
                cr->Name = CurLang.Msg[ TEXTMSG_DLG ].GetStr( STR_NPC_NAME( cr->GetDialogId() ) );
            else
                cr->Name = CurLang.Msg[ TEXTMSG_DLG ].GetStr( STR_NPC_PID_NAME( npc_pid ) );
            if( CurLang.Msg[ TEXTMSG_DLG ].Count( STR_NPC_AVATAR( cr->GetDialogId() ) ) )
                cr->Avatar = CurLang.Msg[ TEXTMSG_DLG ].GetStr( STR_NPC_AVATAR( cr->GetDialogId() ) );
        }
        else
        {
            cr->Name = cl_name;
        }

        cr->Init();

        AddCritter( cr );

        Script::RaiseInternalEvent( ClientFunctions.CritterIn, cr );

        if( cr->IsChosen() )
            RebuildLookBorders = true;
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

    Script::RaiseInternalEvent( ClientFunctions.CritterOut, cr );
}

void FOClient::Net_OnText()
{
    uint   msg_len;
    uint   crid;
    uchar  how_say;
    string text;
    bool   unsafe_text;

    Bin >> msg_len;
    Bin >> crid;
    Bin >> how_say;
    Bin >> text;
    Bin >> unsafe_text;

    CHECK_IN_BUFF_ERROR;

    if( how_say == SAY_FLASH_WINDOW )
    {
        FlashGameWindow();
        return;
    }

    if( unsafe_text )
        Keyb::EraseInvalidChars( text, KIF_NO_SPEC_SYMBOLS );
    OnText( text, crid, how_say );
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
        WriteLog( "Msg num invalid value {}.\n", msg_num );
        return;
    }

    FOMsg& msg = CurLang.Msg[ msg_num ];
    if( msg.Count( num_str ) )
    {
        string str = msg.GetStr( num_str );
        FormatTags( str, Chosen, GetCritter( crid ), lexems );
        OnText( str.c_str(), crid, how_say );
    }
}

void FOClient::OnText( const string& str, uint crid, int how_say )
{
    string fstr = str;
    if( fstr.empty() )
        return;

    uint   text_delay = GameOpt.TextDelay + (uint) fstr.length() * 100;
    string sstr = fstr;
    bool   result = Script::RaiseInternalEvent( ClientFunctions.InMessage, &sstr, &how_say, &crid, &text_delay );
    if( !result )
        return;

    fstr = sstr;
    if( fstr.empty() )
        return;

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
        fstr = _str( fstr ).upperUtf8();
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
        fstr = _str( fstr ).lowerUtf8();
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

    CritterCl* cr = nullptr;
    if( how_say != SAY_RADIO )
        cr = GetCritter( crid );

    string crit_name = ( cr ? cr->GetName() : "?" );

    // CritterCl on head text
    if( fstr_cr && cr )
        cr->SetText( FmtGameText( fstr_cr, fstr.c_str() ).c_str(), COLOR_TEXT, text_delay );

    // Message box text
    if( fstr_mb )
    {
        if( how_say == SAY_NETMSG )
        {
            AddMess( mess_type, FmtGameText( fstr_mb, fstr.c_str() ) );
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
            AddMess( mess_type, FmtGameText( fstr_mb, channel, fstr.c_str() ) );
        }
        else
        {
            AddMess( mess_type, FmtGameText( fstr_mb, crit_name.c_str(), fstr.c_str() ) );
        }
    }

    FlashGameWindow();
}

void FOClient::OnMapText( const string& str, ushort hx, ushort hy, uint color )
{
    uint   text_delay = GameOpt.TextDelay + (uint) str.length() * 100;
    string sstr = str;
    Script::RaiseInternalEvent( ClientFunctions.MapMessage, &sstr, &hx, &hy, &color, &text_delay );

    MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = ( color ? color : COLOR_TEXT );
    t.Fade = false;
    t.StartTick = Timer::GameTick();
    t.Tick = text_delay;
    t.Text = sstr;
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
    string text;
    bool   unsafe_text;

    Bin >> msg_len;
    Bin >> hx;
    Bin >> hy;
    Bin >> color;
    Bin >> text;
    Bin >> unsafe_text;

    CHECK_IN_BUFF_ERROR;

    if( hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight() )
    {
        WriteLog( "Invalid coords, hx {}, hy {}, text '{}'.\n", hx, hy, text );
        return;
    }

    if( unsafe_text )
        Keyb::EraseInvalidChars( text, KIF_NO_SPEC_SYMBOLS );
    OnMapText( text, hx, hy, color );
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
        WriteLog( "Msg num invalid value, num {}.\n", msg_num );
        return;
    }

    string str = CurLang.Msg[ msg_num ].GetStr( num_str );
    FormatTags( str, Chosen, nullptr, "" );
    OnMapText( str, hx, hy, color );
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
        WriteLog( "Msg num invalid value, num {}.\n", msg_num );
        return;
    }

    string str = CurLang.Msg[ msg_num ].GetStr( num_str );
    FormatTags( str, Chosen, nullptr, lexems );
    OnMapText( str, hx, hy, color );
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
        WriteLog( "Invalid dir {}.\n", dir );
        dir = 0;
    }

    CritterCl* cr = GetCritter( crid );
    if( cr )
        cr->ChangeDir( dir );
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

    if( new_hx >= HexMngr.GetWidth() || new_hy >= HexMngr.GetHeight() )
        return;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    cr->IsRunning = FLAG( move_params, MOVE_PARAM_RUN );

    if( cr != Chosen )
    {
        cr->MoveSteps.resize( cr->CurMoveStep > 0 ? cr->CurMoveStep : 0 );
        if( cr->CurMoveStep >= 0 )
            cr->MoveSteps.push_back( std::make_pair( new_hx, new_hy ) );

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
            MoveHexByDir( new_hx, new_hy, dir, HexMngr.GetWidth(), HexMngr.GetHeight() );
            if( j < 0 )
                continue;
            cr->MoveSteps.push_back( std::make_pair( new_hx, new_hy ) );
        }
        cr->CurMoveStep++;
    }
    else
    {
        cr->MoveSteps.clear();
        cr->CurMoveStep = 0;
        HexMngr.TransitCritter( cr, new_hx, new_hy, true, true );
    }
}

void FOClient::Net_OnSomeItem()
{
    uint msg_len;
    uint item_id;
    hash item_pid;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> item_pid;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    CHECK_IN_BUFF_ERROR;

    SAFEREL( SomeItem );

    ProtoItem* proto_item = ProtoMngr.GetProtoItem( item_pid );
    RUNTIME_ASSERT( proto_item );
    SomeItem = new Item( item_id, proto_item );
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

    cr->Action( action, action_ext, is_item ? SomeItem : nullptr, false );
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

        cr->DeleteAllItems();

        for( uchar i = 0; i < slots_data_count; i++ )
        {
            ProtoItem* proto_item = ProtoMngr.GetProtoItem( slots_data_pid[ i ] );
            if( proto_item )
            {
                Item* item = new Item( slots_data_id[ i ], proto_item );
                item->Props.RestoreData( slots_data_data[ i ] );
                item->SetCritSlot( slots_data_slot[ i ] );
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

    cr->Action( action, prev_slot, is_item ? SomeItem : nullptr, false );
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
        cr->Animate( anim1, anim2, is_item ? SomeItem : nullptr );
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
            cr->SetAnim1Life( anim1 );
            cr->SetAnim2Life( anim2 );
        }
        else if( cond == 0 || cond == COND_KNOCKOUT )
        {
            cr->SetAnim1Knockout( anim1 );
            cr->SetAnim2Knockout( anim2 );
        }
        else if( cond == 0 || cond == COND_DEAD )
        {
            cr->SetAnim1Dead( anim1 );
            cr->SetAnim2Dead( anim2 );
        }
        if( !cr->IsAnim() )
            cr->AnimateStay();
    }
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
        AddMess( FOMB_GAME, _str( " - crid {} index {} value {}.", crid, index, value ) );

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    if( cr->IsChosen() )
    {
        if( GameOpt.DebugNet )
            AddMess( FOMB_GAME, _str( " - index {} value {}.", index, value ) );

        switch( index )
        {
        case OTHER_BREAK_TIME:
        {
            if( value < 0 )
                value = 0;
            Chosen->TickStart( value );
            Chosen->MoveSteps.clear();
            Chosen->CurMoveStep = 0;
        }
        break;
        case OTHER_FLAGS:
        {
            Chosen->Flags = value;
        }
        break;
        case OTHER_CLEAR_MAP:
        {
            CritMap crits = HexMngr.GetCritters();
            for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
            {
                CritterCl* cr = it->second;
                if( cr != Chosen )
                    DeleteCritter( cr->GetId() );
            }
            ItemHexVec items = HexMngr.GetItems();
            for( auto it = items.begin(), end = items.end(); it != end; ++it )
            {
                ItemHex* item = *it;
                if( !item->IsStatic() )
                    HexMngr.DeleteItem( *it, true );
            }
        }
        break;
        case OTHER_TELEPORT:
        {
            ushort hx = ( value >> 16 ) & 0xFFFF;
            ushort hy = value & 0xFFFF;
            if( hx < HexMngr.GetWidth() && hy < HexMngr.GetHeight() )
            {
                CritterCl* cr = HexMngr.GetField( hx, hy ).Crit;
                if( Chosen == cr )
                    break;
                if( !Chosen->IsDead() && cr )
                    DeleteCritter( cr->GetId() );
                HexMngr.RemoveCritter( Chosen );
                Chosen->SetHexX( hx );
                Chosen->SetHexY( hy );
                HexMngr.SetCritter( Chosen );
                HexMngr.ScrollToHex( Chosen->GetHexX(), Chosen->GetHexY(), 0.1f, true );
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
            cr->Flags = value;
    }
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
        AddMess( FOMB_GAME, _str( " - crid {} hx {} hy {} dir {}.", crid, hx, hy, dir ) );

    if( !HexMngr.IsMapLoaded() )
        return;

    CritterCl* cr = GetCritter( crid );
    if( !cr )
        return;

    if( hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight() || dir >= DIRS_COUNT )
    {
        WriteLog( "Error data, hx {}, hy {}, dir {}.\n", hx, hy, dir );
        return;
    }

    if( cr->GetDir() != dir )
        cr->ChangeDir( dir );

    if( cr->GetHexX() != hx || cr->GetHexY() != hy )
    {
        Field& f = HexMngr.GetField( hx, hy );
        if( f.Crit && f.Crit->IsFinishing() )
            DeleteCritter( f.Crit->GetId() );

        HexMngr.TransitCritter( cr, hx, hy, false, true );
        cr->AnimateStay();
        if( cr == Chosen )
        {
            // Chosen->TickStart(GameOpt.Breaktime);
            Chosen->TickStart( 200 );
            // SetAction(CHOSEN_NONE);
            Chosen->MoveSteps.clear();
            Chosen->CurMoveStep = 0;
            RebuildLookBorders = true;
        }
    }

    cr->MoveSteps.clear();
    cr->CurMoveStep = 0;
}

void FOClient::Net_OnAllProperties()
{
    WriteLog( "Chosen properties..." );

    uint msg_len = 0;
    Bin >> msg_len;
    NET_READ_PROPERTIES( Bin, TempPropertiesData );

    if( !Chosen )
    {
        WriteLog( "Chosen not created, skip.\n" );
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

void FOClient::Net_OnChosenClearItems()
{
    InitialItemsSend = true;

    if( !Chosen )
    {
        WriteLog( "Chosen is not created.\n" );
        return;
    }

    if( Chosen->IsHaveLightSources() )
        HexMngr.RebuildLight();
    Chosen->DeleteAllItems();
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
        WriteLog( "Chosen is not created.\n" );
        return;
    }

    Item* prev_item = Chosen->GetItem( item_id );
    uchar prev_slot = 0;
    uint  prev_light_hash = 0;
    if( prev_item )
    {
        prev_slot = prev_item->GetCritSlot();
        prev_light_hash = prev_item->LightGetHash();
        Chosen->DeleteItem( prev_item, false );
    }

    ProtoItem* proto_item = ProtoMngr.GetProtoItem( pid );
    RUNTIME_ASSERT( proto_item );

    Item* item = new Item( item_id, proto_item );
    item->Props.RestoreData( TempPropertiesData );
    item->SetAccessory( ITEM_ACCESSORY_CRITTER );
    item->SetCritId( Chosen->GetId() );
    item->SetCritSlot( slot );
    RUNTIME_ASSERT( !item->GetIsHidden() );

    item->AddRef();

    Chosen->AddItem( item );

    RebuildLookBorders = true;
    if( item->LightGetHash() != prev_light_hash && ( slot || prev_slot ) )
        HexMngr.RebuildLight();

    if( !InitialItemsSend )
        Script::RaiseInternalEvent( ClientFunctions.ItemInvIn, item );

    item->Release();
}

void FOClient::Net_OnChosenEraseItem()
{
    uint item_id;
    Bin >> item_id;

    CHECK_IN_BUFF_ERROR;

    if( !Chosen )
    {
        WriteLog( "Chosen is not created.\n" );
        return;
    }

    Item* item = Chosen->GetItem( item_id );
    if( !item )
    {
        WriteLog( "Item not found, id {}.\n", item_id );
        return;
    }

    Item* clone = item->Clone();
    bool  rebuild_light = ( item->GetIsLight() && item->GetCritSlot() );
    Chosen->DeleteItem( item, true );
    if( rebuild_light )
        HexMngr.RebuildLight();
    Script::RaiseInternalEvent( ClientFunctions.ItemInvOut, clone );
    clone->Release();
}

void FOClient::Net_OnAllItemsSend()
{
    InitialItemsSend = false;

    if( !Chosen )
    {
        WriteLog( "Chosen is not created.\n" );
        return;
    }

    if( Chosen->Anim3d )
        Chosen->Anim3d->StartMeshGeneration();

    Script::RaiseInternalEvent( ClientFunctions.ItemInvAllIn );
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
        HexMngr.AddItem( item_id, item_pid, item_hx, item_hy, is_added, &TempPropertiesData );

    Item* item = HexMngr.GetItemById( item_id );
    if( item )
        Script::RaiseInternalEvent( ClientFunctions.ItemMapIn, item );

    // Refresh borders
    if( item && !item->GetIsShootThru() )
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
    if( item )
        Script::RaiseInternalEvent( ClientFunctions.ItemMapOut, item );

    // Refresh borders
    if( item && !item->GetIsShootThru() )
        RebuildLookBorders = true;

    HexMngr.FinishItem( item_id, is_deleted );
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
        Bin.Pop( &data_vec[ 0 ], data_count * sizeof( uint ) );
    }

    CHECK_IN_BUFF_ERROR;

    CScriptArray* arr = Script::CreateArray( "uint[]" );
    arr->Resize( data_count );
    for( uint i = 0; i < data_count; i++ )
        *( (uint*) arr->At( i ) ) = data_vec[ i ];

    Script::RaiseInternalEvent( ClientFunctions.CombatResult, arr );

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
    int    maxhx = HexMngr.GetWidth();
    int    maxhy = HexMngr.GetHeight();

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
        WriteLog( "Run effect '{}' fail.\n", _str().parseHash( eff_pid ) );
        return;
    }
}

void FOClient::Net_OnPlaySound()
{
    uint   msg_len;
    uint   synchronize_crid;
    string sound_name;
    Bin >> msg_len;
    Bin >> synchronize_crid;
    Bin >> sound_name;

    CHECK_IN_BUFF_ERROR;

    SndMngr.PlaySound( sound_name );
}

void FOClient::Net_OnPing()
{
    uchar ping;
    Bin >> ping;

    CHECK_IN_BUFF_ERROR;

    if( ping == PING_WAIT )
    {
        GameOpt.WaitPing = false;
    }
    else if( ping == PING_CLIENT )
    {
        Bout << NETMSG_PING;
        Bout << (uchar) PING_CLIENT;
    }
    else if( ping == PING_PING )
    {
        GameOpt.Ping = Timer::FastTick() - PingTick;
        PingTick = 0;
        PingCallTick = Timer::FastTick() + GameOpt.PingPeriod;
    }
}

void FOClient::Net_OnEndParseToGame()
{
    if( !Chosen )
    {
        WriteLog( "Chosen is not created.\n" );
        return;
    }

    FlashGameWindow();
    ScreenFadeOut();

    if( CurMapPid )
    {
        HexMngr.SkipItemsFade();
        HexMngr.FindSetCenter( Chosen->GetHexX(), Chosen->GetHexY() );
        Chosen->AnimateStay();
        ShowMainScreen( SCREEN_GAME );
        HexMngr.RebuildLight();
    }
    else
    {
        ShowMainScreen( SCREEN_GLOBAL_MAP );
    }

    WriteLog( "Entering to game complete.\n" );
}

void FOClient::Net_OnProperty( uint data_size )
{
    uint              msg_len = 0;
    NetProperty::Type type = NetProperty::None;
    uint              cr_id = 0;
    uint              item_id = 0;
    ushort            property_index = 0;

    if( data_size == 0 )
        Bin >> msg_len;

    char type_ = 0;
    Bin >> type_;
    type = (NetProperty::Type) type_;

    uint additional_args = 0;
    switch( type )
    {
    case NetProperty::CritterItem:
        additional_args = 2;
        Bin >> cr_id;
        Bin >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        Bin >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        Bin >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        Bin >> item_id;
        break;
    default:
        break;
    }

    Bin >> property_index;

    if( data_size != 0 )
    {
        TempPropertyData.resize( data_size );
        Bin.Pop( &TempPropertyData[ 0 ], data_size );
    }
    else
    {
        uint len = msg_len - sizeof( uint ) - sizeof( msg_len ) - sizeof( char ) - additional_args * sizeof( uint ) - sizeof( ushort );
        TempPropertyData.resize( len );
        if( len )
            Bin.Pop( &TempPropertyData[ 0 ], len );
    }

    CHECK_IN_BUFF_ERROR;

    Property* prop = nullptr;
    Entity*   entity = nullptr;
    switch( type )
    {
    case NetProperty::Global:
        prop = GlobalVars::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = Globals;
        break;
    case NetProperty::Critter:
        prop = CritterCl::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = GetCritter( cr_id );
        break;
    case NetProperty::Chosen:
        prop = CritterCl::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = Chosen;
        break;
    case NetProperty::MapItem:
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = HexMngr.GetItemById( item_id );
        break;
    case NetProperty::CritterItem:
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
        {
            CritterCl* cr = GetCritter( cr_id );
            if( cr )
                entity = cr->GetItem( item_id );
        }
        break;
    case NetProperty::ChosenItem:
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = ( Chosen ? Chosen->GetItem( item_id ) : nullptr );
        break;
    case NetProperty::Map:
        prop = Map::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = ScriptFunc.ClientCurMap;
        break;
    case NetProperty::Location:
        prop = Location::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = ScriptFunc.ClientCurLocation;
        break;
    default:
        RUNTIME_ASSERT( false );
        break;
    }
    if( !prop || !entity )
        return;

    entity->Props.SetSendIgnore( prop, entity );
    prop->SetData( entity, !TempPropertyData.empty() ? &TempPropertyData[ 0 ] : nullptr, (uint) TempPropertyData.size() );
    entity->Props.SetSendIgnore( nullptr, nullptr );

    if( type == NetProperty::MapItem )
        Script::RaiseInternalEvent( ClientFunctions.ItemMapChanged, entity, entity );

    if( type == NetProperty::ChosenItem )
    {
        Item* item = (Item*) entity;
        item->AddRef();
        OnItemInvChanged( item, item );
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

    if( !count_answ )
    {
        // End dialog
        if( IsScreenPresent( SCREEN__DIALOG ) )
            HideScreen( SCREEN__DIALOG );
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
    CritterCl* npc = ( is_npc ? GetCritter( talk_id ) : nullptr );

    // Main text
    Bin >> text_id;

    // Answers
    UIntVec answers_texts;
    uint    answ_text_id;
    for( int i = 0; i < count_answ; i++ )
    {
        Bin >> answ_text_id;
        answers_texts.push_back( answ_text_id );
    }

    string        str = CurLang.Msg[ TEXTMSG_DLG ].GetStr( text_id );
    FormatTags( str, Chosen, npc, lexems );
    string        text_to_script = str;
    CScriptArray* answers_to_script = Script::CreateArray( "string[]" );
    for( uint answers_text : answers_texts )
    {
        str = CurLang.Msg[ TEXTMSG_DLG ].GetStr( answers_text );
        FormatTags( str, Chosen, npc, lexems );
        string sstr = str;
        answers_to_script->InsertLast( &sstr );
    }

    Bin >> talk_time;

    CHECK_IN_BUFF_ERROR;

    CScriptDictionary* dict = CScriptDictionary::Create( Script::GetEngine() );
    dict->Set( "TalkerIsNpc", &is_npc, asTYPEID_BOOL );
    dict->Set( "TalkerId", &talk_id, asTYPEID_UINT32 );
    dict->Set( "Text", &text_to_script, Script::GetEngine()->GetTypeIdByDecl( "string" ) );
    dict->Set( "Answers", &answers_to_script, Script::GetEngine()->GetTypeIdByDecl( "string[]@" ) );
    dict->Set( "TalkTime", &talk_time, asTYPEID_UINT32 );
    ShowScreen( SCREEN__DIALOG, dict );
    answers_to_script->Release();
    dict->Release();
}

void FOClient::Net_OnGameInfo()
{
    int    time;
    uchar  rain;
    bool   no_log_out;
    int*   day_time = HexMngr.GetMapDayTime();
    uchar* day_color = HexMngr.GetMapDayColor();
    ushort time_var;
    Bin >> time_var;
    Globals->SetYearStart( time_var );
    Bin >> time_var;
    Globals->SetYear( time_var );
    Bin >> time_var;
    Globals->SetMonth( time_var );
    Bin >> time_var;
    Globals->SetDay( time_var );
    Bin >> time_var;
    Globals->SetHour( time_var );
    Bin >> time_var;
    Globals->SetMinute( time_var );
    Bin >> time_var;
    Globals->SetSecond( time_var );
    Bin >> time_var;
    Globals->SetTimeMultiplier( time_var );
    Bin >> time;
    Bin >> rain;
    Bin >> no_log_out;
    Bin.Pop( day_time, sizeof( int ) * 4 );
    Bin.Pop( day_color, sizeof( uchar ) * 12 );

    CHECK_IN_BUFF_ERROR;

    Timer::InitGameTime();
    GameOpt.GameTimeTick = Timer::GameTick();

    HexMngr.SetWeather( time, rain );
    SetDayTime( true );
    NoLogOut = no_log_out;
}

void FOClient::Net_OnLoadMap()
{
    WriteLog( "Change map...\n" );

    uint  msg_len;
    hash  map_pid;
    hash  loc_pid;
    uchar map_index_in_loc;
    int   map_time;
    uchar map_rain;
    hash  hash_tiles;
    hash  hash_scen;
    Bin >> msg_len;
    Bin >> map_pid;
    Bin >> loc_pid;
    Bin >> map_index_in_loc;
    Bin >> map_time;
    Bin >> map_rain;
    Bin >> hash_tiles;
    Bin >> hash_scen;
    if( map_pid )
    {
        NET_READ_PROPERTIES( Bin, TempPropertiesData );
        NET_READ_PROPERTIES( Bin, TempPropertiesDataExt );
    }

    CHECK_IN_BUFF_ERROR;

    if( ScriptFunc.ClientCurMap )
        ScriptFunc.ClientCurMap->IsDestroyed = true;
    if( ScriptFunc.ClientCurLocation )
        ScriptFunc.ClientCurLocation->IsDestroyed = true;
    SAFEREL( ScriptFunc.ClientCurMap );
    SAFEREL( ScriptFunc.ClientCurLocation );
    if( map_pid )
    {
        ScriptFunc.ClientCurLocation = new Location( 0, ProtoMngr.GetProtoLocation( loc_pid ) );
        ScriptFunc.ClientCurLocation->Props.RestoreData( TempPropertiesDataExt );
        ScriptFunc.ClientCurMap = new Map( 0, ProtoMngr.GetProtoMap( map_pid ) );
        ScriptFunc.ClientCurMap->Props.RestoreData( TempPropertiesData );
    }

    GameOpt.SpritesZoom = 1.0f;

    CurMapPid = map_pid;
    CurMapLocPid = loc_pid;
    CurMapIndexInLoc = map_index_in_loc;
    GameMapTexts.clear();
    HexMngr.UnloadMap();
    SndMngr.StopSounds();
    ShowMainScreen( SCREEN_WAIT );
    DeleteCritters();
    ResMngr.ReinitializeDynamicAtlas();

    // Global
    if( !map_pid )
    {
        GmapNullParams();
        WriteLog( "Global map loaded.\n" );
    }
    // Local
    else
    {
        hash hash_tiles_cl = 0;
        hash hash_scen_cl = 0;
        HexMngr.GetMapHash( map_pid, hash_tiles_cl, hash_scen_cl );

        if( hash_tiles != hash_tiles_cl || hash_scen != hash_scen_cl )
        {
            WriteLog( "Obsolete map data (hashes {}:{}, {}:{}).\n", hash_tiles, hash_tiles_cl, hash_scen, hash_scen_cl );
            Net_SendGiveMap( false, map_pid, 0, hash_tiles_cl, hash_scen_cl );
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
    bool   send_tiles;
    bool   send_scenery;
    Bin >> msg_len;
    Bin >> map_pid;
    Bin >> maxhx;
    Bin >> maxhy;
    Bin >> send_tiles;
    Bin >> send_scenery;

    CHECK_IN_BUFF_ERROR;

    WriteLog( "Map {} received...\n", map_pid );

    string map_name = _str( "{}.map", map_pid );
    bool   tiles = false;
    char*  tiles_data = nullptr;
    uint   tiles_len = 0;
    bool   scen = false;
    char*  scen_data = nullptr;
    uint   scen_len = 0;

    if( send_tiles )
    {
        Bin >> tiles_len;
        if( tiles_len )
        {
            tiles_data = new char[ tiles_len ];
            Bin.Pop( tiles_data, tiles_len );
        }
        tiles = true;
    }

    if( send_scenery )
    {
        Bin >> scen_len;
        if( scen_len )
        {
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
                fm.GetBEUInt();
                fm.GetBEUShort();
                fm.GetBEUShort();
                uint old_tiles_len = fm.GetBEUInt();
                uint old_scen_len = fm.GetBEUInt();

                if( !tiles )
                {
                    tiles_len = old_tiles_len;
                    tiles_data = new char[ tiles_len ];
                    fm.CopyMem( tiles_data, tiles_len );
                    tiles = true;
                }

                if( !scen )
                {
                    scen_len = old_scen_len;
                    scen_data = new char[ scen_len ];
                    fm.CopyMem( scen_data, scen_len );
                    scen = true;
                }

                fm.UnloadFile();
            }
        }
    }
    SAFEDELA( cache );

    if( tiles && scen )
    {
        fm.UnloadFile();
        fm.SetBEUInt( CLIENT_MAP_FORMAT_VER );
        fm.SetBEUInt( map_pid );
        fm.SetBEUShort( maxhx );
        fm.SetBEUShort( maxhy );
        fm.SetBEUInt( tiles_len );
        fm.SetBEUInt( scen_len );
        if( tiles_len )
            fm.SetData( tiles_data, tiles_len );
        if( scen_len )
            fm.SetData( scen_data, scen_len );

        uint   obuf_len = fm.GetOutBufLen();
        uchar* buf = Crypt.Compress( fm.GetOutBuf(), obuf_len );
        if( !buf )
        {
            WriteLog( "Failed to compress data '{}', disconnect.\n", map_name );
            NetDisconnect();
            return;
        }

        fm.UnloadFile();
        fm.SetData( buf, obuf_len );
        delete[] buf;

        Crypt.SetCache( map_name, fm.GetOutBuf(), fm.GetOutBufLen() );
    }
    else
    {
        WriteLog( "Not for all data of map, disconnect.\n" );
        NetDisconnect();
        SAFEDELA( tiles_data );
        SAFEDELA( scen_data );
        return;
    }

    SAFEDELA( tiles_data );
    SAFEDELA( scen_data );

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

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
    {
        Bin.Pop( GmapFog->GetData(), GM_ZONES_FOG_SIZE );
    }

    if( FLAG( info_flags, GM_INFO_FOG ) )
    {
        ushort zx, zy;
        uchar  fog;
        Bin >> zx;
        Bin >> zy;
        Bin >> fog;
        GmapFog->Set2Bit( zx, zy, fog );
    }

    if( FLAG( info_flags, GM_INFO_CRITTERS ) )
    {
        DeleteCritters();
        // After wait AddCritters
    }

    CHECK_IN_BUFF_ERROR;
}

void FOClient::Net_OnSomeItems()
{
    uint msg_len;
    int  param;
    bool is_null;
    uint items_count;
    Bin >> msg_len;
    Bin >> param;
    Bin >> is_null;
    Bin >> items_count;

    ItemVec item_container;
    for( uint i = 0; i < items_count; i++ )
    {
        uint item_id;
        hash item_pid;
        Bin >> item_id;
        Bin >> item_pid;
        NET_READ_PROPERTIES( Bin, TempPropertiesData );

        ProtoItem* proto_item = ProtoMngr.GetProtoItem( item_pid );
        if( item_id && proto_item )
        {
            Item* item = new Item( item_id, proto_item );
            item->Props.RestoreData( TempPropertiesData );
            item_container.push_back( item );
        }
    }

    CHECK_IN_BUFF_ERROR;

    CScriptArray* items_arr = nullptr;
    if( !is_null )
    {
        items_arr = Script::CreateArray( "Item[]" );
        Script::AppendVectorToArray( item_container, items_arr );
    }

    Script::RaiseInternalEvent( ClientFunctions.ReceiveItems, items_arr, param );

    if( items_arr )
        items_arr->Release();
}

void FOClient::Net_OnUpdateFilesList()
{
    uint     msg_len;
    bool     outdated;
    uint     data_size;
    UCharVec data;
    Bin >> msg_len;
    Bin >> outdated;
    Bin >> data_size;
    data.resize( data_size );
    if( data_size )
        Bin.Pop( &data[ 0 ], (uint) data.size() );
    if( !outdated )
        NET_READ_PROPERTIES( Bin, GlovalVarsPropertiesData );

    CHECK_IN_BUFF_ERROR;

    FileManager fm;
    fm.LoadStream( &data[ 0 ], (uint) data.size() );

    SAFEDEL( UpdateFilesList );
    UpdateFilesList = new UpdateFileVec();
    UpdateFilesWholeSize = 0;
    UpdateFilesClientOutdated = outdated;

    #ifdef FO_WINDOWS
    bool   have_exe = false;
    string exe_name;
    if( outdated )
        exe_name = _str( FileManager::GetExePath() ).extractFileName();
    #endif

    for( uint file_index = 0; ; file_index++ )
    {
        short name_len = fm.GetLEShort();
        if( name_len == -1 )
            break;

        RUNTIME_ASSERT( name_len > 0 );
        string name = string( (const char*) fm.GetCurBuf(), name_len );
        fm.GoForward( name_len );
        uint   size = fm.GetLEUInt();
        uint   hash = fm.GetLEUInt();

        // Skip platform depended
        #ifdef FO_WINDOWS
        if( outdated && _str( exe_name ).compareIgnoreCase( name ) )
            have_exe = true;
        string ext = _str( name ).getFileExtension();
        if( !outdated && ( ext == "exe" || ext == "pdb" ) )
            continue;
        #else
        string ext = _str( name ).getFileExtension();
        if( ext == "exe" || ext == "pdb" )
            continue;
        #endif

        // Check hash
        uint  cur_hash_len;
        uint* cur_hash = (uint*) Crypt.GetCache( _str( "{}.hash", name ), cur_hash_len );
        bool  cached_hash_same = ( cur_hash && cur_hash_len == sizeof( hash ) && *cur_hash == hash );
        if( name[ 0 ] != '$' )
        {
            // Real file, human can disturb file consistency, make base recheck
            FileManager file;
            if( file.LoadFile( name, true ) && file.GetFsize() == size )
            {
                if( cached_hash_same || ( file.LoadFile( name ) && hash == Crypt.MurmurHash2( file.GetBuf(), file.GetFsize() ) ) )
                {
                    if( !cached_hash_same )
                        Crypt.SetCache( _str( "{}.hash", name ), (uchar*) &hash, sizeof( hash ) );
                    continue;
                }
            }
        }
        else
        {
            // In cache, consistency granted
            if( cached_hash_same )
                continue;
        }

        // Get this file
        UpdateFile update_file;
        update_file.Index = file_index;
        update_file.Name = name;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        UpdateFilesList->push_back( update_file );
        UpdateFilesWholeSize += size;
    }

    #ifdef FO_WINDOWS
    if( UpdateFilesClientOutdated && !have_exe )
        UpdateFilesAbort( STR_CLIENT_OUTDATED, "Client outdated!" );
    #else
    if( UpdateFilesClientOutdated )
        UpdateFilesAbort( STR_CLIENT_OUTDATED, "Client outdated!" );
    #endif
}

void FOClient::Net_OnUpdateFileData()
{
    // Get portion
    uchar data[ FILE_UPDATE_PORTION ];
    Bin.Pop( data, sizeof( data ) );

    CHECK_IN_BUFF_ERROR;

    UpdateFile& update_file = UpdateFilesList->front();

    // Write data to temp file
    if( !FileWrite( UpdateFileTemp, data, MIN( update_file.RemaningSize, sizeof( data ) ) ) )
    {
        UpdateFilesAbort( STR_FILESYSTEM_ERROR, "Can't write update file!" );
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
        UpdateFileTemp = nullptr;

        // Cache
        if( update_file.Name[ 0 ] == '$' )
        {
            void* temp_file = FileOpen( FileManager::GetWritePath( "Update.bin" ), false );
            if( !temp_file )
            {
                UpdateFilesAbort( STR_FILESYSTEM_ERROR, "Can't load update file!" );
                return;
            }

            uint     len = FileGetSize( temp_file );
            UCharVec buf( len );
            if( !FileRead( temp_file, &buf[ 0 ], len ) )
            {
                UpdateFilesAbort( STR_FILESYSTEM_ERROR, "Can't read update file!" );
                FileClose( temp_file );
                return;
            }
            FileClose( temp_file );

            Crypt.SetCache( update_file.Name, &buf[ 0 ], len );
            Crypt.SetCache( update_file.Name + ".hash", (uchar*) &update_file.Hash, sizeof( update_file.Hash ) );
            FileManager::DeleteFile( FileManager::GetWritePath( "Update.bin" ) );
        }
        // File
        else
        {
            string from_path = FileManager::GetWritePath( "Update.bin" );
            string to_path = FileManager::GetWritePath( update_file.Name );
            FileManager::DeleteFile( to_path );
            if( !FileManager::RenameFile( from_path, to_path ) )
            {
                UpdateFilesAbort( STR_FILESYSTEM_ERROR, _str( "Can't rename file '{}' to '{}'!", from_path, to_path ) );
                return;
            }
        }

        UpdateFilesList->erase( UpdateFilesList->begin() );
        UpdateFileDownloading = false;
    }
}

void FOClient::Net_OnAutomapsInfo()
{
    uint   msg_len;
    bool   clear;
    ushort locs_count;
    Bin >> msg_len;
    Bin >> clear;

    if( clear )
        Automaps.clear();

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
            amap.LocName = CurLang.Msg[ TEXTMSG_LOCATIONS ].GetStr( STR_LOC_NAME( loc_pid ) );

            for( ushort j = 0; j < maps_count; j++ )
            {
                hash  map_pid;
                uchar map_index_in_loc;
                Bin >> map_pid;
                Bin >> map_index_in_loc;

                amap.MapPids.push_back( map_pid );
                amap.MapNames.push_back( CurLang.Msg[ TEXTMSG_LOCATIONS ].GetStr( STR_LOC_MAP_NAME( loc_pid, map_index_in_loc ) ) );
            }

            if( it != Automaps.end() )
                *it = amap;
            else
                Automaps.push_back( amap );
        }
    }

    CHECK_IN_BUFF_ERROR;
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
    ShowMainScreen( SCREEN_GAME );
    ScreenFadeOut();
    HexMngr.RebuildLight();

    CScriptDictionary* dict = CScriptDictionary::Create( Script::GetEngine() );
    dict->Set( "LocationId", &loc_id, asTYPEID_UINT32 );
    dict->Set( "LocationEntrance", &loc_ent, asTYPEID_UINT32 );
    ShowScreen( SCREEN__TOWN_VIEW, dict );
    dict->Release();
}

void FOClient::SetGameColor( uint color )
{
    SprMngr.SetSpritesColor( color );
    if( HexMngr.IsMapLoaded() )
        HexMngr.RefreshMap();
}

void FOClient::SetDayTime( bool refresh )
{
    if( !HexMngr.IsMapLoaded() )
        return;

    static uint old_color = -1;
    uint        color = GetColorDay( HexMngr.GetMapDayTime(), HexMngr.GetMapDayColor(), HexMngr.GetMapTime(), nullptr );
    color = COLOR_GAME_RGB( ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF );

    if( refresh )
        old_color = -1;
    if( old_color != color )
    {
        old_color = color;
        SetGameColor( old_color );
    }
}

void FOClient::WaitPing()
{
    GameOpt.WaitPing = true;
    Net_SendPing( PING_WAIT );
}

void FOClient::CrittersProcess()
{
    for( auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end();)
    {
        CritterCl* cr = it->second;
        ++it;

        cr->Process();

        if( cr->IsNeedReSet() )
        {
            HexMngr.RemoveCritter( cr );
            HexMngr.SetCritter( cr );
            cr->ReSetOk();
        }

        if( !cr->IsChosen() && cr->IsNeedMove() && HexMngr.TransitCritter( cr, cr->MoveSteps[ 0 ].first, cr->MoveSteps[ 0 ].second, true, cr->CurMoveStep > 0 ) )
        {
            cr->MoveSteps.erase( cr->MoveSteps.begin() );
            cr->CurMoveStep--;
        }

        if( cr->IsFinish() )
            DeleteCritter( cr->GetId() );
    }
}

void FOClient::TryExit()
{
    int active = GetActiveScreen();
    if( active != SCREEN_NONE )
    {
        switch( active )
        {
        case SCREEN__TOWN_VIEW:
            Net_SendRefereshMe();
            break;
        default:
            HideScreen( SCREEN_NONE );
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
        case SCREEN_WAIT:
            NetDisconnect();
            break;
        default:
            break;
        }
    }
}

bool FOClient::IsCurInWindow()
{
    if( !SprMngr.IsWindowFocused() )
        return false;
    return true;
}

void FOClient::FlashGameWindow()
{
    if( SprMngr.IsWindowFocused() )
        return;

    SprMngr.BlinkWindow();

    #ifdef FO_WINDOWS
    if( GameOpt.SoundNotify )
        Beep( 100, 200 );
    #endif
}

string FOClient::FmtGameText( uint str_num, ... )
{
    string  str = _str( CurLang.Msg[ TEXTMSG_GAME ].GetStr( str_num ) ).replace( '\\', 'n', '\n' );
    char    res[ MAX_FOTEXT ];
    va_list list;
    va_start( list, str_num );
    vsprintf( res, str.c_str(), list );
    va_end( list );
    return res;
}

void FOClient::AddVideo( const char* video_name, bool can_stop, bool clear_sequence )
{
    // Stop current
    if( clear_sequence && ShowVideos.size() )
        StopVideo();

    // Show parameters
    ShowVideo sw;

    // Paths
    string str = video_name;
    size_t sound_pos = str.find( '|' );
    if( sound_pos != string::npos )
    {
        sw.SoundName += str.substr( sound_pos + 1 );
        str.erase( sound_pos );
    }
    sw.FileName += str;

    // Add video in sequence
    sw.CanStop = can_stop;
    ShowVideos.push_back( sw );

    // Instant play
    if( ShowVideos.size() == 1 )
    {
        // Clear screen
        SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) );
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
    if( !CurVideo->RawData.LoadFile( video.FileName.c_str() ) )
    {
        WriteLog( "Video file '{}' not found.\n", video.FileName );
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
    CurVideo->SetupInfo = nullptr;
    while( true )
    {
        int stream_index = VideoDecodePacket();
        if( stream_index < 0 )
        {
            WriteLog( "Decode header packet fail.\n" );
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
                        WriteLog( "Seek first data packet fail.\n" );
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
    CurVideo->RT = SprMngr.CreateRenderTarget( false, false, false, CurVideo->VideoInfo.pic_width, CurVideo->VideoInfo.pic_height, true );
    if( !CurVideo->RT )
    {
        WriteLog( "Can't create render target.\n" );
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
        SndMngr.PlayMusic( video.SoundName, 0 );
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
        int r = th_decode_packetin( CurVideo->Context, &CurVideo->Packet, nullptr );
        if( r != TH_DUPFRAME )
        {
            if( r )
            {
                WriteLog( "Frame does not contain encoded video data, error {}.\n", r );
                NextVideo();
                return;
            }

            // Decode color
            r = th_decode_ycbcr_out( CurVideo->Context, CurVideo->ColorBuffer );
            if( r )
            {
                WriteLog( "th_decode_ycbcr_out() fail, error {}\n.", r );
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
        WriteLog( "Wrong pixel format.\n" );
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
    int  x = ( GameOpt.ScreenWidth - w ) / 2;
    int  y = ( GameOpt.ScreenHeight - h ) / 2;
    SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) );
    Rect r = Rect( x, y, x + w, y + h );
    SprMngr.DrawRenderTarget( CurVideo->RT, false, nullptr, &r );
    SprMngr.EndScene();

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
        SprMngr.BeginScene( COLOR_RGB( 0, 0, 0 ) );
        SprMngr.EndScene();

        // Stop current
        StopVideo();
        ShowVideos.erase( ShowVideos.begin() );

        // Manage next
        if( ShowVideos.size() )
            PlayVideo();
        else
            ScreenFadeOut();
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

uint FOClient::AnimLoad( const char* fname, int res_type )
{
    AnyFrames* anim = ResMngr.GetAnim( _str( fname ).toHash(), res_type );
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

void FOClient::OnSendGlobalValue( Entity* entity, Property* prop )
{
    if( prop->GetAccess() == Property::PublicFullModifiable )
        Self->Net_SendProperty( NetProperty::Global, prop, Globals );
    else
        SCRIPT_ERROR_R( "Unable to send global modifiable property '{}'", prop->GetName() );
}

void FOClient::OnSendCritterValue( Entity* entity, Property* prop )
{
    CritterCl* cr = (CritterCl*) entity;
    if( cr->IsChosen() )
        Self->Net_SendProperty( NetProperty::Chosen, prop, cr );
    else if( prop->GetAccess() == Property::PublicFullModifiable )
        Self->Net_SendProperty( NetProperty::Critter, prop, cr );
    else
        SCRIPT_ERROR_R( "Unable to send critter modifiable property '{}'", prop->GetName() );
}

void FOClient::OnSetCritterModelName( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    CritterCl* cr = (CritterCl*) entity;
    cr->RefreshAnim();
    cr->Action( ACTION_REFRESH, 0, nullptr );
}

void FOClient::OnSendItemValue( Entity* entity, Property* prop )
{
    Item* item = (Item*) entity;
    #pragma MESSAGE( "Clean up client 0 and -1 item ids" )
    if( item->Id && item->Id != uint( -1 ) )
    {
        if( item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
        {
            CritterCl* cr = Self->GetCritter( item->GetCritId() );
            if( cr && cr->IsChosen() )
                Self->Net_SendProperty( NetProperty::ChosenItem, prop, item );
            else if( cr && prop->GetAccess() == Property::PublicFullModifiable )
                Self->Net_SendProperty( NetProperty::CritterItem, prop, item );
            else
                SCRIPT_ERROR_R( "Unable to send item (a critter) modifiable property '{}'", prop->GetName() );
        }
        else if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
        {
            if( prop->GetAccess() == Property::PublicFullModifiable )
                Self->Net_SendProperty( NetProperty::MapItem, prop, item );
            else
                SCRIPT_ERROR_R( "Unable to send item (a map) modifiable property '{}'", prop->GetName() );
        }
        else
        {
            SCRIPT_ERROR_R( "Unable to send item (a container) modifiable property '{}'", prop->GetName() );
        }
    }
}

void FOClient::OnSetItemFlags( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    // IsColorize, IsBadItem, IsShootThru, IsLightThru, IsNoBlock

    Item* item = (Item*) entity;
    if( item->GetAccessory() == ITEM_ACCESSORY_HEX && Self->HexMngr.IsMapLoaded() )
    {
        ItemHex* hex_item = (ItemHex*) item;
        bool     rebuild_cache = false;
        if( prop == Item::PropertyIsColorize )
            hex_item->RefreshAlpha();
        else if( prop == Item::PropertyIsBadItem )
            hex_item->SetSprite( nullptr );
        else if( prop == Item::PropertyIsShootThru )
            Self->RebuildLookBorders = true, rebuild_cache = true;
        else if( prop == Item::PropertyIsLightThru )
            Self->HexMngr.RebuildLight(), rebuild_cache = true;
        else if( prop == Item::PropertyIsNoBlock )
            rebuild_cache = true;
        if( rebuild_cache )
            Self->HexMngr.GetField( hex_item->GetHexX(), hex_item->GetHexY() ).ProcessCache();
    }
}

void FOClient::OnSetItemSomeLight( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    if( Self->HexMngr.IsMapLoaded() )
        Self->HexMngr.RebuildLight();
}

void FOClient::OnSetItemPicMap( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) entity;

    if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
    {
        ItemHex* hex_item = (ItemHex*) item;
        hex_item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetXY( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    // OffsetX, OffsetY

    Item* item = (Item*) entity;

    if( item->GetAccessory() == ITEM_ACCESSORY_HEX && Self->HexMngr.IsMapLoaded() )
    {
        ItemHex* hex_item = (ItemHex*) item;
        hex_item->SetAnimOffs();
        Self->HexMngr.ProcessHexBorders( hex_item );
    }
}

void FOClient::OnSetItemOpened( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) entity;
    bool  cur = *(bool*) cur_value;
    bool  old = *(bool*) old_value;

    if( item->GetIsCanOpen() )
    {
        ItemHex* hex_item = (ItemHex*) item;
        if( !old && cur )
            hex_item->SetAnimFromStart();
        if( old && !cur )
            hex_item->SetAnimFromEnd();
    }
}

void FOClient::OnSendMapValue( Entity* entity, Property* prop )
{
    if( prop->GetAccess() == Property::PublicFullModifiable )
        Self->Net_SendProperty( NetProperty::Map, prop, Globals );
    else
        SCRIPT_ERROR_R( "Unable to send map modifiable property '{}'", prop->GetName() );
}

void FOClient::OnSendLocationValue( Entity* entity, Property* prop )
{
    if( prop->GetAccess() == Property::PublicFullModifiable )
        Self->Net_SendProperty( NetProperty::Location, prop, Globals );
    else
        SCRIPT_ERROR_R( "Unable to send location modifiable property '{}'", prop->GetName() );
}

/************************************************************************/
/* Scripts                                                              */
/************************************************************************/

namespace ClientBind
{
    #undef BIND_SERVER
    #undef BIND_CLIENT
    #undef BIND_MAPPER
    #undef BIND_CLASS
    #undef BIND_ASSERT
    #undef BIND_DUMMY_DATA
    #define BIND_CLIENT
    #define BIND_CLASS        FOClient::SScriptFunc::
    #define BIND_CLASS_EXT    FOClient::Self->
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line {}.\n", __LINE__ ); errors++; }
    #include "ScriptBind.h"
}

Map*      FOClient::SScriptFunc::ClientCurMap;
Location* FOClient::SScriptFunc::ClientCurLocation;

bool FOClient::ReloadScripts()
{
    WriteLog( "Load scripts...\n" );

    FOMsg& msg_script = CurLang.Msg[ TEXTMSG_INTERNAL ];
    if( !msg_script.Count( STR_INTERNAL_SCRIPT_MODULE ) || !msg_script.Count( STR_INTERNAL_SCRIPT_MODULE + 1 ) )
    {
        WriteLog( "Main script section not found in MSG.\n" );
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    #ifdef MEMORY_DEBUG
    asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
    #endif

    // Finish previous
    Script::Finish();

    // Reinitialize engine
    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( PRAGMA_CLIENT );
    if( !Script::Init( pragma_callback, "CLIENT", true, 0, 0, false ) )
    {
        WriteLog( "Unable to start script engine.\n" );
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }
    Script::SetExceptionCallback([ this ] ( const string &str )
                                 {
                                     ShowMessage( str );
                                     DoRestart = true;
                                 } );

    // Bind stuff
    asIScriptEngine*      engine = Script::GetEngine();
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();
    int                   bind_errors = ClientBind::Bind( engine, registrators );
    if( bind_errors )
    {
        WriteLog( "Bind fail, errors {}.\n", bind_errors );
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_FAIL_RUN_START_SCRIPT ) );
        return false;
    }

    // Options
    Script::Undef( "" );
    Script::Define( "__CLIENT" );
    Script::Define( _str( "__VERSION {}", FONLINE_VERSION ) );

    // Store dlls
    for( int i = STR_INTERNAL_SCRIPT_DLLS; ; i += 2 )
    {
        if( !msg_script.Count( i ) )
            break;
        RUNTIME_ASSERT( msg_script.Count( i + 1 ) );

        string   dll_name = msg_script.GetStr( i );
        UCharVec dll_binary;
        if( !msg_script.GetBinary( i + 1, dll_binary ) )
            break;

        // Save to cache
        FileManager dll;
        if( dll.LoadStream( &dll_binary[ 0 ], (uint) dll_binary.size() ) )
        {
            dll.SwitchToWrite();
            dll.SaveFile( _str( dll_name ).normalizePathSlashes().replace( '/', '.' ) );
        }
    }

    // Pragmas
    Pragmas pragmas;
    for( int i = STR_INTERNAL_SCRIPT_PRAGMAS; ; i += 3 )
    {
        if( !msg_script.Count( i ) )
            break;
        RUNTIME_ASSERT( msg_script.Count( i + 1 ) );
        RUNTIME_ASSERT( msg_script.Count( i + 2 ) );

        Preprocessor::PragmaInstance pragma;
        pragma.Name = msg_script.GetStr( i );
        pragma.Text = msg_script.GetStr( i + 1 );
        pragma.CurrentFile = msg_script.GetStr( i + 2 );
        pragmas.push_back( pragma );
    }
    Script::CallPragmas( pragmas );

    // Load module
    int      errors = 0;

    UCharVec bytecode;
    msg_script.GetBinary( STR_INTERNAL_SCRIPT_MODULE, bytecode );
    RUNTIME_ASSERT( !bytecode.empty() );
    UCharVec lnt_data;
    msg_script.GetBinary( STR_INTERNAL_SCRIPT_MODULE + 1, lnt_data );
    RUNTIME_ASSERT( !lnt_data.empty() );
    if( Script::RestoreRootModule( bytecode, lnt_data ) )
    {
        if( Script::PostInitScriptSystem() )
        {
            Script::CacheEnumValues();
        }
        else
        {
            WriteLog( "Init client script fail.\n" );
            errors++;
        }
    }
    else
    {
        WriteLog( "Load client script fail.\n" );
        errors++;
    }

    #define BIND_INTERNAL_EVENT( name )    ClientFunctions.name = Script::FindInternalEvent( "Event" # name )
    BIND_INTERNAL_EVENT( Start );
    BIND_INTERNAL_EVENT( Finish );
    BIND_INTERNAL_EVENT( Loop );
    BIND_INTERNAL_EVENT( GetActiveScreens );
    BIND_INTERNAL_EVENT( AutoLogin );
    BIND_INTERNAL_EVENT( ScreenChange );
    BIND_INTERNAL_EVENT( ScreenScroll );
    BIND_INTERNAL_EVENT( RenderIface );
    BIND_INTERNAL_EVENT( RenderMap );
    BIND_INTERNAL_EVENT( MouseDown );
    BIND_INTERNAL_EVENT( MouseUp );
    BIND_INTERNAL_EVENT( MouseMove );
    BIND_INTERNAL_EVENT( KeyDown );
    BIND_INTERNAL_EVENT( KeyUp );
    BIND_INTERNAL_EVENT( InputLost );
    BIND_INTERNAL_EVENT( CritterIn );
    BIND_INTERNAL_EVENT( CritterOut );
    BIND_INTERNAL_EVENT( ItemMapIn );
    BIND_INTERNAL_EVENT( ItemMapChanged );
    BIND_INTERNAL_EVENT( ItemMapOut );
    BIND_INTERNAL_EVENT( ItemInvAllIn );
    BIND_INTERNAL_EVENT( ItemInvIn );
    BIND_INTERNAL_EVENT( ItemInvChanged );
    BIND_INTERNAL_EVENT( ItemInvOut );
    BIND_INTERNAL_EVENT( MapLoad );
    BIND_INTERNAL_EVENT( MapUnload );
    BIND_INTERNAL_EVENT( ReceiveItems );
    BIND_INTERNAL_EVENT( MapMessage );
    BIND_INTERNAL_EVENT( InMessage );
    BIND_INTERNAL_EVENT( OutMessage );
    BIND_INTERNAL_EVENT( MessageBox );
    BIND_INTERNAL_EVENT( CombatResult );
    BIND_INTERNAL_EVENT( ItemCheckMove );
    BIND_INTERNAL_EVENT( CritterAction );
    BIND_INTERNAL_EVENT( Animation2dProcess );
    BIND_INTERNAL_EVENT( Animation3dProcess );
    BIND_INTERNAL_EVENT( CritterAnimation );
    BIND_INTERNAL_EVENT( CritterAnimationSubstitute );
    BIND_INTERNAL_EVENT( CritterAnimationFallout );
    BIND_INTERNAL_EVENT( CritterCheckMoveItem );
    BIND_INTERNAL_EVENT( CritterGetAttackDistantion );
    #undef BIND_INTERNAL_EVENT

    if( errors )
        return false;

    #ifdef MEMORY_DEBUG
    ASDbgMemoryCanWork = true;
    #endif

    GlobalVars::SetPropertyRegistrator( registrators[ 0 ] );
    GlobalVars::PropertiesRegistrator->SetNativeSendCallback( OnSendGlobalValue );
    Globals = new GlobalVars();
    CritterCl::SetPropertyRegistrator( registrators[ 1 ] );
    CritterCl::PropertiesRegistrator->SetNativeSendCallback( OnSendCritterValue );
    CritterCl::PropertiesRegistrator->SetNativeSetCallback( "ModelName", OnSetCritterModelName );
    Item::SetPropertyRegistrator( registrators[ 2 ] );
    Item::PropertiesRegistrator->SetNativeSendCallback( OnSendItemValue );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsColorize", OnSetItemFlags );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsBadItem", OnSetItemFlags );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsShootThru", OnSetItemFlags );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsLightThru", OnSetItemFlags );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsNoBlock", OnSetItemFlags );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsLight", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightIntensity", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightDistance", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightFlags", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "LightColor", OnSetItemSomeLight );
    Item::PropertiesRegistrator->SetNativeSetCallback( "PicMap", OnSetItemPicMap );
    Item::PropertiesRegistrator->SetNativeSetCallback( "OffsetX", OnSetItemOffsetXY );
    Item::PropertiesRegistrator->SetNativeSetCallback( "OffsetY", OnSetItemOffsetXY );
    Item::PropertiesRegistrator->SetNativeSetCallback( "Opened", OnSetItemOpened );
    Map::SetPropertyRegistrator( registrators[ 3 ] );
    Map::PropertiesRegistrator->SetNativeSendCallback( OnSendMapValue );
    Location::SetPropertyRegistrator( registrators[ 4 ] );
    Location::PropertiesRegistrator->SetNativeSendCallback( OnSendLocationValue );

    Globals->Props.RestoreData( GlovalVarsPropertiesData );

    WriteLog( "Load scripts complete.\n" );
    return true;
}

void FOClient::OnItemInvChanged( Item* old_item, Item* item )
{
    Script::RaiseInternalEvent( ClientFunctions.ItemInvChanged, item, old_item );
    old_item->Release();
}

static int SortCritterHx_ = 0;
static int SortCritterHy_ = 0;
static bool SortCritterByDistPred( CritterCl* cr1, CritterCl* cr2 )
{
    return DistGame( SortCritterHx_, SortCritterHy_, cr1->GetHexX(), cr1->GetHexY() ) < DistGame( SortCritterHx_, SortCritterHy_, cr2->GetHexX(), cr2->GetHexY() );
}

static void SortCritterByDist( CritterCl* cr, CritVec& critters )
{
    SortCritterHx_ = cr->GetHexX();
    SortCritterHy_ = cr->GetHexY();
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}

static void SortCritterByDist( int hx, int hy, CritVec& critters )
{
    SortCritterHx_ = hx;
    SortCritterHy_ = hy;
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}

bool FOClient::SScriptFunc::Crit_IsChosen( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsChosen();
}

bool FOClient::SScriptFunc::Crit_IsPlayer( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsPlayer();
}

bool FOClient::SScriptFunc::Crit_IsNpc( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsNpc();
}

bool FOClient::SScriptFunc::Crit_IsOffline( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsOffline();
}

bool FOClient::SScriptFunc::Crit_IsLife( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsLife();
}

bool FOClient::SScriptFunc::Crit_IsKnockout( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsKnockout();
}

bool FOClient::SScriptFunc::Crit_IsDead( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsDead();
}

bool FOClient::SScriptFunc::Crit_IsFree( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsFree();
}

bool FOClient::SScriptFunc::Crit_IsBusy( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return !cr->IsFree();
}

bool FOClient::SScriptFunc::Crit_IsAnim3d( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->Anim3d != nullptr;
}

bool FOClient::SScriptFunc::Crit_IsAnimAviable( CritterCl* cr, uint anim1, uint anim2 )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsAnimAviable( anim1, anim2 );
}

bool FOClient::SScriptFunc::Crit_IsAnimPlaying( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsAnim();
}

uint FOClient::SScriptFunc::Crit_GetAnim1( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetAnim1();
}

void FOClient::SScriptFunc::Crit_Animate( CritterCl* cr, uint anim1, uint anim2 )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->Animate( anim1, anim2, nullptr );
}

void FOClient::SScriptFunc::Crit_AnimateEx( CritterCl* cr, uint anim1, uint anim2, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->Animate( anim1, anim2, item );
}

void FOClient::SScriptFunc::Crit_ClearAnim( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->ClearAnim();
}

void FOClient::SScriptFunc::Crit_Wait( CritterCl* cr, uint ms )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->TickStart( ms );
}

uint FOClient::SScriptFunc::Crit_CountItem( CritterCl* cr, hash proto_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->CountItemPid( proto_id );
}

Item* FOClient::SScriptFunc::Crit_GetItem( CritterCl* cr, uint item_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetItem( item_id );
}

Item* FOClient::SScriptFunc::Crit_GetItemPredicate( CritterCl* cr, asIScriptFunction* predicate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec inv_items = cr->InvItems;
    for( Item* item : inv_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            return item;
    }
    return nullptr;
}

Item* FOClient::SScriptFunc::Crit_GetItemBySlot( CritterCl* cr, uchar slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetItemSlot( slot );
}

Item* FOClient::SScriptFunc::Crit_GetItemByPid( CritterCl* cr, hash proto_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetItemByPidInvPriority( proto_id );
}

CScriptArray* FOClient::SScriptFunc::Crit_GetItems( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return Script::CreateArrayRef( "Item[]", cr->InvItems );
}

CScriptArray* FOClient::SScriptFunc::Crit_GetItemsBySlot( CritterCl* cr, uchar slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec items;
    cr->GetItemsSlot( slot, items );
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOClient::SScriptFunc::Crit_GetItemsPredicate( CritterCl* cr, asIScriptFunction* predicate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec inv_items = cr->InvItems;
    ItemVec items;
    items.reserve( inv_items.size() );
    for( Item* item : inv_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            items.push_back( item );
    }
    return Script::CreateArrayRef( "Item[]", items );
}

void FOClient::SScriptFunc::Crit_SetVisible( CritterCl* cr, bool visible )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->Visible = visible;
    Self->HexMngr.RefreshMap();
}

bool FOClient::SScriptFunc::Crit_GetVisible( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->Visible;
}

void FOClient::SScriptFunc::Crit_set_ContourColor( CritterCl* cr, uint value )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( cr->SprDrawValid )
        cr->SprDraw->SetContour( cr->SprDraw->ContourType, value );

    cr->ContourColor = value;
}

uint FOClient::SScriptFunc::Crit_get_ContourColor( CritterCl* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->ContourColor;
}

void FOClient::SScriptFunc::Crit_GetNameTextInfo( CritterCl* cr, bool& name_visible, int& x, int& y, int& w, int& h, int& lines )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->GetNameTextInfo( name_visible, x, y, w, h, lines );
}

void FOClient::SScriptFunc::Crit_AddAnimationCallback( CritterCl* cr, uint anim1, uint anim2, float normalized_time, asIScriptFunction* animation_callback )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cr->Anim3d )
        SCRIPT_ERROR_R( "Critter is not 3d." );
    if( normalized_time < 0.0f || normalized_time > 1.0f )
        SCRIPT_ERROR_R( "Normalized time is not in range 0..1." );

    uint bind_id = Script::BindByFunc( animation_callback, false );
    RUNTIME_ASSERT( bind_id );
    cr->Anim3d->AnimationCallbacks.push_back( { anim1, anim2, normalized_time, [ cr, bind_id ]
                                                {
                                                    if( cr->IsDestroyed )
                                                        return;

                                                    Script::PrepareContext( bind_id, "AnimationCallback" );
                                                    Script::SetArgEntity( cr );
                                                    Script::RunPrepared();
                                                } } );
}

bool FOClient::SScriptFunc::Crit_GetBonePosition( CritterCl* cr, hash bone_name, int& bone_x, int& bone_y )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr->Anim3d )
        SCRIPT_ERROR_R0( "Critter is not 3d." );

    if( !cr->Anim3d->GetBonePos( bone_name, bone_x, bone_y ) )
        return false;

    bone_x += cr->SprOx;
    bone_y += cr->SprOy;
    return true;
}

Item* FOClient::SScriptFunc::Item_Clone( Item* item, uint count )
{
    Item* clone = item->Clone();
    if( count )
        clone->SetCount( count );
    return clone;
}

bool FOClient::SScriptFunc::Item_GetMapPosition( Item* item, ushort& hx, ushort& hy )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );

    switch( item->GetAccessory() )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        CritterCl* cr = Self->GetCritter( item->GetCritId() );
        if( !cr )
            SCRIPT_ERROR_R0( "CritterCl accessory, CritterCl not found." );
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        hx = item->GetHexX();
        hy = item->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
    {

        if( item->GetId() == item->GetContainerId() )
            SCRIPT_ERROR_R0( "Container accessory, crosslinks." );
        Item* cont = Self->GetItem( item->GetContainerId() );
        if( !cont )
            SCRIPT_ERROR_R0( "Container accessory, container not found." );
        return Item_GetMapPosition( cont, hx, hy );             // Recursion
    }
    break;
    default:
        SCRIPT_ERROR_R0( "Unknown accessory." );
        return false;
    }
    return true;
}

void FOClient::SScriptFunc::Item_Animate( Item* item, uint from_frame, uint to_frame )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( item->Type == EntityType::ItemHex )
    {
        ItemHex* item_hex = (ItemHex*) item;
        item_hex->SetAnim( from_frame, to_frame );
    }
}

CScriptArray* FOClient::SScriptFunc::Item_GetItems( Item* cont, uint stack_id )
{
    if( cont->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec items;
    cont->ContGetItems( items, stack_id );
    return Script::CreateArrayRef( "Item[]", items );
}

string FOClient::SScriptFunc::Global_CustomCall( string command, string separator )
{
    // Parse command
    vector< string >  args;
    std::stringstream ss( command );
    if( separator.length() > 0 )
    {
        string arg;
        while( getline( ss, arg, *separator.c_str() ) )
            args.push_back( arg );
    }
    else
    {
        args.push_back( command );
    }
    if( args.size() < 1 )
        SCRIPT_ERROR_R0( "Empty custom call command." );

    // Execute
    string cmd = args[ 0 ];
    if( cmd == "Login" && args.size() >= 3 )
    {
        if( Self->InitNetReason == INIT_NET_REASON_NONE )
        {
            Self->LoginName = args[ 1 ];
            Self->LoginPassword = args[ 2 ];
            Self->InitNetReason = INIT_NET_REASON_LOGIN;
        }
    }
    else if( cmd == "Register" && args.size() >= 3 )
    {
        if( Self->InitNetReason == INIT_NET_REASON_NONE )
        {
            Self->LoginName = args[ 1 ];
            Self->LoginPassword = args[ 2 ];
            Self->InitNetReason = INIT_NET_REASON_REG;
        }
    }
    else if( cmd == "CustomConnect" )
    {
        if( Self->InitNetReason == INIT_NET_REASON_NONE )
        {
            Self->InitNetReason = INIT_NET_REASON_CUSTOM;
            if( !Self->UpdateFilesInProgress )
                Self->UpdateFilesStart();
        }
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
            if( SprMngr.EnableFullscreen() )
                GameOpt.FullScreen = true;
        }
        else
        {
            if( SprMngr.DisableFullscreen() )
            {
                GameOpt.FullScreen = false;

                if( Self->WindowResolutionDiffX || Self->WindowResolutionDiffY )
                {
                    int x, y;
                    SprMngr.GetWindowPosition( x, y );
                    SprMngr.SetWindowPosition( x - Self->WindowResolutionDiffX, y - Self->WindowResolutionDiffY );
                    Self->WindowResolutionDiffX = Self->WindowResolutionDiffY = 0;
                }
            }
        }
        SprMngr.RefreshViewport();
    }
    else if( cmd == "MinimizeWindow" )
    {
        SprMngr.MinimizeWindow();
    }
    else if( cmd == "SwitchLookBorders" )
    {
        // Self->DrawLookBorders = !Self->DrawLookBorders;
        // Self->RebuildLookBorders = true;
    }
    else if( cmd == "SwitchShootBorders" )
    {
        // Self->DrawShootBorders = !Self->DrawShootBorders;
        // Self->RebuildLookBorders = true;
    }
    else if( cmd == "GetShootBorders" )
    {
        return Self->DrawShootBorders ? "true" : "false";
    }
    else if( cmd == "SetShootBorders" && args.size() >= 2 )
    {
        bool set = ( args[ 1 ] == "true" );
        if( Self->DrawShootBorders != set )
        {
            Self->DrawShootBorders = set;
            Self->RebuildLookBorders = true;
        }
    }
    else if( cmd == "SetMousePos" && args.size() == 4 )
    {
        #ifndef FO_WEB
        int  x = _str( args[ 1 ] ).toInt();
        int  y = _str( args[ 2 ] ).toInt();
        bool motion = _str( args[ 3 ] ).toBool();
        if( motion )
        {
            SprMngr.SetMousePosition( x, y );
        }
        else
        {
            SDL_EventState( SDL_MOUSEMOTION, SDL_DISABLE );
            SprMngr.SetMousePosition( x, y );
            SDL_EventState( SDL_MOUSEMOTION, SDL_ENABLE );
            GameOpt.MouseX = GameOpt.LastMouseX = x;
            GameOpt.MouseY = GameOpt.LastMouseY = y;
        }
        #endif
    }
    else if( cmd == "SetCursorPos" )
    {
        if( Self->HexMngr.IsMapLoaded() )
            Self->HexMngr.SetCursorPos( GameOpt.MouseX, GameOpt.MouseY, Keyb::CtrlDwn, true );
    }
    else if( cmd == "NetDisconnect" )
    {
        Self->NetDisconnect();

        if( !Self->IsConnected && !Self->IsMainScreen( SCREEN_LOGIN ) )
            Self->ShowMainScreen( SCREEN_LOGIN );
    }
    else if( cmd == "TryExit" )
    {
        Self->TryExit();
    }
    else if( cmd == "Version" )
    {
        return _str( "{}", FONLINE_VERSION );
    }
    else if( cmd == "BytesSend" )
    {
        return _str( "{}", Self->BytesSend );
    }
    else if( cmd == "BytesReceive" )
    {
        return _str( "{}", Self->BytesReceive );
    }
    else if( cmd == "GetLanguage" )
    {
        return Self->CurLang.NameStr;
    }
    else if( cmd == "SetLanguage" && args.size() >= 2 )
    {
        if( args[ 1 ].length() == 4 )
            Self->CurLang.LoadFromCache( args[ 1 ].c_str() );
    }
    else if( cmd == "SetResolution" && args.size() >= 3 )
    {
        int w = _str( args[ 1 ] ).toInt();
        int h = _str( args[ 2 ] ).toInt();
        int diff_w = w - GameOpt.ScreenWidth;
        int diff_h = h - GameOpt.ScreenHeight;

        GameOpt.ScreenWidth = w;
        GameOpt.ScreenHeight = h;
        SprMngr.SetWindowSize( w, h );

        if( !GameOpt.FullScreen )
        {
            int x, y;
            SprMngr.GetWindowPosition( x, y );
            SprMngr.SetWindowPosition( x - diff_w / 2, y - diff_h / 2 );
        }
        else
        {
            Self->WindowResolutionDiffX += diff_w / 2;
            Self->WindowResolutionDiffY += diff_h / 2;
        }

        SprMngr.OnResolutionChanged();
        if( Self->HexMngr.IsMapLoaded() )
            Self->HexMngr.OnResolutionChanged();
    }
    else if( cmd == "RefreshAlwaysOnTop" )
    {
        SprMngr.SetAlwaysOnTop( GameOpt.AlwaysOnTop );
    }
    else if( cmd == "Command" && args.size() >= 2 )
    {
        string str;
        for( size_t i = 1; i < args.size(); i++ )
            str += args[ i ] + " ";
        str = _str( str ).trim();

        string buf;
        if( !PackCommand( str, Self->Bout, [ &buf, &separator ] ( auto s ) { buf += s + separator; }, Self->Chosen->Name ) )
            return "UNKNOWN";

        return buf;
    }
    else if( cmd == "ConsoleMessage" && args.size() >= 2 )
    {
        Self->Net_SendText( args[ 1 ].c_str(), SAY_NORM );
    }
    else if( cmd == "SaveLog" && args.size() == 3 )
    {
//              if( file_name == "" )
//              {
//                      DateTime dt;
//                      Timer::GetCurrentDateTime(dt);
//                      char     log_path[MAX_FOPATH];
//                      X_str(log_path, "messbox_%04d.%02d.%02d_%02d-%02d-%02d.txt",
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
    else if( cmd == "DialogAnswer" && args.size() >= 4 )
    {
        bool is_npc = ( args[ 1 ] == "true" );
        uint talker_id = _str( args[ 2 ] ).toUInt();
        uint answer_index = _str( args[ 3 ] ).toUInt();
        Self->Net_SendTalk( is_npc, talker_id, answer_index );
    }
    else if( cmd == "DrawMiniMap" && args.size() >= 6 )
    {
        static int zoom, x, y, x2, y2;
        zoom = _str( args[ 1 ] ).toInt();
        x = _str( args[ 2 ] ).toInt();
        y = _str( args[ 3 ] ).toInt();
        x2 = x + _str( args[ 4 ] ).toInt();
        y2 = y + _str( args[ 5 ] ).toInt();

        if( zoom != Self->LmapZoom || x != Self->LmapWMap[ 0 ] || y != Self->LmapWMap[ 1 ] || x2 != Self->LmapWMap[ 2 ] || y2 != Self->LmapWMap[ 3 ] )
        {
            Self->LmapZoom = zoom;
            Self->LmapWMap[ 0 ] = x;
            Self->LmapWMap[ 1 ] = y;
            Self->LmapWMap[ 2 ] = x2;
            Self->LmapWMap[ 3 ] = y2;
            Self->LmapPrepareMap();
        }
        else if( Timer::FastTick() >= Self->LmapPrepareNextTick )
        {
            Self->LmapPrepareMap();
        }

        SprMngr.DrawPoints( Self->LmapPrepPix, PRIMITIVE_LINELIST );
    }
    else if( cmd == "RefreshMe" )
    {
        Self->Net_SendRefereshMe();
    }
    else if( cmd == "SetCrittersContour" && args.size() == 2 )
    {
        int countour_type = _str( args[ 1 ] ).toInt();
        Self->HexMngr.SetCrittersContour( countour_type );
    }
    else if( cmd == "DrawWait" )
    {
        Self->WaitDraw();
    }
    else if( cmd == "ChangeDir" && args.size() == 2 )
    {
        int dir = _str( args[ 1 ] ).toInt();
        Self->Chosen->ChangeDir( dir );
        Self->Net_SendDir();
    }
    else if( cmd == "MoveItem" && args.size() == 5 )
    {
        uint  item_count = _str( args[ 1 ] ).toUInt();
        uint  item_id = _str( args[ 2 ] ).toUInt();
        uint  item_swap_id = _str( args[ 3 ] ).toUInt();
        int   to_slot = _str( args[ 4 ] ).toInt();
        Item* item = Self->Chosen->GetItem( item_id );
        Item* item_swap = ( item_swap_id ? Self->Chosen->GetItem( item_swap_id ) : nullptr );
        Item* old_item = item->Clone();
        int   from_slot = item->GetCritSlot();

        // Move
        bool is_light = item->GetIsLight();
        if( to_slot == -1 )
        {
            Self->Chosen->Action( ACTION_DROP_ITEM, from_slot, item );
            if( item->GetStackable() && item_count < item->GetCount() )
            {
                item->ChangeCount( -(int) item->GetCount() );
            }
            else
            {
                Self->Chosen->DeleteItem( item, true );
                item = nullptr;
            }
        }
        else
        {
            item->SetCritSlot( to_slot );
            if( item_swap )
                item_swap->SetCritSlot( from_slot );

            Self->Chosen->Action( ACTION_MOVE_ITEM, from_slot, item );
            if( item_swap )
                Self->Chosen->Action( ACTION_MOVE_ITEM_SWAP, to_slot, item_swap );
        }

        // Light
        Self->RebuildLookBorders = true;
        if( is_light && ( !to_slot || ( !from_slot && to_slot != -1 ) ) )
            Self->HexMngr.RebuildLight();

        // Notify scripts about item changing
        Self->OnItemInvChanged( old_item, item );
    }
    else if( cmd == "SkipRoof" && args.size() == 3 )
    {
        uint hx = _str( args[ 1 ] ).toUInt();
        uint hy = _str( args[ 2 ] ).toUInt();
        Self->HexMngr.SetSkipRoof( hx, hy );
    }
    else if( cmd == "RebuildLookBorders" )
    {
        Self->RebuildLookBorders = true;
    }
    else if( cmd == "TransitCritter" && args.size() == 5 )
    {
        int  hx = _str( args[ 1 ] ).toInt();
        int  hy = _str( args[ 2 ] ).toInt();
        bool animate = _str( args[ 3 ] ).toBool();
        bool force = _str( args[ 4 ] ).toBool();

        Self->HexMngr.TransitCritter( Self->Chosen, hx, hy, animate, force );
    }
    else if( cmd == "SendMove" )
    {
        UCharVec dirs;
        for( size_t i = 1; i < args.size(); i++ )
            dirs.push_back( (uchar) _str( args[ i ] ).toInt() );

        Self->Net_SendMove( dirs );

        if( dirs.size() > 1 )
        {
            Self->Chosen->MoveSteps.resize( 1 );
        }
        else
        {
            Self->Chosen->MoveSteps.resize( 0 );
            if( !Self->Chosen->IsAnim() )
                Self->Chosen->AnimateStay();
        }
    }
    else if( cmd == "ChosenAlpha" && args.size() == 2 )
    {
        int alpha = _str( args[ 1 ] ).toInt();

        Self->Chosen->Alpha = (uchar) alpha;
    }
    else if( cmd == "SetScreenKeyboard" && args.size() == 2 )
    {
        if( SDL_HasScreenKeyboardSupport() )
        {
            bool cur = ( SDL_IsTextInputActive() != SDL_FALSE );
            bool next = _str( args[ 1 ] ).toBool();
            if( cur != next )
            {
                if( next )
                    SDL_StartTextInput();
                else
                    SDL_StopTextInput();
            }
        }
    }
    else
    {
        SCRIPT_ERROR_R0( "Invalid custom call command." );
    }
    return "";
}

CritterCl* FOClient::SScriptFunc::Global_GetChosen()
{
    if( Self->Chosen && Self->Chosen->IsDestroyed )
        return nullptr;
    return Self->Chosen;
}

Item* FOClient::SScriptFunc::Global_GetItem( uint item_id )
{
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );

    // On map
    Item* item = Self->GetItem( item_id );

    // On Chosen
    if( !item && Self->Chosen )
        item = Self->Chosen->GetItem( item_id );

    // On other critters
    if( !item )
    {
        for( auto it = Self->HexMngr.GetCritters().begin(); !item && it != Self->HexMngr.GetCritters().end(); ++it )
            if( !it->second->IsChosen() )
                item = it->second->GetItem( item_id );
    }

    if( !item || item->IsDestroyed )
        return nullptr;
    return item;
}

CScriptArray* FOClient::SScriptFunc::Global_GetMapAllItems()
{
    CScriptArray* items = Script::CreateArray( "Item[]" );
    if( Self->HexMngr.IsMapLoaded() )
    {
        ItemHexVec& items_ = Self->HexMngr.GetItems();
        for( auto it = items_.begin(); it != items_.end();)
            it = ( ( *it )->IsFinishing() ? items_.erase( it ) : ++it );
        Script::AppendVectorToArrayRef( items_, items );
    }
    return items;
}

CScriptArray* FOClient::SScriptFunc::Global_GetMapHexItems( ushort hx, ushort hy )
{
    ItemHexVec items;
    if( Self->HexMngr.IsMapLoaded() )
    {
        Self->HexMngr.GetItems( hx, hy, items );
        for( auto it = items.begin(); it != items.end();)
            it = ( ( *it )->IsFinishing() ? items.erase( it ) : ++it );
    }
    return Script::CreateArrayRef( "Item[]", items );
}

uint FOClient::SScriptFunc::Global_GetCrittersDistantion( CritterCl* cr1, CritterCl* cr2 )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    if( !cr1 )
        SCRIPT_ERROR_R0( "Critter1 arg is null." );
    if( cr1->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter1 arg is destroyed." );
    if( !cr2 )
        SCRIPT_ERROR_R0( "Critter2 arg is null." );
    if( cr2->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter2 arg is destroyed." );
    return DistGame( cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY() );
}

CritterCl* FOClient::SScriptFunc::Global_GetCritter( uint critter_id )
{
    if( !critter_id )
        return nullptr;                // SCRIPT_ERROR_R0("Critter id arg is zero.");
    CritterCl* cr = Self->GetCritter( critter_id );
    if( !cr || cr->IsDestroyed )
        return nullptr;
    return cr;
}

CScriptArray* FOClient::SScriptFunc::Global_GetCritters( ushort hx, ushort hy, uint radius, int find_type )
{
    if( hx >= Self->HexMngr.GetWidth() || hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    CritMap& crits = Self->HexMngr.GetCritters();
    CritVec  critters;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        CritterCl* cr = it->second;
        if( cr->CheckFind( find_type ) && CheckDist( hx, hy, cr->GetHexX(), cr->GetHexY(), radius ) )
            critters.push_back( cr );
    }

    SortCritterByDist( hx, hy, critters );
    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOClient::SScriptFunc::Global_GetCrittersByPids( hash pid, int find_type )
{
    CritMap& crits = Self->HexMngr.GetCritters();
    CritVec  critters;
    if( !pid )
    {
        for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
        {
            CritterCl* cr = it->second;
            if( cr->CheckFind( find_type ) )
                critters.push_back( cr );
        }
    }
    else
    {
        for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
        {
            CritterCl* cr = it->second;
            if( cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind( find_type ) )
                critters.push_back( cr );
        }
    }
    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOClient::SScriptFunc::Global_GetCrittersInPath( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type )
{
    CritVec critters;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, nullptr, false, &critters, find_type, nullptr, nullptr, nullptr, true );
    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOClient::SScriptFunc::Global_GetCrittersInPathBlock( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy )
{
    CritVec    critters;
    UShortPair block, pre_block;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, nullptr, false, &critters, find_type, &block, &pre_block, nullptr, true );
    pre_block_hx = pre_block.first;
    pre_block_hy = pre_block.second;
    block_hx = block.first;
    block_hy = block.second;
    return Script::CreateArrayRef( "Critter[]", critters );
}

void FOClient::SScriptFunc::Global_GetHexInPath( ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    UShortPair pre_block, block;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true );
    to_hx = pre_block.first;
    to_hy = pre_block.second;
}

CScriptArray* FOClient::SScriptFunc::Global_GetPathHex( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut )
{
    if( from_hx >= Self->HexMngr.GetWidth() || from_hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid from hexes args." );
    if( to_hx >= Self->HexMngr.GetWidth() || to_hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    if( cut > 0 && !Self->HexMngr.CutPath( nullptr, from_hx, from_hy, to_hx, to_hy, cut ) )
        return nullptr;

    UCharVec steps;
    if( !Self->HexMngr.FindPath( nullptr, from_hx, from_hy, to_hx, to_hy, steps, -1 ) )
        return nullptr;

    CScriptArray* path = Script::CreateArray( "uint8[]" );
    Script::AppendVectorToArray( steps, path );
    return path;
}

CScriptArray* FOClient::SScriptFunc::Global_GetPathCr( CritterCl* cr, ushort to_hx, ushort to_hy, uint cut )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( to_hx >= Self->HexMngr.GetWidth() || to_hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    if( cut > 0 && !Self->HexMngr.CutPath( cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, cut ) )
        return nullptr;

    UCharVec steps;
    if( !Self->HexMngr.FindPath( cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, steps, -1 ) )
        return nullptr;

    CScriptArray* path = Script::CreateArray( "uint8[]" );
    Script::AppendVectorToArray( steps, path );
    return path;
}

uint FOClient::SScriptFunc::Global_GetPathLengthHex( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut )
{
    if( from_hx >= Self->HexMngr.GetWidth() || from_hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid from hexes args." );
    if( to_hx >= Self->HexMngr.GetWidth() || to_hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    if( cut > 0 && !Self->HexMngr.CutPath( nullptr, from_hx, from_hy, to_hx, to_hy, cut ) )
        return 0;

    UCharVec steps;
    if( !Self->HexMngr.FindPath( nullptr, from_hx, from_hy, to_hx, to_hy, steps, -1 ) )
        steps.clear();

    return (uint) steps.size();
}

uint FOClient::SScriptFunc::Global_GetPathLengthCr( CritterCl* cr, ushort to_hx, ushort to_hy, uint cut )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( to_hx >= Self->HexMngr.GetWidth() || to_hy >= Self->HexMngr.GetHeight() )
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

bool FOClient::SScriptFunc::Global_PlaySound( string sound_name )
{
    return SndMngr.PlaySound( sound_name );
}

bool FOClient::SScriptFunc::Global_PlayMusic( string music_name, uint repeat_time )
{
    if( music_name.empty() )
    {
        SndMngr.StopMusic();
        return true;
    }

    return SndMngr.PlayMusic( music_name, repeat_time );
}

void FOClient::SScriptFunc::Global_PlayVideo( string video_name, bool can_stop )
{
    SndMngr.StopMusic();
    Self->AddVideo( video_name.c_str(), can_stop, true );
}

hash FOClient::SScriptFunc::Global_GetCurrentMapPid()
{
    if( !Self->HexMngr.IsMapLoaded() )
        return 0;
    return Self->CurMapPid;
}

void FOClient::SScriptFunc::Global_Message( string msg )
{
    Self->AddMess( FOMB_GAME, msg, true );
}

void FOClient::SScriptFunc::Global_MessageType( string msg, int type )
{
    Self->AddMess( type, msg, true );
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

    Self->AddMess( type, Self->CurLang.Msg[ text_msg ].GetStr( str_num ), true );
}

void FOClient::SScriptFunc::Global_MapMessage( string text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy )
{
    FOClient::MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = ( color ? color : COLOR_TEXT );
    t.Fade = fade;
    t.StartTick = Timer::GameTick();
    t.Tick = ms;
    t.Text = text;
    t.Pos = Self->HexMngr.GetRectForText( hx, hy );
    t.EndPos = Rect( t.Pos, ox, oy );
    auto it = std::find( Self->GameMapTexts.begin(), Self->GameMapTexts.end(), t );
    if( it != Self->GameMapTexts.end() )
        Self->GameMapTexts.erase( it );
    Self->GameMapTexts.push_back( t );
}

string FOClient::SScriptFunc::Global_GetMsgStr( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].GetStr( str_num );
}

string FOClient::SScriptFunc::Global_GetMsgStrSkip( int text_msg, uint str_num, uint skip_count )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].GetStr( str_num, skip_count );
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

string FOClient::SScriptFunc::Global_ReplaceTextStr( string text, string replace, string str )
{
    size_t pos = text.find( replace, 0 );
    if( pos == std::string::npos )
        return text;
    return string( text ).replace( pos, replace.length(), str );
}

string FOClient::SScriptFunc::Global_ReplaceTextInt( string text, string replace, int i )
{
    size_t pos = text.find( replace, 0 );
    if( pos == std::string::npos )
        return text;
    return string( text ).replace( pos, replace.length(), _str( "{}", i ) );
}

string FOClient::SScriptFunc::Global_FormatTags( string text, string lexems )
{
    Self->FormatTags( text, Self->Chosen, nullptr, lexems );
    return text;
}

void FOClient::SScriptFunc::Global_MoveScreenToHex( ushort hx, ushort hy, uint speed, bool can_stop )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R( "Map is not loaded." );
    if( hx >= Self->HexMngr.GetWidth() || hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R( "Invalid hex args." );

    if( !speed )
        Self->HexMngr.FindSetCenter( hx, hy );
    else
        Self->HexMngr.ScrollToHex( hx, hy, (float) speed / 1000.0f, can_stop );
}

void FOClient::SScriptFunc::Global_MoveScreenOffset( int ox, int oy, uint speed, bool can_stop )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R( "Map is not loaded." );

    Self->HexMngr.ScrollOffset( ox, oy, (float) speed / 1000.0f, can_stop );
}

void FOClient::SScriptFunc::Global_LockScreenScroll( CritterCl* cr, bool soft_lock, bool unlock_if_same )
{
    if( cr && cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );

    uint id = ( cr ? cr->GetId() : 0 );
    if( soft_lock )
    {
        if( unlock_if_same && id == Self->HexMngr.AutoScroll.SoftLockedCritter )
            Self->HexMngr.AutoScroll.SoftLockedCritter = 0;
        else
            Self->HexMngr.AutoScroll.SoftLockedCritter = id;

        Self->HexMngr.AutoScroll.CritterLastHexX = ( cr ? cr->GetHexX() : 0 );
        Self->HexMngr.AutoScroll.CritterLastHexY = ( cr ? cr->GetHexY() : 0 );
    }
    else
    {
        if( unlock_if_same && id == Self->HexMngr.AutoScroll.HardLockedCritter )
            Self->HexMngr.AutoScroll.HardLockedCritter = 0;
        else
            Self->HexMngr.AutoScroll.HardLockedCritter = id;
    }
}

int FOClient::SScriptFunc::Global_GetFog( ushort zone_x, ushort zone_y )
{
    if( !Self->Chosen || Self->Chosen->IsDestroyed )
        SCRIPT_ERROR_R0( "Chosen is destroyed." );
    if( zone_x >= GameOpt.GlobalMapWidth || zone_y >= GameOpt.GlobalMapHeight )
        SCRIPT_ERROR_R0( "Invalid world map pos arg." );
    return Self->GmapFog->Get2Bit( zone_x, zone_y );
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

uint FOClient::SScriptFunc::Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    if( !year )
        year = Globals->GetYear();
    else
        year = CLAMP( year, Globals->GetYearStart(), Globals->GetYearStart() + 130 );
    if( !month )
        month = Globals->GetMonth();
    else
        month = CLAMP( month, 1, 12 );
    if( !day )
    {
        day = Globals->GetDay();
    }
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
            MoveHexByDir( hx, hy, dir, Self->HexMngr.GetWidth(), Self->HexMngr.GetHeight() );
    }
    else
    {
        MoveHexByDir( hx, hy, dir, Self->HexMngr.GetWidth(), Self->HexMngr.GetHeight() );
    }
}

hash FOClient::SScriptFunc::Global_GetTileName( ushort hx, ushort hy, bool roof, int layer )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map not loaded." );
    if( hx >= Self->HexMngr.GetWidth() )
        SCRIPT_ERROR_R0( "Invalid hex x arg." );
    if( hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hex y arg." );

    AnyFrames* simply_tile = Self->HexMngr.GetField( hx, hy ).SimplyTile[ roof ? 1 : 0 ];
    if( simply_tile && !layer )
        return simply_tile->NameHash;

    Field::TileVec* tiles = Self->HexMngr.GetField( hx, hy ).Tiles[ roof ? 1 : 0 ];
    if( !tiles || tiles->empty() )
        return 0;

    for( auto& tile :* tiles )
    {
        if( tile.Layer == layer )
            return tile.Anim->NameHash;
    }
    return 0;
}

void FOClient::SScriptFunc::Global_Preload3dFiles( CScriptArray* fnames )
{
    size_t k = Self->Preload3dFiles.size();

    for( asUINT i = 0; i < fnames->GetSize(); i++ )
    {
        string& s = *(string*) fnames->At( i );
        Self->Preload3dFiles.push_back( s );
    }

    for( ; k < Self->Preload3dFiles.size(); k++ )
        Self->Preload3dFiles[ k ] = Self->Preload3dFiles[ k ].c_str();
}

void FOClient::SScriptFunc::Global_WaitPing()
{
    Self->WaitPing();
}

bool FOClient::SScriptFunc::Global_LoadFont( int font_index, string font_fname )
{
    SprMngr.PushAtlasType( RES_ATLAS_STATIC );
    bool result;
    if( font_fname.length() > 0 && font_fname[ 0 ] == '*' )
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

bool FOClient::SScriptFunc::Global_SetEffect( int effect_type, int effect_subtype, string effect_name, string effect_defines )
{
    // Effect types
    #define EFFECT_2D_GENERIC                ( 0x00000001 ) // Subtype can be item id, zero for all items
    #define EFFECT_2D_CRITTER                ( 0x00000002 ) // Subtype can be critter id, zero for all critters
    #define EFFECT_2D_TILE                   ( 0x00000004 )
    #define EFFECT_2D_ROOF                   ( 0x00000008 )
    #define EFFECT_2D_RAIN                   ( 0x00000010 )
    #define EFFECT_3D_SKINNED                ( 0x00000400 )
    #define EFFECT_INTERFACE_BASE            ( 0x00001000 )
    #define EFFECT_INTERFACE_CONTOUR         ( 0x00002000 )
    #define EFFECT_FONT                      ( 0x00010000 ) // Subtype is FONT_*, -1 default for all fonts
    #define EFFECT_PRIMITIVE_GENERIC         ( 0x00100000 )
    #define EFFECT_PRIMITIVE_LIGHT           ( 0x00200000 )
    #define EFFECT_PRIMITIVE_FOG             ( 0x00400000 )
    #define EFFECT_FLUSH_RENDER_TARGET       ( 0x01000000 )
    #define EFFECT_FLUSH_RENDER_TARGET_MS    ( 0x02000000 ) // Multisample
    #define EFFECT_FLUSH_PRIMITIVE           ( 0x04000000 )
    #define EFFECT_FLUSH_MAP                 ( 0x08000000 )
    #define EFFECT_FLUSH_LIGHT               ( 0x10000000 )
    #define EFFECT_FLUSH_FOG                 ( 0x20000000 )
    #define EFFECT_OFFSCREEN                 ( 0x40000000 )

    Effect* effect = nullptr;
    if( !effect_name.empty() )
    {
        bool use_in_2d = !( effect_type & EFFECT_3D_SKINNED );
        effect = GraphicLoader::LoadEffect( effect_name, use_in_2d, effect_defines );
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
    if( effect_type & EFFECT_PRIMITIVE_FOG )
        *Effect::Fog = ( effect ? *effect : *Effect::FogDefault );

    if( effect_type & EFFECT_FLUSH_RENDER_TARGET )
        *Effect::FlushRenderTarget = ( effect ? *effect : *Effect::FlushRenderTargetDefault );
    if( effect_type & EFFECT_FLUSH_RENDER_TARGET_MS )
        *Effect::FlushRenderTargetMS = ( effect ? *effect : *Effect::FlushRenderTargetMSDefault );
    if( effect_type & EFFECT_FLUSH_PRIMITIVE )
        *Effect::FlushPrimitive = ( effect ? *effect : *Effect::FlushPrimitiveDefault );
    if( effect_type & EFFECT_FLUSH_MAP )
        *Effect::FlushMap = ( effect ? *effect : *Effect::FlushMapDefault );
    if( effect_type & EFFECT_FLUSH_LIGHT )
        *Effect::FlushLight = ( effect ? *effect : *Effect::FlushLightDefault );
    if( effect_type & EFFECT_FLUSH_FOG )
        *Effect::FlushFog = ( effect ? *effect : *Effect::FlushFogDefault );

    if( effect_type & EFFECT_OFFSCREEN )
    {
        if( effect_subtype < 0 )
            SCRIPT_ERROR_R0( "Negative effect subtype." );

        Self->OffscreenEffects.resize( effect_subtype + 1 );
        Self->OffscreenEffects[ effect_subtype ] = effect;
    }

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

void FOClient::SScriptFunc::Global_MouseClick( int x, int y, int button )
{
    IntVec prev_events = MainWindowMouseEvents;
    MainWindowMouseEvents.clear();
    int    prev_x = GameOpt.MouseX;
    int    prev_y = GameOpt.MouseY;
    int    last_prev_x = GameOpt.LastMouseX;
    int    last_prev_y = GameOpt.LastMouseY;
    GameOpt.MouseX = GameOpt.LastMouseX = x;
    GameOpt.MouseY = GameOpt.LastMouseY = y;
    MainWindowMouseEvents.push_back( SDL_MOUSEBUTTONDOWN );
    MainWindowMouseEvents.push_back( button );
    MainWindowMouseEvents.push_back( SDL_MOUSEBUTTONUP );
    MainWindowMouseEvents.push_back( button );
    Self->ParseMouse();
    MainWindowMouseEvents = prev_events;
    GameOpt.MouseX = prev_x;
    GameOpt.MouseY = prev_y;
    GameOpt.LastMouseX = last_prev_x;
    GameOpt.LastMouseY = last_prev_y;
}

void FOClient::SScriptFunc::Global_KeyboardPress( uchar key1, uchar key2, string key1_text, string key2_text )
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
        MainWindowKeyboardEventsText.push_back( key1_text );
    }
    if( key2 )
    {
        MainWindowKeyboardEvents.push_back( SDL_KEYDOWN );
        MainWindowKeyboardEvents.push_back( Keyb::UnmapKey( key2 ) );
        MainWindowKeyboardEventsText.push_back( key2_text );
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

void FOClient::SScriptFunc::Global_SetRainAnimation( string fall_anim_name, string drop_anim_name )
{
    Self->HexMngr.SetRainAnimation( !fall_anim_name.empty() ? fall_anim_name.c_str() : nullptr, !drop_anim_name.empty() ? drop_anim_name.c_str() : nullptr );
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

void FOClient::SScriptFunc::Global_SetPropertyGetCallback( asIScriptGeneric* gen )
{
    int   prop_enum_value = gen->GetArgDWord( 0 );
    void* ref = gen->GetArgAddress( 1 );
    gen->SetReturnByte( 0 );
    RUNTIME_ASSERT( ref );

    Property* prop = GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : CritterCl::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Map::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Location::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R( "Property '{}' not found.", _str().parseHash( prop_enum_value ) );

    string result = prop->SetGetCallback( *(asIScriptFunction**) ref );
    if( result != "" )
        SCRIPT_ERROR_R( result.c_str() );

    gen->SetReturnByte( 1 );
}

void FOClient::SScriptFunc::Global_AddPropertySetCallback( asIScriptGeneric* gen )
{
    int   prop_enum_value = gen->GetArgDWord( 0 );
    void* ref = gen->GetArgAddress( 1 );
    bool  deferred = gen->GetArgByte( 2 ) != 0;
    gen->SetReturnByte( 0 );
    RUNTIME_ASSERT( ref );

    Property* prop = CritterCl::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Map::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Location::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R( "Property '{}' not found.", _str().parseHash( prop_enum_value ) );

    string result = prop->AddSetCallback( *(asIScriptFunction**) ref, deferred );
    if( result != "" )
        SCRIPT_ERROR_R( result );

    gen->SetReturnByte( 1 );
}

void FOClient::SScriptFunc::Global_AllowSlot( uchar index, bool enable_send )
{
    CritterCl::SlotEnabled[ index ] = true;
}

bool FOClient::SScriptFunc::Global_LoadDataFile( string dat_name )
{
    if( FileManager::LoadDataFile( dat_name ) )
    {
        ResMngr.Refresh();
        return true;
    }
    return false;
}

uint FOClient::SScriptFunc::Global_LoadSprite( string spr_name )
{
    return Self->AnimLoad( spr_name.c_str(), RES_ATLAS_STATIC );
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

void FOClient::SScriptFunc::Global_GetTextInfo( string text, int w, int h, int font, int flags, int& tw, int& th, int& lines )
{
    SprMngr.GetTextInfo( w, h, text, font, flags, tw, th, lines );
}

void FOClient::SScriptFunc::Global_DrawSprite( uint spr_id, int frame_index, int x, int y, uint color, bool offs )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( !spr_id )
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
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( !spr_id )
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
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( !spr_id )
        return;

    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || frame_index >= (int) anim->GetCnt() )
        return;

    SprMngr.DrawSpritePattern( frame_index < 0 ? anim->GetCurSprId() : anim->GetSprId( frame_index ), x, y, w, h, spr_width, spr_height, COLOR_SCRIPT_SPRITE( color ) );
}

void FOClient::SScriptFunc::Global_DrawText( string text, int x, int y, int w, int h, uint color, int font, int flags )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( text.length() == 0 )
        return;

    if( w < 0 )
        w = -w, x -= w;
    if( h < 0 )
        h = -h, y -= h;

    Rect r = Rect( x, y, x + w, y + h );
    SprMngr.DrawStr( r, text.c_str(), flags, COLOR_SCRIPT_TEXT( color ), font );
}

void FOClient::SScriptFunc::Global_DrawPrimitive( int primitive_type, CScriptArray* data )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( data->GetSize() == 0 )
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
    int             size = data->GetSize() / 3;
    points.resize( size );

    for( int i = 0; i < size; i++ )
    {
        PrepPoint& pp = points[ i ];
        pp.PointX = *(int*) data->At( i * 3 );
        pp.PointY = *(int*) data->At( i * 3 + 1 );
        pp.PointColor = *(int*) data->At( i * 3 + 2 );
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    SprMngr.DrawPoints( points, prim );
}

void FOClient::SScriptFunc::Global_DrawMapSprite( MapSprite* map_spr )
{
    if( !map_spr )
        SCRIPT_ERROR_R( "Map sprite arg is null." );

    if( !Self->HexMngr.IsMapLoaded() )
        return;
    if( map_spr->HexX >= Self->HexMngr.GetWidth() || map_spr->HexY >= Self->HexMngr.GetHeight() )
        return;
    if( !Self->HexMngr.IsHexToDraw( map_spr->HexX, map_spr->HexY ) )
        return;

    AnyFrames* anim = Self->AnimGetFrames( map_spr->SprId );
    if( !anim || map_spr->FrameIndex >= (int) anim->GetCnt() )
        return;

    uint color = map_spr->Color;
    bool is_flat = map_spr->IsFlat;
    bool no_light = map_spr->NoLight;
    int  draw_order = map_spr->DrawOrder;
    int  draw_order_hy_offset = map_spr->DrawOrderHyOffset;
    int  corner = map_spr->Corner;
    bool disable_egg = map_spr->DisableEgg;
    uint contour_color = map_spr->ContourColor;

    if( map_spr->ProtoId )
    {
        ProtoItem* proto_item = ProtoMngr.GetProtoItem( map_spr->ProtoId );
        if( !proto_item )
            return;

        color = ( proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0 );
        is_flat = proto_item->GetIsFlat();
        bool is_item = !proto_item->IsAnyScenery();
        no_light = ( is_flat && !is_item );
        draw_order = ( is_flat ? ( is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY ) : ( is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY ) );
        draw_order_hy_offset = proto_item->GetDrawOrderOffsetHexY();
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = ( proto_item->GetIsBadItem() ? COLOR_RGB( 255, 0, 0 ) : 0 );
    }

    Field&   f = Self->HexMngr.GetField( map_spr->HexX, map_spr->HexY );
    Sprites& tree = Self->HexMngr.GetDrawTree();
    Sprite&  spr = tree.InsertSprite( draw_order, map_spr->HexX, map_spr->HexY + draw_order_hy_offset, 0,
                                      HEX_OX + map_spr->OffsX, HEX_OY + map_spr->OffsY, &f.ScrX, &f.ScrY,
                                      map_spr->FrameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId( map_spr->FrameIndex ),
                                      nullptr, map_spr->IsTweakOffs ? &map_spr->TweakOffsX : nullptr, map_spr->IsTweakOffs ? &map_spr->TweakOffsY : nullptr,
                                      map_spr->IsTweakAlpha ? &map_spr->TweakAlpha : nullptr, nullptr, &map_spr->Valid );

    spr.MapSpr = map_spr;
    map_spr->AddRef();

    if( !no_light )
        spr.SetLight( corner, Self->HexMngr.GetLightHex( 0, 0 ), Self->HexMngr.GetWidth(), Self->HexMngr.GetHeight() );

    if( !is_flat && !disable_egg )
    {
        int egg_type = 0;
        switch( corner )
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
            break;
        }
        spr.SetEgg( egg_type );
    }

    if( color )
    {
        spr.SetColor( color & 0xFFFFFF );
        spr.SetFixedAlpha( color >> 24 );
    }

    if( contour_color )
        spr.SetContour( CONTOUR_CUSTOM, contour_color );
}

void FOClient::SScriptFunc::Global_DrawCritter2d( hash model_name, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color )
{
    AnyFrames* anim = ResMngr.GetCrit2dAnim( model_name, anim1, anim2, dir );
    if( anim )
        SprMngr.DrawSpriteSize( anim->Ind[ 0 ], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE( color ) );
}

Animation3dVec DrawCritter3dAnim;
UIntVec        DrawCritter3dCrType;
BoolVec        DrawCritter3dFailToLoad;
int            DrawCritter3dLayers[ LAYERS3D_COUNT ];
void FOClient::SScriptFunc::Global_DrawCritter3d( uint instance, hash model_name, uint anim1, uint anim2, CScriptArray* layers, CScriptArray* position, uint color )
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b
    if( instance >= DrawCritter3dAnim.size() )
    {
        DrawCritter3dAnim.resize( instance + 1 );
        DrawCritter3dCrType.resize( instance + 1 );
        DrawCritter3dFailToLoad.resize( instance + 1 );
    }

    if( DrawCritter3dFailToLoad[ instance ] && DrawCritter3dCrType[ instance ] == model_name )
        return;

    Animation3d*& anim3d = DrawCritter3dAnim[ instance ];
    if( !anim3d || DrawCritter3dCrType[ instance ] != model_name )
    {
        if( anim3d )
            SprMngr.FreePure3dAnimation( anim3d );
        SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
        anim3d = SprMngr.LoadPure3dAnimation( _str().parseHash( model_name ), false );
        SprMngr.PopAtlasType();
        DrawCritter3dCrType[ instance ] = model_name;
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
    float period = ( count > 9 ? *(float*) position->At( 9 ) : 0.0f );
    float stl = ( count > 10 ? *(float*) position->At( 10 ) : 0.0f );
    float stt = ( count > 11 ? *(float*) position->At( 11 ) : 0.0f );
    float str = ( count > 12 ? *(float*) position->At( 12 ) : 0.0f );
    float stb = ( count > 13 ? *(float*) position->At( 13 ) : 0.0f );
    if( count > 13 )
        SprMngr.PushScissor( (int) stl, (int) stt, (int) str, (int) stb );

    memzero( DrawCritter3dLayers, sizeof( DrawCritter3dLayers ) );
    for( uint i = 0, j = ( layers ? layers->GetSize() : 0 ); i < j && i < LAYERS3D_COUNT; i++ )
        DrawCritter3dLayers[ i ] = *(int*) layers->At( i );

    anim3d->SetDirAngle( 0 );
    anim3d->SetRotation( rx * PI_VALUE / 180.0f, ry * PI_VALUE / 180.0f, rz * PI_VALUE / 180.0f );
    anim3d->SetScale( sx, sy, sz );
    anim3d->SetSpeed( speed );
    anim3d->SetAnimation( anim1, anim2, DrawCritter3dLayers, ANIMATION_PERIOD( (int) ( period * 100.0f ) ) | ANIMATION_NO_SMOOTH );

    SprMngr.Draw3d( (int) x, (int) y, anim3d, COLOR_SCRIPT_SPRITE( color ) );

    if( count > 13 )
        SprMngr.PopScissor();
}

void FOClient::SScriptFunc::Global_PushDrawScissor( int x, int y, int w, int h )
{
    SprMngr.PushScissor( x, y, x + w, y + h );
}

void FOClient::SScriptFunc::Global_PopDrawScissor()
{
    SprMngr.PopScissor();
}

void FOClient::SScriptFunc::Global_ActivateOffscreenSurface( bool force_clear )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );

    if( Self->OffscreenSurfaces.empty() )
    {
        RenderTarget* rt = SprMngr.CreateRenderTarget( false, false, true, 0, 0, false );
        if( !rt )
            SCRIPT_ERROR_R( "Can't create offscreen surface." );

        Self->OffscreenSurfaces.push_back( rt );
    }

    RenderTarget* rt = Self->OffscreenSurfaces.back();
    Self->OffscreenSurfaces.pop_back();
    Self->ActiveOffscreenSurfaces.push_back( rt );

    SprMngr.PushRenderTarget( rt );

    auto it = std::find( Self->DirtyOffscreenSurfaces.begin(), Self->DirtyOffscreenSurfaces.end(), rt );
    if( it != Self->DirtyOffscreenSurfaces.end() || force_clear )
    {
        if( it != Self->DirtyOffscreenSurfaces.end() )
            Self->DirtyOffscreenSurfaces.erase( it );

        SprMngr.ClearCurrentRenderTarget( 0 );
    }

    if( std::find( Self->PreDirtyOffscreenSurfaces.begin(), Self->PreDirtyOffscreenSurfaces.end(), rt ) == Self->PreDirtyOffscreenSurfaces.end() )
        Self->PreDirtyOffscreenSurfaces.push_back( rt );
}

void FOClient::SScriptFunc::Global_PresentOffscreenSurface( int effect_subtype )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( Self->ActiveOffscreenSurfaces.empty() )
        SCRIPT_ERROR_R( "No active offscreen surfaces." );

    RenderTarget* rt = Self->ActiveOffscreenSurfaces.back();
    Self->ActiveOffscreenSurfaces.pop_back();
    Self->OffscreenSurfaces.push_back( rt );

    SprMngr.PopRenderTarget();

    if( effect_subtype < 0 || effect_subtype >= (int) Self->OffscreenEffects.size() || !Self->OffscreenEffects[ effect_subtype ] )
        SCRIPT_ERROR_R( "Invalid effect subtype." );

    rt->DrawEffect = Self->OffscreenEffects[ effect_subtype ];

    SprMngr.DrawRenderTarget( rt, true );
}

void FOClient::SScriptFunc::Global_PresentOffscreenSurfaceExt( int effect_subtype, int x, int y, int w, int h )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( Self->ActiveOffscreenSurfaces.empty() )
        SCRIPT_ERROR_R( "No active offscreen surfaces." );

    RenderTarget* rt = Self->ActiveOffscreenSurfaces.back();
    Self->ActiveOffscreenSurfaces.pop_back();
    Self->OffscreenSurfaces.push_back( rt );

    SprMngr.PopRenderTarget();

    if( effect_subtype < 0 || effect_subtype >= (int) Self->OffscreenEffects.size() || !Self->OffscreenEffects[ effect_subtype ] )
        SCRIPT_ERROR_R( "Invalid effect subtype." );

    rt->DrawEffect = Self->OffscreenEffects[ effect_subtype ];

    Rect from( CLAMP( x, 0, GameOpt.ScreenWidth ), CLAMP( y, 0, GameOpt.ScreenHeight ),
               CLAMP( x + w, 0, GameOpt.ScreenWidth ), CLAMP( y + h, 0, GameOpt.ScreenHeight ) );
    Rect to = from;
    SprMngr.DrawRenderTarget( rt, true, &from, &to );
}

void FOClient::SScriptFunc::Global_PresentOffscreenSurfaceExt2( int effect_subtype, int from_x, int from_y, int from_w, int from_h, int to_x, int to_y, int to_w, int to_h )
{
    if( !Self->CanDrawInScripts )
        SCRIPT_ERROR_R( "You can use this function only in RenderIface event." );
    if( Self->ActiveOffscreenSurfaces.empty() )
        SCRIPT_ERROR_R( "No active offscreen surfaces." );

    RenderTarget* rt = Self->ActiveOffscreenSurfaces.back();
    Self->ActiveOffscreenSurfaces.pop_back();
    Self->OffscreenSurfaces.push_back( rt );

    SprMngr.PopRenderTarget();

    if( effect_subtype < 0 || effect_subtype >= (int) Self->OffscreenEffects.size() || !Self->OffscreenEffects[ effect_subtype ] )
        SCRIPT_ERROR_R( "Invalid effect subtype." );

    rt->DrawEffect = Self->OffscreenEffects[ effect_subtype ];

    Rect from( CLAMP( from_x, 0, GameOpt.ScreenWidth ), CLAMP( from_y, 0, GameOpt.ScreenHeight ),
               CLAMP( from_x + from_w, 0, GameOpt.ScreenWidth ), CLAMP( from_y + from_h, 0, GameOpt.ScreenHeight ) );
    Rect to( CLAMP( to_x, 0, GameOpt.ScreenWidth ), CLAMP( to_y, 0, GameOpt.ScreenHeight ),
             CLAMP( to_x + to_w, 0, GameOpt.ScreenWidth ), CLAMP( to_y + to_h, 0, GameOpt.ScreenHeight ) );
    SprMngr.DrawRenderTarget( rt, true, &from, &to );
}

void FOClient::SScriptFunc::Global_ShowScreen( int screen, CScriptDictionary* params )
{
    if( screen >= SCREEN_LOGIN && screen <= SCREEN_WAIT )
        Self->ShowMainScreen( screen, params );
    else if( screen != SCREEN_NONE )
        Self->ShowScreen( screen, params );
    else
        Self->HideScreen( screen );
}

void FOClient::SScriptFunc::Global_HideScreen( int screen )
{
    Self->HideScreen( screen );
}

bool FOClient::SScriptFunc::Global_GetHexPos( ushort hx, ushort hy, int& x, int& y )
{
    x = y = 0;
    if( Self->HexMngr.IsMapLoaded() && hx < Self->HexMngr.GetWidth() && hy < Self->HexMngr.GetHeight() )
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

bool FOClient::SScriptFunc::Global_GetMonitorHex( int x, int y, ushort& hx, ushort& hy )
{
    int old_x = GameOpt.MouseX;
    int old_y = GameOpt.MouseY;
    GameOpt.MouseX = x;
    GameOpt.MouseY = y;
    ushort hx_ = 0, hy_ = 0;
    bool   result = Self->HexMngr.GetHexPixel( x, y, hx_, hy_ );
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

Item* FOClient::SScriptFunc::Global_GetMonitorItem( int x, int y )
{
    bool item_egg;
    return Self->HexMngr.GetItemPixel( x, y, item_egg );
}

CritterCl* FOClient::SScriptFunc::Global_GetMonitorCritter( int x, int y )
{
    return Self->HexMngr.GetCritterPixel( x, y, false );
}

Entity* FOClient::SScriptFunc::Global_GetMonitorEntity( int x, int y )
{
    ItemHex*   item;
    CritterCl* cr;
    Self->HexMngr.GetSmthPixel( x, y, item, cr );
    return item ? (Entity*) item : (Entity*) cr;
}

ushort FOClient::SScriptFunc::Global_GetMapWidth()
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );

    return Self->HexMngr.GetWidth();
}

ushort FOClient::SScriptFunc::Global_GetMapHeight()
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );

    return Self->HexMngr.GetHeight();
}

bool FOClient::SScriptFunc::Global_IsMapHexPassed( ushort hx, ushort hy )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    if( hx >= Self->HexMngr.GetWidth() || hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hex args." );

    return !Self->HexMngr.GetField( hx, hy ).Flags.IsNotPassed;
}

bool FOClient::SScriptFunc::Global_IsMapHexRaked( ushort hx, ushort hy )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R0( "Map is not loaded." );
    if( hx >= Self->HexMngr.GetWidth() || hy >= Self->HexMngr.GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hex args." );

    return !Self->HexMngr.GetField( hx, hy ).Flags.IsNotRaked;
}

bool FOClient::SScriptFunc::Global_SaveScreenshot( string file_path )
{
    SprMngr.SaveTexture( nullptr, _str( file_path ).formatPath(), true );
    return true;
}

bool FOClient::SScriptFunc::Global_SaveText( string file_path, string text )
{
    void* f = FileOpen( _str( file_path ).formatPath(), true );
    if( !f )
        return false;

    if( text.length() > 0 )
        FileWrite( f, text.c_str(), (uint) text.length() );
    FileClose( f );
    return true;
}

void FOClient::SScriptFunc::Global_SetCacheData( string name, const CScriptArray* data )
{
    UCharVec data_vec;
    Script::AssignScriptArrayInVector( data_vec, data );
    Crypt.SetCache( name, data_vec );
}

void FOClient::SScriptFunc::Global_SetCacheDataSize( string name, const CScriptArray* data, uint data_size )
{
    UCharVec data_vec;
    Script::AssignScriptArrayInVector( data_vec, data );
    data_vec.resize( data_size );
    Crypt.SetCache( name, data_vec );
}

CScriptArray* FOClient::SScriptFunc::Global_GetCacheData( string name )
{
    UCharVec data_vec;
    if( !Crypt.GetCache( name, data_vec ) )
        return nullptr;

    CScriptArray* arr = Script::CreateArray( "uint8[]" );
    Script::AppendVectorToArray( data_vec, arr );
    return arr;
}

void FOClient::SScriptFunc::Global_SetCacheDataStr( string name, string str )
{
    Crypt.SetCache( name, str );
}

string FOClient::SScriptFunc::Global_GetCacheDataStr( string name )
{
    return Crypt.GetCache( name );
}

bool FOClient::SScriptFunc::Global_IsCacheData( string name )
{
    return Crypt.IsCache( name );
}

void FOClient::SScriptFunc::Global_EraseCacheData( string name )
{
    Crypt.EraseCache( name );
}

void FOClient::SScriptFunc::Global_SetUserConfig( CScriptArray* key_values )
{
    FileManager cfg_user;
    for( asUINT i = 0; i < key_values->GetSize() - 1; i += 2 )
    {
        string& key = *(string*) key_values->At( i );
        string& value = *(string*) key_values->At( i + 1 );
        cfg_user.SetStr( _str( "{} = {}\n", key, value ) );
    }
    cfg_user.SaveFile( CONFIG_NAME );
}
