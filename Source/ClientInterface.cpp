#include "Common.h"
#include "Client.h"

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

bool FOClient::IfaceLoadRect( Rect& comp, const char* name )
{
    char res[ MAX_FOTEXT ];
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

void FOClient::IfaceLoadRect2( Rect& comp, const char* name, int ox, int oy )
{
    if( IfaceLoadRect( comp, name ) )
        comp = Rect( comp, ox, oy );
}

void FOClient::IfaceLoadSpr( AnyFrames*& comp, const char* name )
{
    char res[ MAX_FOTEXT ];
    if( !IfaceIni.GetStr( name, "none.png", res ) )
        WriteLog( "Signature<%s> not found.\n", name );
    comp = SprMngr.LoadAnimation( res, PT_ART_INTRFACE, true );
    if( comp == SpriteManager::DummyAnimation )
        WriteLog( "File<%s> not found.\n", res );
}

void FOClient::IfaceLoadAnim( uint& comp, const char* name )
{
    char res[ MAX_FOTEXT ];
    if( !IfaceIni.GetStr( name, "none.png", res ) )
        WriteLog( "Signature<%s> not found.\n", name );
    if( !( comp = AnimLoad( res, PT_ART_INTRFACE, RES_ATLAS_STATIC ) ) )
        WriteLog( "Can't load animation<%s>.\n", res );
}

void FOClient::IfaceLoadArray( IntVec& arr, const char* name )
{
    char res[ MAX_FOTEXT ];
    if( IfaceIni.GetStr( name, "", res ) )
        Str::ParseLine( res, ' ', arr, Str::AtoI );
    else
        WriteLog( "Signature<%s> not found.\n", name );
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
    IfaceIni.UnloadFile();
    for( uint i = 0, j = (uint) IfaceIniNames.size(); i < j; i++ )
    {
        // Create name
        char file_name[ MAX_FOPATH ];
        Str::Copy( file_name, IfaceIniNames[ i ].c_str() );
        if( !Str::Substring( ini_name, "\\" ) && !Str::Substring( ini_name, "/" ) )
            FileManager::GetDataPath( file_name, PT_ART_INTRFACE, file_name );
        for( char* str = file_name; *str; str++ )
        {
            if( *str == '/' )
                *str = '\\';
        }

        // Data files
        DataFileVec& data_files = FileManager::GetDataFiles();
        for( int k = (int) data_files.size() - 1; k >= 0; k-- )
        {
            DataFile* data_file = data_files[ k ];
            uint      len;
            uint64    write_time;
            uchar*    data = data_file->OpenFile( file_name, file_name, len, write_time );
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
    typedef multimap< int, pair< char*, uint > > IniMMap;
    IniMMap sections;

    int     w = 0, h = 0;
    char*   begin = (char*) data;
    char*   end = Str::Substring( begin, "\nresolution " );
    while( true )
    {
        if( w <= GameOpt.ScreenWidth && h <= GameOpt.ScreenHeight )
        {
            uint l = (uint) ( end ? (size_t) end - (size_t) begin : (size_t) ( data + len ) - (size_t) begin );
            auto value = PAIR( begin, l );
            sections.insert( PAIR( w, value ) );
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
        IfaceIni.AppendPtrToBegin( ( *it ).second.first, ( *it ).second.second );
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

    // Use
    IfaceLoadRect( UseWMain, "UseMain" );
    IfaceLoadRect( UseWChosen, "UseChosen" );
    IfaceLoadRect( UseWInv, "UseInv" );
    IfaceLoadRect( UseBScrUp, "UseScrUp" );
    IfaceLoadRect( UseBScrDown, "UseScrDown" );
    IfaceLoadRect( UseBCancel, "UseCancel" );
    UseX = IfaceIni.GetInt( "UseX", -1 );
    if( UseX == -1 )
        UseX = ( GameOpt.ScreenWidth - UseWMain[ 2 ] ) / 2;
    UseY = IfaceIni.GetInt( "UseY", -1 );
    if( UseY == -1 )
        UseY = ( GameOpt.ScreenHeight - UseWMain[ 3 ] ) / 2;
    UseVectX = 0;
    UseVectY = 0;
    UseScroll = 0;
    UseHeightItem = IfaceIni.GetInt( "UseHeightItem", 30 );
    UseHoldId = 0;

    // Dialog
    DlgCurAnswPage = 0;
    DlgMaxAnswPage = 0;
    DlgCurAnsw = -1;
    DlgHoldAnsw = -1;
    DlgVectX = 0;
    DlgVectY = 0;
    DlgIsNpc = 0;
    DlgNpcId = 0;
    DlgEndTick = 0;
    DlgMainText = "";
    DlgMainTextCur = 0;
    DlgMainTextLinesReal = 1;
    IfaceLoadRect( DlgWMain, "DlgMain" );
    IfaceLoadRect( DlgWText, "DlgText" );
    DlgMainTextLinesRect = SprMngr.GetLinesCount( 0, DlgWText.H(), NULL );
    IfaceLoadRect( DlgBScrUp, "DlgScrUp" );
    IfaceLoadRect( DlgBScrDn, "DlgScrDn" );
    IfaceLoadRect( DlgAnsw, "DlgAnsw" );
    IfaceLoadRect2( DlgAnswText, "DlgAnswText", DlgAnsw[ 0 ], DlgAnsw[ 1 ] );
    IfaceLoadRect2( DlgWMoney, "DlgMoney", DlgAnsw[ 0 ], DlgAnsw[ 1 ] );
    IfaceLoadRect2( DlgBBarter, "DlgBarter", DlgAnsw[ 0 ], DlgAnsw[ 1 ] );
    IfaceLoadRect2( DlgBBarterText, "DlgBarterText", DlgAnsw[ 0 ], DlgAnsw[ 1 ] );
    IfaceLoadRect2( DlgBSay, "DlgSay", DlgAnsw[ 0 ], DlgAnsw[ 1 ] );
    IfaceLoadRect2( DlgBSayText, "DlgSayText", DlgAnsw[ 0 ], DlgAnsw[ 1 ] );
    IfaceLoadRect( DlgWAvatar, "DlgAvatar" );
    IfaceLoadRect( DlgWTimer, "DlgTimer" );
    DlgNextAnswX = IfaceIni.GetInt( "DlgNextAnswX", 1 );
    DlgNextAnswY = IfaceIni.GetInt( "DlgNextAnswY", 1 );
    DlgX = IfaceIni.GetInt( "DlgX", -1 );
    if( DlgX == -1 )
        DlgX = GameOpt.ScreenWidth / 2 - DlgWMain[ 2 ] / 2;
    DlgY = IfaceIni.GetInt( "DlgY", -1 );
    if( DlgY == -1 )
        DlgY = GameOpt.ScreenHeight / 2 - DlgWMain[ 3 ] / 2;
    // Barter
    BarterPlayerId = 0;
    IfaceLoadRect( BarterWMain, "BarterMain" );
    IfaceLoadRect2( BarterBOffer, "BarterOffer", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBOfferText, "BarterOfferText", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBTalk, "BarterTalk", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBTalkText, "BarterTalkText", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCont1Pic, "BarterCont1Pic", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCont2Pic, "BarterCont2Pic", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCont1, "BarterCont1", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCont2, "BarterCont2", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCont1o, "BarterCont1o", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCont2o, "BarterCont2o", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont1ScrUp, "BarterCont1ScrUp", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont2ScrUp, "BarterCont2ScrUp", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont1oScrUp, "BarterCont1oScrUp", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont2oScrUp, "BarterCont2oScrUp", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont1ScrDn, "BarterCont1ScrDn", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont2ScrDn, "BarterCont2ScrDn", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont1oScrDn, "BarterCont1oScrDn", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterBCont2oScrDn, "BarterCont2oScrDn", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCost1, "BarterCost1", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCost2, "BarterCost2", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWChosen, "BarterChosen", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    IfaceLoadRect2( BarterWCritter, "BarterCritter", BarterWMain[ 0 ], BarterWMain[ 1 ] );
    BarterCont1HeightItem = IfaceIni.GetInt( "BarterCont1ItemHeight", 30 );
    BarterCont2HeightItem = IfaceIni.GetInt( "BarterCont2ItemHeight", 30 );
    BarterCont1oHeightItem = IfaceIni.GetInt( "BarterCont1oItemHeight", 30 );
    BarterCont2oHeightItem = IfaceIni.GetInt( "BarterCont2oItemHeight", 30 );
    BarterHoldId = 0;
    BarterCount = 0;
    BarterScroll1 = 0;
    BarterScroll2 = 0;
    BarterScroll1o = 0;
    BarterScroll2o = 0;
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
    LMenuNodeHeight = IfaceIni.GetInt( "LMenuNodeHeight", 40 );
    LMenuCritNodes.push_back( LMENU_NODE_LOOK );
    LMenuCritNodes.push_back( LMENU_NODE_BREAK );
    LMenuScenNodes.push_back( LMENU_NODE_BREAK );
    LMenuNodes.push_back( LMENU_NODE_BREAK );

    // Minimap
    LmapX = IfaceIni.GetInt( "LmapX", 100 );
    LmapY = IfaceIni.GetInt( "LmapY", 100 );
    LmapVectX = 0;
    LmapVectY = 0;
    LmapZoom = 2;
    LmapSwitchHi = false;
    LmapPrepareNextTick = 0;
    IfaceLoadRect( LmapMain, "LmapMain" );
    IfaceLoadRect( LmapWMap, "LmapMap" );
    IfaceLoadRect( LmapBOk, "LmapOk" );
    IfaceLoadRect( LmapBScan, "LmapScan" );
    IfaceLoadRect( LmapBLoHi, "LmapLoHi" );

    // Town view
    IfaceLoadRect( TViewWMain, "TViewMain" );
    IfaceLoadRect( TViewBBack, "TViewBack" );
    IfaceLoadRect( TViewBEnter, "TViewEnter" );
    IfaceLoadRect( TViewBContours, "TViewContours" );
    TViewX = GameOpt.ScreenWidth - TViewWMain.W() - 10;
    TViewY = 10;
    TViewVectX = 0;
    TViewVectY = 0;
    TViewShowCountours = false;
    TViewType = TOWN_VIEW_FROM_NONE;
    TViewGmapLocId = 0;
    TViewGmapLocEntrance = 0;

    // Global map
    if( !IfaceIni.GetStr( "GmapTilesPic", "", GmapTilesPic ) )
        WriteLogF( _FUNC_, " - <GmapTilesPic> signature not found.\n" );
    GmapTilesX = IfaceIni.GetInt( "GmapTilesX", 0 );
    GmapTilesY = IfaceIni.GetInt( "GmapTilesY", 0 );
    GmapPic.resize( GmapTilesX * GmapTilesY );
    GmapFog.Create( GM__MAXZONEX, GM__MAXZONEY, NULL );

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
    GmapWNameStepX = IfaceIni.GetInt( "GmapNameStepX", 0 );
    GmapWNameStepY = IfaceIni.GetInt( "GmapNameStepY", 22 );
    IfaceLoadRect2( GmapWTabs, "GmapTabs", GmapX, GmapY );
    IfaceLoadRect2( GmapBTabsScrUp, "GmapTabsScrUp", GmapX, GmapY );
    IfaceLoadRect2( GmapBTabsScrDn, "GmapTabsScrDn", GmapX, GmapY );
    IfaceLoadRect( GmapWTab, "GmapTab" );
    IfaceLoadRect( GmapWTabLoc, "GmapTabLocImage" );
    IfaceLoadRect( GmapBTabLoc, "GmapTabLoc" );
    GmapTabNextX = IfaceIni.GetInt( "GmapTabNextX", 0 );
    GmapTabNextY = IfaceIni.GetInt( "GmapTabNextY", 0 );
    GmapNullParams();
    GmapTabsScrX = 0;
    GmapTabsScrY = 0;
    GmapVectX = 0;
    GmapVectY = 0;
    GmapNextShowEntrancesTick = 0;
    GmapShowEntrancesLocId = 0;
    memzero( GmapShowEntrances, sizeof( GmapShowEntrances ) );
    GmapPTownInOffsX = IfaceIni.GetInt( "GmapTownInOffsX", 0 );
    GmapPTownInOffsY = IfaceIni.GetInt( "GmapTownInOffsY", 0 );
    GmapPTownViewOffsX = IfaceIni.GetInt( "GmapTownViewOffsX", 0 );
    GmapPTownViewOffsY = IfaceIni.GetInt( "GmapTownViewOffsY", 0 );
    GmapZoom = 1.0f;

    // PickUp
    IfaceLoadRect( PupWMain, "PupMain" );
    IfaceLoadRect( PupWInfo, "PupInfo" );
    IfaceLoadRect( PupWCont1, "PupCont1" );
    IfaceLoadRect( PupWCont2, "PupCont2" );
    IfaceLoadRect( PupBTakeAll, "PupTakeAll" );
    IfaceLoadRect( PupBOk, "PupOk" );
    IfaceLoadRect( PupBScrUp1, "PupScrUp1" );
    IfaceLoadRect( PupBScrDw1, "PupScrDw1" );
    IfaceLoadRect( PupBScrUp2, "PupScrUp2" );
    IfaceLoadRect( PupBScrDw2, "PupScrDw2" );
    IfaceLoadRect( PupBNextCritLeft, "PupNextCritLeft" );
    IfaceLoadRect( PupBNextCritRight, "PupNextCritRight" );
    PupX = ( GameOpt.ScreenWidth - PupWMain.W() ) / 2;
    PupY = ( GameOpt.ScreenHeight - PupWMain.H() ) / 2;
    PupHeightItem1 = IfaceIni.GetInt( "PupHeightCont1", 0 );
    PupHeightItem2 = IfaceIni.GetInt( "PupHeightCont2", 0 );
    PupHoldId = 0;
    PupScroll1 = 0;
    PupScroll2 = 0;
    PupScrollCrit = 0;
    PupVectX = 0;
    PupVectY = 0;
    PupTransferType = 0;
    PupContId = 0;
    PupContPid = 0;
    PupCount = 0;
    Item::ClearItems( PupCont2 );
    PupLastPutId = 0;

    // Pipboy
    IfaceLoadRect( PipWMain, "PipMain" );
    IfaceLoadRect( PipWMonitor, "PipMonitor" );
    IfaceLoadRect( PipBStatus, "PipStatus" );
    // IfaceLoadRect(PipBGames,"PipGames");
    IfaceLoadRect( PipBAutomaps, "PipAutomaps" );
    IfaceLoadRect( PipBArchives, "PipArchives" );
    IfaceLoadRect( PipBClose, "PipClose" );
    IfaceLoadRect( PipWTime, "PipTime" );
    PipX = ( GameOpt.ScreenWidth - PipWMain.W() ) / 2;
    PipY = ( GameOpt.ScreenHeight - PipWMain.H() ) / 2;
    PipVectX = 0;
    PipVectY = 0;
    PipMode = PIP__NONE;
    memzero( PipScroll, sizeof( PipScroll ) );
    PipInfoNum = 0;
    Automaps.clear();
    AutomapWaitPids.clear();
    AutomapReceivedPids.clear();
    AutomapPoints.clear();
    AutomapCurMapPid = 0;
    AutomapScrX = 0.0f;
    AutomapScrY = 0.0f;
    AutomapZoom = 1.0f;

    // Aim
    IfaceLoadRect( AimWMain, "AimMain" );
    IfaceLoadRect( AimBCancel, "AimCancel" );
    IfaceLoadRect( AimWHeadT, "AimHeadText" );
    IfaceLoadRect( AimWLArmT, "AimLArmText" );
    IfaceLoadRect( AimWRArmT, "AimRArmText" );
    IfaceLoadRect( AimWTorsoT, "AimTorsoText" );
    IfaceLoadRect( AimWRLegT, "AimRLegText" );
    IfaceLoadRect( AimWLLegT, "AimLLegText" );
    IfaceLoadRect( AimWEyesT, "AimEyesText" );
    IfaceLoadRect( AimWGroinT, "AimGroinText" );
    IfaceLoadRect( AimWHeadP, "AimHeadProc" );
    IfaceLoadRect( AimWLArmP, "AimLArmProc" );
    IfaceLoadRect( AimWRArmP, "AimRArmProc" );
    IfaceLoadRect( AimWTorsoP, "AimTorsoProc" );
    IfaceLoadRect( AimWRLegP, "AimRLegProc" );
    IfaceLoadRect( AimWLLegP, "AimLLegProc" );
    IfaceLoadRect( AimWEyesP, "AimEyesProc" );
    IfaceLoadRect( AimWGroinP, "AimGroinProc" );
    AimPicX = IfaceIni.GetInt( "AimPicX", 0 );
    AimPicY = IfaceIni.GetInt( "AimPicY", 0 );
    AimX = ( GameOpt.ScreenWidth - AimWMain.W() ) / 2;
    AimY = ( GameOpt.ScreenHeight - AimWMain.H() ) / 2;
    AimVectX = 0;
    AimVectY = 0;
    AimPic = NULL;
    AimTargetId = 0;

    // Dialog box
    IfaceLoadRect( DlgboxWTop, "DlgboxTop" );
    IfaceLoadRect( DlgboxWMiddle, "DlgboxMiddle" );
    IfaceLoadRect( DlgboxWBottom, "DlgboxBottom" );
    IfaceLoadRect( DlgboxWText, "DlgboxText" );
    IfaceLoadRect( DlgboxBButton, "DlgboxButton" );
    IfaceLoadRect( DlgboxBButtonText, "DlgboxButtonText" );
    DlgboxX = ( GameOpt.ScreenWidth - DlgboxWTop.W() ) / 2;
    DlgboxY = ( GameOpt.ScreenHeight - DlgboxWTop.H() ) / 2;
    DlgboxVectX = 0;
    DlgboxVectY = 0;
    FollowRuleId = 0;
    DlgboxType = DIALOGBOX_NONE;
    FollowMap = 0;
    DlgboxWait = 0;
    memzero( DlgboxText, sizeof( DlgboxText ) );
    FollowType = 0;
    DlgboxButtonsCount = 0;
    DlgboxSelectedButton = 0;
    FollowType = 0;
    FollowRuleId = 0;
    FollowMap = 0;
    PBarterPlayerId = 0;
    PBarterHide = false;

    // Elevator
    ElevatorMainPic = 0;
    ElevatorExtPic = 0;
    ElevatorIndicatorAnim = 0;
    ElevatorButtonPicDown = 0;
    ElevatorButtonsCount = 0;
    ElevatorType = 0;
    ElevatorLevelsCount = 0;
    ElevatorStartLevel = 0;
    ElevatorCurrentLevel = 0;
    ElevatorX = 0;
    ElevatorY = 0;
    ElevatorVectX = 0;
    ElevatorVectY = 0;
    ElevatorSelectedButton = -1;
    ElevatorAnswerDone = false;
    ElevatorSendAnswerTick = 0;

    // Say box
    IfaceLoadRect( SayWMain, "SayMain" );
    IfaceLoadRect( SayWMainText, "SayMainText" );
    IfaceLoadRect( SayWSay, "SaySay" );
    IfaceLoadRect( SayBOk, "SayOk" );
    IfaceLoadRect( SayBOkText, "SayOkText" );
    IfaceLoadRect( SayBCancel, "SayCancel" );
    IfaceLoadRect( SayBCancelText, "SayCancelText" );
    SayX = ( GameOpt.ScreenWidth - SayWMain.W() ) / 2;
    SayY = ( GameOpt.ScreenHeight - SayWMain.H() ) / 2;
    SayVectX = 0;
    SayVectY = 0;
    SayType = DIALOGSAY_NONE;
    SayText = "";

    // Split
    IfaceLoadRect( SplitWMain, "SplitMain" );
    IfaceLoadRect( SplitWTitle, "SplitTitle" );
    IfaceLoadRect( SplitWItem, "SplitItem" );
    IfaceLoadRect( SplitBUp, "SplitUp" );
    IfaceLoadRect( SplitBDown, "SplitDown" );
    IfaceLoadRect( SplitBAll, "SplitAll" );
    IfaceLoadRect( SplitWValue, "SplitValue" );
    IfaceLoadRect( SplitBDone, "SplitDone" );
    IfaceLoadRect( SplitBCancel, "SplitCancel" );
    SplitVectX = 0;
    SplitVectY = 0;
    SplitItemId = 0;
    SplitCont = 0;
    SplitValue = 0;
    SplitMinValue = 0;
    SplitMaxValue = 0;
    SplitValueKeyPressed = false;
    SplitX = ( GameOpt.ScreenWidth - SplitWMain.W() ) / 2;
    SplitY = ( GameOpt.ScreenHeight - SplitWMain.H() ) / 2;
    SplitItemPic = 0;
    SplitItemColor = 0;
    SplitParentScreen = SCREEN_NONE;

    // Timer
    IfaceLoadRect( TimerWMain, "TimerMain" );
    IfaceLoadRect( TimerWTitle, "TimerTitle" );
    IfaceLoadRect( TimerWItem, "TimerItem" );
    IfaceLoadRect( TimerBUp, "TimerUp" );
    IfaceLoadRect( TimerBDown, "TimerDown" );
    IfaceLoadRect( TimerWValue, "TimerValue" );
    IfaceLoadRect( TimerBDone, "TimerDone" );
    IfaceLoadRect( TimerBCancel, "TimerCancel" );
    TimerVectX = 0;
    TimerVectY = 0;
    TimerItemId = 0;
    TimerValue = 0;
    TimerX = ( GameOpt.ScreenWidth - TimerWMain.W() ) / 2;
    TimerY = ( GameOpt.ScreenHeight - TimerWMain.H() ) / 2;
    TimerItemPic = 0;
    TimerItemColor = 0;

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

    // Input box
    IfaceLoadRect( IboxWMain, "IboxMain" );
    IfaceLoadRect( IboxWTitle, "IboxTitle" );
    IfaceLoadRect( IboxWText, "IboxText" );
    IfaceLoadRect( IboxBDone, "IboxDone" );
    IfaceLoadRect( IboxBDoneText, "IboxDoneText" );
    IfaceLoadRect( IboxBCancel, "IboxCancel" );
    IfaceLoadRect( IboxBCancelText, "IboxCancelText" );
    IboxMode = IBOX_MODE_NONE;
    IboxX = ( GameOpt.ScreenWidth - IboxWMain.W() ) / 2;
    IboxY = ( GameOpt.ScreenHeight - IboxWMain.H() ) / 2;
    IboxVectX = 0;
    IboxVectY = 0;
    IboxTitle = "";
    IboxText = "";
    IboxTitleCur = 0;
    IboxTextCur = 0;
    IboxLastKey = 0;
    IboxLastKeyText = "";
    IboxHolodiskId = 0;

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

    // Use
    IfaceLoadSpr( UseWMainPicNone, "UseMainPic" );
    IfaceLoadSpr( UseBCancelPicDown, "UseCancelPicDn" );
    IfaceLoadSpr( UseBScrUpPicDown, "UseScrUpPicDn" );
    IfaceLoadSpr( UseBScrUpPicUp, "UseScrUpPic" );
    IfaceLoadSpr( UseBScrUpPicOff, "UseScrUpPicOff" );
    IfaceLoadSpr( UseBScrDownPicDown, "UseScrDownPicDn" );
    IfaceLoadSpr( UseBScrDownPicUp, "UseScrDownPic" );
    IfaceLoadSpr( UseBScrDownPicOff, "UseScrDownPicOff" );

    // Dialog
    IfaceLoadSpr( DlgPMain, "DlgMainPic" );
    IfaceLoadSpr( DlgPAnsw, "DlgAnswPic" );
    IfaceLoadSpr( DlgPBBarter, "DlgBarterPicDn" );
    IfaceLoadSpr( DlgPBSay, "DlgSayPicDn" );
    DlgAvatarPic = NULL;
    // Barter
    IfaceLoadSpr( BarterPMain, "BarterMainPic" );
    IfaceLoadSpr( BarterPBOfferDn, "BarterOfferPic" );
    IfaceLoadSpr( BarterPBTalkDn, "BarterTalkPic" );
    IfaceLoadSpr( BarterPBC1ScrUpDn, "BarterCont1ScrUpPicDn" );
    IfaceLoadSpr( BarterPBC2ScrUpDn, "BarterCont2ScrUpPicDn" );
    IfaceLoadSpr( BarterPBC1oScrUpDn, "BarterCont1oScrUpPicDn" );
    IfaceLoadSpr( BarterPBC2oScrUpDn, "BarterCont2oScrUpPicDn" );
    IfaceLoadSpr( BarterPBC1ScrDnDn, "BarterCont1ScrDnPicDn" );
    IfaceLoadSpr( BarterPBC2ScrDnDn, "BarterCont2ScrDnPicDn" );
    IfaceLoadSpr( BarterPBC1oScrDnDn, "BarterCont1oScrDnPicDn" );
    IfaceLoadSpr( BarterPBC2oScrDnDn, "BarterCont2oScrDnPicDn" );

    // Cursors
    CurPDef = SprMngr.LoadAnimation( "cursor_default.png", PT_ART_INTRFACE, true );
    CurPHand = SprMngr.LoadAnimation( "cursor_hand.png", PT_ART_INTRFACE, true );
    if( !LMenuOX && !LMenuOY )
    {
        SpriteInfo* si_cur = SprMngr.GetSpriteInfo( CurPDef->GetSprId( 0 ) );
        LMenuOX = ( si_cur ? si_cur->Width : 0 );
    }

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

    // MiniMap
    IfaceLoadSpr( LmapPMain, "LmapMainPic" );
    IfaceLoadSpr( LmapPBOkDw, "LmapOkPicDn" );
    IfaceLoadSpr( LmapPBScanDw, "LmapScanPicDn" );
    IfaceLoadSpr( LmapPBLoHiDw, "LmapLoHiPicDn" );
    LmapPPix = SprMngr.LoadAnimation( "green_pix.png", PT_ART_INTRFACE, true );
    if( !LmapPPix )
        return __LINE__;

    // Town view
    IfaceLoadSpr( TViewWMainPic, "TViewMainPic" );
    IfaceLoadSpr( TViewBBackPicDn, "TViewBackPicDn" );
    IfaceLoadSpr( TViewBEnterPicDn, "TViewEnterPicDn" );
    IfaceLoadSpr( TViewBContoursPicDn, "TViewContoursPicDn" );

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

    // PickUp
    IfaceLoadSpr( PupPMain, "PupMainPic" );
    IfaceLoadSpr( PupPTakeAllOn, "PupTAPicDn" );
    IfaceLoadSpr( PupPBOkOn, "PupOkPicDn" );
    IfaceLoadSpr( PupPBScrUpOn1, "PupScrUp1PicDn" );
    IfaceLoadSpr( PupPBScrUpOff1, "PupScrUp1PicOff" );
    IfaceLoadSpr( PupPBScrDwOn1, "PupScrDw1PicDn" );
    IfaceLoadSpr( PupPBScrDwOff1, "PupScrDw1PicOff" );
    IfaceLoadSpr( PupPBScrUpOn2, "PupScrUp2PicDn" );
    IfaceLoadSpr( PupPBScrUpOff2, "PupScrUp2PicOff" );
    IfaceLoadSpr( PupPBScrDwOn2, "PupScrDw2PicDn" );
    IfaceLoadSpr( PupPBScrDwOff2, "PupScrDw2PicOff" );
    IfaceLoadSpr( PupBNextCritLeftPicUp, "PupNextCritLeftPic" );
    IfaceLoadSpr( PupBNextCritLeftPicDown, "PupNextCritLeftPicDn" );
    IfaceLoadSpr( PupBNextCritRightPicUp, "PupNextCritRightPic" );
    IfaceLoadSpr( PupBNextCritRightPicDown, "PupNextCritRightPicDn" );

    // Pipboy
    IfaceLoadSpr( PipPMain, "PipMainPic" );
    IfaceLoadSpr( PipPWMonitor, "PipMonitorPic" );
    IfaceLoadSpr( PipPBStatusDn, "PipStatusPicDn" );
    // IfaceLoadSpr(PipPBGamesDn,"PipGamesPicDn");
    IfaceLoadSpr( PipPBAutomapsDn, "PipAutomapsPicDn" );
    IfaceLoadSpr( PipPBArchivesDn, "PipArchivesPicDn" );
    IfaceLoadSpr( PipPBCloseDn, "PipClosePicDn" );

    // Aim
    IfaceLoadSpr( AimPWMain, "AimMainPic" );
    IfaceLoadSpr( AimPBCancelDn, "AimCancelPicDn" );

    // Dialog box
    IfaceLoadSpr( DlgboxWTopPicNone, "DlgboxTopPic" );
    IfaceLoadSpr( DlgboxWMiddlePicNone, "DlgboxMiddlePic" );
    IfaceLoadSpr( DlgboxWBottomPicNone, "DlgboxBottomPic" );
    IfaceLoadSpr( DlgboxBButtonPicDown, "DlgboxButtonPicDn" );

    // Say box
    IfaceLoadSpr( SayWMainPicNone, "SayMainPic" );
    IfaceLoadSpr( SayBOkPicDown, "SayOkPicDn" );
    IfaceLoadSpr( SayBCancelPicDown, "SayCancelPicDn" );

    // Split
    IfaceLoadSpr( SplitMainPic, "SplitMainPic" );
    IfaceLoadSpr( SplitPBUpDn, "SplitUpPicDn" );
    IfaceLoadSpr( SplitPBDnDn, "SplitDownPicDn" );
    IfaceLoadSpr( SplitPBAllDn, "SplitAllPicDn" );
    IfaceLoadSpr( SplitPBDoneDn, "SplitDonePic" );
    IfaceLoadSpr( SplitPBCancelDn, "SplitCancelPic" );

    // Timer
    IfaceLoadSpr( TimerMainPic, "TimerMainPic" );
    IfaceLoadSpr( TimerBUpPicDown, "TimerUpPicDn" );
    IfaceLoadSpr( TimerBDownPicDown, "TimerDownPicDn" );
    IfaceLoadSpr( TimerBDonePicDown, "TimerDonePicDn" );
    IfaceLoadSpr( TimerBCancelPicDown, "TimerCancelPicDn" );

    // FixBoy
    IfaceLoadSpr( FixMainPic, "FixMainPic" );
    IfaceLoadSpr( FixPBScrUpDn, "FixScrUpPicDn" );
    IfaceLoadSpr( FixPBScrDnDn, "FixScrDnPicDn" );
    IfaceLoadSpr( FixPBDoneDn, "FixDonePicDn" );
    IfaceLoadSpr( FixPBFixDn, "FixFixPicDn" );

    // Input box
    IfaceLoadSpr( IboxWMainPicNone, "IboxMainPic" );
    IfaceLoadSpr( IboxBDonePicDown, "IboxDonePicDn" );
    IfaceLoadSpr( IboxBCancelPicDown, "IboxCancelPicDn" );

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
                        points[ i ] = PrepPoint( rect[ 0 ], rect[ 1 ] + i * 2, color, NULL, NULL );
                    else
                        points[ i ] = PrepPoint( rect[ 0 ], rect[ 3 ] - i * 2, color, NULL, NULL );
                }
                else
                {
                    if( from_top_or_left )
                        points[ i ] = PrepPoint( rect[ 0 ] + i * 2, rect[ 1 ], color, NULL, NULL );
                    else
                        points[ i ] = PrepPoint( rect[ 2 ] - i * 2, rect[ 1 ], color, NULL, NULL );
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
            AnyFrames* anim = ResMngr.GetInvAnim( item->GetActualPicInv() );
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
    return it != cont.end() ? *it : NULL;
}

void FOClient::CollectContItems()
{
    Item::ClearItems( InvContInit );
    if( Chosen )
    {
        Chosen->GetInvItems( InvContInit );
        for( auto it = InvContInit.begin(), end = InvContInit.end(); it != end; ++it )
            ( *it )->AddRef();
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
                ( *it )->Release();
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

                if( item->IsStackable() )
                    item->ChangeCount( -(int) item_->GetCount() );
                if( !item->IsStackable() || !item->GetCount() )
                {
                    ( *it )->Release();
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
    if( init_items.empty() )
        return;

    // Prepare to call
    if( Script::PrepareContext( ClientFunctions.ItemsCollection, _FUNC_, "Game" ) )
    {
        // Create script array
        ScriptArray* arr = Script::CreateArray( "ItemCl@[]" );
        if( arr )
        {
            // Copy to script array
            Script::AppendVectorToArrayRef( init_items, arr );

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
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::UseDraw()
{
    // Check
    if( !Chosen )
        return;

    // Main
    SprMngr.DrawSprite( UseWMainPicNone, UseX, UseY );

    // Scroll up
    if( UseScroll <= 0 )
        SprMngr.DrawSprite( UseBScrUpPicOff, UseBScrUp[ 0 ] + UseX, UseBScrUp[ 1 ] + UseY );
    else
    {
        if( IfaceHold == IFACE_USE_SCRUP )
            SprMngr.DrawSprite( UseBScrUpPicDown, UseBScrUp[ 0 ] + UseX, UseBScrUp[ 1 ] + UseY );
        else
            SprMngr.DrawSprite( UseBScrUpPicUp, UseBScrUp[ 0 ] + UseX, UseBScrUp[ 1 ] + UseY );
    }

    // Scroll down
    if( UseScroll >= (int) UseCont.size() - UseWInv.H() / UseHeightItem )
        SprMngr.DrawSprite( UseBScrDownPicOff, UseBScrDown[ 0 ] + UseX, UseBScrDown[ 1 ] + UseY );
    else
    {
        if( IfaceHold == IFACE_USE_SCRDW )
            SprMngr.DrawSprite( UseBScrDownPicDown, UseBScrDown[ 0 ] + UseX, UseBScrDown[ 1 ] + UseY );
        else
            SprMngr.DrawSprite( UseBScrDownPicUp, UseBScrDown[ 0 ] + UseX, UseBScrDown[ 1 ] + UseY );
    }

    // Cancel button
    if( IfaceHold == IFACE_USE_CANCEL )
        SprMngr.DrawSprite( UseBCancelPicDown, UseBCancel[ 0 ] + UseX, UseBCancel[ 1 ] + UseY );

    // Chosen
    Chosen->DrawStay( Rect( UseWChosen, UseX, UseY ) );

    // Items
    ContainerDraw( Rect( UseWInv, UseX, UseY ), UseHeightItem, UseScroll, UseCont, 0 );
}

void FOClient::UseLMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( !IsCurInRect( UseWMain, UseX, UseY ) )
        return;

    if( IsCurInRect( UseWInv, UseX, UseY ) )
    {
        UseHoldId = GetCurContainerItemId( Rect( UseWInv, UseX, UseY ), UseHeightItem, UseScroll, UseCont );
        if( UseHoldId && IsCurMode( CUR_HAND ) )
            IfaceHold = IFACE_USE_INV;
        if( UseHoldId && IsCurMode( CUR_DEFAULT ) )
            LMenuTryActivate();
    }
    else if( IsCurInRect( UseBScrUp, UseX, UseY ) )
    {
        Timer::StartAccelerator( ACCELERATE_USE_SCRUP );
        IfaceHold = IFACE_USE_SCRUP;
    }
    else if( IsCurInRect( UseBScrDown, UseX, UseY ) )
    {
        Timer::StartAccelerator( ACCELERATE_USE_SCRDOWN );
        IfaceHold = IFACE_USE_SCRDW;
    }
    else if( IsCurInRect( UseBCancel, UseX, UseY ) )
        IfaceHold = IFACE_USE_CANCEL;
    else
    {
        UseVectX = GameOpt.MouseX - UseX;
        UseVectY = GameOpt.MouseY - UseY;
        IfaceHold = IFACE_USE_MAIN;
    }
}

void FOClient::UseLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_USE_INV:
        if( !IsCurInRect( UseWInv, UseX, UseY ) || !IsCurMode( CUR_HAND ) || !UseHoldId )
            break;

        if( ShowScreenType )
        {
            if( ShowScreenNeedAnswer )
                Net_SendScreenAnswer( UseHoldId, "" );
        }
        else
        {
            if( UseSelect.IsCritter() )
            {
                AddActionBack( CHOSEN_USE_ITEM, UseHoldId, 0, TARGET_CRITTER, UseSelect.GetId(), USE_USE );
            }
            else if( UseSelect.IsItem() )
            {
                ItemHex* item = GetItem( UseSelect.GetId() );
                if( item )
                    AddActionBack( CHOSEN_USE_ITEM, UseHoldId, 0, item->IsItem() ? TARGET_ITEM : TARGET_SCENERY, UseSelect.GetId(), USE_USE );
            }
        }

        ShowScreen( SCREEN_NONE );
        break;
    case IFACE_USE_SCRUP:
        if( !IsCurInRect( UseBScrUp, UseX, UseY ) || UseScroll <= 0 )
            break;
        UseScroll--;
        break;
    case IFACE_USE_SCRDW:
        if( !IsCurInRect( UseBScrDown, UseX, UseY ) || UseScroll >= (int) UseCont.size() - UseWInv.H() / UseHeightItem )
            break;
        UseScroll++;
        break;
    case IFACE_USE_CANCEL:
        if( !IsCurInRect( UseBCancel, UseX, UseY ) )
            break;
        ShowScreen( SCREEN_NONE );
        break;
    default:
        break;
    }

    UseHoldId = 0;
    IfaceHold = IFACE_NONE;
}

void FOClient::UseRMouseDown()
{
    if( IfaceHold == IFACE_NONE )
        SetCurCastling( CUR_DEFAULT, CUR_HAND );
}

void FOClient::UseMouseMove()
{
    if( IfaceHold == IFACE_USE_MAIN )
    {
        UseX = GameOpt.MouseX - UseVectX;
        UseY = GameOpt.MouseY - UseVectY;
        if( UseX < 0 )
            UseX = 0;
        if( UseX + UseWMain[ 2 ] > GameOpt.ScreenWidth )
            UseX = GameOpt.ScreenWidth - UseWMain[ 2 ];
        if( UseY < 0 )
            UseY = 0;
        if( UseY + UseWMain[ 3 ] > GameOpt.ScreenHeight )
            UseY = GameOpt.ScreenHeight - UseWMain[ 3 ];
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
        CritterCl* cr = ( *it ).second;

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
        ActionEvent* act = ( IsAction( CHOSEN_MOVE ) ? &ChosenAction[ 0 ] : NULL );
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
            CritterCl* cr = NULL;
            ItemHex*   item = NULL;
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

void FOClient::ConnectToGame()
{
    if( !Singleplayer )
    {
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
            return;
        }

        if( !Str::IsValidUTF8( GameOpt.Name->c_str() ) || Str::Substring( GameOpt.Name->c_str(), "*" ) )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_NAME_WRONG_CHARS ) );
            return;
        }

        uint pass_len_utf8 = Str::LengthUTF8( Password.c_str() );
        if( pass_len_utf8 < MIN_NAME || pass_len_utf8 < GameOpt.MinNameLength ||
            pass_len_utf8 > MAX_NAME || pass_len_utf8 > GameOpt.MaxNameLength )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_WRONG_PASS ) );
            return;
        }

        if( !Str::IsValidUTF8( Password.c_str() ) || Str::Substring( Password.c_str(), "*" ) )
        {
            AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_PASS_WRONG_CHARS ) );
            return;
        }

        // Save login and password
        Crypt.SetCache( "__name", (uchar*) GameOpt.Name->c_str(), (uint) GameOpt.Name->length() + 1 );
        Crypt.SetCache( "__pass", (uchar*) Password.c_str(), (uint) Password.length() + 1 );

        AddMess( FOMB_GAME, MsgGame->GetStr( STR_NET_CONNECTION ) );
    }

    // Connect to server
    InitNetReason = INIT_NET_REASON_LOGIN;
    ScreenEffects.clear();
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::DlgDraw( bool is_dialog )
{
    SprMngr.DrawSprite( DlgPMain, DlgX, DlgY );
    if( !BarterIsPlayers && DlgAvatarPic )
        SprMngr.DrawSpriteSize( DlgAvatarPic, DlgWAvatar.L + DlgX, DlgWAvatar.T + DlgY, DlgWAvatar.W(), DlgWAvatar.H(), true, true );
    if( !Chosen )
        return;

    // Main pic
    if( is_dialog )
        SprMngr.DrawSprite( DlgPAnsw, DlgAnsw[ 0 ] + DlgX, DlgAnsw[ 1 ] + DlgY );
    else
        SprMngr.DrawSprite( BarterPMain, BarterWMain[ 0 ] + DlgX, BarterWMain[ 1 ] + DlgY );

    // Text scroll
    const char scr_up[] = { 24, 0 };
    const char scr_down[] = { 25, 0 };
    if( DlgMainTextCur )
        SprMngr.DrawStr( Rect( DlgBScrUp, DlgX, DlgY ), scr_up, 0, IfaceHold == IFACE_DLG_SCR_UP ? COLOR_TEXT_DGREEN : COLOR_TEXT );
    if( DlgMainTextCur < DlgMainTextLinesReal - DlgMainTextLinesRect )
        SprMngr.DrawStr( Rect( DlgBScrDn, DlgX, DlgY ), scr_down, 0, IfaceHold == IFACE_DLG_SCR_DN ? COLOR_TEXT_DGREEN : COLOR_TEXT );

    // Dialog
    if( is_dialog )
    {
        switch( IfaceHold )
        {
        case IFACE_DLG_BARTER:
            SprMngr.DrawSprite( DlgPBBarter, DlgBBarter[ 0 ] + DlgX, DlgBBarter[ 1 ] + DlgY );
            break;
        case IFACE_DLG_SAY:
            SprMngr.DrawSprite( DlgPBSay, DlgBSay[ 0 ] + DlgX, DlgBSay[ 1 ] + DlgY );
            break;
        default:
            break;
        }

        // Texts
        SprMngr.DrawStr( Rect( DlgBBarterText, DlgX, DlgY ), MsgGame->GetStr( STR_DIALOG_BARTER ), FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
        SprMngr.DrawStr( Rect( DlgBSayText, DlgX, DlgY ), MsgGame->GetStr( STR_DIALOG_SAY ), FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );

        // Npc text
        SprMngr.DrawStr( Rect( DlgWText, DlgX, DlgY ), DlgMainText.c_str(), FT_SKIPLINES( DlgMainTextCur ), COLOR_TEXT );

        // Answers
        for( uint i = 0; i < DlgAnswers.size(); i++ )
        {
            Answer& a = DlgAnswers[ i ];
            if( i == (uint) DlgCurAnsw )
                SprMngr.DrawStr( Rect( a.Position, DlgX, DlgY ), DlgAnswers[ i ].Text.c_str(), a.AnswerNum < 0 ? FT_CENTERX : 0, IfaceHold == IFACE_DLG_ANSWER && DlgCurAnsw == DlgHoldAnsw ? COLOR_TEXT_DDGREEN : ( IfaceHold != IFACE_DLG_ANSWER ? COLOR_TEXT_DGREEN : COLOR_TEXT ) );
            else
                SprMngr.DrawStr( Rect( a.Position, DlgX, DlgY ), DlgAnswers[ i ].Text.c_str(), a.AnswerNum < 0 ? FT_CENTERX : 0, COLOR_TEXT );
        }

        // Chosen money
        // if( Chosen )
        //    SprMngr.DrawStr( Rect( DlgWMoney, DlgX, DlgY ), Chosen->GetMoneyStr(), FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
    }
    // Barter
    else
    {
        if( BarterIsPlayers && BarterOffer )
            SprMngr.DrawSprite( BarterPBOfferDn, BarterBOffer[ 0 ] + DlgX, BarterBOffer[ 1 ] + DlgY );

        switch( IfaceHold )
        {
        case IFACE_BARTER_OFFER:
            SprMngr.DrawSprite( BarterPBOfferDn, BarterBOffer[ 0 ] + DlgX, BarterBOffer[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_TALK:
            SprMngr.DrawSprite( BarterPBTalkDn, BarterBTalk[ 0 ] + DlgX, BarterBTalk[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT1SU:
            SprMngr.DrawSprite( BarterPBC1ScrUpDn, BarterBCont1ScrUp[ 0 ] + DlgX, BarterBCont1ScrUp[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT1SD:
            SprMngr.DrawSprite( BarterPBC1ScrDnDn, BarterBCont1ScrDn[ 0 ] + DlgX, BarterBCont1ScrDn[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT2SU:
            SprMngr.DrawSprite( BarterPBC2ScrUpDn, BarterBCont2ScrUp[ 0 ] + DlgX, BarterBCont2ScrUp[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT2SD:
            SprMngr.DrawSprite( BarterPBC2ScrDnDn, BarterBCont2ScrDn[ 0 ] + DlgX, BarterBCont2ScrDn[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT1OSU:
            SprMngr.DrawSprite( BarterPBC1oScrUpDn, BarterBCont1oScrUp[ 0 ] + DlgX, BarterBCont1oScrUp[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT1OSD:
            SprMngr.DrawSprite( BarterPBC1oScrDnDn, BarterBCont1oScrDn[ 0 ] + DlgX, BarterBCont1oScrDn[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT2OSU:
            SprMngr.DrawSprite( BarterPBC2oScrUpDn, BarterBCont2oScrUp[ 0 ] + DlgX, BarterBCont2oScrUp[ 1 ] + DlgY );
            break;
        case IFACE_BARTER_CONT2OSD:
            SprMngr.DrawSprite( BarterPBC2oScrDnDn, BarterBCont2oScrDn[ 0 ] + DlgX, BarterBCont2oScrDn[ 1 ] + DlgY );
            break;
        default:
            break;
        }

        Chosen->DrawStay( Rect( BarterWChosen, DlgX, DlgY ) );
        CritterCl* cr = GetCritter( BarterIsPlayers ? BarterOpponentId : DlgNpcId );
        if( cr )
            cr->DrawStay( Rect( BarterWCritter, DlgX, DlgY ) );

        SprMngr.Flush();

        // Items
        ContainerDraw( Rect( BarterWCont1, DlgX, DlgY ), BarterCont1HeightItem, BarterScroll1, BarterCont1, IfaceHold == IFACE_BARTER_CONT1 ? BarterHoldId : 0 );
        ContainerDraw( Rect( BarterWCont2, DlgX, DlgY ), BarterCont2HeightItem, BarterScroll2, BarterCont2, IfaceHold == IFACE_BARTER_CONT2 ? BarterHoldId : 0 );
        ContainerDraw( Rect( BarterWCont1o, DlgX, DlgY ), BarterCont1oHeightItem, BarterScroll1o, BarterCont1o, IfaceHold == IFACE_BARTER_CONT1O ? BarterHoldId : 0 );
        ContainerDraw( Rect( BarterWCont2o, DlgX, DlgY ), BarterCont2oHeightItem, BarterScroll2o, BarterCont2o, IfaceHold == IFACE_BARTER_CONT2O ? BarterHoldId : 0 );

        // Info
        SprMngr.DrawStr( Rect( BarterBOfferText, DlgX, DlgY ), MsgGame->GetStr( STR_BARTER_OFFER ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
        if( BarterIsPlayers )
        {
            SprMngr.DrawStr( Rect( BarterBTalkText, DlgX, DlgY ), MsgGame->GetStr( STR_BARTER_END ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
            SprMngr.DrawStr( Rect( DlgWText, DlgX, DlgY ), BarterText.c_str(), FT_UPPER );
        }
        else
        {
            SprMngr.DrawStr( Rect( BarterBTalkText, DlgX, DlgY ), MsgGame->GetStr( STR_BARTER_TALK ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
            SprMngr.DrawStr( Rect( DlgWText, DlgX, DlgY ), BarterText.c_str(), 0 );
        }

        // Cost
        uint c1, w1, v1, c2, w2, v2;
        ContainerCalcInfo( BarterCont1o, c1, w1, v1, -BarterK, true );
        ContainerCalcInfo( BarterCont2o, c2, w2, v2, Chosen->GetPerkMasterTrader() ? 0 : BarterK, false );
        if( !BarterIsPlayers && BarterK )
        {
            SprMngr.DrawStr( Rect( BarterWCost1, DlgX, DlgY ), Str::FormatBuf( "$%u", c1 ), FT_NOBREAK | FT_CENTERX, COLOR_TEXT_WHITE ); // BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
            SprMngr.DrawStr( Rect( BarterWCost2, DlgX, DlgY ), Str::FormatBuf( "$%u", c2 ), FT_NOBREAK | FT_CENTERX, COLOR_TEXT_WHITE ); // BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
        }
        else
        {
            SprMngr.DrawStr( Rect( BarterWCost1, DlgX, DlgY ), Str::FormatBuf( "%u", w1 / 1000 ), FT_NOBREAK | FT_CENTERX, COLOR_TEXT_WHITE ); // BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
            SprMngr.DrawStr( Rect( BarterWCost2, DlgX, DlgY ), Str::FormatBuf( "%u", w2 / 1000 ), FT_NOBREAK | FT_CENTERX, COLOR_TEXT_WHITE ); // BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
        }
        // Overweight, oversize indicate
        if( Chosen->GetFreeWeight() + (int) w1 < (int) w2 )
            SprMngr.DrawStr( Rect( DlgWText.L, DlgWText.B - 5, DlgWText.R, DlgWText.B + 10, DlgX, DlgY ), MsgGame->GetStr( STR_OVERWEIGHT_TITLE ), FT_NOBREAK | FT_CENTERX, COLOR_TEXT_DDGREEN );
        else if( Chosen->GetFreeVolume() + (int) v1 < (int) v2 )
            SprMngr.DrawStr( Rect( DlgWText.L, DlgWText.B - 5, DlgWText.R, DlgWText.B + 10, DlgX, DlgY ), MsgGame->GetStr( STR_OVERVOLUME_TITLE ), FT_NOBREAK | FT_CENTERX, COLOR_TEXT_DDGREEN );
    }

    // Timer
    if( !BarterIsPlayers && DlgEndTick && DlgEndTick > Timer::GameTick() )
        SprMngr.DrawStr( Rect( DlgWTimer, DlgX, DlgY ), Str::FormatBuf( "%u", ( DlgEndTick - Timer::GameTick() ) / 1000 ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_DGREEN );
}

void FOClient::DlgLMouseDown( bool is_dialog )
{
    IfaceHold = IFACE_NONE;
    BarterHoldId = 0;

    if( !IsCurInRect( DlgWMain, DlgX, DlgY ) )
        return;

    if( IsCurInRect( DlgBScrUp, DlgX, DlgY ) )
    {
        if( DlgMainTextCur > 0 )
        {
            IfaceHold = IFACE_DLG_SCR_UP;
            Timer::StartAccelerator( ACCELERATE_DLG_TEXT_UP );
        }
    }
    else if( IsCurInRect( DlgBScrDn, DlgX, DlgY ) )
    {
        if( DlgMainTextCur < DlgMainTextLinesReal - DlgMainTextLinesRect )
        {
            IfaceHold = IFACE_DLG_SCR_DN;
            Timer::StartAccelerator( ACCELERATE_DLG_TEXT_DOWN );
        }
    }

    // Dialog
    if( is_dialog && IsCurInRect( DlgWMain, DlgX, DlgY ) )
    {
        if( IsCurInRect( DlgAnswText, DlgX, DlgY ) )
        {
            IfaceHold = IFACE_DLG_ANSWER;
            DlgHoldAnsw = DlgCurAnsw;
        }
        else if( IsCurInRect( DlgBBarter, DlgX, DlgY ) )
            IfaceHold = IFACE_DLG_BARTER;
        else if( IsCurInRect( DlgBSay, DlgX, DlgY ) )
            IfaceHold = IFACE_DLG_SAY;
    }
    // Barter
    else if( !is_dialog && IsCurInRect( BarterWMain, DlgX, DlgY ) )
    {
        if( IsCurInRect( BarterWCont1, DlgX, DlgY ) )
        {
            BarterHoldId = GetCurContainerItemId( Rect( BarterWCont1, DlgX, DlgY ), BarterCont1HeightItem, BarterScroll1, BarterCont1 );
            if( !BarterHoldId )
                return;
            IfaceHold = IFACE_BARTER_CONT1;
        }
        else if( IsCurInRect( BarterWCont2, DlgX, DlgY ) )
        {
            if( !( BarterIsPlayers && BarterOpponentHide ) )
            {
                BarterHoldId = GetCurContainerItemId( Rect( BarterWCont2, DlgX, DlgY ), BarterCont2HeightItem, BarterScroll2, BarterCont2 );
                if( !BarterHoldId )
                    return;
                IfaceHold = IFACE_BARTER_CONT2;
            }
        }
        else if( IsCurInRect( BarterWCont1o, DlgX, DlgY ) )
        {
            BarterHoldId = GetCurContainerItemId( Rect( BarterWCont1o, DlgX, DlgY ), BarterCont1oHeightItem, BarterScroll1o, BarterCont1o );
            if( !BarterHoldId )
                return;
            IfaceHold = IFACE_BARTER_CONT1O;
        }
        else if( IsCurInRect( BarterWCont2o, DlgX, DlgY ) )
        {
            if( !( BarterIsPlayers && BarterOpponentHide ) )
            {
                BarterHoldId = GetCurContainerItemId( Rect( BarterWCont2o, DlgX, DlgY ), BarterCont2oHeightItem, BarterScroll2o, BarterCont2o );
                if( !BarterHoldId )
                    return;
                IfaceHold = IFACE_BARTER_CONT2O;
            }
        }
        else if( IsCurInRect( BarterBOffer, DlgX, DlgY ) )
            IfaceHold = IFACE_BARTER_OFFER;
        else if( IsCurInRect( BarterBTalk, DlgX, DlgY ) )
            IfaceHold = IFACE_BARTER_TALK;
        else if( IsCurInRect( BarterBCont1ScrUp, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT1SU );
            IfaceHold = IFACE_BARTER_CONT1SU;
        }
        else if( IsCurInRect( BarterBCont1ScrDn, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT1SD );
            IfaceHold = IFACE_BARTER_CONT1SD;
        }
        else if( IsCurInRect( BarterBCont2ScrUp, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT2SU );
            IfaceHold = IFACE_BARTER_CONT2SU;
        }
        else if( IsCurInRect( BarterBCont2ScrDn, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT2SD );
            IfaceHold = IFACE_BARTER_CONT2SD;
        }
        else if( IsCurInRect( BarterBCont1oScrUp, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT1OSU );
            IfaceHold = IFACE_BARTER_CONT1OSU;
        }
        else if( IsCurInRect( BarterBCont1oScrDn, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT1OSD );
            IfaceHold = IFACE_BARTER_CONT1OSD;
        }
        else if( IsCurInRect( BarterBCont2oScrUp, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT2OSU );
            IfaceHold = IFACE_BARTER_CONT2OSU;
        }
        else if( IsCurInRect( BarterBCont2oScrDn, DlgX, DlgY ) )
        {
            Timer::StartAccelerator( ACCELERATE_BARTER_CONT2OSD );
            IfaceHold = IFACE_BARTER_CONT2OSD;
        }
        else if( IsCurInRect( DlgBScrUp, DlgX, DlgY ) && BarterIsPlayers )
            IfaceHold = IFACE_DLG_SCR_UP;
        else if( IsCurInRect( DlgBScrDn, DlgX, DlgY ) && BarterIsPlayers )
            IfaceHold = IFACE_DLG_SCR_DN;

        if( IsCurMode( CUR_DEFAULT ) && ( IfaceHold == IFACE_BARTER_CONT1 || IfaceHold == IFACE_BARTER_CONT2 || IfaceHold == IFACE_BARTER_CONT1O || IfaceHold == IFACE_BARTER_CONT2O ) )
        {
            IfaceHold = IFACE_NONE;
            LMenuTryActivate();
            return;
        }
    }

    if( IfaceHold == IFACE_NONE )
    {
        DlgVectX = GameOpt.MouseX - DlgX;
        DlgVectY = GameOpt.MouseY - DlgY;
        IfaceHold = IFACE_DLG_MAIN;
    }
}

void FOClient::DlgLMouseUp( bool is_dialog )
{
    if( !Chosen )
        return;

    if( IfaceHold == IFACE_DLG_SCR_UP && IsCurInRect( DlgBScrUp, DlgX, DlgY ) )
    {
        if( DlgMainTextCur > 0 )
            DlgMainTextCur--;
    }
    else if( IfaceHold == IFACE_DLG_SCR_DN && IsCurInRect( DlgBScrDn, DlgX, DlgY ) )
    {
        if( DlgMainTextCur < DlgMainTextLinesReal - DlgMainTextLinesRect )
            DlgMainTextCur++;
    }

    if( is_dialog )
    {
        if( IfaceHold == IFACE_DLG_ANSWER && IsCurInRect( DlgAnswText, DlgX, DlgY ) )
        {
            if( DlgCurAnsw == DlgHoldAnsw && DlgCurAnsw >= 0 && DlgCurAnsw < (int) DlgAnswers.size() && IsCurInRect( DlgAnswers[ DlgCurAnsw ].Position, DlgX, DlgY ) )
            {
                if( DlgAnswers[ DlgCurAnsw ].AnswerNum < 0 )
                {
                    DlgCollectAnswers( DlgAnswers[ DlgCurAnsw ].AnswerNum == -2 );
                }
                else
                {
                    Net_SendTalk( DlgIsNpc, DlgNpcId, DlgAnswers[ DlgCurAnsw ].AnswerNum );
                    WaitPing();
                }
            }
        }
        else if( IfaceHold == IFACE_DLG_BARTER && IsCurInRect( DlgBBarter, DlgX, DlgY ) )
        {
            CritterCl* cr = GetCritter( DlgNpcId );
            if( !cr || cr->GetIsNoBarter() )
            {
                DlgMainText = MsgGame->GetStr( STR_BARTER_NO_BARTER_MODE );
                DlgMainTextCur = 0;
                DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, DlgMainText.c_str() );
            }
            else
            {
                Net_SendTalk( DlgIsNpc, DlgNpcId, ANSWER_BARTER );
                WaitPing();
                HideScreen( SCREEN__DIALOG );
                ShowScreen( SCREEN__BARTER );
                CollectContItems();
                Item::ClearItems( BarterCont1o );
                Item::ClearItems( BarterCont2 );
                Item::ClearItems( BarterCont2o );
                BarterText = "";
                DlgMainTextCur = 0;
                DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
            }
        }
        else if( IfaceHold == IFACE_DLG_SAY && IsCurInRect( DlgBSay, DlgX, DlgY ) )
        {
            ShowScreen( SCREEN__SAY );
            SayType = DIALOGSAY_TEXT;
            SayText = "";
        }
    }
    else
    {
        if( IfaceHold == IFACE_BARTER_CONT1 )
        {
            if( IsCurInRect( BarterWCont1o, DlgX, DlgY ) )
            {
                auto it = PtrCollectionFind( BarterCont1.begin(), BarterCont1.end(), BarterHoldId );
                if( it != BarterCont1.end() )
                {
                    Item* item = *it;
                    if( item->GetCount() > 1 )
                        SplitStart( item, IFACE_BARTER_CONT1 );
                    else
                        BarterTransfer( BarterHoldId, IFACE_BARTER_CONT1, item->GetCount() );
                }
            }
        }
        else if( IfaceHold == IFACE_BARTER_CONT2 )
        {
            if( IsCurInRect( BarterWCont2o, DlgX, DlgY ) && !( BarterIsPlayers && BarterOpponentHide ) )
            {
                auto it = PtrCollectionFind( BarterCont2.begin(), BarterCont2.end(), BarterHoldId );
                if( it != BarterCont2.end() )
                {
                    Item* item = *it;
                    if( item->GetCount() > 1 )
                        SplitStart( item, IFACE_BARTER_CONT2 );
                    else
                        BarterTransfer( BarterHoldId, IFACE_BARTER_CONT2, item->GetCount() );
                }
            }
        }
        else if( IfaceHold == IFACE_BARTER_CONT1O )
        {
            if( IsCurInRect( BarterWCont1, DlgX, DlgY ) )
            {
                auto it = PtrCollectionFind( BarterCont1o.begin(), BarterCont1o.end(), BarterHoldId );
                if( it != BarterCont1o.end() )
                {
                    Item* item = *it;
                    if( item->GetCount() > 1 )
                        SplitStart( item, IFACE_BARTER_CONT1O );
                    else
                        BarterTransfer( BarterHoldId, IFACE_BARTER_CONT1O, item->GetCount() );
                }
            }
        }
        else if( IfaceHold == IFACE_BARTER_CONT2O )
        {
            if( IsCurInRect( BarterWCont2, DlgX, DlgY ) && !( BarterIsPlayers && BarterOpponentHide ) )
            {
                auto it = PtrCollectionFind( BarterCont2o.begin(), BarterCont2o.end(), BarterHoldId );
                if( it != BarterCont2o.end() )
                {
                    Item* item = *it;
                    if( item->GetCount() > 1 )
                        SplitStart( item, IFACE_BARTER_CONT2O );
                    else
                        BarterTransfer( BarterHoldId, IFACE_BARTER_CONT2O, item->GetCount() );
                }
            }
        }
        else if( IfaceHold == IFACE_BARTER_OFFER && IsCurInRect( BarterBOffer, DlgX, DlgY ) )
        {
            BarterTryOffer();
        }
        else if( IfaceHold == IFACE_BARTER_TALK && IsCurInRect( BarterBTalk, DlgX, DlgY ) )
        {
            if( BarterIsPlayers )
            {
                Net_SendPlayersBarter( BARTER_END, 0, 0 );
                ShowScreen( SCREEN_NONE );
            }
            else
            {
                Net_SendTalk( DlgIsNpc, DlgNpcId, ANSWER_BEGIN );
                WaitPing();
            }
            CollectContItems();
        }
        else if( IfaceHold == IFACE_BARTER_CONT1SU && IsCurInRect( BarterBCont1ScrUp, DlgX, DlgY ) )
        {
            if( BarterScroll1 > 0 )
                BarterScroll1--;
        }
        else if( IfaceHold == IFACE_BARTER_CONT1SD && IsCurInRect( BarterBCont1ScrDn, DlgX, DlgY ) )
        {
            if( BarterScroll1 < (int) Chosen->GetItemsCountInv() - ( BarterWCont1[ 3 ] - BarterWCont1[ 1 ] ) / BarterCont1HeightItem - (int) BarterCont1o.size() )
                BarterScroll1++;
        }
        else if( IfaceHold == IFACE_BARTER_CONT2SU && IsCurInRect( BarterBCont2ScrUp, DlgX, DlgY ) )
        {
            if( BarterScroll2 > 0 )
                BarterScroll2--;
        }
        else if( IfaceHold == IFACE_BARTER_CONT2SD && IsCurInRect( BarterBCont2ScrDn, DlgX, DlgY ) )
        {
            if( BarterScroll2 < (int) BarterCont2.size() - ( BarterWCont2[ 3 ] - BarterWCont2[ 1 ] ) / BarterCont2HeightItem )
                BarterScroll2++;
        }
        else if( IfaceHold == IFACE_BARTER_CONT1OSU && IsCurInRect( BarterBCont1oScrUp, DlgX, DlgY ) )
        {
            if( BarterScroll1o > 0 )
                BarterScroll1o--;
        }
        else if( IfaceHold == IFACE_BARTER_CONT1OSD && IsCurInRect( BarterBCont1oScrDn, DlgX, DlgY ) )
        {
            if( BarterScroll1o < (int) BarterCont1o.size() - ( BarterWCont1o[ 3 ] - BarterWCont1o[ 1 ] ) / BarterCont1oHeightItem )
                BarterScroll1o++;
        }
        else if( IfaceHold == IFACE_BARTER_CONT2OSU && IsCurInRect( BarterBCont2oScrUp, DlgX, DlgY ) )
        {
            if( BarterScroll2o > 0 )
                BarterScroll2o--;
        }
        else if( IfaceHold == IFACE_BARTER_CONT2OSD && IsCurInRect( BarterBCont2oScrDn, DlgX, DlgY ) )
        {
            if( BarterScroll2o < (int) BarterCont2o.size() - ( BarterWCont2o[ 3 ] - BarterWCont2o[ 1 ] ) / BarterCont2oHeightItem )
                BarterScroll2o++;
        }
    }

    IfaceHold = IFACE_NONE;
    BarterHoldId = 0;
}

void FOClient::DlgMouseMove( bool is_dialog )
{
    if( is_dialog )
    {
        DlgCurAnsw = -1;
        for( uint i = 0; i < DlgAnswers.size(); i++ )
        {
            if( IsCurInRect( DlgAnswers[ i ].Position, DlgX, DlgY ) )
            {
                DlgCurAnsw = i;
                break;
            }
        }
    }

    if( IfaceHold == IFACE_DLG_MAIN )
    {
        DlgX = GameOpt.MouseX - DlgVectX;
        DlgY = GameOpt.MouseY - DlgVectY;

        if( DlgX < 0 )
            DlgX = 0;
        if( DlgX + DlgWMain[ 2 ] > GameOpt.ScreenWidth )
            DlgX = GameOpt.ScreenWidth - DlgWMain[ 2 ];
        if( DlgY < 0 )
            DlgY = 0;
        if( DlgY + DlgWMain[ 3 ] > GameOpt.ScreenHeight )
            DlgY = GameOpt.ScreenHeight - DlgWMain[ 3 ];
    }
}

void FOClient::DlgRMouseDown( bool is_dialog )
{
    if( !is_dialog )
        SetCurCastling( CUR_DEFAULT, CUR_HAND );
}

void FOClient::DlgKeyDown( bool is_dialog, uchar dik, const char* dik_text )
{
    int num = -1;
    switch( dik )
    {
    case DIK_ESCAPE:
    case DIK_0:
    case DIK_NUMPAD0:
        if( BarterIsPlayers )
        {
            Net_SendPlayersBarter( BARTER_END, 0, 0 );
            ShowScreen( SCREEN_NONE );
        }
        else
        {
            Net_SendTalk( DlgIsNpc, DlgNpcId, ANSWER_END );
            WaitPing();
        }
        CollectContItems();
        return;
    case DIK_INSERT:
        BarterTryOffer();
        return;
    case DIK_1:
    case DIK_NUMPAD1:
        num = 0;
        break;
    case DIK_2:
    case DIK_NUMPAD2:
        num = 1;
        break;
    case DIK_3:
    case DIK_NUMPAD3:
        num = 2;
        break;
    case DIK_4:
    case DIK_NUMPAD4:
        num = 3;
        break;
    case DIK_5:
    case DIK_NUMPAD5:
        num = 4;
        break;
    case DIK_6:
    case DIK_NUMPAD6:
        num = 5;
        break;
    case DIK_7:
    case DIK_NUMPAD7:
        num = 6;
        break;
    case DIK_8:
    case DIK_NUMPAD8:
        num = 7;
        break;
    case DIK_9:
    case DIK_NUMPAD9:
        num = 8;
        break;
    case DIK_UP:
        DlgCollectAnswers( false );
        return;
    case DIK_DOWN:
        DlgCollectAnswers( true );
        return;
    default:
        return;
    }

    if( is_dialog && num < (int) DlgAnswers.size() )
    {
        if( DlgAnswers[ num ].AnswerNum < 0 )
        {
            DlgCollectAnswers( DlgAnswers[ num ].AnswerNum == -2 );
        }
        else
        {
            Net_SendTalk( DlgIsNpc, DlgNpcId, DlgAnswers[ num ].AnswerNum );
            WaitPing();
        }
    }
}

void FOClient::DlgCollectAnswers( bool next )
{
    if( next && DlgCurAnswPage < DlgMaxAnswPage )
        DlgCurAnswPage++;
    if( !next && DlgCurAnswPage > 0 )
        DlgCurAnswPage--;

    DlgAnswers.clear();
    for( uint i = 0, j = (uint) DlgAllAnswers.size(); i < j; i++ )
    {
        Answer& a = DlgAllAnswers[ i ];
        if( a.Page == DlgCurAnswPage )
            DlgAnswers.push_back( a );
    }
    DlgMouseMove( true );
}

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
        uint c1, w1, v1, c2, w2, v2;
        ContainerCalcInfo( BarterCont1oInit, c1, w1, v1, -BarterK, true );
        ContainerCalcInfo( BarterCont2oInit, c2, w2, v2, Chosen->GetPerkMasterTrader() ? 0 : BarterK, false );
        if( !c1 && !c2 && BarterK )
            return;
        if( c1 < c2 && BarterK )
            BarterText = MsgGame->GetStr( STR_BARTER_BAD_OFFER );
        else if( Chosen->GetFreeWeight() + w1 < w2 )
            BarterText = MsgGame->GetStr( STR_BARTER_OVERWEIGHT );
        else if( Chosen->GetFreeVolume() + v1 < v2 )
            BarterText = MsgGame->GetStr( STR_BARTER_OVERSIZE );
        else
        {
            Net_SendBarter( DlgNpcId, BarterCont1oInit, BarterCont2oInit );
            WaitPing();
            return;
        }
        DlgMainTextCur = 0;
        DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
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
    case IFACE_BARTER_CONT1:
        from_cont = &InvContInit;
        to_cont = &BarterCont1oInit;
        break;
    case IFACE_BARTER_CONT2:
        from_cont = &BarterCont2Init;
        to_cont = &BarterCont2oInit;
        break;
    case IFACE_BARTER_CONT1O:
        from_cont = &BarterCont1oInit;
        to_cont = &InvContInit;
        break;
    case IFACE_BARTER_CONT2O:
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
    Item* to_item = NULL;

    if( item->GetCount() < item_count )
        return;

    if( item->IsStackable() )
    {
        auto it_to = PtrCollectionFind( to_cont->begin(), to_cont->end(), item->GetId() );
        if( it_to != to_cont->end() )
            to_item = ( *it_to );
    }

    if( to_item )
    {
        to_item->ChangeCount( item_count );
    }
    else
    {
        item->AddRef();
        to_cont->push_back( item );
        Item* last = ( *to_cont )[ to_cont->size() - 1 ];
        last->SetCount( item_count );
    }

    item->ChangeCount( -(int) item_count );
    if( !item->GetCount() || !item->IsStackable() )
    {
        ( *it )->Release();
        from_cont->erase( it );
    }
    CollectContItems();

    switch( item_cont )
    {
    case IFACE_BARTER_CONT1:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_SET_SELF, item_id, item_count );
        break;
    case IFACE_BARTER_CONT2:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_SET_OPPONENT, item_id, item_count );
        break;
    case IFACE_BARTER_CONT1O:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_UNSET_SELF, item_id, item_count );
        break;
    case IFACE_BARTER_CONT2O:
        if( BarterIsPlayers )
            Net_SendPlayersBarter( BARTER_UNSET_OPPONENT, item_id, item_count );
        break;
    default:
        break;
    }
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
                    Script::SetArgObject( item );
                    Script::SetArgObject( Chosen );
                    Script::SetArgObject( npc );
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
        weigth += item->GetWeight();
        volume += item->GetVolume();
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::FormatTags( char* text, uint text_len, CritterCl* player, CritterCl* npc, const char* lexems )
{
    if( Str::CompareCase( text, "error" ) )
    {
        Str::Copy( text, text_len, "Text not found!" );
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
                    {
                        if( Str::Length( str ) + Str::Length( tag ) < text_len )
                            Str::Insert( &str[ i ], tag );
                    }
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
                uint bind_id = Script::Bind( "client_main", func_name, "string %s(string&)", true );
                Str::Copy( tag, "<script function not found>" );
                if( bind_id && Script::PrepareContext( bind_id, _FUNC_, "Game" ) )
                {
                    ScriptString* script_lexems = ScriptString::Create( lexems ? lexems : "" );
                    Script::SetArgObject( script_lexems );
                    if( Script::RunPrepared() )
                    {
                        ScriptString* result = (ScriptString*) Script::GetReturnedObject();
                        if( result )
                            Str::Copy( tag, result->c_str() );
                    }
                    script_lexems->Release();
                }
            }
            // Error
            else
            {
                Str::Copy( tag, "<error>" );
            }

            if( Str::Length( str ) + Str::Length( tag ) < text_len )
                Str::Insert( str + i, tag );
        }
            continue;
        default:
            break;
        }

        ++i;
    }

    dialogs.push_back( str );
    Str::Copy( text, text_len, dialogs[ Random( 0, (uint) dialogs.size() - 1 ) ] );
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
    LMenuCurNodes = NULL;
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
            {
                if( GetActiveScreen() == SCREEN__BARTER )
                {
                    if( BarterIsPlayers )
                    {
                        const char* str = FmtItemLook( cont_item, ITEM_LOOK_DEFAULT );
                        if( str )
                        {
                            BarterText += str;
                            BarterText += "\n";
                            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
                        }
                    }
                    else
                    {
                        const char* str = FmtItemLook( cont_item, ITEM_LOOK_DEFAULT );
                        if( str )
                        {
                            BarterText = FmtItemLook( cont_item, ITEM_LOOK_BARTER );
                            DlgMainTextCur = 0;
                            DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
                        }
                    }
                }
                else
                {
                    AddMess( FOMB_VIEW, FmtItemLook( cont_item, ITEM_LOOK_ONLY_NAME ) );
                }
            }
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

        SndMngr.PlaySound( SND_LMENU );
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
    if( screen )
    {
        switch( screen )
        {
        case SCREEN__USE:
        {
            uint item_id = GetCurContainerItemId( Rect( UseWInv, UseX, UseY ), UseHeightItem, UseScroll, UseCont );
            if( item_id )
            {
                TargetSmth.SetContItem( item_id, 0 );
                LMenuSet( LMENU_ITEM_INV );
            }
        }
        break;
        case SCREEN__BARTER:
        {
            int  cont_type = 0;
            uint item_id = GetCurContainerItemId( Rect( BarterWCont1, DlgX, DlgY ), BarterCont1HeightItem, BarterScroll1, BarterCont1 );
            if( !item_id )
                item_id = GetCurContainerItemId( Rect( BarterWCont2, DlgX, DlgY ), BarterCont2HeightItem, BarterScroll2, BarterCont2 );
            if( !item_id )
            {
                cont_type = 1;
                item_id = GetCurContainerItemId( Rect( BarterWCont1o, DlgX, DlgY ), BarterCont1oHeightItem, BarterScroll1o, BarterCont1o );
                if( !item_id )
                    item_id = GetCurContainerItemId( Rect( BarterWCont2o, DlgX, DlgY ), BarterCont2oHeightItem, BarterScroll2o, BarterCont2o );
            }
            if( !item_id )
                break;

            TargetSmth.SetContItem( item_id, cont_type );
            LMenuSet( LMENU_ITEM_INV );
        }
        break;
        case SCREEN__PICKUP:
        {
            uint item_id = GetCurContainerItemId( Rect( PupWCont1, PupX, PupY ), PupHeightItem1, PupScroll1, PupCont1 );
            if( !item_id )
                item_id = GetCurContainerItemId( Rect( PupWCont2, PupX, PupY ), PupHeightItem2, PupScroll2, PupCont2 );
            if( !item_id )
                break;

            TargetSmth.SetContItem( item_id, 0 );
            LMenuSet( LMENU_ITEM_INV );
        }
        break;
        default:
            break;
        }
    }
    else
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
                CritterCl* cr = ( *it ).second;
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

        LMenuCritNodes.clear();

        if( Chosen->IsLife() )
        {
            if( LMenuMode == LMENU_NPC )
            {
                // Npc
                if( cr->IsCanTalk() )
                    LMenuCritNodes.push_back( LMENU_NODE_TALK );
                if( cr->IsDead() && !cr->GetIsNoLoot() )
                    LMenuCritNodes.push_back( LMENU_NODE_USE );
                LMenuCritNodes.push_back( LMENU_NODE_LOOK );
                if( cr->IsLife() && !cr->GetIsNoPush() )
                    LMenuCritNodes.push_back( LMENU_NODE_PUSH );
                LMenuCritNodes.push_back( LMENU_NODE_BAG );
                LMenuCritNodes.push_back( LMENU_NODE_SKILL );
            }
            else
            {
                if( cr != Chosen )
                {
                    // Player
                    if( cr->IsDead() && !cr->GetIsNoLoot() )
                        LMenuCritNodes.push_back( LMENU_NODE_USE );
                    LMenuCritNodes.push_back( LMENU_NODE_LOOK );
                    if( cr->IsLife() && !cr->GetIsNoPush() )
                        LMenuCritNodes.push_back( LMENU_NODE_PUSH );
                    LMenuCritNodes.push_back( LMENU_NODE_BAG );
                    LMenuCritNodes.push_back( LMENU_NODE_SKILL );
                    LMenuCritNodes.push_back( LMENU_NODE_GMFOLLOW );
                    if( cr->IsLife() && cr->IsPlayer() && cr->IsOnline() && CheckDist( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY(), BARTER_DIST ) )
                    {
                        LMenuCritNodes.push_back( LMENU_NODE_BARTER_OPEN );
                        LMenuCritNodes.push_back( LMENU_NODE_BARTER_HIDE );
                    }
                    if( !Chosen->GetKarmaVoting() )
                    {
                        LMenuCritNodes.push_back( LMENU_NODE_VOTE_UP );
                        LMenuCritNodes.push_back( LMENU_NODE_VOTE_DOWN );
                    }
                }
                else
                {
                    // Self
                    LMenuCritNodes.push_back( LMENU_NODE_ROTATE );
                    LMenuCritNodes.push_back( LMENU_NODE_LOOK );
                    LMenuCritNodes.push_back( LMENU_NODE_BAG );
                    LMenuCritNodes.push_back( LMENU_NODE_SKILL );
                    LMenuCritNodes.push_back( LMENU_NODE_PICK_ITEM );
                }
            }
        }
        else
        {
            LMenuCritNodes.push_back( LMENU_NODE_LOOK );
            if( LMenuMode != LMENU_NPC && cr != Chosen && !Chosen->GetKarmaVoting() )
            {
                LMenuCritNodes.push_back( LMENU_NODE_VOTE_UP );
                LMenuCritNodes.push_back( LMENU_NODE_VOTE_DOWN );
            }
        }

        LMenuCritNodes.push_back( LMENU_NODE_BREAK );
        LMenuCurNodes = &LMenuCritNodes;
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
            if( inv_item->AccCritter.Slot == SLOT_INV && Chosen->IsCanSortItems() )
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
            WriteLogF( _FUNC_, " - Unknown node<%d>.\n", num_node );
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

    switch( LMenuMode )
    {
    case LMENU_PLAYER:
    {
        if( !TargetSmth.IsCritter() )
            break;
        CritterCl* cr = GetCritter( TargetSmth.GetId() );
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
            ShowScreen( SCREEN__USE );
            UseSelect = TargetSmth;
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
        if( !TargetSmth.IsCritter() )
            break;
        CritterCl* cr = GetCritter( TargetSmth.GetId() );
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
            ShowScreen( SCREEN__USE );
            UseSelect = TargetSmth;
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
        if( !TargetSmth.IsItem() )
            break;
        ItemHex* item = GetItem( TargetSmth.GetId() );
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
            ShowScreen( SCREEN__USE );
            UseSelect = TargetSmth;
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
        Item* cont_item = GetTargetContItem();
        if( !cont_item )
            break;
        Item* inv_item = Chosen->GetItem( cont_item->GetId() );

        switch( *it_l )
        {
        case LMENU_NODE_LOOK:
            if( GetActiveScreen() == SCREEN__BARTER )
            {
                if( BarterIsPlayers )
                {
                    const char* str = FmtItemLook( cont_item, ITEM_LOOK_DEFAULT );
                    if( str )
                    {
                        BarterText += str;
                        BarterText += "\n";
                        DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
                    }
                }
                else
                {
                    const char* str = FmtItemLook( cont_item, ITEM_LOOK_BARTER );
                    if( str )
                    {
                        BarterText = str;
                        DlgMainTextCur = 0;
                        DlgMainTextLinesReal = SprMngr.GetLinesCount( DlgWText.W(), 0, BarterText.c_str() );
                    }
                }
            }
            else if( GetActiveScreen() == SCREEN__PICKUP )
                AddMess( FOMB_VIEW, FmtItemLook( cont_item, ITEM_LOOK_DEFAULT ) );
            break;
        case LMENU_NODE_DROP:
            if( !cont_item )
                break;
            if( !Chosen->IsLife() || !Chosen->IsFree() )
                break;
            if( cont_item->IsStackable() && cont_item->GetCount() > 1 )
                SplitStart( cont_item, SLOT_GROUND | ( ( ( TargetSmth.GetParam() == 1 || TargetSmth.GetParam() == 3 ) ? 1 : 0 ) << 16 ) );
            else
                AddActionBack( CHOSEN_MOVE_ITEM, cont_item->GetId(), cont_item->GetCount(), SLOT_GROUND, ( TargetSmth.GetParam() == 1 || TargetSmth.GetParam() == 3 ) ? 1 : 0 );
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
                TimerStart( inv_item->GetId(), ResMngr.GetInvAnim( inv_item->GetActualPicInv() ), inv_item->GetInvColor() );
            else
                SetAction( CHOSEN_USE_ITEM, inv_item->GetId(), 0, TARGET_SELF, 0, USE_USE );
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
        if( !TargetSmth.IsCritter() )
            break;
        CritterCl* cr = GetCritter( TargetSmth.GetId() );
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
            ShowScreen( SCREEN__USE );
            UseSelect = TargetSmth;
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
        RunScreenScript( false, ScreenModeMain, NULL );
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
    case SCREEN_CREDITS:
        CreditsNextTick = Timer::FastTick();
        CreditsYPos = GameOpt.ScreenHeight;
        CreaditsExt = Keyb::ShiftDwn;
        break;
    case SCREEN_OPTIONS:
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
    ShowScreenType = 0;
    ShowScreenParam = 0;
    ShowScreenNeedAnswer = false;
    IfaceHold = IFACE_NONE;
    Timer::StartAccelerator( ACCELERATE_NONE );
    DropScroll();

    if( screen == SCREEN_NONE )
        HideScreen( screen );

    switch( screen )
    {
    case SCREEN__INVENTORY:
        CollectContItems();
        break;
    case SCREEN__PICKUP:
        CollectContItems();
        PupScroll1 = 0;
        PupScroll2 = 0;
        PupMouseMove();
        break;
    case SCREEN__MINI_MAP:
        LmapPrepareMap();
        LmapMouseMove();
        break;
    case SCREEN__DIALOG:
        DlgMouseMove( true );
        break;
    case SCREEN__BARTER:
        BarterHoldId = 0;
        BarterIsPlayers = false;
        break;
    case SCREEN__PIP_BOY:
        PipMode = PIP__NONE;
        PipMouseMove();
        break;
    case SCREEN__FIX_BOY:
        FixMode = FIX_MODE_LIST;
        FixCurCraft = -1;
        FixGenerate( FIX_MODE_LIST );
        FixMouseMove();
        break;
    case SCREEN__AIM:
    {
        AimTargetId = 0;
        if( !smth.IsCritter() )
            break;
        CritterCl* cr = GetCritter( smth.GetId() );
        if( !cr )
            break;
        AimTargetId = cr->GetId();
        AimPic = AimGetPic( cr, "frm" );
        if( !AimPic )
            AimPic = AimGetPic( cr, "png" );
        if( !AimPic )
            AimPic = AimGetPic( cr, "bmp" );
        AimMouseMove();
    }
    break;
    case SCREEN__SAY:
        SayType = DIALOGSAY_NONE;
        SayTitle = MsgGame->GetStr( STR_SAY_TITLE );
        SayOnlyNumbers = false;
        break;
    case SCREEN__DIALOGBOX:
        DlgboxType = DIALOGBOX_NONE;
        DlgboxWait = 0;
        Str::Copy( DlgboxText, "" );
        DlgboxButtonsCount = 0;
        DlgboxSelectedButton = 0;
        break;
    case SCREEN__GM_TOWN:
    {
        GmapTownTextPos.clear();
        GmapTownText.clear();

        GmapTownPic = NULL;
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
    case SCREEN__INPUT_BOX:
        IboxMode = IBOX_MODE_NONE;
        IboxHolodiskId = 0;
        IboxTitleCur = 0;
        IboxTextCur = 0;
        break;
    case SCREEN__USE:
        UseSelect.Clear();
        TargetSmth = smth;
        CollectContItems();
        UseHoldId = 0;
        UseScroll = 0;
        UseMouseMove();
        break;
    case SCREEN__TOWN_VIEW:
        TViewShowCountours = false;
        TViewType = TOWN_VIEW_FROM_NONE;
        TViewGmapLocId = 0;
        TViewGmapLocEntrance = 0;
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
        else if( screen == SCREEN__SAY && SayType == DIALOGSAY_SAVE )
        {
            SaveLoadShowDraft();
        }
    }

    RunScreenScript( false, screen, NULL );
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
    if( !Chosen )
        return;

    LmapPrepPix.clear();

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
            if( i1 < 0 || i2 < 0 || i1 >= HexMngr.GetMaxHexX() || i2 >= HexMngr.GetMaxHexY() )
                continue;

            bool is_far = false;
            uint dist = DistGame( Chosen->GetHexX(), Chosen->GetHexY(), i1, i2 );
            if( dist > vis )
                is_far = true;

            Field& f = HexMngr.GetField( i1, i2 );
            if( f.Crit )
            {
                cur_color = ( f.Crit == Chosen ? 0xFF0000FF : ( f.Crit->GetFollowCrit() == Chosen->GetId() ? 0xFFFF00FF : 0xFFFF0000 ) );
                LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x + ( LmapZoom - 1 ), LmapWMap[ 1 ] + pix_y, cur_color, &LmapX, &LmapY ) );
                LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x, LmapWMap[ 1 ] + pix_y + ( ( LmapZoom - 1 ) / 2 ), cur_color, &LmapX, &LmapY ) );
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
            LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x, LmapWMap[ 1 ] + pix_y, cur_color, &LmapX, &LmapY ) );
            LmapPrepPix.push_back( PrepPoint( LmapWMap[ 0 ] + pix_x + ( LmapZoom - 1 ), LmapWMap[ 1 ] + pix_y + ( ( LmapZoom - 1 ) / 2 ), cur_color, &LmapX, &LmapY ) );
        }
        pix_x -= LmapZoom;
        pix_y = 0;
    }

    LmapPrepareNextTick = Timer::FastTick() + MINIMAP_PREPARE_TICK;
}

void FOClient::LmapDraw()
{
    if( Timer::FastTick() >= LmapPrepareNextTick )
        LmapPrepareMap();

    SprMngr.DrawSprite( LmapPMain, LmapMain[ 0 ] + LmapX, LmapMain[ 1 ] + LmapY );
    SprMngr.DrawPoints( LmapPrepPix, PRIMITIVE_LINELIST );
    SprMngr.DrawStr( Rect( LmapWMap[ 0 ] + LmapX, LmapWMap[ 1 ] + LmapY, LmapWMap[ 0 ] + LmapX + 100, LmapWMap[ 1 ] + LmapY + 15 ), Str::FormatBuf( "Zoom: %d", LmapZoom - 1 ), 0 );
    if( IfaceHold == IFACE_LMAP_OK )
        SprMngr.DrawSprite( LmapPBOkDw, LmapBOk[ 0 ] + LmapX, LmapBOk[ 1 ] + LmapY );
    if( IfaceHold == IFACE_LMAP_SCAN )
        SprMngr.DrawSprite( LmapPBScanDw, LmapBScan[ 0 ] + LmapX, LmapBScan[ 1 ] + LmapY );
    if( LmapSwitchHi )
        SprMngr.DrawSprite( LmapPBLoHiDw, LmapBLoHi[ 0 ] + LmapX, LmapBLoHi[ 1 ] + LmapY );
}

void FOClient::LmapLMouseDown()
{
    IfaceHold = IFACE_NONE;
    if( IsCurInRect( LmapBOk, LmapX, LmapY ) )
        IfaceHold = IFACE_LMAP_OK;
    else if( IsCurInRect( LmapBScan, LmapX, LmapY ) )
        IfaceHold = IFACE_LMAP_SCAN;
    else if( IsCurInRect( LmapBLoHi, LmapX, LmapY ) )
        IfaceHold = IFACE_LMAP_LOHI;
    else if( IsCurInRect( LmapMain, LmapX, LmapY ) )
    {
        LmapVectX = GameOpt.MouseX - LmapX;
        LmapVectY = GameOpt.MouseY - LmapY;
        IfaceHold = IFACE_LMAP_MAIN;
    }
}

void FOClient::LmapMouseMove()
{
    if( IfaceHold == IFACE_LMAP_MAIN )
    {
        LmapX = GameOpt.MouseX - LmapVectX;
        LmapY = GameOpt.MouseY - LmapVectY;
        if( LmapX < 0 )
            LmapX = 0;
        if( LmapX + LmapMain[ 2 ] > GameOpt.ScreenWidth )
            LmapX = GameOpt.ScreenWidth - LmapMain[ 2 ];
        if( LmapY < 0 )
            LmapY = 0;
        if( LmapY + LmapMain[ 3 ] > GameOpt.ScreenHeight )
            LmapY = GameOpt.ScreenHeight - LmapMain[ 3 ];
    }
}

void FOClient::LmapLMouseUp()
{
    if( IsCurInRect( LmapBOk, LmapX, LmapY ) && IfaceHold == IFACE_LMAP_OK )
    {
        ShowScreen( SCREEN_NONE );
    }
    if( IsCurInRect( LmapBScan, LmapX, LmapY ) && IfaceHold == IFACE_LMAP_SCAN )
    {
        LmapZoom = LmapZoom * 3 / 2;
        if( LmapZoom > 13 )
            LmapZoom = 2;
        LmapPrepareMap();
    }
    if( IsCurInRect( LmapBLoHi, LmapX, LmapY ) && IfaceHold == IFACE_LMAP_LOHI )
    {
        LmapSwitchHi = !LmapSwitchHi;
        LmapPrepareMap();
    }

    IfaceHold = IFACE_NONE;
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
        gt.push_back( PrepPoint( (int) ( ( *it ).first / GmapZoom ) + GmapOffsetX, (int) ( ( *it ).second / GmapZoom ) + GmapOffsetY, 0xFFFF0000 ) );
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
    while( true )
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
        CritterCl* cr = ( *it ).second;
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
            GmapLocation* cur_loc = NULL;
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

            SpriteInfo* si = SprMngr.GetSpriteInfo( CurPDef->GetCurSprId() );
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
            }
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
    else if( IfaceHold == IFACE_GMAP_INV && IsCurInRect( GmapBInv ) )
        ShowScreen( SCREEN__INVENTORY );
    else if( IfaceHold == IFACE_GMAP_MENU && IsCurInRect( GmapBMenu ) )
        ShowScreen( SCREEN__MENU_OPTION );
    else if( IfaceHold == IFACE_GMAP_CHA && IsCurInRect( GmapBCha ) )
        ShowScreen( SCREEN__CHARACTER );
    else if( IfaceHold == IFACE_GMAP_PIP && IsCurInRect( GmapBPip ) )
        ShowScreen( SCREEN__PIP_BOY );
    else if( IfaceHold == IFACE_GMAP_FIX && IsCurInRect( GmapBFix ) )
        ShowScreen( SCREEN__FIX_BOY );

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

void FOClient::CreditsDraw()
{
    SprMngr.DrawStr( Rect( 0, CreditsYPos, GameOpt.ScreenWidth, GameOpt.ScreenHeight + 50 ),
                     MsgGame->GetStr( CreaditsExt ? STR_GAME_CREDITS_EXT : STR_GAME_CREDITS ), FT_CENTERX, COLOR_TEXT, FONT_BIG );

    if( Timer::FastTick() >= CreditsNextTick )
    {
        CreditsYPos--;
        uint wait = MsgGame->GetInt( STR_GAME_CREDITS_SPEED );
        wait = CLAMP( wait, 10, 1000 );
        CreditsNextTick = Timer::FastTick() + wait;
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::TViewDraw()
{
    SprMngr.DrawSprite( TViewWMainPic, TViewWMain[ 0 ] + TViewX, TViewWMain[ 1 ] + TViewY );

    if( IfaceHold == IFACE_TOWN_VIEW_BACK )
        SprMngr.DrawSprite( TViewBBackPicDn, TViewBBack[ 0 ] + TViewX, TViewBBack[ 1 ] + TViewY );
    if( IfaceHold == IFACE_TOWN_VIEW_ENTER )
        SprMngr.DrawSprite( TViewBEnterPicDn, TViewBEnter[ 0 ] + TViewX, TViewBEnter[ 1 ] + TViewY );
    if( IfaceHold == IFACE_TOWN_VIEW_CONTOUR || TViewShowCountours )
        SprMngr.DrawSprite( TViewBContoursPicDn, TViewBContours[ 0 ] + TViewX, TViewBContours[ 1 ] + TViewY );

    SprMngr.DrawStr( Rect( TViewBBack, TViewX, IfaceHold == IFACE_TOWN_VIEW_BACK ? TViewY - 1 : TViewY ), MsgGame->GetStr( STR_TOWN_VIEW_BACK ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( TViewBEnter, TViewX, IfaceHold == IFACE_TOWN_VIEW_ENTER ? TViewY - 1 : TViewY ), MsgGame->GetStr( STR_TOWN_VIEW_ENTER ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( TViewBContours, TViewX, IfaceHold == IFACE_TOWN_VIEW_CONTOUR || TViewShowCountours ? TViewY - 1 : TViewY ), MsgGame->GetStr( STR_TOWN_VIEW_CONTOURS ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
}

void FOClient::TViewLMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( IsCurInRect( TViewBBack, TViewX, TViewY ) )
        IfaceHold = IFACE_TOWN_VIEW_BACK;
    else if( IsCurInRect( TViewBEnter, TViewX, TViewY ) )
        IfaceHold = IFACE_TOWN_VIEW_ENTER;
    else if( IsCurInRect( TViewBContours, TViewX, TViewY ) )
        IfaceHold = IFACE_TOWN_VIEW_CONTOUR;
    else if( IsCurInRect( TViewWMain, TViewX, TViewY ) )
    {
        TViewVectX = GameOpt.MouseX - TViewX;
        TViewVectY = GameOpt.MouseY - TViewY;
        IfaceHold = IFACE_TOWN_VIEW_MAIN;
    }
}

void FOClient::TViewLMouseUp()
{
    if( IfaceHold == IFACE_TOWN_VIEW_BACK && IsCurInRect( TViewBBack, TViewX, TViewY ) )
    {
        // Back
        Net_SendRefereshMe();
    }
    else if( IfaceHold == IFACE_TOWN_VIEW_ENTER && IsCurInRect( TViewBEnter, TViewX, TViewY ) )
    {
        // Enter to map
        if( TViewType == TOWN_VIEW_FROM_GLOBAL )
            Net_SendRuleGlobal( GM_CMD_TOLOCAL, TViewGmapLocId, TViewGmapLocEntrance );
    }
    else if( IfaceHold == IFACE_TOWN_VIEW_CONTOUR && IsCurInRect( TViewBContours, TViewX, TViewY ) )
    {
        if( !TViewShowCountours )
        {
            // Show contours
            HexMngr.SetCrittersContour( CONTOUR_CUSTOM );
        }
        else
        {
            // Hide contours
            HexMngr.SetCrittersContour( 0 );
        }
        TViewShowCountours = !TViewShowCountours;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::TViewMouseMove()
{
    if( IfaceHold == IFACE_TOWN_VIEW_MAIN )
    {
        TViewX = GameOpt.MouseX - TViewVectX;
        TViewY = GameOpt.MouseY - TViewVectY;

        if( TViewX < 0 )
            TViewX = 0;
        if( TViewX + TViewWMain[ 2 ] > GameOpt.ScreenWidth )
            TViewX = GameOpt.ScreenWidth - TViewWMain[ 2 ];
        if( TViewY < 0 )
            TViewY = 0;
        if( TViewY + TViewWMain[ 3 ] > GameOpt.ScreenHeight )
            TViewY = GameOpt.ScreenHeight - TViewWMain[ 3 ];
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::PipDraw()
{
    SprMngr.DrawSprite( PipPMain, PipWMain[ 0 ] + PipX, PipWMain[ 1 ] + PipY );
    if( !Chosen )
        return;

    switch( IfaceHold )
    {
    case IFACE_PIP_STATUS:
        SprMngr.DrawSprite( PipPBStatusDn, PipBStatus[ 0 ] + PipX, PipBStatus[ 1 ] + PipY );
        break;
    case IFACE_PIP_AUTOMAPS:
        SprMngr.DrawSprite( PipPBAutomapsDn, PipBAutomaps[ 0 ] + PipX, PipBAutomaps[ 1 ] + PipY );
        break;
    case IFACE_PIP_ARCHIVES:
        SprMngr.DrawSprite( PipPBArchivesDn, PipBArchives[ 0 ] + PipX, PipBArchives[ 1 ] + PipY );
        break;
    case IFACE_PIP_CLOSE:
        SprMngr.DrawSprite( PipPBCloseDn, PipBClose[ 0 ] + PipX, PipBClose[ 1 ] + PipY );
        break;
    default:
        break;
    }

    int   scr = -(int) PipScroll[ PipMode ];
    Rect& r = PipWMonitor;
    int   ml = SprMngr.GetLinesCount( 0, r.H(), NULL, FONT_DEFAULT );
    int   h = r.H() / ml;
    #define PIP_DRAW_TEXT( text, flags, color ) \
        do { if( scr >= 0 && scr < ml )         \
                 SprMngr.DrawStr( Rect( r[ 0 ], r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h, PipX, PipY ), text, flags, color ); } while( 0 )
    #define PIP_DRAW_TEXTR( text, flags, color ) \
        do { if( scr >= 0 && scr < ml )          \
                 SprMngr.DrawStr( Rect( r[ 2 ] - r.W() / 3, r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h, PipX, PipY ), text, flags, color ); } while( 0 )

    switch( PipMode )
    {
    case PIP__NONE:
    {
        SpriteInfo* si = SprMngr.GetSpriteInfo( PipPWMonitor->GetCurSprId() );
        SprMngr.DrawSprite( PipPWMonitor, PipWMonitor[ 0 ] + ( PipWMonitor[ 2 ] - PipWMonitor[ 0 ] - si->Width ) / 2 + PipX, PipWMonitor[ 1 ] + ( PipWMonitor[ 3 ] - PipWMonitor[ 1 ] - si->Height ) / 2 + PipY );
    }
    break;
    case PIP__STATUS:
    {
        // Status
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_STATUS ), FT_CENTERX, COLOR_TEXT_DGREEN );
        scr++;
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_REPLICATION_MONEY ), 0, COLOR_TEXT );
        PIP_DRAW_TEXTR( FmtGameText( STR_PIP_REPLICATION_MONEY_VAL, Chosen->GetReplicationMoney() ), 0, COLOR_TEXT );
        scr++;
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_REPLICATION_COST ), 0, COLOR_TEXT );
        PIP_DRAW_TEXTR( FmtGameText( STR_PIP_REPLICATION_COST_VAL, Chosen->GetReplicationCost() ), 0, COLOR_TEXT );
        scr++;
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_REPLICATION_COUNT ), 0, COLOR_TEXT );
        PIP_DRAW_TEXTR( FmtGameText( STR_PIP_REPLICATION_COUNT_VAL, Chosen->GetReplicationCount() ), 0, COLOR_TEXT );
        scr++;

        // Timeouts
        scr++;
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_TIMEOUTS ), FT_CENTERX, COLOR_TEXT_DGREEN );
        scr++;

        IntVec timeouts;
        if( Script::PrepareContext( ClientFunctions.GetTimeouts, _FUNC_, "Perk" ) )
        {
            ScriptArray* arr = Script::CreateArray( "CritterProperty[]" );
            Script::SetArgObject( arr );
            if( Script::RunPrepared() )
                Script::AssignScriptArrayInVector( timeouts, arr );
            arr->Release();
        }

        for( size_t j = 0; j < timeouts.size(); j++ )
        {
            uint val = Chosen->Props.GetValueAsInt( timeouts[ j ] );

            if( timeouts[ j ] == CritterCl::PropertyTimeoutRemoveFromGame->GetEnumValue() )
            {
                uint to_battle = Chosen->GetTimeoutBattle();
                if( val < to_battle )
                    val = to_battle;
            }

            val /= ( GameOpt.TimeMultiplier ? GameOpt.TimeMultiplier : 1 );             // Convert to seconds
            if( timeouts[ j ] == CritterCl::PropertyTimeoutRemoveFromGame->GetEnumValue() && val < GameOpt.MinimumOfflineTime / 1000 )
                val = GameOpt.MinimumOfflineTime / 1000;
            if( timeouts[ j ] == CritterCl::PropertyTimeoutRemoveFromGame->GetEnumValue() && NoLogOut )
                val = 1000000;                                                      // Infinity

            uint str_num = STR_TIMEOUT_SECONDS;
            if( val > 300 )
            {
                val /= 60;
                str_num = STR_TIMEOUT_MINUTES;
            }
            PIP_DRAW_TEXT( FmtGameText( STR_PARAM_NAME( j ) ), 0, COLOR_TEXT );
            if( val > 1440 )
                PIP_DRAW_TEXTR( FmtGameText( str_num, "?" ), 0, COLOR_TEXT );
            else
                PIP_DRAW_TEXTR( FmtGameText( str_num, Str::FormatBuf( "%u", val ) ), 0, COLOR_TEXT );
            scr++;
        }

        // Quests
        scr++;
        if( scr >= 0 && scr < ml )
            SprMngr.DrawStr( Rect( PipWMonitor[ 0 ], PipWMonitor[ 1 ] + scr * h, PipWMonitor[ 2 ], PipWMonitor[ 1 ] + scr * h + h, PipX, PipY ), FmtGameText( STR_PIP_QUESTS ), FT_CENTERX, COLOR_TEXT_DGREEN );
        scr++;
        QuestTabMap* tabs = QuestMngr.GetTabs();
        for( auto it = tabs->begin(), end = tabs->end(); it != end; ++it )
        {
            PIP_DRAW_TEXT( ( *it ).first.c_str(), FT_NOBREAK, COLOR_TEXT );
            scr++;
        }

        // Scores title
        scr++;
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_SCORES ), FT_CENTERX, COLOR_TEXT_DGREEN );
    }
    break;
    case PIP__STATUS_QUESTS:
    {
        QuestTab* tab = QuestMngr.GetTab( QuestNumTab );
        if( !tab )
            break;
        SprMngr.DrawStr( Rect( PipWMonitor, PipX, PipY ), tab->GetText(), FT_SKIPLINES( PipScroll[ PipMode ] ) );
    }
    break;
    case PIP__STATUS_SCORES:
    {
        bool         is_first_title = true;
        char         last_title[ 256 ];

        ScriptArray* best_scores = Globals->GetBestScores();
        for( int i = 0; i < (int) best_scores->GetSize(); i++ )
        {
            if( MsgGame->Count( STR_SCORES_TITLE( i ) ) )
                Str::Copy( last_title, MsgGame->GetStr( STR_SCORES_TITLE( i ) ) );

            const char* name = ( *(ScriptString**) best_scores->At( i ) )->c_str();
            if( !name[ 0 ] )
                continue;

            if( last_title[ 0 ] )
            {
                if( !is_first_title )
                    scr++;
                PIP_DRAW_TEXT( last_title, FT_CENTERX, COLOR_TEXT_DGREEN );
                scr += 2;
                last_title[ 0 ] = '\0';
                is_first_title = false;
            }
            PIP_DRAW_TEXT( MsgGame->GetStr( STR_SCORES_NAME( i ) ), FT_CENTERX, COLOR_TEXT );
            scr++;
            PIP_DRAW_TEXT( name, FT_CENTERX, COLOR_TEXT );
            scr++;
        }
        SAFEREL( best_scores );
    }
    break;
    case PIP__AUTOMAPS:
    {
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_MAPS ), FT_CENTERX, COLOR_TEXT_DGREEN );
        scr += 2;
        for( uint i = 0, j = (uint) Automaps.size(); i < j; i++ )
        {
            Automap& amap = Automaps[ i ];
            PIP_DRAW_TEXT( amap.LocName.c_str(), FT_CENTERX, COLOR_TEXT );
            scr++;

            for( uint k = 0, l = (uint) amap.MapNames.size(); k < l; k++ )
            {
                PIP_DRAW_TEXT( amap.MapNames[ k ].c_str(), FT_CENTERX, COLOR_TEXT_GREEN );
                scr++;
            }
            scr++;
        }
    }
    break;
    case PIP__AUTOMAPS_LOC:
    {
        SprMngr.DrawStr( Rect( PipWMonitor, PipX, PipY ), MsgLocations->GetStr( STR_LOC_INFO( AutomapSelected.LocPid ) ), FT_SKIPLINES( PipScroll[ PipMode ] ) | FT_ALIGN );
    }
    break;
    case PIP__AUTOMAPS_MAP:
    {
        hash        map_pid = AutomapSelected.MapPids[ AutomapSelected.CurMap ];
        const char* map_name = AutomapSelected.MapNames[ AutomapSelected.CurMap ].c_str();

        scr = 0;
        PIP_DRAW_TEXT( map_name, FT_CENTERX, COLOR_TEXT_GREEN );
        scr += 2;

        // Draw already builded minimap
        if( map_pid == AutomapCurMapPid )
        {
            RectF  stencil( (float) ( PipWMonitor.L + PipX ), (float) ( PipWMonitor.T + PipY ), (float) ( PipWMonitor.R + PipX ), (float) ( PipWMonitor.B + PipY ) );
            PointF offset( AutomapScrX, AutomapScrY );
            SprMngr.DrawPoints( AutomapPoints, PRIMITIVE_LINELIST, &AutomapZoom, &stencil, &offset );
            break;
        }

        // Check wait of data
        if( AutomapWaitPids.count( map_pid ) )
        {
            PIP_DRAW_TEXT( MsgGame->GetStr( STR_AUTOMAP_LOADING ), FT_CENTERX, COLOR_TEXT );
            break;
        }

        // Try load map
        ushort  maxhx, maxhy;
        ItemVec items;
        if( !HexMngr.GetMapData( map_pid, items, maxhx, maxhy ) )
        {
            // Check for already received
            if( AutomapReceivedPids.count( map_pid ) )
            {
                PIP_DRAW_TEXT( MsgGame->GetStr( STR_AUTOMAP_LOADING_ERROR ), FT_CENTERX, COLOR_TEXT );
                break;
            }

            Net_SendGiveMap( true, map_pid, AutomapSelected.LocId, 0, 0, 0 );
            AutomapWaitPids.insert( map_pid );
            break;
        }

        // Build minimap
        AutomapPoints.clear();
        AutomapScrX = (float) ( PipX - maxhx * 2 / 2 + PipWMonitor.W() / 2 );
        AutomapScrY = (float) ( PipY - maxhy * 2 / 2 + PipWMonitor.H() / 2 );
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
        {
            Item* item = *it;
            hash  pid = item->GetProtoId();
            if( pid == SP_SCEN_IBLOCK || pid == SP_MISC_SCRBLOCK )
                continue;

            uint color;
            if( pid == SP_GRID_EXITGRID )
                color = 0x3FFF7F00;
            else if( item->Proto->IsWall() )
                color = 0xFF00FF00;
            else if( item->Proto->IsScen() )
                color = 0x7F00FF00;
            else if( item->Proto->IsGrid() )
                color = 0x7F00FF00;
            else
                continue;

            int x = PipWMonitor.L + maxhx * 2 - item->AccHex.HexX * 2;
            int y = PipWMonitor.T + item->AccHex.HexY * 2;

            AutomapPoints.push_back( PrepPoint( x, y, color ) );
            AutomapPoints.push_back( PrepPoint( x + 1, y + 1, color ) );
        }
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            ( *it )->Release();
        AutomapCurMapPid = map_pid;
        AutomapZoom = 1.0f;
    }
    break;
    case PIP__ARCHIVES:
    {
        PIP_DRAW_TEXT( FmtGameText( STR_PIP_INFO ), FT_CENTERX, COLOR_TEXT_DGREEN );
        scr++;
        for( int i = 0; i < MAX_HOLO_INFO; i++ )
        {
            uint num = HoloInfo[ i ];
            if( !num )
            {
                scr++;
                continue;
            }
            PIP_DRAW_TEXT( GetHoloText( STR_HOLO_INFO_NAME( num ) ), 0, COLOR_TEXT );
            scr++;
        }
    }
    break;
    case PIP__ARCHIVES_INFO:
    {
        SprMngr.DrawStr( Rect( PipWMonitor, PipX, PipY ), GetHoloText( STR_HOLO_INFO_DESC( PipInfoNum ) ), FT_SKIPLINES( PipScroll[ PipMode ] ) | FT_ALIGN );
    }
    break;
    default:
        break;
    }

    // Time
    SprMngr.DrawStr( Rect( PipWTime, PipX, PipY ), Str::FormatBuf( "%02d", GameOpt.Day ), 0, COLOR_IFACE, FONT_NUM );                            // Day
    char mval = '0' + GameOpt.Month - 1 + 0x30;                                                                                                  // Month
    SprMngr.DrawStr( Rect( PipWTime, PipX + 26, PipY + 1 ), Str::FormatBuf( "%c", mval ), 0, COLOR_IFACE, FONT_NUM );                            // Month
    SprMngr.DrawStr( Rect( PipWTime, PipX + 63, PipY ), Str::FormatBuf( "%04d", GameOpt.Year ), 0, COLOR_IFACE, FONT_NUM );                      // Year
    SprMngr.DrawStr( Rect( PipWTime, PipX + 135, PipY ), Str::FormatBuf( "%02d%02d", GameOpt.Hour, GameOpt.Minute ), 0, COLOR_IFACE, FONT_NUM ); // Hour,Minute
}

void FOClient::PipLMouseDown()
{
    IfaceHold = IFACE_NONE;
    if( !Chosen )
        return;

    Rect& r = PipWMonitor;
    int   ml = SprMngr.GetLinesCount( 0, r.H(), NULL, FONT_DEFAULT );
    int   h = r.H() / ml;
    int   scr = -(int) PipScroll[ PipMode ];

    if( IsCurInRect( PipBStatus, PipX, PipY ) )
        IfaceHold = IFACE_PIP_STATUS;
    else if( IsCurInRect( PipBAutomaps, PipX, PipY ) )
        IfaceHold = IFACE_PIP_AUTOMAPS;
    else if( IsCurInRect( PipBArchives, PipX, PipY ) )
        IfaceHold = IFACE_PIP_ARCHIVES;
    else if( IsCurInRect( PipBClose, PipX, PipY ) )
        IfaceHold = IFACE_PIP_CLOSE;
    else if( IsCurInRect( PipWMonitor, PipX, PipY ) )
    {
        switch( PipMode )
        {
        case PIP__STATUS:
        {
            scr += 8;

            IntVec timeouts;
            if( Script::PrepareContext( ClientFunctions.GetTimeouts, _FUNC_, "Perk" ) )
            {
                ScriptArray* arr = Script::CreateArray( "CritterProperty[]" );
                Script::SetArgObject( arr );
                if( Script::RunPrepared() )
                    Script::AssignScriptArrayInVector( timeouts, arr );
                arr->Release();
            }
            scr += (int) timeouts.size();
            int          scr_ = scr;

            QuestTabMap* tabs = QuestMngr.GetTabs();
            for( auto it = tabs->begin(), end = tabs->end(); it != end; ++it )
            {
                if( scr >= 0 && scr < ml && IsCurInRect( Rect( r[ 0 ], r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h ), PipX, PipY ) )
                {
                    QuestNumTab = scr - scr_ + PipScroll[ PipMode ];
                    PipMode = PIP__STATUS_QUESTS;
                    PipScroll[ PipMode ] = 0;
                    break;
                }
                scr++;
            }
            if( PipMode == PIP__STATUS )
            {
                scr++;
                if( scr >= 0 && scr < ml && IsCurInRect( Rect( r[ 0 ], r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h ), PipX, PipY ) )
                {
                    PipMode = PIP__STATUS_SCORES;
                    PipScroll[ PipMode ] = 0;
                    break;
                }
            }
        }
        break;
        case PIP__AUTOMAPS:
        {
            scr += 2;
            for( uint i = 0, j = (uint) Automaps.size(); i < j; i++ )
            {
                Automap& amap = Automaps[ i ];

                if( scr >= 0 && scr < ml && IsCurInRect( Rect( r[ 0 ], r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h ), PipX, PipY ) )
                {
                    PipMode = PIP__AUTOMAPS_LOC;
                    AutomapSelected = amap;
                    PipScroll[ PipMode ] = 0;
                    break;
                }
                scr++;

                for( uint k = 0, l = (uint) amap.MapNames.size(); k < l; k++ )
                {
                    if( scr >= 0 && scr < ml && IsCurInRect( Rect( r[ 0 ], r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h ), PipX, PipY ) )
                    {
                        PipMode = PIP__AUTOMAPS_MAP;
                        AutomapSelected = amap;
                        AutomapSelected.CurMap = k;
                        PipScroll[ PipMode ] = 0;
                        AutomapCurMapPid = 0;
                        i = j;
                        break;
                    }
                    scr++;
                }
                scr++;
            }
        }
        break;
        case PIP__AUTOMAPS_MAP:
        {
            PipVectX = GameOpt.MouseX - (int) AutomapScrX;
            PipVectY = GameOpt.MouseY - (int) AutomapScrY;
            IfaceHold = IFACE_PIP_AUTOMAPS_SCR;
        }
        break;
        case PIP__ARCHIVES:
        {
            scr += 1;
            for( int i = 0; i < MAX_HOLO_INFO; i++ )
            {
                if( scr >= 0 && scr < ml && IsCurInRect( Rect( r[ 0 ], r[ 1 ] + scr * h, r[ 2 ], r[ 1 ] + scr * h + h ), PipX, PipY ) )
                {
                    PipInfoNum = HoloInfo[ scr - 1 + PipScroll[ PipMode ] ];
                    if( !PipInfoNum )
                        break;
                    PipMode = PIP__ARCHIVES_INFO;
                    PipScroll[ PipMode ] = 0;
                    break;
                }
                scr++;
            }
        }
        break;
        default:
            break;
        }
    }
    else if( IsCurInRect( PipWMain, PipX, PipY ) )
    {
        PipVectX = GameOpt.MouseX - PipX;
        PipVectY = GameOpt.MouseY - PipY;
        IfaceHold = IFACE_PIP_MAIN;
    }
}

void FOClient::PipLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_PIP_STATUS:
        if( !IsCurInRect( PipBStatus, PipX, PipY ) )
            break;
        PipMode = PIP__STATUS;
        break;
    case IFACE_PIP_AUTOMAPS:
        if( !IsCurInRect( PipBAutomaps, PipX, PipY ) )
            break;
        PipMode = PIP__AUTOMAPS;
        break;
    case IFACE_PIP_ARCHIVES:
        if( !IsCurInRect( PipBArchives, PipX, PipY ) )
            break;
        PipMode = PIP__ARCHIVES;
        break;
    case IFACE_PIP_CLOSE:
        if( !IsCurInRect( PipBClose, PipX, PipY ) )
            break;
        ShowScreen( SCREEN_NONE );
        break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::PipRMouseDown()
{
    if( IsCurInRect( PipWMonitor, PipX, PipY ) )
    {
        switch( PipMode )
        {
        case PIP__STATUS_QUESTS:
            PipMode = PIP__STATUS;
            break;
        case PIP__STATUS_SCORES:
            PipMode = PIP__STATUS;
            break;
        case PIP__AUTOMAPS_LOC:
            PipMode = PIP__AUTOMAPS;
            break;
        case PIP__AUTOMAPS_MAP:
            PipMode = PIP__AUTOMAPS;
            break;
        case PIP__ARCHIVES_INFO:
            PipMode = PIP__ARCHIVES;
            break;
        default:
            break;
        }
    }
}

void FOClient::PipMouseMove()
{
    if( IfaceHold == IFACE_PIP_MAIN )
    {
        AutomapScrX -= (float) PipX;
        AutomapScrY -= (float) PipY;
        PipX = GameOpt.MouseX - PipVectX;
        PipY = GameOpt.MouseY - PipVectY;

        if( PipX < 0 )
            PipX = 0;
        if( PipX + PipWMain[ 2 ] > GameOpt.ScreenWidth )
            PipX = GameOpt.ScreenWidth - PipWMain[ 2 ];
        if( PipY < 0 )
            PipY = 0;
        if( PipY + PipWMain[ 3 ] > GameOpt.ScreenHeight )
            PipY = GameOpt.ScreenHeight - PipWMain[ 3 ];
        AutomapScrX += (float) PipX;
        AutomapScrY += (float) PipY;
    }
    else if( IfaceHold == IFACE_PIP_AUTOMAPS_SCR )
    {
        AutomapScrX = (float) ( GameOpt.MouseX - PipVectX );
        AutomapScrY = (float) ( GameOpt.MouseY - PipVectY );
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::AimDraw()
{
    SprMngr.DrawSprite( AimPWMain, AimWMain[ 0 ] + AimX, AimWMain[ 1 ] + AimY );

    if( AimPic )
        SprMngr.DrawSprite( AimPic, AimWMain[ 0 ] + AimPicX + AimX, AimWMain[ 1 ] + AimPicY + AimY );
    if( IfaceHold == IFACE_AIM_CANCEL )
        SprMngr.DrawSprite( AimPBCancelDn, AimBCancel[ 0 ] + AimX, AimBCancel[ 1 ] + AimY );

    SprMngr.Flush();

    CritterCl* cr = GetCritter( AimTargetId );
    if( !Chosen || !cr )
        return;

    if( GameOpt.ApCostAimArms == GameOpt.ApCostAimTorso && GameOpt.ApCostAimTorso == GameOpt.ApCostAimLegs && GameOpt.ApCostAimLegs == GameOpt.ApCostAimGroin && GameOpt.ApCostAimGroin == GameOpt.ApCostAimEyes && GameOpt.ApCostAimEyes == GameOpt.ApCostAimHead )
    {
        SprMngr.DrawStr( Rect( AimWHeadT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_HEAD - 1 ) ), FT_NOBREAK, IfaceHold == IFACE_AIM_HEAD ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWLArmT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_LEFT_ARM - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_LARM ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWRArmT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_RIGHT_ARM - 1 ) ), FT_NOBREAK, IfaceHold == IFACE_AIM_RARM ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWTorsoT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_TORSO - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_TORSO ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWRLegT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_RIGHT_LEG - 1 ) ), FT_NOBREAK, IfaceHold == IFACE_AIM_RLEG ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWLLegT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_LEFT_LEG - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_LLEG ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWEyesT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_EYES - 1 ) ), FT_NOBREAK, IfaceHold == IFACE_AIM_EYES ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWGroinT, AimX, AimY ), Str::FormatBuf( "%s", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_GROIN - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_GROIN ? COLOR_TEXT_RED : COLOR_TEXT );
    }
    else
    {
        SprMngr.DrawStr( Rect( AimWHeadT, AimX, AimY ), Str::FormatBuf( "%s (%u)", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_HEAD - 1 ), GameOpt.ApCostAimHead ), FT_NOBREAK, IfaceHold == IFACE_AIM_HEAD ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWLArmT, AimX, AimY ), Str::FormatBuf( "(%u) %s", GameOpt.ApCostAimArms, MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_LEFT_ARM - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_LARM ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWRArmT, AimX, AimY ), Str::FormatBuf( "%s (%u)", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_RIGHT_ARM - 1 ), GameOpt.ApCostAimArms ), FT_NOBREAK, IfaceHold == IFACE_AIM_RARM ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWTorsoT, AimX, AimY ), Str::FormatBuf( "(%u) %s", GameOpt.ApCostAimTorso, MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_TORSO - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_TORSO ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWRLegT, AimX, AimY ), Str::FormatBuf( "%s (%u)", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_RIGHT_LEG - 1 ), GameOpt.ApCostAimLegs ), FT_NOBREAK, IfaceHold == IFACE_AIM_RLEG ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWLLegT, AimX, AimY ), Str::FormatBuf( "(%u) %s", GameOpt.ApCostAimLegs, MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_LEFT_LEG - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_LLEG ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWEyesT, AimX, AimY ), Str::FormatBuf( "%s (%u)", MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_EYES - 1 ), GameOpt.ApCostAimEyes ), FT_NOBREAK, IfaceHold == IFACE_AIM_EYES ? COLOR_TEXT_RED : COLOR_TEXT );
        SprMngr.DrawStr( Rect( AimWGroinT, AimX, AimY ), Str::FormatBuf( "(%u) %s", GameOpt.ApCostAimGroin, MsgCombat->GetStr( 1000 + cr->GetCrTypeAlias() * 10 + HIT_LOCATION_GROIN - 1 ) ), FT_NOBREAK | FT_CENTERR, IfaceHold == IFACE_AIM_GROIN ? COLOR_TEXT_RED : COLOR_TEXT );
    }

    bool zero = !HexMngr.TraceBullet( Chosen->GetHexX(), Chosen->GetHexY(), cr->GetHexX(), cr->GetHexY(), Chosen->GetAttackDist(), 0.0f, cr, false, NULL, 0, NULL, NULL, NULL, true );
    SprMngr.DrawStr( Rect( AimWHeadP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_HEAD ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWLArmP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_LEFT_ARM ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWRArmP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_RIGHT_ARM ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWTorsoP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_TORSO ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWRLegP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_RIGHT_LEG ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWLLegP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_LEFT_LEG ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWEyesP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_EYES ) ), FT_NOBREAK | FT_CENTERX );
    SprMngr.DrawStr( Rect( AimWGroinP, AimX, AimY ), Str::ItoA( zero ? 0 : ScriptGetHitProc( cr, HIT_LOCATION_GROIN ) ), FT_NOBREAK | FT_CENTERX );
}

void FOClient::AimLMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( IsCurInRect( AimBCancel, AimX, AimY ) )
        IfaceHold = IFACE_AIM_CANCEL;
    else if( IsCurInRect( AimWHeadT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_HEAD;
    else if( IsCurInRect( AimWLArmT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_LARM;
    else if( IsCurInRect( AimWRArmT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_RARM;
    else if( IsCurInRect( AimWTorsoT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_TORSO;
    else if( IsCurInRect( AimWRLegT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_RLEG;
    else if( IsCurInRect( AimWLLegT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_LLEG;
    else if( IsCurInRect( AimWEyesT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_EYES;
    else if( IsCurInRect( AimWGroinT, AimX, AimY ) )
        IfaceHold = IFACE_AIM_GROIN;
    else if( IsCurInRect( AimWMain, AimX, AimY ) )
    {
        AimVectX = GameOpt.MouseX - AimX;
        AimVectY = GameOpt.MouseY - AimY;
        IfaceHold = IFACE_AIM_MAIN;
    }
}

void FOClient::AimLMouseUp()
{
    if( !Chosen )
        return;

    switch( IfaceHold )
    {
    case IFACE_AIM_CANCEL:
        if( !IsCurInRect( AimBCancel, AimX, AimY ) )
            break;
        ShowScreen( SCREEN_NONE );
        break;
    case IFACE_AIM_HEAD:
        if( !IsCurInRect( AimWHeadT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_HEAD );
        goto Label_Attack;
        break;
    case IFACE_AIM_LARM:
        if( !IsCurInRect( AimWLArmT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_LEFT_ARM );
        goto Label_Attack;
        break;
    case IFACE_AIM_RARM:
        if( !IsCurInRect( AimWRArmT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_RIGHT_ARM );
        goto Label_Attack;
        break;
    case IFACE_AIM_TORSO:
        if( !IsCurInRect( AimWTorsoT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_TORSO );
        goto Label_Attack;
        break;
    case IFACE_AIM_RLEG:
        if( !IsCurInRect( AimWRLegT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_RIGHT_LEG );
        goto Label_Attack;
        break;
    case IFACE_AIM_LLEG:
        if( !IsCurInRect( AimWLLegT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_LEFT_LEG );
        goto Label_Attack;
        break;
    case IFACE_AIM_EYES:
        if( !IsCurInRect( AimWEyesT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_EYES );
        goto Label_Attack;
        break;
    case IFACE_AIM_GROIN:
        if( !IsCurInRect( AimWGroinT, AimX, AimY ) )
            break;
        Chosen->SetAim( HIT_LOCATION_GROIN );
        goto Label_Attack;
        break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
    return;

Label_Attack:
    CritterCl * cr = GetCritter( AimTargetId );
    if( cr )
        SetAction( CHOSEN_USE_ITEM, Chosen->ItemSlotMain->GetId(), 0, TARGET_CRITTER, AimTargetId, Chosen->GetFullRate() );

    IfaceHold = IFACE_NONE;
    if( !Keyb::ShiftDwn )
    {
        ShowScreen( SCREEN_NONE );
        SetCurMode( CUR_USE_WEAPON );
    }
}

void FOClient::AimMouseMove()
{
    if( IfaceHold == IFACE_AIM_MAIN )
    {
        AimX = GameOpt.MouseX - AimVectX;
        AimY = GameOpt.MouseY - AimVectY;

        if( AimX < 0 )
            AimX = 0;
        if( AimX + AimWMain[ 2 ] > GameOpt.ScreenWidth )
            AimX = GameOpt.ScreenWidth - AimWMain[ 2 ];
        if( AimY < 0 )
            AimY = 0;
        if( AimY + AimWMain[ 3 ] > GameOpt.ScreenHeight )
            AimY = GameOpt.ScreenHeight - AimWMain[ 3 ];
    }
}

AnyFrames* FOClient::AimGetPic( CritterCl* cr, const char* ext )
{
    // Make names
    char aim_name[ MAX_FOPATH ];
    char aim_name_alias[ MAX_FOPATH ];
    Str::Format( aim_name, "%s%sna.%s", FileManager::GetDataPath( "", PT_ART_CRITTERS ), CritType::GetName( cr->GetCrType() ), ext );
    Str::Format( aim_name_alias, "%s%sna.%s", FileManager::GetDataPath( "", PT_ART_CRITTERS ), CritType::GetName( cr->GetCrTypeAlias() ), ext );

    // Load
    AnyFrames* anim = ResMngr.GetAnim( Str::GetHash( aim_name ), RES_ATLAS_DYNAMIC );
    if( !anim )
        anim = ResMngr.GetAnim( Str::GetHash( aim_name_alias ), RES_ATLAS_DYNAMIC );
    return anim;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::PupDraw()
{
    SprMngr.DrawSprite( PupPMain, PupX, PupY );

    if( !Chosen )
        return;

    // Info window
    if( PupTransferType == TRANSFER_HEX_CONT_UP || PupTransferType == TRANSFER_HEX_CONT_DOWN || PupTransferType == TRANSFER_FAR_CONT )
    {
        ProtoItem* proto_item = ItemMngr.GetProtoItem( PupContPid );
        if( proto_item )
        {
            AnyFrames* anim = ResMngr.GetItemAnim( proto_item->GetPicMap() );
            if( anim )
                SprMngr.DrawSpriteSize( anim->GetSprId( anim->GetCnt() - 1 ), PupWInfo[ 0 ] + PupX, PupWInfo[ 1 ] + PupY, PupWInfo.W(), PupWInfo.H(), false, true );
        }
    }
    else if( PupTransferType == TRANSFER_CRIT_STEAL || PupTransferType == TRANSFER_CRIT_LOOT || PupTransferType == TRANSFER_FAR_CRIT )
    {
        CritterCl* cr = HexMngr.GetCritter( PupContId );
        if( cr )
            cr->DrawStay( Rect( PupWInfo, PupX, PupY ) );
    }

    // Button Ok
    if( IfaceHold == IFACE_PUP_OK )
        SprMngr.DrawSprite( PupPBOkOn, PupBOk[ 0 ] + PupX, PupBOk[ 1 ] + PupY );

    // Button Take All
    if( IfaceHold == IFACE_PUP_TAKEALL )
        SprMngr.DrawSprite( PupPTakeAllOn, PupBTakeAll[ 0 ] + PupX, PupBTakeAll[ 1 ] + PupY );

    // Scrolling Buttons Window 1
    if( PupScroll1 <= 0 )
        SprMngr.DrawSprite( PupPBScrUpOff1, PupBScrUp1[ 0 ] + PupX, PupBScrUp1[ 1 ] + PupY );
    else if( IfaceHold == IFACE_PUP_SCRUP1 )
        SprMngr.DrawSprite( PupPBScrUpOn1, PupBScrUp1[ 0 ] + PupX, PupBScrUp1[ 1 ] + PupY );

    int count_items = (int) Chosen->GetItemsCountInv();
    if( PupScroll1 >= count_items - ( PupWCont1[ 3 ] - PupWCont1[ 1 ] ) / PupHeightItem1 )
        SprMngr.DrawSprite( PupPBScrDwOff1, PupBScrDw1[ 0 ] + PupX, PupBScrDw1[ 1 ] + PupY );
    else if( IfaceHold == IFACE_PUP_SCRDOWN1 )
        SprMngr.DrawSprite( PupPBScrDwOn1, PupBScrDw1[ 0 ] + PupX, PupBScrDw1[ 1 ] + PupY );

    // Scrolling Buttons Window 2
    if( PupScroll2 <= 0 )
        SprMngr.DrawSprite( PupPBScrUpOff2, PupBScrUp2[ 0 ] + PupX, PupBScrUp2[ 1 ] + PupY );
    else if( IfaceHold == IFACE_PUP_SCRUP2 )
        SprMngr.DrawSprite( PupPBScrUpOn2, PupBScrUp2[ 0 ] + PupX, PupBScrUp2[ 1 ] + PupY );

    count_items = (int) PupCont2.size();
    if( PupScroll2 >= count_items - ( PupWCont2[ 3 ] - PupWCont2[ 1 ] ) / PupHeightItem2 )
        SprMngr.DrawSprite( PupPBScrDwOff2, PupBScrDw2[ 0 ] + PupX, PupBScrDw2[ 1 ] + PupY );
    else if( IfaceHold == IFACE_PUP_SCRDOWN2 )
        SprMngr.DrawSprite( PupPBScrDwOn2, PupBScrDw2[ 0 ] + PupX, PupBScrDw2[ 1 ] + PupY );

    // Scrolling critters
    if( PupGetLootCrits().size() > 1 )
    {
        if( IfaceHold == IFACE_PUP_SCRCR_L )
            SprMngr.DrawSprite( PupBNextCritLeftPicDown, PupBNextCritLeft[ 0 ] + PupX, PupBNextCritLeft[ 1 ] + PupY );
        else
            SprMngr.DrawSprite( PupBNextCritLeftPicUp, PupBNextCritLeft[ 0 ] + PupX, PupBNextCritLeft[ 1 ] + PupY );

        if( IfaceHold == IFACE_PUP_SCRCR_R )
            SprMngr.DrawSprite( PupBNextCritRightPicDown, PupBNextCritRight[ 0 ] + PupX, PupBNextCritRight[ 1 ] + PupY );
        else
            SprMngr.DrawSprite( PupBNextCritRightPicUp, PupBNextCritRight[ 0 ] + PupX, PupBNextCritRight[ 1 ] + PupY );
    }

    // Items
    ContainerDraw( Rect( PupWCont1, PupX, PupY ), PupHeightItem1, PupScroll1, PupCont1, IfaceHold == IFACE_PUP_CONT1 ? PupHoldId : 0 );
    ContainerDraw( Rect( PupWCont2, PupX, PupY ), PupHeightItem2, PupScroll2, PupCont2, IfaceHold == IFACE_PUP_CONT2 ? PupHoldId : 0 );
}

void FOClient::PupLMouseDown()
{
    PupHoldId = 0;
    IfaceHold = IFACE_NONE;
    if( !Chosen )
        return;

    if( IsCurInRect( PupWCont1, PupX, PupY ) )
    {
        PupHoldId = GetCurContainerItemId( Rect( PupWCont1, PupX, PupY ), PupHeightItem1, PupScroll1, PupCont1 );
        if( PupHoldId )
            IfaceHold = IFACE_PUP_CONT1;
    }
    else if( IsCurInRect( PupWCont2, PupX, PupY ) )
    {
        PupHoldId = GetCurContainerItemId( Rect( PupWCont2, PupX, PupY ), PupHeightItem2, PupScroll2, PupCont2 );
        if( PupHoldId )
            IfaceHold = IFACE_PUP_CONT2;
    }
    else if( IsCurInRect( PupBOk, PupX, PupY ) )
        IfaceHold = IFACE_PUP_OK;
    else if( IsCurInRect( PupBScrUp1, PupX, PupY ) )
    {
        Timer::StartAccelerator( ACCELERATE_PUP_SCRUP1 );
        IfaceHold = IFACE_PUP_SCRUP1;
    }
    else if( IsCurInRect( PupBScrDw1, PupX, PupY ) )
    {
        Timer::StartAccelerator( ACCELERATE_PUP_SCRDOWN1 );
        IfaceHold = IFACE_PUP_SCRDOWN1;
    }
    else if( IsCurInRect( PupBScrUp2, PupX, PupY ) )
    {
        Timer::StartAccelerator( ACCELERATE_PUP_SCRUP2 );
        IfaceHold = IFACE_PUP_SCRUP2;
    }
    else if( IsCurInRect( PupBScrDw2, PupX, PupY ) )
    {
        Timer::StartAccelerator( ACCELERATE_PUP_SCRDOWN2 );
        IfaceHold = IFACE_PUP_SCRDOWN2;
    }
    else if( IsCurInRect( PupBTakeAll, PupX, PupY ) )
        IfaceHold = IFACE_PUP_TAKEALL;
    else if( IsCurInRect( PupBNextCritLeft, PupX, PupY ) && PupGetLootCrits().size() > 1 )
    {
        if( Chosen->IsFree() && ChosenAction.empty() )
            IfaceHold = IFACE_PUP_SCRCR_L;
    }
    else if( IsCurInRect( PupBNextCritRight, PupX, PupY ) && PupGetLootCrits().size() > 1 )
    {
        if( Chosen->IsFree() && ChosenAction.empty() )
            IfaceHold = IFACE_PUP_SCRCR_R;
    }
    else if( IsCurInRect( PupWMain, PupX, PupY ) )
    {
        PupVectX = GameOpt.MouseX - PupX;
        PupVectY = GameOpt.MouseY - PupY;
        IfaceHold = IFACE_PUP_MAIN;
    }

    if( IsCurMode( CUR_DEFAULT ) && ( IfaceHold == IFACE_PUP_CONT1 || IfaceHold == IFACE_PUP_CONT2 ) )
    {
        IfaceHold = IFACE_NONE;
        LMenuTryActivate();
    }
}

void FOClient::PupLMouseUp()
{
    if( !Chosen )
        IfaceHold = IFACE_NONE;

    switch( IfaceHold )
    {
    case IFACE_PUP_CONT2:
    {
        if( !IsCurInRect( PupWCont1, PupX, PupY ) )
            break;

        auto it = PtrCollectionFind( PupCont2.begin(), PupCont2.end(), PupHoldId );
        if( it != PupCont2.end() )
        {
            Item* item = *it;
            if( item->GetCount() > 1 )
                SplitStart( item, IFACE_PUP_CONT2 );
            else
                SetAction( CHOSEN_MOVE_ITEM_CONT, PupHoldId, IFACE_PUP_CONT2, 1 );
        }
    }
    break;
    case IFACE_PUP_CONT1:
    {
        if( !IsCurInRect( PupWCont2, PupX, PupY ) )
            break;

        auto it = PtrCollectionFind( PupCont1.begin(), PupCont1.end(), PupHoldId );
        if( it != PupCont1.end() )
        {
            Item* item = *it;
            if( item->GetCount() > 1 )
                SplitStart( item, IFACE_PUP_CONT1 );
            else
                SetAction( CHOSEN_MOVE_ITEM_CONT, PupHoldId, IFACE_PUP_CONT1, 1 );
        }
    }
    break;
    case IFACE_PUP_OK:
    {
        if( !IsCurInRect( PupBOk, PupX, PupY ) )
            break;
        ShowScreen( SCREEN_NONE );
    }
    break;
    case IFACE_PUP_SCRUP1:
    {
        if( !IsCurInRect( PupBScrUp1, PupX, PupY ) )
            break;
        if( PupScroll1 > 0 )
            PupScroll1--;
    }
    break;
    case IFACE_PUP_SCRDOWN1:
    {
        if( !IsCurInRect( PupBScrDw1, PupX, PupY ) )
            break;
        if( PupScroll1 < (int) Chosen->GetItemsCountInv() - ( PupWCont2[ 3 ] - PupWCont2[ 1 ] ) / PupHeightItem2 )
            PupScroll1++;
    }
    break;
    case IFACE_PUP_SCRUP2:
    {
        if( !IsCurInRect( PupBScrUp2, PupX, PupY ) )
            break;
        if( PupScroll2 > 0 )
            PupScroll2--;
    }
    break;
    case IFACE_PUP_SCRDOWN2:
    {
        if( !IsCurInRect( PupBScrDw2, PupX, PupY ) )
            break;
        if( PupScroll2 < (int) PupCont2.size() - ( PupWCont1[ 3 ] - PupWCont1[ 1 ] ) / PupHeightItem1 )
            PupScroll2++;
    }
    break;
    case IFACE_PUP_TAKEALL:
    {
        if( PupTransferType == TRANSFER_CRIT_STEAL )
            break;
        if( !IsCurInRect( PupBTakeAll, PupX, PupY ) )
            break;
        SetAction( CHOSEN_TAKE_ALL );
    }
    break;
    case IFACE_PUP_SCRCR_L:
    {
        if( !IsCurInRect( PupBNextCritLeft, PupX, PupY ) )
            break;
        if( !Chosen->IsFree() || !ChosenAction.empty() )
            break;
        uint cnt = (uint) PupGetLootCrits().size();
        if( cnt < 2 )
            break;
        if( !PupScrollCrit )
            PupScrollCrit = cnt - 1;
        else
            PupScrollCrit--;
        CritterCl* cr = PupGetLootCrit( PupScrollCrit );
        if( !cr )
            break;
        SetAction( CHOSEN_PICK_CRIT, cr->GetId(), 0 );
    }
    break;
    case IFACE_PUP_SCRCR_R:
    {
        if( !IsCurInRect( PupBNextCritRight, PupX, PupY ) )
            break;
        if( !Chosen->IsFree() || !ChosenAction.empty() )
            break;
        uint cnt = (uint) PupGetLootCrits().size();
        if( cnt < 2 )
            break;
        if( PupScrollCrit + 1 >= (int) cnt )
            PupScrollCrit = 0;
        else
            PupScrollCrit++;
        CritterCl* cr = PupGetLootCrit( PupScrollCrit );
        if( !cr )
            break;
        SetAction( CHOSEN_PICK_CRIT, cr->GetId(), 0 );
    }
    break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
    PupHoldId = 0;
}

void FOClient::PupMouseMove()
{
    if( IfaceHold == IFACE_PUP_MAIN )
    {
        PupX = GameOpt.MouseX - PupVectX;
        PupY = GameOpt.MouseY - PupVectY;

        if( PupX < 0 )
            PupX = 0;
        if( PupX + PupWMain[ 2 ] > GameOpt.ScreenWidth )
            PupX = GameOpt.ScreenWidth - PupWMain[ 2 ];
        if( PupY < 0 )
            PupY = 0;
        // if(PupY+PupWMain[3]>IntY) PupY=IntY-PupWMain[3];
        if( PupY + PupWMain[ 3 ] > GameOpt.ScreenHeight )
            PupY = GameOpt.ScreenHeight - PupWMain[ 3 ];
    }
}

void FOClient::PupRMouseDown()
{
    SetCurCastling( CUR_DEFAULT, CUR_HAND );
}

void FOClient::PupTransfer( uint item_id, uint cont, uint count )
{
    if( !count )
        return;

    // From Chosen to container
    if( cont == IFACE_PUP_CONT1 )
    {
        Item* item = Chosen->GetItem( item_id );
        if( !item )
            return;

        PupLastPutId = item_id;
        Net_SendItemCont( PupTransferType, PupContId, item_id, count, CONT_PUT );
        WaitPing();
    }
    // From container to Chosen
    else if( cont == IFACE_PUP_CONT2 )
    {
        auto it = PtrCollectionFind( PupCont2Init.begin(), PupCont2Init.end(), item_id );
        if( it == PupCont2Init.end() )
            return;
        Item* item = *it;

        if( item->IsStackable() && count < item->GetCount() )
        {
            item->ChangeCount( -(int) count );
        }
        else
        {
            ( *it )->Release();
            PupCont2Init.erase( it );
        }

        Net_SendItemCont( PupTransferType, PupContId, item_id, count, CONT_GET );
        WaitPing();
    }
    CollectContItems();
}

CritVec& FOClient::PupGetLootCrits()
{
    static CritVec loot;
    loot.clear();
    if( PupTransferType != TRANSFER_CRIT_LOOT )
        return loot;
    CritterCl* loot_cr = HexMngr.GetCritter( PupContId );
    if( !loot_cr || !loot_cr->IsDead() )
        return loot;
    Field& f = HexMngr.GetField( loot_cr->GetHexX(), loot_cr->GetHexY() );
    if( f.DeadCrits )
    {
        for( uint i = 0, j = (uint) f.DeadCrits->size(); i < j; i++ )
            if( !f.DeadCrits->at( i )->GetIsNoLoot() )
                loot.push_back( f.DeadCrits->at( i ) );
    }
    return loot;
}

CritterCl* FOClient::PupGetLootCrit( int scroll )
{
    CritVec& loot = PupGetLootCrits();
    for( uint i = 0, j = (uint) loot.size(); i < j; i++ )
        if( i == (uint) scroll )
            return loot[ i ];
    return NULL;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::CurDrawHand()
{
    if( !Chosen )
        return;

    SpriteInfo* si = NULL;
    int         x = 0, y = 0;

    if( GetActiveScreen() == SCREEN__BARTER )
    {
        if( IfaceHold && BarterHoldId )
        {
            ItemVec* cont = NULL;
            switch( IfaceHold )
            {
            case IFACE_BARTER_CONT1:
                cont = &InvContInit;
                break;
            case IFACE_BARTER_CONT2:
                cont = &BarterCont2Init;
                break;
            case IFACE_BARTER_CONT1O:
                cont = &BarterCont1oInit;
                break;
            case IFACE_BARTER_CONT2O:
                cont = &BarterCont2oInit;
                break;
            default:
                goto DrawCurHand;
            }

            auto it = PtrCollectionFind( cont->begin(), cont->end(), BarterHoldId );
            if( it == cont->end() )
                goto DrawCurHand;
            Item*      item = *it;

            AnyFrames* anim = ResMngr.GetInvAnim( item->GetActualPicInv() );
            if( !anim )
                goto DrawCurHand;

            if( !( si = SprMngr.GetSpriteInfo( anim->GetCurSprId() ) ) )
                goto DrawCurHand;
            x = GameOpt.MouseX - ( si->Width / 2 ) + si->OffsX;
            y = GameOpt.MouseY - ( si->Height / 2 ) + si->OffsY;
            SprMngr.DrawSprite( anim, x, y, item->GetInvColor() );
        }
        else
            goto DrawCurHand;
    }
    else if( GetActiveScreen() == SCREEN__PICKUP && PupHoldId )
    {
        Item* item = GetContainerItem( IfaceHold == IFACE_PUP_CONT1 ? PupCont1 : PupCont2, PupHoldId );
        if( item )
        {
            AnyFrames* anim = ResMngr.GetInvAnim( item->GetActualPicInv() );
            if( anim )
            {
                if( !( si = SprMngr.GetSpriteInfo( anim->GetCurSprId() ) ) )
                    return;
                x = GameOpt.MouseX - ( si->Width / 2 ) + si->OffsX;
                y = GameOpt.MouseY - ( si->Height / 2 ) + si->OffsY;
                SprMngr.DrawSprite( anim, x, y, item->GetInvColor() );
            }
        }
    }

DrawCurHand:
    if( !( si = SprMngr.GetSpriteInfo( CurPHand->GetCurSprId() ) ) )
        return;
    x = GameOpt.MouseX - ( si->Width / 2 ) + si->OffsX;
    y = GameOpt.MouseY - si->Height + si->OffsY;
    SprMngr.DrawSprite( CurPHand, x, y );
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::DlgboxDraw()
{
    // Check for end time
    if( DlgboxWait && Timer::GameTick() > DlgboxWait )
    {
        ShowScreen( SCREEN_NONE );
        return;
    }

    SprMngr.DrawSprite( DlgboxWTopPicNone, DlgboxWTop[ 0 ] + DlgboxX, DlgboxWTop[ 1 ] + DlgboxY );
    SprMngr.DrawStr( Rect( DlgboxWText, DlgboxX, DlgboxY ), DlgboxText, 0 );
    uint y_offs = DlgboxWTop.H();
    for( uint i = 0; i < DlgboxButtonsCount; i++ )
    {
        SprMngr.DrawSprite( DlgboxWMiddlePicNone, DlgboxWMiddle[ 0 ] + DlgboxX, DlgboxWMiddle[ 1 ] + DlgboxY + y_offs );
        if( IfaceHold == IFACE_DIALOG_BTN && i == DlgboxSelectedButton )
            SprMngr.DrawSprite( DlgboxBButtonPicDown, DlgboxBButton[ 0 ] + DlgboxX, DlgboxBButton[ 1 ] + DlgboxY + y_offs );
        SprMngr.DrawStr( Rect( DlgboxBButtonText, DlgboxX, DlgboxY + y_offs ), DlgboxButtonText[ i ].c_str(), FT_NOBREAK | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
        y_offs += DlgboxWMiddle.H();
    }
    SprMngr.DrawSprite( DlgboxWBottomPicNone, DlgboxWTop[ 0 ] + DlgboxX, DlgboxWTop[ 1 ] + DlgboxY + y_offs );
}

void FOClient::DlgboxLMouseDown()
{
    IfaceHold = IFACE_NONE;

    uint y_offs = DlgboxWTop.H();
    for( uint i = 0; i < DlgboxButtonsCount; i++ )
    {
        if( IsCurInRect( DlgboxBButton, DlgboxX, DlgboxY + y_offs ) )
        {
            DlgboxSelectedButton = i;
            IfaceHold = IFACE_DIALOG_BTN;
            return;
        }
        if( IsCurInRect( DlgboxWMiddle, DlgboxX, DlgboxY + y_offs ) )
        {
            DlgboxVectX = GameOpt.MouseX - DlgboxX;
            DlgboxVectY = GameOpt.MouseY - DlgboxY;
            IfaceHold = IFACE_DIALOG_MAIN;
            return;
        }
        y_offs += DlgboxWMiddle.H();
    }

    if( IsCurInRect( DlgboxWTop, DlgboxX, DlgboxY ) || IsCurInRect( DlgboxWBottom, DlgboxX, DlgboxY + y_offs ) )
    {
        DlgboxVectX = GameOpt.MouseX - DlgboxX;
        DlgboxVectY = GameOpt.MouseY - DlgboxY;
        IfaceHold = IFACE_DIALOG_MAIN;
    }
}

void FOClient::DlgboxLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_DIALOG_BTN:
        if( !IsCurInRect( DlgboxBButton, DlgboxX, DlgboxY + DlgboxWTop.H() + DlgboxSelectedButton * DlgboxWMiddle.H() ) )
            break;

        if( DlgboxSelectedButton == DlgboxButtonsCount - 1 )
        {
            if( DlgboxType >= DIALOGBOX_ENCOUNTER_ANY && DlgboxType <= DIALOGBOX_ENCOUNTER_TB )
                Net_SendRuleGlobal( GM_CMD_ANSWER, -1 );
            // if(DlgboxType==DIALOGBOX_BARTER) Net_SendPlayersBarter(BARTER_END,PBarterPlayerId,true);
            DlgboxType = DIALOGBOX_NONE;
            ShowScreen( SCREEN_NONE );
            return;
        }

        if( DlgboxType == DIALOGBOX_FOLLOW )
        {
            Net_SendRuleGlobal( GM_CMD_FOLLOW, FollowRuleId );
        }
        else if( DlgboxType == DIALOGBOX_BARTER )
        {
            if( DlgboxSelectedButton == 0 )
                Net_SendPlayersBarter( BARTER_ACCEPTED, PBarterPlayerId, false );
            else
                Net_SendPlayersBarter( BARTER_ACCEPTED, PBarterPlayerId, true );
        }
        else if( DlgboxType == DIALOGBOX_ENCOUNTER_ANY )
        {
            if( DlgboxSelectedButton == 0 )
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
        else if( DlgboxType == DIALOGBOX_MANUAL )
        {
            if( ShowScreenType && ShowScreenNeedAnswer )
                Net_SendScreenAnswer( DlgboxSelectedButton, "" );
        }
        DlgboxType = DIALOGBOX_NONE;
        ShowScreen( SCREEN_NONE );
        break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::DlgboxMouseMove()
{
    if( IfaceHold == IFACE_DIALOG_MAIN )
    {
        DlgboxX = GameOpt.MouseX - DlgboxVectX;
        DlgboxY = GameOpt.MouseY - DlgboxVectY;

        int height = DlgboxWTop.H() + DlgboxButtonsCount* DlgboxWMiddle.H() + DlgboxWBottom.H();
        if( DlgboxX < 0 )
            DlgboxX = 0;
        if( DlgboxX + DlgboxWTop[ 2 ] > GameOpt.ScreenWidth )
            DlgboxX = GameOpt.ScreenWidth - DlgboxWTop[ 2 ];
        if( DlgboxY < 0 )
            DlgboxY = 0;
        if( DlgboxY + height > GameOpt.ScreenHeight )
            DlgboxY = GameOpt.ScreenHeight - height;
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::ElevatorDraw()
{
    if( ElevatorMainPic )
        SprMngr.DrawSprite( ElevatorMainPic, ElevatorMain[ 0 ] + ElevatorX, ElevatorMain[ 1 ] + ElevatorY );
    if( ElevatorExtPic )
        SprMngr.DrawSprite( ElevatorExtPic, ElevatorExt[ 0 ] + ElevatorX, ElevatorExt[ 1 ] + ElevatorY );
    if( ElevatorIndicatorAnim )
        SprMngr.DrawSprite( AnimGetCurSpr( ElevatorIndicatorAnim ), ElevatorIndicator[ 0 ] + ElevatorX, ElevatorIndicator[ 1 ] + ElevatorY );

    if( IfaceHold == IFACE_ELEVATOR_BTN || ElevatorAnswerDone )
    {
        for( uint i = 0; i < ElevatorButtonsCount; i++ )
        {
            Rect& r = ElevatorButtons[ i ];
            if( i == (uint) ElevatorSelectedButton && ElevatorButtonPicDown )
                SprMngr.DrawSprite( ElevatorButtonPicDown, r[ 0 ] + ElevatorX, r[ 1 ] + ElevatorY );
        }
    }
}

void FOClient::ElevatorLMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( !ElevatorAnswerDone && ( ElevatorSelectedButton = ElevatorGetCurButton() ) != -1 )
    {
        IfaceHold = IFACE_ELEVATOR_BTN;
    }
    else if( IsCurInRect( ElevatorMain, ElevatorX, ElevatorY ) )
    {
        ElevatorVectX = GameOpt.MouseX - ElevatorX;
        ElevatorVectY = GameOpt.MouseY - ElevatorY;
        IfaceHold = IFACE_ELEVATOR_MAIN;
    }
}

void FOClient::ElevatorLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_ELEVATOR_BTN:
        if( ElevatorAnswerDone )
            break;
        if( ElevatorSelectedButton != ElevatorGetCurButton() )
            break;
        if( ElevatorStartLevel + ElevatorSelectedButton != ElevatorCurrentLevel && ShowScreenType && ShowScreenNeedAnswer )
        {
            ElevatorAnswerDone = true;
            ElevatorSendAnswerTick = Timer::GameTick();
            int        diff = abs( (int) ElevatorCurrentLevel - int(ElevatorStartLevel + ElevatorSelectedButton) );
            AnyFrames* anim = AnimGetFrames( ElevatorIndicatorAnim );
            if( anim )
                ElevatorSendAnswerTick += anim->Ticks / anim->GetCnt() * ( anim->GetCnt() * Procent( ElevatorLevelsCount - 1, diff ) / 100 );
            AnimRun( ElevatorIndicatorAnim, ElevatorStartLevel + ElevatorSelectedButton < ElevatorCurrentLevel ? ANIMRUN_FROM_END : ANIMRUN_TO_END );
            return;
        }
        ShowScreen( SCREEN_NONE );
        break;
    default:
        break;
    }

    if( !ElevatorAnswerDone )
        ElevatorSelectedButton = -1;
    IfaceHold = IFACE_NONE;
}

void FOClient::ElevatorMouseMove()
{
    if( IfaceHold == IFACE_ELEVATOR_MAIN )
    {
        ElevatorX = GameOpt.MouseX - ElevatorVectX;
        ElevatorY = GameOpt.MouseY - ElevatorVectY;

        if( ElevatorX < 0 )
            ElevatorX = 0;
        if( ElevatorX + ElevatorMain[ 2 ] > GameOpt.ScreenWidth )
            ElevatorX = GameOpt.ScreenWidth - ElevatorMain[ 2 ];
        if( ElevatorY < 0 )
            ElevatorY = 0;
        if( ElevatorY + ElevatorMain[ 3 ] > GameOpt.ScreenHeight )
            ElevatorY = GameOpt.ScreenHeight - ElevatorMain[ 3 ];
    }
}

void FOClient::ElevatorGenerate( uint param )
{
    ElevatorMainPic = NULL;
    ElevatorExtPic = NULL;
    ElevatorButtonPicDown = NULL;
    ElevatorIndicatorAnim = 0;
    ElevatorButtonsCount = 0;
    ElevatorLevelsCount = 0;
    ElevatorStartLevel = 0;
    ElevatorCurrentLevel = 0;

    if( !Script::PrepareContext( ClientFunctions.GetElevator, _FUNC_, "Game" ) )
        return;
    ScriptArray* arr = Script::CreateArray( "int[]" );
    if( !arr )
        return;
    Script::SetArgUInt( param );
    Script::SetArgObject( arr );
    if( !Script::RunPrepared() || !Script::GetReturnedBool() )
    {
        arr->Release();
        return;
    }

    uint added_buttons = 0;
    uint main_pic = 0, ext_pic = 0, button_pic = 0;
    for( int i = 0, j = arr->GetSize(); i < j; i++ )
    {
        if( i > 100 )
            break;

        uint val = *(uint*) arr->At( i );
        switch( i )
        {
        case 0:
            ElevatorCurrentLevel = val;
            break;
        case 1:
            ElevatorStartLevel = val;
            break;
        case 2:
            ElevatorLevelsCount = val;
            break;
        case 3:
            main_pic = val;
            break;
        case 4:
            ElevatorMain.R = val;
            break;
        case 5:
            ElevatorMain.B = val;
            break;
        case 6:
            ext_pic = val;
            break;
        case 7:
            ElevatorExt.L = val;
            break;
        case 8:
            ElevatorExt.T = val;
            break;
        case 9:
            ElevatorIndicatorAnim = val;
            break;
        case 10:
            ElevatorIndicator.L = val;
            break;
        case 11:
            ElevatorIndicator.T = val;
            break;
        case 12:
            button_pic = val;
            break;
        case 13:
            ElevatorButtonsCount = val;
            break;
        default:
            ElevatorButtons[ ( i - 14 ) / 4 ][ ( i - 14 ) % 4 ] = val;
            if( ( i - 14 ) % 4 == 3 )
                added_buttons++;
            break;
        }
    }

    arr->Release();
    if( !ElevatorLevelsCount || ElevatorLevelsCount > MAX_DLGBOX_BUTTONS )
        return;
    if( ElevatorCurrentLevel < ElevatorStartLevel || ElevatorCurrentLevel >= ElevatorStartLevel + ElevatorLevelsCount )
        return;
    if( ElevatorButtonsCount > MAX_DLGBOX_BUTTONS || ElevatorButtonsCount != added_buttons )
        return;
    if( main_pic && !( ElevatorMainPic = ResMngr.GetIfaceAnim( main_pic ) ) )
        return;
    if( ext_pic && !( ElevatorExtPic = ResMngr.GetIfaceAnim( ext_pic ) ) )
        return;
    if( button_pic && !( ElevatorButtonPicDown = ResMngr.GetIfaceAnim( button_pic ) ) )
        return;
    if( ElevatorIndicatorAnim && !( ElevatorIndicatorAnim = AnimLoad( ElevatorIndicatorAnim, RES_ATLAS_STATIC ) ) )
        return;

    AnimRun( ElevatorIndicatorAnim, ANIMRUN_SET_FRM( AnimGetSprCount( ElevatorIndicatorAnim ) * Procent( ElevatorLevelsCount - 1, ElevatorCurrentLevel - ElevatorStartLevel ) / 100 ) | ANIMRUN_STOP );
    ElevatorX = ( GameOpt.ScreenWidth - ElevatorMain.W() ) / 2;
    ElevatorY = ( GameOpt.ScreenHeight - ElevatorMain.H() ) / 2;
    ElevatorAnswerDone = false;
    ElevatorSendAnswerTick = 0;
    ShowScreen( SCREEN__ELEVATOR );
}

void FOClient::ElevatorProcess()
{
    if( ElevatorAnswerDone && Timer::GameTick() >= ElevatorSendAnswerTick )
    {
        AnimRun( ElevatorIndicatorAnim, ANIMRUN_SET_FRM( AnimGetSprCount( ElevatorIndicatorAnim ) * Procent( ElevatorLevelsCount - 1, ElevatorSelectedButton ) / 100 ) | ANIMRUN_STOP );
        if( ShowScreenNeedAnswer )
        {
            Net_SendScreenAnswer( ElevatorStartLevel + ElevatorSelectedButton, "" );
            ShowScreenNeedAnswer = false;
            ElevatorSendAnswerTick += 1000;
            WaitPing();
        }
        else
        {
            ShowScreen( SCREEN_NONE );
        }
    }
}

int FOClient::ElevatorGetCurButton()
{
    if( ElevatorButtonPicDown )
    {
        for( uint i = 0; i < ElevatorButtonsCount; i++ )
            if( IsCurInRectNoTransp( ElevatorButtonPicDown->GetCurSprId(), ElevatorButtons[ i ], ElevatorX, ElevatorY ) )
                return i;
    }
    return -1;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::SayDraw()
{
    SprMngr.DrawSprite( SayWMainPicNone, SayWMain[ 0 ] + SayX, SayWMain[ 1 ] + SayY );
    switch( IfaceHold )
    {
    case IFACE_SAY_OK:
        SprMngr.DrawSprite( SayBOkPicDown, SayBOk[ 0 ] + SayX, SayBOk[ 1 ] + SayY );
        break;
    case IFACE_SAY_CANCEL:
        SprMngr.DrawSprite( SayBCancelPicDown, SayBCancel[ 0 ] + SayX, SayBCancel[ 1 ] + SayY );
        break;
    default:
        break;
    }

    SprMngr.DrawStr( Rect( SayWMainText, SayX, SayY ), SayTitle.c_str(), FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SayBOkText, SayX, SayY ), MsgGame->GetStr( STR_SAY_OK ), FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SayBCancelText, SayX, SayY ), MsgGame->GetStr( STR_SAY_CANCEL ), FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SayWSay, SayX, SayY ), SayText.c_str(), FT_NOBREAK | FT_CENTERY );
}

void FOClient::SayLMouseDown()
{
    IfaceHold = IFACE_NONE;
    if( !IsCurInRect( SayWMain, SayX, SayY ) )
        return;

    if( IsCurInRect( SayBOk, SayX, SayY ) )
        IfaceHold = IFACE_SAY_OK;
    else if( IsCurInRect( SayBCancel, SayX, SayY ) )
        IfaceHold = IFACE_SAY_CANCEL;

    if( IfaceHold != IFACE_NONE )
        return;
    SayVectX = GameOpt.MouseX - SayX;
    SayVectY = GameOpt.MouseY - SayY;
    IfaceHold = IFACE_SAY_MAIN;
}

void FOClient::SayLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_SAY_OK:
        if( !IsCurInRect( SayBOk, SayX, SayY ) )
            break;
        if( SayText.empty() )
            break;
        if( ShowScreenType )
        {
            if( ShowScreenNeedAnswer )
                Net_SendScreenAnswer( 0, SayText.c_str() );
        }
        else
        {
            if( SayType == DIALOGSAY_TEXT )
                Net_SendSayNpc( DlgIsNpc, DlgNpcId, SayText.c_str() );
            else if( SayType == DIALOGSAY_SAVE )
                SaveLoadSaveGame( SayText.c_str() );
        }
        ShowScreen( SCREEN_NONE );
        WaitPing();
        return;
    case IFACE_SAY_CANCEL:
        if( !IsCurInRect( SayBCancel, SayX, SayY ) )
            break;
        ShowScreen( SCREEN_NONE );
        return;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::SayMouseMove()
{
    if( IfaceHold == IFACE_SAY_MAIN )
    {
        SayX = GameOpt.MouseX - SayVectX;
        SayY = GameOpt.MouseY - SayVectY;

        if( SayX < 0 )
            SayX = 0;
        if( SayX + SayWMain[ 2 ] > GameOpt.ScreenWidth )
            SayX = GameOpt.ScreenWidth - SayWMain[ 2 ];
        if( SayY < 0 )
            SayY = 0;
        if( SayY + SayWMain[ 3 ] > GameOpt.ScreenHeight )
            SayY = GameOpt.ScreenHeight - SayWMain[ 3 ];
    }
}

void FOClient::SayKeyDown( uchar dik, const char* dik_text )
{
    if( dik == DIK_RETURN || dik == DIK_NUMPADENTER )
    {
        if( SayText.empty() )
            return;
        if( ShowScreenType )
        {
            if( ShowScreenNeedAnswer )
                Net_SendScreenAnswer( 0, SayText.c_str() );
        }
        else
        {
            if( SayType == DIALOGSAY_TEXT )
                Net_SendSayNpc( DlgIsNpc, DlgNpcId, SayText.c_str() );
            else if( SayType == DIALOGSAY_SAVE )
                SaveLoadSaveGame( SayText.c_str() );
        }
        ShowScreen( SCREEN_NONE );
        WaitPing();
        return;
    }

    if( SayType == DIALOGSAY_TEXT )
        Keyb::GetChar( dik, dik_text, SayText, NULL, MAX_SAY_NPC_TEXT, SayOnlyNumbers ? KIF_ONLY_NUMBERS : KIF_NO_SPEC_SYMBOLS );
    else if( SayType == DIALOGSAY_SAVE )
        Keyb::GetChar( dik, dik_text, SayText, NULL, MAX_FOPATH, SayOnlyNumbers ? KIF_ONLY_NUMBERS : KIF_FILE_NAME );
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::WaitDraw()
{
    SprMngr.DrawSpriteSize( WaitPic, 0, 0, GameOpt.ScreenWidth, GameOpt.ScreenHeight, true, true );
    SprMngr.Flush();
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::SplitStart( Item* item, int to_cont )
{
    SplitItemId = item->GetId();
    SplitCont = to_cont;
    SplitValue = 1;
    SplitMinValue = 1;
    SplitMaxValue = item->GetCount();
    SplitValueKeyPressed = false;
    SplitItemPic = ResMngr.GetInvAnim( item->GetActualPicInv() );
    SplitItemColor = item->GetInvColor();
    SplitParentScreen = GetActiveScreen();

    ShowScreen( SCREEN__SPLIT );
}

void FOClient::SplitClose( bool change )
{
    if( change && ( SplitValue < SplitMinValue || SplitValue > SplitMaxValue ) )
        return;

    if( ( SplitCont & 0xFFFF ) == 0xFF )
        goto label_DropItems;
    switch( SplitParentScreen )
    {
    case SCREEN__INVENTORY:
        IfaceHold = IFACE_NONE;
label_DropItems:
        if( !change )
            break;
        AddActionBack( CHOSEN_MOVE_ITEM, SplitItemId, SplitValue, SplitCont & 0xFFFF, ( SplitCont >> 16 ) & 1 );
        break;
    case SCREEN__BARTER:
        IfaceHold = IFACE_NONE;
        BarterHoldId = 0;
        if( !change )
            break;
        BarterTransfer( SplitItemId, SplitCont & 0xFFFF, SplitValue );
        break;
    case SCREEN__PICKUP:
        IfaceHold = IFACE_NONE;
        PupHoldId = 0;
        if( !change )
            break;
        SetAction( CHOSEN_MOVE_ITEM_CONT, SplitItemId, SplitCont & 0xFFFF, SplitValue );
        break;
    default:
        break;
    }

    SplitParentScreen = SCREEN_NONE;
    ShowScreen( SCREEN_NONE, NULL );
}

void FOClient::SplitDraw()
{
    SprMngr.DrawSprite( SplitMainPic, SplitWMain[ 0 ] + SplitX, SplitWMain[ 1 ] + SplitY );

    switch( IfaceHold )
    {
    case IFACE_SPLIT_UP:
        SprMngr.DrawSprite( SplitPBUpDn, SplitBUp[ 0 ] + SplitX, SplitBUp[ 1 ] + SplitY );
        break;
    case IFACE_SPLIT_DOWN:
        SprMngr.DrawSprite( SplitPBDnDn, SplitBDown[ 0 ] + SplitX, SplitBDown[ 1 ] + SplitY );
        break;
    case IFACE_SPLIT_ALL:
        SprMngr.DrawSprite( SplitPBAllDn, SplitBAll[ 0 ] + SplitX, SplitBAll[ 1 ] + SplitY );
        break;
    case IFACE_SPLIT_DONE:
        SprMngr.DrawSprite( SplitPBDoneDn, SplitBDone[ 0 ] + SplitX, SplitBDone[ 1 ] + SplitY );
        break;
    case IFACE_SPLIT_CANCEL:
        SprMngr.DrawSprite( SplitPBCancelDn, SplitBCancel[ 0 ] + SplitX, SplitBCancel[ 1 ] + SplitY );
        break;
    default:
        break;
    }

    if( SplitItemPic )
        SprMngr.DrawSpriteSize( SplitItemPic, SplitWItem[ 0 ] + SplitX, SplitWItem[ 1 ] + SplitY, SplitWItem.W(), SplitWItem.H(), false, true, SplitItemColor );

    SprMngr.DrawStr( Rect( SplitWTitle, SplitX, SplitY ), MsgGame->GetStr( STR_SPLIT_TITLE ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SplitBAll, SplitX, SplitY - ( IfaceHold == IFACE_SPLIT_ALL ? 1 : 0 ) ), MsgGame->GetStr( STR_SPLIT_ALL ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( SplitWValue, SplitX, SplitY ), Str::FormatBuf( "%05d", SplitValue ), FT_NOBREAK, COLOR_IFACE, FONT_BIG_NUM );
}

void FOClient::SplitKeyDown( uchar dik, const char* dik_text )
{
    int add = 0;

    switch( dik )
    {
    case DIK_RETURN:
    case DIK_NUMPADENTER:
        SplitClose( true );
        return;
    case DIK_ESCAPE:
        SplitClose( false );
        return;
    case DIK_BACK:
        add = -1;
        break;
    case DIK_DELETE:
        add = -2;
        break;
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
    default:
        return;
    }

    if( add == -1 )
        SplitValue /= 10;
    else if( add == -2 )
        SplitValue = 0;
    else if( !SplitValueKeyPressed )
        SplitValue = add;
    else
        SplitValue = SplitValue * 10 + add;

    SplitValueKeyPressed = true;
    if( SplitValue >= MAX_SPLIT_VALUE )
        SplitValue = SplitValue % MAX_SPLIT_VALUE;
}

void FOClient::SplitLMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( !IsCurInRect( SplitWMain, SplitX, SplitY ) )
        return;

    if( IsCurInRect( SplitBUp, SplitX, SplitY ) )
    {
        Timer::StartAccelerator( ACCELERATE_SPLIT_UP );
        IfaceHold = IFACE_SPLIT_UP;
    }
    else if( IsCurInRect( SplitBDown, SplitX, SplitY ) )
    {
        Timer::StartAccelerator( ACCELERATE_SPLIT_DOWN );
        IfaceHold = IFACE_SPLIT_DOWN;
    }
    else if( IsCurInRect( SplitBAll, SplitX, SplitY ) )
        IfaceHold = IFACE_SPLIT_ALL;
    else if( IsCurInRect( SplitBDone, SplitX, SplitY ) )
        IfaceHold = IFACE_SPLIT_DONE;
    else if( IsCurInRect( SplitBCancel, SplitX, SplitY ) )
        IfaceHold = IFACE_SPLIT_CANCEL;
    else
    {
        SplitVectX = GameOpt.MouseX - SplitX;
        SplitVectY = GameOpt.MouseY - SplitY;
        IfaceHold = IFACE_SPLIT_MAIN;
    }
}

void FOClient::SplitLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_SPLIT_UP:
        if( !IsCurInRect( SplitBUp, SplitX, SplitY ) )
            break;
        if( SplitValue < SplitMaxValue )
            SplitValue++;
        break;
    case IFACE_SPLIT_DOWN:
        if( !IsCurInRect( SplitBDown, SplitX, SplitY ) )
            break;
        if( SplitValue > SplitMinValue )
            SplitValue--;
        break;
    case IFACE_SPLIT_ALL:
        if( !IsCurInRect( SplitBAll, SplitX, SplitY ) )
            break;
        SplitValue = SplitMaxValue;
        if( SplitValue >= MAX_SPLIT_VALUE )
            SplitValue = MAX_SPLIT_VALUE - 1;
        break;
    case IFACE_SPLIT_DONE:
        if( !IsCurInRect( SplitBDone, SplitX, SplitY ) )
            break;
        SplitClose( true );
        break;
    case IFACE_SPLIT_CANCEL:
        if( !IsCurInRect( SplitBCancel, SplitX, SplitY ) )
            break;
        SplitClose( false );
        break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::SplitMouseMove()
{
    if( IfaceHold == IFACE_SPLIT_MAIN )
    {
        SplitX = GameOpt.MouseX - SplitVectX;
        SplitY = GameOpt.MouseY - SplitVectY;

        if( SplitX < 0 )
            SplitX = 0;
        if( SplitX + SplitWMain[ 2 ] > GameOpt.ScreenWidth )
            SplitX = GameOpt.ScreenWidth - SplitWMain[ 2 ];
        if( SplitY < 0 )
            SplitY = 0;
        if( SplitY + SplitWMain[ 3 ] > GameOpt.ScreenHeight )
            SplitY = GameOpt.ScreenHeight - SplitWMain[ 3 ];
    }
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::TimerStart( uint item_id, AnyFrames* pic, uint pic_color )
{
    TimerItemId = item_id;
    TimerValue = TIMER_MIN_VALUE;
    TimerItemPic = pic;
    TimerItemColor = pic_color;

    ShowScreen( SCREEN__TIMER );
}

void FOClient::TimerClose( bool done )
{
    if( done )
    {
        if( ShowScreenType )
        {
            if( ShowScreenNeedAnswer )
                Net_SendScreenAnswer( TimerValue, "" );
        }
        else
            AddActionBack( CHOSEN_USE_ITEM, TimerItemId, 0, TARGET_SELF, 0, USE_USE, TimerValue );
    }
    ShowScreen( SCREEN_NONE, NULL );
}

void FOClient::TimerDraw()
{
    SprMngr.DrawSprite( TimerMainPic, TimerWMain[ 0 ] + TimerX, TimerWMain[ 1 ] + TimerY );

    switch( IfaceHold )
    {
    case IFACE_TIMER_UP:
        SprMngr.DrawSprite( TimerBUpPicDown, TimerBUp[ 0 ] + TimerX, TimerBUp[ 1 ] + TimerY );
        break;
    case IFACE_TIMER_DOWN:
        SprMngr.DrawSprite( TimerBDownPicDown, TimerBDown[ 0 ] + TimerX, TimerBDown[ 1 ] + TimerY );
        break;
    case IFACE_TIMER_DONE:
        SprMngr.DrawSprite( TimerBDonePicDown, TimerBDone[ 0 ] + TimerX, TimerBDone[ 1 ] + TimerY );
        break;
    case IFACE_TIMER_CANCEL:
        SprMngr.DrawSprite( TimerBCancelPicDown, TimerBCancel[ 0 ] + TimerX, TimerBCancel[ 1 ] + TimerY );
        break;
    default:
        break;
    }

    if( TimerItemPic )
        SprMngr.DrawSpriteSize( TimerItemPic, TimerWItem[ 0 ] + TimerX, TimerWItem[ 1 ] + TimerY, TimerWItem.W(), TimerWItem.H(), false, true, TimerItemColor );
    SprMngr.Flush();

    SprMngr.DrawStr( Rect( TimerWTitle, TimerX, TimerY ), MsgGame->GetStr( STR_TIMER_TITLE ), FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( TimerWValue, TimerX, TimerY ), Str::FormatBuf( "%d%c%02d", TimerValue / 60, '9' + 3, TimerValue % 60 ), FT_NOBREAK, COLOR_IFACE, FONT_BIG_NUM );
}

void FOClient::TimerKeyDown( uchar dik, const char* dik_text )
{
    switch( dik )
    {
    case DIK_RETURN:
    case DIK_NUMPADENTER:
        TimerClose( true );
        return;
    case DIK_ESCAPE:
        TimerClose( false );
        return;
    // Numetric +, -
    // Arrow Right, Left
    case DIK_EQUALS:
    case DIK_ADD:
    case DIK_RIGHT:
        TimerValue += 1;
        break;
    case DIK_MINUS:
    case DIK_SUBTRACT:
    case DIK_LEFT:
        TimerValue -= 1;
        break;
    // PageUp, PageDown
    case DIK_PRIOR:
        TimerValue += 60;
        break;
    case DIK_NEXT:
        TimerValue -= 60;
        break;
    // Arrow Up, Down
    case DIK_UP:
        TimerValue += 10;
        break;
    case DIK_DOWN:
        TimerValue -= 10;
        break;
    // End, Home
    case DIK_END:
        TimerValue = TIMER_MAX_VALUE;
        break;
    case DIK_HOME:
        TimerValue = TIMER_MIN_VALUE;
        break;
    default:
        return;
    }

    if( TimerValue < TIMER_MIN_VALUE )
        TimerValue = TIMER_MIN_VALUE;
    if( TimerValue > TIMER_MAX_VALUE )
        TimerValue = TIMER_MAX_VALUE;
}

void FOClient::TimerLMouseDown()
{
    IfaceHold = IFACE_NONE;

    if( !IsCurInRect( TimerWMain, TimerX, TimerY ) )
        return;

    if( IsCurInRect( TimerBUp, TimerX, TimerY ) )
    {
        Timer::StartAccelerator( ACCELERATE_TIMER_UP );
        IfaceHold = IFACE_TIMER_UP;
    }
    else if( IsCurInRect( TimerBDown, TimerX, TimerY ) )
    {
        Timer::StartAccelerator( ACCELERATE_TIMER_DOWN );
        IfaceHold = IFACE_TIMER_DOWN;
    }
    else if( IsCurInRect( TimerBDone, TimerX, TimerY ) )
        IfaceHold = IFACE_TIMER_DONE;
    else if( IsCurInRect( TimerBCancel, TimerX, TimerY ) )
        IfaceHold = IFACE_TIMER_CANCEL;
    else
    {
        TimerVectX = GameOpt.MouseX - TimerX;
        TimerVectY = GameOpt.MouseY - TimerY;
        IfaceHold = IFACE_TIMER_MAIN;
    }
}

void FOClient::TimerLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_TIMER_UP:
        if( !IsCurInRect( TimerBUp, TimerX, TimerY ) )
            break;
        if( TimerValue < TIMER_MAX_VALUE )
            TimerValue++;
        break;
    case IFACE_TIMER_DOWN:
        if( !IsCurInRect( TimerBDown, TimerX, TimerY ) )
            break;
        if( TimerValue > TIMER_MIN_VALUE )
            TimerValue--;
        break;
    case IFACE_TIMER_DONE:
        if( !IsCurInRect( TimerBDone, TimerX, TimerY ) )
            break;
        TimerClose( true );
        break;
    case IFACE_TIMER_CANCEL:
        if( !IsCurInRect( TimerBCancel, TimerX, TimerY ) )
            break;
        TimerClose( false );
        break;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::TimerMouseMove()
{
    if( IfaceHold == IFACE_TIMER_MAIN )
    {
        TimerX = GameOpt.MouseX - TimerVectX;
        TimerY = GameOpt.MouseY - TimerVectY;

        if( TimerX < 0 )
            TimerX = 0;
        if( TimerX + TimerWMain[ 2 ] > GameOpt.ScreenWidth )
            TimerX = GameOpt.ScreenWidth - TimerWMain[ 2 ];
        if( TimerY < 0 )
            TimerY = 0;
        if( TimerY + TimerWMain[ 3 ] > GameOpt.ScreenHeight )
            TimerY = GameOpt.ScreenHeight - TimerWMain[ 3 ];
    }
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
                str += Str::ItoA( Chosen->Props.GetValueAsInt( craft->NeedPNum[ i ] ) );
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

        ProtoItem* proto = ItemMngr.GetProtoItem( items_vec[ i ] );
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
        ProtoItem* proto = ItemMngr.GetProtoItem( items_vec[ i ] );
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
        return NULL;
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

void FOClient::IboxDraw()
{
    SprMngr.DrawSprite( IboxWMainPicNone, IboxWMain[ 0 ] + IboxX, IboxWMain[ 1 ] + IboxY );

    switch( IfaceHold )
    {
    case IFACE_IBOX_DONE:
        SprMngr.DrawSprite( IboxBDonePicDown, IboxBDone[ 0 ] + IboxX, IboxBDone[ 1 ] + IboxY );
        break;
    case IFACE_IBOX_CANCEL:
        SprMngr.DrawSprite( IboxBCancelPicDown, IboxBCancel[ 0 ] + IboxX, IboxBCancel[ 1 ] + IboxY );
        break;
    default:
        break;
    }

    char* buf = (char*) Str::FormatBuf( "%s", IboxTitle.c_str() );
    if( IfaceHold == IFACE_IBOX_TITLE )
        Str::Insert( &buf[ IboxTitleCur ], Timer::FastTick() % 798 < 399 ? "!" : "." );
    SprMngr.DrawStr( Rect( IboxWTitle, IboxX, IboxY ), buf, FT_NOBREAK | FT_CENTERY );
    buf = (char*) Str::FormatBuf( "%s", IboxText.c_str() );
    if( IfaceHold == IFACE_IBOX_TEXT )
        Str::Insert( &buf[ IboxTextCur ], Timer::FastTick() % 798 < 399 ? "." : "!" );
    SprMngr.DrawStr( Rect( IboxWText, IboxX, IboxY ), buf, 0 );

    SprMngr.DrawStr( Rect( IboxBDoneText, IboxX, IboxY ), MsgGame->GetStr( STR_INPUT_BOX_WRITE ), FT_NOBREAK | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
    SprMngr.DrawStr( Rect( IboxBCancelText, IboxX, IboxY ), MsgGame->GetStr( STR_INPUT_BOX_BACK ), FT_NOBREAK | FT_CENTERY, COLOR_TEXT_SAND, FONT_FAT );
}

void FOClient::IboxLMouseDown()
{
    IfaceHold = IFACE_NONE;
    if( !IsCurInRect( IboxWMain, IboxX, IboxY ) )
        return;

    if( IsCurInRect( IboxWTitle, IboxX, IboxY ) )
        IfaceHold = IFACE_IBOX_TITLE;
    else if( IsCurInRect( IboxWText, IboxX, IboxY ) )
        IfaceHold = IFACE_IBOX_TEXT;
    else if( IsCurInRect( IboxBDone, IboxX, IboxY ) )
        IfaceHold = IFACE_IBOX_DONE;
    else if( IsCurInRect( IboxBCancel, IboxX, IboxY ) )
        IfaceHold = IFACE_IBOX_CANCEL;

    if( IfaceHold == IFACE_NONE )
    {
        IboxVectX = GameOpt.MouseX - IboxX;
        IboxVectY = GameOpt.MouseY - IboxY;
        IfaceHold = IFACE_IBOX_MAIN;
    }
}

void FOClient::IboxLMouseUp()
{
    switch( IfaceHold )
    {
    case IFACE_IBOX_DONE:
    {
        if( !IsCurInRect( IboxBDone, IboxX, IboxY ) )
            break;
        AddActionBack( CHOSEN_WRITE_HOLO, IboxHolodiskId );
        ShowScreen( SCREEN_NONE );
    }
    break;
    case IFACE_IBOX_CANCEL:
    {
        if( !IsCurInRect( IboxBCancel, IboxX, IboxY ) )
            break;
        ShowScreen( SCREEN_NONE );
    }
    break;
    case IFACE_IBOX_TITLE:
    case IFACE_IBOX_TEXT:
        return;
    default:
        break;
    }

    IfaceHold = IFACE_NONE;
}

void FOClient::IboxKeyDown( uchar dik, const char* dik_text )
{
    if( IfaceHold == IFACE_IBOX_TITLE )
        Keyb::GetChar( dik, dik_text, IboxTitle, &IboxTitleCur, USER_HOLO_MAX_TITLE_LEN, KIF_NO_SPEC_SYMBOLS );
    else if( IfaceHold == IFACE_IBOX_TEXT )
        Keyb::GetChar( dik, dik_text, IboxText, &IboxTextCur, USER_HOLO_MAX_LEN, 0 );
    else
        return;
    if( dik == DIK_PAUSE )
        return;
    IboxLastKey = dik;
    IboxLastKeyText = dik_text;
    Timer::StartAccelerator( ACCELERATE_IBOX );
}

void FOClient::IboxKeyUp( uchar dik )
{
    IboxLastKey = 0;
    IboxLastKeyText = "";
}

void FOClient::IboxProcess()
{
    if( IboxLastKey && Timer::ProcessAccelerator( ACCELERATE_IBOX ) )
    {
        if( IfaceHold == IFACE_IBOX_TITLE )
            Keyb::GetChar( IboxLastKey, IboxLastKeyText.c_str(), IboxTitle, &IboxTitleCur, USER_HOLO_MAX_TITLE_LEN, KIF_NO_SPEC_SYMBOLS );
        else if( IfaceHold == IFACE_IBOX_TEXT )
            Keyb::GetChar( IboxLastKey, IboxLastKeyText.c_str(), IboxText, &IboxTextCur, USER_HOLO_MAX_LEN, 0 );
    }
}

void FOClient::IboxMouseMove()
{
    if( IfaceHold == IFACE_IBOX_MAIN )
    {
        IboxX = GameOpt.MouseX - IboxVectX;
        IboxY = GameOpt.MouseY - IboxVectY;

        if( IboxX < 0 )
            IboxX = 0;
        if( IboxX + IboxWMain[ 2 ] > GameOpt.ScreenWidth )
            IboxX = GameOpt.ScreenWidth - IboxWMain[ 2 ];
        if( IboxY < 0 )
            IboxY = 0;
        if( IboxY + IboxWMain[ 3 ] > GameOpt.ScreenHeight )
            IboxY = GameOpt.ScreenHeight - IboxWMain[ 3 ];
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
    FileManager::GetFolderFileNames( FileManager::GetWritePath( "", PT_SAVE ), true, "fo", fnames );
    PtrVec open_handles;
    for( uint i = 0; i < fnames.size(); i++ )
    {
        const string& fname = fnames[ i ];

        // Open file
        void* f = FileOpen( FileManager::GetWritePath( fname.c_str(), PT_SAVE ), false );
        if( !f )
            continue;
        open_handles.push_back( f );

        // Get file information
        uint64 tw = FileGetWriteTime( f );

        // Read save data, offsets see SaveGameInfoFile in Server.cpp
        // Check singleplayer data
        uint sp;
        FileSetPointer( f, 4, SEEK_SET );
        if( !FileRead( f, &sp, sizeof( sp ) ) )
            continue;
        if( sp == 0 )
            continue;               // Save not contain singleplayer data

        // Critter name
        uint crname_size = UTF8_BUF_SIZE( MAX_NAME );
        char crname[ UTF8_BUF_SIZE( MAX_NAME ) ];
        FileSetPointer( f, 8, SEEK_SET );
        if( sp >= SINGLEPLAYER_SAVE_V2 )
        {
            if( !FileRead( f, crname, sizeof( crname ) ) )
                continue;
        }
        else
        {
            crname_size = MAX_NAME + 1;
            if( !FileRead( f, crname, MAX_NAME + 1 ) )
                continue;
            for( char* name = crname; *name; name++ )
                *name = ( ( ( *name >= 'A' && *name <= 'Z' ) || ( *name >= 'a' && *name <= 'z' ) ) ? *name : 'X' );
        }

        // Map pid
        hash map_pid;
        FileSetPointer( f, 8 + crname_size + 68, SEEK_SET );
        if( !FileRead( f, &map_pid, sizeof( map_pid ) ) )
            continue;

        // Calculate critter time events size
        uint te_size;
        FileSetPointer( f, 8 + crname_size + 7404 + 6944, SEEK_SET );
        if( !FileRead( f, &te_size, sizeof( te_size ) ) )
            continue;
        te_size = te_size * 16 + 4;

        // Picture data
        uint     pic_data_len;
        UCharVec pic_data;
        FileSetPointer( f, 8 + crname_size + 7404 + 6944 + te_size, SEEK_SET );
        if( !FileRead( f, &pic_data_len, sizeof( pic_data_len ) ) )
            continue;
        if( pic_data_len )
        {
            pic_data.resize( pic_data_len );
            if( !FileRead( f, &pic_data[ 0 ], pic_data_len ) )
                continue;
        }

        // Game time
        ushort year, month, day, hour, minute;
        FileSetPointer( f, 8 + crname_size + 7404 + 6944 + te_size + 4 + pic_data_len + 2, SEEK_SET );
        if( !FileRead( f, &year, sizeof( year ) ) )
            continue;
        if( !FileRead( f, &month, sizeof( month ) ) )
            continue;
        if( !FileRead( f, &day, sizeof( day ) ) )
            continue;
        if( !FileRead( f, &hour, sizeof( hour ) ) )
            continue;
        if( !FileRead( f, &minute, sizeof( minute ) ) )
            continue;

        // Extract name
        char name[ MAX_FOPATH ];
        Str::Copy( name, fname.c_str() );
        if( Str::Length( name ) < 4 )
            continue;
        name[ Str::Length( name ) - 3 ] = 0; // Cut '.fo'

        // Extract full path
        char fname_ex[ MAX_FOPATH ];
        FileManager::GetWritePath( name, PT_SAVE, fname_ex );
        ResolvePath( fname_ex );
        Str::Append( fname_ex, ".foworld" );

        // Convert time
        DateTimeStamp dt;
        Timer::FullTimeToDateTime( tw, dt );

        // Fill slot data
        SaveLoadDataSlot slot;
        slot.Name = name;
        slot.Info = Str::FormatBuf( "%s\n%02d.%02d.%04d %02d:%02d:%02d\n", name, dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second );
        slot.InfoExt = Str::FormatBuf( "%s\n%02d %3s %04d %02d%02d\n%s", crname,
                                       day, MsgGame->GetStr( STR_MONTH( month ) ), year, hour, minute, MsgLocations->GetStr( STR_LOC_MAP_NAME( CurMapLocPid, CurMapIndexInLoc ) ) );
        slot.FileName = fname_ex;
        slot.RealTime = tw;
        slot.PicData = pic_data;
        SaveLoadDataSlots.push_back( slot );
    }

    // Close opened file handles
    for( auto it = open_handles.begin(); it != open_handles.end(); ++it )
        FileClose( *it );

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

        ShowScreen( SCREEN__SAY );
        SayType = DIALOGSAY_SAVE;
        SayTitle = MsgGame->GetStr( STR_SAVE_LOAD_TYPE_RECORD_NAME );
        SayText = "";
        SayOnlyNumbers = false;

        SaveLoadFileName = "";
        if( SaveLoadSlotIndex >= 0 && SaveLoadSlotIndex < (int) SaveLoadDataSlots.size() )
        {
            SayText = SaveLoadDataSlots[ SaveLoadSlotIndex ].Name;
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
            Net_SendSaveLoad( false, SaveLoadDataSlots[ SaveLoadSlotIndex ].FileName.c_str(), NULL );
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
        SprMngr.DrawRenderTarget( SaveLoadDraft, false, NULL, &dst );
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
    {
        IfaceHold = IFACE_SAVELOAD_SCR_UP;
        Timer::StartAccelerator( ACCELERATE_SAVE_LOAD_SCR_UP );
    }
    else if( IsCurInRect( SaveLoadScrDown, ox, oy ) )
    {
        IfaceHold = IFACE_SAVELOAD_SCR_DN;
        Timer::StartAccelerator( ACCELERATE_SAVE_LOAD_SCR_DN );
    }
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
