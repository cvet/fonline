#include "Common.h"
#include "Client.h"

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

bool FOClient::IfaceLoadRect( Rect& comp, const char* name )
{
    const char* res = IfaceIni.GetStr( "", name );
    if( !res )
        return false;

    if( sscanf( res, "%d%d%d%d", &comp[ 0 ], &comp[ 1 ], &comp[ 2 ], &comp[ 3 ] ) != 4 )
    {
        comp.Clear();
        WriteLog( "Unable to parse signature '%s'.\n", name );
        return false;
    }

    return true;
}

void FOClient::IfaceLoadRect2( Rect& comp, const char* name, int ox, int oy )
{
    if( IfaceLoadRect( comp, name ) )
        comp = Rect( comp, ox, oy );
}

void FOClient::IfaceLoadSpr( AnyFrames*& comp, const char* name )
{
    const char* res = IfaceIni.GetStr( "", name, "none.png" );
    comp = SprMngr.LoadAnimation( res, PT_ART_INTRFACE, true );
}

void FOClient::IfaceLoadAnim( uint& comp, const char* name )
{
    const char* res = IfaceIni.GetStr( "", name, "none.png" );
    comp = AnimLoad( res, PT_ART_INTRFACE, RES_ATLAS_STATIC );
}

void FOClient::IfaceLoadArray( IntVec& arr, const char* name )
{
    const char* res = IfaceIni.GetStr( "", name );
    if( res )
        Str::ParseLine( res, ' ', arr, Str::AtoI );
}

bool FOClient::AppendIfaceIni( const char* ini_name )
{
    // Process names collection
    static char default_name[] = "default.ini";
    if( !ini_name || !ini_name[ 0 ] )
        ini_name = default_name;

    bool founded = false;
    for( uint i = 0, j = (uint) IfaceIniNames.size(); i < j; i++ )
    {
        if( Str::CompareCase( IfaceIniNames[ i ].c_str(), ini_name ) )
        {
            founded = true;
            break;
        }
    }
    if( !founded )
        IfaceIniNames.push_back( string( ini_name ) );

    // Build ini file
    IfaceIni.Clear();
    for( uint i = 0, j = (uint) IfaceIniNames.size(); i < j; i++ )
    {
        // Create name
        char file_name[ MAX_FOPATH ];
        Str::Copy( file_name, IfaceIniNames[ i ].c_str() );
        if( !Str::Substring( ini_name, "\\" ) && !Str::Substring( ini_name, "/" ) )
            FileManager::GetDataPath( file_name, PT_ART_INTRFACE, file_name );
        NormalizePathSlashes( file_name );
        char file_name_lower[ MAX_FOPATH ];
        Str::Copy( file_name_lower, file_name );
        Str::Lower( file_name_lower );

        // Data files
        DataFileVec& data_files = FileManager::GetDataFiles();
        for( int k = (int) data_files.size() - 1; k >= 0; k-- )
        {
            DataFile* data_file = data_files[ k ];
            uint      len;
            uint64    write_time;
            uchar*    data = data_file->OpenFile( file_name, file_name_lower, len, write_time );
            if( data )
            {
                AppendIfaceIni( data, len );
                delete[] data;
            }
        }
    }

    return true;
}

void FOClient::AppendIfaceIni( uchar* data, uint len )
{
    typedef multimap< int, char* > IniMMap;
    IniMMap sections;

    int     w = 0, h = 0;
    char*   begin = (char*) data;
    char*   end = Str::Substring( begin, "\nresolution " );
    while( true )
    {
        if( w <= GameOpt.ScreenWidth && h <= GameOpt.ScreenHeight )
        {
            uint  l = (uint) ( end ? (size_t) end - (size_t) begin : (size_t) ( data + len ) - (size_t) begin );
            char* buf = new char[ l + 1 ];
            memcpy( buf, begin, l );
            buf[ l ] = 0;
            sections.insert( PAIR( w, buf ) );
        }

        if( !end )
            break;

        end += Str::Length( "\nresolution " );
        if( sscanf( end, "%d%d", &w, &h ) != 2 )
            break;
        Str::GoTo( end, '\n', true );

        begin = end;
        end = Str::Substring( begin, "\nresolution " );
    }

    for( auto it = sections.begin(), end = sections.end(); it != end; ++it )
    {
        IfaceIni.AppendStr( it->second );
        delete[] it->second;
    }
}

int FOClient::InitIface()
{
    WriteLog( "Interface initialization...\n" );

/************************************************************************/
/* Data                                                                 */
/************************************************************************/
    // Other
    IfaceHold = IFACE_NONE;
    TargetSmth.Clear();

    // Barter
    BarterPlayerId = 0;
    BarterK = 0;
    BarterIsPlayers = false;
    BarterOpponentHide = false;
    BarterOffer = false;
    BarterOpponentOffer = false;

    // LMenu
    LMenuCurNode = 0;
    LMenuActive = false;
    LMenuTryActivated = false;
    LMenuStartTime = 0;
    LMenuX = 0;
    LMenuY = 0;
    LMenuRestoreCurX = 0;
    LMenuRestoreCurY = 0;
    LMenuHeightOffset = 0;
    LMenuSet( LMENU_OFF );
    LMenuNodeHeight = IfaceIni.GetInt( "", "LMenuNodeHeight", 40 );
    LMenuNodes.push_back( LMENU_NODE_LOOK );

    // Minimap
    LmapZoom = 2;
    LmapSwitchHi = false;
    LmapPrepareNextTick = 0;

    // Global map
    const char* tiles_pic = IfaceIni.GetStr( "", "GmapTilesPic" );
    if( !tiles_pic )
        WriteLogF( _FUNC_, " - <GmapTilesPic> signature not found.\n" );
    Str::Copy( GmapTilesPic, tiles_pic );
    GmapTilesX = IfaceIni.GetInt( "", "GmapTilesX", 0 );
    GmapTilesY = IfaceIni.GetInt( "", "GmapTilesY", 0 );
    GmapPic.resize( GmapTilesX * GmapTilesY );
    GmapFog.Create( GM__MAXZONEX, GM__MAXZONEY, nullptr );

    // Other
    IfaceLoadRect( GmapWMain, "GmapMain" );
    GmapX = GmapWMain.L;
    GmapY = GmapWMain.T;
    IfaceLoadRect2( GmapWMap, "GmapMap", GmapX, GmapY );
    IfaceLoadRect2( GmapBTown, "GmapTown", GmapX, GmapY );
    IfaceLoadRect2( GmapWName, "GmapName", GmapX, GmapY );
    IfaceLoadRect2( GmapWCar, "GmapCar", GmapX, GmapY );
    IfaceLoadRect2( GmapWTime, "GmapTime", GmapX, GmapY );
    IfaceLoadRect2( GmapWDayTime, "GmapDayTime", GmapX, GmapY );
    IfaceLoadRect2( GmapBInv, "GmapInv", GmapX, GmapY );
    IfaceLoadRect2( GmapBMenu, "GmapMenu", GmapX, GmapY );
    IfaceLoadRect2( GmapBCha, "GmapCha", GmapX, GmapY );
    IfaceLoadRect2( GmapBPip, "GmapPip", GmapX, GmapY );
    IfaceLoadRect2( GmapBFix, "GmapFix", GmapX, GmapY );
    GmapOffsetX = GmapWMap.W() / 2 + GmapWMap.L;
    GmapOffsetY = GmapWMap.H() / 2 + GmapWMap.T;
    IfaceLoadRect2( GmapWLock, "GmapLock", GmapX, GmapY );
    GmapWNameStepX = IfaceIni.GetInt( "", "GmapNameStepX", 0 );
    GmapWNameStepY = IfaceIni.GetInt( "", "GmapNameStepY", 22 );
    IfaceLoadRect2( GmapWTabs, "GmapTabs", GmapX, GmapY );
    IfaceLoadRect2( GmapBTabsScrUp, "GmapTabsScrUp", GmapX, GmapY );
    IfaceLoadRect2( GmapBTabsScrDn, "GmapTabsScrDn", GmapX, GmapY );
    IfaceLoadRect( GmapWTab, "GmapTab" );
    IfaceLoadRect( GmapWTabLoc, "GmapTabLocImage" );
    IfaceLoadRect( GmapBTabLoc, "GmapTabLoc" );
    GmapTabNextX = IfaceIni.GetInt( "", "GmapTabNextX", 0 );
    GmapTabNextY = IfaceIni.GetInt( "", "GmapTabNextY", 0 );
    GmapNullParams();
    GmapTabsScrX = 0;
    GmapTabsScrY = 0;
    GmapVectX = 0;
    GmapVectY = 0;
    GmapNextShowEntrancesTick = 0;
    GmapShowEntrancesLocId = 0;
    memzero( GmapShowEntrances, sizeof( GmapShowEntrances ) );
    GmapPTownInOffsX = IfaceIni.GetInt( "", "GmapTownInOffsX", 0 );
    GmapPTownInOffsY = IfaceIni.GetInt( "", "GmapTownInOffsY", 0 );
    GmapPTownViewOffsX = IfaceIni.GetInt( "", "GmapTownViewOffsX", 0 );
    GmapPTownViewOffsY = IfaceIni.GetInt( "", "GmapTownViewOffsY", 0 );
    GmapZoom = 1.0f;

    // PickUp
    PupTransferType = 0;
    PupContId = 0;
    PupContPid = 0;
    PupCount = 0;
    Item::ClearItems( PupCont2 );

    // Pipboy
    Automaps.clear();

    // FixBoy
    IfaceLoadRect( FixWMain, "FixMain" );
    IfaceLoadRect( FixBScrUp, "FixScrUp" );
    IfaceLoadRect( FixBScrDn, "FixScrDn" );
    IfaceLoadRect( FixBDone, "FixDone" );
    IfaceLoadRect( FixWWin, "FixWin" );
    IfaceLoadRect( FixBFix, "FixFix" );
    FixVectX = 0;
    FixVectY = 0;
    FixX = ( GameOpt.ScreenWidth - FixWMain.W() ) / 2;
    FixY = ( GameOpt.ScreenHeight - FixWMain.H() ) / 2;
    FixMode = FIX_MODE_LIST;
    FixCurCraft = -1;
    FixScrollLst = 0;
    FixScrollFix = 0;
    FixResult = 0;
    FixResultStr = "";
    FixShowCraft.clear();
    FixNextShowCraftTick = 0;

    // Save/Load
    IfaceLoadRect( SaveLoadMain, "SaveLoadMain" );
    IfaceLoadRect( SaveLoadText, "SaveLoadText" );
    IfaceLoadRect( SaveLoadScrUp, "SaveLoadScrUp" );
    IfaceLoadRect( SaveLoadScrDown, "SaveLoadScrDown" );
    IfaceLoadRect( SaveLoadSlots, "SaveLoadSlots" );
    IfaceLoadRect( SaveLoadPic, "SaveLoadPic" );
    IfaceLoadRect( SaveLoadInfo, "SaveLoadInfo" );
    IfaceLoadRect( SaveLoadDone, "SaveLoadDone" );
    IfaceLoadRect( SaveLoadDoneText, "SaveLoadDoneText" );
    IfaceLoadRect( SaveLoadBack, "SaveLoadBack" );
    IfaceLoadRect( SaveLoadBackText, "SaveLoadBackText" );
    SaveLoadCX = ( GameOpt.ScreenWidth - SaveLoadMain.W() ) / 2;
    SaveLoadCY = ( GameOpt.ScreenHeight - SaveLoadMain.H() ) / 2;
    SaveLoadX = SaveLoadCX;
    SaveLoadY = SaveLoadCY;
    SaveLoadVectX = 0;
    SaveLoadVectY = 0;
    SaveLoadLoginScreen = false;
    SaveLoadSave = false;
    SaveLoadClickSlotTick = 0;
    SaveLoadClickSlotIndex = 0;
    SaveLoadSlotIndex = 0;
    SaveLoadSlotScroll = 0;
    SaveLoadSlotsMax = 0;
    SaveLoadDataSlots.clear();
    // Save/load surface creating
    if( Singleplayer )
    {
        if( !SaveLoadDraft )
            SaveLoadDraft = SprMngr.CreateRenderTarget( false, false, false, SAVE_LOAD_IMAGE_WIDTH, SAVE_LOAD_IMAGE_HEIGHT, true );
    }
    SaveLoadProcessDraft = false;
    SaveLoadDraftValid = false;

/************************************************************************/
/* Sprites                                                              */
/************************************************************************/
    // Hex field sprites
    HexMngr.ReloadSprites();

    // LMenu
    IfaceLoadSpr( LmenuPTalkOff, "LMenuTalkPic" );
    IfaceLoadSpr( LmenuPTalkOn, "LMenuTalkPicDn" );
    IfaceLoadSpr( LmenuPLookOff, "LMenuLookPic" );
    IfaceLoadSpr( LmenuPLookOn, "LMenuLookPicDn" );
    IfaceLoadSpr( LmenuPBreakOff, "LMenuCancelPic" );
    IfaceLoadSpr( LmenuPBreakOn, "LMenuCancelPicDn" );
    IfaceLoadSpr( LmenuPUseOff, "LMenuUsePic" );
    IfaceLoadSpr( LmenuPUseOn, "LMenuUsePicDn" );
    IfaceLoadSpr( LmenuPGMFollowOff, "LMenuGMFollowPic" );
    IfaceLoadSpr( LmenuPGMFollowOn, "LMenuGMFollowPicDn" );
    IfaceLoadSpr( LmenuPRotateOff, "LMenuRotatePic" );
    IfaceLoadSpr( LmenuPRotateOn, "LMenuRotatePicDn" );
    IfaceLoadSpr( LmenuPDropOff, "LMenuDropPic" );
    IfaceLoadSpr( LmenuPDropOn, "LMenuDropPicDn" );
    IfaceLoadSpr( LmenuPUnloadOff, "LMenuUnloadPic" );
    IfaceLoadSpr( LmenuPUnloadOn, "LMenuUnloadPicDn" );
    IfaceLoadSpr( LmenuPSortUpOff, "LMenuSortUpPic" );
    IfaceLoadSpr( LmenuPSortUpOn, "LMenuSortUpPicDn" );
    IfaceLoadSpr( LmenuPSortDownOff, "LMenuSortDownPic" );
    IfaceLoadSpr( LmenuPSortDownOn, "LMenuSortDownPicDn" );
    IfaceLoadSpr( LmenuPPickItemOff, "LMenuPickItemPic" );
    IfaceLoadSpr( LmenuPPickItemOn, "LMenuPickItemPicDn" );
    IfaceLoadSpr( LmenuPPushOff, "LMenuPushPic" );
    IfaceLoadSpr( LmenuPPushOn, "LMenuPushPicDn" );
    IfaceLoadSpr( LmenuPBagOff, "LMenuBagPic" );
    IfaceLoadSpr( LmenuPBagOn, "LMenuBagPicDn" );
    IfaceLoadSpr( LmenuPSkillOff, "LMenuSkillPic" );
    IfaceLoadSpr( LmenuPSkillOn, "LMenuSkillPicDn" );
    IfaceLoadSpr( LmenuPBarterOpenOff, "LMenuBarterOpenPic" );
    IfaceLoadSpr( LmenuPBarterOpenOn, "LMenuBarterOpenPicDn" );
    IfaceLoadSpr( LmenuPBarterHideOff, "LMenuBarterHidePic" );
    IfaceLoadSpr( LmenuPBarterHideOn, "LMenuBarterHidePicDn" );
    IfaceLoadSpr( LmenuPGmapKickOff, "LMenuGmapKickPic" );
    IfaceLoadSpr( LmenuPGmapKickOn, "LMenuGmapKickPicDn" );
    IfaceLoadSpr( LmenuPGmapRuleOff, "LMenuGmapRulePic" );
    IfaceLoadSpr( LmenuPGmapRuleOn, "LMenuGmapRulePicDn" );
    IfaceLoadSpr( LmenuPVoteUpOff, "LMenuVoteUpPic" );
    IfaceLoadSpr( LmenuPVoteUpOn, "LMenuVoteUpPicDn" );
    IfaceLoadSpr( LmenuPVoteDownOff, "LMenuVoteDownPic" );
    IfaceLoadSpr( LmenuPVoteDownOn, "LMenuVoteDownPicDn" );

    // Global map
    IfaceLoadSpr( GmapWMainPic, "GmapMainPic" );
    IfaceLoadSpr( GmapPBTownDw, "GmapTownPicDn" );
    IfaceLoadSpr( GmapPGr, "GmapGroupLocPic" );
    IfaceLoadSpr( GmapPTarg, "GmapGroupTargPic" );
    IfaceLoadSpr( GmapPStay, "GmapStayPic" );
    IfaceLoadSpr( GmapPStayDn, "GmapStayPicDn" );
    IfaceLoadSpr( GmapPStayMask, "GmapStayPicMask" );
    IfaceLoadSpr( GmapPTownInPic, "GmapTownInPic" );
    IfaceLoadSpr( GmapPTownInPicDn, "GmapTownInPicDn" );
    IfaceLoadSpr( GmapPTownInPicMask, "GmapTownInPicMask" );
    IfaceLoadSpr( GmapPTownViewPic, "GmapTownViewPic" );
    IfaceLoadSpr( GmapPTownViewPicDn, "GmapTownViewPicDn" );
    IfaceLoadSpr( GmapPTownViewPicMask, "GmapTownViewPicMask" );
    IfaceLoadSpr( GmapLocPic, "GmapLocPic" );
    IfaceLoadSpr( GmapPFollowCrit, "GmapFollowCritPic" );
    IfaceLoadSpr( GmapPFollowCritSelf, "GmapFollowCritSelfPic" );
    IfaceLoadSpr( GmapPWTab, "GmapTabPic" );
    IfaceLoadSpr( GmapPWBlankTab, "GmapBlankTabPic" );
    IfaceLoadSpr( GmapPBTabLoc, "GmapTabLocPicDn" );
    IfaceLoadSpr( GmapPTabScrUpDw, "GmapTabsScrUpPicDn" );
    IfaceLoadSpr( GmapPTabScrDwDw, "GmapTabsScrDnPicDn" );
    IfaceLoadAnim( GmapWDayTimeAnim, "GmapDayTimeAnim" );
    IfaceLoadSpr( GmapBInvPicDown, "GmapInvPicDn" );
    IfaceLoadSpr( GmapBMenuPicDown, "GmapMenuPicDn" );
    IfaceLoadSpr( GmapBChaPicDown, "GmapChaPicDn" );
    IfaceLoadSpr( GmapBPipPicDown, "GmapPipPicDn" );
    IfaceLoadSpr( GmapBFixPicDown, "GmapFixPicDn" );
    IfaceLoadSpr( GmapPLightPic0, "GmapLightPic0" );
    IfaceLoadSpr( GmapPLightPic1, "GmapLightPic1" );

    // FixBoy
    IfaceLoadSpr( FixMainPic, "FixMainPic" );
    IfaceLoadSpr( FixPBScrUpDn, "FixScrUpPicDn" );
    IfaceLoadSpr( FixPBScrDnDn, "FixScrDnPicDn" );
    IfaceLoadSpr( FixPBDoneDn, "FixDonePicDn" );
    IfaceLoadSpr( FixPBFixDn, "FixFixPicDn" );

    // Save/Load
    IfaceLoadSpr( SaveLoadMainPic, "SaveLoadMainPic" );
    IfaceLoadSpr( SaveLoadScrUpPicDown, "SaveLoadScrUpPicDn" );
    IfaceLoadSpr( SaveLoadScrDownPicDown, "SaveLoadScrDownPicDn" );
    IfaceLoadSpr( SaveLoadDonePicDown, "SaveLoadDonePicDn" );
    IfaceLoadSpr( SaveLoadBackPicDown, "SaveLoadBackPicDn" );

    WriteLog( "Interface initialization complete.\n" );
    return 0;
}

#define INDICATOR_CHANGE_TICK    ( 35 )
void FOClient::DrawIndicator( Rect& rect, PointVec& points, uint color, int procent, uint& tick, bool is_vertical, bool from_top_or_left )
{
    if( Timer::GameTick() >= tick )
    {
        uint points_count = ( is_vertical ? rect.H() : rect.W() ) / 2 * procent / 100;
        if( !points_count && procent )
            points_count = 1;
        if( points.size() != points_count )
        {
            if( points_count > (uint) points.size() )
                points_count = (uint) points.size() + 1;
            else
                points_count = (uint) points.size() - 1;
            points.resize( points_count );
            for( uint i = 0; i < points_count; i++ )
            {
                if( is_vertical )
                {
                    if( from_top_or_left )
                        points[ i ] = PrepPoint( rect[ 0 ], rect[ 1 ] + i * 2, color, nullptr, nullptr );
                    else
                        points[ i ] = PrepPoint( rect[ 0 ], rect[ 3 ] - i * 2, color, nullptr, nullptr );
                }
                else
                {
                    if( from_top_or_left )
                        points[ i ] = PrepPoint( rect[ 0 ] + i * 2, rect[ 1 ], color, nullptr, nullptr );
                    else
                        points[ i ] = PrepPoint( rect[ 2 ] - i * 2, rect[ 1 ], color, nullptr, nullptr );
                }
            }
        }
        tick = Timer::GameTick() + INDICATOR_CHANGE_TICK;
    }
    if( points.size() > 0 )
        SprMngr.DrawPoints( points, PRIMITIVE_POINTLIST );
}

uint FOClient::GetCurContainerItemId( const Rect& pos, int height, int scroll, ItemVec& cont )
{
    if( !IsCurInRect( pos ) )
        return 0;
    auto it = cont.begin();
    int  pos_cur = ( GameOpt.MouseY - pos.T ) / height;
    for( int i = 0; it != cont.end(); ++it, ++i )
    {
        if( i - scroll != pos_cur )
            continue;
        return ( *it )->GetId();
    }
    return 0;
}

void FOClient::ContainerDraw( const Rect& pos, int height, int scroll, ItemVec& cont, uint skip_id )
{
    int i = 0, i2 = 0;
    for( auto it = cont.begin(), end = cont.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == skip_id )
            continue;
        if( i >= scroll && i < scroll + pos.H() / height )
        {
            AnyFrames* anim = ResMngr.GetInvAnim( item->GetPicInv() );
            if( anim )
                SprMngr.DrawSpriteSize( anim->GetCurSprId(), pos.L, pos.T + ( i2 * height ), pos.W(), height, false, true, item->GetInvColor() );
            i2++;
        }
        i++;
    }

    SprMngr.Flush();

    i = 0, i2 = 0;
    for( auto it = cont.begin(), end = cont.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == skip_id )
            continue;
        if( i >= scroll && i < scroll + pos.H() / height )
        {
            if( item->GetCount() > 1 )
                SprMngr.DrawStr( Rect( pos.L, pos.T + ( i2 * height ), pos.R, pos.T + ( i2 * height ) + height ), Str::FormatBuf( "x%u", item->GetCount() ), 0, COLOR_TEXT_WHITE );
            i2++;
        }
        i++;
    }
}

Item* FOClient::GetContainerItem( ItemVec& cont, uint id )
{
    auto it = PtrCollectionFind( cont.begin(), cont.end(), id );
    return it != cont.end() ? *it : nullptr;
}

void FOClient::CollectContItems()
{
    Item::ClearItems( InvContInit );
    if( Chosen )
    {
        Chosen->GetInvItems( InvContInit );
        for( size_t i = 0; i < InvContInit.size(); i++ )
            InvContInit[ i ] = InvContInit[ i ]->Clone();
    }

    if( IsScreenPresent( SCREEN__BARTER ) )
    {
        // Manage offered items
        for( auto it = BarterCont1oInit.begin(); it != BarterCont1oInit.end();)
        {
            Item* item = *it;
            auto  it_ = PtrCollectionFind( InvContInit.begin(), InvContInit.end(), item->GetId() );
            if( it_ == InvContInit.end() || ( *it_ )->GetCount() < item->GetCount() )
            {
                item->Release();
                it = BarterCont1oInit.erase( it );
            }
            else
            {
                ++it;
            }
        }
        for( auto it = InvContInit.begin(); it != InvContInit.end();)
        {
            Item* item = *it;
            auto  it_ = PtrCollectionFind( BarterCont1oInit.begin(), BarterCont1oInit.end(), item->GetId() );
            if( it_ != BarterCont1oInit.end() )
            {
                Item* item_ = *it_;

                uint  count = item_->GetCount();
                item_->Props = item->Props;
                item_->SetCount( count );

                if( item->GetStackable() )
                    item->ChangeCount( -(int) item_->GetCount() );
                if( !item->GetStackable() || !item->GetCount() )
                {
                    item->Release();
                    it = InvContInit.erase( it );
                    continue;
                }
            }
            ++it;
        }
    }

    ProcessItemsCollection( ITEMS_INVENTORY, InvContInit, InvCont );
    ProcessItemsCollection( ITEMS_USE, InvContInit, UseCont );
    ProcessItemsCollection( ITEMS_BARTER, InvContInit, BarterCont1 );
    ProcessItemsCollection( ITEMS_BARTER_OFFER, BarterCont1oInit, BarterCont1o );
    ProcessItemsCollection( ITEMS_BARTER_OPPONENT, BarterCont2Init, BarterCont2 );
    ProcessItemsCollection( ITEMS_BARTER_OPPONENT_OFFER, BarterCont2oInit, BarterCont2o );
    ProcessItemsCollection( ITEMS_PICKUP, InvContInit, PupCont1 );
    ProcessItemsCollection( ITEMS_PICKUP_FROM, PupCont2Init, PupCont2 );
}

void FOClient::ProcessItemsCollection( int collection, ItemVec& init_items, ItemVec& result )
{
    // Default result
    Item::ClearItems( result );

    // Prepare to call
    if( Script::PrepareContext( ClientFunctions.ItemsCollection, _FUNC_, "Game" ) )
    {
        // Create script array
        ScriptArray* arr = Script::CreateArray( "Item@[]" );
        RUNTIME_ASSERT( arr );

        // Copy to script array
        ItemVec items_clone = init_items;
        for( size_t i = 0; i < items_clone.size(); i++ )
            items_clone[ i ] = items_clone[ i ]->Clone();
        Script::AppendVectorToArray( items_clone, arr );

        // Call
        Script::SetArgUInt( collection );
        Script::SetArgObject( arr );
        if( Script::RunPrepared() )
        {
            // Copy from script to std array
            Script::AssignScriptArrayInVector( result, arr );
            for( auto it = result.begin(), end = result.end(); it != end; ++it )
                ( *it )->AddRef();
        }

        // Release array
        arr->Release();
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::GameDraw()
{
    // Move cursor
    if( IsCurMode( CUR_MOVE ) )
    {
        ushort hx, hy;
        if( ( GameOpt.ScrollMouseRight || GameOpt.ScrollMouseLeft || GameOpt.ScrollMouseUp || GameOpt.ScrollMouseDown ) || !GetCurHex( hx, hy, false ) )
        {
            HexMngr.SetCursorVisible( false );
        }
        else
        {
            HexMngr.SetCursorVisible( true );
            HexMngr.SetCursorPos( GameOpt.MouseX, GameOpt.MouseY, Keyb::CtrlDwn, false );
        }
    }

    // Look borders
    if( RebuildLookBorders )
    {
        LookBordersPrepare();
        RebuildLookBorders = false;
    }

    // Map
    HexMngr.DrawMap();

    // Critters
    for( auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end(); it++ )
    {
        CritterCl* cr = it->second;

        // Follow pic
        if( GameOpt.ShowGroups && cr->SprDrawValid && Chosen )
        {
            if( cr->GetId() == Chosen->GetFollowCrit() )
            {
                SpriteInfo* si = SprMngr.GetSpriteInfo( GmapPFollowCrit->GetCurSprId() );
                Rect        tr = cr->GetTextRect();
                int         x = (int) ( ( tr.L + ( ( tr.R - tr.L ) / 2 ) + GameOpt.ScrOx ) / GameOpt.SpritesZoom - (float) ( si ? si->Width / 2 : 0 ) );
                int         y = (int) ( ( tr.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom - (float) ( si ? si->Height : 0 ) );
                uint        col = ( CheckDist( cr->GetHexX(), cr->GetHexY(), Chosen->GetHexX(), Chosen->GetHexY(), FOLLOW_DIST ) /* && Chosen->IsFree()*/ ) ? COLOR_IFACE : COLOR_IFACE_RED;
                SprMngr.DrawSprite( GmapPFollowCrit, x, y, col );
            }
            if( Chosen->GetId() == cr->GetFollowCrit() )
            {
                SpriteInfo* si = SprMngr.GetSpriteInfo( GmapPFollowCritSelf->GetCurSprId() );
                Rect        tr = cr->GetTextRect();
                int         x = (int) ( ( tr.L + ( ( tr.R - tr.L ) / 2 ) + GameOpt.ScrOx ) / GameOpt.SpritesZoom - (float) ( si ? si->Width / 2 : 0 ) );
                int         y = (int) ( ( tr.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom - (float) ( si ? si->Height : 0 ) );
                uint        col = ( CheckDist( cr->GetHexX(), cr->GetHexY(), Chosen->GetHexX(), Chosen->GetHexY(), FOLLOW_DIST ) /* && Chosen->IsFree()*/ ) ? COLOR_IFACE : COLOR_IFACE_RED;
                SprMngr.DrawSprite( GmapPFollowCritSelf, x, y, col );
            }
        }

        // Text on head
        cr->DrawTextOnHead();
    }

    // Texts on map
    uint tick = Timer::GameTick();
    for( auto it = GameMapTexts.begin(); it != GameMapTexts.end();)
    {
        MapText& mt = ( *it );
        if( tick >= mt.StartTick + mt.Tick )
            it = GameMapTexts.erase( it );
        else
        {
            uint   dt = tick - mt.StartTick;
            int    procent = Procent( mt.Tick, dt );
            Rect   r = mt.Pos.Interpolate( mt.EndPos, procent );
            Field& f = HexMngr.GetField( mt.HexX, mt.HexY );
            int    x = (int) ( ( f.ScrX + HEX_OX + GameOpt.ScrOx ) / GameOpt.SpritesZoom - 100.0f - (float) ( mt.Pos.L - r.L ) );
            int    y = (int) ( ( f.ScrY + HEX_OY - mt.Pos.H() - ( mt.Pos.T - r.T ) + GameOpt.ScrOy ) / GameOpt.SpritesZoom - 70.0f );

            uint   color = mt.Color;
            if( mt.Fade )
                color = ( color ^ 0xFF000000 ) | ( ( 0xFF * ( 100 - procent ) / 100 ) << 24 );
            else if( mt.Tick > 500 )
            {
                uint hide = mt.Tick - 200;
                if( dt >= hide )
                {
                    uint alpha = 0xFF * ( 100 - Procent( mt.Tick - hide, dt - hide ) ) / 100;
                    color = ( alpha << 24 ) | ( color & 0xFFFFFF );
                }
            }

            SprMngr.DrawStr( Rect( x, y, x + 200, y + 70 ), mt.Text.c_str(), FT_CENTERX | FT_BOTTOM | FT_BORDERED, color );
            it++;
        }
    }
}

void FOClient::GameKeyDown( uchar dik, const char* dik_text )
{
    if( Keyb::CtrlDwn || Keyb::ShiftDwn || Keyb::AltDwn )
        return;

    switch( dik )
    {
    case DIK_EQUALS:
    case DIK_ADD:
        GameOpt.Light += 5;
        if( GameOpt.Light > 50 )
            GameOpt.Light = 50;
        SetDayTime( true );
        break;
    case DIK_MINUS:
    case DIK_SUBTRACT:
        GameOpt.Light -= 5;
        if( GameOpt.Light < 0 )
            GameOpt.Light = 0;
        SetDayTime( true );
        break;
    default:
        break;
    }
}

void FOClient::GameLMouseDown()
{
    IfaceHold = IFACE_NONE;
    if( !Chosen )
        return;

    if( IsCurMode( CUR_DEFAULT ) )
    {
        LMenuTryActivate();
    }
    else if( IsCurMode( CUR_MOVE ) )
    {
        ActionEvent* act = ( IsAction( CHOSEN_MOVE ) ? &ChosenAction[ 0 ] : nullptr );
        ushort       hx, hy;
        if( act && Timer::FastTick() - act->Param[ 5 ] < ( GameOpt.DoubleClickTime ? GameOpt.DoubleClickTime : GetDoubleClickTicks() ) )
        {
            act->Param[ 2 ] = ( GameOpt.AlwaysRun ? 0 : 1 );
            act->Param[ 4 ] = 0;
        }
        else if( GetCurHex( hx, hy, false ) && Chosen )
        {
            uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), hx, hy );
            bool is_run = ( Keyb::ShiftDwn ? ( !GameOpt.AlwaysRun ) : ( GameOpt.AlwaysRun && dist >= GameOpt.AlwaysRunMoveDist ) );
            SetAction( CHOSEN_MOVE, hx, hy, is_run, 0, act ? 0 : 1, Timer::FastTick() );
        }
    }
    else if( IsCurMode( CUR_USE_ITEM ) || IsCurMode( CUR_USE_WEAPON ) )
    {
        bool  is_attack = IsCurMode( CUR_USE_WEAPON );
        Item* use_item = ( !is_attack && CurUseItem != 0 ? Chosen->GetItem( CurUseItem ) : Chosen->ItemSlotMain );
        if( use_item )
        {
            CritterCl* cr = nullptr;
            ItemHex*   item = nullptr;
            if( is_attack )
                cr = HexMngr.GetCritterPixel( GameOpt.MouseX, GameOpt.MouseY, true );
            else
                HexMngr.GetSmthPixel( GameOpt.MouseX, GameOpt.MouseY, item, cr );

            if( cr )
            {
                // Memory target id
                TargetSmth.SetCritter( cr->GetId() );

                // Aim shoot
                if( is_attack && Chosen->IsAim() )
                {
                    if( !CritType::IsCanAim( Chosen->GetCrType() ) )
                        AddMess( FOMB_GAME, "Aim attack is not available for this critter type." );
                    else if( !Chosen->GetIsNoAim() )
                        ShowScreen( SCREEN__AIM );
                    return;
                }

                // Use item
                SetAction( CHOSEN_USE_ITEM, use_item->GetId(), 0, TARGET_CRITTER, cr->GetId(), is_attack ? Chosen->GetFullRate() : USE_USE );
            }
            else if( item )
            {
                TargetSmth.SetItem( item->GetId() );
                SetAction( CHOSEN_USE_ITEM, use_item->GetId(), 0, item->IsItem() ? TARGET_ITEM : TARGET_SCENERY, item->GetId(), USE_USE );
            }
        }
    }
    else if( IsCurMode( CUR_USE_SKILL ) )
    {
        if( CurUseSkill != 0 )
        {
            CritterCl* cr;
            ItemHex*   item;
            HexMngr.GetSmthPixel( GameOpt.MouseX, GameOpt.MouseY, item, cr );

            // Use skill
            if( cr )
            {
                SetAction( CHOSEN_USE_SKL_ON_CRITTER, CurUseSkill, cr->GetId(), Chosen->GetFullRate() );
            }
            else if( item && item->IsCanUseSkill() )
            {
                if( item->IsScenOrGrid() )
                    SetAction( CHOSEN_USE_SKL_ON_SCEN, CurUseSkill, item->GetProtoId(), item->GetHexX(), item->GetHexY() );
                else
                    SetAction( CHOSEN_USE_SKL_ON_ITEM, false, CurUseSkill, item->GetId() );
            }
        }

        SetCurMode( CUR_DEFAULT );
    }
}

void FOClient::GameLMouseUp()
{
    IfaceHold = IFACE_NONE;
}

void FOClient::GameRMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( !IsCurInInterface( GameOpt.MouseX, GameOpt.MouseY ) )
        IfaceHold = IFACE_GAME_MNEXT;
}

void FOClient::GameRMouseUp()
{
    if( IfaceHold == IFACE_GAME_MNEXT && Chosen )
    {
        switch( GetCurMode() )
        {
        case CUR_DEFAULT:
            SetCurMode( CUR_MOVE );
            break;
        case CUR_MOVE:
            if( IS_TIMEOUT( Chosen->GetTimeoutBattle() ) && Chosen->ItemSlotMain->IsWeapon() )
                SetCurMode( CUR_USE_WEAPON );
            else
                SetCurMode( CUR_DEFAULT );
            break;
        case CUR_USE_ITEM:
            SetCurMode( CUR_DEFAULT );
            break;
        case CUR_USE_WEAPON:
            SetCurMode( CUR_DEFAULT );
            break;
        case CUR_USE_SKILL:
            SetCurMode( CUR_MOVE );
            break;
        default:
            SetCurMode( CUR_DEFAULT );
            break;
        }
    }
    IfaceHold = IFACE_NONE;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::AddMess( int mess_type, const char* msg, bool script_call )
{
    ScriptString* text = ScriptString::Create( msg );
    if( Script::PrepareContext( ClientFunctions.MessageBox, _FUNC_, "Game" ) )
    {
        Script::SetArgAddress( text );
        Script::SetArgUInt( mess_type );
        Script::SetArgBool( script_call );
        Script::RunPrepared();
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

bool FOClient::LoginCheckData()
{
    if( Singleplayer )
        return true;

    string tmp_str = GameOpt.Name->c_std_str();
    while( !tmp_str.empty() && tmp_str[ 0 ] == ' ' )
        tmp_str.erase( 0, 1 );
    while( !tmp_str.empty() && tmp_str[ tmp_str.length() - 1 ] == ' ' )
        tmp_str.erase( tmp_str.length() - 1, 1 );
    *GameOpt.Name = tmp_str;

    uint name_len_utf8 = Str::LengthUTF8( GameOpt.Name->c_str() );
    if( name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength ||
        name_len_utf8 > MAX_NAME || name_len_utf8 > GameOpt.MaxNameLength )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_WRONG_LOGIN ) );
        return false;
    }

    if( !Str::IsValidUTF8( GameOpt.Name->c_str() ) || Str::Substring( GameOpt.Name->c_str(), "*" ) )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_NAME_WRONG_CHARS ) );
        return false;
    }

    uint pass_len_utf8 = Str::LengthUTF8( Password.c_str() );
    if( pass_len_utf8 < MIN_NAME || pass_len_utf8 < GameOpt.MinNameLength ||
        pass_len_utf8 > MAX_NAME || pass_len_utf8 > GameOpt.MaxNameLength )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_WRONG_PASS ) );
        return false;
    }

    if( !Str::IsValidUTF8( Password.c_str() ) || Str::Substring( Password.c_str(), "*" ) )
    {
        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_PASS_WRONG_CHARS ) );
        return false;
    }

    // Save login and password
    Crypt.SetCache( "__name", (uchar*) GameOpt.Name->c_str(), (uint) GameOpt.Name->length() + 1 );
    Crypt.SetCache( "__pass", (uchar*) Password.c_str(), (uint) Password.length() + 1 );

    AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_CONNECTION ) );
    return true;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

bool FOClient::IsScreenPlayersBarter()
{
    return IsScreenPresent( SCREEN__BARTER ) && BarterIsPlayers;
}

void FOClient::BarterTryOffer()
{
    if( !Chosen )
        return;

    if( BarterIsPlayers )
    {
        BarterOffer = !BarterOffer;
        Net_SendPlayersBarter( BARTER_OFFER, BarterOffer, 0 );
    }
    else
    {
        if( BarterCont1oInit.empty() && BarterCont2oInit.empty() )
            return;

        uint c1, w1, v1, c2, w2, v2;
        ContainerCalcInfo( BarterCont1oInit, c1, w1, v1, -BarterK, true );
        ContainerCalcInfo( BarterCont2oInit, c2, w2, v2, Chosen->GetPerkMasterTrader() ? 0 : BarterK, false );

        if( !( c1 < c2 && BarterK ) && Chosen->GetFreeWeight() + w1 >= w2 && Chosen->GetFreeVolume() + v1 >= v2 )
        {
            Net_SendBarter( DlgNpcId, BarterCont1oInit, BarterCont2oInit );
            WaitPing();
        }
    }
}

void FOClient::BarterTransfer( uint item_id, int item_cont, uint item_count )
{
    if( !item_count || !Chosen )
        return;

    ItemVec* from_cont;
    ItemVec* to_cont;

    switch( item_cont )
    {
    case ITEMS_BARTER:
        from_cont = &InvContInit;
        to_cont = &BarterCont1oInit;
        break;
    case ITEMS_BARTER_OPPONENT:
        from_cont = &BarterCont2Init;
        to_cont = &BarterCont2oInit;
        break;
    case ITEMS_BARTER_OFFER:
        from_cont = &BarterCont1oInit;
        to_cont = &InvContInit;
        break;
    case ITEMS_BARTER_OPPONENT_OFFER:
        from_cont = &BarterCont2oInit;
        to_cont = &BarterCont2Init;
        break;
    default:
        return;
    }

    auto it = PtrCollectionFind( from_cont->begin(), from_cont->end(), item_id );
    if( it == from_cont->end() )
        return;

    Item* item = *it;
    Item* to_item = nullptr;

    if( item->GetCount() < item_count )
        return;

    if( item->GetStackable() )
    {
        auto it_to = PtrCollectionFind( to_cont->begin(), to_cont->end(), item->GetId() );
        if( it_to != to_cont->end() )
            to_item = *it_to;
    }

    if( to_item )
    {
        to_item->ChangeCount( item_count );
    }
    else
    {
        to_cont->push_back( item->Clone() );
        to_cont->back()->SetCount( item_count );
    }

    item->ChangeCount( -(int) item_count );
    if( !item->GetCount() || !item->GetStackable() )
    {
        item->Release();
        from_cont->erase( it );
    }

    switch( item_cont )
    {
    case ITEMS_BARTER:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_SET_SELF, item_id, item_count );
        break;
    case ITEMS_BARTER_OPPONENT:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_SET_OPPONENT, item_id, item_count );
        break;
    case ITEMS_BARTER_OFFER:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_UNSET_SELF, item_id, item_count );
        break;
    case ITEMS_BARTER_OPPONENT_OFFER:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_UNSET_OPPONENT, item_id, item_count );
        break;
    default:
        break;
    }

    if( Script::PrepareContext( ClientFunctions.ContainerChanged, _FUNC_, "Game" ) )
        Script::RunPrepared();
}

void FOClient::ContainerCalcInfo( ItemVec& cont, uint& cost, uint& weigth, uint& volume, int barter_k, bool sell )
{
    cost = 0;
    weigth = 0;
    volume = 0;
    for( auto it = cont.begin(); it != cont.end(); it++ )
    {
        Item* item = *it;
        if( barter_k != MAX_INT )
        {
            uint cost_ = item->GetCost1st();
            if( GameOpt.CustomItemCost )
            {
                CritterCl* npc = GetCritter( PupContId );
                if( Chosen && npc && Script::PrepareContext( ClientFunctions.ItemCost, _FUNC_, Chosen->GetInfo() ) )
                {
                    Script::SetArgEntityOK( item );
                    Script::SetArgEntityOK( Chosen );
                    Script::SetArgEntityOK( npc );
                    Script::SetArgBool( sell );
                    if( Script::RunPrepared() )
                        cost_ = Script::GetReturnedUInt();
                }
            }
            else
            {
                cost_ = cost_ * ( 100 + barter_k ) / 100;
                if( !cost_ )
                    cost_++;
            }
            cost += cost_ * item->GetCount();
        }
        weigth += item->GetWholeWeight();
        volume += item->GetWholeVolume();
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::FormatTags( char(&text)[ MAX_FOTEXT ], CritterCl* player, CritterCl* npc, const char* lexems )
{
    if( Str::CompareCase( text, "error" ) )
    {
        Str::Copy( text, "Text not found!" );
        return;
    }

    char*           str = text;
    vector< char* > dialogs;
    int             sex = 0;
    bool            sex_tags = false;
    char            tag[ MAX_FOTEXT ];
    tag[ 0 ] = 0;

    for( int i = 0; str[ i ];)
    {
        switch( str[ i ] )
        {
        case '#':
        {
            str[ i ] = '\n';
        }
        break;
        case '|':
        {
            if( sex_tags )
            {
                Str::CopyWord( tag, &str[ i + 1 ], '|', false );
                Str::EraseInterval( &str[ i ], Str::Length( tag ) + 2 );

                if( sex )
                {
                    if( sex == 1 )
                        Str::Insert( &str[ i ], tag );
                    sex--;
                }
                continue;
            }
        }
        break;
        case '@':
        {
            if( str[ i + 1 ] == '@' )
            {
                str[ i ] = 0;
                dialogs.push_back( str );
                str = &str[ i + 2 ];
                i = 0;
                continue;
            }

            tag[ 0 ] = 0;
            Str::CopyWord( tag, &str[ i + 1 ], '@', false );
            Str::EraseInterval( &str[ i ], Str::Length( tag ) + 2 );

            if( !Str::Length( tag ) )
                break;

            // Player name
            if( Str::CompareCase( tag, "pname" ) )
            {
                Str::Copy( tag, player ? player->GetName() : "" );
            }
            // Npc name
            else if( Str::CompareCase( tag, "nname" ) )
            {
                Str::Copy( tag, npc ? npc->GetName() : "" );
            }
            // Sex
            else if( Str::CompareCase( tag, "sex" ) )
            {
                sex = ( player ? player->GetGender() + 1 : 1 );
                sex_tags = true;
                continue;
            }
            // Random
            else if( Str::CompareCase( tag, "rnd" ) )
            {
                char*           str_ = str + i;
                Str::GoTo( str_, '|', false );
                char*           last_separator = str_;
                vector< char* > rnd;
                while( *str_ )
                {
                    last_separator = str_;
                    rnd.push_back( str_ );
                    str_++;
                    Str::GoTo( str_, '|', false );
                }
                if( !rnd.empty() )
                    rnd.pop_back();
                if( !rnd.empty() )
                {
                    char* rnd_str = rnd[ Random( 0, rnd.size() - 1 ) ];
                    str_ = rnd_str + 1;
                    Str::GoTo( str_, '|' );
                    Str::CopyCount( tag, rnd_str + 1, (uint) ( str_ - rnd_str ) - 1 );
                    Str::EraseInterval( str + i, (uint) ( last_separator - ( str + i ) ) + 1 );
                }
            }
            // Lexems
            else if( Str::Length( tag ) > 4 && tag[ 0 ] == 'l' && tag[ 1 ] == 'e' && tag[ 2 ] == 'x' && tag[ 3 ] == ' ' )
            {
                const char* s = Str::Substring( lexems ? lexems : "", Str::FormatBuf( "$%s", &tag[ 4 ] ) );
                if( s )
                {
                    s += Str::Length( &tag[ 4 ] ) + 1;
                    if( *s == ' ' )
                        s++;
                    Str::CopyWord( tag, s, '$', false );
                }
                else
                {
                    Str::Copy( tag, "<lexem not found>" );
                }
            }
            // Msg text
            else if( Str::Length( tag ) > 4 && tag[ 0 ] == 'm' && tag[ 1 ] == 's' && tag[ 2 ] == 'g' && tag[ 3 ] == ' ' )
            {
                char msg_type_name[ 64 ];
                uint str_num;
                Str::EraseChars( &tag[ 4 ], '(' );
                Str::EraseChars( &tag[ 4 ], ')' );
                if( sscanf( &tag[ 4 ], "%s %u", msg_type_name, &str_num ) != 2 )
                {
                    Str::Copy( tag, "<msg tag parse fail>" );
                }
                else
                {
                    int msg_type = FOMsg::GetMsgType( msg_type_name );
                    if( msg_type < 0 || msg_type >= TEXTMSG_COUNT )
                        Str::Copy( tag, "<msg tag, unknown type>" );
                    else if( !CurLang.Msg[ msg_type ].Count( str_num ) )
                        Str::Copy( tag, Str::FormatBuf( "<msg tag, string %u not found>", str_num ) );
                    else
                        Str::Copy( tag, CurLang.Msg[ msg_type ].GetStr( str_num ) );
                }
            }
            // Script
            else if( Str::Length( tag ) > 7 && tag[ 0 ] == 's' && tag[ 1 ] == 'c' && tag[ 2 ] == 'r' && tag[ 3 ] == 'i' && tag[ 4 ] == 'p' && tag[ 5 ] == 't' && tag[ 6 ] == ' ' )
            {
                char func_name[ MAX_FOTEXT ];
                Str::CopyWord( func_name, &tag[ 7 ], '$', false );
                uint bind_id = Script::BindByScriptName( func_name, "string@ %s(string&)", true );
                Str::Copy( tag, "<script function not found>" );
                if( bind_id && Script::PrepareContext( bind_id, _FUNC_, "Game" ) )
                {
                    ScriptString* script_lexems = ScriptString::Create( lexems ? lexems : "" );
                    Script::SetArgObject( script_lexems );
                    if( Script::RunPrepared() )
                    {
                        ScriptString* result = (ScriptString*) Script::GetReturnedObject();
                        if( result )
                        {
                            Str::Copy( tag, result->c_str() );
                            result->Release();
                        }
                    }
                    script_lexems->Release();
                }
            }
            // Error
            else
            {
                Str::Copy( tag, "<error>" );
            }

            Str::Insert( str + i, tag );
        }
            continue;
        default:
            break;
        }

        ++i;
    }

    dialogs.push_back( str );
    Str::Copy( text, dialogs[ Random( 0, (uint) dialogs.size() - 1 ) ] );
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

bool FOClient::IsLMenu()
{
    return LMenuMode != LMENU_OFF && LMenuActive;
}

void FOClient::LMenuTryActivate()
{
    LMenuTryActivated = true;
    LMenuStartTime = Timer::FastTick();
}

void FOClient::LMenuStayOff()
{
    if( TargetSmth.IsItem() )
    {
        ItemHex* item = GetItem( TargetSmth.GetId() );
        if( item && item->IsDrawContour() && item->SprDrawValid )
            item->SprDraw->SetContour( 0 );
    }
    if( TargetSmth.IsCritter() )
    {
        CritterCl* cr = GetCritter( TargetSmth.GetId() );
        if( cr && cr->SprDrawValid && cr->SprDraw->ContourType == CONTOUR_YELLOW )
            cr->SprDraw->SetContour( 0 );
    }
    TargetSmth.Clear();
    LMenuCurNodes = nullptr;
}

void FOClient::LMenuTryCreate()
{
    // Check valid current state
    if( GameOpt.DisableLMenu || !Chosen || IsScroll() || !IsCurInWindow() || !IsCurMode( CUR_DEFAULT ) || GameOpt.HideCursor )
    {
        LMenuSet( LMENU_OFF );
        return;
    }

    // Already created and showed
    if( IsLMenu() )
        return;

    // Add name of item
    if( !LMenuTryActivated && Timer::FastTick() - GameMouseStay >= LMENU_SHOW_TIME )
    {
        LMenuCollect();
        if( !LMenuCurNodes )
            return;

        static SmthSelected last_look;
        if( TargetSmth.IsItem() && TargetSmth != last_look )
        {
            ItemHex* item = GetItem( TargetSmth.GetId() );
            if( item )
                AddMess( FOMB_VIEW, FmtItemLook( item, ITEM_LOOK_ONLY_NAME ) );
            last_look = TargetSmth;
        }
        if( TargetSmth.IsCritter() && TargetSmth != last_look )
        {
            CritterCl* cr = GetCritter( TargetSmth.GetId() );
            if( cr )
                AddMess( FOMB_VIEW, FmtCritLook( cr, CRITTER_LOOK_SHORT ) );
            last_look = TargetSmth;
        }
        if( TargetSmth.IsContItem() && TargetSmth != last_look )
        {
            Item* cont_item = GetTargetContItem();
            if( cont_item )
                AddMess( FOMB_VIEW, FmtItemLook( cont_item, ITEM_LOOK_ONLY_NAME ) );
            last_look = TargetSmth;
        }

        if( TargetSmth.IsItem() )
        {
            ItemHex* item = GetItem( TargetSmth.GetId() );
            if( item && item->IsDrawContour() && item->SprDrawValid )
                item->SprDraw->SetContour( CONTOUR_YELLOW );
        }
        if( TargetSmth.IsCritter() )
        {
            CritterCl* cr = GetCritter( TargetSmth.GetId() );
            if( cr && cr->IsDead() && cr->SprDrawValid )
                cr->SprDraw->SetContour( CONTOUR_YELLOW );
        }
    }
    // Show LMenu
    else if( LMenuTryActivated && Timer::FastTick() - LMenuStartTime >= LMENU_SHOW_TIME )
    {
        LMenuCollect();
        if( !LMenuCurNodes )
            return;

        SndMngr.PlaySound( "IACCUXX1" );
        int height = (int) LMenuCurNodes->size() * LMenuNodeHeight;
        if( LMenuY + height > GameOpt.ScreenHeight )
            LMenuY -= LMenuY + height - GameOpt.ScreenHeight;
        if( LMenuX + LMenuNodeHeight > GameOpt.ScreenWidth )
            LMenuX -= LMenuX + LMenuNodeHeight - GameOpt.ScreenWidth;
        SetCurPos( LMenuX, LMenuY );
        LMenuCurNode = 0;
        LMenuTryActivated = false;
        LMenuActive = true;
    }
}

void FOClient::LMenuCollect()
{
    LMenuStayOff();
    GameMouseStay = Timer::FastTick();

    LMenuX = GameOpt.MouseX + LMenuOX;
    LMenuY = GameOpt.MouseY + LMenuOY;
    LMenuRestoreCurX = GameOpt.MouseX;
    LMenuRestoreCurY = GameOpt.MouseY;
    LMenuHeightOffset = 0;

    if( Script::PrepareContext( ClientFunctions.GetContItem, _FUNC_, "Game" ) )
    {
        uint item_id = 0;
        bool is_enemy = 0;
        Script::SetArgAddress( &item_id );
        Script::SetArgAddress( &is_enemy );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
        {
            if( item_id )
            {
                TargetSmth.SetContItem( item_id, is_enemy ? 3 : 2 );
                LMenuSet( LMENU_ITEM_INV );
            }
            if( !LMenuCurNodes )
                LMenuSet( LMENU_OFF );
            return;
        }
    }

    int screen = GetActiveScreen();
    if( !screen )
    {
        switch( GetMainScreen() )
        {
        case SCREEN_GAME:
        {
            if( IsCurInInterface( GameOpt.MouseX, GameOpt.MouseY ) )
                break;

            CritterCl* cr;
            ItemHex*   item;
            HexMngr.GetSmthPixel( GameOpt.MouseX, GameOpt.MouseY, item, cr );

            if( cr )
            {
                TargetSmth.SetCritter( cr->GetId() );
                if( cr->IsPlayer() )
                    LMenuSet( LMENU_PLAYER );
                else if( cr->IsNpc() )
                    LMenuSet( LMENU_NPC );
            }
            else if( item )
            {
                TargetSmth.SetItem( item->GetId() );
                LMenuSet( LMENU_ITEM );
            }
        }
        break;
        case SCREEN_GLOBAL_MAP:
        {
            int pos = 0;
            for( auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end(); it++, pos++ )
            {
                CritterCl* cr = it->second;
                if( !IsCurInRect( Rect( GmapWName, GmapWNameStepX * pos, GmapWNameStepY * pos ) ) )
                    continue;
                TargetSmth.SetCritter( cr->GetId() );
                LMenuSet( LMENU_GMAP_CRIT );
                break;
            }
        }
        break;
        }
    }

    if( !LMenuCurNodes )
        LMenuSet( LMENU_OFF );
}

void FOClient::LMenuSet( uchar set_lmenu )
{
    if( IsLMenu() )
        SetCurPos( LMenuRestoreCurX, LMenuRestoreCurY );

    LMenuMode = set_lmenu;
    LMenuCurNodes = nullptr;

    switch( LMenuMode )
    {
    case LMENU_OFF:
    {
        LMenuActive = false;
        LMenuStayOff();
        LMenuTryActivated = false;
        GameMouseStay = Timer::FastTick();
        LMenuStartTime = Timer::FastTick();
    }
    break;
    case LMENU_PLAYER:
    case LMENU_NPC:
    {
        if( !TargetSmth.IsCritter() )
            break;
        CritterCl* cr = GetCritter( TargetSmth.GetId() );
        if( !cr || !Chosen )
            break;

        LMenuNodes.clear();

        if( Chosen->IsLife() )
        {
            if( LMenuMode == LMENU_NPC )
            {
                // Npc
                if( cr->IsCanTalk() )
                    LMenuNodes.push_back( LMENU_NODE_TALK );
                if( cr->IsDead() && !cr->GetIsNoLoot() )
                    LMenuNodes.push_back( LMENU_NODE_USE );
                LMenuNodes.push_back( LMENU_NODE_LOOK );
                if( cr->IsLife() && !cr->GetIsNoPush() )
                    LMenuNodes.push_back( LMENU_NODE_PUSH );
                LMenuNodes.push_back( LMENU_NODE_BAG );
                LMenuNodes.push_back( LMENU_NODE_SKILL );
            }
            else
            {
                if( cr != Chosen )
                {
                    // Player
                    if( cr->IsDead() && !cr->GetIsNoLoot() )
                        LMenuNodes.push_back( LMENU_NODE_USE );
                    LMenuNodes.push_back( LMENU_NODE_LOOK );
                    if( cr->IsLife() && !cr->GetIsNoPush() )
                        LMenuNodes.push_back( LMENU_NODE_PUSH );
                    LMenuNodes.push_back( LMENU_NODE_BAG );
                    LMenuNodes.push_back( LMENU_NODE_SKILL );
                    LMenuNodes.push_back( LMENU_NODE_GMFOLLOW );
                    if( cr->IsLife() && cr->IsPlayer() && cr->IsOnline() && CheckDist( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY(), BARTER_DIST ) )
                    {
                        LMenuNodes.push_back( LMENU_NODE_BARTER_OPEN );
                        LMenuNodes.push_back( LMENU_NODE_BARTER_HIDE );
                    }
                    if( !Chosen->GetKarmaVoting() )
                    {
                        LMenuNodes.push_back( LMENU_NODE_VOTE_UP );
                        LMenuNodes.push_back( LMENU_NODE_VOTE_DOWN );
                    }
                }
                else
                {
                    // Self
                    LMenuNodes.push_back( LMENU_NODE_ROTATE );
                    LMenuNodes.push_back( LMENU_NODE_LOOK );
                    LMenuNodes.push_back( LMENU_NODE_BAG );
                    LMenuNodes.push_back( LMENU_NODE_SKILL );
                    LMenuNodes.push_back( LMENU_NODE_PICK_ITEM );
                }
            }
        }
        else
        {
            LMenuNodes.push_back( LMENU_NODE_LOOK );
            if( LMenuMode != LMENU_NPC && cr != Chosen && !Chosen->GetKarmaVoting() )
            {
                LMenuNodes.push_back( LMENU_NODE_VOTE_UP );
                LMenuNodes.push_back( LMENU_NODE_VOTE_DOWN );
            }
        }

        LMenuNodes.push_back( LMENU_NODE_BREAK );
        LMenuCurNodes = &LMenuNodes;
    }
    break;
    case LMENU_ITEM:
    {
        if( !TargetSmth.IsItem() )
            break;
        ItemHex* item = GetItem( TargetSmth.GetId() );
        if( !item )
            break;

        LMenuNodes.clear();

        if( item->IsUsable() )
            LMenuNodes.push_back( LMENU_NODE_PICK );
        LMenuNodes.push_back( LMENU_NODE_LOOK );
        if( item->IsCanUseSkill() )
            LMenuNodes.push_back( LMENU_NODE_BAG );
        if( item->IsCanUseSkill() )
            LMenuNodes.push_back( LMENU_NODE_SKILL );
        LMenuNodes.push_back( LMENU_NODE_BREAK );

        LMenuCurNodes = &LMenuNodes;
    }
    break;
    case LMENU_ITEM_INV:
    {
        Item* cont_item = GetTargetContItem();
        if( !cont_item || !Chosen )
            break;
        Item* inv_item = Chosen->GetItem( cont_item->GetId() );

        LMenuNodes.clear();
        LMenuNodes.push_back( LMENU_NODE_LOOK );

        if( inv_item )
        {
            if( inv_item->IsWeapon() && inv_item->WeapGetMaxAmmoCount() && !inv_item->WeapIsEmpty() && inv_item->WeapGetAmmoCaliber() )
                LMenuNodes.push_back( LMENU_NODE_UNLOAD );
            if( inv_item->GetIsCanUse() || inv_item->GetIsCanUseOnSmth() )
                LMenuNodes.push_back( LMENU_NODE_USE );
            LMenuNodes.push_back( LMENU_NODE_SKILL );
            if( inv_item->GetCritSlot() == SLOT_INV && Chosen->IsCanSortItems() )
            {
                LMenuNodes.push_back( LMENU_NODE_SORT_UP );
                LMenuNodes.push_back( LMENU_NODE_SORT_DOWN );
            }
            LMenuNodes.push_back( LMENU_NODE_DROP );
        }
        else
        {
            // Items in another containers
        }

        LMenuNodes.push_back( LMENU_NODE_BREAK );
        LMenuCurNodes = &LMenuNodes;
    }
    break;
    case LMENU_GMAP_CRIT:
    {
        if( !TargetSmth.IsCritter() )
            break;
        CritterCl* cr = GetCritter( TargetSmth.GetId() );
        if( !cr || !Chosen )
            break;

        LMenuNodes.clear();
        LMenuNodes.push_back( LMENU_NODE_LOOK );
        LMenuNodes.push_back( LMENU_NODE_SKILL );
        LMenuNodes.push_back( LMENU_NODE_BAG );

        if( cr->GetId() != Chosen->GetId() )             // Another critters
        {
            if( cr->IsPlayer() && cr->IsLife() )
            {
                // Kick and give rule
                if( Chosen->IsGmapRule() )
                {
                    LMenuNodes.push_back( LMENU_NODE_GMAP_KICK );
                    if( !cr->IsOffline() )
                        LMenuNodes.push_back( LMENU_NODE_GMAP_RULE );
                }

                // Barter
                if( !cr->IsOffline() )
                {
                    LMenuNodes.push_back( LMENU_NODE_BARTER_OPEN );
                    LMenuNodes.push_back( LMENU_NODE_BARTER_HIDE );
                }
            }
        }
        else                 // Self
        {
            // Kick self
            if( !Chosen->IsGmapRule() && HexMngr.GetCritters().size() > 1 )
                LMenuNodes.push_back( LMENU_NODE_GMAP_KICK );
        }

        if( cr->IsPlayer() && cr->GetId() != Chosen->GetId() && !Chosen->GetKarmaVoting() )
        {
            LMenuNodes.push_back( LMENU_NODE_VOTE_UP );
            LMenuNodes.push_back( LMENU_NODE_VOTE_DOWN );
        }

        LMenuNodes.push_back( LMENU_NODE_BREAK );
        LMenuCurNodes = &LMenuNodes;
    }
    break;
    default:
        break;
    }

    if( LMenuCurNodes )
    {
        CritterCl* cr = nullptr;
        ItemHex*   item = nullptr;
        Item*      cont_item = nullptr;
        if( TargetSmth.IsCritter() )
            cr = GetCritter( TargetSmth.GetId() );
        else if( TargetSmth.IsItem() )
            item = GetItem( TargetSmth.GetId() );
        else if( TargetSmth.IsContItem() )
            cont_item = GetTargetContItem();

        if( Script::PrepareContext( ClientFunctions.LMenuCollectNodes, _FUNC_, "Game" ) )
        {
            ScriptArray* arr = Script::CreateArray( "int[]" );
            Script::AppendVectorToArray( LMenuNodes, arr );
            Script::SetArgEntityOK( cr );
            Script::SetArgEntityOK( item ? item : cont_item );
            Script::SetArgObject( arr );
            Script::RunPrepared();
            Script::AssignScriptArrayInVector( LMenuNodes, arr );
            arr->Release();
        }
    }
}

void FOClient::LMenuDraw()
{
    if( !LMenuCurNodes )
        return;

    for( uint i = 0, j = (uint) LMenuCurNodes->size(); i < j; i++ )
    {
        int num_node = ( *LMenuCurNodes )[ i ];

// ==================================================================================
        #define LMENU_DRAW_CASE( node, pic_up, pic_down )                              \
        case node:                                                                     \
            if( i == (uint) LMenuCurNode && IsLMenu() )                                \
                SprMngr.DrawSprite( pic_down, LMenuX, LMenuY + LMenuNodeHeight * i );  \
            else                                                                       \
                SprMngr.DrawSprite( pic_up, LMenuX, LMenuY + LMenuNodeHeight * i );    \
            break
// ==================================================================================

        switch( num_node )
        {
            LMENU_DRAW_CASE( LMENU_NODE_LOOK, LmenuPLookOff, LmenuPLookOn );
            LMENU_DRAW_CASE( LMENU_NODE_TALK, LmenuPTalkOff, LmenuPTalkOn );
            LMENU_DRAW_CASE( LMENU_NODE_BREAK, LmenuPBreakOff, LmenuPBreakOn );
            LMENU_DRAW_CASE( LMENU_NODE_PICK, LmenuPUseOff, LmenuPUseOn );
            LMENU_DRAW_CASE( LMENU_NODE_GMFOLLOW, LmenuPGMFollowOff, LmenuPGMFollowOn );
            LMENU_DRAW_CASE( LMENU_NODE_ROTATE, LmenuPRotateOff, LmenuPRotateOn );
            LMENU_DRAW_CASE( LMENU_NODE_DROP, LmenuPDropOff, LmenuPDropOn );
            LMENU_DRAW_CASE( LMENU_NODE_UNLOAD, LmenuPUnloadOff, LmenuPUnloadOn );
            LMENU_DRAW_CASE( LMENU_NODE_USE, LmenuPUseOff, LmenuPUseOn );
            LMENU_DRAW_CASE( LMENU_NODE_SORT_UP, LmenuPSortUpOff, LmenuPSortUpOn );
            LMENU_DRAW_CASE( LMENU_NODE_SORT_DOWN, LmenuPSortDownOff, LmenuPSortDownOn );
            LMENU_DRAW_CASE( LMENU_NODE_PICK_ITEM, LmenuPPickItemOff, LmenuPPickItemOn );
            LMENU_DRAW_CASE( LMENU_NODE_PUSH, LmenuPPushOff, LmenuPPushOn );
            LMENU_DRAW_CASE( LMENU_NODE_BAG, LmenuPBagOff, LmenuPBagOn );
            LMENU_DRAW_CASE( LMENU_NODE_SKILL, LmenuPSkillOff, LmenuPSkillOn );
            LMENU_DRAW_CASE( LMENU_NODE_BARTER_OPEN, LmenuPBarterOpenOff, LmenuPBarterOpenOn );
            LMENU_DRAW_CASE( LMENU_NODE_BARTER_HIDE, LmenuPBarterHideOff, LmenuPBarterHideOn );
            LMENU_DRAW_CASE( LMENU_NODE_GMAP_KICK, LmenuPGmapKickOff, LmenuPGmapKickOn );
            LMENU_DRAW_CASE( LMENU_NODE_GMAP_RULE, LmenuPGmapRuleOff, LmenuPGmapRuleOn );
            LMENU_DRAW_CASE( LMENU_NODE_VOTE_UP, LmenuPVoteUpOff, LmenuPVoteUpOn );
            LMENU_DRAW_CASE( LMENU_NODE_VOTE_DOWN, LmenuPVoteDownOff, LmenuPVoteDownOn );
        default:
            WriteLogF( _FUNC_, " - Unknown node %d.\n", num_node );
            break;
        }
        if( !IsLMenu() )
            break;
    }
}

void FOClient::LMenuMouseMove()
{
    if( IsLMenu() )
    {
        LMenuHeightOffset += GameOpt.MouseY - LMenuRestoreCurY;
        SetCurPos( LMenuRestoreCurX, LMenuRestoreCurY );

        LMenuCurNode = LMenuHeightOffset / LMenuNodeHeight;
        if( LMenuCurNode < 0 )
            LMenuCurNode = 0;
        if( LMenuCurNode > (int) LMenuCurNodes->size() - 1 )
            LMenuCurNode = (int) LMenuCurNodes->size() - 1;
    }
    else
    {
        GameMouseStay = Timer::FastTick();
        LMenuStartTime = Timer::FastTick();
        LMenuStayOff();
    }
}

void FOClient::LMenuMouseUp()
{
    if( !IsLMenu() && !LMenuTryActivated )
        return;
    LMenuTryActivated = false;
    if( !LMenuCurNodes )
        LMenuCollect();
    if( !Chosen || !LMenuCurNodes )
        return;
    if( !IsLMenu() )
        LMenuCurNode = 0;

    auto it_l = LMenuCurNodes->begin();
    it_l += LMenuCurNode;

    CritterCl* cr = nullptr;
    ItemHex*   item = nullptr;
    Item*      cont_item = nullptr;
    if( TargetSmth.IsCritter() )
        cr = GetCritter( TargetSmth.GetId() );
    else if( TargetSmth.IsItem() )
        item = GetItem( TargetSmth.GetId() );
    else if( TargetSmth.IsContItem() )
        cont_item = GetTargetContItem();

    if( Script::PrepareContext( ClientFunctions.LMenuNodeSelect, _FUNC_, "Game" ) )
    {
        Script::SetArgUInt( *it_l );
        Script::SetArgEntityOK( cr );
        Script::SetArgEntityOK( item ? item : cont_item );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
        {
            LMenuSet( LMENU_OFF );
            return;
        }
    }

    switch( LMenuMode )
    {
    case LMENU_PLAYER:
    {
        if( !cr )
            break;

        switch( *it_l )
        {
        case LMENU_NODE_LOOK:
            AddMess( FOMB_VIEW, FmtCritLook( cr, CRITTER_LOOK_FULL ) );
            break;
        case LMENU_NODE_GMFOLLOW:
            Net_SendRuleGlobal( GM_CMD_FOLLOW_CRIT, cr->GetId() );
            break;
        case LMENU_NODE_USE:
            SetAction( CHOSEN_PICK_CRIT, cr->GetId(), 0 );
            break;
        case LMENU_NODE_ROTATE:
            SetAction( CHOSEN_DIR, 0 );
            break;
        case LMENU_NODE_PICK_ITEM:
            TryPickItemOnGround();
            break;
        case LMENU_NODE_PUSH:
            SetAction( CHOSEN_PICK_CRIT, cr->GetId(), 1 );
            break;
        case LMENU_NODE_BAG:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = ( TargetSmth.IsCritter() ? TargetSmth.GetId() : 0 );
            uint              item_id = ( TargetSmth.IsItem() ? TargetSmth.GetId() : 0 );
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            ShowScreen( SCREEN__USE, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_SKILL:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = TargetSmth.GetId();
            uint              item_id = 0;
            bool              is_inventory = false;
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            dict->Set( "IsInventory", &is_inventory, asTYPEID_BOOL );
            ShowScreen( SCREEN__SKILLBOX, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_BARTER_OPEN:
            Net_SendPlayersBarter( BARTER_TRY, cr->GetId(), false );
            break;
        case LMENU_NODE_BARTER_HIDE:
            Net_SendPlayersBarter( BARTER_TRY, cr->GetId(), true );
            break;
        case LMENU_NODE_VOTE_UP:
            Net_SendKarmaVoting( cr->GetId(), true );
            break;
        case LMENU_NODE_VOTE_DOWN:
            Net_SendKarmaVoting( cr->GetId(), false );
            break;
        case LMENU_NODE_BREAK:
            break;
        default:
            break;
        }
    }
    break;
    case LMENU_NPC:
    {
        if( !cr )
            break;

        switch( *it_l )
        {
        case LMENU_NODE_LOOK:
            AddMess( FOMB_VIEW, FmtCritLook( cr, CRITTER_LOOK_FULL ) );
            break;
        case LMENU_NODE_TALK:
            SetAction( CHOSEN_TALK_NPC, cr->GetId() );
            break;
        case LMENU_NODE_GMFOLLOW:
            Net_SendRuleGlobal( GM_CMD_FOLLOW_CRIT, cr->GetId() );
            break;
        case LMENU_NODE_USE:
            SetAction( CHOSEN_PICK_CRIT, cr->GetId(), 0 );
            break;
        case LMENU_NODE_PUSH:
            SetAction( CHOSEN_PICK_CRIT, cr->GetId(), 1 );
            break;
        case LMENU_NODE_BAG:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = ( TargetSmth.IsCritter() ? TargetSmth.GetId() : 0 );
            uint              item_id = ( TargetSmth.IsItem() ? TargetSmth.GetId() : 0 );
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            ShowScreen( SCREEN__USE, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_SKILL:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = TargetSmth.GetId();
            uint              item_id = 0;
            bool              is_inventory = false;
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            dict->Set( "IsInventory", &is_inventory, asTYPEID_BOOL );
            ShowScreen( SCREEN__SKILLBOX, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_BREAK:
            break;
        default:
            break;
        }
    }
    break;
    case LMENU_ITEM:
    {
        if( !item )
            break;

        switch( *it_l )
        {
        case LMENU_NODE_LOOK:
            AddMess( FOMB_VIEW, FmtItemLook( item, ITEM_LOOK_MAP ) );
            break;
        case LMENU_NODE_PICK:
            SetAction( CHOSEN_PICK_ITEM, item->GetProtoId(), item->HexX, item->HexY );
            break;
        case LMENU_NODE_BAG:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = ( TargetSmth.IsCritter() ? TargetSmth.GetId() : 0 );
            uint              item_id = ( TargetSmth.IsItem() ? TargetSmth.GetId() : 0 );
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            ShowScreen( SCREEN__USE, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_SKILL:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = 0;
            uint              item_id = TargetSmth.GetId();
            bool              is_inventory = false;
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            dict->Set( "IsInventory", &is_inventory, asTYPEID_BOOL );
            ShowScreen( SCREEN__SKILLBOX, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_BREAK:
            break;
        default:
            break;
        }
    }
    break;
    case LMENU_ITEM_INV:
    {
        if( !cont_item )
            break;
        Item* inv_item = Chosen->GetItem( cont_item->GetId() );

        switch( *it_l )
        {
        case LMENU_NODE_LOOK:
            if( GetActiveScreen() == SCREEN__PICKUP )
                AddMess( FOMB_VIEW, FmtItemLook( cont_item, ITEM_LOOK_DEFAULT ) );
            break;
        case LMENU_NODE_DROP:
            if( !cont_item )
                break;
            if( !Chosen->IsLife() || !Chosen->IsFree() )
                break;
            if( cont_item->GetStackable() && cont_item->GetCount() > 1 )
                SplitStart( cont_item->GetId(), ITEMS_CHOSEN_ALL );
            else
                AddActionBack( CHOSEN_MOVE_ITEM, cont_item->GetId(), cont_item->GetCount(), SLOT_GROUND );
            break;
        case LMENU_NODE_UNLOAD:
            if( !inv_item )
                break;
            SetAction( CHOSEN_USE_ITEM, cont_item->GetId(), 0, TARGET_SELF_ITEM, -1, USE_RELOAD );
            break;
        case LMENU_NODE_USE:
            if( !inv_item )
                break;
            if( inv_item->GetIsHasTimer() )
            {
                ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
                uint              item_id = inv_item->GetId();
                dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
                ShowScreen( SCREEN__TIMER, dict );
                dict->Release();
            }
            else
            {
                SetAction( CHOSEN_USE_ITEM, inv_item->GetId(), 0, TARGET_SELF, 0, USE_USE );
            }
            break;
        case LMENU_NODE_SKILL:
        {
            if( !inv_item )
                break;
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = 0;
            uint              item_id = inv_item->GetId();
            bool              is_inventory = true;
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            dict->Set( "IsInventory", &is_inventory, asTYPEID_BOOL );
            ShowScreen( SCREEN__SKILLBOX, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_SORT_UP:
        {
            if( !inv_item )
                break;
            Item* item = Chosen->GetItemLowSortValue();
            if( !item || inv_item == item )
                break;
            if( inv_item->GetSortValue() < item->GetSortValue() )
                break;
            Item* old_inv_item = inv_item->Clone();
            inv_item->SetSortValue( item->GetSortValue() - 1 );
            Item::SortItems( Chosen->InvItems );
            OnItemInvChanged( old_inv_item, inv_item );
            CollectContItems();
        }
        break;
        case LMENU_NODE_SORT_DOWN:
        {
            if( !inv_item )
                break;
            Item* item = Chosen->GetItemHighSortValue();
            if( !item || inv_item == item )
                break;
            if( inv_item->GetSortValue() > item->GetSortValue() )
                break;
            Item* old_inv_item = inv_item->Clone();
            inv_item->SetSortValue( item->GetSortValue() + 1 );
            Item::SortItems( Chosen->InvItems );
            OnItemInvChanged( old_inv_item, inv_item );
            CollectContItems();
        }
        break;
        case LMENU_NODE_BREAK:
            break;
        default:
            break;
        }
    }
    break;
    case LMENU_GMAP_CRIT:
    {
        if( !cr )
            break;

        switch( *it_l )
        {
        case LMENU_NODE_LOOK:
            AddMess( FOMB_VIEW, FmtCritLook( cr, CRITTER_LOOK_FULL ) );
            break;
        case LMENU_NODE_SKILL:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = TargetSmth.GetId();
            uint              item_id = 0;
            bool              is_inventory = false;
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            dict->Set( "IsInventory", &is_inventory, asTYPEID_BOOL );
            ShowScreen( SCREEN__SKILLBOX, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_BAG:
        {
            ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
            uint              critter_id = ( TargetSmth.IsCritter() ? TargetSmth.GetId() : 0 );
            uint              item_id = ( TargetSmth.IsItem() ? TargetSmth.GetId() : 0 );
            dict->Set( "TargetCritterId", &critter_id, asTYPEID_UINT32 );
            dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
            ShowScreen( SCREEN__USE, dict );
            dict->Release();
        }
        break;
        case LMENU_NODE_GMAP_KICK:
            Net_SendRuleGlobal( GM_CMD_KICKCRIT, cr->GetId() );
            break;
        case LMENU_NODE_GMAP_RULE:
            Net_SendRuleGlobal( GM_CMD_GIVE_RULE, cr->GetId() );
            break;
        case LMENU_NODE_BARTER_OPEN:
            Net_SendPlayersBarter( BARTER_TRY, cr->GetId(), false );
            break;
        case LMENU_NODE_BARTER_HIDE:
            Net_SendPlayersBarter( BARTER_TRY, cr->GetId(), true );
            break;
        case LMENU_NODE_VOTE_UP:
            Net_SendKarmaVoting( cr->GetId(), true );
            break;
        case LMENU_NODE_VOTE_DOWN:
            Net_SendKarmaVoting( cr->GetId(), false );
            break;
        case LMENU_NODE_BREAK:
            break;
        default:
            break;
        }
    }
    break;
    default:
        break;
    }

    LMenuSet( LMENU_OFF );
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::ShowMainScreen( int new_screen, ScriptDictionary* params /* = NULL */ )
{
    while( GetActiveScreen() != SCREEN_NONE )
        ShowScreen( SCREEN_NONE );

    int prev_main_screen = ScreenModeMain;
    if( ScreenModeMain )
        RunScreenScript( false, ScreenModeMain, nullptr );
    ScreenModeMain = new_screen;
    RunScreenScript( true, new_screen, params );

    switch( GetMainScreen() )
    {
    case SCREEN_LOGIN:
        ScreenFadeOut();
        break;
    case SCREEN_REGISTRATION:
        ScreenFadeOut();
        break;
    case SCREEN_GAME:
        if( Singleplayer )
            SingleplayerData.Pause = false;
        break;
    case SCREEN_GLOBAL_MAP:
        GmapMouseMove();
        if( Singleplayer )
            SingleplayerData.Pause = false;
        break;
    case SCREEN_WAIT:
        if( prev_main_screen != SCREEN_WAIT )
        {
            ScreenEffects.clear();
            WaitPic = ResMngr.GetRandomSplash();
        }
        break;
    default:
        break;
    }
}

int FOClient::GetActiveScreen( IntVec** screens /* = NULL */ )
{
    static IntVec active_screens;
    active_screens.clear();

    if( Script::PrepareContext( ClientFunctions.GetActiveScreens, _FUNC_, "Game" ) )
    {
        ScriptArray* arr = Script::CreateArray( "int[]" );
        if( arr )
        {
            Script::SetArgObject( arr );
            if( Script::RunPrepared() )
                Script::AssignScriptArrayInVector( active_screens, arr );
            arr->Release();
        }
    }

    if( screens )
        *screens = &active_screens;
    int active = ( active_screens.size() ? active_screens.back() : SCREEN_NONE );
    if( active >= SCREEN_LOGIN && active <= SCREEN_WAIT )
        active = SCREEN_NONE;
    return active;
}

bool FOClient::IsScreenPresent( int screen )
{
    IntVec* active_screens;
    GetActiveScreen( &active_screens );
    return std::find( active_screens->begin(), active_screens->end(), screen ) != active_screens->end();
}

void FOClient::ShowScreen( int screen, ScriptDictionary* params /* = NULL */ )
{
    SmthSelected smth = TargetSmth;
    IfaceHold = IFACE_NONE;
    DropScroll();

    if( screen == SCREEN_NONE )
    {
        HideScreen( screen );
        return;
    }

    switch( screen )
    {
    case SCREEN__PICKUP:
        CollectContItems();
        break;
    case SCREEN__MINI_MAP:
        LmapPrepareMap();
        break;
    case SCREEN__BARTER:
        break;
    case SCREEN__FIX_BOY:
        FixMode = FIX_MODE_LIST;
        FixCurCraft = -1;
        FixGenerate( FIX_MODE_LIST );
        FixMouseMove();
        break;
    case SCREEN__GM_TOWN:
    {
        GmapTownTextPos.clear();
        GmapTownText.clear();

        GmapTownPic = nullptr;
        GmapTownPicPos[ 0 ] = 0;
        GmapTownPicPos[ 1 ] = 0;
        GmapTownPicPos[ 2 ] = GameOpt.ScreenWidth;
        GmapTownPicPos[ 3 ] = GameOpt.ScreenHeight;
        int        pic_offsx = 0;
        int        pic_offsy = 0;
        hash       loc_pid = GmapTownLoc.LocPid;

        AnyFrames* anim = ResMngr.GetIfaceAnim( Str::GetHash( MsgLocations->GetStr( STR_LOC_PIC( loc_pid ) ) ) );
        if( !anim )
        {
            RunScreenScript( true, screen, params );
            ShowScreen( SCREEN_NONE );
            return;
        }
        SpriteInfo* si = SprMngr.GetSpriteInfo( anim->GetCurSprId() );

        GmapTownPic = anim;
        pic_offsx = ( GameOpt.ScreenWidth - si->Width ) / 2;
        pic_offsy = ( GameOpt.ScreenHeight - si->Height ) / 2;
        GmapTownPicPos[ 0 ] = pic_offsx;
        GmapTownPicPos[ 1 ] = pic_offsy;
        GmapTownPicPos[ 2 ] = pic_offsx + si->Width;
        GmapTownPicPos[ 3 ] = pic_offsy + si->Height;

        uint entrances = GmapTownLoc.Entrances;
        if( !entrances )
            break;

        for( uint i = 0; i < entrances; i++ )
        {
            int x = MsgLocations->GetInt( STR_LOC_ENTRANCE_PICX( loc_pid, i ) ) + pic_offsx;
            int y = MsgLocations->GetInt( STR_LOC_ENTRANCE_PICY( loc_pid, i ) ) + pic_offsy;

            GmapTownTextPos.push_back( Rect( x, y, GameOpt.ScreenWidth, GameOpt.ScreenHeight ) );
            GmapTownText.push_back( string( MsgLocations->GetStr( STR_LOC_ENTRANCE_NAME( loc_pid, i ) ) ) );
        }

        if( GmapTownLoc.LocId != GmapShowEntrancesLocId || Timer::FastTick() >= GmapNextShowEntrancesTick )
        {
            Net_SendRuleGlobal( GM_CMD_ENTRANCES, GmapTownLoc.LocId );
            GmapShowEntrancesLocId = GmapTownLoc.LocId;
            GmapNextShowEntrancesTick = Timer::FastTick() + GM_ENTRANCES_SEND_TIME;
        }
    }
    break;
    case SCREEN__SAVE_LOAD:
        SaveLoadCollect();
        if( SaveLoadLoginScreen )
            ScreenFadeOut();
        break;
    default:
        break;
    }

    RunScreenScript( true, screen, params );

    TargetSmth = smth;
}

void FOClient::HideScreen( int screen )
{
    if( screen == SCREEN_NONE )
        screen = GetActiveScreen();
    if( screen == SCREEN_NONE )
        return;

    if( Singleplayer )
    {
        if( SingleplayerData.Pause && screen == SCREEN__MENU_OPTION )
            SingleplayerData.Pause = false;

        if( screen == SCREEN__SAVE_LOAD )
        {
            if( SaveLoadLoginScreen )
                ScreenFadeOut();
            SaveLoadDataSlots.clear();
        }
    }

    RunScreenScript( false, screen, nullptr );
}

void FOClient::RunScreenScript( bool show, int screen, ScriptDictionary* params )
{
    if( Script::PrepareContext( ClientFunctions.ScreenChange, _FUNC_, "Game" ) )
    {
        Script::SetArgBool( show );
        Script::SetArgUInt( screen );
        Script::SetArgObject( params );
        Script::RunPrepared();
    }
}

void FOClient::SetCurMode( int new_cur )
{
    if( new_cur == CurMode )
        return;
    if( CurModeLast != CurMode )
        CurModeLast = CurMode;
    CurMode = new_cur;

    LMenuSet( LMENU_OFF );

    if( CurMode == CUR_USE_WEAPON && IsMainScreen( SCREEN_GAME ) && Chosen && Chosen->ItemSlotMain->IsWeapon() )
        HexMngr.SetCrittersContour( CONTOUR_CUSTOM );
    else if( CurModeLast == CUR_USE_WEAPON )
        HexMngr.SetCrittersContour( 0 );

    if( CurMode == CUR_MOVE )
        HexMngr.SetCursorVisible( true );
    else if( CurModeLast == CUR_MOVE )
        HexMngr.SetCursorVisible( false );
}

void FOClient::SetCurCastling( int cur1, int cur2 )
{
    if( CurMode == cur1 )
        SetCurMode( cur2 );
    else if( CurMode == cur2 )
        SetCurMode( cur1 );
}

void FOClient::SetLastCurMode()
{
    if( CurModeLast == CUR_WAIT )
        return;
    if( CurMode == CurModeLast )
        return;
    SetCurMode( CurModeLast );
}

void FOClient::SetCurPos( int x, int y )
{
    GameOpt.MouseX = x;
    GameOpt.MouseY = y;
    SDL_WarpMouseInWindow( MainWindow, x, y );
    SDL_FlushEvent( SDL_MOUSEMOTION );
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::LmapPrepareMap()
{
    LmapPrepPix.clear();

    if( !Chosen )
        return;

    int  maxpixx = ( LmapWMap[ 2 ] - LmapWMap[ 0 ] ) / 2 / LmapZoom;
    int  maxpixy = ( LmapWMap[ 3 ] - LmapWMap[ 1 ] ) / 2 / LmapZoom;
    int  bx = Chosen->GetHexX() - maxpixx;
    int  by = Chosen->GetHexY() - maxpixy;
    int  ex = Chosen->GetHexX() + maxpixx;
    int  ey = Chosen->GetHexY() + maxpixy;

    uint vis = Chosen->GetLookDistance();
    uint cur_color = 0;
    int  pix_x = LmapWMap[ 2 ] - LmapWMap[ 0 ], pix_y = 0;

    for( int i1 = bx; i1 < ex; i1++ )
    {
        for( int i2 = by; i2 < ey; i2++ )
        {
            pix_y += LmapZoom;
            if( i1 < 0 || i2 < 0 || i1 >= HexMngr.GetWidth() || i2 >= HexMngr.GetHeight() )
                continue;

            bool is_far = false;
            uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), i1, i2 );
            if( dist > vis )
                is_far = true;

            Field& f = HexMngr.GetField( i1, i2 );
            if( f.Crit )
            {
                cur_color = ( f.Crit == Chosen ? 0xFF0000FF : ( f.Crit->GetFollowCrit() == Chosen->GetId() ? 0xFFFF00FF : 0xFFFF0000 ) );
                LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x + ( LmapZoom - 1 ), LmapWMap[ 1 ] + pix_y, cur_color ) );
                LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x, LmapWMap[ 1 ] + pix_y + ( ( LmapZoom - 1 ) / 2 ), cur_color ) );
            }
            else if( f.Flags.IsExitGrid )
            {
                cur_color = 0x3FFF7F00;
            }
            else if( f.Flags.IsWall || f.Flags.IsScen )
            {
                if( f.Flags.IsWallSAI || f.Flags.ScrollBlock )
                    continue;
                if( LmapSwitchHi == false && !f.Flags.IsWall )
                    continue;
                cur_color = ( f.Flags.IsWall ? 0xFF00FF00 : 0x7F00FF00 );
            }
            else
            {
                continue;
            }

            if( is_far )
                cur_color = COLOR_CHANGE_ALPHA( cur_color, 0x22 );
            LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x, LmapWMap[ 1 ] + pix_y, cur_color ) );
            LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x + ( LmapZoom - 1 ), LmapWMap[ 1 ] + pix_y + ( ( LmapZoom - 1 ) / 2 ), cur_color ) );
        }
        pix_x -= LmapZoom;
        pix_y = 0;
    }

    LmapPrepareNextTick = Timer::FastTick() + MINIMAP_PREPARE_TICK;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

bool  FOClient::GmapActive;
float FOClient::GmapZoom;
int   FOClient::GmapOffsetX, FOClient::GmapOffsetY;
int   FOClient::GmapGroupCurX, FOClient::GmapGroupCurY, FOClient::GmapGroupToX, FOClient::GmapGroupToY;
bool  FOClient::GmapWait;
float FOClient::GmapGroupSpeed;

#define GMAP_LOC_ALPHA    ( 60 )
#define GMAP_MOVE_TICK    ( 50 )

void FOClient::GmapNullParams()
{
    GmapGroupRealOldX = 0;
    GmapGroupRealOldY = 0;
    GmapGroupRealCurX = 0;
    GmapGroupRealCurY = 0;
    GmapGroupCurX = 0;
    GmapGroupCurY = 0;
    GmapGroupToX = 0;
    GmapGroupToY = 0;
    GmapGroupSpeed = 0.0f;
    GmapWait = false;
    GmapTabsLastScr = 0;
    GmapCurHoldBLocId = 0;
    GmapHoldX = GmapHoldY = 0;
    GmapLoc.clear();
    SAFEREL( GmapCar.Car );
    GmapCar.MasterId = 0;
    GmapFog.Fill( 0 );
    GmapActive = false;
    DeleteCritters();
    GmapTrace.clear();
}

void FOClient::GmapProcess()
{
    if( !GmapActive )
    {
        SetCurMode( CUR_WAIT );
        return;
    }

    if( GmapActive && !GmapWait )
    {
        // Process moving, scroll, path tracing
        if( GmapGroupSpeed != 0.0f )
        {
            int proc = (int) ( (float) ( Timer::GameTick() - GmapMoveTick ) / (float) GameOpt.GlobalMapMoveTime * 100.0f );
            if( proc > 100 )
                proc = 100;
            int old_x = GmapGroupCurX;
            int old_y = GmapGroupCurY;
            GmapGroupCurX = GmapGroupRealOldX + ( GmapGroupRealCurX - GmapGroupRealOldX ) * proc / 100;
            GmapGroupCurY = GmapGroupRealOldY + ( GmapGroupRealCurY - GmapGroupRealOldY ) * proc / 100;

            if( GmapGroupCurX != old_x || GmapGroupCurY != old_y )
            {
                GmapOffsetX += (int) ( ( old_x - GmapGroupCurX ) / GmapZoom );
                GmapOffsetY += (int) ( ( old_y - GmapGroupCurY ) / GmapZoom );

                GMAP_CHECK_MAPSCR;

                static uint point_tick = 0;
                if( Timer::GameTick() >= point_tick )
                {
                    if( GmapTrace.empty() || GmapTrace[ 0 ].first != old_x || GmapTrace[ 0 ].second != old_y )
                        GmapTrace.push_back( PAIR( old_x, old_y ) );
                    point_tick = Timer::GameTick() + GM_TRACE_TIME;
                }
            }
        }
        else
        {
            GmapTrace.clear();
        }
    }

    if( IfaceHold == IFACE_GMAP_TABSCRUP ) // Scroll Up
    {
        if( IsCurInRect( GmapBTabsScrUp ) )
        {
            if( Timer::FastTick() - GmapTabsLastScr >= 10 )
            {
                if( GmapTabNextX )
                    GmapTabsScrX -= ( Timer::FastTick() - GmapTabsLastScr ) / 5;
                if( GmapTabNextY )
                    GmapTabsScrY -= ( Timer::FastTick() - GmapTabsLastScr ) / 5;
                if( GmapTabsScrX < 0 )
                    GmapTabsScrX = 0;
                if( GmapTabsScrY < 0 )
                    GmapTabsScrY = 0;
                GmapTabsLastScr = Timer::FastTick();
            }
        }
        else
        {
            IfaceHold = IFACE_NONE;
        }
    }

    if( IfaceHold == IFACE_GMAP_TABSCRDW ) // Scroll down
    {
        if( IsCurInRect( GmapBTabsScrDn ) )
        {
            int tabs_count = 0;
            for( uint i = 0, j = (uint) GmapLoc.size(); i < j; i++ )
            {
                GmapLocation& loc = GmapLoc[ i ];
                if( MsgLocations->Count( STR_LOC_LABEL_PIC( loc.LocPid ) ) && ResMngr.GetIfaceAnim( Str::GetHash( MsgLocations->GetStr( STR_LOC_LABEL_PIC( loc.LocPid ) ) ) ) )
                    tabs_count++;
            }

            if( Timer::FastTick() - GmapTabsLastScr >= 10 )
            {
                if( GmapTabNextX )
                {
                    GmapTabsScrX += ( Timer::FastTick() - GmapTabsLastScr ) / 5;
                    if( GmapTabsScrX > GmapWTab.W() * tabs_count )
                        GmapTabsScrX = GmapWTab.W() * tabs_count;
                }
                if( GmapTabNextY )
                {
                    GmapTabsScrY += ( Timer::FastTick() - GmapTabsLastScr ) / 5;
                    if( GmapTabsScrY > GmapWTab.H() * tabs_count )
                        GmapTabsScrY = GmapWTab.H() * tabs_count;
                }
                GmapTabsLastScr = Timer::FastTick();
            }
        }
        else
        {
            IfaceHold = IFACE_NONE;
        }
    }
}

void FOClient::GmapDraw()
{
    if( !GmapActive )
    {
        SprMngr.DrawSprite( GmapWMainPic, 0, 0 );
        return;
    }

    // World map
    int wmx = 0, wmy = 0;
    for( uint py = 0; py < GmapTilesY; py++ )
    {
        wmx = 0;
        for( uint px = 0; px < GmapTilesX; px++ )
        {
            uint index = py * GmapTilesX + px;
            if( index >= GmapPic.size() )
                continue;

            if( !GmapPic[ index ] )
                GmapPic[ index ] = ResMngr.GetAnim( FileManager::GetPathHash( Str::FormatBuf( GmapTilesPic, index ), PT_ART_INTRFACE ), RES_ATLAS_DYNAMIC );
            if( !GmapPic[ index ] )
                continue;

            SpriteInfo* spr_inf = SprMngr.GetSpriteInfo( GmapPic[ index ]->GetCurSprId() );
            if( !spr_inf )
                continue;

            int w = spr_inf->Width;
            int h = spr_inf->Height;

            int mx1 = (int) ( wmx / GmapZoom ) + GmapOffsetX;
            int my1 = (int) ( wmy / GmapZoom ) + GmapOffsetY;
            int mx2 = mx1 + (int) ( w / GmapZoom );
            int my2 = my1 + (int) ( h / GmapZoom );

            int sx1 = GmapWMap[ 0 ];
            int sy1 = GmapWMap[ 1 ];
            int sx2 = GmapWMap[ 2 ];
            int sy2 = GmapWMap[ 3 ];

            if( ( sx1 >= mx1 && sx1 <= mx2 && sy1 >= my1 && sy1 <= my2 ) ||
                ( sx2 >= mx1 && sx2 <= mx2 && sy1 >= my1 && sy1 <= my2 ) ||
                ( sx1 >= mx1 && sx1 <= mx2 && sy2 >= my1 && sy2 <= my2 ) ||
                ( sx2 >= mx1 && sx2 <= mx2 && sy2 >= my1 && sy2 <= my2 ) ||
                ( mx1 >= sx1 && mx1 <= sx2 && my1 >= sy1 && my1 <= sy2 ) ||
                ( mx2 >= sx1 && mx2 <= sx2 && my1 >= sy1 && my1 <= sy2 ) ||
                ( mx1 >= sx1 && mx1 <= sx2 && my2 >= sy1 && my2 <= sy2 ) ||
                ( mx2 >= sx1 && mx2 <= sx2 && my2 >= sy1 && my2 <= sy2 ) )
                SprMngr.DrawSpriteSize( GmapPic[ index ],
                                        mx1, my1, (int) ( w / GmapZoom ), (int) ( h / GmapZoom ), true, false );

            wmx += w;
            if( px == GmapTilesX - 1 )
                wmy += h;
        }
    }

    // Script draw
    DrawIfaceLayer( 100 );

    // Global map fog
    int fog_l = 0;
    int fog_r = GameOpt.GlobalMapWidth;
    int fog_t = 0;
    int fog_b = GameOpt.GlobalMapHeight;
    GmapFogPix.clear();
    for( int zx = fog_l; zx <= fog_r; zx++ )
    {
        for( int zy = fog_t; zy <= fog_b; zy++ )
        {
            int val = GmapFog.Get2Bit( zx, zy );
            if( val == GM_FOG_NONE )
                continue;
            uint color = COLOR_RGBA( 0xFF, 0, 0, 0 );      // GM_FOG_FULL
            if( val == GM_FOG_HALF )
                color = COLOR_RGBA( 0x7F, 0, 0, 0 );
            else if( val == GM_FOG_HALF_EX )
                color = COLOR_RGBA( 0x3F, 0, 0, 0 );
            float l = float(zx * GM_ZONE_LEN) / GmapZoom + GmapOffsetX;
            float t = float(zy * GM_ZONE_LEN) / GmapZoom + GmapOffsetY;
            float r = l + GM_ZONE_LEN / GmapZoom;
            float b = t + GM_ZONE_LEN / GmapZoom;
            SprMngr.PrepareSquare( GmapFogPix, RectF( l, t, r, b ), color );
        }
    }
    SprMngr.DrawPoints( GmapFogPix, PRIMITIVE_TRIANGLELIST );

    // Locations on map
    for( auto it = GmapLoc.begin(); it != GmapLoc.end(); ++it )
    {
        GmapLocation& loc = ( *it );
        int           radius = loc.Radius;
        if( radius <= 0 )
            radius = 6;
        int loc_pic_x1 = (int) ( ( loc.LocWx - radius ) / GmapZoom ) + GmapOffsetX;
        int loc_pic_y1 = (int) ( ( loc.LocWy - radius ) / GmapZoom ) + GmapOffsetY;
        int loc_pic_x2 = (int) ( ( loc.LocWx + radius ) / GmapZoom ) + GmapOffsetX;
        int loc_pic_y2 = (int) ( ( loc.LocWy + radius ) / GmapZoom ) + GmapOffsetY;
        if( loc_pic_x1 <= GmapWMap[ 2 ] && loc_pic_y1 <= GmapWMap[ 3 ] && loc_pic_x2 >= GmapWMap[ 0 ] && loc_pic_y2 >= GmapWMap[ 1 ] )
        {
            SprMngr.DrawSpriteSize( GmapLocPic, loc_pic_x1, loc_pic_y1, loc_pic_x2 - loc_pic_x1, loc_pic_y2 - loc_pic_y1,
                                    true, false, loc.Color ? loc.Color : COLOR_RGBA( GMAP_LOC_ALPHA, 0, 255, 0 ) );
        }
    }

    // On map
    if( GmapWait )
    {
        AnyFrames*  pic = ( ( Timer::GameTick() % 1000 ) < 500 ? GmapPLightPic0 : GmapPLightPic1 );
        SpriteInfo* si = SprMngr.GetSpriteInfo( pic->GetCurSprId() );
        if( si )
            SprMngr.DrawSprite( pic, (int) ( GmapGroupCurX / GmapZoom ) + GmapOffsetX - si->Width / 2, (int) ( GmapGroupCurY / GmapZoom ) + GmapOffsetY - si->Height / 2 );
    }
    else
    {
        if( GmapGroupSpeed != 0.0f )
        {
            SpriteInfo* si;
            if( ( si = SprMngr.GetSpriteInfo( GmapPGr->GetCurSprId() ) ) )
                SprMngr.DrawSprite( GmapPGr, (int) ( GmapGroupCurX / GmapZoom ) + GmapOffsetX - si->Width / 2, (int) ( GmapGroupCurY / GmapZoom ) + GmapOffsetY - si->Height / 2 );
            if( ( si = SprMngr.GetSpriteInfo( GmapPTarg->GetCurSprId() ) ) )
                SprMngr.DrawSprite( GmapPTarg, (int) ( GmapGroupToX / GmapZoom ) + GmapOffsetX - si->Width / 2, (int) ( GmapGroupToY / GmapZoom ) + GmapOffsetY - si->Height / 2 );
        }
        else
        {
            SpriteInfo* si;
            if( IfaceHold == IFACE_GMAP_TOLOC )
            {
                if( ( si = SprMngr.GetSpriteInfo( GmapPStayDn->GetCurSprId() ) ) )
                    SprMngr.DrawSprite( GmapPStayDn, (int) ( GmapGroupCurX / GmapZoom ) + GmapOffsetX - si->Width / 2, (int) ( GmapGroupCurY / GmapZoom ) + GmapOffsetY - si->Height / 2 );
            }
            else
            {
                if( ( si = SprMngr.GetSpriteInfo( GmapPStay->GetCurSprId() ) ) )
                    SprMngr.DrawSprite( GmapPStay, (int) ( GmapGroupCurX / GmapZoom ) + GmapOffsetX - si->Width / 2, (int) ( GmapGroupCurY / GmapZoom ) + GmapOffsetY - si->Height / 2 );
            }
        }
    }

    // Trace
    static PointVec gt;
    gt.clear();
    for( auto it = GmapTrace.begin(), end = GmapTrace.end(); it != end; ++it )
        gt.push_back( PrepPoint( (int) ( it->first / GmapZoom ) + GmapOffsetX, (int) ( it->second / GmapZoom ) + GmapOffsetY, 0xFFFF0000 ) );
    SprMngr.DrawPoints( gt, PRIMITIVE_POINTLIST );

    // Script draw
    DrawIfaceLayer( 101 );

    // Cut off map
    GmapMapCutOff.clear();
    SprMngr.PrepareSquare( GmapMapCutOff, Rect( 0, 0, GameOpt.ScreenWidth, GmapWMap.T ), COLOR_RGB( 0, 0, 0 ) );
    SprMngr.PrepareSquare( GmapMapCutOff, Rect( 0, GmapWMap.T, GmapWMap.L, GmapWMap.B ), COLOR_RGB( 0, 0, 0 ) );
    SprMngr.PrepareSquare( GmapMapCutOff, Rect( GmapWMap.R, GmapWMap.T, GameOpt.ScreenWidth, GmapWMap.B ), COLOR_RGB( 0, 0, 0 ) );
    SprMngr.PrepareSquare( GmapMapCutOff, Rect( 0, GmapWMap.B, GameOpt.ScreenWidth, GameOpt.ScreenHeight ), COLOR_RGB( 0, 0, 0 ) );
    SprMngr.DrawPoints( GmapMapCutOff, PRIMITIVE_TRIANGLELIST );

    // Tabs pics
    int cur_tabx = GmapWTabs[ 0 ] - GmapTabsScrX;
    int cur_taby = GmapWTabs[ 1 ] - GmapTabsScrY;

    for( uint i = 0, j = (uint) GmapLoc.size(); i < j; i++ )
    {
        GmapLocation& loc = GmapLoc[ i ];

        if( !MsgLocations->Count( STR_LOC_LABEL_PIC( loc.LocPid ) ) )
            continue;
        AnyFrames* tab_pic = ResMngr.GetIfaceAnim( Str::GetHash( MsgLocations->GetStr( STR_LOC_LABEL_PIC( loc.LocPid ) ) ) );
        if( !tab_pic )
            continue;

        SprMngr.DrawSprite( GmapPWTab, cur_tabx, cur_taby );
        SprMngr.DrawSprite( tab_pic, GmapWTabLoc[ 0 ] + cur_tabx, GmapWTabLoc[ 1 ] + cur_taby );

        if( IfaceHold == IFACE_GMAP_TABBTN && GmapCurHoldBLocId == loc.LocId )   // Button down
            SprMngr.DrawSprite( GmapPBTabLoc, GmapBTabLoc[ 0 ] + cur_tabx, GmapBTabLoc[ 1 ] + cur_taby );

        cur_tabx += GmapTabNextX;
        cur_taby += GmapTabNextY;
        if( cur_tabx > GmapWTabs[ 2 ] || cur_taby > GmapWTabs[ 3 ] )
            break;
    }

    // Empty tabs
    while( GmapTabNextX || GmapTabNextY )
    {
        if( cur_tabx > GmapWTabs[ 2 ] || cur_taby > GmapWTabs[ 3 ] )
            break;
        SprMngr.DrawSprite( GmapPWBlankTab, cur_tabx, cur_taby );
        cur_tabx += GmapTabNextX;
        cur_taby += GmapTabNextY;
    }

    // Main pic
    SprMngr.DrawSprite( GmapWMainPic, 0, 0 );

    // Buttons
    switch( IfaceHold )
    {
    case IFACE_GMAP_TOWN:
        SprMngr.DrawSprite( GmapPBTownDw, GmapBTown[ 0 ], GmapBTown[ 1 ] );
        break;
    case IFACE_GMAP_TABSCRUP:
        SprMngr.DrawSprite( GmapPTabScrUpDw, GmapBTabsScrUp[ 0 ], GmapBTabsScrUp[ 1 ] );
        break;
    case IFACE_GMAP_TABSCRDW:
        SprMngr.DrawSprite( GmapPTabScrDwDw, GmapBTabsScrDn[ 0 ], GmapBTabsScrDn[ 1 ] );
        break;
    case IFACE_GMAP_INV:
        SprMngr.DrawSprite( GmapBInvPicDown, GmapBInv[ 0 ], GmapBInv[ 1 ] );
        break;
    case IFACE_GMAP_MENU:
        SprMngr.DrawSprite( GmapBMenuPicDown, GmapBMenu[ 0 ], GmapBMenu[ 1 ] );
        break;
    case IFACE_GMAP_CHA:
        SprMngr.DrawSprite( GmapBChaPicDown, GmapBCha[ 0 ], GmapBCha[ 1 ] );
        break;
    case IFACE_GMAP_PIP:
        SprMngr.DrawSprite( GmapBPipPicDown, GmapBPip[ 0 ], GmapBPip[ 1 ] );
        break;
    case IFACE_GMAP_FIX:
        SprMngr.DrawSprite( GmapBFixPicDown, GmapBFix[ 0 ], GmapBFix[ 1 ] );
        break;
    default:
        break;
    }

    // Car
    Item* car = GmapGetCar();
    if( car )
    {
        SprMngr.DrawSpriteSize( car->GetCurSprId(), GmapWCar.L, GmapWCar.T, GmapWCar.W(), GmapWCar.H(), false, true );
        SprMngr.DrawStr( GmapWCar, FmtItemLook( car, ITEM_LOOK_WM_CAR ), FT_CENTERX | FT_BOTTOM, COLOR_TEXT, FONT_DEFAULT );
    }

    // Day time
    int gh = GameOpt.Hour + 11;
    if( gh >= 24 )
        gh -= 24;
    AnimRun( GmapWDayTimeAnim, ANIMRUN_SET_FRM( gh ) );
    SprMngr.DrawSprite( AnimGetCurSpr( GmapWDayTimeAnim ), GmapWDayTime[ 0 ], GmapWDayTime[ 1 ] );

    // Lock
    if( Chosen && !Chosen->IsGmapRule() )
        SprMngr.DrawStr( GmapWLock, MsgGame->GetStr( STR_GMAP_LOCKED ), FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );

    // Critters
    int pos = 0;
    for( auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end(); it++, pos++ )
    {
        CritterCl* cr = it->second;
        SprMngr.DrawStr( Rect( GmapWName, GmapWNameStepX * pos, GmapWNameStepY * pos ), cr->GetName(), FT_NOBREAK | FT_CENTERY, cr->IsGmapRule() ? COLOR_TEXT_DGREEN : COLOR_TEXT );
        SprMngr.DrawStr( Rect( GmapWName, GmapWNameStepX * pos, GmapWNameStepY * pos ), cr->IsOffline() ? "offline" : "online", FT_NOBREAK | FT_CENTERR, cr->IsOffline() ? COLOR_TEXT_DDRED : COLOR_TEXT_DDGREEN, FONT_SPECIAL );
    }

    // Map coord
    if( GetActiveScreen() == SCREEN_NONE && IsCurInRect( GmapWMap ) && !IsLMenu() )
    {
        int cx = (int) ( ( GameOpt.MouseX - GmapOffsetX ) * GmapZoom );
        int cy = (int) ( ( GameOpt.MouseY - GmapOffsetY ) * GmapZoom );
        if( GmapFog.Get2Bit( GM_ZONE( cx ), GM_ZONE( cy ) ) != GM_FOG_FULL )
        {
            GmapLocation* cur_loc = nullptr;
            for( auto it = GmapLoc.begin(); it != GmapLoc.end(); ++it )
            {
                GmapLocation& loc = ( *it );
                uint          radius = loc.Radius;      // MsgGM->GetInt(STR_GM_RADIUS(loc.LocPid));
                if( !radius || !MsgLocations->Count( STR_LOC_NAME( loc.LocPid ) ) || !MsgLocations->Count( STR_LOC_INFO( loc.LocPid ) ) )
                    continue;
                if( DistSqrt( loc.LocWx, loc.LocWy, cx, cy ) <= radius )
                {
                    cur_loc = &loc;
                    break;
                }
            }

            /*SpriteInfo* si = SprMngr.GetSpriteInfo( CurPDef->GetCurSprId() );
               if( Chosen )
               {
                SprMngr.DrawStr( Rect( GameOpt.MouseX + si->Width, GameOpt.MouseY + si->Height, GameOpt.MouseX + si->Width + 200, GameOpt.MouseY + si->Height + 500 ), cur_loc ?
                                 FmtGameText( STR_GMAP_CUR_LOC_INFO, cx, cy, GM_ZONE( cx ), GM_ZONE( cy ), MsgLocations->GetStr( STR_LOC_NAME( cur_loc->LocPid ) ), MsgLocations->GetStr( STR_LOC_INFO( cur_loc->LocPid ) ) ) :
                                 FmtGameText( STR_GMAP_CUR_INFO, cx, cy, GM_ZONE( cx ), GM_ZONE( cy ) ), 0 );
               }
               else if( cur_loc )
               {
                SprMngr.DrawStr( Rect( GameOpt.MouseX + si->Width, GameOpt.MouseY + si->Height, GameOpt.MouseX + si->Width + 200, GameOpt.MouseY + si->Height + 500 ),
                                 FmtGameText( STR_GMAP_LOC_INFO, MsgLocations->GetStr( STR_LOC_NAME( cur_loc->LocPid ) ), MsgLocations->GetStr( STR_LOC_INFO( cur_loc->LocPid ) ) ), 0 );
               }*/
        }
    }

    // Time
    SprMngr.DrawStr( Rect( GmapWTime ), Str::FormatBuf( "%02d", GameOpt.Day ), 0, COLOR_IFACE, FONT_NUM );                              // Day
    char mval = '0' + GameOpt.Month - 1 + 0x30;                                                                                         // Month
    SprMngr.DrawStr( Rect( GmapWTime, 26, 1 ), Str::FormatBuf( "%c", mval ), 0, COLOR_IFACE, FONT_NUM );                                // Month
    SprMngr.DrawStr( Rect( GmapWTime, 62, 0 ), Str::FormatBuf( "%04d", GameOpt.Year ), 0, COLOR_IFACE, FONT_NUM );                      // Year
    SprMngr.DrawStr( Rect( GmapWTime, 107, 0 ), Str::FormatBuf( "%02d%02d", GameOpt.Hour, GameOpt.Minute ), 0, COLOR_IFACE, FONT_NUM ); // Hour,Minute
}

void FOClient::GmapTownDraw()
{
    if( !GmapActive )
    {
        ShowScreen( SCREEN_NONE );
        return;
    }

    if( DistSqrt( GmapTownLoc.LocWx, GmapTownLoc.LocWy, GmapGroupCurX, GmapGroupCurY ) > GmapTownLoc.Radius )
    {
        ShowScreen( SCREEN_NONE );
        return;
    }

    SpriteInfo* si = SprMngr.GetSpriteInfo( GmapTownPic->GetCurSprId() );
    if( si )
        SprMngr.DrawSprite( GmapTownPic, ( GameOpt.ScreenWidth - si->Width ) / 2, ( GameOpt.ScreenHeight - si->Height ) / 2 );

    if( !Chosen || !Chosen->IsGmapRule() )
        return;

    for( uint i = 0; i < GmapTownTextPos.size(); i++ )
    {
        if( GmapTownLoc.LocId != GmapShowEntrancesLocId || !GmapShowEntrances[ i ] )
            continue;

        // Enter to entrance
        if( Chosen && Chosen->IsGmapRule() )
        {
            AnyFrames* pic = GmapPTownInPic;
            if( IfaceHold == IFACE_GMAP_TOWN_BUT && GmapTownCurButton == (int) i )
                pic = GmapPTownInPicDn;
            SprMngr.DrawSprite( pic, GmapTownTextPos[ i ][ 0 ] + GmapPTownInOffsX, GmapTownTextPos[ i ][ 1 ] + GmapPTownInOffsY );
        }

        // View entrance
        AnyFrames* pic = GmapPTownViewPic;
        if( IfaceHold == IFACE_GMAP_VIEW_BUT && GmapTownCurButton == (int) i )
            pic = GmapPTownViewPicDn;
        SprMngr.DrawSprite( pic, GmapTownTextPos[ i ][ 0 ] + GmapPTownViewOffsX, GmapTownTextPos[ i ][ 1 ] + GmapPTownViewOffsY );
    }

    for( uint i = 0; i < GmapTownTextPos.size(); i++ )
    {
        if( GmapTownLoc.LocId != GmapShowEntrancesLocId || !GmapShowEntrances[ i ] )
            continue;
        SprMngr.DrawStr( GmapTownTextPos[ i ], GmapTownText[ i ].c_str(), 0 );
    }
}

void FOClient::GmapLMouseDown()
{
    IfaceHold = IFACE_NONE;

    // Town picture
    if( GetActiveScreen() == SCREEN__GM_TOWN )
    {
        for( uint i = 0; i < GmapTownTextPos.size(); i++ )
        {
            if( GmapTownLoc.LocId != GmapShowEntrancesLocId || !GmapShowEntrances[ i ] )
                continue;
            Rect& r = GmapTownTextPos[ i ];

            // Enter to entrance
            if( Chosen && Chosen->IsGmapRule() && IsCurInRect( r, GmapPTownInOffsX, GmapPTownInOffsY ) &&
                SprMngr.IsPixNoTransp( GmapPTownInPicMask->GetCurSprId(), GameOpt.MouseX - r[ 0 ] - GmapPTownInOffsX, GameOpt.MouseY - r[ 1 ] - GmapPTownInOffsY, false ) )
            {
                IfaceHold = IFACE_GMAP_TOWN_BUT;
                GmapTownCurButton = i;
                break;
            }

            // View entrance
            if( IsCurInRect( r, GmapPTownViewOffsX, GmapPTownViewOffsY ) &&
                SprMngr.IsPixNoTransp( GmapPTownViewPicMask->GetCurSprId(), GameOpt.MouseX - r[ 0 ] - GmapPTownViewOffsX, GameOpt.MouseY - r[ 1 ] - GmapPTownViewOffsY, false ) )
            {
                IfaceHold = IFACE_GMAP_VIEW_BUT;
                GmapTownCurButton = i;
                break;
            }
        }

        return;
    }

    // Main screen
    if( IsCurInRect( GmapBInv ) )
        IfaceHold = IFACE_GMAP_INV;
    else if( IsCurInRect( GmapBMenu ) )
        IfaceHold = IFACE_GMAP_MENU;
    else if( IsCurInRect( GmapBCha ) )
        IfaceHold = IFACE_GMAP_CHA;
    else if( IsCurInRect( GmapBPip ) )
        IfaceHold = IFACE_GMAP_PIP;
    else if( IsCurInRect( GmapBFix ) )
        IfaceHold = IFACE_GMAP_FIX;
    else if( IsCurInRect( GmapBTabsScrUp ) )
    {
        IfaceHold = IFACE_GMAP_TABSCRUP;
        GmapTabsLastScr = Timer::FastTick();
    }
    else if( IsCurInRect( GmapBTabsScrDn ) )
    {
        IfaceHold = IFACE_GMAP_TABSCRDW;
        GmapTabsLastScr = Timer::FastTick();
    }
    else
    {
        for( int i = 0; i < (int) HexMngr.GetCritters().size(); i++ )
        {
            if( !IsCurInRect( Rect( GmapWName, GmapWNameStepX * i, GmapWNameStepY * i ) ) )
                continue;
            LMenuTryActivate();
            return;
        }
    }

    if( IfaceHold == IFACE_NONE && Chosen && Chosen->IsGmapRule() )
    {
        if( IsCurInRect( GmapBTown ) )
        {
            IfaceHold = IFACE_GMAP_TOWN;
        }
        else if( IsCurInRect( GmapWTabs ) )
        {
            GmapCurHoldBLocId = GmapGetMouseTabLocId();
            if( GmapCurHoldBLocId )
                IfaceHold = IFACE_GMAP_TABBTN;
        }
    }

    if( IfaceHold == IFACE_NONE && IsCurInRect( GmapWMap ) )
    {
        GmapHoldX = GameOpt.MouseX;
        GmapHoldY = GameOpt.MouseY;

        if( GmapGroupSpeed == 0.0f )
        {
            SpriteInfo* si = SprMngr.GetSpriteInfo( GmapPStayMask->GetCurSprId() );
            if( si )
            {
                int x = (int) ( GmapGroupCurX / GmapZoom ) + GmapOffsetX - si->Width / 2;
                int y = (int) ( GmapGroupCurY / GmapZoom ) + GmapOffsetY - si->Height / 2;

                if( GameOpt.MouseX >= x && GameOpt.MouseX <= x + si->Width && GameOpt.MouseY >= y && GameOpt.MouseY <= y + si->Height &&
                    SprMngr.IsPixNoTransp( GmapPStayMask->GetCurSprId(), GameOpt.MouseX - x, GameOpt.MouseY - y, false ) )
                {
                    IfaceHold = IFACE_GMAP_TOLOC;
                    return;
                }
            }
        }

        IfaceHold = IFACE_GMAP_MAP;
    }
}

void FOClient::GmapLMouseUp()
{
    if( GetActiveScreen() == SCREEN__GM_TOWN )
    {
        if( !IsCurInRect( GmapTownPicPos ) )
        {
            ShowScreen( SCREEN_NONE );
        }
        else if( IfaceHold == IFACE_GMAP_TOWN_BUT && GmapTownCurButton >= 0 && GmapTownCurButton < (int) GmapTownTextPos.size() &&
                 IsCurInRect( GmapTownTextPos[ GmapTownCurButton ], GmapPTownInOffsX, GmapPTownInOffsY ) &&
                 SprMngr.IsPixNoTransp( GmapPTownInPicMask->GetCurSprId(), GameOpt.MouseX - GmapTownTextPos[ GmapTownCurButton ][ 0 ] - GmapPTownInOffsX, GameOpt.MouseY - GmapTownTextPos[ GmapTownCurButton ][ 1 ] - GmapPTownInOffsY, false ) )
        {
            Net_SendRuleGlobal( GM_CMD_TOLOCAL, GmapTownLoc.LocId, GmapTownCurButton );
        }
        else if( IfaceHold == IFACE_GMAP_VIEW_BUT && GmapTownCurButton >= 0 && GmapTownCurButton < (int) GmapTownTextPos.size() &&
                 IsCurInRect( GmapTownTextPos[ GmapTownCurButton ], GmapPTownViewOffsX, GmapPTownViewOffsY ) &&
                 SprMngr.IsPixNoTransp( GmapPTownViewPicMask->GetCurSprId(), GameOpt.MouseX - GmapTownTextPos[ GmapTownCurButton ][ 0 ] - GmapPTownViewOffsX, GameOpt.MouseY - GmapTownTextPos[ GmapTownCurButton ][ 1 ] - GmapPTownViewOffsY, false ) )
        {
            Net_SendRuleGlobal( GM_CMD_VIEW_MAP, GmapTownLoc.LocId, GmapTownCurButton );
        }

        IfaceHold = IFACE_NONE;
        return;
    }

    if( ( IfaceHold == IFACE_GMAP_TOLOC || IfaceHold == IFACE_GMAP_MAP ) && IsCurInRect( GmapWMap ) && DistSqrt( GameOpt.MouseX, GameOpt.MouseY, GmapHoldX, GmapHoldY ) <= 2 )
    {
        if( IfaceHold == IFACE_GMAP_TOLOC )
        {
            SpriteInfo* si = SprMngr.GetSpriteInfo( GmapPStayMask->GetCurSprId() );
            if( si )
            {
                int x = (int) ( GmapGroupCurX / GmapZoom ) + GmapOffsetX - si->Width / 2;
                int y = (int) ( GmapGroupCurY / GmapZoom ) + GmapOffsetY - si->Height / 2;

                if( GameOpt.MouseX >= x && GameOpt.MouseX <= x + si->Width && GameOpt.MouseY >= y && GameOpt.MouseY <= y + si->Height &&
                    SprMngr.IsPixNoTransp( GmapPStayMask->GetCurSprId(), GameOpt.MouseX - x, GameOpt.MouseY - y, false ) )
                {
                    uint loc_id = 0;
                    for( auto it = GmapLoc.begin(); it != GmapLoc.end(); ++it )
                    {
                        GmapLocation& loc = ( *it );
                        uint          radius = loc.Radius;
                        if( !radius )
                            radius = 6;

                        if( DistSqrt( loc.LocWx, loc.LocWy, GmapGroupCurX, GmapGroupCurY ) <= radius )
                        {
                            loc_id = loc.LocId;
                            break;
                        }
                    }

                    Net_SendRuleGlobal( GM_CMD_TOLOCAL, loc_id, 0 );
                }
            }
        }
        else if( IfaceHold == IFACE_GMAP_MAP )
        {
            Net_SendRuleGlobal( GM_CMD_SETMOVE, (int) ( ( GameOpt.MouseX - GmapOffsetX ) * GmapZoom ), (int) ( ( GameOpt.MouseY - GmapOffsetY ) * GmapZoom ) );
        }
    }
    else if( IfaceHold == IFACE_GMAP_TOWN && IsCurInRect( GmapBTown ) )
    {
        for( auto it = GmapLoc.begin(); it != GmapLoc.end(); ++it )
        {
            GmapLocation& loc = ( *it );
            uint          radius = loc.Radius;
            if( !radius )
                radius = 6;

            if( DistSqrt( loc.LocWx, loc.LocWy, GmapGroupCurX, GmapGroupCurY ) <= radius )
            {
                GmapTownLoc = loc;
                ShowScreen( SCREEN__GM_TOWN );
                break;
            }
        }
    }
    else if( IfaceHold == IFACE_GMAP_TABBTN && IsCurInRect( GmapWTabs ) )
    {
        uint loc_id = GmapGetMouseTabLocId();
        if( loc_id == GmapCurHoldBLocId )
        {
            for( uint i = 0, j = (uint) GmapLoc.size(); i < j; i++ )
            {
                GmapLocation& loc = GmapLoc[ i ];
                if( loc.LocId == loc_id )
                {
                    Net_SendRuleGlobal( GM_CMD_SETMOVE, loc.LocWx, loc.LocWy );
                    break;
                }
            }
        }
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::GmapRMouseDown()
{
    if( IsCurInRect( GmapWMap ) )
    {
        IfaceHold = IFACE_GMAP_MOVE_MAP;
        GmapVectX = GameOpt.MouseX - GmapOffsetX;
        GmapVectY = GameOpt.MouseY - GmapOffsetY;
    }
}

void FOClient::GmapRMouseUp()
{
    IfaceHold = IFACE_NONE;
}

void FOClient::GmapMouseMove()
{
    if( IfaceHold == IFACE_GMAP_MOVE_MAP )
    {
        if( GameOpt.MouseX < GmapWMap[ 0 ] )
            GameOpt.MouseX = GmapWMap[ 0 ];
        if( GameOpt.MouseY < GmapWMap[ 1 ] )
            GameOpt.MouseY = GmapWMap[ 1 ];
        if( GameOpt.MouseX > GmapWMap[ 2 ] )
            GameOpt.MouseX = GmapWMap[ 2 ];
        if( GameOpt.MouseY > GmapWMap[ 3 ] )
            GameOpt.MouseY = GmapWMap[ 3 ];

        SetCurPos( GameOpt.MouseX, GameOpt.MouseY );

        GmapOffsetX = GameOpt.MouseX - GmapVectX;
        GmapOffsetY = GameOpt.MouseY - GmapVectY;

        GMAP_CHECK_MAPSCR;
    }
}

void FOClient::GmapChangeZoom( float offs, bool revert /* = false */ )
{
    if( !GmapActive )
        return;
    if( !IsCurInRect( GmapWMap ) )
        return;
    if( GmapZoom + offs > 2.0f )
        return;
    if( GmapZoom + offs < 0.2f )
        return;

    float scr_x = (float) ( GmapWMap.CX() - GmapOffsetX ) * GmapZoom;
    float scr_y = (float) ( GmapWMap.CY() - GmapOffsetY ) * GmapZoom;

    GmapZoom += offs;

    GmapOffsetX = (int) ( (float) GmapWMap.CX() - scr_x / GmapZoom );
    GmapOffsetY = (int) ( (float) GmapWMap.CY() - scr_y / GmapZoom );

    GMAP_CHECK_MAPSCR;

    if( !revert )
    {
        if( GmapOffsetX > GmapWMap.L || GmapOffsetY > GmapWMap.T ||
            GmapOffsetX < GmapWMap.R - GM_MAXX / GmapZoom || GmapOffsetY < GmapWMap.B - GM_MAXY / GmapZoom )
            GmapChangeZoom( -offs, true );
    }
}

Item* FOClient::GmapGetCar()
{
    return GmapCar.Car;
}

uint FOClient::GmapGetMouseTabLocId()
{
    if( IsCurInRect( GmapWTabs ) )
    {
        int tab_x = ( GameOpt.MouseX - ( GmapWTabs[ 0 ] - GmapTabsScrX ) ) % GmapWTab.W();
        int tab_y = ( GameOpt.MouseY - ( GmapWTabs[ 1 ] - GmapTabsScrY ) ) % GmapWTab.H();

        if( tab_x >= GmapBTabLoc[ 0 ] && tab_y >= GmapBTabLoc[ 1 ] && tab_x <= GmapBTabLoc[ 2 ] && tab_y <= GmapBTabLoc[ 3 ] )
        {
            int tab_pos = -1;
            if( GmapTabNextX )
                tab_pos = ( GameOpt.MouseX - ( GmapWTabs[ 0 ] - GmapTabsScrX ) ) / GmapWTab.W();
            else if( GmapTabNextY )
                tab_pos = ( GameOpt.MouseY - ( GmapWTabs[ 1 ] - GmapTabsScrY ) ) / GmapWTab.H();

            int cur_tab = 0;
            for( uint i = 0, j = (uint) GmapLoc.size(); i < j; i++ )
            {
                GmapLocation& loc = GmapLoc[ i ];

                // Skip na
                if( !MsgLocations->Count( STR_LOC_LABEL_PIC( loc.LocPid ) ) )
                    continue;
                AnyFrames* tab_pic = ResMngr.GetIfaceAnim( Str::GetHash( MsgLocations->GetStr( STR_LOC_LABEL_PIC( loc.LocPid ) ) ) );
                if( !tab_pic )
                    continue;

                // Get
                if( tab_pos == cur_tab )
                    return loc.LocId;

                cur_tab++;
            }
        }
    }
    return 0;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::ShowDialogBox()
{
    ScriptArray* button_texts = Script::CreateArray( "string@[]" );
    for( uint i = 0; i < DlgboxButtonsCount; i++ )
    {
        ScriptString* sstr = ScriptString::Create( DlgboxButtonText[ i ].c_str() );
        button_texts->InsertLast( &sstr );
        sstr->Release();
    }

    ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
    int               type = DlgboxType;
    dict->Set( "BoxType", &type, asTYPEID_INT32 );
    dict->Set( "WaitTime", &DlgboxWait, asTYPEID_UINT32 );
    dict->Set( "Buttons", &DlgboxButtonsCount, asTYPEID_UINT32 );
    ScriptString* text = ScriptString::Create( DlgboxText );
    dict->Set( "Text", &text, Script::GetEngine()->GetTypeIdByDecl( "string@" ) );
    dict->Set( "ButtonTexts", &button_texts, Script::GetEngine()->GetTypeIdByDecl( "string@[]@" ) );

    ShowScreen( SCREEN__DIALOGBOX, dict );

    button_texts->Release();
    text->Release();
    dict->Release();
}

void FOClient::DlgboxAnswer( int selected )
{
    if( selected == DlgboxButtonsCount - 1 )
    {
        if( DlgboxType >= DIALOGBOX_ENCOUNTER_ANY && DlgboxType <= DIALOGBOX_ENCOUNTER_TB )
            Net_SendRuleGlobal( GM_CMD_ANSWER, -1 );
        return;
    }

    if( DlgboxType == DIALOGBOX_FOLLOW )
    {
        Net_SendRuleGlobal( GM_CMD_FOLLOW, FollowRuleId );
    }
    else if( DlgboxType == DIALOGBOX_BARTER )
    {
        if( selected == 0 )
            Net_SendPlayersBarter( BARTER_ACCEPTED, PBarterPlayerId, false );
        else
            Net_SendPlayersBarter( BARTER_ACCEPTED, PBarterPlayerId, true );
    }
    else if( DlgboxType == DIALOGBOX_ENCOUNTER_ANY )
    {
        if( selected == 0 )
            Net_SendRuleGlobal( GM_CMD_ANSWER, COMBAT_MODE_REAL_TIME );
        else
            Net_SendRuleGlobal( GM_CMD_ANSWER, COMBAT_MODE_TURN_BASED );
    }
    else if( DlgboxType == DIALOGBOX_ENCOUNTER_RT )
    {
        Net_SendRuleGlobal( GM_CMD_ANSWER, COMBAT_MODE_REAL_TIME );
    }
    else if( DlgboxType == DIALOGBOX_ENCOUNTER_TB )
    {
        Net_SendRuleGlobal( GM_CMD_ANSWER, COMBAT_MODE_TURN_BASED );
    }
}
void FOClient::WaitDraw()
{
    SprMngr.DrawSpriteSize( WaitPic, 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight, true, true );
    SprMngr.Flush();
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::SplitStart( uint item_id, int item_cont )
{
    #define FIND_IN_CONT( cont )    { auto it = PtrCollectionFind( cont.begin(), cont.end(), item_id ); item = ( it != cont.end() ? *it : NULL ); }
    Item* item = nullptr;
    switch( item_cont )
    {
    case ITEMS_CHOSEN_ALL:
        item = ( Chosen ? Chosen->GetItem( item_id ) : nullptr );
        break;
    case ITEMS_INVENTORY:
        FIND_IN_CONT( InvContInit );
        break;
    case ITEMS_USE:
        FIND_IN_CONT( InvContInit );
        break;
    case ITEMS_BARTER:
        FIND_IN_CONT( InvContInit );
        break;
    case ITEMS_BARTER_OFFER:
        FIND_IN_CONT( BarterCont1oInit );
        break;
    case ITEMS_BARTER_OPPONENT:
        FIND_IN_CONT( BarterCont2Init );
        break;
    case ITEMS_BARTER_OPPONENT_OFFER:
        FIND_IN_CONT( BarterCont2oInit );
        break;
    case ITEMS_PICKUP:
        FIND_IN_CONT( InvContInit );
        break;
    case ITEMS_PICKUP_FROM:
        FIND_IN_CONT( PupCont2Init );
        break;
    default:
        return;
    }
    #undef FIND_IN_CONT
    if( !item )
        return;

    ScriptDictionary* dict = ScriptDictionary::Create( Script::GetEngine() );
    dict->Set( "TargetItemId", &item_id, asTYPEID_UINT32 );
    dict->Set( "ItemsCollection", &item_cont, asTYPEID_INT32 );
    ShowScreen( SCREEN__SPLIT, dict );
    dict->Release();
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::FixGenerate( int fix_mode )
{
    if( !Chosen )
    {
        ShowScreen( SCREEN_NONE );
        return;
    }

    FixMode = fix_mode;

    if( fix_mode == FIX_MODE_LIST )
    {
        FixCraftLst.clear();

        CraftItemVec true_crafts;
        MrFixit.GetShowCrafts( Chosen, true_crafts );

        SCraftVec scraft_vec;
        UIntVec   script_craft;
        for( uint i = 0, cur_height = 0; i < true_crafts.size(); ++i )
        {
            CraftItem* craft = true_crafts[ i ];

            if( craft->Script->length() )
            {
                script_craft.push_back( craft->Num );
                if( !FixShowCraft.count( craft->Num ) )
                    continue;
            }

            Rect pos( FixWWin[ 0 ], FixWWin[ 1 ] + cur_height, FixWWin[ 2 ], FixWWin[ 1 ] + cur_height + 100 );
            int  line_height = SprMngr.GetLinesHeight( FixWWin.W(), 0, craft->Name->c_str() );

            cur_height += line_height;
            pos.B = FixWWin[ 1 ] + cur_height;

            if( (int) cur_height > FixWWin.H() )
            {
                FixCraftLst.push_back( scraft_vec );
                scraft_vec.clear();

                i--;
                cur_height = 0;
                continue;

                pos.T = FixWWin[ 1 ];
                pos.B = FixWWin[ 1 ] + line_height;
                cur_height = line_height;
            }

            scraft_vec.push_back( SCraft( pos, craft->Name->c_std_str(), craft->Num, MrFixit.IsTrueCraft( Chosen, craft->Num ) ) );
        }

        if( !scraft_vec.empty() )
            FixCraftLst.push_back( scraft_vec );
        if( script_craft.size() && Timer::FastTick() >= FixNextShowCraftTick )
            Net_SendCraftAsk( script_craft );
    }
    else if( fix_mode == FIX_MODE_FIXIT )
    {
        SCraftVec* cur_vec = GetCurSCrafts();
        if( !cur_vec || FixCurCraft >= (int) cur_vec->size() )
        {
            ShowScreen( SCREEN_NONE );
            return;
        }

        SCraft*    scraft = &( *cur_vec )[ FixCurCraft ];
        CraftItem* craft = MrFixit.GetCraft( scraft->Num );

        if( !craft )
        {
            ShowScreen( SCREEN_NONE );
            return;
        }

        Rect   r( FixWWin[ 0 ], FixWWin[ 1 ], FixWWin[ 2 ], FixWWin[ 1 ] );
        string str;
        int    x;

        for( uint i = 0, j = (uint) FixDrawComp.size(); i < j; i++ )
            delete FixDrawComp[ i ];
        FixDrawComp.clear();

        // Out items
        UCharVec tmp_vec;            // Temp vector
        for( uint i = 0; i < craft->OutItems.size(); i++ )
            tmp_vec.push_back( 0 );  // Push AND
        FixGenerateItems( craft->OutItems, craft->OutItemsVal, tmp_vec, str, r, x );

        // About
        if( craft->Info->length() )
        {
            str = "\n";
            str += craft->Info->c_std_str();
            str += "\n";
            FixGenerateStrLine( str, r );
        }

        // Need params
        if( craft->NeedPNum.size() )
        {
            str = "\n";
            str += MsgGame->GetStr( STR_FIX_PARAMS );
            str += "\n";
            for( uint i = 0, j = (uint) craft->NeedPNum.size(); i < j; i++ )
            {
                // Need
                str += MsgGame->GetStr( STR_PARAM_NAME( craft->NeedPNum[ i ] ) );
                str += " ";
                str += Str::ItoA( craft->NeedPVal[ i ] );

                // You have
                str += " (";
                str += MsgGame->GetStr( STR_FIX_YOUHAVE );
                str += Str::ItoA( Properties::GetValueAsInt( Chosen, craft->NeedPNum[ i ] ) );
                str += ")";

                // And, or
                if( i == j - 1 )
                    break;

                str += "\n";
                if( craft->NeedPOr[ i ] )
                    str += MsgGame->GetStr( STR_OR );
                else
                    str += MsgGame->GetStr( STR_AND );
            }
            FixGenerateStrLine( str, r );
        }

        // Need tools
        if( craft->NeedTools.size() )
        {
            str = "\n";
            str += MsgGame->GetStr( STR_FIX_TOOLS );
            FixGenerateStrLine( str, r );
            FixGenerateItems( craft->NeedTools, craft->NeedToolsVal, craft->NeedToolsOr, str, r, x );
        }

        // Need items
        if( craft->NeedItems.size() )
        {
            str = "\n";
            str += MsgGame->GetStr( STR_FIX_ITEMS );
            FixGenerateStrLine( str, r );
            FixGenerateItems( craft->NeedItems, craft->NeedItemsVal, craft->NeedItemsOr, str, r, x );
        }
    }
    else if( fix_mode == FIX_MODE_RESULT )
    {
        FixResultStr = "ERROR RESULT";

        switch( FixResult )
        {
        case CRAFT_RESULT_NONE:
            FixResultStr = "...";
            break;
        case CRAFT_RESULT_SUCC:
            FixResultStr = MsgGame->GetStr( STR_FIX_SUCCESS );
            break;
        case CRAFT_RESULT_FAIL:
            FixResultStr = MsgGame->GetStr( STR_FIX_FAIL );
            break;
        case CRAFT_RESULT_TIMEOUT:
            FixResultStr = MsgGame->GetStr( STR_FIX_TIMEOUT );
            break;
        default:
            break;
        }
    }
}

void FOClient::FixGenerateStrLine( string& str, Rect& r )
{
    r.B += SprMngr.GetLinesHeight( FixWWin.W(), 0, str.c_str() );
    FixDrawComp.push_back( new FixDrawComponent( r, str ) );
    r.T = r.B;
}

void FOClient::FixGenerateItems( HashVec& items_vec, UIntVec& val_vec, UCharVec& or_vec, string& str, Rect& r, int& x )
{
    str = "";
    for( uint i = 0, j = (uint) items_vec.size(); i < j; i++ )
    {
        uint color = COLOR_TEXT;
        if( Chosen->CountItemPid( items_vec[ i ] ) < val_vec[ i ] )
            color = COLOR_TEXT_DGREEN;

        str += "|";
        str += Str::FormatBuf( "%u", color );
        str += " ";

        if( i > 0 )
        {
            if( or_vec[ i - 1 ] )
                str += MsgGame->GetStr( STR_OR );
            else
                str += MsgGame->GetStr( STR_AND );
        }

        ProtoItem* proto = ProtoMngr.GetProtoItem( items_vec[ i ] );
        if( !proto )
            str += "???";
        else
            str += MsgItem->GetStr( ITEM_STR_ID( proto->ProtoId, 1 ) );

        if( val_vec[ i ] > 1 )
        {
            str += " ";
            str += Str::UItoA( val_vec[ i ] );
            str += " ";
            str += MsgGame->GetStr( STR_FIX_PIECES );
        }

        str += "\n";
    }

    FixGenerateStrLine( str, r );

    x = FixWWin[ 0 ] + FixWWin.W() / 2 - FIX_DRAW_PIC_WIDTH / 2 * (uint) items_vec.size();
    for( uint i = 0, j = (uint) items_vec.size(); i < j; i++, x += FIX_DRAW_PIC_WIDTH )
    {
        ProtoItem* proto = ProtoMngr.GetProtoItem( items_vec[ i ] );
        if( !proto )
            continue;

        AnyFrames* anim = ResMngr.GetInvAnim( proto->GetPicInv() );
        if( !anim )
            continue;

        Rect r2 = r;
        r2.L = x;
        r2.R = x + FIX_DRAW_PIC_WIDTH;
        r2.B += FIX_DRAW_PIC_HEIGHT;

        FixDrawComp.push_back( new FixDrawComponent( r2, anim ) );
    }
    r.B += FIX_DRAW_PIC_HEIGHT;
    r.T = r.B;
}


int FOClient::GetMouseCraft()
{
    if( IfaceHold != IFACE_NONE )
        return -1;
    if( !IsCurInRect( FixWWin, FixX, FixY ) )
        return -1;

    SCraftVec* cur_vec = GetCurSCrafts();
    if( !cur_vec )
        return -1;

    for( uint i = 0, j = (uint) cur_vec->size(); i < j; i++ )
    {
        SCraft* scraft = &( *cur_vec )[ i ];
        if( IsCurInRect( scraft->Pos, FixX, FixY ) )
            return i;
    }

    return -1;
}

FOClient::SCraftVec* FOClient::GetCurSCrafts()
{
    if( FixCraftLst.empty() )
        return nullptr;
    if( FixScrollLst >= (int) FixCraftLst.size() )
        FixScrollLst = (int) FixCraftLst.size() - 1;
    return &FixCraftLst[ FixScrollLst ];
}

void FOClient::FixDraw()
{
    SprMngr.DrawSprite( FixMainPic, FixWMain[ 0 ] + FixX, FixWMain[ 1 ] + FixY );

    switch( IfaceHold )
    {
    case IFACE_FIX_SCRUP:
        SprMngr.DrawSprite( FixPBScrUpDn, FixBScrUp[ 0 ] + FixX, FixBScrUp[ 1 ] + FixY );
        break;
    case IFACE_FIX_SCRDN:
        SprMngr.DrawSprite( FixPBScrDnDn, FixBScrDn[ 0 ] + FixX, FixBScrDn[ 1 ] + FixY );
        break;
    case IFACE_FIX_DONE:
        SprMngr.DrawSprite( FixPBDoneDn, FixBDone[ 0 ] + FixX, FixBDone[ 1 ] + FixY );
        break;
    case IFACE_FIX_FIX:
        SprMngr.DrawSprite( FixPBFixDn, FixBFix[ 0 ] + FixX, FixBFix[ 1 ] + FixY );
        break;
    default:
        break;
    }

    if( FixMode != FIX_MODE_FIXIT )
        SprMngr.DrawSprite( FixPBFixDn, FixBFix[ 0 ] + FixX, FixBFix[ 1 ] + FixY );

    if( FixMode == FIX_MODE_FIXIT )
    {
        for( uint i = 0, j = (uint) FixDrawComp.size(); i < j; i++ )
        {
            FixDrawComponent* c = FixDrawComp[ i ];
            if( !c->IsText )
                SprMngr.DrawSpriteSize( c->Anim, c->Place.L + FixX, c->Place.T + FixY, c->Place.W(), c->Place.H(), false, true );
        }
    }

    SprMngr.Flush();

    switch( FixMode )
    {
    case FIX_MODE_LIST:
    {
        // Crafts names
        SCraftVec* cur_vec = GetCurSCrafts();
        if( !cur_vec )
            break;

        for( uint i = 0, j = (uint) cur_vec->size(); i < j; i++ )
        {
            SCraft* scraft = &( *cur_vec )[ i ];
            uint    col = COLOR_TEXT;
            if( !scraft->IsTrue )
                col = COLOR_TEXT_DRED;
            if( i == (uint) FixCurCraft )
            {
                if( IfaceHold == IFACE_FIX_CHOOSE )
                    col = COLOR_TEXT_DBLUE;
                else
                    col = COLOR_TEXT_DGREEN;
            }

            SprMngr.DrawStr( Rect( scraft->Pos, FixX, FixY ), scraft->Name.c_str(), 0, col );
        }

        // Number of page
        char str[ 64 ];
        Str::Format( str, "%u/%u", FixScrollLst + 1, FixCraftLst.size() );
        SprMngr.DrawStr( Rect( FixWWin[ 2 ] - 30 + FixX, FixWWin[ 3 ] - 15 + FixY, FixWWin[ 2 ] + FixX, FixWWin[ 3 ] + FixY ), str, FT_NOBREAK );
    }
    break;
    case FIX_MODE_FIXIT:
    {
        for( uint i = 0, j = (uint) FixDrawComp.size(); i < j; i++ )
        {
            FixDrawComponent* c = FixDrawComp[ i ];
            if( c->IsText )
                SprMngr.DrawStr( Rect( c->Place, FixX, FixY ), c->Text.c_str(), FT_CENTERX );
        }
    }
    break;
    case FIX_MODE_RESULT:
    {
        SprMngr.DrawStr( Rect( FixWWin, FixX, FixY ), FixResultStr.c_str(), FT_CENTERX );
    }
    break;
    default:
        break;
    }
}

void FOClient::FixLMouseDown()
{
    IfaceHold = IFACE_NONE;
    if( !IsCurInRect( FixWMain, FixX, FixY ) )
        return;

    if( IsCurInRect( FixBScrUp, FixX, FixY ) )
        IfaceHold = IFACE_FIX_SCRUP;
    else if( IsCurInRect( FixBScrDn, FixX, FixY ) )
        IfaceHold = IFACE_FIX_SCRDN;
    else if( IsCurInRect( FixBDone, FixX, FixY ) )
        IfaceHold = IFACE_FIX_DONE;
    else if( IsCurInRect( FixBFix, FixX, FixY ) )
        IfaceHold = IFACE_FIX_FIX;
    else if( FixMode == FIX_MODE_LIST && IsCurInRect( FixWWin, FixX, FixY ) )
    {
        FixCurCraft = GetMouseCraft();
        if( FixCurCraft != -1 )
            IfaceHold = IFACE_FIX_CHOOSE;
    }

    if( IfaceHold != IFACE_NONE )
        return;

    FixVectX = GameOpt.MouseX - FixX;
    FixVectY = GameOpt.MouseY - FixY;
    IfaceHold = IFACE_FIX_MAIN;
}

void FOClient::FixLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_FIX_SCRUP:
    {
        if( !IsCurInRect( FixBScrUp, FixX, FixY ) )
            break;

        if( FixMode == FIX_MODE_LIST )
        {
            if( FixScrollLst > 0 )
                FixScrollLst--;
        }
        else if( FixMode == FIX_MODE_FIXIT )
        {
            if( FixScrollFix > 0 )
                FixScrollFix--;
        }
    }
    break;
    case IFACE_FIX_SCRDN:
    {
        if( !IsCurInRect( FixBScrDn, FixX, FixY ) )
            break;

        if( FixMode == FIX_MODE_LIST )
        {
            if( FixScrollLst < (int) FixCraftLst.size() - 1 )
                FixScrollLst++;
        }
        else if( FixMode == FIX_MODE_FIXIT )
        {
            if( FixScrollFix < (int) FixCraftFix.size() - 1 )
                FixScrollFix++;
        }
    }
    break;
    case IFACE_FIX_DONE:
    {
        if( !IsCurInRect( FixBDone, FixX, FixY ) )
            break;

        if( FixMode != FIX_MODE_LIST )
            FixGenerate( FIX_MODE_LIST );
        else
            ShowScreen( SCREEN_NONE );
    }
    break;
    case IFACE_FIX_FIX:
    {
        if( !IsCurInRect( FixBFix, FixX, FixY ) )
            break;
        if( FixMode != FIX_MODE_FIXIT )
            break;

        SCraftVec* cur_vec = GetCurSCrafts();
        if( !cur_vec )
            break;
        if( FixCurCraft >= (int) cur_vec->size() )
            break;

        SCraft& craft = ( *cur_vec )[ FixCurCraft ];
        uint    num = craft.Num;

        FixResult = CRAFT_RESULT_NONE;
        FixGenerate( FIX_MODE_RESULT );
        Net_SendCraft( num );
        WaitPing();
    }
    break;
    case IFACE_FIX_CHOOSE:
    {
        if( FixMode != FIX_MODE_LIST )
            break;
        if( FixCurCraft < 0 )
            break;
        IfaceHold = IFACE_NONE;
        if( FixCurCraft != GetMouseCraft() )
            break;
        SCraftVec* cur_vec = GetCurSCrafts();
        if( !cur_vec )
            break;
        if( FixCurCraft >= (int) cur_vec->size() )
            break;

        FixGenerate( FIX_MODE_FIXIT );
    }
    break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::FixMouseMove()
{
    if( FixMode == FIX_MODE_LIST && IfaceHold == IFACE_NONE )
        FixCurCraft = GetMouseCraft();

    if( IfaceHold == IFACE_FIX_MAIN )
    {
        FixX = GameOpt.MouseX - FixVectX;
        FixY = GameOpt.MouseY - FixVectY;

        if( FixX < 0 )
            FixX = 0;
        if( FixX + FixWMain[ 2 ] > GameOpt.ScreenWidth )
            FixX = GameOpt.ScreenWidth - FixWMain[ 2 ];
        if( FixY < 0 )
            FixY = 0;
        if( FixY + FixWMain[ 3 ] > GameOpt.ScreenHeight )
            FixY = GameOpt.ScreenHeight - FixWMain[ 3 ];
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

#define SAVE_LOAD_LINES_PER_SLOT    ( 3 )
void FOClient::SaveLoadCollect()
{
    // Collect singleplayer saves
    SaveLoadDataSlots.clear();

    // For each all saves in folder
    StrVec fnames;
    FileManager::GetFolderFileNames( FileManager::GetWritePath( "", PT_SAVE ), true, "foworld", fnames );
    for( uint i = 0; i < fnames.size(); i++ )
    {
        const string& fname = fnames[ i ];

        // Open file
        IniParser save;
        if( !save.AppendFile( FileManager::GetWritePath( fname.c_str(), PT_SAVE ), PT_ROOT ) )
            continue;

        if( !save.IsApp( "GeneralSettings" ) || !save.IsApp( "Client" ) )
            continue;
        StrMap& settings = save.GetApp( "GeneralSettings" );
        StrMap& client = save.GetApp( "Client" );

        // Read save data, offsets see SaveGameInfoFile in Server.cpp
        // Check singleplayer data
        if( !settings.count( "Singleplayer" ) || Str::CompareCase( settings[ "Singleplayer" ].c_str(), "true" ) )
            continue;

        // Critter name
        if( !client.count( "$Name" ) )
            continue;
        string cr_name = client[ "$Name" ];

        // Map pid
        if( !client.count( "MapPid" ) )
            continue;

        // Picture data
        UCharVec pic_data;
        if( settings.count( "SaveLoadPicture" ) )
            Str::ParseLine( settings[ "SaveLoadPicture" ].c_str(), ' ', pic_data, Str::AtoUI );

        // Game time
        ushort year = Str::AtoI( settings[ "Year" ].c_str() );
        ushort month = Str::AtoI( settings[ "Month" ].c_str() );
        ushort day = Str::AtoI( settings[ "Day" ].c_str() );
        ushort hour = Str::AtoI( settings[ "Hour" ].c_str() );
        ushort minute = Str::AtoI( settings[ "Minute" ].c_str() );

        // Extract name
        char save_name[ MAX_FOPATH ];
        Str::Copy( save_name, fname.c_str() );
        FileManager::EraseExtension( save_name );

        // Extract full path
        char fname_ex[ MAX_FOPATH ];
        FileManager::GetWritePath( save_name, PT_SAVE, fname_ex );
        ResolvePath( fname_ex );
        Str::Append( fname_ex, ".foworld" );

        // Fill slot data
        SaveLoadDataSlot slot;
        slot.Name = save_name;
        slot.Info = Str::FormatBuf( "%s\n%02d.%02d.%04d %02d:%02d:%02d\n", save_name,
                                    settings[ "RealDay" ].c_str(), settings[ "RealMonth" ].c_str(), settings[ "RealYear" ].c_str(),
                                    settings[ "RealHour" ].c_str(), settings[ "RealMinute" ].c_str(), settings[ "RealSecond" ].c_str() );
        slot.InfoExt = Str::FormatBuf( "%s\n%02d %3s %04d %02d%02d\n%s", cr_name.c_str(),
                                       day, MsgGame->GetStr( STR_MONTH( month ) ), year, hour, minute,
                                       MsgLocations->GetStr( STR_LOC_MAP_NAME( CurMapLocPid, CurMapIndexInLoc ) ) );
        slot.FileName = fname_ex;
        slot.RealTime = Str::AtoUI64( settings[ "SaveTimestamp" ].c_str() );
        slot.PicData = pic_data;
        SaveLoadDataSlots.push_back( slot );
    }

    // Sort by creation time
    struct SortByTime
    {
        static bool Do( const SaveLoadDataSlot& l, const SaveLoadDataSlot& r ) { return l.RealTime > r.RealTime; }
    };
    std::sort( SaveLoadDataSlots.begin(), SaveLoadDataSlots.end(), SortByTime::Do );

    // Set scroll data
    SaveLoadSlotScroll = 0;
    SaveLoadSlotsMax = SaveLoadSlots.H() / SprMngr.GetLinesHeight( 0, 0, "" ) / SAVE_LOAD_LINES_PER_SLOT;

    // Show actual draft
    SaveLoadShowDraft();
}

void FOClient::SaveLoadSaveGame( const char* name )
{
    // Get name of new save
    char fname[ MAX_FOPATH ];
    FileManager::GetWritePath( "", PT_SAVE, fname );
    ResolvePath( fname );
    Str::Append( fname, name );
    Str::Append( fname, ".foworld" );

    // Delete old files
    if( SaveLoadFileName != "" )
        FileDelete( SaveLoadFileName.c_str() );
    FileDelete( fname );

    // Get image data from surface
    UCharVec pic_data;
    if( SaveLoadDraftValid )
    {
        // Get data
        pic_data.resize( SAVE_LOAD_IMAGE_WIDTH * SAVE_LOAD_IMAGE_HEIGHT * 3 );
        GL( glBindTexture( GL_TEXTURE_2D, SaveLoadDraft->TargetTexture->Id ) );
        GL( glGetTexImage( GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, &pic_data[ 0 ] ) );
        GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
    }

    // Send request
    Net_SendSaveLoad( true, fname, &pic_data );

    // Close save/load screen
    ShowScreen( SCREEN_NONE );
}

void FOClient::SaveLoadFillDraft()
{
    SaveLoadProcessDraft = false;
    SaveLoadDraftValid = false;
    int           w = 0, h = 0;
    SDL_GetWindowSize( MainWindow, &w, &h );
    RenderTarget* rt = SprMngr.CreateRenderTarget( false, false, false, w, h, true );
    if( rt )
    {
        GL( glBindTexture( GL_TEXTURE_2D, rt->TargetTexture->Id ) );
        GL( glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h ) );
        GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
        SprMngr.PushRenderTarget( SaveLoadDraft );
        SprMngr.DrawRenderTarget( rt, false );
        SprMngr.PopRenderTarget();
        SprMngr.DeleteRenderTarget( rt );
        SaveLoadDraftValid = true;
    }
}

void FOClient::SaveLoadShowDraft()
{
    SaveLoadDraftValid = false;
    if( SaveLoadSlotIndex >= 0 && SaveLoadSlotIndex < (int) SaveLoadDataSlots.size() )
    {
        // Get surface from image data
        SaveLoadDataSlot& slot = SaveLoadDataSlots[ SaveLoadSlotIndex ];
        if( slot.PicData.size() == SAVE_LOAD_IMAGE_WIDTH * SAVE_LOAD_IMAGE_HEIGHT * 3 )
        {
            // Copy to texture
            GL( glBindTexture( GL_TEXTURE_2D, SaveLoadDraft->TargetTexture->Id ) );
            GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, SAVE_LOAD_IMAGE_WIDTH, SAVE_LOAD_IMAGE_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, &slot.PicData[ 0 ] ) );
            GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
            SaveLoadDraftValid = true;
        }
    }
    else if( SaveLoadSave && SaveLoadSlotIndex == (int) SaveLoadDataSlots.size() )
    {
        // Process SaveLoadFillDraft
        SaveLoadProcessDraft = true;
    }
}

void FOClient::SaveLoadProcessDone()
{
    if( SaveLoadSave )
    {
        SaveLoadProcessDraft = true;

        // ShowScreen( SCREEN__SAY );
        // SayType = DIALOGSAY_SAVE;
        // SayTitle = MsgGame->GetStr( STR_SAVE_LOAD_TYPE_RECORD_NAME );
        // SayText = "";
        // SayOnlyNumbers = false;

        SaveLoadFileName = "";
        if( SaveLoadSlotIndex >= 0 && SaveLoadSlotIndex < (int) SaveLoadDataSlots.size() )
        {
            // SayText = SaveLoadDataSlots[ SaveLoadSlotIndex ].Name;
            SaveLoadFileName = SaveLoadDataSlots[ SaveLoadSlotIndex ].FileName;
        }
    }
    else if( SaveLoadSlotIndex >= 0 && SaveLoadSlotIndex < (int) SaveLoadDataSlots.size() )
    {
        if( !IsConnected )
        {
            SaveLoadFileName = SaveLoadDataSlots[ SaveLoadSlotIndex ].FileName;
            InitNetReason = INIT_NET_REASON_LOAD;
        }
        else
        {
            Net_SendSaveLoad( false, SaveLoadDataSlots[ SaveLoadSlotIndex ].FileName.c_str(), nullptr );
            ShowScreen( SCREEN_NONE );
            WaitPing();
        }
    }
}

void FOClient::SaveLoadDraw()
{
    int ox = ( SaveLoadLoginScreen ? SaveLoadCX : SaveLoadX );
    int oy = ( SaveLoadLoginScreen ? SaveLoadCY : SaveLoadY );

    if( SaveLoadLoginScreen )
    {
        GL( glClear( GL_COLOR_BUFFER_BIT ) );
    }
    SprMngr.DrawSprite( SaveLoadMainPic, SaveLoadMain[ 0 ] + ox, SaveLoadMain[ 1 ] + oy );

    if( IfaceHold == IFACE_SAVELOAD_SCR_UP )
        SprMngr.DrawSprite( SaveLoadScrUpPicDown, SaveLoadScrUp[ 0 ] + ox, SaveLoadScrUp[ 1 ] + oy );
    else if( IfaceHold == IFACE_SAVELOAD_SCR_DN )
        SprMngr.DrawSprite( SaveLoadScrDownPicDown, SaveLoadScrDown[ 0 ] + ox, SaveLoadScrDown[ 1 ] + oy );
    else if( IfaceHold == IFACE_SAVELOAD_DONE )
        SprMngr.DrawSprite( SaveLoadDonePicDown, SaveLoadDone[ 0 ] + ox, SaveLoadDone[ 1 ] + oy );
    else if( IfaceHold == IFACE_SAVELOAD_BACK )
        SprMngr.DrawSprite( SaveLoadBackPicDown, SaveLoadBack[ 0 ] + ox, SaveLoadBack[ 1 ] + oy );

    SprMngr.DrawStr( Rect( SaveLoadText, ox, oy ), MsgGame->GetStr( SaveLoadSave ? STR_SAVE_LOAD_SAVE : STR_SAVE_LOAD_LOAD ), FT_NOBREAK | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SaveLoadDoneText, ox, oy ), MsgGame->GetStr( STR_SAVE_LOAD_DONE ), FT_NOBREAK | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SaveLoadBackText, ox, oy ), MsgGame->GetStr( STR_SAVE_LOAD_BACK ), FT_NOBREAK | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );

    // Slots
    int line_height = SprMngr.GetLinesHeight( 0, 0, "" );
    int cur = 0;
    for( int i = SaveLoadSlotScroll, j = (int) SaveLoadDataSlots.size(); i < j; i++ )
    {
        SaveLoadDataSlot& slot = SaveLoadDataSlots[ i ];
        SprMngr.DrawStr( Rect( SaveLoadSlots, ox, oy + cur * line_height * SAVE_LOAD_LINES_PER_SLOT ),
                         slot.Info.c_str(), FT_NOBREAK_LINE, i == SaveLoadSlotIndex ? COLOR_TEXT_DDGREEN : COLOR_TEXT );
        if( ++cur >= SaveLoadSlotsMax )
            break;
    }
    if( SaveLoadSave && SaveLoadSlotScroll <= (int) SaveLoadDataSlots.size() && cur <= SaveLoadSlotsMax - 1 )
    {
        SprMngr.DrawStr( Rect( SaveLoadSlots, ox, oy + cur * line_height * SAVE_LOAD_LINES_PER_SLOT ),
                         MsgGame->GetStr( STR_SAVE_LOAD_NEW_RECORD ), FT_NOBREAK_LINE, SaveLoadSlotIndex == (int) SaveLoadDataSlots.size() ? COLOR_TEXT_DDGREEN : COLOR_TEXT );
    }

    // Selected slot ext info
    if( SaveLoadSlotIndex >= 0 && SaveLoadSlotIndex < (int) SaveLoadDataSlots.size() )
    {
        SaveLoadDataSlot& slot = SaveLoadDataSlots[ SaveLoadSlotIndex ];
        SprMngr.DrawStr( Rect( SaveLoadInfo, ox, oy ), slot.InfoExt.c_str(), FT_CENTERY | FT_NOBREAK_LINE );
    }
    if( SaveLoadSave && SaveLoadSlotIndex == (int) SaveLoadDataSlots.size() )
    {
        SprMngr.DrawStr( Rect( SaveLoadInfo, ox, oy ), MsgGame->GetStr( STR_SAVE_LOAD_NEW_RECORD ), FT_CENTERY | FT_NOBREAK_LINE );
    }

    // Draw preview draft
    if( SaveLoadDraftValid )
    {
        Rect dst( SaveLoadPic.L + ox, SaveLoadPic.T + oy, SaveLoadPic.R + ox, SaveLoadPic.B + oy );
        SprMngr.DrawRenderTarget( SaveLoadDraft, false, nullptr, &dst );
    }
}

void FOClient::SaveLoadLMouseDown()
{
    IfaceHold = IFACE_NONE;
    int ox = ( SaveLoadLoginScreen ? SaveLoadCX : SaveLoadX );
    int oy = ( SaveLoadLoginScreen ? SaveLoadCY : SaveLoadY );

    if( !IsCurInRectNoTransp( SaveLoadMainPic->GetCurSprId(), SaveLoadMain, ox, oy ) )
        return;

    if( IsCurInRect( SaveLoadScrUp, ox, oy ) )
        IfaceHold = IFACE_SAVELOAD_SCR_UP;
    else if( IsCurInRect( SaveLoadScrDown, ox, oy ) )
        IfaceHold = IFACE_SAVELOAD_SCR_DN;
    else if( IsCurInRect( SaveLoadDone, ox, oy ) )
        IfaceHold = IFACE_SAVELOAD_DONE;
    else if( IsCurInRect( SaveLoadBack, ox, oy ) )
        IfaceHold = IFACE_SAVELOAD_BACK;
    else if( IsCurInRect( SaveLoadSlots, ox, oy ) )
    {
        int line_height = SprMngr.GetLinesHeight( 0, 0, "" );
        int line = ( GameOpt.MouseY - SaveLoadSlots.T - oy ) / line_height;
        int index = line / SAVE_LOAD_LINES_PER_SLOT;
        if( index < SaveLoadSlotsMax )
        {
            SaveLoadSlotIndex = index + SaveLoadSlotScroll;
            SaveLoadShowDraft();

            if( SaveLoadSlotIndex == SaveLoadClickSlotIndex && Timer::FastTick() - SaveLoadClickSlotTick <= GetDoubleClickTicks() )
            {
                SaveLoadProcessDone();
            }
            else
            {
                SaveLoadClickSlotIndex = SaveLoadSlotIndex;
                SaveLoadClickSlotTick = Timer::FastTick();
            }
        }
    }
    else if( !SaveLoadLoginScreen )
    {
        SaveLoadVectX = GameOpt.MouseX - SaveLoadX;
        SaveLoadVectY = GameOpt.MouseY - SaveLoadY;
        IfaceHold = IFACE_SAVELOAD_MAIN;
    }
}

void FOClient::SaveLoadLMouseUp()
{
    int ox = ( SaveLoadLoginScreen ? SaveLoadCX : SaveLoadX );
    int oy = ( SaveLoadLoginScreen ? SaveLoadCY : SaveLoadY );

    if( IfaceHold == IFACE_SAVELOAD_SCR_UP && IsCurInRect( SaveLoadScrUp, ox, oy ) )
    {
        if( SaveLoadSlotScroll > 0 )
            SaveLoadSlotScroll--;
    }
    else if( IfaceHold == IFACE_SAVELOAD_SCR_DN && IsCurInRect( SaveLoadScrDown, ox, oy ) )
    {
        int max = (int) SaveLoadDataSlots.size() - SaveLoadSlotsMax + ( SaveLoadSave ? 1 : 0 );
        if( SaveLoadSlotScroll < max )
            SaveLoadSlotScroll++;
    }
    else if( IfaceHold == IFACE_SAVELOAD_DONE && IsCurInRect( SaveLoadDone, ox, oy ) )
    {
        SaveLoadProcessDone();
    }
    else if( IfaceHold == IFACE_SAVELOAD_BACK && IsCurInRect( SaveLoadBack, ox, oy ) )
    {
        ShowScreen( SCREEN_NONE );
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::SaveLoadMouseMove()
{
    if( IfaceHold == IFACE_SAVELOAD_MAIN )
    {
        SaveLoadX = GameOpt.MouseX - SaveLoadVectX;
        SaveLoadY = GameOpt.MouseY - SaveLoadVectY;

        if( SaveLoadX < 0 )
            SaveLoadX = 0;
        if( SaveLoadX + SaveLoadMain[ 2 ] > GameOpt.ScreenWidth )
            SaveLoadX = GameOpt.ScreenWidth - SaveLoadMain[ 2 ];
        if( SaveLoadY < 0 )
            SaveLoadY = 0;
        if( SaveLoadY + SaveLoadMain[ 3 ] > GameOpt.ScreenHeight )
            SaveLoadY = GameOpt.ScreenHeight - SaveLoadMain[ 3 ];
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================
