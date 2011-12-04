#include "StdAfx.h"
#include "Mapper.h"

void _PreRestore()
{
    FOMapper::Self->HexMngr.PreRestore();
}

void _PostRestore()
{
    FOMapper::Self->HexMngr.PostRestore();
    FOMapper::Self->ChangeGameTime();
}

bool      FOMapper::SpritesCanDraw = false;
FOMapper* FOMapper::Self = NULL;
FOMapper::FOMapper()
{
    Self = this;
    FPS = 0;
    DrawCrExtInfo = 0;
    IsMapperStarted = false;
}

bool FOMapper::Init()
{
    WriteLog( "Mapper initialization...\n" );

    #if defined ( FO_X86 )
    STATIC_ASSERT( sizeof( SpriteInfo ) == 36 );
    STATIC_ASSERT( sizeof( Sprite ) == 116 );
    STATIC_ASSERT( sizeof( GameOptions ) == 1136 );
    #endif

    // Register dll script data
    struct CritterChangeParameter_
    {
        static void CritterChangeParameter( void*, uint ) {} };                           // Dummy
    GameOpt.CritterChangeParameter = &CritterChangeParameter_::CritterChangeParameter;

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
            if( si->Anim3d ) return si->Anim3d->IsIntersect( x, y );
            int sx = sprite_->ScrX - si->Width / 2 + si->OffsX + GameOpt.ScrOx + ( sprite_->OffsX ? *sprite_->OffsX : 0 );
            int sy = sprite_->ScrY - si->Height + si->OffsY + GameOpt.ScrOy + ( sprite_->OffsY ? *sprite_->OffsY : 0 );
            if( !( sprite_ = sprite_->GetIntersected( x - sx, y - sy ) ) ) return false;
            if( check_egg && SprMngr.CompareHexEgg( sprite_->HexX, sprite_->HexY, sprite_->EggType ) && SprMngr.IsEggTransp( x, y ) ) return false;
            return true;
        }
    };
    GameOpt.IsSpriteHit = &IsSpriteHit_::IsSpriteHit;

    struct GetNameByHash_
    {
        static const char* GetNameByHash( uint hash ) { return Str::GetName( hash ); } };
    GameOpt.GetNameByHash = &GetNameByHash_::GetNameByHash;
    struct GetHashByName_
    {
        static uint GetHashByName( const char* name ) { return Str::GetHash( name ); } };
    GameOpt.GetHashByName = &GetHashByName_::GetHashByName;

    // Input
    Keyb::InitKeyb();

    // Options
    GameOpt.ScrollCheck = false;

    // File manager
    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
    FileManager::InitDataFiles( GameOpt.ClientPath.c_str() );
    FileManager::InitDataFiles( ".\\" );

    // Cache
    FileManager::CreateDirectoryTree( FileManager::GetFullPath( "", PT_CACHE ) );
    char cache_name[ MAX_FOPATH ] = { "singleplayer" };
    if( !Singleplayer )
        Str::Format( cache_name, "%s.%u", GameOpt.Host.c_str(), GameOpt.Port );
    FileManager::SetCacheName( cache_name );

    // Sprite manager
    SpriteMngrParams params;
    params.PreRestoreFunc = &_PreRestore;
    params.PostRestoreFunc = &_PostRestore;
    if( !SprMngr.Init( params ) )
        return false;

    // Fonts
    if( !SprMngr.LoadFontFO( FONT_FO, "OldDefault" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_NUM, "Numbers" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_BIG_NUM, "BigNumbers" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_SAND_NUM, "SandNumbers" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_SPECIAL, "Special" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_DEFAULT, "Default" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_THIN, "Thin" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_FAT, "Fat" ) )
        return false;
    if( !SprMngr.LoadFontFO( FONT_BIG, "Big" ) )
        return false;
    SprMngr.SetDefaultFont( FONT_DEFAULT, COLOR_TEXT );
    Effect* font_effect = GraphicLoader::LoadEffect( SprMngr.GetDevice(), "Font_Default.fx", true );
    if( font_effect )
    {
        SprMngr.SetFontEffect( FONT_FO, font_effect );
        SprMngr.SetFontEffect( FONT_NUM, font_effect );
        SprMngr.SetFontEffect( FONT_BIG_NUM, font_effect );
        SprMngr.SetFontEffect( FONT_SAND_NUM, font_effect );
        SprMngr.SetFontEffect( FONT_SPECIAL, font_effect );
        SprMngr.SetFontEffect( FONT_DEFAULT, font_effect );
        SprMngr.SetFontEffect( FONT_THIN, font_effect );
        SprMngr.SetFontEffect( FONT_FAT, font_effect );
        SprMngr.SetFontEffect( FONT_BIG, font_effect );
    }

    // Names
    ConstantsManager::Initialize( PT_DATA );
    FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
    ConstantsManager::Initialize( PT_SERVER_DATA );
    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );

    // Resource manager
    ResMngr.Refresh();

    if( SprMngr.BeginScene( COLOR_XRGB( 100, 100, 100 ) ) )
        SprMngr.EndScene();

    int res = InitIface();
    if( res != 0 )
    {
        WriteLog( "Error<%d>.\n", res );
        return false;
    }

    // Script system
    InitScriptSystem();

    // Server path
    FileManager::SetDataPath( GameOpt.ServerPath.c_str() );

    // Language Packs
    IniParser cfg_mapper;
    cfg_mapper.LoadFile( GetConfigFileName(), PT_MAPPER_ROOT );
    char      server_cfg_name[ MAX_FOPATH ];
    cfg_mapper.GetStr( "ServerName", "FOnlineServer", server_cfg_name );
    Str::Append( server_cfg_name, ".cfg" );

    IniParser cfg_server;
    cfg_server.LoadFile( server_cfg_name, PT_SERVER_ROOT );
    char      lang_name[ MAX_FOTEXT ];
    cfg_server.GetStr( "Language_0", DEFAULT_LANGUAGE, lang_name );
    if( strlen( lang_name ) != 4 )
        Str::Copy( lang_name, DEFAULT_LANGUAGE );
    Str::Lower( lang_name );

    if( !CurLang.Init( lang_name, PT_SERVER_TEXTS ) )
        return false;

    MsgText = &CurLang.Msg[ TEXTMSG_TEXT ];
    MsgDlg = &CurLang.Msg[ TEXTMSG_DLG ];
    MsgItem = &CurLang.Msg[ TEXTMSG_ITEM ];
    MsgGame = &CurLang.Msg[ TEXTMSG_GAME ];
    MsgGM = &CurLang.Msg[ TEXTMSG_GM ];
    MsgCombat = &CurLang.Msg[ TEXTMSG_COMBAT ];
    MsgQuest = &CurLang.Msg[ TEXTMSG_QUEST ];
    MsgHolo = &CurLang.Msg[ TEXTMSG_HOLO ];
    MsgCraft = &CurLang.Msg[ TEXTMSG_CRAFT ];

    // Critter types
    CritType::InitFromFile( NULL );

    // Item manager
    if( !ItemMngr.Init() )
        return false;
    if( !ItemMngr.LoadProtos() )
        return false;

    // Fill default critter parameters
    for( int i = 0; i < MAPOBJ_CRITTER_PARAMS; i++ )
        DefaultCritterParam[ i ] = -1;
    DefaultCritterParam[ 0 ] = ST_DIALOG_ID;
    DefaultCritterParam[ 1 ] = ST_AI_ID;
    DefaultCritterParam[ 2 ] = ST_BAG_ID;
    DefaultCritterParam[ 3 ] = ST_TEAM_ID;
    DefaultCritterParam[ 4 ] = ST_NPC_ROLE;
    DefaultCritterParam[ 5 ] = ST_REPLICATION_TIME;

    // Critters Manager
    if( !CrMngr.Init() )
        return false;
    if( !CrMngr.LoadProtos() )
        return false;

    // Initialize tabs
    CritData* cr_protos = CrMngr.GetAllProtos();
    for( int i = 1; i < MAX_CRIT_PROTOS; i++ )
    {
        CritData* data = &cr_protos[ i ];
        if( data->ProtoId )
        {
            data->BaseType = data->Params[ ST_BASE_CRTYPE ];
            Tabs[ INT_MODE_CRIT ][ DEFAULT_SUB_TAB ].NpcProtos.push_back( data );
            Tabs[ INT_MODE_CRIT ][ CrMngr.ProtosCollectionName[ i ] ].NpcProtos.push_back( data );
        }
    }

    ProtoItemVec item_protos;
    ItemMngr.GetCopyAllProtos( item_protos );
    for( int i = 0, j = item_protos.size(); i < j; i++ )
    {
        ProtoItem& proto = item_protos[ i ];
        Tabs[ INT_MODE_ITEM ][ DEFAULT_SUB_TAB ].ItemProtos.push_back( proto );
        Tabs[ INT_MODE_ITEM ][ proto.CollectionName ].ItemProtos.push_back( proto );
    }

    for( int i = 0; i < TAB_COUNT; i++ )
    {
        if( Tabs[ i ].empty() )
            Tabs[ i ][ DEFAULT_SUB_TAB ].Scroll = 0;
        TabsActive[ i ] = &( *Tabs[ i ].begin() ).second;
    }

    TabsTiles[ INT_MODE_TILE ].TileDirs.push_back( FileManager::GetPath( PT_ART_TILES ) );
    TabsTiles[ INT_MODE_TILE ].TileSubDirs.push_back( true );

    // Initialize tabs scroll and names
    memzero( TabsScroll, sizeof( TabsScroll ) );
    for( int i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++ )
        TabsName[ i ] = "-";
    TabsName[ INT_MODE_ITEM ] = "Item";
    TabsName[ INT_MODE_TILE ] = "Tile";
    TabsName[ INT_MODE_CRIT ] = "Crit";
    TabsName[ INT_MODE_FAST ] = "Fast";
    TabsName[ INT_MODE_IGNORE ] = "Ign";
    TabsName[ INT_MODE_INCONT ] = "Inv";
    TabsName[ INT_MODE_MESS ] = "Msg";
    TabsName[ INT_MODE_LIST ] = "Maps";

    // Restore to client path
    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );

    // Hex manager
    if( !HexMngr.Init() )
        return false;
    HexMngr.ReloadSprites();
    HexMngr.SwitchShowTrack();
    DayTime = 432720;
    ChangeGameTime();
    AnyId = 0x7FFFFFFF;

    if( Str::Substring( CommandLine, "-Map" ) )
    {
        char map_name[ 256 ];
        sscanf( Str::Substring( CommandLine, "-Map" ) + strlen( "-Map" ) + 1, "%s", map_name );

        ProtoMap* pmap = new ProtoMap();
        if( pmap->Init( 0xFFFF, map_name, PT_SERVER_MAPS ) && HexMngr.SetProtoMap( *pmap ) )
        {
            HexMngr.FindSetCenter( pmap->Header.WorkHexX, pmap->Header.WorkHexY );
            CurProtoMap = pmap;
            LoadedProtoMaps.push_back( pmap );
        }
    }

    // Start script
    RunStartScript();

    // Refresh resources after start script executed
    ResMngr.Refresh();
    for( int tab = 0; tab < TAB_COUNT; tab++ )
        RefreshTiles( tab );
    RefreshCurProtos();

    IsMapperStarted = true;
    WriteLog( "Mapper initialization complete.\n" );
    return true;
}

bool FOMapper::IfaceLoadRect( Rect& comp, const char* name )
{
    char res[ 256 ];
    if( !IfaceIni.GetStr( name, "", res ) )
    {
        WriteLog( "Signature<%s> not found.\n", name );
        return false;
    }

    if( sscanf( res, "%d%d%d%d", &comp[ 0 ], &comp[ 1 ], &comp[ 2 ], &comp[ 3 ] ) != 4 )
    {
        comp.Clear();
        WriteLog( "Unable to parse signature<%s>.\n", name );
        return false;
    }

    return true;
}

int FOMapper::InitIface()
{
    WriteLog( "Init interface.\n" );

    IniParser& ini = IfaceIni;
    char       int_file[ 256 ];

    IniParser  cfg;
    cfg.LoadFile( GetConfigFileName(), PT_MAPPER_ROOT );
    cfg.GetStr( "MapperInterface", CFG_DEF_INT_FILE, int_file );

    if( !ini.LoadFile( int_file, PT_MAPPER_DATA ) )
    {
        WriteLog( "File<%s> not found.\n", FileManager::GetFullPath( int_file, PT_MAPPER_DATA ) );
        return __LINE__;
    }

    // Interface
    IntX = ini.GetInt( "IntX", -1 );
    IntY = ini.GetInt( "IntY", -1 );

    IfaceLoadRect( IntWMain, "IntMain" );
    if( IntX == -1 )
        IntX = ( MODE_WIDTH - IntWMain.W() ) / 2;
    if( IntY == -1 )
        IntY = MODE_HEIGHT - IntWMain.H();

    IfaceLoadRect( IntWWork, "IntWork" );
    IfaceLoadRect( IntWHint, "IntHint" );

    IfaceLoadRect( IntBCust[ 0 ], "IntCustom0" );
    IfaceLoadRect( IntBCust[ 1 ], "IntCustom1" );
    IfaceLoadRect( IntBCust[ 2 ], "IntCustom2" );
    IfaceLoadRect( IntBCust[ 3 ], "IntCustom3" );
    IfaceLoadRect( IntBCust[ 4 ], "IntCustom4" );
    IfaceLoadRect( IntBCust[ 5 ], "IntCustom5" );
    IfaceLoadRect( IntBCust[ 6 ], "IntCustom6" );
    IfaceLoadRect( IntBCust[ 7 ], "IntCustom7" );
    IfaceLoadRect( IntBCust[ 8 ], "IntCustom8" );
    IfaceLoadRect( IntBCust[ 9 ], "IntCustom9" );
    IfaceLoadRect( IntBItem, "IntItem" );
    IfaceLoadRect( IntBTile, "IntTile" );
    IfaceLoadRect( IntBCrit, "IntCrit" );
    IfaceLoadRect( IntBFast, "IntFast" );
    IfaceLoadRect( IntBIgnore, "IntIgnore" );
    IfaceLoadRect( IntBInCont, "IntInCont" );
    IfaceLoadRect( IntBMess, "IntMess" );
    IfaceLoadRect( IntBList, "IntList" );
    IfaceLoadRect( IntBScrBack, "IntScrBack" );
    IfaceLoadRect( IntBScrBackFst, "IntScrBackFst" );
    IfaceLoadRect( IntBScrFront, "IntScrFront" );
    IfaceLoadRect( IntBScrFrontFst, "IntScrFrontFst" );

    IfaceLoadRect( IntBShowItem, "IntShowItem" );
    IfaceLoadRect( IntBShowScen, "IntShowScen" );
    IfaceLoadRect( IntBShowWall, "IntShowWall" );
    IfaceLoadRect( IntBShowCrit, "IntShowCrit" );
    IfaceLoadRect( IntBShowTile, "IntShowTile" );
    IfaceLoadRect( IntBShowRoof, "IntShowRoof" );
    IfaceLoadRect( IntBShowFast, "IntShowFast" );

    IfaceLoadRect( IntBSelectItem, "IntSelectItem" );
    IfaceLoadRect( IntBSelectScen, "IntSelectScen" );
    IfaceLoadRect( IntBSelectWall, "IntSelectWall" );
    IfaceLoadRect( IntBSelectCrit, "IntSelectCrit" );
    IfaceLoadRect( IntBSelectTile, "IntSelectTile" );
    IfaceLoadRect( IntBSelectRoof, "IntSelectRoof" );

    IfaceLoadRect( SubTabsRect, "SubTabs" );

    IntVisible = true;
    IntFix = true;
    IntMode = INT_MODE_MESS;
    IntVectX = 0;
    IntVectY = 0;
    SelectType = SELECT_TYPE_NEW;

    SubTabsActive = false;
    SubTabsActiveTab = 0;
    SubTabsPic = NULL;
    SubTabsX = 0;
    SubTabsY = 0;

    CurProtoMap = NULL;
    CurItemProtos = NULL;
    CurTileHashes = NULL;
    CurTileNames = NULL;
    CurNpcProtos = NULL;
    CurProtoScroll = NULL;

    ProtoWidth = ini.GetInt( "ProtoWidth", 50 );
    ProtosOnScreen = ( IntWWork[ 2 ] - IntWWork[ 0 ] ) / ProtoWidth;
    memzero( TabIndex, sizeof( TabIndex ) );
    NpcDir = 3;
    ListScroll = 0;

    CurMode = CUR_MODE_DEF;

    SelectHX1 = 0;
    SelectHY1 = 0;
    SelectHX2 = 0;
    SelectHY2 = 0;
    SelectX = 0;
    SelectY = 0;

    InContScroll = 0;
    InContObject = NULL;

    DrawRoof = false;
    TileLayer = 0;

    IsSelectItem = true;
    IsSelectScen = true;
    IsSelectWall = true;
    IsSelectCrit = true;
    IsSelectTile = false;
    IsSelectRoof = false;

    GameOpt.ShowRoof = false;

    // Object
    IfaceLoadRect( ObjWMain, "ObjMain" );
    IfaceLoadRect( ObjWWork, "ObjWork" );
    IfaceLoadRect( ObjBToAll, "ObjToAll" );

    ObjX = 0;
    ObjY = 0;
    ItemVectX = 0;
    ItemVectY = 0;
    ObjCurLine = 0;
    ObjVisible = true;
    ObjFix = false;
    ObjToAll = false;

    // Console
    ConsolePicX = ini.GetInt( "ConsolePicX", 0 );
    ConsolePicY = ini.GetInt( "ConsolePicY", 0 );
    ConsoleTextX = ini.GetInt( "ConsoleTextX", 0 );
    ConsoleTextY = ini.GetInt( "ConsoleTextY", 0 );

    ConsoleEdit = 0;
    ConsoleLastKey = 0;
    ConsoleKeyTick = 0;
    ConsoleAccelerate = 0;
    ConsoleStr[ 0 ] = 0;
    ConsoleCur = 0;
    ConsoleHistory.clear();
    ConsoleHistoryCur = 0;

    ItemHex::DefaultAnim = SprMngr.LoadAnimation( "art\\items\\reserved.frm", PT_DATA, ANIM_USE_DUMMY | ANIM_FRM_ANIM_PIX );
    CritterCl::DefaultAnim = SprMngr.LoadAnimation( "art\\critters\\reservaa.frm", PT_DATA, ANIM_USE_DUMMY | ANIM_FRM_ANIM_PIX );

    // Messbox
    MessBoxScroll = 0;

    // Cursor
    CurPDef = SprMngr.LoadAnimation( "actarrow.frm", PT_ART_INTRFACE, ANIM_USE_DUMMY );
    CurPHand = SprMngr.LoadAnimation( "hand.frm", PT_ART_INTRFACE, ANIM_USE_DUMMY );

    // Iface
    char f_name[ 1024 ];
    ini.GetStr( "IntMainPic", "error", f_name );
    IntMainPic = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );
    ini.GetStr( "IntTabPic", "error", f_name );
    IntPTab = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );
    ini.GetStr( "IntSelectPic", "error", f_name );
    IntPSelect = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );
    ini.GetStr( "IntShowPic", "error", f_name );
    IntPShow = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );

    // Object
    ini.GetStr( "ObjMainPic", "error", f_name );
    ObjWMainPic = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );
    ini.GetStr( "ObjToAllPicDn", "error", f_name );
    ObjPBToAllDn = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );

    // Sub tabs
    ini.GetStr( "SubTabsPic", "error", f_name );
    SubTabsPic = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );

    // Console
    ini.GetStr( "ConsolePic", "error", f_name );
    ConsolePic = SprMngr.LoadAnimation( f_name, PT_MAPPER_DATA, ANIM_USE_DUMMY );

    WriteLog( "Init interface complete.\n" );
    return 0;
}

void FOMapper::Finish()
{
    WriteLog( "Mapper finish...\n" );
    Keyb::Finish();
    ResMngr.Finish();
    HexMngr.Finish();
    SprMngr.Finish();
    CrMngr.Finish();
    FileManager::EndOfWork();
    FinishScriptSystem();
    WriteLog( "Mapper finish complete.\n" );
}

void FOMapper::ChangeGameTime()
{
    GameOpt.Minute = DayTime % 60;
    GameOpt.Hour = DayTime / 60 % 24;
    uint color = GetColorDay( HexMngr.GetMapDayTime(), HexMngr.GetMapDayColor(), HexMngr.GetMapTime(), NULL );
    SprMngr.SetSpritesColor( COLOR_GAME_RGB( ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF ) );
    HexMngr.RefreshMap();
}

uint FOMapper::AnimLoad( uint name_hash, uchar dir, int res_type )
{
    AnyFrames* anim = ResMngr.GetAnim( name_hash, dir, res_type );
    if( !anim )
        return 0;
    IfaceAnim* ianim = new (nothrow) IfaceAnim( anim, res_type );
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

uint FOMapper::AnimLoad( const char* fname, int path_type, int res_type )
{
    char full_name[ MAX_FOPATH ];
    Str::Copy( full_name, FileManager::GetPath( path_type ) );
    Str::Append( full_name, fname );

    AnyFrames* anim = ResMngr.GetAnim( Str::GetHash( full_name ), 0, res_type );
    if( !anim )
        return 0;
    IfaceAnim* ianim = new (nothrow) IfaceAnim( anim, res_type );
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

uint FOMapper::AnimGetCurSpr( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->Frames->Ind[ Animations[ anim_id ]->CurSpr ];
}

uint FOMapper::AnimGetCurSprCnt( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->CurSpr;
}

uint FOMapper::AnimGetSprCount( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->Frames->CntFrm;
}

AnyFrames* FOMapper::AnimGetFrames( uint anim_id )
{
    if( anim_id >= Animations.size() || !Animations[ anim_id ] )
        return 0;
    return Animations[ anim_id ]->Frames;
}

void FOMapper::AnimRun( uint anim_id, uint flags )
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

void FOMapper::AnimProcess()
{
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
            uint cur_tick = Timer::FastTick();
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

void FOMapper::AnimFree( int res_type )
{
    ResMngr.FreeResources( res_type );
    for( auto it = Animations.begin(), end = Animations.end(); it != end; ++it )
    {
        IfaceAnim* anim = *it;
        if( anim && anim->ResType == res_type )
        {
            delete anim;
            ( *it ) = NULL;
        }
    }
}

void FOMapper::ParseKeyboard()
{
    // Stop processing if window not active
    if( !MainWindow->active() )
    {
        KeyboardEventsLocker.Lock();
        KeyboardEvents.clear();
        KeyboardEventsLocker.Unlock();

        Keyb::Lost();
        IntHold = INT_NONE;
        if( MapperFunctions.InputLost && Script::PrepareContext( MapperFunctions.InputLost, _FUNC_, "Mapper" ) )
            Script::RunPrepared();
        return;
    }

    // Get buffered data
    KeyboardEventsLocker.Lock();
    if( KeyboardEvents.empty() )
    {
        KeyboardEventsLocker.Unlock();
        return;
    }
    IntVec events = KeyboardEvents;
    KeyboardEvents.clear();
    KeyboardEventsLocker.Unlock();

    // Process events
    for( uint i = 0; i < events.size(); i += 2 )
    {
        // Event data
        int event = events[ i ];
        int event_key = events[ i + 1 ];

        // Keys codes mapping
        uchar dikdw = 0;
        uchar dikup = 0;
        if( event == FL_KEYDOWN )
            dikdw = Keyb::MapKey( event_key );
        else if( event == FL_KEYUP )
            dikup = Keyb::MapKey( event_key );

        // Key script event
        bool script_result = false;
        if( dikdw && MapperFunctions.KeyDown && Script::PrepareContext( MapperFunctions.KeyDown, _FUNC_, "Mapper" ) )
        {
            Script::SetArgUChar( dikdw );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( dikup && MapperFunctions.KeyUp && Script::PrepareContext( MapperFunctions.KeyUp, _FUNC_, "Mapper" ) )
        {
            Script::SetArgUChar( dikup );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }

        // Disable keyboard events
        if( script_result || GameOpt.DisableKeyboardEvents )
        {
            if( dikdw == DIK_ESCAPE && Keyb::ShiftDwn )
                GameOpt.Quit = true;
            continue;
        }

        // Keyboard states, to know outside function
        Keyb::KeyPressed[ dikup ] = false;
        Keyb::KeyPressed[ dikdw ] = true;

        // Control keys
        bool try_change_lang = false;
        if( dikdw == DIK_RCONTROL || dikdw == DIK_LCONTROL )
        {
            Keyb::CtrlDwn = true;
            try_change_lang = true;
        }
        else if( dikdw == DIK_LMENU || dikdw == DIK_RMENU )
        {
            Keyb::AltDwn = true;
            try_change_lang = true;
        }
        else if( dikdw == DIK_LSHIFT || dikdw == DIK_RSHIFT )
        {
            Keyb::ShiftDwn = true;
            try_change_lang = true;
        }
        if( dikup == DIK_RCONTROL || dikup == DIK_LCONTROL )
            Keyb::CtrlDwn = false;
        else if( dikup == DIK_LMENU || dikup == DIK_RMENU )
            Keyb::AltDwn = false;
        else if( dikup == DIK_LSHIFT || dikup == DIK_RSHIFT )
            Keyb::ShiftDwn = false;

        // Switch language
        if( try_change_lang )
        {
            // Ctrl + Shift
            if( Keyb::ShiftDwn && Keyb::CtrlDwn && GameOpt.ChangeLang == CHANGE_LANG_CTRL_SHIFT )
                Keyb::Lang = ( Keyb::Lang == LANG_RUS ) ? LANG_ENG : LANG_RUS;
            // Alt + Shift
            if( Keyb::ShiftDwn && Keyb::AltDwn && GameOpt.ChangeLang == CHANGE_LANG_ALT_SHIFT )
                Keyb::Lang = ( Keyb::Lang == LANG_RUS ) ? LANG_ENG : LANG_RUS;
        }

        // Hotkeys
        if( !Keyb::AltDwn && !Keyb::CtrlDwn && !Keyb::ShiftDwn )
        {
            switch( dikdw )
            {
            case DIK_F1:
                GameOpt.ShowItem = !GameOpt.ShowItem;
                HexMngr.RefreshMap();
                break;
            case DIK_F2:
                GameOpt.ShowScen = !GameOpt.ShowScen;
                HexMngr.RefreshMap();
                break;
            case DIK_F3:
                GameOpt.ShowWall = !GameOpt.ShowWall;
                HexMngr.RefreshMap();
                break;
            case DIK_F4:
                GameOpt.ShowCrit = !GameOpt.ShowCrit;
                HexMngr.RefreshMap();
                break;
            case DIK_F5:
                GameOpt.ShowTile = !GameOpt.ShowTile;
                HexMngr.RefreshMap();
                break;
            case DIK_F6:
                GameOpt.ShowFast = !GameOpt.ShowFast;
                HexMngr.RefreshMap();
                break;
            case DIK_F7:
                IntVisible = !IntVisible;
                break;
            case DIK_F8:
                GameOpt.MouseScroll = !GameOpt.MouseScroll;
                break;
            case DIK_F9:
                ObjVisible = !ObjVisible;
                break;
            case DIK_F10:
                HexMngr.SwitchShowHex();
                break;
            case DIK_F11:
                HexMngr.SwitchShowRain();
                break;
            case DIK_F12:
                #ifdef FO_WINDOWS
                ShowWindow( fl_xid( MainWindow ), SW_MINIMIZE );
                #endif
                break;
            case DIK_DELETE:
                SelectDelete();
                break;
            case DIK_ADD:
                if( !ConsoleEdit && SelectedObj.empty() )
                {
                    DayTime += 60;
                    ChangeGameTime();
                }
                break;
            case DIK_SUBTRACT:
                if( !ConsoleEdit && SelectedObj.empty() )
                {
                    DayTime -= 60;
                    ChangeGameTime();
                }
                break;
            case DIK_TAB:
                SelectType = ( SelectType == SELECT_TYPE_OLD ? SELECT_TYPE_NEW : SELECT_TYPE_OLD );
                break;
            default:
                break;
            }
        }

        if( Keyb::ShiftDwn )
        {
            switch( dikdw )
            {
            case DIK_F7:
                IntFix = !IntFix;
                break;
            case DIK_F9:
                ObjFix = !ObjFix;
                break;
            case DIK_F11:
                SprMngr.SaveSufaces();
                break;
            case DIK_ESCAPE:
                ExitProcess( 0 );
                break;
            case DIK_ADD:
                if( !ConsoleEdit && SelectedObj.empty() )
                {
                    DayTime += 1;
                    ChangeGameTime();
                }
                break;
            case DIK_SUBTRACT:
                if( !ConsoleEdit && SelectedObj.empty() )
                {
                    DayTime -= 1;
                    ChangeGameTime();
                }
                break;
            case DIK_0:
            case DIK_NUMPAD0:
                TileLayer = 0;
                break;
            case DIK_1:
            case DIK_NUMPAD1:
                TileLayer = 1;
                break;
            case DIK_2:
            case DIK_NUMPAD2:
                TileLayer = 2;
                break;
            case DIK_3:
            case DIK_NUMPAD3:
                TileLayer = 3;
                break;
            case DIK_4:
            case DIK_NUMPAD4:
                TileLayer = 4;
                break;
            default:
                break;
            }
        }

        if( Keyb::CtrlDwn )
        {
            switch( dikdw )
            {
            case DIK_X:
                BufferCut();
                break;
            case DIK_C:
                BufferCopy();
                break;
            case DIK_V:
                BufferPaste( 50, 50 );
                break;
            case DIK_A:
                SelectAll();
                break;
            case DIK_S:
                GameOpt.ScrollCheck = !GameOpt.ScrollCheck;
                break;
            case DIK_B:
                HexMngr.MarkPassedHexes();
                break;
            case DIK_Q:
                GameOpt.ShowCorners = !GameOpt.ShowCorners;
                break;
            case DIK_W:
                GameOpt.ShowSpriteCuts = !GameOpt.ShowSpriteCuts;
                break;
            case DIK_E:
                GameOpt.ShowDrawOrder = !GameOpt.ShowDrawOrder;
                break;
            case DIK_M:
                DrawCrExtInfo++;
                if( DrawCrExtInfo > DRAW_CR_INFO_MAX )
                    DrawCrExtInfo = 0;
                break;
            case DIK_L:
                SaveLogFile();
                break;
            default:
                break;
            }
        }

        // Switch fullscreen
        if( Keyb::AltDwn && dikdw == DIK_RETURN )
        {
            #ifndef FO_D3D
            static int x, y, w, h, valid = 0;
            if( !GameOpt.FullScreen )
            {
                int sx, sy, sw, sh;
                Fl::screen_xywh( sx, sy, sw, sh );
                x = MainWindow->x();
                y = MainWindow->y();
                w = MainWindow->w();
                h = MainWindow->h();
                valid = 1;
                MainWindow->border( 0 );
                MainWindow->size( sw, sh );
                MainWindow->position( 0, 0 );
                GameOpt.FullScreen = true;
            }
            else
            {
                MainWindow->border( 1 );
                if( valid )
                {
                    MainWindow->size( w, h );
                    MainWindow->position( x, y );
                }
                else
                {
                    MainWindow->size( MODE_WIDTH, MODE_HEIGHT );
                    MainWindow->position( ( Fl::w() - MODE_WIDTH ) / 2, ( Fl::h() - MODE_HEIGHT ) / 2 );
                }
                // MainWindow->size_range( 100, 100 );
                GameOpt.FullScreen = false;
            }
            #endif
            continue;
        }

        // Key down
        if( dikdw )
        {
            ConsoleKeyDown( dikdw );

            if( !ConsoleEdit )
            {
                switch( dikdw )
                {
                case DIK_LEFT:
                    GameOpt.ScrollKeybLeft = true;
                    break;
                case DIK_RIGHT:
                    GameOpt.ScrollKeybRight = true;
                    break;
                case DIK_UP:
                    GameOpt.ScrollKeybUp = true;
                    break;
                case DIK_DOWN:
                    GameOpt.ScrollKeybDown = true;
                    break;
                default:
                    break;
                }

                ObjKeyDown( dikdw );
            }
        }

        // Key up
        if( dikup )
        {
            ConsoleKeyUp( dikup );

            switch( dikup )
            {
            case DIK_LEFT:
                GameOpt.ScrollKeybLeft = false;
                break;
            case DIK_RIGHT:
                GameOpt.ScrollKeybRight = false;
                break;
            case DIK_UP:
                GameOpt.ScrollKeybUp = false;
                break;
            case DIK_DOWN:
                GameOpt.ScrollKeybDown = false;
                break;
            default:
                break;
            }
        }
    }
}

#define MOUSE_CLICK_LEFT          ( 0 )
#define MOUSE_CLICK_RIGHT         ( 1 )
#define MOUSE_CLICK_MIDDLE        ( 2 )
#define MOUSE_CLICK_WHEEL_UP      ( 3 )
#define MOUSE_CLICK_WHEEL_DOWN    ( 4 )
#define MOUSE_CLICK_EXT0          ( 5 )
#define MOUSE_CLICK_EXT1          ( 6 )
#define MOUSE_CLICK_EXT2          ( 7 )
#define MOUSE_CLICK_EXT3          ( 8 )
#define MOUSE_CLICK_EXT4          ( 9 )
void FOMapper::ParseMouse()
{
    // Mouse position
    int mx = 0, my = 0;
    Fl::get_mouse( mx, my );
    #ifdef FO_D3D
    GameOpt.MouseX = mx - ( !GameOpt.FullScreen ? MainWindow->x() : 0 );
    GameOpt.MouseY = my - ( !GameOpt.FullScreen ? MainWindow->y() : 0 );
    #else
    GameOpt.MouseX = mx - MainWindow->x();
    GameOpt.MouseY = my - MainWindow->y();
    #endif
    GameOpt.MouseX = CLAMP( GameOpt.MouseX, 0, MODE_WIDTH - 1 );
    GameOpt.MouseY = CLAMP( GameOpt.MouseY, 0, MODE_HEIGHT - 1 );

    // Stop processing if window not active
    if( !MainWindow->active() )
    {
        MouseEventsLocker.Lock();
        MouseEvents.clear();
        MouseEventsLocker.Unlock();

        Keyb::Lost();
        IntHold = INT_NONE;
        if( MapperFunctions.InputLost && Script::PrepareContext( MapperFunctions.InputLost, _FUNC_, "Mapper" ) )
            Script::RunPrepared();
        return;
    }

    // Mouse move
    static int old_cur_x = GameOpt.MouseX;
    static int old_cur_y = GameOpt.MouseY;

    if( old_cur_x != GameOpt.MouseX || old_cur_y != GameOpt.MouseY )
    {
        old_cur_x = GameOpt.MouseX;
        old_cur_y = GameOpt.MouseY;

        IntMouseMove();

        if( MapperFunctions.MouseMove && Script::PrepareContext( MapperFunctions.MouseMove, _FUNC_, "Mapper" ) )
        {
            Script::SetArgUInt( GameOpt.MouseX );
            Script::SetArgUInt( GameOpt.MouseY );
            Script::RunPrepared();
        }
    }

    // Mouse Scroll
    if( GameOpt.MouseScroll )
    {
        if( GameOpt.MouseX >= MODE_WIDTH - 1 )
            GameOpt.ScrollMouseRight = true;
        else
            GameOpt.ScrollMouseRight = false;

        if( GameOpt.MouseX <= 0 )
            GameOpt.ScrollMouseLeft = true;
        else
            GameOpt.ScrollMouseLeft = false;

        if( GameOpt.MouseY >= MODE_HEIGHT - 1 )
            GameOpt.ScrollMouseDown = true;
        else
            GameOpt.ScrollMouseDown = false;

        if( GameOpt.MouseY <= 0 )
            GameOpt.ScrollMouseUp = true;
        else
            GameOpt.ScrollMouseUp = false;
    }

    // Get buffered data
    MouseEventsLocker.Lock();
    if( MouseEvents.empty() )
    {
        MouseEventsLocker.Unlock();
        return;
    }
    IntVec events = MouseEvents;
    MouseEvents.clear();
    MouseEventsLocker.Unlock();

    // Process events
    for( uint i = 0; i < events.size(); i += 3 )
    {
        int event = events[ i ];
        int event_button = events[ i + 1 ];
        int event_dy = -events[ i + 2 ];

        // Scripts
        bool script_result = false;
        if( event == FL_MOUSEWHEEL && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( event_dy > 0 ? MOUSE_CLICK_WHEEL_UP : MOUSE_CLICK_WHEEL_DOWN );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_LEFT_MOUSE && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_LEFT );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_LEFT_MOUSE && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_LEFT );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_RIGHT_MOUSE && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_RIGHT );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_RIGHT_MOUSE && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_RIGHT );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_MIDDLE_MOUSE && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_MIDDLE );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_MIDDLE_MOUSE && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_MIDDLE );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_BUTTON( 1 ) && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT0 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_BUTTON( 1 ) && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT0 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_BUTTON( 2 ) && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT1 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_BUTTON( 2 ) && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT1 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_BUTTON( 3 ) && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT2 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_BUTTON( 3 ) && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT2 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_BUTTON( 4 ) && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT3 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_BUTTON( 4 ) && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT3 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_PUSH && event_button == FL_BUTTON( 5 ) && Script::PrepareContext( MapperFunctions.MouseDown, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT4 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }
        if( event == FL_RELEASE && event_button == FL_BUTTON( 5 ) && Script::PrepareContext( MapperFunctions.MouseUp, _FUNC_, "Game" ) )
        {
            Script::SetArgUInt( MOUSE_CLICK_EXT4 );
            if( Script::RunPrepared() )
                script_result = Script::GetReturnedBool();
        }

        if( script_result || GameOpt.DisableMouseEvents )
            continue;

        // Wheel
        if( event == FL_MOUSEWHEEL )
        {
            if( IntVisible && SubTabsActive && IsCurInRect( SubTabsRect, SubTabsX, SubTabsY ) )
            {
                int step = 4;
                if( Keyb::ShiftDwn )
                    step = 8;
                else if( Keyb::CtrlDwn )
                    step = 20;
                else if( Keyb::AltDwn )
                    step = 50;

                int data = event_dy;
                if( data > 0 )
                    TabsScroll[ SubTabsActiveTab ] += step;
                else
                    TabsScroll[ SubTabsActiveTab ] -= step;
                if( TabsScroll[ SubTabsActiveTab ] < 0 )
                    TabsScroll[ SubTabsActiveTab ] = 0;
            }
            else if( IntVisible && IsCurInRect( IntWWork, IntX, IntY ) && ( IsObjectMode() || IsTileMode() || IsCritMode() ) )
            {
                int step = 1;
                if( Keyb::ShiftDwn )
                    step = ProtosOnScreen;
                else if( Keyb::CtrlDwn )
                    step = 100;
                else if( Keyb::AltDwn )
                    step = 1000;

                int data = event_dy;
                if( data > 0 )
                {
                    if( IsObjectMode() || IsTileMode() || IsCritMode() )
                    {
                        ( *CurProtoScroll ) -= step;
                        if( *CurProtoScroll < 0 )
                            *CurProtoScroll = 0;
                    }
                    else if( IntMode == INT_MODE_INCONT )
                    {
                        InContScroll -= step;
                        if( InContScroll < 0 )
                            InContScroll = 0;
                    }
                    else if( IntMode == INT_MODE_LIST )
                    {
                        ListScroll -= step;
                        if( ListScroll < 0 )
                            ListScroll = 0;
                    }
                }
                else
                {
                    if( IsObjectMode() && ( *CurItemProtos ).size() )
                    {
                        ( *CurProtoScroll ) += step;
                        if( *CurProtoScroll >= (int) ( *CurItemProtos ).size() )
                            *CurProtoScroll = (int) ( *CurItemProtos ).size() - 1;
                    }
                    else if( IsTileMode() && CurTileHashes->size() )
                    {
                        ( *CurProtoScroll ) += step;
                        if( *CurProtoScroll >= (int) CurTileHashes->size() )
                            *CurProtoScroll = (int) CurTileHashes->size() - 1;
                    }
                    else if( IsCritMode() && CurNpcProtos->size() )
                    {
                        ( *CurProtoScroll ) += step;
                        if( *CurProtoScroll >= (int) CurNpcProtos->size() )
                            *CurProtoScroll = (int) CurNpcProtos->size() - 1;
                    }
                    else if( IntMode == INT_MODE_INCONT )
                        InContScroll += step;
                    else if( IntMode == INT_MODE_LIST )
                        ListScroll += step;
                }
            }
            else
            {
                if( event_dy )
                    HexMngr.ChangeZoom( event_dy > 0 ? -1 : 1 );
            }
            continue;
        }

        // Middle down
        if( event == FL_PUSH && event_button == FL_MIDDLE_MOUSE )
        {
            CurMMouseDown();
            continue;
        }

        // Left Button Down
        if( event == FL_PUSH && event_button == FL_LEFT_MOUSE )
        {
            IntLMouseDown();
            continue;
        }

        // Left Button Up
        if( event == FL_RELEASE && event_button == FL_LEFT_MOUSE )
        {
            IntLMouseUp();
            continue;
        }

        // Right Button Up
        if( event == FL_RELEASE && event_button == FL_RIGHT_MOUSE )
        {
            CurRMouseUp();
            continue;
        }
    }
}

void FOMapper::MainLoop()
{
    // Fixed FPS
    double start_loop = Timer::AccurateTick();

    // FPS counter
    static uint   last_call = Timer::FastTick();
    static ushort call_cnt = 0;

    if( ( Timer::FastTick() - last_call ) >= 1000 )
    {
        FPS = call_cnt;
        call_cnt = 0;
        last_call = Timer::FastTick();
    }
    else
    {
        call_cnt++;
    }

    // Script loop
    static uint next_call = 0;
    if( MapperFunctions.Loop && Timer::FastTick() >= next_call )
    {
        uint wait_tick = 60000;
        if( Script::PrepareContext( MapperFunctions.Loop, _FUNC_, "Mapper" ) && Script::RunPrepared() )
            wait_tick = Script::GetReturnedUInt();
        next_call = Timer::FastTick() + wait_tick;
    }

    // Input
    ConsoleProcess();
    ParseKeyboard();
    ParseMouse();

    // Process
    AnimProcess();

    if( HexMngr.IsMapLoaded() )
    {
        for( auto it = HexMngr.GetCritters().begin(), end = HexMngr.GetCritters().end(); it != end; ++it )
        {
            CritterCl* cr = ( *it ).second;
            cr->Process();

            //	if(cr->IsNeedReSet())
            //	{
            //		HexMngr.RemoveCrit(cr);
            //		HexMngr.SetCrit(cr);
            //		cr->ReSetOk();
            //	}

            if( cr->IsNeedMove() )
            {
                bool       err_move = ( ( !cr->IsRunning && !CritType::IsCanWalk( cr->GetCrType() ) ) || ( cr->IsRunning && !CritType::IsCanRun( cr->GetCrType() ) ) );
                ushort     old_hx = cr->GetHexX();
                ushort     old_hy = cr->GetHexY();
                MapObject* mobj = FindMapObject( old_hx, old_hy, MAP_OBJECT_CRITTER, cr->Flags, false );
                if( !err_move && mobj && HexMngr.TransitCritter( cr, cr->MoveSteps[ 0 ].first, cr->MoveSteps[ 0 ].second, true, false ) )
                {
                    cr->MoveSteps.erase( cr->MoveSteps.begin() );
                    mobj->MapX = cr->GetHexX();
                    mobj->MapY = cr->GetHexY();
                    mobj->Dir = cr->GetDir();

                    // Move inventory items
                    for( uint i = 0, j = (uint) CurProtoMap->MObjects.size(); i < j; i++ )
                    {
                        MapObject* mo = CurProtoMap->MObjects[ i ];
                        if( mo->ContainerUID && mo->ContainerUID == mobj->UID )
                        {
                            mo->MapX = mobj->MapX;
                            mo->MapY = mobj->MapY;
                        }
                    }
                }
                else
                {
                    cr->MoveSteps.clear();
                }

                HexMngr.RebuildLight();
            }
        }

        HexMngr.Scroll();
        HexMngr.ProcessItems();
        HexMngr.ProcessRain();
    }

    // Render
    if( !SprMngr.BeginScene( COLOR_XRGB( 100, 100, 100 ) ) )
    {
        Sleep( 100 );
        return;
    }

    DrawIfaceLayer( 0 );
    if( HexMngr.IsMapLoaded() )
    {
        HexMngr.DrawMap();

        // Texts on heads
        if( DrawCrExtInfo )
        {
            for( auto it = HexMngr.GetCritters().begin(), end = HexMngr.GetCritters().end(); it != end; ++it )
            {
                CritterCl* cr = ( *it ).second;
                if( cr->SprDrawValid )
                {
                    MapObject* mobj = FindMapObject( cr->GetHexX(), cr->GetHexY(), MAP_OBJECT_CRITTER, cr->Flags, false );
                    CritData*  pnpc = CrMngr.GetProto( mobj->ProtoId );
                    if( !mobj || !pnpc )
                        continue;
                    char str[ 512 ] = { 0 };

                    if( DrawCrExtInfo == 1 )
                        Str::Format( str, "|0xffaabbcc ProtoId...%u\n|0xffff1122 DialogId...%u\n|0xff4433ff BagId...%u\n|0xff55ff77 TeamId...%u\n", mobj->ProtoId, cr->Params[ ST_DIALOG_ID ], cr->Params[ ST_BAG_ID ], cr->Params[ ST_TEAM_ID ] );
                    else if( DrawCrExtInfo == 2 )
                        Str::Format( str, "|0xff11ff22 NpcRole...%u\n|0xffccaabb AiPacket...%u (%u)\n|0xffff00ff RespawnTime...%d\n", cr->Params[ ST_NPC_ROLE ], cr->Params[ ST_AI_ID ], pnpc->Params[ ST_AI_ID ], cr->Params[ ST_REPLICATION_TIME ] );
                    else if( DrawCrExtInfo == 3 )
                        Str::Format( str, "|0xff00ff00 ScriptName...%s\n|0xffff0000 FuncName...%s\n", mobj->ScriptName, mobj->FuncName );

                    cr->SetText( str, COLOR_TEXT_WHITE, 60000000 );
                    cr->DrawTextOnHead();
                }
            }
        }

        // Texts on map
        uint tick = Timer::FastTick();
        for( auto it = GameMapTexts.begin(); it != GameMapTexts.end();)
        {
            MapText& t = ( *it );
            if( tick >= t.StartTick + t.Tick )
                it = GameMapTexts.erase( it );
            else
            {
                int    procent = Procent( t.Tick, tick - t.StartTick );
                Rect   r = t.Pos.Interpolate( t.EndPos, procent );
                Field& f = HexMngr.GetField( t.HexX, t.HexY );
                int    x = (int) ( ( f.ScrX + HEX_W / 2 + GameOpt.ScrOx ) / GameOpt.SpritesZoom - 100.0f - (float) ( t.Pos.L - r.L ) );
                int    y = (int) ( ( f.ScrY + HEX_LINE_H / 2 - t.Pos.H() - ( t.Pos.T - r.T ) + GameOpt.ScrOy ) / GameOpt.SpritesZoom - 70.0f );
                uint   color = t.Color;
                if( t.Fade )
                    color = ( color ^ 0xFF000000 ) | ( ( 0xFF * ( 100 - procent ) / 100 ) << 24 );
                SprMngr.DrawStr( Rect( x, y, x + 200, y + 70 ), t.Text.c_str(), FT_CENTERX | FT_BOTTOM | FT_BORDERED, color );
                it++;
            }
        }
    }

    // Iface
    DrawIfaceLayer( 1 );
    IntDraw();
    DrawIfaceLayer( 2 );
    ConsoleDraw();
    DrawIfaceLayer( 3 );
    ObjDraw();
    DrawIfaceLayer( 4 );
    CurDraw();
    DrawIfaceLayer( 5 );
    SprMngr.EndScene();

    Script::CollectGarbage( true );

    // Fixed FPS
    if( !GameOpt.VSync && GameOpt.FixedFPS > 0 )
    {
        static double balance = 0.0;
        double        elapsed = Timer::AccurateTick() - start_loop;
        double        need_elapsed = 1000.0 / (double) GameOpt.FixedFPS;
        if( need_elapsed > elapsed )
        {
            double sleep = need_elapsed - elapsed + balance;
            balance = fmod ( sleep, 1.0 );
            Sleep( (uint) floor( sleep) );
        }
    }
}

void FOMapper::RefreshTiles( int tab )
{
    const char* formats[] =
    {
        "frm", "fofrm", "bmp", "dds", "dib", "hdr", "jpg",
        "jpeg", "pfm", "png", "tga", "spr", "til", "zar", "art"
    };
    size_t      formats_count = sizeof( formats ) / sizeof( formats[ 0 ] );

    // Clear old tile names
    for( auto it = Tabs[ tab ].begin(); it != Tabs[ tab ].end();)
    {
        SubTab& stab = ( *it ).second;
        if( stab.TileNames.size() )
        {
            if( TabsActive[ tab ] == &stab )
                TabsActive[ tab ] = NULL;
            Tabs[ tab ].erase( it++ );
        }
        else
            ++it;
    }

    // Find names
    TileTab& ttab = TabsTiles[ tab ];
    if( ttab.TileDirs.empty() )
        return;

    Tabs[ tab ].clear();
    Tabs[ tab ][ DEFAULT_SUB_TAB ].Index = 0; // Add default

    StrUIntMap PathIndex;

    for( uint t = 0, tt = (uint) ttab.TileDirs.size(); t < tt; t++ )
    {
        string& path = ttab.TileDirs[ t ];
        bool    include_subdirs = ttab.TileSubDirs[ t ];

        StrVec  tiles;
        FileManager::GetFolderFileNames( path.c_str(), include_subdirs, NULL, tiles );
        FileManager::GetDatsFileNames( path.c_str(), include_subdirs, NULL, tiles );

        struct StrComparator_
        {
            static bool StrComparator( const string& left, const string& right )
            {
                for( auto lit = left.begin(), rit = right.begin(); lit != left.end() && rit != right.end(); ++lit, ++rit )
                {
                    int lc = tolower( *lit );
                    int rc = tolower( *rit );
                    if( lc < rc ) return true;
                    else if( lc > rc ) return false;
                }
                return left.size() < right.size();
            }
        };
        std::sort( tiles.begin(), tiles.end(), StrComparator_::StrComparator );

        for( auto it = tiles.begin(), end = tiles.end(); it != end; ++it )
        {
            const string& fname = *it;
            const char*   ext = FileManager::GetExtension( fname.c_str() );
            if( !ext )
                continue;

            // Check format availability
            bool format_aviable = false;
            for( uint i = 0; i < formats_count; i++ )
            {
                if( Str::CompareCase( formats[ i ], ext ) )
                {
                    format_aviable = true;
                    break;
                }
            }
            if( !format_aviable )
                format_aviable = GraphicLoader::IsExtensionSupported( ext );

            if( format_aviable )
            {
                // Make primary collection name
                char path_[ MAX_FOPATH ];
                FileManager::ExtractPath( fname.c_str(), path_ );
                if( !path_[ 0 ] )
                    Str::Copy( path_, "root" );
                uint path_index = PathIndex[ path_ ];
                if( !path_index )
                {
                    path_index = (uint) PathIndex.size();
                    PathIndex[ path_ ] = path_index;
                }
                string collection_name = Str::FormatBuf( "%03d - %s", path_index, path_ );

                // Make secondary collection name
                string collection_name_ex;
                if( GameOpt.SplitTilesCollection )
                {
                    size_t pos = fname.find_last_of( '\\' );
                    if( pos == string::npos )
                        pos = 0;
                    else
                        pos++;
                    for( uint i = (uint) pos, j = (uint) fname.size(); i < j; i++ )
                    {
                        if( fname[ i ] >= '0' && fname[ i ] <= '9' )
                        {
                            if( i - pos )
                            {
                                collection_name_ex += collection_name;
                                collection_name_ex += fname.substr( pos, i - pos );
                            }
                            break;
                        }
                    }
                    if( !collection_name_ex.length() )
                    {
                        collection_name_ex += collection_name;
                        collection_name_ex += "<other>";
                    }
                }

                // Write tile
                uint hash = Str::GetHash( fname.c_str() );
                Tabs[ tab ][ DEFAULT_SUB_TAB ].TileHashes.push_back( hash );
                Tabs[ tab ][ DEFAULT_SUB_TAB ].TileNames.push_back( fname );
                Tabs[ tab ][ collection_name ].TileHashes.push_back( hash );
                Tabs[ tab ][ collection_name ].TileNames.push_back( fname );
                Tabs[ tab ][ collection_name_ex ].TileHashes.push_back( hash );
                Tabs[ tab ][ collection_name_ex ].TileNames.push_back( fname );
            }
        }
    }

    // Set default active tab
    TabsActive[ tab ] = &( *Tabs[ tab ].begin() ).second;
}

void FOMapper::IntDraw()
{
    if( !IntVisible )
        return;

    SprMngr.DrawSprite( IntMainPic, IntX, IntY );

    switch( IntMode )
    {
    case INT_MODE_CUSTOM0:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 0 ][ 0 ] + IntX, IntBCust[ 0 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM1:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 1 ][ 0 ] + IntX, IntBCust[ 1 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM2:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 2 ][ 0 ] + IntX, IntBCust[ 2 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM3:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 3 ][ 0 ] + IntX, IntBCust[ 3 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM4:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 4 ][ 0 ] + IntX, IntBCust[ 4 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM5:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 5 ][ 0 ] + IntX, IntBCust[ 5 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM6:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 6 ][ 0 ] + IntX, IntBCust[ 6 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM7:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 7 ][ 0 ] + IntX, IntBCust[ 7 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM8:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 8 ][ 0 ] + IntX, IntBCust[ 8 ][ 1 ] + IntY );
        break;
    case INT_MODE_CUSTOM9:
        SprMngr.DrawSprite( IntPTab, IntBCust[ 9 ][ 0 ] + IntX, IntBCust[ 9 ][ 1 ] + IntY );
        break;
    case INT_MODE_ITEM:
        SprMngr.DrawSprite( IntPTab, IntBItem[ 0 ] + IntX, IntBItem[ 1 ] + IntY );
        break;
    case INT_MODE_TILE:
        SprMngr.DrawSprite( IntPTab, IntBTile[ 0 ] + IntX, IntBTile[ 1 ] + IntY );
        break;
    case INT_MODE_CRIT:
        SprMngr.DrawSprite( IntPTab, IntBCrit[ 0 ] + IntX, IntBCrit[ 1 ] + IntY );
        break;
    case INT_MODE_FAST:
        SprMngr.DrawSprite( IntPTab, IntBFast[ 0 ] + IntX, IntBFast[ 1 ] + IntY );
        break;
    case INT_MODE_IGNORE:
        SprMngr.DrawSprite( IntPTab, IntBIgnore[ 0 ] + IntX, IntBIgnore[ 1 ] + IntY );
        break;
    case INT_MODE_INCONT:
        SprMngr.DrawSprite( IntPTab, IntBInCont[ 0 ] + IntX, IntBInCont[ 1 ] + IntY );
        break;
    case INT_MODE_MESS:
        SprMngr.DrawSprite( IntPTab, IntBMess[ 0 ] + IntX, IntBMess[ 1 ] + IntY );
        break;
    case INT_MODE_LIST:
        SprMngr.DrawSprite( IntPTab, IntBList[ 0 ] + IntX, IntBList[ 1 ] + IntY );
        break;
    default:
        break;
    }

    for( int i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++ )
        SprMngr.DrawStr( Rect( IntBCust[ i ], IntX, IntY ), TabsName[ INT_MODE_CUSTOM0 + i ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBItem, IntX, IntY ), TabsName[ INT_MODE_ITEM ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBTile, IntX, IntY ), TabsName[ INT_MODE_TILE ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBCrit, IntX, IntY ), TabsName[ INT_MODE_CRIT ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBFast, IntX, IntY ), TabsName[ INT_MODE_FAST ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBIgnore, IntX, IntY ), TabsName[ INT_MODE_IGNORE ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBInCont, IntX, IntY ), TabsName[ INT_MODE_INCONT ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBMess, IntX, IntY ), TabsName[ INT_MODE_MESS ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    SprMngr.DrawStr( Rect( IntBList, IntX, IntY ), TabsName[ INT_MODE_LIST ].c_str(), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );

    if( GameOpt.ShowItem )
        SprMngr.DrawSprite( IntPShow, IntBShowItem[ 0 ] + IntX, IntBShowItem[ 1 ] + IntY );
    if( GameOpt.ShowScen )
        SprMngr.DrawSprite( IntPShow, IntBShowScen[ 0 ] + IntX, IntBShowScen[ 1 ] + IntY );
    if( GameOpt.ShowWall )
        SprMngr.DrawSprite( IntPShow, IntBShowWall[ 0 ] + IntX, IntBShowWall[ 1 ] + IntY );
    if( GameOpt.ShowCrit )
        SprMngr.DrawSprite( IntPShow, IntBShowCrit[ 0 ] + IntX, IntBShowCrit[ 1 ] + IntY );
    if( GameOpt.ShowTile )
        SprMngr.DrawSprite( IntPShow, IntBShowTile[ 0 ] + IntX, IntBShowTile[ 1 ] + IntY );
    if( GameOpt.ShowRoof )
        SprMngr.DrawSprite( IntPShow, IntBShowRoof[ 0 ] + IntX, IntBShowRoof[ 1 ] + IntY );
    if( GameOpt.ShowFast )
        SprMngr.DrawSprite( IntPShow, IntBShowFast[ 0 ] + IntX, IntBShowFast[ 1 ] + IntY );

    if( IsSelectItem )
        SprMngr.DrawSprite( IntPSelect, IntBSelectItem[ 0 ] + IntX, IntBSelectItem[ 1 ] + IntY );
    if( IsSelectScen )
        SprMngr.DrawSprite( IntPSelect, IntBSelectScen[ 0 ] + IntX, IntBSelectScen[ 1 ] + IntY );
    if( IsSelectWall )
        SprMngr.DrawSprite( IntPSelect, IntBSelectWall[ 0 ] + IntX, IntBSelectWall[ 1 ] + IntY );
    if( IsSelectCrit )
        SprMngr.DrawSprite( IntPSelect, IntBSelectCrit[ 0 ] + IntX, IntBSelectCrit[ 1 ] + IntY );
    if( IsSelectTile )
        SprMngr.DrawSprite( IntPSelect, IntBSelectTile[ 0 ] + IntX, IntBSelectTile[ 1 ] + IntY );
    if( IsSelectRoof )
        SprMngr.DrawSprite( IntPSelect, IntBSelectRoof[ 0 ] + IntX, IntBSelectRoof[ 1 ] + IntY );

    int x = IntWWork[ 0 ] + IntX;
    int y = IntWWork[ 1 ] + IntY;
    int h = IntWWork[ 3 ] - IntWWork[ 1 ];
    int w = ProtoWidth;

    if( IsObjectMode() )
    {
        int i = *CurProtoScroll;
        int j = i + ProtosOnScreen;
        if( j > (int) ( *CurItemProtos ).size() )
            j = (int) ( *CurItemProtos ).size();

        for( ; i < j; i++, x += w )
        {
            ProtoItem* proto_item = &( *CurItemProtos )[ i ];
            uint       col = ( i == (int) GetTabIndex() ? COLOR_IFACE_RED : COLOR_IFACE );
            SprMngr.DrawSpriteSize( proto_item->GetCurSprId(), x, y, (float) w, (float) ( h / 2 ), false, true, col );

            if( proto_item->IsItem() )
            {
                AnyFrames* anim = ResMngr.GetInvAnim( proto_item->PicInv );
                if( anim )
                    SprMngr.DrawSpriteSize( anim->GetCurSprId(), x, y + h / 2, (float) w, (float) ( h / 2 ), false, true, col );
            }

            SprMngr.DrawStr( Rect( x, y + h - 15, x + w, y + h ), Str::FormatBuf( "%u", proto_item->ProtoId ), FT_NOBREAK, COLOR_TEXT_WHITE );
        }

        if( GetTabIndex() < (uint) ( *CurItemProtos ).size() )
        {
            ProtoItem* proto_item = &( *CurItemProtos )[ GetTabIndex() ];
            string     info = MsgItem->GetStr( proto_item->ProtoId * 100 );
            info += " - ";
            info += MsgItem->GetStr( proto_item->ProtoId * 100 + 1 );
            SprMngr.DrawStr( Rect( IntWHint, IntX, IntY ), info.c_str(), 0 );
        }
    }
    else if( IsTileMode() )
    {
        int i = *CurProtoScroll;
        int j = i + ProtosOnScreen;
        if( j > (int) CurTileHashes->size() )
            j = (int) CurTileHashes->size();

        for( ; i < j; i++, x += w )
        {
            AnyFrames* anim = ResMngr.GetItemAnim( ( *CurTileHashes )[ i ] );
            if( !anim )
                anim = ItemHex::DefaultAnim;

            uint col = ( i == (int) GetTabIndex() ? COLOR_IFACE_RED : COLOR_IFACE );
            SprMngr.DrawSpriteSize( anim->GetCurSprId(), x, y, (float) w, (float) ( h / 2 ), false, true, col );

            string& name = ( *CurTileNames )[ i ];
            size_t  pos = name.find_last_of( '\\' );
            if( pos != string::npos )
                SprMngr.DrawStr( Rect( x, y + h - 15, x + w, y + h ), name.substr( pos + 1 ).c_str(), FT_NOBREAK, COLOR_TEXT_WHITE );
            else
                SprMngr.DrawStr( Rect( x, y + h - 15, x + w, y + h ), name.c_str(), FT_NOBREAK, COLOR_TEXT_WHITE );
        }

        if( GetTabIndex() < CurTileNames->size() )
            SprMngr.DrawStr( Rect( IntWHint, IntX, IntY ), ( *CurTileNames )[ GetTabIndex() ].c_str(), 0 );
    }
    else if( IsCritMode() )
    {
        uint i = *CurProtoScroll;
        uint j = i + ProtosOnScreen;
        if( j > CurNpcProtos->size() )
            j = (uint) CurNpcProtos->size();

        for( ; i < j; i++, x += w )
        {
            CritData* pnpc = ( *CurNpcProtos )[ i ];

            uint      spr_id = ResMngr.GetCritSprId( pnpc->BaseType, 1, 1, NpcDir, &pnpc->Params[ ST_ANIM3D_LAYER_BEGIN ] );
            if( !spr_id )
                continue;

            uint col = COLOR_IFACE;
            if( i == GetTabIndex() )
                col = COLOR_IFACE_RED;

            SprMngr.DrawSpriteSize( spr_id, x, y, (float) w, (float) ( h / 2 ), false, true, col );
            SprMngr.DrawStr( Rect( x, y + h - 15, x + w, y + h ), Str::FormatBuf( "%u", pnpc->ProtoId ), FT_NOBREAK, COLOR_TEXT_WHITE );
        }

        if( GetTabIndex() < CurNpcProtos->size() )
        {
            CritData* pnpc = ( *CurNpcProtos )[ GetTabIndex() ];
            SprMngr.DrawStr( Rect( IntWHint, IntX, IntY ), MsgDlg->GetStr( STR_NPC_PROTO_NAME_( pnpc->ProtoId ) ), 0 );
        }
    }
    else if( IntMode == INT_MODE_INCONT && !SelectedObj.empty() )
    {
        SelMapObj* o = &SelectedObj[ 0 ];

        uint       i = InContScroll;
        uint       j = i + ProtosOnScreen;
        if( j > o->Childs.size() )
            j = (uint) o->Childs.size();

        for( ; i < j; i++, x += w )
        {
            MapObject* mobj = o->Childs[ i ];
            ProtoItem* proto_item = ItemMngr.GetProtoItem( mobj->ProtoId );
            if( !proto_item )
                continue;

            AnyFrames* anim = ResMngr.GetInvAnim( proto_item->PicInv );
            if( !anim )
                continue;

            uint col = COLOR_IFACE;
            if( mobj == InContObject )
                col = COLOR_IFACE_RED;

            SprMngr.DrawSpriteSize( anim->GetCurSprId(), x, y, (float) w, (float) h, false, true, col );

            uint cnt = ( proto_item->Stackable ? mobj->MItem.Count : 1 );
            if( proto_item->Stackable && !cnt )
                cnt = proto_item->StartCount;
            if( !cnt )
                cnt = 1;
            SprMngr.DrawStr( Rect( x, y + h - 15, x + w, y + h ), Str::FormatBuf( "x%u", cnt ), FT_NOBREAK, COLOR_TEXT_WHITE );
            if( mobj->MItem.ItemSlot != SLOT_INV )
            {
                if( mobj->MItem.ItemSlot == SLOT_HAND1 )
                    SprMngr.DrawStr( Rect( x, y, x + w, y + h ), "Main", FT_NOBREAK, COLOR_TEXT_WHITE );
                else if( mobj->MItem.ItemSlot == SLOT_HAND2 )
                    SprMngr.DrawStr( Rect( x, y, x + w, y + h ), "Ext", FT_NOBREAK, COLOR_TEXT_WHITE );
                else if( mobj->MItem.ItemSlot == SLOT_ARMOR )
                    SprMngr.DrawStr( Rect( x, y, x + w, y + h ), "Armor", FT_NOBREAK, COLOR_TEXT_WHITE );
                else
                {
                    auto it = Self->SlotsExt.find( mobj->MItem.ItemSlot );
                    if( it != Self->SlotsExt.end() )
                        SprMngr.DrawStr( Rect( x, y, x + w, y + h ), ( *it ).second.SlotName, FT_NOBREAK, COLOR_TEXT_WHITE );
                    else
                        SprMngr.DrawStr( Rect( x, y, x + w, y + h ), "Error", FT_NOBREAK, COLOR_TEXT_WHITE );
                }
            }
        }
    }
    else if( IntMode == INT_MODE_LIST )
    {
        int i = ListScroll;
        int j = (int) LoadedProtoMaps.size();

        for( ; i < j; i++, x += w )
        {
            ProtoMap* pm = LoadedProtoMaps[ i ];
            SprMngr.DrawStr( Rect( x, y, x + w, y + h ), Str::FormatBuf( "<%s>", pm->GetName() ), 0, pm == CurProtoMap ? COLOR_IFACE_RED : COLOR_TEXT );
        }
    }

    // Message box
    if( IntMode == INT_MODE_MESS )
        MessBoxDraw();

    // Sub tabs
    if( SubTabsActive )
    {
        SprMngr.DrawSprite( SubTabsPic, SubTabsX, SubTabsY );

        int        line_height = SprMngr.GetLineHeight() + 1;
        int        posy = SubTabsRect.H() - line_height - 2;
        int        i = 0;
        SubTabMap& stabs = Tabs[ SubTabsActiveTab ];
        for( auto it = stabs.begin(), end = stabs.end(); it != end; ++it )
        {
            i++;
            if( i - 1 < TabsScroll[ SubTabsActiveTab ] )
                continue;

            string  name = ( *it ).first;
            SubTab& stab = ( *it ).second;

            uint    color = ( TabsActive[ SubTabsActiveTab ] == &stab ? COLOR_TEXT_WHITE : COLOR_TEXT );
            Rect    r = Rect( SubTabsRect.L + SubTabsX + 5, SubTabsRect.T + SubTabsY + posy,
                              SubTabsRect.L + SubTabsX + 5 + MODE_WIDTH, SubTabsRect.T + SubTabsY + posy + line_height - 1 );
            if( IsCurInRect( r ) )
                color = COLOR_TEXT_DWHITE;

            uint count = (uint) stab.TileNames.size();
            if( !count )
                count = (uint) stab.NpcProtos.size();
            if( !count )
                count = (uint) stab.ItemProtos.size();
            name += Str::FormatBuf( " (%u)", count );
            SprMngr.DrawStr( r, name.c_str(), 0, color );

            posy -= line_height;
            if( posy < 0 )
                break;
        }
    }

    // Map info
    if( HexMngr.IsMapLoaded() )
    {
        bool   hex_thru = false;
        ushort hx, hy;
        if( HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy ) )
            hex_thru = true;
        SprMngr.DrawStr( Rect( MODE_WIDTH - 100, 0, MODE_WIDTH, MODE_HEIGHT ),
                         Str::FormatBuf(
                             "Map '%s'\n"
                             "Hex %d %d\n"
                             "Time %u : %u\n"
                             "Fps %u\n"
                             "Tile layer %d\n"
                             "%s",
                             HexMngr.CurProtoMap->GetName(),
                             hex_thru ? hx : -1, hex_thru ? hy : -1,
                             DayTime / 60 % 24, DayTime % 60,
                             FPS,
                             TileLayer,
                             GameOpt.ScrollCheck ? "Scroll check" : "" ),
                         FT_NOBREAK_LINE );
    }
}

void FOMapper::ObjDraw()
{
    if( !ObjVisible )
        return;
    if( SelectedObj.empty() )
        return;

    SelMapObj& so = SelectedObj[ 0 ];
    MapObject* o = SelectedObj[ 0 ].MapObj;
    if( IntMode == INT_MODE_INCONT && InContObject )
        o = InContObject;

    ProtoItem* proto = NULL;
    if( o->MapObjType != MAP_OBJECT_CRITTER )
        proto = ItemMngr.GetProtoItem( o->ProtoId );

    int w = ObjWWork[ 2 ] - ObjWWork[ 0 ];
    int h = ObjWWork[ 3 ] - ObjWWork[ 1 ];
    int x = ObjWWork[ 0 ] + ObjX;
    int y = ObjWWork[ 1 ] + ObjY;

    SprMngr.DrawSprite( ObjWMainPic, ObjX, ObjY );
    if( ObjToAll )
        SprMngr.DrawSprite( ObjPBToAllDn, ObjBToAll[ 0 ] + ObjX, ObjBToAll[ 1 ] + ObjY );

    if( proto )
    {
        AnyFrames* anim = ResMngr.GetItemAnim( o->MItem.PicMapHash ? o->MItem.PicMapHash : proto->PicMap, o->Dir );
        if( !anim )
            anim = ItemHex::DefaultAnim;
        SprMngr.DrawSpriteSize( anim->GetCurSprId(), x + w - ProtoWidth, y, (float) ProtoWidth, (float) ProtoWidth, false, true );

        if( proto->IsItem() )
        {
            AnyFrames* anim = ResMngr.GetInvAnim( o->MItem.PicInvHash ? o->MItem.PicInvHash : proto->PicInv );
            if( anim )
                SprMngr.DrawSpriteSize( anim->GetCurSprId(), x + w - ProtoWidth, y + ProtoWidth, (float) ProtoWidth, (float) ProtoWidth, false, true );
        }
    }

    int  step = DRAW_NEXT_HEIGHT;
    uint col = COLOR_TEXT;
    int  dummy = 0;

// ====================================================================================
    #define DRAW_COMPONENT( name, val, unsign, cnst )                                                                       \
        do {                                                                                                                \
            char str_[ 256 ];                                                                                               \
            col = COLOR_TEXT;                                                                                               \
            if( ObjCurLine == ( y - ObjWWork[ 1 ] - ObjY ) / DRAW_NEXT_HEIGHT )                                             \
                col = COLOR_TEXT_RED;                                                                                       \
            if( ( cnst ) == true )                                                                                          \
                col = COLOR_TEXT_WHITE;                                                                                     \
            Str::Copy( str_, name );                                                                                        \
            Str::Append( str_, "...................................................." );                                    \
            SprMngr.DrawStr( Rect( Rect( x, y, x + w / 3, y + h ), 0, 0 ), str_, FT_NOBREAK, col );                         \
            ( unsign == true ) ? Str::Format( str_, "%u (0x%0X)", val, val ) : Str::Format( str_, "%d (0x%0X)", val, val ); \
            SprMngr.DrawStr( Rect( Rect( x + w / 3, y, x + w, y + h ), 0, 0 ), str_, FT_NOBREAK, col );                     \
            y += step;                                                                                                      \
        } while( 0 )
    #define DRAW_COMPONENT_TEXT( name, text, cnst )                                                     \
        do {                                                                                            \
            char str_[ 256 ];                                                                           \
            col = COLOR_TEXT;                                                                           \
            if( ObjCurLine == ( y - ObjWWork[ 1 ] - ObjY ) / DRAW_NEXT_HEIGHT )                         \
                col = COLOR_TEXT_RED;                                                                   \
            if( ( cnst ) == true )                                                                      \
                col = COLOR_TEXT_WHITE;                                                                 \
            Str::Copy( str_, name );                                                                    \
            Str::Append( str_, "...................................................." );                \
            SprMngr.DrawStr( Rect( Rect( x, y, x + w / 3, y + h ), 0, 0 ), str_, FT_NOBREAK, col );     \
            Str::Copy( str_, text ? text : "" );                                                        \
            SprMngr.DrawStr( Rect( Rect( x + w / 3, y, x + w, y + h ), 0, 0 ), str_, FT_NOBREAK, col ); \
            y += step;                                                                                  \
        } while( 0 )
// ====================================================================================

    if( so.MapNpc )
    {
        DRAW_COMPONENT_TEXT( "Name", CritType::GetName( so.MapNpc->GetCrType() ), true );         // 0
        DRAW_COMPONENT_TEXT( "Type", Str::FormatBuf( "Type %u", so.MapNpc->GetCrType() ), true ); // 1
    }
    else if( so.MapItem && proto )
    {
        DRAW_COMPONENT_TEXT( "PicMap", Str::GetName( proto->PicMap ), true );                   // 0
        DRAW_COMPONENT_TEXT( "PicInv", Str::GetName( proto->PicInv ), true );                   // 1
    }

    if( o->MapObjType == MAP_OBJECT_CRITTER )
        DRAW_COMPONENT_TEXT( "MapObjType", "Critter", true );                               // 2
    if( o->MapObjType == MAP_OBJECT_ITEM )
        DRAW_COMPONENT_TEXT( "MapObjType", "Item", true );                                  // 2
    if( o->MapObjType == MAP_OBJECT_SCENERY )
        DRAW_COMPONENT_TEXT( "MapObjType", "Scenery", true );                               // 2
    DRAW_COMPONENT( "ProtoId", o->ProtoId, true, true );                                    // 3
    DRAW_COMPONENT( "MapX", o->MapX, true, true );                                          // 4
    DRAW_COMPONENT( "MapY", o->MapY, true, true );                                          // 5
    DRAW_COMPONENT( "Dir", o->Dir, false, o->MapObjType == MAP_OBJECT_CRITTER );            // 6
    DRAW_COMPONENT_TEXT( "ScriptName", o->ScriptName, false );                              // 7
    DRAW_COMPONENT_TEXT( "FuncName", o->FuncName, false );                                  // 8
    DRAW_COMPONENT( "LightIntensity", o->LightIntensity, false, false );                    // 9
    DRAW_COMPONENT( "LightDistance", o->LightDistance, true, false );                       // 10
    DRAW_COMPONENT( "LightColor", o->LightColor, true, false );                             // 11
    DRAW_COMPONENT( "LightDirOff", o->LightDirOff, true, false );                           // 12
    DRAW_COMPONENT( "LightDay", o->LightDay, true, false );                                 // 13
    y += step;                                                                              // 14

    if( o->MapObjType == MAP_OBJECT_CRITTER )
    {
        DRAW_COMPONENT( "Cond", o->MCritter.Cond, true, false );                                // 15
        DRAW_COMPONENT( "Anim1", o->MCritter.Anim1, true, false );                              // 16
        DRAW_COMPONENT( "Anim2", o->MCritter.Anim2, true, false );                              // 17
        for( int i = 0; i < MAPOBJ_CRITTER_PARAMS; i++ )                                        // 18..57
        {
            if( o->MCritter.ParamIndex[ i ] >= 0 && o->MCritter.ParamIndex[ i ] < MAX_PARAMS )
            {
                const char* param_name = ConstantsManager::GetParamName( o->MCritter.ParamIndex[ i ] );
                DRAW_COMPONENT( param_name ? param_name : "???", o->MCritter.ParamValue[ i ], false, false );
            }
        }
    }
    else if( o->MapObjType == MAP_OBJECT_ITEM || o->MapObjType == MAP_OBJECT_SCENERY )
    {
        DRAW_COMPONENT( "OffsetX", o->MItem.OffsetX, false, false );                             // 15
        DRAW_COMPONENT( "OffsetY", o->MItem.OffsetY, false, false );                             // 16
        DRAW_COMPONENT( "AnimStayBegin", o->MItem.AnimStayBegin, true, false );                  // 17
        DRAW_COMPONENT( "AnimStayEnd", o->MItem.AnimStayEnd, true, false );                      // 18
        DRAW_COMPONENT( "AnimWaitTime", o->MItem.AnimWait, true, false );                        // 19
        DRAW_COMPONENT_TEXT( "PicMap", o->RunTime.PicMapName, false );                           // 20
        DRAW_COMPONENT_TEXT( "PicInv", o->RunTime.PicInvName, false );                           // 21
        DRAW_COMPONENT( "InfoOffset", o->MItem.InfoOffset, true, false );                        // 22
        y += step;                                                                               // 23

        if( o->MapObjType == MAP_OBJECT_ITEM )
        {
            DRAW_COMPONENT( "TrapValue", o->MItem.TrapValue, false, false );                             // 24
            DRAW_COMPONENT( "Value0", o->MItem.Val[ 0 ], false, false );                                 // 25
            DRAW_COMPONENT( "Value1", o->MItem.Val[ 1 ], false, false );                                 // 26
            DRAW_COMPONENT( "Value2", o->MItem.Val[ 2 ], false, false );                                 // 27
            DRAW_COMPONENT( "Value3", o->MItem.Val[ 3 ], false, false );                                 // 28
            DRAW_COMPONENT( "Value4", o->MItem.Val[ 4 ], false, false );                                 // 29

            if( proto->Stackable )
            {
                DRAW_COMPONENT( "Count", o->MItem.Count, true, false );                                      // 30
            }
            else
            {
                y += step;                                                                                   // 30
            }

            if( proto->Deteriorable )
            {
                DRAW_COMPONENT( "BrokenFlags", o->MItem.BrokenFlags, true, false );                          // 31
                DRAW_COMPONENT( "BrokenCount", o->MItem.BrokenCount, true, false );                          // 32
                DRAW_COMPONENT( "Deterioration", o->MItem.Deterioration, true, false );                      // 33
            }
            else
            {
                y += step;                                                                                   // 31
                y += step;                                                                                   // 32
                y += step;                                                                                   // 33
            }

            switch( proto->Type )
            {
            case ITEM_TYPE_WEAPON:
                if( !proto->Weapon_MaxAmmoCount )
                    break;
                DRAW_COMPONENT( "AmmoPid", o->MItem.AmmoPid, true, false );                                  // 34
                DRAW_COMPONENT( "AmmoCount", o->MItem.AmmoCount, true, false );                              // 35
                break;
            case ITEM_TYPE_KEY:
                DRAW_COMPONENT( "LockerDoorId", o->MItem.LockerDoorId, true, false );                        // 34
                break;
            case ITEM_TYPE_CONTAINER:
            case ITEM_TYPE_DOOR:
                DRAW_COMPONENT( "LockerDoorId", o->MItem.LockerDoorId, true, false );                        // 34
                DRAW_COMPONENT( "LockerCondition", o->MItem.LockerCondition, true, false );                  // 35
                DRAW_COMPONENT( "LockerComplexity", o->MItem.LockerComplexity, true, false );                // 36
                break;
            default:
                break;
            }
        }
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
        {
            DRAW_COMPONENT( "SpriteCut", o->MScenery.SpriteCut, false, false );                          // 24

            if( proto->Type == ITEM_TYPE_GRID )
            {
                DRAW_COMPONENT( "ToMapPid", o->MScenery.ToMapPid, true, false );                             // 25
                if( o->ProtoId == SP_GRID_ENTIRE )
                    DRAW_COMPONENT( "EntireNumber", o->MScenery.ToEntire, true, false );                     // 26
                else
                    DRAW_COMPONENT( "ToEntire", o->MScenery.ToEntire, true, false );                         // 26
                DRAW_COMPONENT( "ToDir", o->MScenery.ToDir, true, false );                                   // 27
            }
            else if( proto->Type == ITEM_TYPE_GENERIC )
            {
                DRAW_COMPONENT( "ParamsCount", o->MScenery.ParamsCount, true, false );                       // 25
                DRAW_COMPONENT( "Parameter0", o->MScenery.Param[ 0 ], false, false );                        // 26
                DRAW_COMPONENT( "Parameter1", o->MScenery.Param[ 1 ], false, false );                        // 27
                DRAW_COMPONENT( "Parameter2", o->MScenery.Param[ 2 ], false, false );                        // 28
                DRAW_COMPONENT( "Parameter3", o->MScenery.Param[ 3 ], false, false );                        // 29
                DRAW_COMPONENT( "Parameter4", o->MScenery.Param[ 4 ], false, false );                        // 30
                if( o->ProtoId == SP_SCEN_TRIGGER )
                {
                    DRAW_COMPONENT( "TriggerNum", o->MScenery.TriggerNum, true, false );                     // 31
                }
                else
                {
                    DRAW_COMPONENT( "CanUse", o->MScenery.CanUse, true, false );                                 // 31
                    DRAW_COMPONENT( "CanTalk", o->MScenery.CanTalk, true, false );                               // 32
                }
            }
        }
    }
}

void FOMapper::ObjKeyDown( uchar dik )
{
    if( !ObjVisible )
        return;
    if( ConsoleEdit )
        return;
    if( SelectedObj.empty() )
        return;

    HexMngr.RebuildLight();

    if( IntMode == INT_MODE_INCONT && InContObject )
    {
        ObjKeyDownA( InContObject, dik );
        return;
    }

    MapObject* o = SelectedObj[ 0 ].MapObj;
    ProtoItem* proto = ItemMngr.GetProtoItem( o->ProtoId );
    for( uint i = 0, j = ObjToAll ? (uint) SelectedObj.size() : 1; i < j; i++ ) // At least one time
    {
        MapObject* o2 = SelectedObj[ i ].MapObj;
        ProtoItem* proto2 = ItemMngr.GetProtoItem( o2->ProtoId );

        if( o->MapObjType == o2->MapObjType && ( o->MapObjType == MAP_OBJECT_CRITTER || ( proto && proto2 && proto->Type == proto2->Type ) ) )
        {
            ObjKeyDownA( o2, dik );

            if( SelectedObj[ i ].IsItem() )
                HexMngr.AffectItem( o2, SelectedObj[ i ].MapItem );
            else if( SelectedObj[ i ].IsNpc() )
                HexMngr.AffectCritter( o2, SelectedObj[ i ].MapNpc );
        }
    }
}

void FOMapper::ObjKeyDownA( MapObject* o, uchar dik )
{
    char*      val_c = NULL;
    uchar*     val_b = NULL;
    short*     val_s = NULL;
    ushort*    val_w = NULL;
    uint*      val_dw = NULL;
    int*       val_i = NULL;
    bool*      val_bool = NULL;

    ProtoItem* proto = ( o->MapObjType != MAP_OBJECT_CRITTER ? ItemMngr.GetProtoItem( o->ProtoId ) : NULL );
    if( o->MapObjType != MAP_OBJECT_CRITTER && !proto )
        return;

    if( o->MapObjType == MAP_OBJECT_CRITTER && ObjCurLine >= 18 && ObjCurLine <= 18 + MAPOBJ_CRITTER_PARAMS - 1 )
    {
        int pos = ObjCurLine - 18;
        for( int i = 0; i < MAPOBJ_CRITTER_PARAMS; i++ )
        {
            if( o->MCritter.ParamIndex[ i ] >= 0 && o->MCritter.ParamIndex[ i ] < MAX_PARAMS )
            {
                if( !pos )
                {
                    val_i = &o->MCritter.ParamValue[ i ];
                    break;
                }
                pos--;
            }
        }
    }

    switch( ObjCurLine )
    {
    case 6:
        if( o->MapObjType != MAP_OBJECT_CRITTER )
            val_s = &o->Dir;
        break;
    case 7:
        Keyb::GetChar( dik, o->ScriptName, NULL, MAPOBJ_SCRIPT_NAME, KIF_NO_SPEC_SYMBOLS );
        return;
    case 8:
        Keyb::GetChar( dik, o->FuncName, NULL, MAPOBJ_SCRIPT_NAME, KIF_NO_SPEC_SYMBOLS );
        return;
    case 9:
        val_c = &o->LightIntensity;
        break;
    case 10:
        val_b = &o->LightDistance;
        break;
    case 11:
        val_dw = &o->LightColor;
        break;
    case 12:
        val_b = &o->LightDirOff;
        break;
    case 13:
        val_b = &o->LightDay;
        break;
    case 14:
        break;

    case 15:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            val_b = &o->MCritter.Cond;
        else
            val_s = &o->MItem.OffsetX;
        break;
    case 16:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            val_dw = &o->MCritter.Anim1;
        else
            val_s = &o->MItem.OffsetY;
        break;
    case 17:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            val_dw = &o->MCritter.Anim2;
        else
            val_b = &o->MItem.AnimStayBegin;
        break;
    case 18:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            break;
        else
            val_b = &o->MItem.AnimStayEnd;
        break;
    case 19:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            break;
        else
            val_w = &o->MItem.AnimWait;
        break;
    case 20:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            break;
        else
        {
            Keyb::GetChar( dik, o->RunTime.PicMapName, NULL, sizeof( o->RunTime.PicMapName ), KIF_NO_SPEC_SYMBOLS );
            return;
        }
        break;
    case 21:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            break;
        else
        {
            Keyb::GetChar( dik, o->RunTime.PicInvName, NULL, sizeof( o->RunTime.PicInvName ), KIF_NO_SPEC_SYMBOLS );
            return;
        }
        break;
    case 22:
        if( o->MapObjType == MAP_OBJECT_CRITTER )
            break;
        else
            val_b = &o->MItem.InfoOffset;
        break;
    case 23:
        break;

    case 24:
        if( o->MapObjType == MAP_OBJECT_ITEM )
            val_s = &o->MItem.TrapValue;
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
            val_b = &o->MScenery.SpriteCut;
        break;
    case 25:
        if( o->MapObjType == MAP_OBJECT_ITEM )
            val_i = &o->MItem.Val[ 0 ];
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( proto->Type == ITEM_TYPE_GRID )
                val_w = &o->MScenery.ToMapPid;
            else if( proto->Type == ITEM_TYPE_GENERIC )
                val_b = &o->MScenery.ParamsCount;
        }
        break;
    case 26:
        if( o->MapObjType == MAP_OBJECT_ITEM )
            val_i = &o->MItem.Val[ 1 ];
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( proto->Type == ITEM_TYPE_GRID )
                val_dw = &o->MScenery.ToEntire;
            else if( proto->Type == ITEM_TYPE_GENERIC )
                val_i = &o->MScenery.Param[ 0 ];
        }
        break;
    case 27:
        if( o->MapObjType == MAP_OBJECT_ITEM )
            val_i = &o->MItem.Val[ 2 ];
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( proto->Type == ITEM_TYPE_GRID )
                val_b = &o->MScenery.ToDir;
            else if( proto->Type == ITEM_TYPE_GENERIC )
                val_i = &o->MScenery.Param[ 1 ];
        }
        break;
    case 28:
        if( o->MapObjType == MAP_OBJECT_ITEM )
            val_i = &o->MItem.Val[ 3 ];
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( proto->Type == ITEM_TYPE_GENERIC )
                val_i = &o->MScenery.Param[ 2 ];
        }
        break;
    case 29:
        if( o->MapObjType == MAP_OBJECT_ITEM )
            val_i = &o->MItem.Val[ 4 ];
        else if( o->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( proto->Type == ITEM_TYPE_GENERIC )
                val_i = &o->MScenery.Param[ 3 ];
        }
        break;
    case 30:
        if( o->MapObjType == MAP_OBJECT_ITEM && proto->Stackable )
            val_dw = &o->MItem.Count;
        else if( o->MapObjType == MAP_OBJECT_SCENERY && proto->Type == ITEM_TYPE_GENERIC )
            val_i = &o->MScenery.Param[ 4 ];
        break;
    case 31:
        if( o->MapObjType == MAP_OBJECT_ITEM && proto->Deteriorable )
            val_b = &o->MItem.BrokenFlags;
        else if( o->MapObjType == MAP_OBJECT_SCENERY && proto->Type == ITEM_TYPE_GENERIC )
        {
            if( o->ProtoId == SP_SCEN_TRIGGER )
                val_dw = &o->MScenery.TriggerNum;
            else
                val_bool = &o->MScenery.CanUse;
        }
        break;
    case 32:
        if( o->MapObjType == MAP_OBJECT_ITEM && proto->Deteriorable )
            val_b = &o->MItem.BrokenCount;
        else if( o->MapObjType == MAP_OBJECT_SCENERY && proto->Type == ITEM_TYPE_GENERIC && o->ProtoId != SP_SCEN_TRIGGER )
            val_bool = &o->MScenery.CanTalk;
        break;
    case 33:
        if( o->MapObjType == MAP_OBJECT_ITEM && proto->Deteriorable )
            val_w = &o->MItem.Deterioration;
        break;
    case 34:
        if( o->MapObjType == MAP_OBJECT_ITEM && proto->Type == ITEM_TYPE_WEAPON && proto->Weapon_MaxAmmoCount )
            val_w = &o->MItem.AmmoPid;
        else if( o->MapObjType == MAP_OBJECT_ITEM && ( proto->Type == ITEM_TYPE_KEY || proto->Type == ITEM_TYPE_CONTAINER || proto->Type == ITEM_TYPE_DOOR ) )
            val_dw = &o->MItem.LockerDoorId;
        break;
    case 35:
        if( o->MapObjType == MAP_OBJECT_ITEM && proto->Type == ITEM_TYPE_WEAPON && proto->Weapon_MaxAmmoCount )
            val_dw = &o->MItem.AmmoCount;
        else if( o->MapObjType == MAP_OBJECT_ITEM && ( proto->Type == ITEM_TYPE_CONTAINER || proto->Type == ITEM_TYPE_DOOR ) )
            val_w = &o->MItem.LockerCondition;
        break;
    case 36:
        if( o->MapObjType == MAP_OBJECT_ITEM && ( proto->Type == ITEM_TYPE_CONTAINER || proto->Type == ITEM_TYPE_DOOR ) )
            val_w = &o->MItem.LockerComplexity;
        break;
    default:
        break;
    }

    int add = 0;

    switch( dik )
    {
    case DIK_0:
    case DIK_NUMPAD0:
        add = 0;
        break;
    case DIK_1:
    case DIK_NUMPAD1:
        add = 1;
        break;
    case DIK_2:
    case DIK_NUMPAD2:
        add = 2;
        break;
    case DIK_3:
    case DIK_NUMPAD3:
        add = 3;
        break;
    case DIK_4:
    case DIK_NUMPAD4:
        add = 4;
        break;
    case DIK_5:
    case DIK_NUMPAD5:
        add = 5;
        break;
    case DIK_6:
    case DIK_NUMPAD6:
        add = 6;
        break;
    case DIK_7:
    case DIK_NUMPAD7:
        add = 7;
        break;
    case DIK_8:
    case DIK_NUMPAD8:
        add = 8;
        break;
    case DIK_9:
    case DIK_NUMPAD9:
        add = 9;
        break;
//	case DIK_DELETE:
    case DIK_BACK:
        if( val_c )
            *val_c = *val_c / 10;
        if( val_b )
            *val_b = *val_b / 10;
        if( val_s )
            *val_s = *val_s / 10;
        if( val_w )
            *val_w = *val_w / 10;
        if( val_dw )
            *val_dw = *val_dw / 10;
        if( val_i )
            *val_i = *val_i / 10;
        if( val_bool )
            *val_bool = false;
        return;
    case DIK_MINUS:
    case DIK_SUBTRACT:
        if( val_c )
            *val_c = -*val_c;
        if( val_s )
            *val_s = -*val_s;
        if( val_i )
            *val_i = -*val_i;
        if( val_bool )
            *val_bool = !*val_bool;
        return;
    default:
        return;
    }

    if( val_b == &o->MCritter.Cond )
    {
        *val_b = add;
        return;
    }

    if( val_c )
        *val_c = *val_c * 10 + add;
    if( val_b )
        *val_b = *val_b * 10 + add;
    if( val_s )
        *val_s = *val_s * 10 + add;
    if( val_w )
        *val_w = *val_w * 10 + add;
    if( val_dw )
        *val_dw = *val_dw * 10 + add;
    if( val_i )
        *val_i = *val_i * 10 + add;
    if( val_bool )
        *val_bool = ( add ? true : false );
}

void FOMapper::IntLMouseDown()
{
    IntHold = INT_NONE;

    // Sub tabs
    if( IntVisible && SubTabsActive )
    {
        if( IsCurInRect( SubTabsRect, SubTabsX, SubTabsY ) )
        {
            int        line_height = SprMngr.GetLineHeight() + 1;
            int        posy = SubTabsRect.H() - line_height - 2;
            int        i = 0;
            SubTabMap& stabs = Tabs[ SubTabsActiveTab ];
            for( auto it = stabs.begin(), end = stabs.end(); it != end; ++it )
            {
                i++;
                if( i - 1 < TabsScroll[ SubTabsActiveTab ] )
                    continue;

                const string& name = ( *it ).first;
                SubTab&       stab = ( *it ).second;

                Rect          r = Rect( SubTabsRect.L + SubTabsX + 5, SubTabsRect.T + SubTabsY + posy,
                                        SubTabsRect.L + SubTabsX + 5 + SubTabsRect.W(), SubTabsRect.T + SubTabsY + posy + line_height - 1 );
                if( IsCurInRect( r ) )
                {
                    TabsActive[ SubTabsActiveTab ] = &stab;
                    RefreshCurProtos();
                    break;
                }

                posy -= line_height;
                if( posy < 0 )
                    break;
            }

            return;
        }

        if( !IsCurInRect( IntWMain, IntX, IntY ) )
        {
            SubTabsActive = false;
            return;
        }
    }

    // Map
    if( ( !IntVisible || !IsCurInRect( IntWMain, IntX, IntY ) ) && ( !ObjVisible || SelectedObj.empty() || !IsCurInRect( ObjWMain, ObjX, ObjY ) ) )
    {
        InContObject = NULL;

        if( !HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, SelectHX1, SelectHY1 ) )
            return;
        SelectHX2 = SelectHX1;
        SelectHY2 = SelectHY1;
        SelectX = GameOpt.MouseX;
        SelectY = GameOpt.MouseY;

        if( CurMode == CUR_MODE_DEF )
        {
            if( Keyb::ShiftDwn )
            {
                for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
                {
                    SelMapObj& so = SelectedObj[ i ];
                    if( so.MapNpc )
                    {
                        uint crtype = so.MapNpc->GetCrType();
                        bool is_run = ( so.MapNpc->MoveSteps.size() && so.MapNpc->MoveSteps[ so.MapNpc->MoveSteps.size() - 1 ].first == SelectHX1 &&
                                        so.MapNpc->MoveSteps[ so.MapNpc->MoveSteps.size() - 1 ].second == SelectHY1 /* && CritType::IsCanRun(crtype)*/ );

                        so.MapNpc->MoveSteps.clear();
                        if( !is_run && !CritType::IsCanWalk( crtype ) )
                            break;

                        ushort   hx = so.MapNpc->GetHexX();
                        ushort   hy = so.MapNpc->GetHexY();
                        UCharVec steps;
                        if( HexMngr.FindPath( NULL, hx, hy, SelectHX1, SelectHY1, steps, -1 ) )
                        {
                            for( uint k = 0; k < steps.size(); k++ )
                            {
                                MoveHexByDir( hx, hy, steps[ k ], HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
                                so.MapNpc->MoveSteps.push_back( UShortPair( hx, hy ) );
                            }
                            so.MapNpc->IsRunning = is_run;
                        }

                        break;
                    }
                }
            }
            else if( !Keyb::CtrlDwn )
                SelectClear();

            // HexMngr.ClearHexTrack();
            // HexMngr.RefreshMap();
            IntHold = INT_SELECT;
        }
        else if( CurMode == CUR_MODE_MOVE )
        {
            IntHold = INT_SELECT;
        }
        else if( CurMode == CUR_MODE_DRAW )
        {
            if( IsObjectMode() && ( *CurItemProtos ).size() )
                ParseProto( ( *CurItemProtos )[ GetTabIndex() ].ProtoId, SelectHX1, SelectHY1, NULL );
            else if( IsTileMode() && CurTileHashes->size() )
                ParseTile( ( *CurTileHashes )[ GetTabIndex() ], SelectHX1, SelectHY1, 0, 0, TileLayer, DrawRoof );
            else if( IsCritMode() && CurNpcProtos->size() )
                ParseNpc( ( *CurNpcProtos )[ GetTabIndex() ]->ProtoId, SelectHX1, SelectHY1 );
        }

        return;
    }

    // Object editor
    if( ObjVisible && !SelectedObj.empty() && IsCurInRect( ObjWMain, ObjX, ObjY ) )
    {
        if( IsCurInRect( ObjWWork, ObjX, ObjY ) )
        {
            ObjCurLine = ( GameOpt.MouseY - ObjY - ObjWWork[ 1 ] ) / DRAW_NEXT_HEIGHT;
            //		return;
        }

        if( IsCurInRect( ObjBToAll, ObjX, ObjY ) )
        {
            ObjToAll = !ObjToAll;
            IntHold = INT_BUTTON;
            return;
        }
        else if( !ObjFix )
        {
            IntHold = INT_OBJECT;
            ItemVectX = GameOpt.MouseX - ObjX;
            ItemVectY = GameOpt.MouseY - ObjY;
        }

        return;
    }

    // Interface
    if( !IntVisible || !IsCurInRect( IntWMain, IntX, IntY ) )
        return;

    if( IsCurInRect( IntWWork, IntX, IntY ) )
    {
        int ind = ( GameOpt.MouseX - IntX - IntWWork[ 0 ] ) / ProtoWidth;

        if( IsObjectMode() && ( *CurItemProtos ).size() )
        {
            ind += *CurProtoScroll;
            if( ind >= (int) ( *CurItemProtos ).size() )
                ind = (int) ( *CurItemProtos ).size() - 1;
            SetTabIndex( ind );

            // Switch ignore pid to draw
            if( Keyb::CtrlDwn )
            {
                ushort  pid = ( *CurItemProtos )[ ind ].ProtoId;

                SubTab& stab = Tabs[ INT_MODE_IGNORE ][ DEFAULT_SUB_TAB ];
                auto    it = std::find( stab.ItemProtos.begin(), stab.ItemProtos.end(), pid );
                if( it != stab.ItemProtos.end() )
                    stab.ItemProtos.erase( it );
                else
                    stab.ItemProtos.push_back( ( *CurItemProtos )[ ind ] );

                HexMngr.SwitchIgnorePid( pid );
                HexMngr.RefreshMap();
            }
            // Add to container
            else if( Keyb::AltDwn && SelectedObj.size() && SelectedObj[ 0 ].IsContainer() )
            {
                bool       add = true;
                ProtoItem* proto_item = &( *CurItemProtos )[ ind ];

                if( proto_item->Stackable )
                {
                    for( uint i = 0, j = (uint) SelectedObj[ 0 ].Childs.size(); i < j; i++ )
                    {
                        if( proto_item->ProtoId == SelectedObj[ 0 ].Childs[ i ]->ProtoId )
                        {
                            add = false;
                            break;
                        }
                    }
                }

                if( add )
                {
                    MapObject* mobj = SelectedObj[ 0 ].MapObj;
                    ParseProto( proto_item->ProtoId, mobj->MapX, mobj->MapY, mobj );
                    SelectAdd( mobj );
                }
            }
        }
        else if( IsTileMode() && CurTileHashes->size() )
        {
            ind += *CurProtoScroll;
            if( ind >= (int) CurTileHashes->size() )
                ind = (int) CurTileHashes->size() - 1;
            SetTabIndex( ind );
        }
        else if( IsCritMode() && CurNpcProtos->size() )
        {
            ind += *CurProtoScroll;
            if( ind >= (int) CurNpcProtos->size() )
                ind = (int) CurNpcProtos->size() - 1;
            SetTabIndex( ind );
        }
        else if( IntMode == INT_MODE_INCONT )
        {
            InContObject = NULL;
            ind += InContScroll;

            if( !SelectedObj.empty() && !SelectedObj[ 0 ].Childs.empty() )
            {
                if( ind < (int) SelectedObj[ 0 ].Childs.size() )
                    InContObject = SelectedObj[ 0 ].Childs[ ind ];

                // Delete child
                if( Keyb::AltDwn && InContObject )
                {
                    auto it = std::find( CurProtoMap->MObjects.begin(), CurProtoMap->MObjects.end(), InContObject );
                    if( it != CurProtoMap->MObjects.end() )
                    {
                        SAFEREL( InContObject );
                        CurProtoMap->MObjects.erase( it );
                    }
                    InContObject = NULL;

                    // Reselect
                    MapObject* mobj = SelectedObj[ 0 ].MapObj;
                    SelectClear();
                    SelectAdd( mobj );
                }
                // Change child slot
                else if( Keyb::ShiftDwn && InContObject && SelectedObj[ 0 ].MapNpc )
                {
                    ProtoItem* proto_item = ItemMngr.GetProtoItem( InContObject->ProtoId );
                    uchar      crtype = SelectedObj[ 0 ].MapNpc->GetCrType();
                    uchar      anim1 = ( proto_item->IsWeapon() ? proto_item->Weapon_Anim1 : 0 );

                    if( proto_item->IsArmor() && CritType::IsCanArmor( crtype ) )
                    {
                        if( InContObject->MItem.ItemSlot != SLOT_INV )
                        {
                            InContObject->MItem.ItemSlot = SLOT_INV;
                        }
                        else
                        {
                            uchar to_slot = proto_item->Slot;
                            to_slot = to_slot ? to_slot : SLOT_ARMOR;
                            for( uint i = 0; i < SelectedObj[ 0 ].Childs.size(); i++ )
                            {
                                MapObject* child = SelectedObj[ 0 ].Childs[ i ];
                                if( child->MItem.ItemSlot == to_slot )
                                    child->MItem.ItemSlot = SLOT_INV;
                            }
                            InContObject->MItem.ItemSlot = to_slot;
                        }
                    }
                    else if( !proto_item->IsArmor() && ( !anim1 || CritType::IsAnim1( crtype, anim1 ) ) )
                    {
                        if( InContObject->MItem.ItemSlot == SLOT_HAND1 )
                        {
                            for( uint i = 0; i < SelectedObj[ 0 ].Childs.size(); i++ )
                            {
                                MapObject* child = SelectedObj[ 0 ].Childs[ i ];
                                if( child->MItem.ItemSlot == SLOT_HAND2 )
                                    child->MItem.ItemSlot = SLOT_INV;
                            }
                            InContObject->MItem.ItemSlot = SLOT_HAND2;
                        }
                        else if( InContObject->MItem.ItemSlot == SLOT_HAND2 )
                        {
                            InContObject->MItem.ItemSlot = SLOT_INV;
                        }
                        else
                        {
                            for( uint i = 0; i < SelectedObj[ 0 ].Childs.size(); i++ )
                            {
                                MapObject* child = SelectedObj[ 0 ].Childs[ i ];
                                if( child->MItem.ItemSlot == SLOT_HAND1 )
                                    child->MItem.ItemSlot = SLOT_INV;
                            }
                            InContObject->MItem.ItemSlot = SLOT_HAND1;
                        }
                    }
                }

                if( SelectedObj[ 0 ].MapNpc )
                {
                    ProtoItem* pitem_ext = NULL;
                    ProtoItem* pitem_armor = NULL;
                    for( uint i = 0; i < SelectedObj[ 0 ].Childs.size(); i++ )
                    {
                        MapObject* child = SelectedObj[ 0 ].Childs[ i ];
                        if( child->MItem.ItemSlot == SLOT_HAND2 )
                            pitem_ext = ItemMngr.GetProtoItem( child->ProtoId );
                        else if( child->MItem.ItemSlot == SLOT_ARMOR )
                            pitem_armor = ItemMngr.GetProtoItem( child->ProtoId );
                    }
                    SelectedObj[ 0 ].MapNpc->DefItemSlotArmor->Init( pitem_armor ? pitem_armor : ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) );

                    ProtoItem* pitem_main = NULL;
                    for( uint i = 0; i < SelectedObj[ 0 ].Childs.size(); i++ )
                    {
                        MapObject* child = SelectedObj[ 0 ].Childs[ i ];
                        if( child->MItem.ItemSlot == SLOT_HAND1 )
                        {
                            pitem_main = ItemMngr.GetProtoItem( child->ProtoId );
                            uchar anim1 = ( pitem_main->IsWeapon() ? pitem_main->Weapon_Anim1 : 0 );
                            if( anim1 && !CritType::IsAnim1( SelectedObj[ 0 ].MapNpc->GetCrType(), anim1 ) )
                            {
                                pitem_main = NULL;
                                child->MItem.ItemSlot = SLOT_INV;
                            }
                        }
                    }
                    SelectedObj[ 0 ].MapNpc->DefItemSlotHand->Init( pitem_main ? pitem_main : ItemMngr.GetProtoItem( ITEM_DEF_SLOT ) );
                    SelectedObj[ 0 ].MapNpc->AnimateStay();
                }
                HexMngr.RebuildLight();
            }
        }
        else if( IntMode == INT_MODE_LIST )
        {
            ind += ListScroll;

            if( ind < (int) LoadedProtoMaps.size() && CurProtoMap != LoadedProtoMaps[ ind ] )
            {
                HexMngr.GetScreenHexes( CurProtoMap->Header.WorkHexX, CurProtoMap->Header.WorkHexY );
                SelectClear();

                if( HexMngr.SetProtoMap( *LoadedProtoMaps[ ind ] ) )
                {
                    CurProtoMap = LoadedProtoMaps[ ind ];
                    HexMngr.FindSetCenter( CurProtoMap->Header.WorkHexX, CurProtoMap->Header.WorkHexY );
                }
            }
        }
    }
    else if( IsCurInRect( IntBCust[ 0 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM0 );
    else if( IsCurInRect( IntBCust[ 1 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM1 );
    else if( IsCurInRect( IntBCust[ 2 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM2 );
    else if( IsCurInRect( IntBCust[ 3 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM3 );
    else if( IsCurInRect( IntBCust[ 4 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM4 );
    else if( IsCurInRect( IntBCust[ 5 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM5 );
    else if( IsCurInRect( IntBCust[ 6 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM6 );
    else if( IsCurInRect( IntBCust[ 7 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM7 );
    else if( IsCurInRect( IntBCust[ 8 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM8 );
    else if( IsCurInRect( IntBCust[ 9 ], IntX, IntY ) )
        IntSetMode( INT_MODE_CUSTOM9 );
    else if( IsCurInRect( IntBItem, IntX, IntY ) )
        IntSetMode( INT_MODE_ITEM );
    else if( IsCurInRect( IntBTile, IntX, IntY ) )
        IntSetMode( INT_MODE_TILE );
    else if( IsCurInRect( IntBCrit, IntX, IntY ) )
        IntSetMode( INT_MODE_CRIT );
    else if( IsCurInRect( IntBFast, IntX, IntY ) )
        IntSetMode( INT_MODE_FAST );
    else if( IsCurInRect( IntBIgnore, IntX, IntY ) )
        IntSetMode( INT_MODE_IGNORE );
    else if( IsCurInRect( IntBInCont, IntX, IntY ) )
        IntSetMode( INT_MODE_INCONT );
    else if( IsCurInRect( IntBMess, IntX, IntY ) )
        IntSetMode( INT_MODE_MESS );
    else if( IsCurInRect( IntBList, IntX, IntY ) )
        IntSetMode( INT_MODE_LIST );
    else if( IsCurInRect( IntBScrBack, IntX, IntY ) )
    {
        if( IsObjectMode() || IsTileMode() || IsCritMode() )
        {
            ( *CurProtoScroll )--;
            if( *CurProtoScroll < 0 )
                *CurProtoScroll = 0;
        }
        else if( IntMode == INT_MODE_INCONT )
        {
            InContScroll--;
            if( InContScroll < 0 )
                InContScroll = 0;
        }
        else if( IntMode == INT_MODE_LIST )
        {
            ListScroll--;
            if( ListScroll < 0 )
                ListScroll = 0;
        }
    }
    else if( IsCurInRect( IntBScrBackFst, IntX, IntY ) )
    {
        if( IsObjectMode() || IsTileMode() || IsCritMode() )
        {
            ( *CurProtoScroll ) -= ProtosOnScreen;
            if( *CurProtoScroll < 0 )
                *CurProtoScroll = 0;
        }
        else if( IntMode == INT_MODE_INCONT )
        {
            InContScroll -= ProtosOnScreen;
            if( InContScroll < 0 )
                InContScroll = 0;
        }
        else if( IntMode == INT_MODE_LIST )
        {
            ListScroll -= ProtosOnScreen;
            if( ListScroll < 0 )
                ListScroll = 0;
        }
    }
    else if( IsCurInRect( IntBScrFront, IntX, IntY ) )
    {
        if( IsObjectMode() && ( *CurItemProtos ).size() )
        {
            ( *CurProtoScroll )++;
            if( *CurProtoScroll >= (int) ( *CurItemProtos ).size() )
                *CurProtoScroll = (int) ( *CurItemProtos ).size() - 1;
        }
        else if( IsTileMode() && CurTileHashes->size() )
        {
            ( *CurProtoScroll )++;
            if( *CurProtoScroll >= (int) CurTileHashes->size() )
                *CurProtoScroll = (int) CurTileHashes->size() - 1;
        }
        else if( IsCritMode() && CurNpcProtos->size() )
        {
            ( *CurProtoScroll )++;
            if( *CurProtoScroll >= (int) CurNpcProtos->size() )
                *CurProtoScroll = (int) CurNpcProtos->size() - 1;
        }
        else if( IntMode == INT_MODE_INCONT )
            InContScroll++;
        else if( IntMode == INT_MODE_LIST )
            ListScroll++;
    }
    else if( IsCurInRect( IntBScrFrontFst, IntX, IntY ) )
    {
        if( IsObjectMode() && ( *CurItemProtos ).size() )
        {
            ( *CurProtoScroll ) += ProtosOnScreen;
            if( *CurProtoScroll >= (int) ( *CurItemProtos ).size() )
                *CurProtoScroll = (int) ( *CurItemProtos ).size() - 1;
        }
        else if( IsTileMode() && CurTileHashes->size() )
        {
            ( *CurProtoScroll ) += ProtosOnScreen;
            if( *CurProtoScroll >= (int) CurTileHashes->size() )
                *CurProtoScroll = (int) CurTileHashes->size() - 1;
        }
        else if( IsCritMode() && CurNpcProtos->size() )
        {
            ( *CurProtoScroll ) += ProtosOnScreen;
            if( *CurProtoScroll >= (int) CurNpcProtos->size() )
                *CurProtoScroll = (int) CurNpcProtos->size() - 1;
        }
        else if( IntMode == INT_MODE_INCONT )
            InContScroll += ProtosOnScreen;
        else if( IntMode == INT_MODE_LIST )
            ListScroll += ProtosOnScreen;
    }
    else if( IsCurInRect( IntBShowItem, IntX, IntY ) )
    {
        GameOpt.ShowItem = !GameOpt.ShowItem;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBShowScen, IntX, IntY ) )
    {
        GameOpt.ShowScen = !GameOpt.ShowScen;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBShowWall, IntX, IntY ) )
    {
        GameOpt.ShowWall = !GameOpt.ShowWall;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBShowCrit, IntX, IntY ) )
    {
        GameOpt.ShowCrit = !GameOpt.ShowCrit;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBShowTile, IntX, IntY ) )
    {
        GameOpt.ShowTile = !GameOpt.ShowTile;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBShowRoof, IntX, IntY ) )
    {
        GameOpt.ShowRoof = !GameOpt.ShowRoof;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBShowFast, IntX, IntY ) )
    {
        GameOpt.ShowFast = !GameOpt.ShowFast;
        HexMngr.RefreshMap();
    }
    else if( IsCurInRect( IntBSelectItem, IntX, IntY ) )
        IsSelectItem = !IsSelectItem;
    else if( IsCurInRect( IntBSelectScen, IntX, IntY ) )
        IsSelectScen = !IsSelectScen;
    else if( IsCurInRect( IntBSelectWall, IntX, IntY ) )
        IsSelectWall = !IsSelectWall;
    else if( IsCurInRect( IntBSelectCrit, IntX, IntY ) )
        IsSelectCrit = !IsSelectCrit;
    else if( IsCurInRect( IntBSelectTile, IntX, IntY ) )
        IsSelectTile = !IsSelectTile;
    else if( IsCurInRect( IntBSelectRoof, IntX, IntY ) )
        IsSelectRoof = !IsSelectRoof;
    else if( !IntFix )
    {
        IntHold = INT_MAIN;
        IntVectX = GameOpt.MouseX - IntX;
        IntVectY = GameOpt.MouseY - IntY;
        return;
    }
    else
        return;

    IntHold = INT_BUTTON;
}

void FOMapper::IntLMouseUp()
{
    if( IntHold == INT_SELECT && HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, SelectHX2, SelectHY2 ) )
    {
        if( CurMode == CUR_MODE_DEF )
        {
            if( SelectHX1 != SelectHX2 || SelectHY1 != SelectHY2 )
            {
                HexMngr.ClearHexTrack();
                UShortPairVec h;

                if( SelectType == SELECT_TYPE_OLD )
                {
                    int fx = min( SelectHX1, SelectHX2 );
                    int tx = max( SelectHX1, SelectHX2 );
                    int fy = min( SelectHY1, SelectHY2 );
                    int ty = max( SelectHY1, SelectHY2 );

                    for( int i = fx; i <= tx; i++ )
                    {
                        for( int j = fy; j <= ty; j++ )
                        {
                            h.push_back( PAIR( i, j ) );
                        }
                    }
                }
                else                 // SELECT_TYPE_NEW
                {
                    HexMngr.GetHexesRect( Rect( SelectHX1, SelectHY1, SelectHX2, SelectHY2 ), h );
                }

                ItemHexVec items;
                CritVec    critters;
                for( uint i = 0, j = (uint) h.size(); i < j; i++ )
                {
                    ushort hx = h[ i ].first;
                    ushort hy = h[ i ].second;

                    // Items, critters
                    HexMngr.GetItems( hx, hy, items );
                    HexMngr.GetCritters( hx, hy, critters, FIND_ALL );

                    // Tile, roof
                    if( IsSelectTile && GameOpt.ShowTile )
                        SelectAddTile( hx, hy, false );
                    if( IsSelectRoof && GameOpt.ShowRoof )
                        SelectAddTile( hx, hy, true );
                }

                for( uint k = 0; k < items.size(); k++ )
                {
                    ushort pid = items[ k ]->GetProtoId();
                    if( HexMngr.IsIgnorePid( pid ) )
                        continue;
                    if( !GameOpt.ShowFast && HexMngr.IsFastPid( pid ) )
                        continue;

                    if( items[ k ]->IsItem() && IsSelectItem && GameOpt.ShowItem )
                        SelectAddItem( items[ k ] );
                    else if( items[ k ]->IsScenOrGrid() && IsSelectScen && GameOpt.ShowScen )
                        SelectAddItem( items[ k ] );
                    else if( items[ k ]->IsWall() && IsSelectWall && GameOpt.ShowWall )
                        SelectAddItem( items[ k ] );
                    else if( GameOpt.ShowFast && HexMngr.IsFastPid( pid ) )
                        SelectAddItem( items[ k ] );
                }

                for( uint l = 0; l < critters.size(); l++ )
                {
                    if( IsSelectCrit && GameOpt.ShowCrit )
                        SelectAddCrit( critters[ l ] );
                }
            }
            else
            {
                ItemHex*   item;
                CritterCl* cr;
                HexMngr.GetSmthPixel( GameOpt.MouseX, GameOpt.MouseY, item, cr );

                if( item )
                {
                    if( !HexMngr.IsIgnorePid( item->GetProtoId() ) )
                        SelectAddItem( item );
                }
                else if( cr )
                {
                    SelectAddCrit( cr );
                }
            }

            //	if(SelectedObj.size() || SelectedTile.size()) CurMode=CUR_MODE_MOVE;

            // Crits or item container
            if( SelectedObj.size() && SelectedObj[ 0 ].IsContainer() )
                IntSetMode( INT_MODE_INCONT );
        }

        HexMngr.RefreshMap();
    }

    IntHold = INT_NONE;
}

void FOMapper::IntMouseMove()
{
    if( IntHold == INT_SELECT )
    {
        HexMngr.ClearHexTrack();
        if( !HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, SelectHX2, SelectHY2 ) )
        {
            if( SelectHX2 || SelectHY2 )
            {
                HexMngr.RefreshMap();
                SelectHX2 = SelectHY2 = 0;
            }
            return;
        }

        if( CurMode == CUR_MODE_DEF )
        {
            if( SelectHX1 != SelectHX2 || SelectHY1 != SelectHY2 )
            {
                if( SelectType == SELECT_TYPE_OLD )
                {
                    int fx = min( SelectHX1, SelectHX2 );
                    int tx = max( SelectHX1, SelectHX2 );
                    int fy = min( SelectHY1, SelectHY2 );
                    int ty = max( SelectHY1, SelectHY2 );

                    for( int i = fx; i <= tx; i++ )
                        for( int j = fy; j <= ty; j++ )
                            HexMngr.GetHexTrack( i, j ) = 1;
                }
                else if( SelectType == SELECT_TYPE_NEW )
                {
                    UShortPairVec h;
                    HexMngr.GetHexesRect( Rect( SelectHX1, SelectHY1, SelectHX2, SelectHY2 ), h );

                    for( uint i = 0, j = (uint) h.size(); i < j; i++ )
                        HexMngr.GetHexTrack( h[ i ].first, h[ i ].second ) = 1;
                }

                HexMngr.RefreshMap();
            }
        }
        else if( CurMode == CUR_MODE_MOVE )
        {
            int offs_hx = (int) SelectHX2 - (int) SelectHX1;
            int offs_hy = (int) SelectHY2 - (int) SelectHY1;
            SelectHX1 = SelectHX2;
            SelectHY1 = SelectHY2;
            int offs_x = GameOpt.MouseX - SelectX;
            int offs_y = GameOpt.MouseY - SelectY;
            SelectX = GameOpt.MouseX;
            SelectY = GameOpt.MouseY;

            SelectMove( offs_hx, offs_hy, offs_x, offs_y );
            HexMngr.RefreshMap();
        }
    }
    else if( IntHold == INT_MAIN )
    {
        IntX = GameOpt.MouseX - IntVectX;
        IntY = GameOpt.MouseY - IntVectY;
    }
    else if( IntHold == INT_OBJECT )
    {
        ObjX = GameOpt.MouseX - ItemVectX;
        ObjY = GameOpt.MouseY - ItemVectY;
    }
}

uint FOMapper::GetTabIndex()
{
    if( IntMode < TAB_COUNT )
        return TabsActive[ IntMode ]->Index;
    return TabIndex[ IntMode ];
}

void FOMapper::SetTabIndex( uint index )
{
    if( IntMode < TAB_COUNT )
        TabsActive[ IntMode ]->Index = index;
    TabIndex[ IntMode ] = index;
}

void FOMapper::RefreshCurProtos()
{
    // Select protos and scroll
    CurItemProtos = NULL;
    CurProtoScroll = NULL;
    CurTileHashes = NULL;
    CurTileNames = NULL;
    CurNpcProtos = NULL;
    InContObject = NULL;

    if( IntMode >= 0 && IntMode < TAB_COUNT )
    {
        SubTab* stab = TabsActive[ IntMode ];
        if( stab->TileNames.size() )
        {
            CurTileNames = &stab->TileNames;
            CurTileHashes = &stab->TileHashes;
        }
        else if( stab->NpcProtos.size() )
        {
            CurNpcProtos = &stab->NpcProtos;
        }
        else
        {
            CurItemProtos = &stab->ItemProtos;
        }
        CurProtoScroll = &stab->Scroll;
    }

    if( IntMode == INT_MODE_INCONT )
        InContScroll = 0;

    // Update fast pids
    HexMngr.ClearFastPids();
    ProtoItemVec& fast_pids = TabsActive[ INT_MODE_FAST ]->ItemProtos;
    for( uint i = 0, j = (uint) fast_pids.size(); i < j; i++ )
        HexMngr.AddFastPid( fast_pids[ i ].ProtoId );

    // Update ignore pids
    HexMngr.ClearIgnorePids();
    ProtoItemVec& ignore_pids = TabsActive[ INT_MODE_IGNORE ]->ItemProtos;
    for( uint i = 0, j = (uint) ignore_pids.size(); i < j; i++ )
        HexMngr.AddIgnorePid( ignore_pids[ i ].ProtoId );

    // Refresh map
    if( HexMngr.IsMapLoaded() )
        HexMngr.RefreshMap();
}

void FOMapper::IntSetMode( int mode )
{
    if( SubTabsActive && mode == SubTabsActiveTab )
    {
        SubTabsActive = false;
        return;
    }

    if( !SubTabsActive && mode == IntMode && mode >= 0 && mode < TAB_COUNT )
    {
        // Show sub tabs screen
        SubTabsActive = true;
        SubTabsActiveTab = mode;

        // Calculate position
        if( mode <= INT_MODE_CUSTOM9 )
            SubTabsX = IntBCust[ mode - INT_MODE_CUSTOM0 ].CX(), SubTabsY = IntBCust[ mode - INT_MODE_CUSTOM0 ].T;
        else if( mode == INT_MODE_ITEM )
            SubTabsX = IntBItem.CX(), SubTabsY = IntBItem.T;
        else if( mode == INT_MODE_TILE )
            SubTabsX = IntBTile.CX(), SubTabsY = IntBTile.T;
        else if( mode == INT_MODE_CRIT )
            SubTabsX = IntBCrit.CX(), SubTabsY = IntBCrit.T;
        else if( mode == INT_MODE_FAST )
            SubTabsX = IntBFast.CX(), SubTabsY = IntBFast.T;
        else if( mode == INT_MODE_IGNORE )
            SubTabsX = IntBIgnore.CX(), SubTabsY = IntBIgnore.T;
        else
            SubTabsX = SubTabsY = 0;
        SubTabsX += IntX - SubTabsRect.W() / 2;
        SubTabsY += IntY - SubTabsRect.H();
        if( SubTabsX < 0 )
            SubTabsX = 0;
        if( SubTabsX + SubTabsRect.W() > MODE_WIDTH )
            SubTabsX -= SubTabsX + SubTabsRect.W() - MODE_WIDTH;
        if( SubTabsY < 0 )
            SubTabsY = 0;
        if( SubTabsY + SubTabsRect.H() > MODE_HEIGHT )
            SubTabsY -= SubTabsY + SubTabsRect.H() - MODE_HEIGHT;

        return;
    }

    IntMode = mode;
    IntHold = INT_NONE;

    RefreshCurProtos();

    if( SubTabsActive )
    {
        // Reinit sub tabs
        SubTabsActive = false;
        IntSetMode( IntMode );
    }
}

MapObject* FOMapper::FindMapObject( ProtoMap& pmap, ushort hx, ushort hy, uchar mobj_type, ushort pid, uint skip )
{
    for( uint i = 0, j = (uint) pmap.MObjects.size(); i < j; i++ )
    {
        MapObject* mo = pmap.MObjects[ i ];

        if( mo->MapX == hx && mo->MapY == hy && mo->MapObjType == mobj_type && ( !pid || mo->ProtoId == pid ) && !mo->ContainerUID )
        {
            if( skip )
                skip--;
            else
                return mo;
        }
    }
    return NULL;
}

void FOMapper::FindMapObjects( ProtoMap& pmap, ushort hx, ushort hy, uint radius, uchar mobj_type, ushort pid, MapObjectPtrVec& objects )
{
    for( uint i = 0, j = (uint) pmap.MObjects.size(); i < j; i++ )
    {
        MapObject* mo = pmap.MObjects[ i ];

        if( mo->MapObjType == mobj_type && DistGame( mo->MapX, mo->MapY, hx, hy ) <= radius && ( !pid || mo->ProtoId == pid ) && !mo->ContainerUID )
        {
            objects.push_back( mo );
        }
    }
}

MapObject* FOMapper::FindMapObject( ushort hx, ushort hy, uchar mobj_type, ushort pid, bool skip_selected )
{
    for( uint i = 0, j = (uint) CurProtoMap->MObjects.size(); i < j; i++ )
    {
        MapObject* mo = CurProtoMap->MObjects[ i ];

        if( mo->MapX == hx && mo->MapY == hy && mo->MapObjType == mobj_type && mo->ProtoId == pid && !mo->ContainerUID &&
            ( !skip_selected || std::find( SelectedObj.begin(), SelectedObj.end(), mo ) == SelectedObj.end() ) )
        {
            return mo;
        }
    }
    return NULL;
}

void FOMapper::UpdateMapObject( MapObject* mobj )
{
    if( !mobj->RunTime.MapObjId )
        return;

    if( mobj->MapObjType == MAP_OBJECT_CRITTER )
    {
        CritterCl* cr = HexMngr.GetCritter( mobj->RunTime.MapObjId );
        if( cr )
            HexMngr.AffectCritter( mobj, cr );
    }
    else if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
    {
        ItemHex* item = HexMngr.GetItemById( mobj->RunTime.MapObjId );
        if( item )
            HexMngr.AffectItem( mobj, item );
    }
}

void FOMapper::MoveMapObject( MapObject* mobj, ushort hx, ushort hy )
{
    if( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
        return;
    if( !mobj->RunTime.MapObjId )
        return;

    if( Self->CurProtoMap == mobj->RunTime.FromMap )
    {
        if( mobj->MapObjType == MAP_OBJECT_CRITTER )
        {
            CritterCl* cr = HexMngr.GetCritter( mobj->RunTime.MapObjId );
            if( cr && ( cr->IsDead() || !HexMngr.GetField( hx, hy ).Crit ) )
            {
                HexMngr.RemoveCrit( cr );
                cr->HexX = hx;
                cr->HexY = hy;
                mobj->MapX = hx;
                mobj->MapY = hy;
                HexMngr.SetCrit( cr );
            }
        }
        else if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
        {
            ItemHex* item = HexMngr.GetItemById( mobj->RunTime.MapObjId );
            if( item )
            {
                HexMngr.DeleteItem( item, false );
                item->HexX = hx;
                item->HexY = hy;
                mobj->MapX = hx;
                mobj->MapY = hy;
                HexMngr.PushItem( item );
            }
        }
    }
    else if( hx < mobj->RunTime.FromMap->Header.MaxHexX && hy < mobj->RunTime.FromMap->Header.MaxHexY )
    {
        if( mobj->MapObjType == MAP_OBJECT_CRITTER && mobj->MCritter.Cond != COND_DEAD )
        {
            for( auto it = mobj->RunTime.FromMap->MObjects.begin(); it != mobj->RunTime.FromMap->MObjects.end(); ++it )
            {
                MapObject* mobj_ = *it;
                if( mobj_->MapObjType == MAP_OBJECT_CRITTER && mobj_->MCritter.Cond != COND_DEAD &&
                    mobj_->MapX == hx && mobj_->MapY == hy )
                    return;
            }
        }

        mobj->MapX = hx;
        mobj->MapY = hy;
    }
}

void FOMapper::DeleteMapObject( MapObject* mobj )
{
    ProtoMap* pmap = mobj->RunTime.FromMap;
    if( !pmap )
        return;

    SelectClear();

    // Delete container items
    if( mobj->UID )
    {
        for( auto it = pmap->MObjects.begin(); it != pmap->MObjects.end();)
        {
            MapObject* mobj_ = *it;
            if( mobj_->ContainerUID && mobj_->ContainerUID == mobj->UID )
            {
                mobj_->Release();
                it = pmap->MObjects.erase( it );
            }
            else
                ++it;
        }
    }

    // Delete on active map
    if( pmap == CurProtoMap && mobj->RunTime.MapObjId )
    {
        if( mobj->MapObjType == MAP_OBJECT_CRITTER )
        {
            CritterCl* cr = HexMngr.GetCritter( mobj->RunTime.MapObjId );
            if( cr )
                HexMngr.EraseCrit( cr->GetId() );
        }
        else if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
        {
            ItemHex* item = HexMngr.GetItemById( mobj->RunTime.MapObjId );
            if( item )
                HexMngr.FinishItem( item->GetId(), 0 );
        }
    }

    // Delete
    auto it = std::find( pmap->MObjects.begin(), pmap->MObjects.end(), mobj );
    if( it != pmap->MObjects.end() )
        pmap->MObjects.erase( it );

    // Delete childs
    if( mobj->ParentUID || mobj->UID )
    {
        for( uint i = 0, j = (uint) pmap->MObjects.size(); i < j; i++ )
        {
            MapObject* mobj_ = *it;
            if( ( mobj->ParentUID && mobj_->UID == mobj->ParentUID ) || ( mobj->UID && mobj_->ParentUID == mobj->UID ) )
            {
                DeleteMapObject( mobj_ );               // Recursive deleting
                return;
            }
        }
    }

    // Free
    mobj->Release();
}

void FOMapper::SelectClear()
{
    if( !CurProtoMap )
        return;

    // Clear map objects
    for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
    {
        SelMapObj& sobj = SelectedObj[ i ];
        if( sobj.IsItem() )
            sobj.MapItem->RestoreAlpha();
        else if( sobj.IsNpc() )
            sobj.MapNpc->Alpha = 0xFF;
    }
    SelectedObj.clear();

    // Clear tiles
    if( SelectedTile.size() )
        HexMngr.ParseSelTiles();
    SelectedTile.clear();
}

void FOMapper::SelectAddItem( ItemHex* item )
{
    if( !item )
        return;
    MapObject* mobj = FindMapObject( item->GetHexX(), item->GetHexY(), MAP_OBJECT_ITEM, item->GetProtoId(), true );
    if( !mobj )
        mobj = FindMapObject( item->GetHexX(), item->GetHexY(), MAP_OBJECT_SCENERY, item->GetProtoId(), true );
    if( !mobj )
        return;
    SelectAdd( mobj );
}

void FOMapper::SelectAddCrit( CritterCl* npc )
{
    if( !npc )
        return;
    MapObject* mobj = FindMapObject( npc->GetHexX(), npc->GetHexY(), MAP_OBJECT_CRITTER, npc->Flags, true );
    if( !mobj )
        return;
    SelectAdd( mobj );
}

void FOMapper::SelectAddTile( ushort hx, ushort hy, bool is_roof )
{
    Field& f = HexMngr.GetField( hx, hy );
    if( !is_roof && f.Tiles.empty() )
        return;
    if( is_roof && f.Roofs.empty() )
        return;

    // Helper
    for( uint i = 0, j = (uint) SelectedTile.size(); i < j; i++ )
    {
        SelMapTile& stile = SelectedTile[ i ];
        if( stile.HexX == hx && stile.HexY == hy && stile.IsRoof == is_roof )
            return;
    }
    SelectedTile.push_back( SelMapTile( hx, hy, is_roof ) );

    // Select
    ProtoMap::TileVec& tiles = CurProtoMap->GetTiles( hx, hy, is_roof );
    for( uint i = 0, j = (uint) tiles.size(); i < j; i++ )
        tiles[ i ].IsSelected = true;
}

void FOMapper::SelectAdd( MapObject* mobj, bool select_childs /* = true */ )
{
    if( mobj->RunTime.FromMap == CurProtoMap && mobj->RunTime.MapObjId )
    {
        if( mobj->MapObjType == MAP_OBJECT_CRITTER )
        {
            CritterCl* cr = HexMngr.GetCritter( mobj->RunTime.MapObjId );
            if( cr )
            {
                SelectedObj.push_back( SelMapObj( mobj, cr ) );
                cr->Alpha = SELECT_ALPHA;

                // In container
                if( mobj->UID )
                {
                    SelMapObj* sobj = &SelectedObj[ SelectedObj.size() - 1 ];
                    for( uint i = 0, j = (uint) CurProtoMap->MObjects.size(); i < j; i++ )
                    {
                        MapObject* mobj_ = CurProtoMap->MObjects[ i ];
                        if( mobj_->ContainerUID == mobj->UID && mobj_ != mobj )
                            sobj->Childs.push_back( mobj_ );
                    }
                }
            }
        }
        else if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( mobj->MapObjType == MAP_OBJECT_ITEM && mobj->ContainerUID )
                return;

            ItemHex* item = HexMngr.GetItemById( mobj->RunTime.MapObjId );
            if( item )
            {
                SelectedObj.push_back( SelMapObj( mobj, item ) );
                item->Alpha = SELECT_ALPHA;

                // In container
                if( mobj->UID )
                {
                    SelMapObj* sobj = &SelectedObj[ SelectedObj.size() - 1 ];
                    for( uint i = 0, j = (uint) CurProtoMap->MObjects.size(); i < j; i++ )
                    {
                        MapObject* mobj_ = CurProtoMap->MObjects[ i ];
                        if( mobj_->ContainerUID == mobj->UID && mobj_ != mobj )
                            sobj->Childs.push_back( mobj_ );
                    }
                }

                // Childs
                if( select_childs && ( mobj->UID || mobj->ParentUID ) )
                {
                    SelMapObj* sobj = &SelectedObj[ SelectedObj.size() - 1 ];
                    for( uint i = 0, j = (uint) CurProtoMap->MObjects.size(); i < j; i++ )
                    {
                        MapObject* mobj_ = CurProtoMap->MObjects[ i ];
                        if( ( ( mobj->UID && mobj_->ParentUID == mobj->UID ) || ( mobj->ParentUID && mobj_->UID == mobj->ParentUID ) ) && mobj_ != mobj )
                        {
                            SelectAdd( mobj_, false );
                        }
                    }
                }
            }
        }
    }
}

void FOMapper::SelectErase( MapObject* mobj )
{
    if( mobj->RunTime.FromMap == CurProtoMap )
    {
        if( mobj->MapObjType == MAP_OBJECT_ITEM && mobj->ContainerUID )
            return;

        for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
        {
            SelMapObj& sobj = SelectedObj[ i ];
            if( sobj.MapObj == mobj )
            {
                if( mobj->RunTime.MapObjId )
                {
                    if( mobj->MapObjType == MAP_OBJECT_CRITTER )
                    {
                        CritterCl* cr = HexMngr.GetCritter( mobj->RunTime.MapObjId );
                        if( cr )
                            cr->Alpha = 0xFF;
                    }
                    else if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
                    {
                        ItemHex* item = HexMngr.GetItemById( mobj->RunTime.MapObjId );
                        if( item )
                            item->RestoreAlpha();
                    }
                }

                SelectedObj.erase( SelectedObj.begin() + i );
                break;
            }
        }
    }
}

void FOMapper::SelectAll()
{
    SelectClear();

    for( uint i = 0; i < HexMngr.GetMaxHexX(); i++ )
    {
        for( uint j = 0; j < HexMngr.GetMaxHexY(); j++ )
        {
            if( IsSelectTile && GameOpt.ShowTile )
                SelectAddTile( i, j, false );
            if( IsSelectRoof && GameOpt.ShowRoof )
                SelectAddTile( i, j, true );
        }
    }

    ItemHexVec& items = HexMngr.GetItems();
    for( uint i = 0; i < items.size(); i++ )
    {
        if( HexMngr.IsIgnorePid( items[ i ]->GetProtoId() ) )
            continue;

        if( items[ i ]->IsItem() && IsSelectItem && GameOpt.ShowItem )
            SelectAddItem( items[ i ] );
        else if( items[ i ]->IsScenOrGrid() && IsSelectScen && GameOpt.ShowScen )
            SelectAddItem( items[ i ] );
        else if( items[ i ]->IsWall() && IsSelectWall && GameOpt.ShowWall )
            SelectAddItem( items[ i ] );
    }

    if( IsSelectCrit && GameOpt.ShowCrit )
    {
        CritMap& crits = HexMngr.GetCritters();
        for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
            SelectAddCrit( ( *it ).second );
    }

    HexMngr.RefreshMap();
}

void FOMapper::SelectMove( int offs_hx, int offs_hy, int offs_x, int offs_y )
{
    if( Keyb::ShiftDwn && ( !offs_x && !offs_y ) )
        return;
    if( !Keyb::ShiftDwn && ( !offs_hx && !offs_hy ) )
        return;

    // Change moving speed on zooming
    if( Keyb::ShiftDwn )
    {
        static float small_ox = 0.0f, small_oy = 0.0f;
        float        ox = (float) offs_x * GameOpt.SpritesZoom + small_ox;
        float        oy = (float) offs_y * GameOpt.SpritesZoom + small_oy;
        if( offs_x && fabs( ox ) < 1.0f )
            small_ox = ox;
        else
            small_ox = 0.0f;
        if( offs_y && fabs( oy ) < 1.0f )
            small_oy = oy;
        else
            small_oy = 0.0f;
        offs_x = (int) ox;
        offs_y = (int) oy;
    }
    // Check borders
    else
    {
        // Objects
        int switcher = ( SelectedObj.size() ? SelectedObj[ 0 ].MapObj->MapX % 2 : 0 );
        for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
        {
            SelMapObj* obj = &SelectedObj[ i ];
            int        hx = obj->MapObj->MapX;
            int        hy = obj->MapObj->MapY;
            if( GameOpt.MapHexagonal )
            {
                int sw = switcher;
                for( int k = 0, l = abs( offs_hx ); k < l; k++, sw++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hx > 0 ? ( ( sw & 1 ) ? 4 : 3 ) : ( ( sw & 1 ) ? 0 : 1 ) );
                for( int k = 0, l = abs( offs_hy ); k < l; k++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hy > 0 ? 2 : 5 );
            }
            else
            {
                hx += offs_hx;
                hy += offs_hy;
            }
            if( hx < 0 || hy < 0 || hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
                return;                                                                                  // Disable moving
        }

        // Tiles
        for( uint i = 0, j = (uint) SelectedTile.size(); i < j; i++ )
        {
            SelMapTile& stile = SelectedTile[ i ];
            int         hx = stile.HexX;
            int         hy = stile.HexY;
            if( GameOpt.MapHexagonal )
            {
                int sw = switcher;
                for( int k = 0, l = abs( offs_hx ); k < l; k++, sw++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hx > 0 ? ( ( sw & 1 ) ? 4 : 3 ) : ( ( sw & 1 ) ? 0 : 1 ) );
                for( int k = 0, l = abs( offs_hy ); k < l; k++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hy > 0 ? 2 : 5 );
            }
            else
            {
                hx += offs_hx;
                hy += offs_hy;
            }
            if( hx < 0 || hy < 0 || hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
                return;                                                                                  // Disable moving
        }
    }

    // Move map objects
    int switcher = ( SelectedObj.size() ? SelectedObj[ 0 ].MapObj->MapX % 2 : 0 );
    for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
    {
        SelMapObj* obj = &SelectedObj[ i ];

        if( Keyb::ShiftDwn )
        {
            if( !obj->IsItem() )
                continue;

            int ox = obj->MapObj->MItem.OffsetX + offs_x;
            int oy = obj->MapObj->MItem.OffsetY + offs_y;
            ox = CLAMP( ox, -MAX_MOVE_OX, MAX_MOVE_OX );
            oy = CLAMP( oy, -MAX_MOVE_OY, MAX_MOVE_OY );
            if( Keyb::AltDwn )
                ox = oy = 0;

            obj->MapObj->MItem.OffsetX = ox;
            obj->MapObj->MItem.OffsetY = oy;
            obj->MapItem->StartScrX = ox;
            obj->MapItem->StartScrY = oy;
            obj->MapItem->RefreshAnim();
        }
        else
        {
            int hx = obj->MapObj->MapX;
            int hy = obj->MapObj->MapY;
            if( GameOpt.MapHexagonal )
            {
                int sw = switcher;
                for( int k = 0, l = abs( offs_hx ); k < l; k++, sw++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hx > 0 ? ( ( sw & 1 ) ? 4 : 3 ) : ( ( sw & 1 ) ? 0 : 1 ) );
                for( int k = 0, l = abs( offs_hy ); k < l; k++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hy > 0 ? 2 : 5 );
            }
            else
            {
                hx += offs_hx;
                hy += offs_hy;
            }
            hx = CLAMP( hx, 0, HexMngr.GetMaxHexX() - 1 );
            hy = CLAMP( hy, 0, HexMngr.GetMaxHexY() - 1 );
            obj->MapObj->MapX = hx;
            obj->MapObj->MapY = hy;

            if( obj->IsItem() )
            {
                HexMngr.DeleteItem( obj->MapItem, false );
                obj->MapItem->HexX = hx;
                obj->MapItem->HexY = hy;
                HexMngr.PushItem( obj->MapItem );
            }
            else if( obj->IsNpc() )
            {
                HexMngr.RemoveCrit( obj->MapNpc );
                obj->MapNpc->HexX = hx;
                obj->MapNpc->HexY = hy;
                HexMngr.SetCrit( obj->MapNpc );
            }
            else
            {
                continue;
            }

            for( uint k = 0, m = (uint) obj->Childs.size(); k < m; k++ )
            {
                MapObject* mobj = obj->Childs[ k ];
                mobj->MapX = hx;
                mobj->MapY = hy;
            }
        }
    }

    // Move tiles
    struct TileToMove
    {
        Field*             field;
        Field::Tile        tile;
        ProtoMap::TileVec* ptiles;
        ProtoMap::Tile     ptile;
        bool               roof;
        TileToMove() {}
        TileToMove( Field* f, Field::Tile& t, ProtoMap::TileVec* pts, ProtoMap::Tile& pt, bool r ): field( f ), tile( t ), ptiles( pts ), ptile( pt ), roof( r ) {}
    };
    TileToMove tiles_to_move[ 1000 ];
    uint       tiles_to_move_count = 0;

    for( uint i = 0, j = (uint) SelectedTile.size(); i < j; i++ )
    {
        SelMapTile& stile = SelectedTile[ i ];

        if( Keyb::ShiftDwn )
        {
            Field&             f = HexMngr.GetField( stile.HexX, stile.HexY );
            Field::TileVec&    ftiles = ( stile.IsRoof ? f.Roofs : f.Tiles );
            ProtoMap::TileVec& tiles = CurProtoMap->GetTiles( stile.HexX, stile.HexY, stile.IsRoof );

            for( uint k = 0, l = (uint) tiles.size(); k < l; k++ )
            {
                if( tiles[ k ].IsSelected )
                {
                    int ox = tiles[ k ].OffsX + offs_x;
                    int oy = tiles[ k ].OffsY + offs_y;
                    ox = CLAMP( ox, -MAX_MOVE_OX, MAX_MOVE_OX );
                    oy = CLAMP( oy, -MAX_MOVE_OY, MAX_MOVE_OY );
                    if( Keyb::AltDwn )
                        ox = oy = 0;

                    tiles[ k ].OffsX = ox;
                    tiles[ k ].OffsY = oy;
                    ftiles[ k ].OffsX = ox;
                    ftiles[ k ].OffsY = oy;
                }
            }
        }
        else
        {
            int hx = stile.HexX;
            int hy = stile.HexY;
            if( GameOpt.MapHexagonal )
            {
                int sw = switcher;
                for( int k = 0, l = abs( offs_hx ); k < l; k++, sw++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hx > 0 ? ( ( sw & 1 ) ? 4 : 3 ) : ( ( sw & 1 ) ? 0 : 1 ) );
                for( int k = 0, l = abs( offs_hy ); k < l; k++ )
                    MoveHexByDirUnsafe( hx, hy, offs_hy > 0 ? 2 : 5 );
            }
            else
            {
                hx += offs_hx;
                hy += offs_hy;
            }
            hx = CLAMP( hx, 0, HexMngr.GetMaxHexX() - 1 );
            hy = CLAMP( hy, 0, HexMngr.GetMaxHexY() - 1 );
            if( stile.HexX == hx && stile.HexY == hy )
                continue;

            Field&             f = HexMngr.GetField( stile.HexX, stile.HexY );
            Field::TileVec&    ftiles = ( stile.IsRoof ? f.Roofs : f.Tiles );
            ProtoMap::TileVec& tiles = CurProtoMap->GetTiles( stile.HexX, stile.HexY, stile.IsRoof );

            for( uint k = 0; k < tiles.size();)
            {
                if( tiles[ k ].IsSelected )
                {
                    tiles[ k ].HexX = hx;
                    tiles[ k ].HexY = hy;
                    tiles_to_move[ tiles_to_move_count++ ] = TileToMove( &HexMngr.GetField( hx, hy ), ftiles[ k ],
                                                                         &CurProtoMap->GetTiles( hx, hy, stile.IsRoof ), tiles[ k ], stile.IsRoof );
                    tiles.erase( tiles.begin() + k );
                    ftiles.erase( ftiles.begin() + k );
                }
                else
                    k++;
            }
            stile.HexX = hx;
            stile.HexY = hy;
        }
    }

    for( uint i = 0; i < tiles_to_move_count; i++ )
    {
        TileToMove& ttm = tiles_to_move[ i ];
        if( !ttm.roof )
            ttm.field->Tiles.push_back( ttm.tile );
        else
            ttm.field->Roofs.push_back( ttm.tile );
        ttm.ptiles->push_back( ttm.ptile );
    }
}

void FOMapper::SelectDelete()
{
    if( !CurProtoMap )
        return;

    for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
    {
        SelMapObj* o = &SelectedObj[ i ];

        auto       it = std::find( CurProtoMap->MObjects.begin(), CurProtoMap->MObjects.end(), o->MapObj );
        if( it != CurProtoMap->MObjects.end() )
        {
            SAFEREL( o->MapObj );
            CurProtoMap->MObjects.erase( it );
        }

        if( o->IsItem() )
            HexMngr.FinishItem( o->MapItem->GetId(), 0 );
        else if( o->IsNpc() )
            HexMngr.EraseCrit( o->MapNpc->GetId() );

        for( uint k = 0, l = (uint) o->Childs.size(); k < l; k++ )
        {
            it = std::find( CurProtoMap->MObjects.begin(), CurProtoMap->MObjects.end(), o->Childs[ k ] );
            if( it != CurProtoMap->MObjects.end() )
            {
                SAFEREL( o->Childs[ k ] );
                CurProtoMap->MObjects.erase( it );
            }
        }
    }

    for( uint i = 0, j = (uint) SelectedTile.size(); i < j; i++ )
    {
        SelMapTile&        stile = SelectedTile[ i ];
        Field&             f = HexMngr.GetField( stile.HexX, stile.HexY );
        Field::TileVec&    ftiles = ( stile.IsRoof ? f.Roofs : f.Tiles );
        ProtoMap::TileVec& tiles = CurProtoMap->GetTiles( stile.HexX, stile.HexY, stile.IsRoof );

        for( uint k = 0; k < tiles.size();)
        {
            if( tiles[ k ].IsSelected )
            {
                tiles.erase( tiles.begin() + k );
                ftiles.erase( ftiles.begin() + k );
            }
            else
                k++;
        }
    }

    SelectedObj.clear();
    SelectedTile.clear();
    HexMngr.ClearSelTiles();
    SelectClear();
    HexMngr.RefreshMap();
    IntHold = INT_NONE;
    CurMode = CUR_MODE_DEF;
}

MapObject* FOMapper::ParseProto( ushort pid, ushort hx, ushort hy, MapObject* owner, bool is_child /* = false */ )
{
    // Checks
    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item )
        return NULL;
    if( owner && !proto_item->IsCanPickUp() )
        return NULL;
    if( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
        return NULL;
    if( ( proto_item->IsScen() || proto_item->IsGrid() || proto_item->IsWall() ) && owner )
        return NULL;

    // Clear selection
    SelectClear();

    // Create object
    MapObject* mobj = new MapObject();
    mobj->RunTime.FromMap = CurProtoMap;
    mobj->MapObjType = MAP_OBJECT_ITEM;
    if( proto_item->IsScen() || proto_item->IsGrid() || proto_item->IsWall() )
        mobj->MapObjType = MAP_OBJECT_SCENERY;
    mobj->ProtoId = pid;
    mobj->MapX = hx;
    mobj->MapY = hy;
    mobj->Dir = proto_item->Dir;
    mobj->LightDistance = proto_item->LightDistance;
    mobj->LightIntensity = proto_item->LightIntensity;

    // Object in container or inventory
    if( owner )
    {
        if( !owner->UID )
            owner->UID = ++CurProtoMap->LastObjectUID;
        mobj->ContainerUID = owner->UID;
        CurProtoMap->MObjects.push_back( mobj );
        return mobj;
    }

    // Add base object
    mobj->RunTime.MapObjId = ++AnyId;
    if( !HexMngr.AddItem( AnyId, pid, hx, hy, 0, NULL ) )
        return NULL;
    CurProtoMap->MObjects.push_back( mobj );

    // Add childs
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        if( !proto_item->ChildPid[ i ] )
            continue;

        ProtoItem* child = ItemMngr.GetProtoItem( proto_item->ChildPid[ i ] );
        if( !child )
            continue;

        ushort child_hx = hx, child_hy = hy;
        FOREACH_PROTO_ITEM_LINES( proto_item->ChildLines[ i ], child_hx, child_hy, HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY(),;
                                  );

        MapObject* mobj_child = ParseProto( proto_item->ChildPid[ i ], child_hx, child_hy, NULL, true );
        if( mobj_child )
        {
            if( !mobj->UID )
                mobj->UID = ++CurProtoMap->LastObjectUID;
            mobj_child->ParentUID = mobj->UID;
            mobj_child->ParentChildIndex = i;
        }
    }

    // Finish
    if( !is_child )
    {
        SelectAdd( mobj );
        HexMngr.RefreshMap();
        CurMode = CUR_MODE_DEF;
    }
    else
    {
        // mobj->Ru
    }

    return mobj;
}

void FOMapper::ParseTile( uint name_hash, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof )
{
    if( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
        return;

    SelectClear();

    HexMngr.SetTile( name_hash, hx, hy, ox, oy, layer, is_roof, false );
    CurMode = CUR_MODE_DEF;
}

void FOMapper::ParseNpc( ushort pid, ushort hx, ushort hy )
{
    CritData* pnpc = CrMngr.GetProto( pid );
    if( !pnpc )
        return;

    if( hx >= HexMngr.GetMaxHexX() || hy >= HexMngr.GetMaxHexY() )
        return;
    if( HexMngr.GetField( hx, hy ).Crit )
        return;

    uint spr_id = ResMngr.GetCritSprId( pnpc->BaseType, 1, 1, NpcDir );
    if( !spr_id )
        return;

    SelectClear();

    MapObject* mobj = new MapObject();
    mobj->RunTime.FromMap = CurProtoMap;
    mobj->RunTime.MapObjId = ++AnyId;
    mobj->MapObjType = MAP_OBJECT_CRITTER;
    mobj->ProtoId = pid;
    mobj->MapX = hx;
    mobj->MapY = hy;
    mobj->Dir = NpcDir;
    mobj->MCritter.Cond = COND_LIFE;
    CurProtoMap->MObjects.push_back( mobj );

    CritterCl* cr = new CritterCl();
    cr->SetBaseType( pnpc->BaseType );
    cr->DefItemSlotHand->Init( ItemMngr.GetProtoItem( ITEM_DEF_SLOT ) );
    cr->DefItemSlotArmor->Init( ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) );
    cr->HexX = hx;
    cr->HexY = hy;
    cr->SetDir( NpcDir );
    cr->Cond = COND_LIFE;
    cr->Flags = pid;
    memcpy( cr->Params, pnpc->Params, sizeof( pnpc->Params ) );
    cr->Id = AnyId;
    cr->Init();

    // Default parameters
    for( int i = 0; i < MAPOBJ_CRITTER_PARAMS; i++ )
    {
        int param = DefaultCritterParam[ i ];
        mobj->MCritter.ParamIndex[ i ] = param;
        if( param >= 0 && param < MAX_PARAMS )
            mobj->MCritter.ParamValue[ i ] = cr->Params[ param ];
    }

    HexMngr.AddCrit( cr );
    SelectAdd( mobj );

    HexMngr.RefreshMap();
    CurMode = CUR_MODE_DEF;
}

MapObject* FOMapper::ParseMapObj( MapObject* mobj )
{
    if( mobj->MapX >= HexMngr.GetMaxHexX() || mobj->MapY >= HexMngr.GetMaxHexY() )
        return NULL;

    if( mobj->MapObjType == MAP_OBJECT_CRITTER )
    {
        if( HexMngr.GetField( mobj->MapX, mobj->MapY ).Crit )
            return NULL;
        CritData* pnpc = CrMngr.GetProto( mobj->ProtoId );
        if( !pnpc )
            return NULL;

        CurProtoMap->MObjects.push_back( new MapObject( *mobj ) );
        mobj = CurProtoMap->MObjects[ CurProtoMap->MObjects.size() - 1 ];
        mobj->RunTime.FromMap = CurProtoMap;
        mobj->RunTime.MapObjId = ++AnyId;

        CritterCl* cr = new CritterCl();
        cr->SetBaseType( pnpc->BaseType );
        cr->DefItemSlotHand->Init( ItemMngr.GetProtoItem( ITEM_DEF_SLOT ) );
        cr->DefItemSlotArmor->Init( ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) );
        cr->HexX = mobj->MapX;
        cr->HexY = mobj->MapY;
        cr->SetDir( (uchar) mobj->Dir );
        cr->Cond = COND_LIFE;
        cr->Flags = mobj->ProtoId;
        memcpy( cr->Params, pnpc->Params, sizeof( pnpc->Params ) );
        cr->Id = AnyId;
        cr->Init();
        HexMngr.AddCrit( cr );
        HexMngr.AffectCritter( mobj, cr );
        SelectAdd( mobj );
    }
    else if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
    {
        ProtoItem* proto = ItemMngr.GetProtoItem( mobj->ProtoId );
        if( !proto )
            return NULL;

        CurProtoMap->MObjects.push_back( new MapObject( *mobj ) );
        mobj = CurProtoMap->MObjects.back();
        mobj->RunTime.FromMap = CurProtoMap;

        if( mobj->MapObjType == MAP_OBJECT_ITEM && mobj->ContainerUID )
            return mobj;

        mobj->RunTime.MapObjId = ++AnyId;
        if( HexMngr.AddItem( AnyId, mobj->ProtoId, mobj->MapX, mobj->MapY, 0, NULL ) )
        {
            HexMngr.AffectItem( mobj, HexMngr.GetItemById( AnyId ) );
            SelectAdd( mobj );
        }
    }
    return mobj;
}

void FOMapper::BufferCopy()
{
    if( !CurProtoMap )
        return;

    for( uint i = 0, j = (uint) MapObjBuffer.size(); i < j; i++ )
        MapObjBuffer[ i ]->Release();
    MapObjBuffer.clear();
    TilesBuffer.clear();

    for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
    {
        MapObjBuffer.push_back( new MapObject( *SelectedObj[ i ].MapObj ) );
        for( uint k = 0, l = (uint) SelectedObj[ i ].Childs.size(); k < l; k++ )
            MapObjBuffer.push_back( new MapObject( *SelectedObj[ i ].Childs[ k ] ) );
    }

    for( uint i = 0, j = (uint) SelectedTile.size(); i < j; i++ )
    {
        ushort             hx = SelectedTile[ i ].HexX;
        ushort             hy = SelectedTile[ i ].HexY;
        Field&             f = HexMngr.GetField( hx, hy );
        Field::TileVec&    ftiles = ( SelectedTile[ i ].IsRoof ? f.Roofs : f.Tiles );
        ProtoMap::TileVec& tiles = CurProtoMap->GetTiles( hx, hy, SelectedTile[ i ].IsRoof );

        for( uint k = 0, l = (uint) tiles.size(); k < l; k++ )
        {
            if( tiles[ k ].IsSelected )
            {
                TileBuf tb;
                tb.NameHash = tiles[ k ].NameHash;
                tb.HexX = hx;
                tb.HexY = hy;
                tb.OffsX = tiles[ k ].OffsX;
                tb.OffsY = tiles[ k ].OffsY;
                tb.Layer = tiles[ k ].Layer;
                tb.IsRoof = SelectedTile[ i ].IsRoof;
                TilesBuffer.push_back( tb );
            }
        }
    }
}

void FOMapper::BufferCut()
{
    if( !CurProtoMap )
        return;

    BufferCopy();
    SelectDelete();
}

void FOMapper::BufferPaste( int hx, int hy )
{
    if( !CurProtoMap )
        return;

    SelectClear();

    // Fix UIDs
    for( uint i = 0, j = (uint) MapObjBuffer.size(); i < j; i++ )
    {
        MapObject* mobj = MapObjBuffer[ i ];
        if( mobj->UID )
        {
            uint old_uid = mobj->UID;
            mobj->UID = ++CurProtoMap->LastObjectUID;
            for( uint k = 0, l = (uint) MapObjBuffer.size(); k < l; k++ )
            {
                MapObject* mobj_ = MapObjBuffer[ k ];
                if( mobj_->ContainerUID == old_uid )
                    mobj_->ContainerUID = mobj->UID;
            }
        }
    }

    // Paste map objects
    CurProtoMap->MObjects.reserve( CurProtoMap->MObjects.size() + MapObjBuffer.size() * 2 );
    for( uint i = 0, j = (uint) MapObjBuffer.size(); i < j; i++ )
    {
        ParseMapObj( MapObjBuffer[ i ] );
    }

    // Paste tiles
    for( uint i = 0, j = (uint) TilesBuffer.size(); i < j; i++ )
    {
        TileBuf& tb = TilesBuffer[ i ];

        if( tb.HexX < HexMngr.GetMaxHexX() && tb.HexY < HexMngr.GetMaxHexY() )
        {
            // Create
            HexMngr.SetTile( tb.NameHash, tb.HexX, tb.HexY, tb.OffsX, tb.OffsY, tb.Layer, tb.IsRoof, true );

            // Select helper
            bool sel_added = false;
            for( uint i = 0, j = (uint) SelectedTile.size(); i < j; i++ )
            {
                SelMapTile& stile = SelectedTile[ i ];
                if( stile.HexX == tb.HexX && stile.HexY == tb.HexY && stile.IsRoof == tb.IsRoof )
                {
                    sel_added = true;
                    break;
                }
            }
            if( !sel_added )
                SelectedTile.push_back( SelMapTile( tb.HexX, tb.HexY, tb.IsRoof ) );
        }
    }
}

void FOMapper::CurDraw()
{
    switch( CurMode )
    {
    case CUR_MODE_DEF:
    case CUR_MODE_MOVE:
    {
        AnyFrames* anim = ( CurMode == CUR_MODE_DEF ? CurPDef : CurPHand );
        if( anim )
        {
            SpriteInfo* si = SprMngr.GetSpriteInfo( anim->GetCurSprId() );
            if( si )
            {
                int x = GameOpt.MouseX - ( si->Width / 2 ) + si->OffsX;
                int y = GameOpt.MouseY - si->Height + si->OffsY;
                SprMngr.DrawSprite( anim, x, y, COLOR_IFACE );
            }
        }
    }
    break;
    case CUR_MODE_DRAW:
        if( IsObjectMode() && ( *CurItemProtos ).size() )
        {
            ProtoItem& proto_item = ( *CurItemProtos )[ GetTabIndex() ];

            ushort     hx, hy;
            if( !HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy ) )
                break;

            uint        spr_id = proto_item.GetCurSprId();
            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
            if( si )
            {
                int x = HexMngr.GetField( hx, hy ).ScrX - ( si->Width / 2 ) + si->OffsX + HEX_OX + GameOpt.ScrOx;
                int y = HexMngr.GetField( hx, hy ).ScrY - si->Height + si->OffsY + HEX_OY + GameOpt.ScrOy;

                SprMngr.DrawSpriteSize( spr_id, (int) ( x / GameOpt.SpritesZoom ), (int) ( y / GameOpt.SpritesZoom ),
                                        si->Width / GameOpt.SpritesZoom, si->Height / GameOpt.SpritesZoom, true, false );
            }
        }
        else if( IsTileMode() && CurTileHashes->size() )
        {
            AnyFrames* anim = ResMngr.GetItemAnim( ( *CurTileHashes )[ GetTabIndex() ] );
            if( !anim )
                anim = ItemHex::DefaultAnim;

            ushort hx, hy;
            if( !HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy ) )
                break;

            SpriteInfo* si = SprMngr.GetSpriteInfo( anim->GetCurSprId() );
            if( si )
            {
                int x = HexMngr.GetField( hx, hy ).ScrX - ( si->Width / 2 ) + si->OffsX;
                int y = HexMngr.GetField( hx, hy ).ScrY - si->Height + si->OffsY;
                if( !DrawRoof )
                {
                    x += TILE_OX;
                    y += TILE_OY;
                }
                else
                {
                    x += ROOF_OX;
                    y += ROOF_OY;
                }

                SprMngr.DrawSpriteSize( anim, (int) ( ( x + GameOpt.ScrOx ) / GameOpt.SpritesZoom ), (int) ( ( y + GameOpt.ScrOy ) / GameOpt.SpritesZoom ),
                                        si->Width / GameOpt.SpritesZoom, si->Height / GameOpt.SpritesZoom, true, false );
            }
        }
        else if( IsCritMode() && CurNpcProtos->size() )
        {
            uint spr_id = ResMngr.GetCritSprId( ( *CurNpcProtos )[ GetTabIndex() ]->BaseType, 1, 1, NpcDir );
            if( !spr_id )
                spr_id = ItemHex::DefaultAnim->GetSprId( 0 );

            ushort hx, hy;
            if( !HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy ) )
                break;

            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
            if( si )
            {
                int x = HexMngr.GetField( hx, hy ).ScrX - ( si->Width / 2 ) + si->OffsX;
                int y = HexMngr.GetField( hx, hy ).ScrY - si->Height + si->OffsY;

                SprMngr.DrawSpriteSize( spr_id, (int) ( ( x + GameOpt.ScrOx + HEX_OX ) / GameOpt.SpritesZoom ), (int) ( ( y + GameOpt.ScrOy + HEX_OY ) / GameOpt.SpritesZoom ),
                                        si->Width / GameOpt.SpritesZoom, si->Height / GameOpt.SpritesZoom, true, false );
            }
        }
        else
        {
            CurMode = CUR_MODE_DEF;
        }
        break;
    default:
        CurMode = CUR_MODE_DEF;
        break;
    }
}

void FOMapper::CurRMouseUp()
{
    if( IntHold == INT_NONE )
    {
        if( CurMode == CUR_MODE_MOVE )
        {
            /*if(SelectedTile.size())
               {
                    for(int i=0,j=SelectedTile.size();i<j;i++)
                    {
                            SelMapTile* t=&SelectedTile[i];

                            if(!t->IsRoof) CurProtoMap->Tiles.SetTile(t->TileX,t->TileY,t->Pid);
                            else CurProtoMap->SetRoof(t->TileX,t->TileY,t->Pid);
                    }

                    HexMngr.ParseSelTiles();
                    HexMngr.RefreshMap();
               }

               SelectClear();*/
            CurMode = CUR_MODE_DEF;
        }
        else if( CurMode == CUR_MODE_DRAW )
        {
            CurMode = CUR_MODE_DEF;
        }
        else if( CurMode == CUR_MODE_DEF )
        {
            if( SelectedObj.size() || SelectedTile.size() )
                CurMode = CUR_MODE_MOVE;
            else if( IsObjectMode() || IsTileMode() || IsCritMode() )
                CurMode = CUR_MODE_DRAW;
        }
    }
}

void FOMapper::CurMMouseDown()
{
    if( SelectedObj.empty() )
    {
        NpcDir++;
        if( NpcDir >= DIRS_COUNT )
            NpcDir = 0;

        DrawRoof = !DrawRoof;
    }
    else
    {
        for( uint i = 0, j = (uint) SelectedObj.size(); i < j; i++ )
        {
            SelMapObj* o = &SelectedObj[ i ];

            if( o->IsNpc() )
            {
                int dir = o->MapNpc->GetDir() + 1;
                if( dir >= DIRS_COUNT )
                    dir = 0;
                o->MapNpc->SetDir( dir );
                o->MapObj->Dir = dir;
            }
        }
    }
}

bool FOMapper::GetMouseHex( ushort& hx, ushort& hy )
{
    hx = hy = 0;
    if( IntVisible && IsCurInRectNoTransp( IntMainPic->GetCurSprId(), IntWMain, IntX, IntY ) )
        return false;
    if( ObjVisible && !SelectedObj.empty() && IsCurInRectNoTransp( ObjWMainPic->GetCurSprId(), ObjWMain, 0, 0 ) )
        return false;

    // if(ConsoleEdit && IsCurInRectNoTransp(ConsolePic,Main,0,0)) //TODO:
    return HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy );
}

void FOMapper::ConsoleDraw()
{
    if( ConsoleEdit )
        SprMngr.DrawSprite( ConsolePic, IntX + ConsolePicX, ( IntVisible ? IntY : MODE_HEIGHT ) + ConsolePicY );

    if( ConsoleEdit )
    {
        char        str_to_edit[ 2048 ];
        static bool show_cur = true;
        static uint show_cur_last_time = Timer::FastTick();

        if( Timer::FastTick() > show_cur_last_time + 400 )
        {
            show_cur = !show_cur;
            show_cur_last_time = Timer::FastTick();
        }

        str_to_edit[ 0 ] = 0;
        Str::Copy( str_to_edit, ConsoleStr );
        if( show_cur )
            str_to_edit[ ConsoleCur ] = '!';
        else
            str_to_edit[ ConsoleCur ] = '.';

        str_to_edit[ ConsoleCur + 1 ] = 0;
        Str::Append( str_to_edit, &ConsoleStr[ ConsoleCur ] );
        SprMngr.DrawStr( Rect( IntX + ConsoleTextX, ( IntVisible ? IntY : MODE_HEIGHT ) + ConsoleTextY, MODE_WIDTH, MODE_HEIGHT ), str_to_edit, FT_NOBREAK );
    }
}

void FOMapper::ConsoleKeyDown( uchar dik )
{
    if( dik == DIK_RETURN || dik == DIK_NUMPADENTER )
    {
        if( ConsoleEdit )
        {
            if( !ConsoleStr[ 0 ] )
            {
                ConsoleEdit = false;
            }
            else
            {
                ConsoleHistory.push_back( string( ConsoleStr ) );
                for( uint i = 0; i < ConsoleHistory.size() - 1; i++ )
                {
                    if( ConsoleHistory[ i ] == ConsoleHistory[ ConsoleHistory.size() - 1 ] )
                    {
                        ConsoleHistory.erase( ConsoleHistory.begin() + i );
                        i = -1;
                    }
                }
                ConsoleHistoryCur = (int) ConsoleHistory.size();

                bool process_command = true;
                if( MapperFunctions.ConsoleMessage && Script::PrepareContext( MapperFunctions.ConsoleMessage, _FUNC_, "Mapper" ) )
                {
                    CScriptString* sstr = new CScriptString( ConsoleStr );
                    Script::SetArgObject( sstr );
                    if( Script::RunPrepared() && Script::GetReturnedBool() )
                        process_command = false;
                    Str::Copy( ConsoleStr, sstr->c_str() );
                    sstr->Release();
                }

                AddMess( ConsoleStr );
                if( process_command )
                    ParseCommand( ConsoleStr );
                ConsoleStr[ 0 ] = 0;
                ConsoleCur = 0;
            }
        }
        else
        {
            ConsoleEdit = true;
            ConsoleStr[ 0 ] = 0;
            ConsoleCur = 0;
            ConsoleHistoryCur = (int) ConsoleHistory.size();
        }

        return;
    }

    switch( dik )
    {
    case DIK_UP:
        if( ConsoleHistoryCur - 1 < 0 )
            return;
        ConsoleHistoryCur--;
        Str::Copy( ConsoleStr, ConsoleHistory[ ConsoleHistoryCur ].c_str() );
        ConsoleCur = (int) Str::Length( ConsoleStr );
        return;
    case DIK_DOWN:
        if( ConsoleHistoryCur + 1 >= (int) ConsoleHistory.size() )
        {
            ConsoleHistoryCur = (int) ConsoleHistory.size();
            Str::Copy( ConsoleStr, "" );
            ConsoleCur = 0;
            return;
        }
        ConsoleHistoryCur++;
        Str::Copy( ConsoleStr, ConsoleHistory[ ConsoleHistoryCur ].c_str() );
        ConsoleCur = (int) Str::Length( ConsoleStr );
        return;
    default:
        Keyb::GetChar( dik, ConsoleStr, &ConsoleCur, MAX_NET_TEXT, KIF_NO_SPEC_SYMBOLS );

        ConsoleLastKey = dik;
        ConsoleKeyTick = Timer::FastTick();
        ConsoleAccelerate = 1;
        return;
    }
}

void FOMapper::ConsoleKeyUp( uchar key )
{
    ConsoleLastKey = 0;
}

void FOMapper::ConsoleProcess()
{
    if( !ConsoleLastKey )
        return;

    if( (int) ( Timer::FastTick() - ConsoleKeyTick ) >= CONSOLE_KEY_TICK - ConsoleAccelerate )
    {
        ConsoleKeyTick = Timer::FastTick();

        ConsoleAccelerate = CONSOLE_MAX_ACCELERATE;
//		if((ConsoleAccelerate*=4)>=CONSOLE_MAX_ACCELERATE) ConsoleAccelerate=CONSOLE_MAX_ACCELERATE;

        Keyb::GetChar( ConsoleLastKey, ConsoleStr, &ConsoleCur, MAX_NET_TEXT, KIF_NO_SPEC_SYMBOLS );
    }
}

void FOMapper::ParseCommand( const char* cmd )
{
    if( !cmd || !*cmd )
        return;

    // Load map
    if( *cmd == '~' )
    {
        cmd++;
        char map_name[ 256 ];
        Str::CopyWord( map_name, cmd, ' ', false );
        if( !map_name[ 0 ] )
        {
            AddMess( "Error parse map name." );
            return;
        }

        ProtoMap* pmap = new ProtoMap();
        FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
        if( !pmap->Init( 0xFFFF, map_name, PT_SERVER_MAPS ) )
        {
            AddMess( "File not found or truncated." );
            FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
            return;
        }
        FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );

        SelectClear();
        if( !HexMngr.SetProtoMap( *pmap ) )
        {
            AddMess( "Load map fail." );
            return;
        }

        HexMngr.FindSetCenter( pmap->Header.WorkHexX, pmap->Header.WorkHexY );
        CurProtoMap = pmap;
        LoadedProtoMaps.push_back( pmap );

        AddMess( "Load map complete." );
    }
    // Save map
    else if( *cmd == '^' )
    {
        cmd++;

        char map_name[ 256 ];
        Str::CopyWord( map_name, cmd, ' ', false );
        if( !map_name[ 0 ] )
        {
            AddMess( "Error parse map name." );
            return;
        }

        if( !CurProtoMap )
        {
            AddMess( "Map not loaded." );
            return;
        }

        SelectClear();
        HexMngr.RefreshMap();
        FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
        if( CurProtoMap->Save( map_name, PT_SERVER_MAPS ) )
            AddMess( "Save map success." );
        else
            AddMess( "Save map fail, see log." );
        FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
    }
    // Run script
    else if( *cmd == '#' )
    {
        cmd++;
        char func_name[ 1024 ];
        char str[ 1024 ] = { 0 };
        if( sscanf( cmd, "%s", func_name ) != 1 || !strlen( func_name ) )
        {
            AddMess( "Function name not typed." );
            return;
        }
        Str::Copy( str, cmd + strlen( func_name ) );
        while( str[ 0 ] == ' ' )
            Str::CopyBack( str );

        // Reparse module
        int bind_id;
        if( Str::Substring( func_name, "@" ) )
            bind_id = Script::Bind( func_name, "string %s(string)", true );
        else
            bind_id = Script::Bind( "mapper_main", func_name, "string %s(string)", true );

        if( bind_id > 0 && Script::PrepareContext( bind_id, _FUNC_, "Mapper" ) )
        {
            CScriptString* sstr = new CScriptString( str );
            Script::SetArgObject( sstr );
            if( Script::RunPrepared() )
            {
                CScriptString* sstr_ = (CScriptString*) Script::GetReturnedObject();
                AddMessFormat( Str::FormatBuf( "Result: %s", sstr_->c_str() ) );
                sstr_->Release();
            }
            else
            {
                AddMess( "Script execution fail." );
            }
            sstr->Release();
        }
        else
        {
            AddMess( "Function not found." );
            return;
        }
    }
    // Critter animations
    else if( *cmd == '@' )
    {
        AddMess( "Playing critter animations." );

        if( !CurProtoMap )
        {
            AddMess( "Map not loaded." );
            return;
        }

        cmd++;
        char anim[ 512 ];
        Str::Copy( anim, cmd );
        Str::Lower( anim );
        Str::EraseChars( anim, ' ' );
        if( !strlen( anim ) || strlen( anim ) & 1 )
            return;

        if( SelectedObj.size() )
        {
            for( uint i = 0; i < SelectedObj.size(); i++ )
            {
                SelMapObj& sobj = SelectedObj[ i ];
                if( sobj.MapNpc )
                {
                    sobj.MapNpc->ClearAnim();
                    for( uint j = 0; j < strlen( anim ) / 2; j++ )
                        sobj.MapNpc->Animate( anim[ j * 2 ] - 'a' + 1, anim[ j * 2 + 1 ] - 'a' + 1, NULL );
                }
            }
        }
        else
        {
            CritMap& crits = HexMngr.GetCritters();
            for( auto it = crits.begin(); it != crits.end(); ++it )
            {
                CritterCl* cr = ( *it ).second;
                cr->ClearAnim();
                for( uint j = 0; j < strlen( anim ) / 2; j++ )
                    cr->Animate( anim[ j * 2 ] - 'a' + 1, anim[ j * 2 + 1 ] - 'a' + 1, NULL );
            }
        }
    }
    // Other
    else if( *cmd == '*' )
    {
        char cmd_[ MAX_FOTEXT ];
        if( sscanf( cmd + 1, "%s", cmd_ ) != 1 )
            return;

        if( Str::CompareCase( cmd_, "new" ) )
        {
            ProtoMap* pmap = new ProtoMap();
            pmap->GenNew();

            if( !HexMngr.SetProtoMap( *pmap ) )
            {
                AddMess( "Create map fail, see log." );
                return;
            }

            AddMess( "Create map success." );
            HexMngr.FindSetCenter( 150, 150 );

            CurProtoMap = pmap;
            LoadedProtoMaps.push_back( pmap );
        }
        else if( Str::CompareCase( cmd_, "unload" ) )
        {
            AddMess( "Unload map." );

            auto it = std::find( LoadedProtoMaps.begin(), LoadedProtoMaps.end(), CurProtoMap );
            if( it == LoadedProtoMaps.end() )
                return;

            LoadedProtoMaps.erase( it );
            SelectedObj.clear();
            CurProtoMap->Clear();
            SAFEREL( CurProtoMap );

            if( LoadedProtoMaps.empty() )
            {
                HexMngr.UnloadMap();
                return;
            }

            CurProtoMap = LoadedProtoMaps[ 0 ];
            if( HexMngr.SetProtoMap( *CurProtoMap ) )
            {
                HexMngr.FindSetCenter( CurProtoMap->Header.WorkHexX, CurProtoMap->Header.WorkHexY );
                return;
            }
        }
        else if( Str::CompareCase( cmd_, "scripts" ) )
        {
            FinishScriptSystem();
            InitScriptSystem();
            RunStartScript();
            AddMess( "Scripts reloaded." );
        }
        else if( !CurProtoMap )
            return;

        if( Str::CompareCase( cmd_, "dupl" ) )
        {
            AddMess( "Find duplicates." );

            for( int hx = 0; hx < HexMngr.GetMaxHexX(); hx++ )
            {
                for( int hy = 0; hy < HexMngr.GetMaxHexY(); hy++ )
                {
                    Field&    f = HexMngr.GetField( hx, hy );
                    UShortVec pids;
                    for( auto it = f.Items.begin(), end = f.Items.end(); it != end; ++it )
                        pids.push_back( ( *it )->GetProtoId() );
                    std::sort( pids.begin(), pids.end() );
                    for( uint i = 0, j = (uint) pids.size(), same = 0; i < j; i++ )
                    {
                        if( i < j - 1 && pids[ i ] == pids[ i + 1 ] )
                            same++;
                        else
                        {
                            if( same )
                                AddMessFormat( "%d duplicates of %d on %d:%d.", same, pids[ i ], hx, hy );
                            same = 0;
                        }
                    }
                }
            }
        }
        else if( Str::CompareCase( cmd_, "scroll" ) )
        {
            AddMess( "Find alone scroll blockers." );

            for( int hx = 0; hx < HexMngr.GetMaxHexX(); hx++ )
            {
                for( int hy = 0; hy < HexMngr.GetMaxHexY(); hy++ )
                {
                    if( !HexMngr.GetField( hx, hy ).ScrollBlock )
                        continue;
                    ushort hx_ = hx, hy_ = hy;
                    int    count = 0;
                    MoveHexByDir( hx_, hy_, 4, HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
                    for( int i = 0; i < 5; i++ )
                    {
                        if( HexMngr.GetField( hx_, hy_ ).ScrollBlock )
                            count++;
                        MoveHexByDir( hx_, hy_, i, HexMngr.GetMaxHexX(), HexMngr.GetMaxHexY() );
                    }
                    if( count < 2 )
                        AddMessFormat( "Alone scroll blocker on %03d:%03d.", hx, hy );
                }
            }
        }
        else if( Str::CompareCase( cmd_, "pidpos" ) )
        {
            AddMess( "Find pid positions." );

            cmd += Str::Length( "*pidpos" );
            uint pid;
            if( sscanf( cmd, "%u", &pid ) != 1 )
            {
                AddMess( "Invalid args." );
                return;
            }

            uint count = 1;
            for( auto it = CurProtoMap->MObjects.begin(), end = CurProtoMap->MObjects.end(); it != end; ++it )
            {
                MapObject* mobj = *it;
                if( mobj->ProtoId == pid )
                {
                    AddMessFormat( "%04u) %03d:%03d %s", count, mobj->MapX, mobj->MapY, mobj->ContainerUID ? "in container" : "" );
                    count++;
                }
            }
        }
        else if( Str::CompareCase( cmd_, "size" ) )
        {
            AddMess( "Resize map." );

            cmd += Str::Length( "*size" );
            int maxhx, maxhy;
            if( sscanf( cmd, "%d%d", &maxhx, &maxhy ) != 2 )
            {
                AddMess( "Invalid args." );
                return;
            }

            SelectClear();
            HexMngr.UnloadMap();
            ushort old_maxhx = CurProtoMap->Header.MaxHexX;
            ushort old_maxhy = CurProtoMap->Header.MaxHexY;
            maxhx = CLAMP( maxhx, MAXHEX_MIN, MAXHEX_MAX );
            maxhy = CLAMP( maxhy, MAXHEX_MIN, MAXHEX_MAX );
            if( CurProtoMap->Header.WorkHexX >= maxhx )
                CurProtoMap->Header.WorkHexX = maxhx - 1;
            if( CurProtoMap->Header.WorkHexY >= maxhy )
                CurProtoMap->Header.WorkHexY = maxhy - 1;
            CurProtoMap->Header.MaxHexX = maxhx;
            CurProtoMap->Header.MaxHexY = maxhy;

            // Delete truncated map objects
            if( maxhx < old_maxhx || maxhy < old_maxhy )
            {
                for( auto it = CurProtoMap->MObjects.begin(); it != CurProtoMap->MObjects.end();)
                {
                    MapObject* mobj = *it;
                    if( mobj->MapX >= maxhx || mobj->MapY >= maxhy )
                    {
                        SAFEREL( mobj );
                        it = CurProtoMap->MObjects.erase( it );
                    }
                    else
                        ++it;
                }
            }

            // Resize tiles
            ProtoMap::TileVecVec tiles_field = CurProtoMap->TilesField;
            ProtoMap::TileVecVec roofs_field = CurProtoMap->RoofsField;
            CurProtoMap->TilesField.clear();
            CurProtoMap->TilesField.resize( maxhx * maxhy );
            CurProtoMap->RoofsField.clear();
            CurProtoMap->RoofsField.resize( maxhx * maxhy );
            for( int hy = 0; hy < min( (ushort) maxhy, old_maxhy ); hy++ )
            {
                for( int hx = 0; hx < min( (ushort) maxhx, old_maxhx ); hx++ )
                {
                    for( int r = 0; r <= 1; r++ )
                    {
                        ProtoMap::TileVecVec& field_from = ( !r ? tiles_field : roofs_field );
                        ProtoMap::TileVecVec& field_to = ( !r ? CurProtoMap->TilesField : CurProtoMap->RoofsField );
                        ProtoMap::TileVec&    tiles_from = field_from[ hy * old_maxhx + hx ];
                        ProtoMap::TileVec&    tiles_to = field_to[ hy * maxhx + hx ];
                        tiles_to = tiles_from;
                    }
                }
            }

            HexMngr.SetProtoMap( *CurProtoMap );
            HexMngr.FindSetCenter( CurProtoMap->Header.WorkHexX, CurProtoMap->Header.WorkHexY );
        }
        else if( Str::CompareCase( cmd_, "hex" ) )
        {
            AddMess( "Show hex objects." );

            cmd += Str::Length( "*hex" );
            int hx, hy;
            if( sscanf( cmd, "%d%d", &hx, &hy ) != 2 )
            {
                AddMess( "Invalid args." );
                return;
            }

            // Show all objects in this hex
            uint count = 1;
            for( auto it = CurProtoMap->MObjects.begin(); it != CurProtoMap->MObjects.end(); ++it )
            {
                MapObject* mobj = *it;
                if( mobj->MapX == hx && mobj->MapY == hy )
                {
                    AddMessFormat( "%04u) pid %03d %s", count, mobj->ProtoId, mobj->ContainerUID ? "in container" : "" );
                    count++;
                }
            }
        }
    }
    else
    {
        AddMess( "Unknown command." );
    }
}

void FOMapper::AddMess( const char* message_text )
{
    // Text
    char str[ MAX_FOTEXT ];
    Str::Format( str, "|%u %c |%u %s\n", COLOR_TEXT, 149, COLOR_TEXT, message_text );

    // Time
    DateTime dt;
    Timer::GetCurrentDateTime( dt );
    char     mess_time[ 64 ];
    Str::Format( mess_time, "%02d:%02d:%02d ", dt.Hour, dt.Minute, dt.Second );

    // Add
    MessBox.push_back( MessBoxMessage( 0, str, mess_time ) );

    // Generate mess box
    MessBoxScroll = 0;
    MessBoxGenerate();
}

void FOMapper::AddMessFormat( const char* message_text, ... )
{
    char    str[ MAX_FOTEXT ];
    va_list list;
    va_start( list, message_text );
    vsprintf( str, message_text, list );
    va_end( list );
    AddMess( str );
}

void FOMapper::MessBoxGenerate()
{
    MessBoxCurText = "";
    if( MessBox.empty() )
        return;

    Rect ir( IntWWork[ 0 ] + IntX, IntWWork[ 1 ] + IntY, IntWWork[ 2 ] + IntX, IntWWork[ 3 ] + IntY );
    int  max_lines = ir.H() / 10;
    if( ir.IsZero() )
        max_lines = 20;

    int cur_mess = (int) MessBox.size() - 1;
    for( int i = 0, j = 0; cur_mess >= 0; cur_mess-- )
    {
        MessBoxMessage& m = MessBox[ cur_mess ];
        // Scroll
        j++;
        if( j <= MessBoxScroll )
            continue;
        // Add to message box
        if( GameOpt.MsgboxInvert )
            MessBoxCurText += m.Mess;                      // Back
        else
            MessBoxCurText = m.Mess + MessBoxCurText;      // Front
        i++;
        if( i >= max_lines )
            break;
    }
}

void FOMapper::MessBoxDraw()
{
    if( !IntVisible )
        return;
    if( MessBoxCurText.empty() )
        return;

    uint flags = 0;
    if( !GameOpt.MsgboxInvert )
        flags |= FT_UPPER | FT_BOTTOM;

    SprMngr.DrawStr( Rect( IntWWork[ 0 ] + IntX, IntWWork[ 1 ] + IntY, IntWWork[ 2 ] + IntX, IntWWork[ 3 ] + IntY ), MessBoxCurText.c_str(), flags );
}

bool FOMapper::SaveLogFile()
{
    if( MessBox.empty() )
        return false;

    DateTime dt;
    Timer::GetCurrentDateTime( dt );
    char     log_path[ MAX_FOPATH ];
    Str::Format( log_path, DIR_SLASH_SD "mapper_messbox_%02d-%02d-%d_%02d-%02d-%02d.txt",
                 dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second );

    void* f = FileOpen( log_path, true );
    if( !f )
        return false;


    char   cur_mess[ MAX_FOTEXT ];
    string fmt_log;
    for( uint i = 0; i < MessBox.size(); ++i )
    {
        Str::Copy( cur_mess, MessBox[ i ].Mess.c_str() );
        Str::EraseWords( cur_mess, '|', ' ' );
        fmt_log += MessBox[ i ].Time + string( cur_mess );
    }

    FileWrite( f, fmt_log.c_str(), (uint) fmt_log.length() );
    FileClose( f );
    return true;
}

void FOMapper::InitScriptSystem()
{
    WriteLog( "Script system initialization...\n" );

    // Init
    if( !Script::Init( false, new ScriptPragmaCallback( PRAGMA_MAPPER ) ) )
    {
        WriteLog( "Script system initialization fail.\n" );
        return;
    }

    // Bind vars and functions, look bind.h
    asIScriptEngine* engine = Script::GetEngine();
    #define BIND_MAPPER
    #define BIND_CLASS    FOMapper::SScriptFunc::
    #define BIND_ASSERT( x )           if( ( x ) < 0 ) { WriteLogF( _FUNC_, " - Bind error, line<%d>.\n", __LINE__ ); }
    #include <ScriptBind.h>

    // Load scripts
    FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
    Script::SetScriptsPath( PT_SERVER_SCRIPTS );

    // Get config file
    FileManager scripts_cfg;
    scripts_cfg.LoadFile( SCRIPTS_LST, PT_SERVER_SCRIPTS );
    if( !scripts_cfg.IsLoaded() )
    {
        WriteLog( "Config file<%s> not found.\n", SCRIPTS_LST );
        FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
        return;
    }

    // Load script modules
    Script::Undefine( NULL );
    Script::Define( "__MAPPER" );
    Script::ReloadScripts( (char*) scripts_cfg.GetBuf(), "mapper", false, "MAPPER_" );
    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );

    // Bind game functions
    ReservedScriptFunction BindGameFunc[] =
    {
        { &MapperFunctions.Start, "start", "void %s()" },
        { &MapperFunctions.Loop, "loop", "uint %s()" },
        { &MapperFunctions.ConsoleMessage, "console_message", "bool %s(string&)" },
        { &MapperFunctions.RenderIface, "render_iface", "void %s(uint)" },
        { &MapperFunctions.RenderMap, "render_map", "void %s()" },
        { &MapperFunctions.MouseDown, "mouse_down", "bool %s(int)" },
        { &MapperFunctions.MouseUp, "mouse_up", "bool %s(int)" },
        { &MapperFunctions.MouseMove, "mouse_move", "void %s(int,int)" },
        { &MapperFunctions.KeyDown, "key_down", "bool %s(uint8)" },
        { &MapperFunctions.KeyUp, "key_up", "bool %s(uint8)" },
        { &MapperFunctions.InputLost, "input_lost", "void %s()" },
        { &MapperFunctions.CritterAnimation, "critter_animation", "string@ %s(int,uint,uint,uint,uint&,uint&,int&,int&)" },
        { &MapperFunctions.CritterAnimationSubstitute, "critter_animation_substitute", "bool %s(int,uint,uint,uint,uint&,uint&,uint&)" },
        { &MapperFunctions.CritterAnimationFallout, "critter_animation_fallout", "bool %s(uint,uint&,uint&,uint&,uint&,uint&)" },
    };
    Script::BindReservedFunctions( (char*) scripts_cfg.GetBuf(), "mapper", BindGameFunc, sizeof( BindGameFunc ) / sizeof( BindGameFunc[ 0 ] ) );

    WriteLog( "Script system initialization complete.\n" );
}

void FOMapper::FinishScriptSystem()
{
    Script::Finish();
    memzero( &MapperFunctions, sizeof( MapperFunctions ) );
}

void FOMapper::RunStartScript()
{
    if( MapperFunctions.Start && Script::PrepareContext( MapperFunctions.Start, _FUNC_, "Mapper" ) )
        Script::RunPrepared();
}

void FOMapper::DrawIfaceLayer( uint layer )
{
    if( MapperFunctions.RenderIface && Script::PrepareContext( MapperFunctions.RenderIface, _FUNC_, "Mapper" ) )
    {
        SpritesCanDraw = true;
        Script::SetArgUInt( layer );
        Script::RunPrepared();
        SpritesCanDraw = false;
    }
}

#define SCRIPT_ERROR( error )          do { ScriptLastError = error; Script::LogError( _FUNC_, error ); } while( 0 )
#define SCRIPT_ERROR_RX( error, x )    do { ScriptLastError = error; Script::LogError( _FUNC_, error ); return x; } while( 0 )
#define SCRIPT_ERROR_R( error )        do { ScriptLastError = error; Script::LogError( _FUNC_, error ); return; } while( 0 )
#define SCRIPT_ERROR_R0( error )       do { ScriptLastError = error; Script::LogError( _FUNC_, error ); return 0; } while( 0 )
static string ScriptLastError;

CScriptString* FOMapper::SScriptFunc::MapperObject_get_ScriptName( MapObject& mobj )
{
    return new CScriptString( mobj.ScriptName );
}

void FOMapper::SScriptFunc::MapperObject_set_ScriptName( MapObject& mobj, CScriptString* str )
{
    Str::Copy( mobj.ScriptName, str ? str->c_str() : NULL );
}

CScriptString* FOMapper::SScriptFunc::MapperObject_get_FuncName( MapObject& mobj )
{
    return new CScriptString( mobj.FuncName );
}

void FOMapper::SScriptFunc::MapperObject_set_FuncName( MapObject& mobj, CScriptString* str )
{
    Str::Copy( mobj.FuncName, str ? str->c_str() : NULL );
}

uchar FOMapper::SScriptFunc::MapperObject_get_Critter_Cond( MapObject& mobj )
{
    if( mobj.MapObjType != MAP_OBJECT_CRITTER )
        SCRIPT_ERROR_R0( "Map object is not critter." );
    return mobj.MCritter.Cond;
}

void FOMapper::SScriptFunc::MapperObject_set_Critter_Cond( MapObject& mobj, uchar value )
{
    if( mobj.MapObjType != MAP_OBJECT_CRITTER )
        SCRIPT_ERROR_R( "Map object is not critter." );
    if( !mobj.RunTime.FromMap )
        return;
    if( value < COND_LIFE || value > COND_DEAD )
        return;
    if( mobj.MCritter.Cond == COND_DEAD && value != COND_LIFE )
    {
        for( auto it = mobj.RunTime.FromMap->MObjects.begin(); it != mobj.RunTime.FromMap->MObjects.end(); ++it )
        {
            MapObject* mobj_ = *it;
            if( mobj_->MapObjType == MAP_OBJECT_CRITTER && mobj_->MCritter.Cond != COND_DEAD && mobj_->MapX == mobj.MapX && mobj_->MapY == mobj.MapY )
                return;
        }
    }
    mobj.MCritter.Cond = value;
}

CScriptString* FOMapper::SScriptFunc::MapperObject_get_PicMap( MapObject& mobj )
{
    if( mobj.MapObjType != MAP_OBJECT_ITEM && mobj.MapObjType != MAP_OBJECT_SCENERY )
        SCRIPT_ERROR_RX( "Map object is not item or scenery.", new CScriptString( "" ) );
    if( mobj.RunTime.PicMapName[ 0 ] )
        return new CScriptString( mobj.RunTime.PicMapName );
    ProtoItem* proto_item = ItemMngr.GetProtoItem( mobj.ProtoId );
    if( !proto_item )
        SCRIPT_ERROR_RX( "Proto item not found.", new CScriptString( "" ) );
    const char* name = Str::GetName( proto_item->PicMap );
    if( !name )
        SCRIPT_ERROR_RX( "Name not found.", new CScriptString( "" ) );
    return new CScriptString( name );
}

void FOMapper::SScriptFunc::MapperObject_set_PicMap( MapObject& mobj, CScriptString* str )
{
    if( mobj.MapObjType != MAP_OBJECT_ITEM && mobj.MapObjType != MAP_OBJECT_SCENERY )
        SCRIPT_ERROR_R( "Map object is not item or scenery." );
    Str::Copy( mobj.RunTime.PicMapName, str->c_str() );
    mobj.MItem.PicMapHash = Str::GetHash( mobj.RunTime.PicMapName );
}

CScriptString* FOMapper::SScriptFunc::MapperObject_get_PicInv( MapObject& mobj )
{
    if( mobj.MapObjType != MAP_OBJECT_ITEM && mobj.MapObjType != MAP_OBJECT_SCENERY )
        SCRIPT_ERROR_RX( "Map object is not item or scenery.", new CScriptString( "" ) );
    return new CScriptString( mobj.RunTime.PicInvName );
    if( mobj.RunTime.PicMapName[ 0 ] )
        return new CScriptString( mobj.RunTime.PicInvName );
    ProtoItem* proto_item = ItemMngr.GetProtoItem( mobj.ProtoId );
    if( !proto_item )
        SCRIPT_ERROR_RX( "Proto item not found.", new CScriptString( "" ) );
    const char* name = Str::GetName( proto_item->PicInv );
    if( !name )
        SCRIPT_ERROR_RX( "Name not found.", new CScriptString( "" ) );
    return new CScriptString( name );
}

void FOMapper::SScriptFunc::MapperObject_set_PicInv( MapObject& mobj, CScriptString* str )
{
    if( mobj.MapObjType != MAP_OBJECT_ITEM && mobj.MapObjType != MAP_OBJECT_SCENERY )
        SCRIPT_ERROR_R( "Map object is not item or scenery." );
    Str::Copy( mobj.RunTime.PicInvName, str->c_str() );
    mobj.MItem.PicInvHash = Str::GetHash( mobj.RunTime.PicInvName );
}

void FOMapper::SScriptFunc::MapperObject_Update( MapObject& mobj )
{
    if( mobj.RunTime.FromMap && mobj.RunTime.FromMap == Self->CurProtoMap )
        Self->UpdateMapObject( &mobj );
}

MapObject* FOMapper::SScriptFunc::MapperObject_AddChild( MapObject& mobj, ushort pid )
{
    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item || !proto_item->IsContainer() )
        SCRIPT_ERROR_R0( "Object is not container item." );
    ProtoItem* proto_item_ = ItemMngr.GetProtoItem( pid );
    if( !proto_item_ || !proto_item_->IsItem() )
        SCRIPT_ERROR_R0( "Added child is not item." );

    MapObject* mobj_ = new MapObject();
    if( !mobj_ )
        return NULL;
    mobj_->RunTime.FromMap = mobj.RunTime.FromMap;
    mobj_->MapObjType = MAP_OBJECT_ITEM;
    mobj_->ProtoId = pid;
    mobj_->MapX = mobj.MapX;
    mobj_->MapY = mobj.MapY;
    if( !mobj.UID )
        mobj.UID = ++mobj.RunTime.FromMap->LastObjectUID;
    mobj_->ContainerUID = mobj.UID;
    return mobj_;
}

uint FOMapper::SScriptFunc::MapperObject_GetChilds( MapObject& mobj, CScriptArray* objects )
{
    if( !mobj.RunTime.FromMap )
        return 0;
    if( mobj.MapObjType != MAP_OBJECT_ITEM && mobj.MapObjType != MAP_OBJECT_CRITTER )
        return 0;
    MapObjectPtrVec objects_;
    if( mobj.UID )
    {
        for( auto it = mobj.RunTime.FromMap->MObjects.begin(); it != mobj.RunTime.FromMap->MObjects.end(); ++it )
        {
            MapObject* mobj_ = *it;
            if( mobj_->ContainerUID == mobj.UID )
                objects_.push_back( mobj_ );
        }
    }
    if( objects )
        Script::AppendVectorToArrayRef( objects_, objects );
    return (uint) objects_.size();
}

void FOMapper::SScriptFunc::MapperObject_MoveToHex( MapObject& mobj, ushort hx, ushort hy )
{
    ProtoMap* pmap = mobj.RunTime.FromMap;
    if( !pmap )
        return;
    if( mobj.ContainerUID )
        return;

    if( hx >= pmap->Header.MaxHexX )
        hx = pmap->Header.MaxHexX - 1;
    if( hy >= pmap->Header.MaxHexY )
        hy = pmap->Header.MaxHexY - 1;
    Self->MoveMapObject( &mobj, hx, hy );
}

void FOMapper::SScriptFunc::MapperObject_MoveToHexOffset( MapObject& mobj, int x, int y )
{
    ProtoMap* pmap = mobj.RunTime.FromMap;
    if( !pmap )
        return;
    if( mobj.ContainerUID )
        return;

    int hx = (int) mobj.MapX + x;
    int hy = (int) mobj.MapY + y;
    hx = CLAMP( hx, 0, pmap->Header.MaxHexX - 1 );
    hy = CLAMP( hy, 0, pmap->Header.MaxHexY - 1 );
    Self->MoveMapObject( &mobj, hx, hy );
}

void FOMapper::SScriptFunc::MapperObject_MoveToDir( MapObject& mobj, uchar dir )
{
    ProtoMap* pmap = mobj.RunTime.FromMap;
    if( !pmap )
        return;
    if( mobj.ContainerUID )
        return;

    int hx = mobj.MapX;
    int hy = mobj.MapY;
    MoveHexByDirUnsafe( hx, hy, dir );
    hx = CLAMP( hx, 0, pmap->Header.MaxHexX - 1 );
    hy = CLAMP( hy, 0, pmap->Header.MaxHexY - 1 );
    Self->MoveMapObject( &mobj, hx, hy );
}

MapObject* FOMapper::SScriptFunc::MapperMap_AddObject( ProtoMap& pmap, ushort hx, ushort hy, int mobj_type, ushort pid )
{
    if( mobj_type == MAP_OBJECT_CRITTER )
    {
        for( auto it = pmap.MObjects.begin(); it != pmap.MObjects.end(); ++it )
        {
            MapObject* mobj = *it;
            if( mobj->MapObjType == MAP_OBJECT_CRITTER && mobj->MCritter.Cond != COND_DEAD &&
                mobj->MapX == hx && mobj->MapY == hy )
                SCRIPT_ERROR_R0( "Critter already present on this hex." );
        }

        CritData* pnpc = CrMngr.GetProto( pid );
        if( !pnpc )
            SCRIPT_ERROR_R0( "Unknown critter prototype." );
    }
    else if( mobj_type == MAP_OBJECT_ITEM || mobj_type == MAP_OBJECT_SCENERY )
    {
        ProtoItem* proto = ItemMngr.GetProtoItem( pid );
        if( !proto )
            SCRIPT_ERROR_R0( "Invalid item/scenery prototype." );
    }
    else
    {
        SCRIPT_ERROR_R0( "Invalid map object type." );
    }

    MapObject* mobj = new MapObject();
    if( !mobj )
        return NULL;
    mobj->RunTime.FromMap = &pmap;
    mobj->MapObjType = mobj_type;
    mobj->ProtoId = pid;
    mobj->MapX = hx;
    mobj->MapY = hy;
    if( mobj_type == MAP_OBJECT_CRITTER )
        mobj->MCritter.Cond = COND_LIFE;

    if( Self->CurProtoMap == &pmap )
    {
        MapObject* mobj_ = Self->ParseMapObj( mobj );
        SAFEREL( mobj );
        mobj = mobj_;
    }
    else
    {
        pmap.MObjects.push_back( mobj );
    }
    return mobj;
}

MapObject* FOMapper::SScriptFunc::MapperMap_GetObject( ProtoMap& pmap, ushort hx, ushort hy, int mobj_type, ushort pid, uint skip )
{
    return Self->FindMapObject( pmap, hx, hy, mobj_type, pid, skip );
}

uint FOMapper::SScriptFunc::MapperMap_GetObjects( ProtoMap& pmap, ushort hx, ushort hy, uint radius, int mobj_type, ushort pid, CScriptArray* objects )
{
    MapObjectPtrVec objects_;
    Self->FindMapObjects( pmap, hx, hy, radius, mobj_type, pid, objects_ );
    if( objects )
        Script::AppendVectorToArrayRef( objects_, objects );
    return (uint) objects_.size();
}

void FOMapper::SScriptFunc::MapperMap_UpdateObjects( ProtoMap& pmap )
{
    if( Self->CurProtoMap == &pmap )
    {
        for( auto it = pmap.MObjects.begin(), end = pmap.MObjects.end(); it != end; ++it )
            Self->UpdateMapObject( *it );
    }
}

void FOMapper::SScriptFunc::MapperMap_Resize( ProtoMap& pmap, ushort width, ushort height )
{
    if( Self->CurProtoMap == &pmap )
        Self->HexMngr.UnloadMap();

    int maxhx = CLAMP( width, MAXHEX_MIN, MAXHEX_MAX );
    int maxhy = CLAMP( height, MAXHEX_MIN, MAXHEX_MAX );
    ;
    int old_maxhx = pmap.Header.MaxHexX;
    int old_maxhy = pmap.Header.MaxHexY;
    maxhx = CLAMP( maxhx, MAXHEX_MIN, MAXHEX_MAX );
    maxhy = CLAMP( maxhy, MAXHEX_MIN, MAXHEX_MAX );
    if( pmap.Header.WorkHexX >= maxhx )
        pmap.Header.WorkHexX = maxhx - 1;
    if( pmap.Header.WorkHexY >= maxhy )
        pmap.Header.WorkHexY = maxhy - 1;
    pmap.Header.MaxHexX = maxhx;
    pmap.Header.MaxHexY = maxhy;

    if( maxhx < old_maxhx || maxhy < old_maxhy )
    {
        // Delete truncated map objects
        if( maxhx < old_maxhx || maxhy < old_maxhy )
        {
            for( auto it = pmap.MObjects.begin(); it != pmap.MObjects.end();)
            {
                MapObject* mobj = *it;
                if( mobj->MapX >= maxhx || mobj->MapY >= maxhy )
                {
                    SAFEREL( mobj );
                    it = pmap.MObjects.erase( it );
                }
                else
                    ++it;
            }
        }

        // Resize tiles
        ProtoMap::TileVecVec tiles_field = pmap.TilesField;
        ProtoMap::TileVecVec roofs_field = pmap.RoofsField;
        pmap.TilesField.clear();
        pmap.TilesField.resize( maxhx * maxhy );
        pmap.RoofsField.clear();
        pmap.RoofsField.resize( maxhx * maxhy );
        for( int hy = 0; hy < min( maxhy, old_maxhy ); hy++ )
        {
            for( int hx = 0; hx < min( maxhx, old_maxhx ); hx++ )
            {
                for( int r = 0; r <= 1; r++ )
                {
                    ProtoMap::TileVecVec& field_from = ( !r ? tiles_field : roofs_field );
                    ProtoMap::TileVecVec& field_to = ( !r ? pmap.TilesField : pmap.RoofsField );
                    ProtoMap::TileVec&    tiles_from = field_from[ hy * old_maxhx + hx ];
                    ProtoMap::TileVec&    tiles_to = field_to[ hy * maxhx + hx ];
                    tiles_to = tiles_from;
                }
            }
        }
    }

    // Update visibility
    if( Self->CurProtoMap == &pmap )
    {
        Self->HexMngr.SetProtoMap( pmap );
        Self->HexMngr.FindSetCenter( pmap.Header.WorkHexX, pmap.Header.WorkHexY );
    }
}

uint FOMapper::SScriptFunc::MapperMap_GetTilesCount( ProtoMap& pmap, ushort hx, ushort hy, bool roof )
{
    if( hx >= pmap.Header.MaxHexX )
        SCRIPT_ERROR_R0( "Invalid hex x arg." );
    if( hy >= pmap.Header.MaxHexY )
        SCRIPT_ERROR_R0( "Invalid hex y arg." );

    ProtoMap::TileVec& tiles = pmap.GetTiles( hx, hy, roof );
    return (uint) tiles.size();
}

void FOMapper::SScriptFunc::MapperMap_DeleteTile( ProtoMap& pmap, ushort hx, ushort hy, bool roof, uint index )
{
    if( hx >= pmap.Header.MaxHexX )
        SCRIPT_ERROR_R( "Invalid hex x arg." );
    if( hy >= pmap.Header.MaxHexY )
        SCRIPT_ERROR_R( "Invalid hex y arg." );

    ProtoMap::TileVec& tiles = pmap.GetTiles( hx, hy, roof );
    Field&             f = Self->HexMngr.GetField( hx, hy );
    Field::TileVec&    ftiles = ( roof ? f.Roofs : f.Tiles );
    if( index == uint( -1 ) )
    {
        ftiles.clear();
        tiles.clear();
    }
    else if( index < tiles.size() )
    {
        ftiles.erase( ftiles.begin() + index );
        tiles.erase( tiles.begin() + index );
    }

    if( Self->CurProtoMap == &pmap )
    {
        if( roof )
            Self->HexMngr.RebuildRoof();
        else
            Self->HexMngr.RebuildTiles();
    }
}

uint FOMapper::SScriptFunc::MapperMap_GetTileHash( ProtoMap& pmap, ushort hx, ushort hy, bool roof, uint index )
{
    if( hx >= pmap.Header.MaxHexX )
        SCRIPT_ERROR_R0( "Invalid hex x arg." );
    if( hy >= pmap.Header.MaxHexY )
        SCRIPT_ERROR_R0( "Invalid hex y arg." );

    ProtoMap::TileVec& tiles = pmap.GetTiles( hx, hy, roof );
    return index < tiles.size() ? tiles[ index ].NameHash : 0;
}

void FOMapper::SScriptFunc::MapperMap_AddTileHash( ProtoMap& pmap, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, uint pic_hash )
{
    if( hx >= pmap.Header.MaxHexX )
        SCRIPT_ERROR_R( "Invalid hex x arg." );
    if( hy >= pmap.Header.MaxHexY )
        SCRIPT_ERROR_R( "Invalid hex y arg." );
    if( !pic_hash )
        return;
    ox = CLAMP( ox, -MAX_MOVE_OX, MAX_MOVE_OX );
    oy = CLAMP( oy, -MAX_MOVE_OY, MAX_MOVE_OY );
    layer = CLAMP( layer, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END );

    if( Self->CurProtoMap == &pmap )
        Self->HexMngr.SetTile( pic_hash, hx, hy, ox, oy, layer, roof, false );
    else
    {
        ProtoMap::TileVec& tiles = pmap.GetTiles( hx, hy, roof );
        tiles.push_back( ProtoMap::Tile( pic_hash, hx, hy, ox, oy, layer, roof ) );
        tiles.back().      IsSelected = false;
    }
}

CScriptString* FOMapper::SScriptFunc::MapperMap_GetTileName( ProtoMap& pmap, ushort hx, ushort hy, bool roof, uint index )
{
    if( hx >= pmap.Header.MaxHexX )
        SCRIPT_ERROR_RX( "Invalid hex x arg.", new CScriptString( "" ) );
    if( hy >= pmap.Header.MaxHexY )
        SCRIPT_ERROR_RX( "Invalid hex y arg.", new CScriptString( "" ) );

    ProtoMap::TileVec& tiles = pmap.GetTiles( hx, hy, roof );
    if( index >= tiles.size() )
        return new CScriptString( "" );
    const char* name = Str::GetName( tiles[ index ].NameHash );
    return new CScriptString( name ? name : "" );
}

void FOMapper::SScriptFunc::MapperMap_AddTileName( ProtoMap& pmap, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, CScriptString* pic_name )
{
    if( hx >= pmap.Header.MaxHexX )
        SCRIPT_ERROR_R( "Invalid hex x arg." );
    if( hy >= pmap.Header.MaxHexY )
        SCRIPT_ERROR_R( "Invalid hex y arg." );
    if( !pic_name || !pic_name->length() )
        return;
    ox = CLAMP( ox, -MAX_MOVE_OX, MAX_MOVE_OX );
    oy = CLAMP( oy, -MAX_MOVE_OY, MAX_MOVE_OY );
    layer = CLAMP( layer, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END );

    uint pic_hash = Str::GetHash( pic_name->c_str() );
    if( Self->CurProtoMap == &pmap )
        Self->HexMngr.SetTile( pic_hash, hx, hy, ox, oy, layer, roof, false );
    else
    {
        ProtoMap::TileVec& tiles = pmap.GetTiles( hx, hy, roof );
        tiles.push_back( ProtoMap::Tile( pic_hash, hx, hy, ox, oy, layer, roof ) );
        tiles.back().      IsSelected = false;
    }
}

uint FOMapper::SScriptFunc::MapperMap_GetDayTime( ProtoMap& pmap, uint day_part )
{
    if( day_part >= 4 )
        SCRIPT_ERROR_R0( "Invalid day part arg." );
    return pmap.Header.DayTime[ day_part ];
}

void FOMapper::SScriptFunc::MapperMap_SetDayTime( ProtoMap& pmap, uint day_part, uint time )
{
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );
    if( time >= 1440 )
        SCRIPT_ERROR_R( "Invalid time arg." );

    pmap.Header.DayTime[ day_part ] = time;
    if( pmap.Header.DayTime[ 1 ] < pmap.Header.DayTime[ 0 ] )
        pmap.Header.DayTime[ 1 ] = pmap.Header.DayTime[ 0 ];
    if( pmap.Header.DayTime[ 2 ] < pmap.Header.DayTime[ 1 ] )
        pmap.Header.DayTime[ 2 ] = pmap.Header.DayTime[ 1 ];
    if( pmap.Header.DayTime[ 3 ] < pmap.Header.DayTime[ 2 ] )
        pmap.Header.DayTime[ 3 ] = pmap.Header.DayTime[ 2 ];

    // Update visibility
    if( Self->CurProtoMap == &pmap )
    {
        int* dt = Self->HexMngr.GetMapDayTime();
        for( int i = 0; i < 4; i++ )
            dt[ i ] = pmap.Header.DayTime[ i ];
        Self->HexMngr.RefreshMap();
    }
}

void FOMapper::SScriptFunc::MapperMap_GetDayColor( ProtoMap& pmap, uint day_part, uchar& r, uchar& g, uchar& b )
{
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );
    r = pmap.Header.DayColor[ 0 + day_part ];
    g = pmap.Header.DayColor[ 4 + day_part ];
    b = pmap.Header.DayColor[ 8 + day_part ];
}

void FOMapper::SScriptFunc::MapperMap_SetDayColor( ProtoMap& pmap, uint day_part, uchar r, uchar g, uchar b )
{
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );

    pmap.Header.DayColor[ 0 + day_part ] = r;
    pmap.Header.DayColor[ 4 + day_part ] = g;
    pmap.Header.DayColor[ 8 + day_part ] = b;

    // Update visibility
    if( Self->CurProtoMap == &pmap )
    {
        uchar* dc = Self->HexMngr.GetMapDayColor();
        for( int i = 0; i < 12; i++ )
            dc[ i ] = pmap.Header.DayColor[ i ];
        Self->HexMngr.RefreshMap();
    }
}

CScriptString* FOMapper::SScriptFunc::MapperMap_get_ScriptModule( ProtoMap& pmap )
{
    return new CScriptString( pmap.Header.ScriptModule );
}

void FOMapper::SScriptFunc::MapperMap_set_ScriptModule( ProtoMap& pmap, CScriptString* str )
{
    Str::Copy( pmap.Header.ScriptModule, str ? str->c_str() : "" );
}

CScriptString* FOMapper::SScriptFunc::MapperMap_get_ScriptFunc( ProtoMap& pmap )
{
    return new CScriptString( pmap.Header.ScriptFunc );
}

void FOMapper::SScriptFunc::MapperMap_set_ScriptFunc( ProtoMap& pmap, CScriptString* str )
{
    Str::Copy( pmap.Header.ScriptFunc, str ? str->c_str() : "" );
}

void FOMapper::SScriptFunc::Global_SetDefaultCritterParam( uint index, int param )
{
    if( index >= MAPOBJ_CRITTER_PARAMS )
        SCRIPT_ERROR_R( "Invalid index arg." );
    Self->DefaultCritterParam[ index ] = param;
}

void FOMapper::SScriptFunc::Global_AllowSlot( uchar index, CScriptString& slot_name )
{
    if( index <= SLOT_ARMOR || index == SLOT_GROUND )
        SCRIPT_ERROR_R( "Invalid index arg." );
    if( !slot_name.length() )
        SCRIPT_ERROR_R( "Slot name string is empty." );
    SlotExt se;
    se.Index = index;
    se.SlotName = Str::Duplicate( slot_name.c_str() );
    Self->SlotsExt.insert( PAIR( index, se ) );
}

ProtoMap* FOMapper::SScriptFunc::Global_LoadMap( CScriptString& file_name, int path_type )
{
    ProtoMap* pmap = new ProtoMap();
    FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
    if( !pmap->Init( 0xFFFF, file_name.c_str(), path_type ) )
    {
        FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
        return NULL;
    }
    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
    Self->LoadedProtoMaps.push_back( pmap );
    return pmap;
}

void FOMapper::SScriptFunc::Global_UnloadMap( ProtoMap* pmap )
{
    if( !pmap )
        SCRIPT_ERROR_R( "Proto map arg nullptr." );
    auto it = std::find( Self->LoadedProtoMaps.begin(), Self->LoadedProtoMaps.end(), pmap );
    if( it != Self->LoadedProtoMaps.end() )
        Self->LoadedProtoMaps.erase( it );
    if( pmap == Self->CurProtoMap )
    {
        Self->HexMngr.UnloadMap();
        Self->SelectedObj.clear();
        SAFEREL( Self->CurProtoMap );
    }
    pmap->Clear();
    pmap->Release();
}

bool FOMapper::SScriptFunc::Global_SaveMap( ProtoMap* pmap, CScriptString& file_name, int path_type )
{
    if( !pmap )
        SCRIPT_ERROR_R0( "Proto map arg nullptr." );
    FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
    bool result = pmap->Save( file_name.c_str(), path_type );
    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
    return result;
}

bool FOMapper::SScriptFunc::Global_ShowMap( ProtoMap* pmap )
{
    if( !pmap )
        SCRIPT_ERROR_R0( "Proto map arg nullptr." );
    if( Self->CurProtoMap == pmap )
        return true;                             // Already

    Self->SelectClear();
    if( !Self->HexMngr.SetProtoMap( *pmap ) )
        return false;
    Self->HexMngr.FindSetCenter( pmap->Header.WorkHexX, pmap->Header.WorkHexY );
    Self->CurProtoMap = pmap;
    return true;
}

int FOMapper::SScriptFunc::Global_GetLoadedMaps( CScriptArray* maps )
{
    int index = -1;
    for( int i = 0, j = (int) Self->LoadedProtoMaps.size(); i < j; i++ )
    {
        ProtoMap* pmap = Self->LoadedProtoMaps[ i ];
        if( pmap == Self->CurProtoMap )
            index = i;
    }
    if( maps )
        Script::AppendVectorToArrayRef( Self->LoadedProtoMaps, maps );
    return index;
}

uint FOMapper::SScriptFunc::Global_GetMapFileNames( CScriptString* dir, CScriptArray* names )
{
    FileManager::SetDataPath( GameOpt.ServerPath.c_str() );
    string dir_ = FileManager::GetFullPath( NULL, PT_SERVER_MAPS );
    uint   n = 0;
    if( dir )
        dir_ = dir->c_std_str();

    FIND_DATA fd;
    void*     h = FileFindFirst( dir_.c_str(), NULL, fd );
    if( !h )
    {
        FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
        return 0;
    }

    while( true )
    {
        if( ProtoMap::IsMapFile( Str::FormatBuf( "%s%s", dir_.c_str(), fd.FileName ) ) )
        {
            if( names )
            {
                char  fname[ MAX_FOPATH ];
                Str::Copy( fname, fd.FileName );
                char* ext = (char*) FileManager::GetExtension( fname );
                if( ext )
                    *( ext - 1 ) = 0;

                int             len = names->GetSize();
                names->Resize( names->GetSize() + 1 );
                CScriptString** ptr = (CScriptString**) names->At( len );
                *ptr = new CScriptString( fname );
            }
            n++;
        }

        if( !FileFindNext( h, fd ) )
            break;
    }
    FileFindClose( h );

    FileManager::SetDataPath( ( GameOpt.ClientPath + GameOpt.FoDataPath ).c_str() );
    return n;
}

void FOMapper::SScriptFunc::Global_DeleteObject( MapObject* mobj )
{
    if( mobj )
        Self->DeleteMapObject( mobj );
}

void FOMapper::SScriptFunc::Global_DeleteObjects( CScriptArray& objects )
{
    for( int i = 0, j = objects.GetSize(); i < j; i++ )
    {
        MapObject* mobj = *(MapObject**) objects.At( i );
        if( mobj )
            Self->DeleteMapObject( mobj );
    }
}

void FOMapper::SScriptFunc::Global_SelectObject( MapObject* mobj, bool set )
{
    if( mobj )
    {
        if( set )
            Self->SelectAdd( mobj );
        else
            Self->SelectErase( mobj );
    }
}

void FOMapper::SScriptFunc::Global_SelectObjects( CScriptArray& objects, bool set )
{
    for( int i = 0, j = objects.GetSize(); i < j; i++ )
    {
        MapObject* mobj = *(MapObject**) objects.At( i );
        if( mobj )
        {
            if( set )
                Self->SelectAdd( mobj );
            else
                Self->SelectErase( mobj );
        }
    }
}

MapObject* FOMapper::SScriptFunc::Global_GetSelectedObject()
{
    return Self->SelectedObj.size() ? Self->SelectedObj[ 0 ].MapObj : NULL;
}

uint FOMapper::SScriptFunc::Global_GetSelectedObjects( CScriptArray* objects )
{
    MapObjectPtrVec objects_;
    objects_.reserve( Self->SelectedObj.size() );
    for( uint i = 0, j = (uint) Self->SelectedObj.size(); i < j; i++ )
        objects_.push_back( Self->SelectedObj[ i ].MapObj );
    if( objects )
        Script::AppendVectorToArrayRef( objects_, objects );
    return (uint) objects_.size();
}

uint FOMapper::SScriptFunc::Global_TabGetTileDirs( int tab, CScriptArray* dir_names, CScriptArray* include_subdirs )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return 0;

    TileTab& ttab = Self->TabsTiles[ tab ];
    if( dir_names )
    {
        asUINT i = dir_names->GetSize();
        dir_names->Resize( dir_names->GetSize() + (uint) ttab.TileDirs.size() );
        for( uint k = 0, l = (uint) ttab.TileDirs.size(); k < l; k++, i++ )
        {
            CScriptString** p = (CScriptString**) dir_names->At( i );
            *p = new CScriptString( ttab.TileDirs[ k ] );
        }
    }
    if( include_subdirs )
        Script::AppendVectorToArray( ttab.TileSubDirs, include_subdirs );
    return (uint) ttab.TileDirs.size();
}

uint FOMapper::SScriptFunc::Global_TabGetItemPids( int tab, CScriptString* sub_tab, CScriptArray* item_pids )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return 0;
    if( sub_tab && sub_tab->length() && !Self->Tabs[ tab ].count( sub_tab->c_std_str() ) )
        return 0;

    SubTab& stab = Self->Tabs[ tab ][ sub_tab && sub_tab->length() ? sub_tab->c_std_str() : DEFAULT_SUB_TAB ];
    if( item_pids )
        Script::AppendVectorToArray( stab.ItemProtos, item_pids );
    return (uint) stab.ItemProtos.size();
}

uint FOMapper::SScriptFunc::Global_TabGetCritterPids( int tab, CScriptString* sub_tab, CScriptArray* critter_pids )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return 0;
    if( sub_tab && sub_tab->length() && !Self->Tabs[ tab ].count( sub_tab->c_std_str() ) )
        return 0;

    SubTab& stab = Self->Tabs[ tab ][ sub_tab && sub_tab->length() ? sub_tab->c_std_str() : DEFAULT_SUB_TAB ];
    if( critter_pids )
        Script::AppendVectorToArray( stab.NpcProtos, critter_pids );
    return (uint) stab.NpcProtos.size();
}

void FOMapper::SScriptFunc::Global_TabSetTileDirs( int tab, CScriptArray* dir_names, CScriptArray* include_subdirs )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return;
    if( dir_names && include_subdirs && dir_names->GetSize() != include_subdirs->GetSize() )
        return;

    TileTab& ttab = Self->TabsTiles[ tab ];
    ttab.TileDirs.clear();
    ttab.TileSubDirs.clear();

    if( dir_names )
    {
        for( uint i = 0, j = dir_names->GetSize(); i < j; i++ )
        {
            CScriptString* name = *(CScriptString**) dir_names->At( i );
            if( name && name->length() )
            {
                ttab.TileDirs.push_back( name->c_std_str() );
                ttab.TileSubDirs.push_back( include_subdirs ? *(bool*) include_subdirs->At( i ) : false );
            }
        }
    }

    if( Self->IsMapperStarted )
        Self->RefreshTiles( tab );
}

void FOMapper::SScriptFunc::Global_TabSetItemPids( int tab, CScriptString* sub_tab, CScriptArray* item_pids )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return;
    if( !sub_tab || !sub_tab->length() || sub_tab->c_std_str() == DEFAULT_SUB_TAB )
        return;

    // Add protos to sub tab
    if( item_pids && item_pids->GetSize() )
    {
        ProtoItemVec proto_items;
        for( int i = 0, j = item_pids->GetSize(); i < j; i++ )
        {
            ushort     pid = *(ushort*) item_pids->At( i );
            ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
            if( proto_item )
                proto_items.push_back( *proto_item );
        }

        if( proto_items.size() )
        {
            SubTab& stab = Self->Tabs[ tab ][ sub_tab->c_std_str() ];
            stab.ItemProtos = proto_items;
        }
    }
    // Delete sub tab
    else
    {
        auto it = Self->Tabs[ tab ].find( sub_tab->c_std_str() );
        if( it != Self->Tabs[ tab ].end() )
        {
            if( Self->TabsActive[ tab ] == &( *it ).second )
                Self->TabsActive[ tab ] = NULL;
            Self->Tabs[ tab ].erase( it );
        }
    }

    // Recalculate whole pids
    SubTab& stab_default = Self->Tabs[ tab ][ DEFAULT_SUB_TAB ];
    stab_default.ItemProtos.clear();
    for( auto it = Self->Tabs[ tab ].begin(), end = Self->Tabs[ tab ].end(); it != end; ++it )
    {
        SubTab& stab = ( *it ).second;
        if( &stab == &stab_default )
            continue;
        for( uint i = 0, j = (uint) stab.ItemProtos.size(); i < j; i++ )
            stab_default.ItemProtos.push_back( stab.ItemProtos[ i ] );
    }
    if( !Self->TabsActive[ tab ] )
        Self->TabsActive[ tab ] = &stab_default;

    // Refresh
    if( Self->IsMapperStarted )
        Self->RefreshCurProtos();
}

void FOMapper::SScriptFunc::Global_TabSetCritterPids( int tab, CScriptString* sub_tab, CScriptArray* critter_pids )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return;
    if( !sub_tab || !sub_tab->length() || sub_tab->c_std_str() == DEFAULT_SUB_TAB )
        return;

    // Add protos to sub tab
    if( critter_pids && critter_pids->GetSize() )
    {
        CritDataVec cr_protos;
        for( int i = 0, j = critter_pids->GetSize(); i < j; i++ )
        {
            ushort    pid = *(ushort*) critter_pids->At( i );
            CritData* cr_data = CrMngr.GetProto( pid );
            if( cr_data )
                cr_protos.push_back( cr_data );
        }

        if( cr_protos.size() )
        {
            SubTab& stab = Self->Tabs[ tab ][ sub_tab->c_std_str() ];
            stab.NpcProtos = cr_protos;
        }
    }
    // Delete sub tab
    else
    {
        auto it = Self->Tabs[ tab ].find( sub_tab->c_std_str() );
        if( it != Self->Tabs[ tab ].end() )
        {
            if( Self->TabsActive[ tab ] == &( *it ).second )
                Self->TabsActive[ tab ] = NULL;
            Self->Tabs[ tab ].erase( it );
        }
    }

    // Recalculate whole pids
    SubTab& stab_default = Self->Tabs[ tab ][ DEFAULT_SUB_TAB ];
    stab_default.NpcProtos.clear();
    for( auto it = Self->Tabs[ tab ].begin(), end = Self->Tabs[ tab ].end(); it != end; ++it )
    {
        SubTab& stab = ( *it ).second;
        if( &stab == &stab_default )
            continue;
        for( uint i = 0, j = (uint) stab.NpcProtos.size(); i < j; i++ )
            stab_default.NpcProtos.push_back( stab.NpcProtos[ i ] );
    }
    if( !Self->TabsActive[ tab ] )
        Self->TabsActive[ tab ] = &stab_default;

    // Refresh
    if( Self->IsMapperStarted )
        Self->RefreshCurProtos();
}

void FOMapper::SScriptFunc::Global_TabDelete( int tab )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return;

    Self->Tabs[ tab ].clear();
    SubTab& stab_default = Self->Tabs[ tab ][ DEFAULT_SUB_TAB ];
    Self->TabsActive[ tab ] = &stab_default;
}

void FOMapper::SScriptFunc::Global_TabSelect( int tab, CScriptString* sub_tab )
{
    if( tab < 0 || tab >= TAB_COUNT )
        return;
    auto it = Self->Tabs[ tab ].find( sub_tab && sub_tab->length() ? sub_tab->c_std_str() : DEFAULT_SUB_TAB );
    if( it != Self->Tabs[ tab ].end() )
        Self->TabsActive[ tab ] = &( *it ).second;
}

void FOMapper::SScriptFunc::Global_TabSetName( int tab, CScriptString* tab_name )
{
    if( tab < 0 || tab >= INT_MODE_COUNT )
        return;
    Self->TabsName[ tab ] = ( tab_name ? tab_name->c_std_str() : "" );
}

CScriptString* FOMapper::SScriptFunc::Global_GetLastError()
{
    return new CScriptString( ScriptLastError );
}

ProtoItem* FOMapper::SScriptFunc::Global_GetProtoItem( ushort proto_id )
{
    ProtoItem* proto_item = ItemMngr.GetProtoItem( proto_id );
    // if(!proto_item) SCRIPT_ERROR_R0("Proto item not found.");
    return proto_item;
}

void FOMapper::SScriptFunc::Global_MoveScreen( ushort hx, ushort hy, uint speed )
{
    if( hx >= Self->HexMngr.GetMaxHexX() || hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hex args." );
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R( "Map is not loaded." );
    if( !speed )
        Self->HexMngr.FindSetCenter( hx, hy );
    else
        Self->HexMngr.ScrollToHex( hx, hy, double(speed) / 1000.0, false );
}

void FOMapper::SScriptFunc::Global_MoveHexByDir( ushort& hx, ushort& hy, uchar dir, uint steps )
{
    if( !Self->HexMngr.IsMapLoaded() )
        SCRIPT_ERROR_R( "Map not loaded." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid dir arg." );
    if( !steps )
        SCRIPT_ERROR_R( "Steps arg is zero." );
    int hx_ = hx, hy_ = hy;
    if( steps > 1 )
    {
        for( uint i = 0; i < steps; i++ )
            MoveHexByDirUnsafe( hx_, hy_, dir );
    }
    else
    {
        MoveHexByDirUnsafe( hx_, hy_, dir );
    }
    if( hx_ < 0 )
        hx_ = 0;
    if( hy_ < 0 )
        hy_ = 0;
    hx = hx_;
    hy = hy_;
}

CScriptString* FOMapper::SScriptFunc::Global_GetIfaceIniStr( CScriptString& key )
{
    char* big_buf = Str::GetBigBuf();
    if( Self->IfaceIni.GetStr( key.c_str(), "", big_buf ) )
        return new CScriptString( big_buf );
    return new CScriptString( "" );
}

void FOMapper::SScriptFunc::Global_Log( CScriptString& text )
{
    // Self->AddMessFormat(text.buffer.c_str());
    Script::Log( text.c_str() );
}

bool FOMapper::SScriptFunc::Global_StrToInt( CScriptString* text, int& result )
{
    if( !text || !text->length() || !Str::IsNumber( text->c_str() ) )
        return false;
    result = Str::AtoI( text->c_str() );
    return true;
}

bool FOMapper::SScriptFunc::Global_StrToFloat( CScriptString* text, float& result )
{
    if( !text || !text->length() )
        return false;
    result = (float) strtod( text->c_str(), NULL );
    return true;
}

void FOMapper::SScriptFunc::Global_Message( CScriptString& msg )
{
    Self->AddMess( msg.c_str() );
}

void FOMapper::SScriptFunc::Global_MessageMsg( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R( "Invalid text msg arg." );
    Self->AddMess( Self->CurLang.Msg[ text_msg ].GetStr( str_num ) );
}

void FOMapper::SScriptFunc::Global_MapMessage( CScriptString& text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy )
{
    FOMapper::MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = ( color ? color : COLOR_TEXT );
    t.Fade = fade;
    t.StartTick = Timer::FastTick();
    t.Tick = ms;
    t.Text = text.c_std_str();
    t.Pos = Self->HexMngr.GetRectForText( hx, hy );
    t.EndPos = Rect( t.Pos, ox, oy );
    auto it = std::find( Self->GameMapTexts.begin(), Self->GameMapTexts.end(), t );
    if( it != Self->GameMapTexts.end() )
        Self->GameMapTexts.erase( it );
    Self->GameMapTexts.push_back( t );
}

CScriptString* FOMapper::SScriptFunc::Global_GetMsgStr( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    CScriptString* str = new CScriptString( Self->CurLang.Msg[ text_msg ].GetStr( str_num ) );
    return str;
}

CScriptString* FOMapper::SScriptFunc::Global_GetMsgStrSkip( int text_msg, uint str_num, uint skip_count )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    CScriptString* str = new CScriptString( Self->CurLang.Msg[ text_msg ].GetStr( str_num, skip_count ) );
    return str;
}

uint FOMapper::SScriptFunc::Global_GetMsgStrNumUpper( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].GetStrNumUpper( str_num );
}

uint FOMapper::SScriptFunc::Global_GetMsgStrNumLower( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].GetStrNumLower( str_num );
}

uint FOMapper::SScriptFunc::Global_GetMsgStrCount( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].Count( str_num );
}

bool FOMapper::SScriptFunc::Global_IsMsgStr( int text_msg, uint str_num )
{
    if( text_msg >= TEXTMSG_COUNT )
        SCRIPT_ERROR_R0( "Invalid text msg arg." );
    return Self->CurLang.Msg[ text_msg ].Count( str_num ) > 0;
}

CScriptString* FOMapper::SScriptFunc::Global_ReplaceTextStr( CScriptString& text, CScriptString& replace, CScriptString& str )
{
    size_t pos = text.c_std_str().find( replace.c_std_str(), 0 );
    if( pos == std::string::npos )
        return new CScriptString( text );
    string str_ = text.c_std_str();
    return new CScriptString( str_.replace( pos, replace.length(), str_ ) );
}

CScriptString* FOMapper::SScriptFunc::Global_ReplaceTextInt( CScriptString& text, CScriptString& replace, int i )
{
    size_t pos = text.c_std_str().find( replace.c_std_str(), 0 );
    if( pos == std::string::npos )
        return new CScriptString( text );
    char   val[ 32 ];
    Str::Format( val, "%d", i );
    string str_ = text.c_std_str();
    return new CScriptString( str_.replace( pos, replace.length(), val ) );
}

uint FOMapper::SScriptFunc::Global_GetDistantion( ushort hex_x1, ushort hex_y1, ushort hex_x2, ushort hex_y2 )
{
    return DistGame( hex_x1, hex_y1, hex_x2, hex_y2 );
}

uchar FOMapper::SScriptFunc::Global_GetDirection( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    return GetFarDir( from_hx, from_hy, to_hx, to_hy );
}

uchar FOMapper::SScriptFunc::Global_GetOffsetDir( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float offset )
{
    return GetFarDir( from_hx, from_hy, to_hx, to_hy, offset );
}

void FOMapper::SScriptFunc::Global_GetHexInPath( ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    UShortPair pre_block, block;
    Self->HexMngr.TraceBullet( from_hx, from_hy, to_hx, to_hy, dist, angle, NULL, false, NULL, 0, &block, &pre_block, NULL, true );
    to_hx = pre_block.first;
    to_hy = pre_block.second;
}

uint FOMapper::SScriptFunc::Global_GetPathLengthHex( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut )
{
    if( from_hx >= Self->HexMngr.GetMaxHexX() || from_hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid from hexes args." );
    if( to_hx >= Self->HexMngr.GetMaxHexX() || to_hy >= Self->HexMngr.GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    if( cut > 0 && !Self->HexMngr.CutPath( NULL, from_hx, from_hy, to_hx, to_hy, cut ) )
        return 0;
    UCharVec steps;
    if( !Self->HexMngr.FindPath( NULL, from_hx, from_hy, to_hx, to_hy, steps, -1 ) )
        return 0;
    return (uint) steps.size();
}

bool FOMapper::SScriptFunc::Global_GetHexPos( ushort hx, ushort hy, int& x, int& y )
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

bool FOMapper::SScriptFunc::Global_GetMonitorHex( int x, int y, ushort& hx, ushort& hy )
{
    ushort hx_ = 0, hy_ = 0;
    if( Self->GetMouseHex( hx_, hy_ ) )
    {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

uint FOMapper::SScriptFunc::Global_GetAngelScriptProperty( int property )
{
    asIScriptEngine* engine = Script::GetEngine();
    if( !engine )
        SCRIPT_ERROR_R0( "Can't get engine." );
    return (uint) engine->GetEngineProperty( (asEEngineProp) property );
}

bool FOMapper::SScriptFunc::Global_SetAngelScriptProperty( int property, uint value )
{
    asIScriptEngine* engine = Script::GetEngine();
    if( !engine )
        SCRIPT_ERROR_R0( "Can't get engine." );
    int result = engine->SetEngineProperty( (asEEngineProp) property, value );
    if( result < 0 )
        SCRIPT_ERROR_R0( "Invalid data. Property not setted." );
    return true;
}

uint FOMapper::SScriptFunc::Global_GetStrHash( CScriptString* str )
{
    if( str )
        return Str::GetHash( str->c_str() );
    return 0;
}

bool FOMapper::SScriptFunc::Global_LoadDataFile( CScriptString& dat_name )
{
    if( FileManager::LoadDataFile( dat_name.c_std_str().find( ':' ) == string::npos ? ( GameOpt.ClientPath + dat_name.c_std_str() ).c_str() : dat_name.c_str() ) )
    {
        // Reload resource manager
        if( Self->IsMapperStarted )
        {
            ResMngr.Refresh();
            for( int tab = 0; tab < TAB_COUNT; tab++ )
                Self->RefreshTiles( tab );
        }
        return true;
    }
    return false;
}

int FOMapper::SScriptFunc::Global_GetConstantValue( int const_collection, CScriptString* name )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    if( !name || !name->length() )
        SCRIPT_ERROR_R0( "Invalid name arg." );
    return ConstantsManager::GetValue( const_collection, name->c_str() );
}

CScriptString* FOMapper::SScriptFunc::Global_GetConstantName( int const_collection, int value )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    return new CScriptString( ConstantsManager::GetName( const_collection, value ) );
}

void FOMapper::SScriptFunc::Global_AddConstant( int const_collection, CScriptString* name, int value )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R( "Invalid namesFile arg." );
    if( !name || !name->length() )
        SCRIPT_ERROR_R( "Invalid name arg." );
    ConstantsManager::AddConstant( const_collection, name->c_str(), value );
}

bool FOMapper::SScriptFunc::Global_LoadConstants( int const_collection, CScriptString* file_name, int path_type )
{
    if( const_collection < 0 || const_collection > 1000 )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    if( !file_name || !file_name->length() )
        SCRIPT_ERROR_R0( "Invalid fileName arg." );
    return ConstantsManager::AddCollection( const_collection, file_name->c_str(), path_type );
}

bool FOMapper::SScriptFunc::Global_LoadFont( int font_index, CScriptString& font_fname )
{
    if( font_fname.c_str()[ 0 ] == '*' )
        return SprMngr.LoadFontFO( font_index, font_fname.c_str() + 1 );
    return SprMngr.LoadFontBMF( font_index, font_fname.c_str() );
}

void FOMapper::SScriptFunc::Global_SetDefaultFont( int font, uint color )
{
    SprMngr.SetDefaultFont( font, color );
}

uint FOMapper::SScriptFunc::Global_LoadSprite( CScriptString& spr_name, int path_index )
{
    if( path_index >= PATH_LIST_COUNT )
        SCRIPT_ERROR_R0( "Invalid path index arg." );
    return Self->AnimLoad( spr_name.c_str(), path_index, RES_SCRIPT );
}

uint FOMapper::SScriptFunc::Global_LoadSpriteHash( uint name_hash, uchar dir )
{
    return Self->AnimLoad( name_hash, dir, RES_SCRIPT );
}

int FOMapper::SScriptFunc::Global_GetSpriteWidth( uint spr_id, int spr_index )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return 0;
    SpriteInfo* si = SprMngr.GetSpriteInfo( spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ) );
    if( !si )
        return 0;
    return si->Width;
}

int FOMapper::SScriptFunc::Global_GetSpriteHeight( uint spr_id, int spr_index )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return 0;
    SpriteInfo* si = SprMngr.GetSpriteInfo( spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ) );
    if( !si )
        return 0;
    return si->Height;
}

uint FOMapper::SScriptFunc::Global_GetSpriteCount( uint spr_id )
{
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    return anim ? anim->CntFrm : 0;
}

void FOMapper::SScriptFunc::Global_GetTextInfo( CScriptString& text, int w, int h, int font, int flags, int& tw, int& th, int& lines )
{
    SprMngr.GetTextInfo( w, h, text.c_str(), font, flags, tw, th, lines );
}

void FOMapper::SScriptFunc::Global_DrawSprite( uint spr_id, int spr_index, int x, int y, uint color )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return;
    SprMngr.DrawSprite( spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ), x, y, color );
}

void FOMapper::SScriptFunc::Global_DrawSpriteOffs( uint spr_id, int spr_index, int x, int y, uint color, bool offs )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return;
    uint spr_id_ = ( spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ) );
    if( offs )
    {
        SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id_ );
        if( !si )
            return;
        x += -si->Width / 2 + si->OffsX;
        y += -si->Height + si->OffsY;
    }
    SprMngr.DrawSprite( spr_id_, x, y, color );
}

void FOMapper::SScriptFunc::Global_DrawSpriteSize( uint spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, uint color )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return;
    SprMngr.DrawSpriteSize( spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ), x, y, (float) w, (float) h, scratch, true, color );
}

void FOMapper::SScriptFunc::Global_DrawSpriteSizeOffs( uint spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, uint color, bool offs )
{
    if( !SpritesCanDraw || !spr_id )
        return;
    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return;
    uint spr_id_ = ( spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ) );
    if( offs )
    {
        SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id_ );
        if( !si )
            return;
        x += si->OffsX;
        y += si->OffsY;
    }
    SprMngr.DrawSpriteSize( spr_id_, x, y, (float) w, (float) h, scratch, true, color );
}

void FOMapper::SScriptFunc::Global_DrawText( CScriptString& text, int x, int y, int w, int h, uint color, int font, int flags )
{
    if( !SpritesCanDraw )
        return;
    if( !w && x < GameOpt.ScreenWidth )
        w = GameOpt.ScreenWidth - x;
    if( !h && y < GameOpt.ScreenHeight )
        h = GameOpt.ScreenHeight - y;
    SprMngr.DrawStr( Rect( x, y, x + w, y + h ), text.c_str(), flags, color, font );
}

void FOMapper::SScriptFunc::Global_DrawPrimitive( int primitive_type, CScriptArray& data )
{
    if( !SpritesCanDraw )
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

void FOMapper::SScriptFunc::Global_DrawMapSprite( ushort hx, ushort hy, ushort proto_id, uint spr_id, int spr_index, int ox, int oy )
{
    if( !Self->HexMngr.SpritesCanDrawMap )
        return;
    if( !Self->HexMngr.GetHexToDraw( hx, hy ) )
        return;

    AnyFrames* anim = Self->AnimGetFrames( spr_id );
    if( !anim || spr_index >= (int) anim->GetCnt() )
        return;

    ProtoItem* proto_item = ItemMngr.GetProtoItem( proto_id );
    bool       is_flat = ( proto_item ? FLAG( proto_item->Flags, ITEM_FLAT ) : false );
    bool       is_item = ( proto_item ? proto_item->IsItem() : false );
    bool       is_wall = ( proto_item ? proto_item->IsWall() : false );
    bool       no_light = ( is_flat && !is_item );

    Field&     f = Self->HexMngr.GetField( hx, hy );
    Sprites&   tree = Self->HexMngr.GetDrawTree();
    Sprite&    spr = tree.InsertSprite( is_flat ? ( is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY ) : ( is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY ),
                                        hx, hy + ( proto_item ? proto_item->DrawOrderOffsetHexY : 0 ), 0,
                                        f.ScrX + HEX_OX + ox, f.ScrY + HEX_OY + oy, spr_index < 0 ? anim->GetCurSprId() : anim->GetSprId( spr_index ), NULL, NULL, NULL, NULL, NULL );
    if( !no_light )
        spr.SetLight( Self->HexMngr.GetLightHex( 0, 0 ), Self->HexMngr.GetMaxHexX(), Self->HexMngr.GetMaxHexY() );

    if( proto_item )
    {
        if( !is_flat && !proto_item->DisableEgg )
        {
            int egg_type = 0;
            switch( proto_item->Corner )
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

        if( FLAG( proto_item->Flags, ITEM_COLORIZE ) )
        {
            spr.SetAlpha( ( (uchar*) &proto_item->LightColor ) + 3 );
            spr.SetColor( proto_item->LightColor & 0xFFFFFF );
        }

        if( FLAG( proto_item->Flags, ITEM_BAD_ITEM ) )
            spr.SetContour( CONTOUR_RED );
    }
}

void FOMapper::SScriptFunc::Global_DrawCritter2d( uint crtype, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color )
{
    if( CritType::IsEnabled( crtype ) )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( crtype, anim1, anim2, dir );
        if( anim )
            SprMngr.DrawSpriteSize( anim->Ind[ 0 ], l, t, (float) ( r - l ), (float) ( b - t ), scratch, center, color ? color : COLOR_IFACE );
    }
}

Animation3dVec DrawCritter3dAnim;
UIntVec        DrawCritter3dCrType;
UIntVec        DrawCritter3dFailToLoad;
int            DrawCritter3dLayers[ LAYERS3D_COUNT ];
void FOMapper::SScriptFunc::Global_DrawCritter3d( uint instance, uint crtype, uint anim1, uint anim2, CScriptArray* layers, CScriptArray* position, uint color )
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // stencil l t r b
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

        Animation3d*& anim = DrawCritter3dAnim[ instance ];
        if( !anim || DrawCritter3dCrType[ instance ] != crtype )
        {
            SAFEDEL( anim );
            anim = Animation3d::GetAnimation( Str::FormatBuf( "%s.fo3d", CritType::GetName( crtype ) ), PT_ART_CRITTERS, false );
            DrawCritter3dCrType[ instance ] = crtype;
            DrawCritter3dFailToLoad[ instance ] = 0;

            if( !anim )
            {
                DrawCritter3dFailToLoad[ instance ] = 1;
                return;
            }
            anim->EnableShadow( false );
        }

        if( anim )
        {
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

            memzero( DrawCritter3dLayers, sizeof( DrawCritter3dLayers ) );
            for( uint i = 0, j = ( layers ? layers->GetSize() : 0 ); i < j && i < LAYERS3D_COUNT; i++ )
                DrawCritter3dLayers[ i ] = *(int*) layers->At( i );

            anim->SetRotation( rx * PI_VALUE / 180.0f, ry * PI_VALUE / 180.0f, rz * PI_VALUE / 180.0f );
            anim->SetScale( sx, sy, sz );
            anim->SetSpeed( speed );
            anim->SetAnimation( anim1, anim2, DrawCritter3dLayers, 0 );
            RectF r = RectF( stl, stt, str, stb );
            SprMngr.Draw3d( (int) x, (int) y, 1.0f, anim, stl < str && stt < stb ? &r : NULL, color ? color : COLOR_IFACE );
        }
    }
}

bool FOMapper::SScriptFunc::Global_IsCritterCanWalk( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanWalk( cr_type );
}

bool FOMapper::SScriptFunc::Global_IsCritterCanRun( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanRun( cr_type );
}

bool FOMapper::SScriptFunc::Global_IsCritterCanRotate( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanRotate( cr_type );
}

bool FOMapper::SScriptFunc::Global_IsCritterCanAim( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanAim( cr_type );
}

bool FOMapper::SScriptFunc::Global_IsCritterAnim1( uint cr_type, uint index )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsAnim1( cr_type, index );
}

int FOMapper::SScriptFunc::Global_GetCritterAnimType( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::GetAnimType( cr_type );
}

uint FOMapper::SScriptFunc::Global_GetCritterAlias( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::GetAlias( cr_type );
}

CScriptString* FOMapper::SScriptFunc::Global_GetCritterTypeName( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_RX( "Invalid critter type arg.", new CScriptString( "" ) );
    return new CScriptString( CritType::GetCritType( cr_type ).Name );
}

CScriptString* FOMapper::SScriptFunc::Global_GetCritterSoundName( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_RX( "Invalid critter type arg.", new CScriptString( "" ) );
    return new CScriptString( CritType::GetSoundName( cr_type ) );
}
