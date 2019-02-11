#include "Common.h"
#include "HexManager.h"
#include "ResourceManager.h"
#include "LineTracer.h"
#include "ProtoManager.h"

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
# include "Script.h"
#endif

/************************************************************************/
/* FIELD                                                                */
/************************************************************************/

Field::~Field()
{
    SAFEDEL( DeadCrits );
    SAFEDEL( Tiles[ 0 ] );
    SAFEDEL( Tiles[ 1 ] );
    SAFEDEL( Items );
    SAFEDEL( BlockLinesItems );
}

void Field::AddItem( ItemHex* item, ItemHex* block_lines_item )
{
    RUNTIME_ASSERT( item || block_lines_item );

    if( item )
    {
        item->HexScrX = &ScrX;
        item->HexScrY = &ScrY;

        if( !Items )
            Items = new ItemHexVec();
        Items->push_back( item );
    }
    if( block_lines_item )
    {
        if( !BlockLinesItems )
            BlockLinesItems = new ItemHexVec();
        BlockLinesItems->push_back( block_lines_item );
    }

    ProcessCache();
}

void Field::EraseItem( ItemHex* item, ItemHex* block_lines_item )
{
    RUNTIME_ASSERT( item || block_lines_item );

    if( item )
    {
        RUNTIME_ASSERT( Items );
        auto it = std::find( Items->begin(), Items->end(), item );
        RUNTIME_ASSERT( it != Items->end() );
        Items->erase( it );
        if( Items->empty() )
            SAFEDEL( Items );
    }
    if( block_lines_item )
    {
        RUNTIME_ASSERT( BlockLinesItems );
        auto it = std::find( BlockLinesItems->begin(), BlockLinesItems->end(), block_lines_item );
        RUNTIME_ASSERT( it != BlockLinesItems->end() );
        BlockLinesItems->erase( it );
        if( BlockLinesItems->empty() )
            SAFEDEL( BlockLinesItems );
    }

    ProcessCache();
}

Field::Tile& Field::AddTile( AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof )
{
    Tile*       tile;
    TileVec*&   tiles_vec = Tiles[ is_roof ? 1 : 0 ];
    AnyFrames*& stile = SimplyTile[ is_roof ? 1 : 0 ];
    if( !tiles_vec && !stile && !ox && !oy && !layer )
    {
        static Tile simply_tile;
        stile = anim;
        tile = &simply_tile;
    }
    else
    {
        if( !tiles_vec )
            tiles_vec = new TileVec();
        tiles_vec->push_back( Tile() );
        tile = &tiles_vec->back();
    }

    tile->Anim = anim;
    tile->OffsX = ox;
    tile->OffsY = oy;
    tile->Layer = layer;
    return *tile;
}

void Field::EraseTile( uint index, bool is_roof )
{
    TileVec*&   tiles_vec = Tiles[ is_roof ? 1 : 0 ];
    AnyFrames*& stile = SimplyTile[ is_roof ? 1 : 0 ];
    if( index == 0 && stile )
    {
        stile = nullptr;
        if( tiles_vec && !tiles_vec->front().OffsX && !tiles_vec->front().OffsY && !tiles_vec->front().Layer )
        {
            stile = tiles_vec->front().Anim;
            tiles_vec->erase( tiles_vec->begin() );
        }
    }
    else
    {
        tiles_vec->erase( tiles_vec->begin() + index - ( stile ? 1 : 0 ) );
    }
    if( tiles_vec && tiles_vec->empty() )
        SAFEDEL( tiles_vec );
}

uint Field::GetTilesCount( bool is_roof )
{
    TileVec*&   tiles_vec = Tiles[ is_roof ? 1 : 0 ];
    AnyFrames*& stile = SimplyTile[ is_roof ? 1 : 0 ];
    return ( stile ? 1 : 0 ) + ( tiles_vec ? (uint) tiles_vec->size() : 0 );
}

Field::Tile& Field::GetTile( uint index, bool is_roof )
{
    TileVec*&   tiles_vec = Tiles[ is_roof ? 1 : 0 ];
    AnyFrames*& stile = SimplyTile[ is_roof ? 1 : 0 ];
    if( index == 0 && stile )
    {
        static Tile simply_tile;
        simply_tile.Anim = stile;
        return simply_tile;
    }
    return tiles_vec->at( index - ( stile ? 1 : 0 ) );
}

void Field::AddDeadCrit( CritterCl* cr )
{
    if( !DeadCrits )
        DeadCrits = new CritVec();
    if( std::find( DeadCrits->begin(), DeadCrits->end(), cr ) == DeadCrits->end() )
        DeadCrits->push_back( cr );
}

void Field::EraseDeadCrit( CritterCl* cr )
{
    if( !DeadCrits )
        return;
    auto it = std::find( DeadCrits->begin(), DeadCrits->end(), cr );
    if( it != DeadCrits->end() )
    {
        DeadCrits->erase( it );
        if( DeadCrits->empty() )
            SAFEDEL( DeadCrits );
    }
}

void Field::ProcessCache()
{
    Flags.IsWall = false;
    Flags.IsWallTransp = false;
    Flags.IsScen = false;
    Flags.IsNotPassed = ( Crit != nullptr || Flags.IsMultihex );
    Flags.IsNotRaked = false;
    Flags.IsNoLight = false;
    Flags.ScrollBlock = false;
    Corner = 0;

    if( Items )
    {
        for( ItemHex* item :* Items )
        {
            hash pid = item->GetProtoId();
            if( item->IsWall() )
            {
                Flags.IsWall = true;
                Flags.IsWallTransp = item->GetIsLightThru();
                Corner = item->GetCorner();
            }
            else if( item->IsScenery() )
            {
                Flags.IsScen = true;
            }
            if( !item->GetIsNoBlock() )
                Flags.IsNotPassed = true;
            if( !item->GetIsShootThru() )
                Flags.IsNotRaked = true;
            if( item->GetIsScrollBlock() )
                Flags.ScrollBlock = true;
            if( !item->GetIsLightThru() )
                Flags.IsNoLight = true;
        }
    }

    if( BlockLinesItems )
    {
        for( ItemHex* item :* BlockLinesItems )
        {
            Flags.IsNotPassed = true;
            if( !item->GetIsShootThru() )
                Flags.IsNotRaked = true;
            if( !item->GetIsLightThru() )
                Flags.IsNoLight = true;
        }
    }
}

void Field::AddSpriteToChain( Sprite* spr )
{
    if( !SpriteChain )
    {
        SpriteChain = spr;
        spr->ExtraChainRoot = &SpriteChain;
    }
    else
    {
        Sprite* last_spr = SpriteChain;
        while( last_spr->ExtraChainChild )
            last_spr = last_spr->ExtraChainChild;
        last_spr->ExtraChainChild = spr;
        spr->ExtraChainParent = last_spr;
    }
}

void Field::UnvalidateSpriteChain()
{
    if( SpriteChain )
    {
        while( SpriteChain )
            SpriteChain->Unvalidate();
    }
}

/************************************************************************/
/* HEX FIELD                                                            */
/************************************************************************/

HexManager::HexManager()
{
    curPidMap = 0;
    viewField = nullptr;
    isShowHex = false;
    roofSkip = 0;
    rainCapacity = 0;
    chosenId = 0;
    critterContourCrId = 0;
    crittersContour = 0;
    critterContour = 0;
    maxHexX = 0;
    maxHexY = 0;
    hexField = nullptr;
    hexTrack = nullptr;
    hexLight = nullptr;
    hTop = 0;
    hBottom = 0;
    wLeft = 0;
    wRight = 0;
    drawCursorX = 0;
    cursorPrePic = nullptr;
    cursorPostPic = nullptr;
    cursorXPic = nullptr;
    cursorX = 0;
    cursorY = 0;
    memzero( (void*) &AutoScroll, sizeof( AutoScroll ) );
    requestRebuildLight = false;
    requestRenderLight = false;
    lightPointsCount = 0;
    lightCapacity = 0;
    lightMinHx = 0;
    lightMaxHx = 0;
    lightMinHy = 0;
    lightMaxHy = 0;
    lightProcentR = 0;
    lightProcentG = 0;
    lightProcentB = 0;
    dayTime[ 0 ] = 300;
    dayTime[ 1 ] = 600;
    dayTime[ 2 ] = 1140;
    dayTime[ 3 ] = 1380;
    dayColor[ 0 ] = 18;
    dayColor[ 1 ] = 128;
    dayColor[ 2 ] = 103;
    dayColor[ 3 ] = 51;
    dayColor[ 4 ] = 18;
    dayColor[ 5 ] = 128;
    dayColor[ 6 ] = 95;
    dayColor[ 7 ] = 40;
    dayColor[ 8 ] = 53;
    dayColor[ 9 ] = 128;
    dayColor[ 10 ] = 86;
    dayColor[ 11 ] = 29;
    picRainFallName = "Rain/Fall.fofrm";
    picRainDropName = "Rain/Drop.fofrm";
    picRainFall = nullptr;
    picRainDrop = nullptr;
    picTrack1 = picTrack2 = picHexMask = nullptr;
    rtMap = rtLight = rtFog = nullptr;
    rtScreenOX = rtScreenOY = 0;
    fogOffsX = fogOffsY = nullptr;
    fogLastOffsX = fogLastOffsY = 0;
    fogForceRerender = false;
}

bool HexManager::Init()
{
    WriteLog( "Hex field initialization...\n" );

    rtMap = SprMngr.CreateRenderTarget( false, false, true, 0, 0, false, Effect::FlushMap );

    rtScreenOX = (uint) ceilf( (float) SCROLL_OX / MIN_ZOOM );
    rtScreenOY = (uint) ceilf( (float) SCROLL_OY / MIN_ZOOM );
    rtLight = SprMngr.CreateRenderTarget( false, false, true, rtScreenOX * 2, rtScreenOY * 2, false, Effect::FlushLight );
    #ifdef FONLINE_CLIENT
    rtFog = SprMngr.CreateRenderTarget( false, false, true, rtScreenOX * 2, rtScreenOY * 2, false, Effect::FlushFog );
    #endif

    isShowTrack = false;
    curPidMap = 0;
    curMapTime = -1;
    curHashTiles = 0;
    curHashScen = 0;
    cursorX = 0;
    cursorY = 0;
    chosenId = 0;
    memzero( (void*) &AutoScroll, sizeof( AutoScroll ) );
    maxHexX = 0;
    maxHexY = 0;

    #ifdef FONLINE_MAPPER
    ClearSelTiles();
    #endif

    WriteLog( "Hex field initialization complete.\n" );
    return true;
}

void HexManager::Finish()
{
    WriteLog( "Hex field finish...\n" );

    mainTree.Clear();
    roofRainTree.Clear();
    roofTree.Clear();
    tilesTree.Clear();

    for( auto it = rainData.begin(), end = rainData.end(); it != end; ++it )
        SAFEDEL( *it );
    rainData.clear();

    for( uint i = 0, j = (uint) hexItems.size(); i < j; i++ )
        hexItems[ i ]->Release();
    hexItems.clear();

    SAFEDELA( viewField );
    ResizeField( 0, 0 );

    chosenId = 0;
    WriteLog( "Hex field finish complete.\n" );
}

void HexManager::ReloadSprites()
{
    curDataPrefix = GameOpt.MapDataPrefix;

    // Must be valid
    picHex[ 0 ] = SprMngr.LoadAnimation( curDataPrefix + "hex1.png", true );
    picHex[ 1 ] = SprMngr.LoadAnimation( curDataPrefix + "hex2.png", true );
    picHex[ 2 ] = SprMngr.LoadAnimation( curDataPrefix + "hex3.png", true );
    cursorPrePic = SprMngr.LoadAnimation( curDataPrefix + "move_pre.png", true );
    cursorPostPic = SprMngr.LoadAnimation( curDataPrefix + "move_post.png", true );
    cursorXPic = SprMngr.LoadAnimation( curDataPrefix + "move_x.png", true );
    picTrack1 = SprMngr.LoadAnimation( curDataPrefix + "track1.png", true );
    picTrack2 = SprMngr.LoadAnimation( curDataPrefix + "track2.png", true );

    // May be null
    picHexMask = SprMngr.LoadAnimation( curDataPrefix + "hex_mask.png" );

    // Rain
    SetRainAnimation( nullptr, nullptr );
}

void HexManager::AddFieldItem( ushort hx, ushort hy, ItemHex* item )
{
    GetField( hx, hy ).AddItem( item, nullptr );
    if( item->IsNonEmptyBlockLines() )
    {
        FOREACH_PROTO_ITEM_LINES( item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
                                  GetField( hx, hy ).AddItem( nullptr, item );
                                  );
    }
}

void HexManager::EraseFieldItem( ushort hx, ushort hy, ItemHex* item )
{
    GetField( hx, hy ).EraseItem( item, nullptr );
    if( item->IsNonEmptyBlockLines() )
    {
        FOREACH_PROTO_ITEM_LINES( item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
                                  GetField( hx, hy ).EraseItem( nullptr, item );
                                  );
    }
}

uint HexManager::AddItem( uint id, hash pid, ushort hx, ushort hy, bool is_added, UCharVecVec* data )
{
    if( !id )
    {
        WriteLog( "Item id is zero.\n" );
        return 0;
    }

    if( !IsMapLoaded() )
    {
        WriteLog( "Map is not loaded." );
        return 0;
    }

    if( hx >= maxHexX || hy >= maxHexY )
    {
        WriteLog( "Position hx {} or hy {} error value.\n", hx, hy );
        return 0;
    }

    ProtoItem* proto = ProtoMngr.GetProtoItem( pid );
    if( !proto )
    {
        WriteLog( "Proto not found '{}'.\n", _str().parseHash( pid ) );
        return 0;
    }

    // Change
    ItemHex* item_old = GetItemById( id );
    if( item_old )
    {
        if( item_old->GetProtoId() == pid && item_old->GetHexX() == hx && item_old->GetHexY() == hy )
        {
            item_old->IsDestroyed = false;
            if( item_old->IsFinishing() )
                item_old->StopFinishing();
            if( data )
                item_old->Props.RestoreData( *data );
            return item_old->GetId();
        }
        else
        {
            DeleteItem( item_old );
        }
    }

    // Parse
    Field&   f = GetField( hx, hy );
    ItemHex* item = new ItemHex( id, proto, data, hx, hy, &f.ScrX, &f.ScrY );
    if( is_added )
        item->SetShowAnim();
    PushItem( item );

    // Check ViewField size
    if( !ProcessHexBorders( item->Anim->GetSprId( 0 ), item->GetOffsetX(), item->GetOffsetY(), true ) )
    {
        // Draw
        if( IsHexToDraw( hx, hy ) && !item->GetIsHidden() && !item->GetIsHiddenPicture() && !item->IsFullyTransparent() )
        {
            Sprite& spr = mainTree.InsertSprite( DRAW_ORDER_ITEM_AUTO( item ), hx, hy + item->GetDrawOrderOffsetHexY(), item->GetSpriteCut(),
                                                 HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha,
                                                 &item->DrawEffect, &item->SprDrawValid );
            if( !item->GetIsNoLightInfluence() )
                spr.SetLight( item->GetCorner(), hexLight, maxHexX, maxHexY );
            item->SetSprite( &spr );

            f.AddSpriteToChain( &spr );
        }

        if( item->GetIsLight() || !item->GetIsLightThru() )
            RebuildLight();
    }

    return item->GetId();
}

void HexManager::FinishItem( uint id, bool is_deleted )
{
    if( !id )
    {
        WriteLog( "Item zero id.\n" );
        return;
    }

    ItemHex* item = GetItemById( id );
    if( !item )
    {
        WriteLog( "Item '{}' not found.\n", _str().parseHash( id ) );
        return;
    }

    item->Finish();
    if( is_deleted )
        item->SetHideAnim();

    #ifdef FONLINE_MAPPER
    DeleteItem( item );
    #endif
}

void HexManager::DeleteItem( ItemHex* item, bool destroy_item /* = true */, ItemHexVec::iterator* it_hex_items /* = nullptr */ )
{
    ushort hx = item->GetHexX();
    ushort hy = item->GetHexY();

    if( item->SprDrawValid )
        item->SprDraw->Unvalidate();

    auto it = std::find( hexItems.begin(), hexItems.end(), item );
    RUNTIME_ASSERT( it != hexItems.end() );
    it = hexItems.erase( it );
    if( it_hex_items )
        *it_hex_items = it;

    EraseFieldItem( hx, hy, item );

    if( item->GetIsLight() || !item->GetIsLightThru() )
        RebuildLight();

    if( destroy_item )
    {
        item->IsDestroyed = true;
        Script::RemoveEventsEntity( item );
        item->Release();
    }
}

void HexManager::ProcessItems()
{
    for( auto it = hexItems.begin(); it != hexItems.end();)
    {
        ItemHex* item = *it;
        item->Process();

        if( item->IsDynamicEffect() && !item->IsFinishing() )
        {
            UShortPair step = item->GetEffectStep();
            if( item->GetHexX() != step.first || item->GetHexY() != step.second )
            {
                ushort hx = item->GetHexX();
                ushort hy = item->GetHexY();

                int    x, y;
                GetHexInterval( hx, hy, step.first, step.second, x, y );
                item->EffOffsX -= x;
                item->EffOffsY -= y;

                EraseFieldItem( hx, hy, item );

                Field& f = GetField( step.first, step.second );
                item->SetHexX( step.first );
                item->SetHexY( step.second );

                AddFieldItem( step.first, step.second, item );

                if( item->SprDrawValid )
                    item->SprDraw->Unvalidate();
                if( IsHexToDraw( step.first, step.second ) )
                {
                    item->SprDraw = &mainTree.InsertSprite( DRAW_ORDER_ITEM_AUTO( item ), step.first, step.second + item->GetDrawOrderOffsetHexY(), item->GetSpriteCut(),
                                                            HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha,
                                                            &item->DrawEffect, &item->SprDrawValid );
                    if( !item->GetIsNoLightInfluence() )
                        item->SprDraw->SetLight( item->GetCorner(), hexLight, maxHexX, maxHexY );
                    f.AddSpriteToChain( item->SprDraw );
                }
                item->SetAnimOffs();
            }
        }

        if( item->IsFinish() )
            DeleteItem( item, true, &it );
        else
            ++it;
    }
}

void HexManager::SkipItemsFade()
{
    for( auto& item : hexItems )
        item->SkipFade();
}

bool ItemCompScen( ItemHex* o1, ItemHex* o2 ) { return o1->IsScenery() && !o2->IsScenery(); }
bool ItemCompWall( ItemHex* o1, ItemHex* o2 ) { return o1->IsWall() && !o2->IsWall(); }
void HexManager::PushItem( ItemHex* item )
{
    hexItems.push_back( item );

    ushort hx = item->GetHexX();
    ushort hy = item->GetHexY();
    AddFieldItem( hx, hy, item );

    // Sort
    Field& f = GetField( hx, hy );
    std::sort( f.Items->begin(), f.Items->end(), ItemCompScen );
    std::sort( f.Items->begin(), f.Items->end(), ItemCompWall );
}

ItemHex* HexManager::GetItem( ushort hx, ushort hy, hash pid )
{
    if( !IsMapLoaded() || hx >= maxHexX || hy >= maxHexY || !GetField( hx, hy ).Items )
        return nullptr;

    for( auto it = GetField( hx, hy ).Items->begin(), end = GetField( hx, hy ).Items->end(); it != end; ++it )
    {
        ItemHex* item = *it;
        if( item->GetProtoId() == pid )
            return item;
    }
    return nullptr;
}

ItemHex* HexManager::GetItemById( ushort hx, ushort hy, uint id )
{
    if( !IsMapLoaded() || hx >= maxHexX || hy >= maxHexY || !GetField( hx, hy ).Items )
        return nullptr;

    for( auto it = GetField( hx, hy ).Items->begin(), end = GetField( hx, hy ).Items->end(); it != end; ++it )
    {
        ItemHex* item = *it;
        if( item->GetId() == id )
            return item;
    }
    return nullptr;
}

ItemHex* HexManager::GetItemById( uint id )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        ItemHex* item = *it;
        if( item->GetId() == id )
            return item;
    }
    return nullptr;
}

void HexManager::GetItems( ushort hx, ushort hy, ItemHexVec& items )
{
    if( !IsMapLoaded() )
        return;

    Field& f = GetField( hx, hy );
    if( !f.Items )
        return;

    for( uint i = 0, j = (uint) f.Items->size(); i < j; i++ )
    {
        if( std::find( items.begin(), items.end(), f.Items->at( i ) ) == items.end() )
            items.push_back( f.Items->at( i ) );
    }
}

Rect HexManager::GetRectForText( ushort hx, ushort hy )
{
    if( !IsMapLoaded() )
        return Rect();
    Field& f = GetField( hx, hy );

    // Critters first
    if( f.Crit )
        return f.Crit->GetTextRect();
    else if( f.DeadCrits )
        return f.DeadCrits->front()->GetTextRect();

    // Items
    Rect r( 0, 0, 0, 0 );
    if( f.Items )
    {
        for( uint i = 0, j = (uint) f.Items->size(); i < j; i++ )
        {
            SpriteInfo* si = SprMngr.GetSpriteInfo( f.Items->at( i )->SprId );
            if( si )
            {
                int w = si->Width - si->OffsX;
                int h = si->Height - si->OffsY;
                if( w > r.L )
                    r.L = w;
                if( h > r.B )
                    r.B = h;
            }
        }
    }
    return r;
}

bool HexManager::RunEffect( hash eff_pid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( !IsMapLoaded() )
        return false;

    if( from_hx >= maxHexX || from_hy >= maxHexY || to_hx >= maxHexX || to_hy >= maxHexY )
    {
        WriteLog( "Incorrect value of from_x {} or from_y {} or to_x {} or to_y {}.\n", from_hx, from_hy, to_hx, to_hy );
        return false;
    }

    ProtoItem* proto = ProtoMngr.GetProtoItem( eff_pid );
    if( !proto )
    {
        WriteLog( "Proto '{}' not found.\n", _str().parseHash( eff_pid ) );
        return false;
    }

    Field&   f = GetField( from_hx, from_hy );
    ItemHex* item = new ItemHex( 0, proto, nullptr, from_hx, from_hy, &f.ScrX, &f.ScrY );

    float    sx = 0;
    float    sy = 0;
    uint     dist = 0;

    if( from_hx != to_hx || from_hy != to_hy )
    {
        item->EffSteps.push_back( std::make_pair( from_hx, from_hy ) );
        TraceBullet( from_hx, from_hy, to_hx, to_hy, 0, 0.0f, nullptr, false, nullptr, 0, nullptr, nullptr, &item->EffSteps, false );
        int x, y;
        GetHexInterval( from_hx, from_hy, to_hx, to_hy, x, y );
        y += Random( 5, 25 );    // Center of body
        GetStepsXY( sx, sy, 0, 0, x, y );
        dist = DistSqrt( 0, 0, x, y );
    }

    item->SetEffect( sx, sy, dist, GetFarDir( from_hx, from_hy, to_hx, to_hy ) );

    AddFieldItem( from_hx, from_hy, item );
    hexItems.push_back( item );

    if( IsHexToDraw( from_hx, from_hy ) )
    {
        item->SprDraw = &mainTree.InsertSprite( DRAW_ORDER_ITEM_AUTO( item ), from_hx, from_hy + item->GetDrawOrderOffsetHexY(), item->GetSpriteCut(),
                                                HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha,
                                                &item->DrawEffect, &item->SprDrawValid );
        if( !item->GetIsNoLightInfluence() )
            item->SprDraw->SetLight( item->GetCorner(), hexLight, maxHexX, maxHexY );
        f.AddSpriteToChain( item->SprDraw );
    }

    return true;
}

void HexManager::ProcessRain()
{
    if( !rainCapacity )
        return;

    static uint last_tick = Timer::GameTick();
    uint        delta = Timer::GameTick() - last_tick;
    if( delta < GameOpt.RainTick )
        return;

    for( auto it = rainData.begin(), end = rainData.end(); it != end; ++it )
    {
        Drop* cur_drop = *it;

        // Fall
        if( cur_drop->DropCnt == -1 )
        {
            cur_drop->OffsX += GameOpt.RainSpeedX;
            cur_drop->OffsY += GameOpt.RainSpeedY;
            if( cur_drop->OffsY >= cur_drop->GroundOffsY )
            {
                cur_drop->DropCnt = 0;
                cur_drop->CurSprId = picRainDrop->GetSprId( 0 );
            }
        }
        // Drop
        else
        {
            cur_drop->DropCnt++;
            if( (uint) cur_drop->DropCnt >= picRainDrop->GetCnt() )
            {
                cur_drop->CurSprId = picRainFall->GetCurSprId();
                cur_drop->DropCnt = -1;
                cur_drop->OffsX = Random( -10, 10 );
                cur_drop->OffsY = -100 - Random( 0, 100 );
            }
            else
            {
                cur_drop->CurSprId = picRainDrop->GetSprId( cur_drop->DropCnt );
            }
        }
    }

    last_tick = Timer::GameTick();
}

void HexManager::SetRainAnimation( const char* fall_anim_name, const char* drop_anim_name )
{
    if( fall_anim_name )
        picRainFallName = fall_anim_name;
    if( drop_anim_name )
        picRainDropName = drop_anim_name;

    if( picRainFall == SpriteManager::DummyAnimation )
        picRainFall = nullptr;
    else
        AnyFrames::Destroy( picRainFall );
    if( picRainDrop == SpriteManager::DummyAnimation )
        picRainDrop = nullptr;
    else
        AnyFrames::Destroy( picRainDrop );

    picRainFall = SprMngr.LoadAnimation( picRainFallName, true );
    picRainDrop = SprMngr.LoadAnimation( picRainDropName, true );
}

void HexManager::SetCursorPos( int x, int y, bool show_steps, bool refresh )
{
    ushort hx, hy;
    if( GetHexPixel( x, y, hx, hy ) )
    {
        Field& f = GetField( hx, hy );
        cursorX = f.ScrX + 1 - 1;
        cursorY = f.ScrY - 1 - 1;

        CritterCl* chosen = GetChosen();
        if( !chosen )
        {
            drawCursorX = -1;
            return;
        }

        int  cx = chosen->GetHexX();
        int  cy = chosen->GetHexY();
        uint mh = chosen->GetMultihex();

        if( ( cx == hx && cy == hy ) || ( f.Flags.IsNotPassed && ( !mh || !CheckDist( cx, cy, hx, hy, mh ) ) ) )
        {
            drawCursorX = -1;
        }
        else
        {
            static int    last_cur_x = 0;
            static ushort last_hx = 0, last_hy = 0;
            if( refresh || hx != last_hx || hy != last_hy )
            {
                if( chosen->IsLife() )
                {
                    UCharVec steps;
                    if( !FindPath( chosen, cx, cy, hx, hy, steps, -1 ) )
                        drawCursorX = -1;
                    else
                        drawCursorX = (int) ( show_steps ? steps.size() : 0 );
                }
                else
                {
                    drawCursorX = -1;
                }

                last_hx = hx;
                last_hy = hy;
                last_cur_x = drawCursorX;
            }
            else
            {
                drawCursorX = last_cur_x;
            }
        }
    }
}

void HexManager::DrawCursor( uint spr_id )
{
    if( GameOpt.HideCursor || !GameOpt.ShowMoveCursor )
        return;

    SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
    if( si )
    {
        SprMngr.DrawSpriteSize( spr_id,
                                (int) ( (float) ( cursorX + GameOpt.ScrOx ) / GameOpt.SpritesZoom ),
                                (int) ( (float) ( cursorY + GameOpt.ScrOy ) / GameOpt.SpritesZoom ),
                                (int) ( (float) si->Width / GameOpt.SpritesZoom ),
                                (int) ( (float) si->Height / GameOpt.SpritesZoom ), true, false );
    }
}

void HexManager::DrawCursor( const char* text )
{
    if( GameOpt.HideCursor || !GameOpt.ShowMoveCursor )
        return;

    int x = (int) ( (float) ( cursorX + GameOpt.ScrOx ) / GameOpt.SpritesZoom );
    int y = (int) ( (float) ( cursorY + GameOpt.ScrOy ) / GameOpt.SpritesZoom );
    SprMngr.DrawStr( Rect( x, y, (int) ( (float) ( x + HEX_W ) / GameOpt.SpritesZoom ),
                           (int) ( (float) ( y + HEX_REAL_H ) / GameOpt.SpritesZoom ) ), text, FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE );
}

void HexManager::RebuildMap( int rx, int ry )
{
    RUNTIME_ASSERT( viewField );

    for( int i = 0, j = hVisible * wVisible; i < j; i++ )
    {
        int hx = viewField[ i ].HexX;
        int hy = viewField[ i ].HexY;
        if( hx < 0 || hy < 0 || hx >= maxHexX || hy >= maxHexY )
            continue;

        Field& f = GetField( hx, hy );
        f.IsView = false;
        f.UnvalidateSpriteChain();
    }

    InitView( rx, ry );

    // Light
    RealRebuildLight();
    requestRebuildLight = false;
    requestRenderLight = true;

    // Tiles, roof
    RebuildTiles();
    RebuildRoof();

    // Erase old sprites
    mainTree.Unvalidate();
    roofRainTree.Unvalidate();

    for( auto it = rainData.begin(), end = rainData.end(); it != end; ++it )
        delete ( *it );
    rainData.clear();

    SprMngr.EggNotValid();

    // Begin generate new sprites
    for( int i = 0, j = hVisible * wVisible; i < j; i++ )
    {
        ViewField& vf = viewField[ i ];
        int        nx = vf.HexX;
        int        ny = vf.HexY;
        if( ny < 0 || nx < 0 || nx >= maxHexX || ny >= maxHexY )
            continue;

        Field& f = GetField( nx, ny );
        f.IsView = true;
        f.ScrX = vf.ScrX;
        f.ScrY = vf.ScrY;

        // Track
        if( isShowTrack && GetHexTrack( nx, ny ) )
        {
            uint        spr_id = ( GetHexTrack( nx, ny ) == 1 ? picTrack1->GetCurSprId() : picTrack2->GetCurSprId() );
            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
            Sprite&     spr = mainTree.AddSprite( DRAW_ORDER_TRACK, nx, ny, 0, HEX_OX, HEX_OY + ( si ? si->Height / 2 : 0 ), &f.ScrX, &f.ScrY, spr_id,
                                                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            f.AddSpriteToChain( &spr );
        }

        // Hex Lines
        if( isShowHex )
        {
            uint        spr_id = picHex[ 0 ]->GetCurSprId();
            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
            Sprite&     spr = mainTree.AddSprite( DRAW_ORDER_HEX_GRID, nx, ny, 0, si ? si->Width / 2 : 0, si ? si->Height : 0, &f.ScrX, &f.ScrY, spr_id,
                                                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            f.AddSpriteToChain( &spr );
        }

        // Rain
        if( rainCapacity && rainCapacity >= Random( 0, 255 ) )
        {
            int rofx = nx;
            int rofy = ny;
            if( rofx & 1 )
                rofx--;
            if( rofy & 1 )
                rofy--;

            Drop* new_drop = nullptr;
            if( !GetField( rofx, rofy ).GetTilesCount( true ) )
            {
                new_drop = new Drop( picRainFall->GetCurSprId(), Random( -10, 10 ), -Random( 0, 200 ), 0 );
                rainData.push_back( new_drop );

                Sprite& spr = mainTree.AddSprite( DRAW_ORDER_RAIN, nx, ny, 0, HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &new_drop->CurSprId,
                                                  &new_drop->OffsX, &new_drop->OffsY, nullptr, &Effect::Rain, nullptr );
                spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
                f.AddSpriteToChain( &spr );
            }
            else if( !roofSkip || roofSkip != GetField( rofx, rofy ).RoofNum )
            {
                new_drop = new Drop( picRainFall->GetCurSprId(), Random( -10, 10 ), -100 - Random( 0, 100 ), -100 );
                rainData.push_back( new_drop );

                Sprite& spr = roofRainTree.AddSprite( DRAW_ORDER_RAIN, nx, ny, 0, HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &new_drop->CurSprId,
                                                      &new_drop->OffsX, &new_drop->OffsY, nullptr, &Effect::Rain, nullptr );
                spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
                f.AddSpriteToChain( &spr );
            }
            if( new_drop )
            {
                new_drop->OffsX = Random( -10, 10 );
                new_drop->OffsY = -100 - Random( 0, 100 );
                if( new_drop->OffsY < 0 )
                    new_drop->OffsY = Random( new_drop->OffsY, 0 );
            }
        }

        // Items on hex
        if( f.Items )
        {
            for( auto it = f.Items->begin(), end = f.Items->end(); it != end; ++it )
            {
                ItemHex* item = *it;

                #ifdef FONLINE_CLIENT
                if( item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent() )
                    continue;
                if( !GameOpt.ShowScen && item->IsScenery() )
                    continue;
                if( !GameOpt.ShowItem && !item->IsAnyScenery() )
                    continue;
                if( !GameOpt.ShowWall && item->IsWall() )
                    continue;
                #else
                bool is_fast = fastPids.count( item->GetProtoId() ) != 0;
                if( !GameOpt.ShowScen && !is_fast && item->IsScenery() )
                    continue;
                if( !GameOpt.ShowItem && !is_fast && !item->IsAnyScenery() )
                    continue;
                if( !GameOpt.ShowWall && !is_fast && item->IsWall() )
                    continue;
                if( !GameOpt.ShowFast && is_fast )
                    continue;
                if( ignorePids.count( item->GetProtoId() ) )
                    continue;
                #endif

                Sprite& spr = mainTree.AddSprite( DRAW_ORDER_ITEM_AUTO( item ), nx, ny + item->GetDrawOrderOffsetHexY(), item->GetSpriteCut(),
                                                  HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha,
                                                  &item->DrawEffect, &item->SprDrawValid );
                if( !item->GetIsNoLightInfluence() )
                    spr.SetLight( item->GetCorner(), hexLight, maxHexX, maxHexY );
                item->SetSprite( &spr );
                f.AddSpriteToChain( &spr );
            }
        }

        // Critters
        CritterCl* cr = f.Crit;
        if( cr && GameOpt.ShowCrit && cr->Visible )
        {
            Sprite& spr = mainTree.AddSprite( DRAW_ORDER_CRIT_AUTO( cr ), nx, ny, 0,
                                              HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy,
                                              &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid );
            spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
            cr->SprDraw = &spr;
            cr->SetSprRect();

            int contour = 0;
            if( cr->GetId() == critterContourCrId )
                contour = critterContour;
            else if( !cr->IsChosen() )
                contour = crittersContour;
            spr.SetContour( contour, cr->ContourColor );

            f.AddSpriteToChain( &spr );
        }

        // Dead critters
        if( f.DeadCrits && GameOpt.ShowCrit )
        {
            for( auto it = f.DeadCrits->begin(), end = f.DeadCrits->end(); it != end; ++it )
            {
                CritterCl* cr = *it;
                if( !cr->Visible )
                    continue;

                Sprite& spr = mainTree.AddSprite( DRAW_ORDER_CRIT_AUTO( cr ), nx, ny, 0,
                                                  HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy,
                                                  &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid );
                spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
                cr->SprDraw = &spr;
                cr->SetSprRect();

                f.AddSpriteToChain( &spr );
            }
        }
    }
    mainTree.SortByMapPos();

    screenHexX = rx;
    screenHexY = ry;

    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( ClientFunctions.RenderMap );
    #else // FONLINE_MAPPER
    Script::RaiseInternalEvent( MapperFunctions.RenderMap );
    #endif
}

void HexManager::RebuildMapOffset( int ox, int oy )
{
    RUNTIME_ASSERT( viewField );
    RUNTIME_ASSERT( ox == 0 || ox == -1 || ox == 1 );
    RUNTIME_ASSERT( oy == 0 || oy == -2 || oy == 2 );

    auto hide_hex = [ this ] ( ViewField & vf )
    {
        int nx = vf.HexX;
        int ny = vf.HexY;
        if( nx < 0 || ny < 0 || nx >= maxHexX || ny >= maxHexY || !IsHexToDraw( nx, ny ) )
            return;

        Field& f = GetField( nx, ny );
        f.IsView = false;
        f.UnvalidateSpriteChain();
    };

    if( ox != 0 )
    {
        int from_x = ( ox > 0 ? 0 : wVisible + ox );
        int to_x = ( ox > 0 ? ox : wVisible );
        for( int x = from_x; x < to_x; x++ )
            for( int y = 0; y < hVisible; y++ )
                hide_hex( viewField[ y * wVisible + x ] );
    }

    if( oy != 0 )
    {
        int from_y = ( oy > 0 ? 0 : hVisible + oy );
        int to_y = ( oy > 0 ? oy : hVisible );
        for( int y = from_y; y < to_y; y++ )
            for( int x = 0; x < wVisible; x++ )
                hide_hex( viewField[ y * wVisible + x ] );
    }

    int vpos1 = 5 * wVisible + 4;
    int vpos2 = ( 5 + oy ) * wVisible + 4 + ox;
    screenHexX += viewField[ vpos2 ].HexX - viewField[ vpos1 ].HexX;
    screenHexY += viewField[ vpos2 ].HexY - viewField[ vpos1 ].HexY;

    for( int i = 0, j = wVisible * hVisible; i < j; i++ )
    {
        ViewField& vf = viewField[ i ];

        if( ox < 0 )
        {
            vf.HexX--;
            if( vf.HexX & 1 )
                vf.HexY++;
        }
        else if( ox > 0 )
        {
            vf.HexX++;
            if( !( vf.HexX & 1 ) )
                vf.HexY--;
        }

        if( oy < 0 )
        {
            vf.HexX--;
            vf.HexY--;
            if( !( vf.HexX & 1 ) )
                vf.HexY--;
        }
        else if( oy > 0 )
        {
            vf.HexX++;
            vf.HexY++;
            if( vf.HexX & 1 )
                vf.HexY++;
        }

        if( vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < maxHexX && vf.HexY < maxHexY )
        {
            Field& f = GetField( vf.HexX, vf.HexY );
            f.ScrX = vf.ScrX;
            f.ScrY = vf.ScrY;
        }
    }

    auto show_hex = [ this ] ( ViewField & vf )
    {
        int nx = vf.HexX;
        int ny = vf.HexY;
        if( nx < 0 || ny < 0 || nx >= maxHexX || ny >= maxHexY || IsHexToDraw( nx, ny ) )
            return;

        Field& f = GetField( nx, ny );
        f.IsView = true;

        // Track
        if( isShowTrack && GetHexTrack( nx, ny ) )
        {
            uint        spr_id = ( GetHexTrack( nx, ny ) == 1 ? picTrack1->GetCurSprId() : picTrack2->GetCurSprId() );
            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
            Sprite&     spr = mainTree.InsertSprite( DRAW_ORDER_TRACK, nx, ny, 0, HEX_OX, HEX_OY + ( si ? si->Height / 2 : 0 ), &f.ScrX, &f.ScrY, spr_id,
                                                     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            f.AddSpriteToChain( &spr );
        }

        // Hex lines
        if( isShowHex )
        {
            uint        spr_id = picHex[ 0 ]->GetCurSprId();
            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
            Sprite&     spr = mainTree.InsertSprite( DRAW_ORDER_HEX_GRID, nx, ny, 0, si ? si->Width / 2 : 0, si ? si->Height : 0, &f.ScrX, &f.ScrY, spr_id,
                                                     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            f.AddSpriteToChain( &spr );
        }

        // Rain
        if( rainCapacity && rainCapacity >= Random( 0, 255 ) )
        {
            int rofx = nx;
            int rofy = ny;
            if( rofx & 1 )
                rofx--;
            if( rofy & 1 )
                rofy--;

            Drop* new_drop = nullptr;
            if( !GetField( rofx, rofy ).GetTilesCount( true ) )
            {
                new_drop = new Drop( picRainFall->GetCurSprId(), Random( -10, 10 ), -Random( 0, 200 ), 0 );
                rainData.push_back( new_drop );

                Sprite& spr = mainTree.InsertSprite( DRAW_ORDER_RAIN, nx, ny, 0, HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &new_drop->CurSprId,
                                                     &new_drop->OffsX, &new_drop->OffsY, nullptr, &Effect::Rain, nullptr );
                spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
                f.AddSpriteToChain( &spr );
            }
            else if( !roofSkip || roofSkip != GetField( rofx, rofy ).RoofNum )
            {
                new_drop = new Drop( picRainFall->GetCurSprId(), Random( -10, 10 ), -100 - Random( 0, 100 ), -100 );
                rainData.push_back( new_drop );

                Sprite& spr = roofRainTree.InsertSprite( DRAW_ORDER_RAIN, nx, ny, 0, HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &new_drop->CurSprId,
                                                         &new_drop->OffsX, &new_drop->OffsY, nullptr, &Effect::Rain, nullptr );
                spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
                f.AddSpriteToChain( &spr );
            }
            if( new_drop )
            {
                new_drop->OffsX = Random( -10, 10 );
                new_drop->OffsY = -100 - Random( 0, 100 );
                if( new_drop->OffsY < 0 )
                    new_drop->OffsY = Random( new_drop->OffsY, 0 );
            }
        }

        // Items on hex
        if( f.Items )
        {
            for( auto it = f.Items->begin(), end = f.Items->end(); it != end; ++it )
            {
                ItemHex* item = *it;

                #ifdef FONLINE_CLIENT
                if( item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent() )
                    continue;
                if( !GameOpt.ShowScen && item->IsScenery() )
                    continue;
                if( !GameOpt.ShowItem && !item->IsAnyScenery() )
                    continue;
                if( !GameOpt.ShowWall && item->IsWall() )
                    continue;
                #else
                bool is_fast = fastPids.count( item->GetProtoId() ) != 0;
                if( !GameOpt.ShowScen && !is_fast && item->IsScenery() )
                    continue;
                if( !GameOpt.ShowItem && !is_fast && !item->IsAnyScenery() )
                    continue;
                if( !GameOpt.ShowWall && !is_fast && item->IsWall() )
                    continue;
                if( !GameOpt.ShowFast && is_fast )
                    continue;
                if( ignorePids.count( item->GetProtoId() ) )
                    continue;
                #endif

                Sprite& spr = mainTree.InsertSprite( DRAW_ORDER_ITEM_AUTO( item ), nx, ny + item->GetDrawOrderOffsetHexY(), item->GetSpriteCut(),
                                                     HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha,
                                                     &item->DrawEffect, &item->SprDrawValid );
                if( !item->GetIsNoLightInfluence() )
                    spr.SetLight( item->GetCorner(), hexLight, maxHexX, maxHexY );
                item->SetSprite( &spr );
                f.AddSpriteToChain( &spr );
            }
        }

        // Critters
        CritterCl* cr = f.Crit;
        if( cr && GameOpt.ShowCrit && cr->Visible )
        {
            Sprite& spr = mainTree.InsertSprite( DRAW_ORDER_CRIT_AUTO( cr ), nx, ny, 0,
                                                 HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy,
                                                 &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid );
            spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
            cr->SprDraw = &spr;
            cr->SetSprRect();

            int contour = 0;
            if( cr->GetId() == critterContourCrId )
                contour = critterContour;
            else if( !cr->IsChosen() )
                contour = crittersContour;
            spr.SetContour( contour, cr->ContourColor );

            f.AddSpriteToChain( &spr );
        }

        // Dead critters
        if( f.DeadCrits && GameOpt.ShowCrit )
        {
            for( auto it = f.DeadCrits->begin(), end = f.DeadCrits->end(); it != end; ++it )
            {
                CritterCl* cr = *it;
                if( !cr->Visible )
                    continue;

                Sprite& spr = mainTree.InsertSprite( DRAW_ORDER_CRIT_AUTO( cr ), nx, ny, 0,
                                                     HEX_OX, HEX_OY, &f.ScrX, &f.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy,
                                                     &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid );
                spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
                cr->SprDraw = &spr;
                cr->SetSprRect();

                f.AddSpriteToChain( &spr );
            }
        }

        // Tiles
        uint tiles_count = f.GetTilesCount( false );
        if( GameOpt.ShowTile && tiles_count )
        {
            for( uint i = 0; i < tiles_count; i++ )
            {
                Field::Tile& tile = f.GetTile( i, false );
                uint         spr_id = tile.Anim->GetSprId( 0 );

                #ifdef FONLINE_MAPPER
                ProtoMap::TileVec& tiles = GetTiles( nx, ny, false );
                Sprite&            spr = tilesTree.InsertSprite( DRAW_ORDER_TILE + tile.Layer, nx, ny, 0, tile.OffsX + TILE_OX, tile.OffsY + TILE_OY,
                                                                 &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                                 tiles[ i ].IsSelected ? (uchar*) &SELECT_ALPHA : nullptr, &Effect::Tile, nullptr );
                #else
                Sprite& spr = tilesTree.InsertSprite( DRAW_ORDER_TILE + tile.Layer, nx, ny, 0, tile.OffsX + TILE_OX, tile.OffsY + TILE_OY,
                                                      &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                      nullptr, &Effect::Tile, nullptr );
                #endif
                f.AddSpriteToChain( &spr );
            }
        }

        // Roof
        uint roofs_count = f.GetTilesCount( true );
        if( GameOpt.ShowRoof && roofs_count && ( !roofSkip || roofSkip != f.RoofNum ) )
        {
            for( uint i = 0; i < roofs_count; i++ )
            {
                Field::Tile& roof = f.GetTile( i, true );
                uint         spr_id = roof.Anim->GetSprId( 0 );

                #ifdef FONLINE_MAPPER
                ProtoMap::TileVec& roofs = GetTiles( nx, ny, true );
                Sprite&            spr = roofTree.InsertSprite( DRAW_ORDER_TILE + roof.Layer, nx, ny, 0, roof.OffsX + ROOF_OX, roof.OffsY + ROOF_OY,
                                                                &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                                roofs[ i ].IsSelected ? (uchar*) &SELECT_ALPHA : &GameOpt.RoofAlpha, &Effect::Roof, nullptr );
                #else
                Sprite& spr = roofTree.InsertSprite( DRAW_ORDER_TILE + roof.Layer, nx, ny, 0, roof.OffsX + ROOF_OX, roof.OffsY + ROOF_OY,
                                                     &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                     &GameOpt.RoofAlpha, &Effect::Roof, nullptr );
                #endif
                spr.SetEgg( EGG_ALWAYS );
                f.AddSpriteToChain( &spr );
            }
        }
    };

    if( ox != 0 )
    {
        int from_x = ( ox > 0 ? wVisible - ox : 0 );
        int to_x = ( ox > 0 ? wVisible : -ox );
        for( int x = from_x; x < to_x; x++ )
            for( int y = 0; y < hVisible; y++ )
                show_hex( viewField[ y * wVisible + x ] );
    }

    if( oy != 0 )
    {
        int from_y = ( oy > 0 ? hVisible - oy : 0 );
        int to_y = ( oy > 0 ? hVisible : -oy );
        for( int y = from_y; y < to_y; y++ )
            for( int x = 0; x < wVisible; x++ )
                show_hex( viewField[ y * wVisible + x ] );
    }

    // Critters text rect
    for( auto& kv : allCritters )
        kv.second->SetSprRect();

    // Light
    RealRebuildLight();
    requestRebuildLight = false;
    requestRenderLight = true;

    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( ClientFunctions.RenderMap );
    #else     // FONLINE_MAPPER
    Script::RaiseInternalEvent( MapperFunctions.RenderMap );
    #endif
}

/************************************************************************/
/* Light                                                                */
/************************************************************************/

void HexManager::PrepareLightToDraw()
{
    if( !rtLight )
        return;

    // Rebuild light
    if( requestRebuildLight )
    {
        requestRebuildLight = false;
        RealRebuildLight();
    }

    // Check dynamic light sources
    if( !requestRenderLight )
    {
        for( size_t i = 0; i < lightSources.size(); i++ )
        {
            LightSource& ls = lightSources[ i ];
            if( ls.OffsX && ( *ls.OffsX != ls.LastOffsX || *ls.OffsY != ls.LastOffsY ) )
            {
                ls.LastOffsX = *ls.OffsX;
                ls.LastOffsY = *ls.OffsY;
                requestRenderLight = true;
            }
        }
    }

    // Prerender light
    if( requestRenderLight )
    {
        requestRenderLight = false;
        SprMngr.PushRenderTarget( rtLight );
        SprMngr.ClearCurrentRenderTarget( 0 );
        PointF offset( (float) rtScreenOX, (float) rtScreenOY );
        for( uint i = 0; i < lightPointsCount; i++ )
            SprMngr.DrawPoints( lightPoints[ i ], PRIMITIVE_TRIANGLEFAN, &GameOpt.SpritesZoom, &offset, Effect::Light );
        SprMngr.DrawPoints( lightSoftPoints, PRIMITIVE_TRIANGLELIST, &GameOpt.SpritesZoom, &offset, Effect::Light );
        SprMngr.PopRenderTarget();
    }
}

#define MAX_LIGHT_VALUE      ( 10000 )
#define MAX_LIGHT_HEX        ( 200 )
#define MAX_LIGHT_ALPHA      ( 255 )
#define LIGHT_SOFT_LENGTH    ( HEX_W )

void HexManager::MarkLight( ushort hx, ushort hy, uint inten )
{
    int    light = inten * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * lightCapacity / 100;
    int    lr = light * lightProcentR / 100;
    int    lg = light * lightProcentG / 100;
    int    lb = light * lightProcentB / 100;
    uchar* p = &hexLight[ hy * maxHexX * 3 + hx * 3 ];
    if( lr > *p )
        *p = lr;
    if( lg > *( p + 1 ) )
        *( p + 1 ) = lg;
    if( lb > *( p + 2 ) )
        *( p + 2 ) = lb;
}

void HexManager::MarkLightEndNeighbor( ushort hx, ushort hy, bool north_south, uint inten )
{
    Field& f = GetField( hx, hy );
    if( f.Flags.IsWall )
    {
        int lt = f.Corner;
        if( ( north_south && ( lt == CORNER_NORTH_SOUTH || lt == CORNER_NORTH || lt == CORNER_WEST ) ) ||
            ( !north_south && ( lt == CORNER_EAST_WEST || lt == CORNER_EAST ) ) ||
            lt == CORNER_SOUTH )
        {
            uchar* p = &hexLight[ hy * maxHexX * 3 + hx * 3 ];
            int    light_full = inten * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * lightCapacity / 100;
            int    light_self = ( inten / 2 ) * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * lightCapacity / 100;
            int    lr_full = light_full * lightProcentR / 100;
            int    lg_full = light_full * lightProcentG / 100;
            int    lb_full = light_full * lightProcentB / 100;
            int    lr_self = int(*p) + light_self * lightProcentR / 100;
            int    lg_self = int( *( p + 1 ) ) + light_self * lightProcentG / 100;
            int    lb_self = int( *( p + 2 ) ) + light_self * lightProcentB / 100;
            if( lr_self > lr_full )
                lr_self = lr_full;
            if( lg_self > lg_full )
                lg_self = lg_full;
            if( lb_self > lb_full )
                lb_self = lb_full;
            if( lr_self > *p )
                *p = lr_self;
            if( lg_self > *( p + 1 ) )
                *( p + 1 ) = lg_self;
            if( lb_self > *( p + 2 ) )
                *( p + 2 ) = lb_self;
        }
    }
}

void HexManager::MarkLightEnd( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten )
{
    bool   is_wall = false;
    bool   north_south = false;
    Field& f = GetField( to_hx, to_hy );
    if( f.Flags.IsWall )
    {
        is_wall = true;
        if( f.Corner == CORNER_NORTH_SOUTH || f.Corner == CORNER_NORTH || f.Corner == CORNER_WEST )
            north_south = true;
    }

    int dir = GetFarDir( from_hx, from_hy, to_hx, to_hy );
    if( dir == 0 || ( north_south && dir == 1 ) || ( !north_south && ( dir == 4 || dir == 5 ) ) )
    {
        MarkLight( to_hx, to_hy, inten );
        if( is_wall )
        {
            if( north_south )
            {
                if( to_hy > 0 )
                    MarkLightEndNeighbor( to_hx, to_hy - 1, true, inten );
                if( to_hy < maxHexY - 1 )
                    MarkLightEndNeighbor( to_hx, to_hy + 1, true, inten );
            }
            else
            {
                if( to_hx > 0 )
                {
                    MarkLightEndNeighbor( to_hx - 1, to_hy, false, inten );
                    if( to_hy > 0 )
                        MarkLightEndNeighbor( to_hx - 1, to_hy - 1, false, inten );
                    if( to_hy < maxHexY - 1 )
                        MarkLightEndNeighbor( to_hx - 1, to_hy + 1, false, inten );
                }
                if( to_hx < maxHexX - 1 )
                {
                    MarkLightEndNeighbor( to_hx + 1, to_hy, false, inten );
                    if( to_hy > 0 )
                        MarkLightEndNeighbor( to_hx + 1, to_hy - 1, false, inten );
                    if( to_hy < maxHexY - 1 )
                        MarkLightEndNeighbor( to_hx + 1, to_hy + 1, false, inten );
                }
            }
        }
    }
}

void HexManager::MarkLightStep( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten )
{
    Field& f = GetField( to_hx, to_hy );
    if( f.Flags.IsWallTransp )
    {
        bool north_south = ( f.Corner == CORNER_NORTH_SOUTH || f.Corner == CORNER_NORTH || f.Corner == CORNER_WEST );
        int  dir = GetFarDir( from_hx, from_hy, to_hx, to_hy );
        if( dir == 0 || ( north_south && dir == 1 ) || ( !north_south && ( dir == 4 || dir == 5 ) ) )
            MarkLight( to_hx, to_hy, inten );
    }
    else
    {
        MarkLight( to_hx, to_hy, inten );
    }
}

void HexManager::TraceLight( ushort from_hx, ushort from_hy, ushort& hx, ushort& hy, int dist, uint inten )
{
    float base_sx, base_sy;
    GetStepsXY( base_sx, base_sy, from_hx, from_hy, hx, hy );
    float sx1f = base_sx;
    float sy1f = base_sy;
    float curx1f = (float) from_hx;
    float cury1f = (float) from_hy;
    int   curx1i = from_hx;
    int   cury1i = from_hy;
    int   old_curx1i = 0;
    int   old_cury1i = 0;
    uint  inten_sub = inten / dist;

    for( ; ;)
    {
        inten -= inten_sub;
        curx1f += sx1f;
        cury1f += sy1f;
        old_curx1i = curx1i;
        old_cury1i = cury1i;

        // Casts
        curx1i = (int) curx1f;
        if( curx1f - (float) curx1i >= 0.5f )
            curx1i++;
        cury1i = (int) cury1f;
        if( cury1f - (float) cury1i >= 0.5f )
            cury1i++;
        bool can_mark = ( curx1i >= lightMinHx && curx1i <= lightMaxHx && cury1i >= lightMinHy && cury1i <= lightMaxHy );

        // Left&Right trace
        int ox = 0;
        int oy = 0;

        if( old_curx1i & 1 )
        {
            if( old_curx1i + 1 == curx1i && old_cury1i + 1 == cury1i )
            {
                ox = 1;
                oy = 1;
            }
            if( old_curx1i - 1 == curx1i && old_cury1i + 1 == cury1i )
            {
                ox = -1;
                oy = 1;
            }
        }
        else
        {
            if( old_curx1i - 1 == curx1i && old_cury1i - 1 == cury1i )
            {
                ox = -1;
                oy = -1;
            }
            if( old_curx1i + 1 == curx1i && old_cury1i - 1 == cury1i )
            {
                ox = 1;
                oy = -1;
            }
        }

        if( ox )
        {
            // Left side
            ox = old_curx1i + ox;
            if( ox < 0 || ox >= maxHexX || GetField( ox, old_cury1i ).Flags.IsNoLight )
            {
                hx = ( ox < 0 || ox >= maxHexX ? old_curx1i : ox );
                hy = old_cury1i;
                if( can_mark )
                    MarkLightEnd( old_curx1i, old_cury1i, hx, hy, inten );
                break;
            }
            if( can_mark )
                MarkLightStep( old_curx1i, old_cury1i, ox, old_cury1i, inten );

            // Right side
            oy = old_cury1i + oy;
            if( oy < 0 || oy >= maxHexY || GetField( old_curx1i, oy ).Flags.IsNoLight )
            {
                hx = old_curx1i;
                hy = ( oy < 0 || oy >= maxHexY ? old_cury1i : oy );
                if( can_mark )
                    MarkLightEnd( old_curx1i, old_cury1i, hx, hy, inten );
                break;
            }
            if( can_mark )
                MarkLightStep( old_curx1i, old_cury1i, old_curx1i, oy, inten );
        }

        // Main trace
        if( curx1i < 0 || curx1i >= maxHexX || cury1i < 0 || cury1i >= maxHexY || GetField( curx1i, cury1i ).Flags.IsNoLight )
        {
            hx = ( curx1i < 0 || curx1i >= maxHexX ? old_curx1i : curx1i );
            hy = ( cury1i < 0 || cury1i >= maxHexY ? old_cury1i : cury1i );
            if( can_mark )
                MarkLightEnd( old_curx1i, old_cury1i, hx, hy, inten );
            break;
        }
        if( can_mark )
            MarkLightEnd( old_curx1i, old_cury1i, curx1i, cury1i, inten );
        if( curx1i == hx && cury1i == hy )
            break;
    }
}

void HexManager::ParseLightTriangleFan( LightSource& ls )
{
    // All dirs disabled
    if( ( ls.Flags & 0x3F ) == 0x3F )
        return;

    ushort hx = ls.HexX;
    ushort hy = ls.HexY;

    // Distance
    int dist = ls.Distance;
    if( dist < 1 )
        return;

    // Intensity
    int inten = abs( ls.Intensity );
    if( inten > 100 )
        inten = 100;
    inten *= 100;
    if( FLAG( ls.Flags, LIGHT_GLOBAL ) )
        GetColorDay( GetMapDayTime(), GetMapDayColor(), GetDayTime(), &lightCapacity );
    else if( ls.Intensity >= 0 )
        GetColorDay( GetMapDayTime(), GetMapDayColor(), GetMapTime(), &lightCapacity );
    else
        lightCapacity = 100;
    if( FLAG( ls.Flags, LIGHT_INVERSE ) )
        lightCapacity = 100 - lightCapacity;

    // Color
    uint color = ls.ColorRGB;
    int  alpha = MAX_LIGHT_ALPHA * lightCapacity / 100 * inten / MAX_LIGHT_VALUE;
    color = COLOR_RGBA( alpha, ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF );
    lightProcentR = ( ( color >> 16 ) & 0xFF ) * 100 / 0xFF;
    lightProcentG = ( ( color >> 8 ) & 0xFF ) * 100 / 0xFF;
    lightProcentB = ( color & 0xFF ) * 100 / 0xFF;

    // Begin
    MarkLight( hx, hy, inten );
    int base_x, base_y;
    GetHexCurrentPosition( hx, hy, base_x, base_y );
    base_x += HEX_OX;
    base_y += HEX_OY;

    lightPointsCount++;
    if( lightPoints.size() < lightPointsCount )
        lightPoints.push_back( PointVec() );
    PointVec& points = lightPoints[ lightPointsCount - 1 ];
    points.clear();
    points.reserve( 3 + dist * DIRS_COUNT );
    points.push_back( PrepPoint( base_x, base_y, color, ls.OffsX, ls.OffsY ) ); // Center of light

    int    hx_far = hx, hy_far = hy;
    bool   seek_start = true;
    ushort last_hx = -1, last_hy = -1;

    for( int i = 0, ii = ( GameOpt.MapHexagonal ? 6 : 4 ); i < ii; i++ )
    {
        int dir = ( GameOpt.MapHexagonal ? ( i + 2 ) % 6 : ( ( i + 1 ) * 2 ) % 8 );

        for( int j = 0, jj = ( GameOpt.MapHexagonal ? dist : dist * 2 ); j < jj; j++ )
        {
            if( seek_start )
            {
                // Move to start position
                for( int l = 0; l < dist; l++ )
                    MoveHexByDirUnsafe( hx_far, hy_far, GameOpt.MapHexagonal ? 0 : 7 );
                seek_start = false;
                j = -1;
            }
            else
            {
                // Move to next hex
                MoveHexByDirUnsafe( hx_far, hy_far, dir );
            }

            ushort hx_ = CLAMP( hx_far, 0, maxHexX - 1 );
            ushort hy_ = CLAMP( hy_far, 0, maxHexY - 1 );
            if( j >= 0 && FLAG( ls.Flags, LIGHT_DISABLE_DIR( i ) ) )
            {
                hx_ = hx;
                hy_ = hy;
            }
            else
            {
                TraceLight( hx, hy, hx_, hy_, dist, inten );
            }

            if( hx_ != last_hx || hy_ != last_hy )
            {
                short* ox = nullptr;
                short* oy = nullptr;
                if( (int) hx_ != hx_far || (int) hy_ != hy_far )
                {
                    int a = alpha - DistGame( hx, hy, hx_, hy_ ) * alpha / dist;
                    a = CLAMP( a, 0, alpha );
                    color = COLOR_RGBA( a, ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF );
                }
                else
                {
                    color = COLOR_RGBA( 0, ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF );
                    ox = ls.OffsX;
                    oy = ls.OffsY;
                }
                int x, y;
                GetHexInterval( hx, hy, hx_, hy_, x, y );
                points.push_back( PrepPoint( base_x + x, base_y + y, color, ox, oy ) );
                last_hx = hx_;
                last_hy = hy_;
            }
        }
    }

    for( uint i = 1, j = (uint) points.size(); i < j; i++ )
    {
        PrepPoint& cur = points[ i ];
        PrepPoint& next = points[ i >= points.size() - 1 ? 1 : i + 1 ];
        if( DistSqrt( cur.PointX, cur.PointY, next.PointX, next.PointY ) > (uint) LIGHT_SOFT_LENGTH )
        {
            bool dist_comp = ( DistSqrt( base_x, base_y, cur.PointX, cur.PointY ) > DistSqrt( base_x, base_y, next.PointX, next.PointY ) );
            lightSoftPoints.push_back( PrepPoint( next.PointX, next.PointY, next.PointColor, next.PointOffsX, next.PointOffsY ) );
            lightSoftPoints.push_back( PrepPoint( cur.PointX, cur.PointY, cur.PointColor, cur.PointOffsX, cur.PointOffsY ) );
            float x = (float) ( dist_comp ? next.PointX - cur.PointX : cur.PointX - next.PointX );
            float y = (float) ( dist_comp ? next.PointY - cur.PointY : cur.PointY - next.PointY );
            ChangeStepsXY( x, y, dist_comp ? -2.5f : 2.5f );
            if( dist_comp )
                lightSoftPoints.push_back( PrepPoint( cur.PointX + int(x), cur.PointY + int(y), cur.PointColor, cur.PointOffsX, cur.PointOffsY ) );
            else
                lightSoftPoints.push_back( PrepPoint( next.PointX + int(x), next.PointY + int(y), next.PointColor, next.PointOffsX, next.PointOffsY ) );
        }
    }
}

void HexManager::RealRebuildLight()
{
    RUNTIME_ASSERT( viewField );

    lightPointsCount = 0;
    lightSoftPoints.clear();
    ClearHexLight();
    CollectLightSources();

    lightMinHx = viewField[ 0 ].HexX;
    lightMaxHx = viewField[ hVisible * wVisible - 1 ].HexX;
    lightMinHy = viewField[ wVisible - 1 ].HexY;
    lightMaxHy = viewField[ hVisible * wVisible - wVisible ].HexY;

    for( auto it = lightSources.begin(), end = lightSources.end(); it != end; ++it )
    {
        LightSource& ls = *it;
        #pragma MESSAGE( "Optimize lighting rebuilding to skip unvisible lights." )
        // if( (int) ls.HexX < lightMinHx - (int) ls.Distance || (int) ls.HexX > lightMaxHx + (int) ls.Distance ||
        //    (int) ls.HexY < lightMinHy - (int) ls.Distance || (int) ls.HexY > lightMaxHy + (int) ls.Distance )
        //    continue;
        ParseLightTriangleFan( ls );
    }
}

void HexManager::CollectLightSources()
{
    lightSources.clear();

    if( !IsMapLoaded() )
        return;

    // Scenery
    #ifndef FONLINE_MAPPER
    lightSources = lightSourcesScen;
    #else
    for( auto& item : hexItems )
        if( item->IsStatic() && item->GetIsLight() )
            lightSources.push_back( LightSource( item->GetHexX(), item->GetHexY(), item->LightGetColor(), item->LightGetDistance(), item->LightGetIntensity(), item->LightGetFlags() ) );
    #endif

    // Items on ground
    for( auto& item : hexItems )
        if( !item->IsStatic() && item->GetIsLight() )
            lightSources.push_back( LightSource( item->GetHexX(), item->GetHexY(), item->LightGetColor(), item->LightGetDistance(), item->LightGetIntensity(), item->LightGetFlags() ) );

    // Items in critters slots
    for( auto& kv : allCritters )
    {
        CritterCl* cr = kv.second;
        bool       added = false;
        for( auto it_ = cr->InvItems.begin(), end_ = cr->InvItems.end(); it_ != end_; ++it_ )
        {
            Item* item = *it_;
            if( item->GetIsLight() && item->GetCritSlot() )
            {
                lightSources.push_back( LightSource( cr->GetHexX(), cr->GetHexY(), item->LightGetColor(), item->LightGetDistance(), item->LightGetIntensity(), item->LightGetFlags(), &cr->SprOx, &cr->SprOy ) );
                added = true;
            }
        }

        // Default chosen light
        #ifndef FONLINE_MAPPER
        if( cr->IsChosen() && !added )
            lightSources.push_back( LightSource( cr->GetHexX(), cr->GetHexY(), GameOpt.ChosenLightColor, GameOpt.ChosenLightDistance, GameOpt.ChosenLightIntensity, GameOpt.ChosenLightFlags, &cr->SprOx, &cr->SprOy ) );
        #endif
    }
}

/************************************************************************/
/* Tiles                                                                */
/************************************************************************/

bool HexManager::CheckTilesBorder( Field::Tile& tile, bool is_roof )
{
    int ox = ( is_roof ? ROOF_OX : TILE_OX ) + tile.OffsX;
    int oy = ( is_roof ? ROOF_OY : TILE_OY ) + tile.OffsY;
    return ProcessHexBorders( tile.Anim->GetSprId( 0 ), ox, oy, false );
}

void HexManager::RebuildTiles()
{
    tilesTree.Unvalidate();

    if( !GameOpt.ShowTile )
        return;

    int vpos;
    int y2 = 0;
    for( int ty = 0; ty < hVisible; ty++ )
    {
        for( int x = 0; x < wVisible; x++ )
        {
            vpos = y2 + x;
            if( vpos < 0 || vpos >= wVisible * hVisible )
                continue;
            int hx = viewField[ vpos ].HexX;
            int hy = viewField[ vpos ].HexY;

            if( hy < 0 || hx < 0 || hy >= maxHexY || hx >= maxHexX )
                continue;

            Field& f = GetField( hx, hy );
            uint   tiles_count = f.GetTilesCount( false );
            if( !tiles_count )
                continue;

            for( uint i = 0; i < tiles_count; i++ )
            {
                Field::Tile& tile = f.GetTile( i, false );
                uint         spr_id = tile.Anim->GetSprId( 0 );

                #ifdef FONLINE_MAPPER
                ProtoMap::TileVec& tiles = GetTiles( hx, hy, false );
                Sprite&            spr = tilesTree.AddSprite( DRAW_ORDER_TILE + tile.Layer, hx, hy, 0, tile.OffsX + TILE_OX, tile.OffsY + TILE_OY,
                                                              &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                              tiles[ i ].IsSelected ? (uchar*) &SELECT_ALPHA : nullptr, &Effect::Tile, nullptr );
                #else
                Sprite& spr = tilesTree.AddSprite( DRAW_ORDER_TILE + tile.Layer, hx, hy, 0, tile.OffsX + TILE_OX, tile.OffsY + TILE_OY,
                                                   &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                   nullptr, &Effect::Tile, nullptr );
                #endif
                f.AddSpriteToChain( &spr );
            }
        }
        y2 += wVisible;
    }

    // Sort
    tilesTree.SortByMapPos();
}

void HexManager::RebuildRoof()
{
    roofTree.Unvalidate();

    if( !GameOpt.ShowRoof )
        return;

    int vpos;
    int y2 = 0;
    for( int ty = 0; ty < hVisible; ty++ )
    {
        for( int x = 0; x < wVisible; x++ )
        {
            vpos = y2 + x;
            int hx = viewField[ vpos ].HexX;
            int hy = viewField[ vpos ].HexY;

            if( hy < 0 || hx < 0 || hy >= maxHexY || hx >= maxHexX )
                continue;

            Field& f = GetField( hx, hy );
            uint   roofs_count = f.GetTilesCount( true );
            if( !roofs_count )
                continue;

            if( !roofSkip || roofSkip != f.RoofNum )
            {
                for( uint i = 0; i < roofs_count; i++ )
                {
                    Field::Tile& roof = f.GetTile( i, true );
                    uint         spr_id = roof.Anim->GetSprId( 0 );

                    #ifdef FONLINE_MAPPER
                    ProtoMap::TileVec& roofs = GetTiles( hx, hy, true );
                    Sprite&            spr = roofTree.AddSprite( DRAW_ORDER_TILE + roof.Layer, hx, hy, 0, roof.OffsX + ROOF_OX, roof.OffsY + ROOF_OY,
                                                                 &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                                 roofs[ i ].IsSelected ? (uchar*) &SELECT_ALPHA : &GameOpt.RoofAlpha, &Effect::Roof, nullptr );
                    #else
                    Sprite& spr = roofTree.AddSprite( DRAW_ORDER_TILE + roof.Layer, hx, hy, 0, roof.OffsX + ROOF_OX, roof.OffsY + ROOF_OY,
                                                      &f.ScrX, &f.ScrY, spr_id, nullptr, nullptr, nullptr,
                                                      &GameOpt.RoofAlpha, &Effect::Roof, nullptr );
                    #endif
                    spr.SetEgg( EGG_ALWAYS );
                    f.AddSpriteToChain( &spr );
                }
            }
        }
        y2 += wVisible;
    }

    // Sort
    roofTree.SortByMapPos();
}

void HexManager::SetSkipRoof( int hx, int hy )
{
    if( roofSkip != GetField( hx, hy ).RoofNum )
    {
        roofSkip = GetField( hx, hy ).RoofNum;
        if( rainCapacity )
            RefreshMap();
        else
            RebuildRoof();
    }
}

void HexManager::MarkRoofNum( int hx, int hy, int num )
{
    if( hx < 0 || hx >= maxHexX || hy < 0 || hy >= maxHexY )
        return;
    if( !GetField( hx, hy ).GetTilesCount( true ) )
        return;
    if( GetField( hx, hy ).RoofNum )
        return;

    for( int x = 0; x < GameOpt.MapRoofSkipSize; x++ )
        for( int y = 0; y < GameOpt.MapRoofSkipSize; y++ )
            if( hx + x >= 0 && hx + x < maxHexX && hy + y >= 0 && hy + y < maxHexY )
                GetField( hx + x, hy + y ).RoofNum = num;

    MarkRoofNum( hx + GameOpt.MapRoofSkipSize, hy, num );
    MarkRoofNum( hx - GameOpt.MapRoofSkipSize, hy, num );
    MarkRoofNum( hx, hy + GameOpt.MapRoofSkipSize, num );
    MarkRoofNum( hx, hy - GameOpt.MapRoofSkipSize, num );
}

bool HexManager::IsVisible( uint spr_id, int ox, int oy )
{
    if( !spr_id )
        return false;
    SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
    if( !si )
        return false;

    int top = oy + si->OffsY - si->Height - SCROLL_OY;
    int bottom = oy + si->OffsY + SCROLL_OY;
    int left = ox + si->OffsX - si->Width / 2 - SCROLL_OX;
    int right = ox + si->OffsX + si->Width / 2 + SCROLL_OX;
    return !( top > GameOpt.ScreenHeight * GameOpt.SpritesZoom || bottom < 0 || left > GameOpt.ScreenWidth * GameOpt.SpritesZoom || right < 0 );
}

bool HexManager::ProcessHexBorders( ItemHex* item )
{
    return ProcessHexBorders( item->Anim->GetSprId( 0 ), item->GetOffsetX(), item->GetOffsetY(), true );
}

bool HexManager::ProcessHexBorders( uint spr_id, int ox, int oy, bool resize_map )
{
    SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
    if( !si )
        return false;

    int top = si->OffsY + oy - hTop * HEX_LINE_H + SCROLL_OY;
    if( top < 0 )
        top = 0;
    int bottom = si->Height - si->OffsY - oy - hBottom * HEX_LINE_H + SCROLL_OY;
    if( bottom < 0 )
        bottom = 0;
    int left = si->Width / 2 + si->OffsX + ox - wLeft * HEX_W + SCROLL_OX;
    if( left < 0 )
        left = 0;
    int right = si->Width / 2 - si->OffsX - ox - wRight * HEX_W + SCROLL_OX;
    if( right < 0 )
        right = 0;

    if( top || bottom || left || right )
    {
        // Resize
        hTop += top / HEX_LINE_H + ( ( top % HEX_LINE_H ) ? 1 : 0 );
        hBottom += bottom / HEX_LINE_H + ( ( bottom % HEX_LINE_H ) ? 1 : 0 );
        wLeft += left / HEX_W + ( ( left % HEX_W ) ? 1 : 0 );
        wRight += right / HEX_W + ( ( right % HEX_W ) ? 1 : 0 );

        if( resize_map )
        {
            ResizeView();
            RefreshMap();
        }
        return true;
    }
    return false;
}

void HexManager::SwitchShowHex()
{
    isShowHex = !isShowHex;
    RefreshMap();
}

void HexManager::SwitchShowRain()
{
    rainCapacity = ( rainCapacity ? 0 : 255 );
    RefreshMap();
}

void HexManager::SetWeather( int time, uchar rain )
{
    curMapTime = time;
    rainCapacity = rain;
}

void HexManager::ResizeField( ushort w, ushort h )
{
    GameOpt.ClientMap = nullptr;
    GameOpt.ClientMapLight = nullptr;
    GameOpt.ClientMapWidth = 0;
    GameOpt.ClientMapHeight = 0;

    maxHexX = w;
    maxHexY = h;
    SAFEDELA( hexField );
    SAFEDELA( hexTrack );
    SAFEDELA( hexLight );
    if( !w || !h )
        return;

    hexField = new Field[ w * h ];
    memzero( hexField, w * h * sizeof( Field ) );
    hexTrack = new char[ w * h ];
    memzero( hexTrack, w * h * sizeof( char ) );
    hexLight = new uchar[ w * h * 3 ];
    memzero( hexLight, w * h * 3 * sizeof( uchar ) );

    GameOpt.ClientMap = hexField;
    GameOpt.ClientMapLight = hexLight;
    GameOpt.ClientMapWidth = w;
    GameOpt.ClientMapHeight = h;
}

void HexManager::SwitchShowTrack()
{
    isShowTrack = !isShowTrack;
    if( !isShowTrack )
        ClearHexTrack();
    if( IsMapLoaded() )
        RefreshMap();
}

void HexManager::InitView( int cx, int cy )
{
    if( GameOpt.MapHexagonal )
    {
        // Get center offset
        int hw = VIEW_WIDTH / 2 + wRight;
        int hv = VIEW_HEIGHT / 2 + hTop;
        int vw = hv / 2 + ( hv & 1 ) + 1;
        int vh = hv - vw / 2 - 1;
        for( int i = 0; i < hw; i++ )
        {
            if( vw & 1 )
                vh--;
            vw++;
        }

        // Subtract offset
        cx -= abs( vw );
        cy -= abs( vh );

        int xa = -( wRight * HEX_W );
        int xb = -( HEX_W / 2 ) - ( wRight * HEX_W );
        int oy = -HEX_LINE_H * hTop;
        int wx = (int) ( GameOpt.ScreenWidth * GameOpt.SpritesZoom );

        for( int yv = 0; yv < hVisible; yv++ )
        {
            int hx = cx + yv / 2 + ( yv & 1 );
            int hy = cy + ( yv - ( hx - cx - ( cx & 1 ) ) / 2 );
            int ox = ( ( yv & 1 ) ? xa : xb );

            if( yv == 0 && ( cx & 1 ) )
                hy++;

            for( int xv = 0; xv < wVisible; xv++ )
            {
                ViewField& vf = viewField[ yv * wVisible + xv ];
                vf.ScrX = wx - ox;
                vf.ScrY = oy;
                vf.ScrXf = (float) vf.ScrX;
                vf.ScrYf = (float) vf.ScrY;
                vf.HexX = hx;
                vf.HexY = hy;

                if( hx & 1 )
                    hy--;
                hx++;
                ox += HEX_W;
            }
            oy += HEX_LINE_H;
        }
    }
    else
    {
        // Calculate data
        int halfw = VIEW_WIDTH / 2 + wRight;
        int halfh = VIEW_HEIGHT / 2 + hTop;
        int basehx = cx - halfh / 2 - halfw;
        int basehy = cy - halfh / 2 + halfw;
        int y2 = 0;
        int vpos;
        int x;
        int xa = -HEX_W * wRight;
        int xb = -HEX_W * wRight - HEX_W / 2;
        int y = -HEX_LINE_H * hTop;
        int wx = (int) ( GameOpt.ScreenWidth * GameOpt.SpritesZoom );
        int hx, hy;

        // Initialize field
        for( int j = 0; j < hVisible; j++ )
        {
            x = ( ( j & 1 ) ? xa : xb );
            hx = basehx;
            hy = basehy;

            for( int i = 0; i < wVisible; i++ )
            {
                vpos = y2 + i;
                viewField[ vpos ].ScrX = wx - x;
                viewField[ vpos ].ScrY = y;
                viewField[ vpos ].ScrXf = (float) viewField[ vpos ].ScrX;
                viewField[ vpos ].ScrYf = (float) viewField[ vpos ].ScrY;
                viewField[ vpos ].HexX = hx;
                viewField[ vpos ].HexY = hy;

                hx++;
                hy--;
                x += HEX_W;
            }

            if( j & 1 )
                basehy++;
            else
                basehx++;

            y += HEX_LINE_H;
            y2 += wVisible;
        }
    }
}

void HexManager::ResizeView()
{
    if( viewField )
    {
        for( int i = 0, j = hVisible * wVisible; i < j; i++ )
        {
            ViewField& vf = viewField[ i ];
            if( vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < maxHexX && vf.HexY < maxHexY )
            {
                Field& f = GetField( vf.HexX, vf.HexY );
                f.IsView = false;
                f.UnvalidateSpriteChain();
            }
        }
    }

    hVisible = VIEW_HEIGHT + hTop + hBottom;
    wVisible = VIEW_WIDTH + wLeft + wRight;
    SAFEDELA( viewField );
    viewField = new ViewField[ hVisible * wVisible ];
}

void HexManager::ChangeZoom( int zoom )
{
    if( !IsMapLoaded() )
        return;
    if( GameOpt.SpritesZoomMin == GameOpt.SpritesZoomMax )
        return;
    if( !zoom && GameOpt.SpritesZoom == 1.0f )
        return;
    if( zoom > 0 && GameOpt.SpritesZoom >= MIN( GameOpt.SpritesZoomMax, MAX_ZOOM ) )
        return;
    if( zoom < 0 && GameOpt.SpritesZoom <= MAX( GameOpt.SpritesZoomMin, MIN_ZOOM ) )
        return;

    // Check screen blockers
    if( GameOpt.ScrollCheck && ( zoom > 0 || ( zoom == 0 && GameOpt.SpritesZoom < 1.0f ) ) )
    {
        for( int x = -1; x <= 1; x++ )
        {
            for( int y = -1; y <= 1; y++ )
            {
                if( ( x || y ) && ScrollCheck( x, y ) )
                    return;
            }
        }
    }

    if( zoom || GameOpt.SpritesZoom < 1.0f )
    {
        float old_zoom = GameOpt.SpritesZoom;
        float w = (float) ( GameOpt.ScreenWidth / HEX_W + ( ( GameOpt.ScreenWidth % HEX_W ) ? 1 : 0 ) );
        GameOpt.SpritesZoom = ( w * GameOpt.SpritesZoom + ( zoom >= 0 ? 2.0f : -2.0f ) ) / w;

        if( GameOpt.SpritesZoom < MAX( GameOpt.SpritesZoomMin, MIN_ZOOM ) || GameOpt.SpritesZoom > MIN( GameOpt.SpritesZoomMax, MAX_ZOOM ) )
        {
            GameOpt.SpritesZoom = old_zoom;
            return;
        }
    }
    else
    {
        GameOpt.SpritesZoom = 1.0f;
    }

    ResizeView();
    RefreshMap();

    if( zoom == 0 && GameOpt.SpritesZoom != 1.0f )
        ChangeZoom( 0 );
}

void HexManager::GetScreenHexes( int& sx, int& sy )
{
    sx = screenHexX;
    sy = screenHexY;
}

void HexManager::GetHexCurrentPosition( ushort hx, ushort hy, int& x, int& y )
{
    ViewField& center_hex = viewField[ hVisible / 2 * wVisible + wVisible / 2 ];
    int        center_hx = center_hex.HexX;
    int        center_hy = center_hex.HexY;

    GetHexInterval( center_hx, center_hy, hx, hy, x, y );
    x += center_hex.ScrX;
    y += center_hex.ScrY;
}

void HexManager::DrawMap()
{
    // Prepare light
    PrepareLightToDraw();

    // Prepare fog
    PrepareFogToDraw();

    // Prerendered offsets
    int  ox = rtScreenOX - (int) roundf( (float) GameOpt.ScrOx / GameOpt.SpritesZoom );
    int  oy = rtScreenOY - (int) roundf( (float) GameOpt.ScrOy / GameOpt.SpritesZoom );
    Rect prerenderedRect = Rect( ox, oy, ox + GameOpt.ScreenWidth, oy + GameOpt.ScreenHeight );

    // Separate render target
    if( rtMap )
    {
        SprMngr.PushRenderTarget( rtMap );
        SprMngr.ClearCurrentRenderTarget( 0 );
    }

    // Tiles
    if( GameOpt.ShowTile )
        SprMngr.DrawSprites( tilesTree, false, false, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END );

    // Flat sprites
    SprMngr.DrawSprites( mainTree, true, false, DRAW_ORDER_FLAT, DRAW_ORDER_LIGHT - 1 );

    // Light
    if( rtLight )
        SprMngr.DrawRenderTarget( rtLight, true, &prerenderedRect );

    // Cursor flat
    DrawCursor( cursorPrePic->GetCurSprId() );

    // Sprites
    SprMngr.DrawSprites( mainTree, true, true, DRAW_ORDER_LIGHT, DRAW_ORDER_LAST );

    // Roof
    if( GameOpt.ShowRoof )
    {
        SprMngr.DrawSprites( roofTree, false, true, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END );
        if( rainCapacity )
            SprMngr.DrawSprites( roofRainTree, false, false, DRAW_ORDER_RAIN, DRAW_ORDER_RAIN );
    }

    // Contours
    SprMngr.DrawContours();

    // Fog
    if( rtFog )
        SprMngr.DrawRenderTarget( rtFog, true, &prerenderedRect );

    // Cursor
    DrawCursor( cursorPostPic->GetCurSprId() );
    if( drawCursorX < 0 )
        DrawCursor( cursorXPic->GetCurSprId() );
    else if( drawCursorX > 0 )
        DrawCursor( _str( "{}", drawCursorX ).c_str() );

    // Draw map from render target
    if( rtMap )
    {
        SprMngr.PopRenderTarget();
        SprMngr.DrawRenderTarget( rtMap, false );
    }
}

void HexManager::SetFog( PointVec& look_points, PointVec& shoot_points, short* offs_x, short* offs_y )
{
    if( !rtFog )
        return;

    fogOffsX = offs_x;
    fogOffsY = offs_y;
    fogLastOffsX = ( fogOffsX ? *fogOffsX : 0 );
    fogLastOffsY = ( fogOffsY ? *fogOffsY : 0 );
    fogForceRerender = true;
    fogLookPoints = look_points;
    fogShootPoints = shoot_points;
}

void HexManager::PrepareFogToDraw()
{
    if( !rtFog )
        return;
    if( !fogForceRerender && ( fogOffsX && *fogOffsX == fogLastOffsX && *fogOffsY == fogLastOffsY ) )
        return;
    if( fogOffsX )
    {
        fogLastOffsX = *fogOffsX;
        fogLastOffsY = *fogOffsY;
    }
    fogForceRerender = false;

    PointF offset( (float) rtScreenOX, (float) rtScreenOY );
    SprMngr.PushRenderTarget( rtFog );
    SprMngr.ClearCurrentRenderTarget( 0 );
    SprMngr.DrawPoints( fogLookPoints, PRIMITIVE_TRIANGLEFAN, &GameOpt.SpritesZoom, &offset, Effect::Fog );
    SprMngr.DrawPoints( fogShootPoints, PRIMITIVE_TRIANGLEFAN, &GameOpt.SpritesZoom, &offset, Effect::Fog );
    SprMngr.PopRenderTarget();
}

bool HexManager::Scroll()
{
    if( !IsMapLoaded() )
        return false;

    // Scroll delay
    float time_k = 1.0f;
    if( GameOpt.ScrollDelay )
    {
        uint        tick = Timer::FastTick();
        static uint last_tick = tick;
        if( tick - last_tick < GameOpt.ScrollDelay / 2 )
            return false;
        time_k = (float) ( tick - last_tick ) / (float) GameOpt.ScrollDelay;
        last_tick = tick;
    }

    bool is_scroll = ( GameOpt.ScrollMouseLeft || GameOpt.ScrollKeybLeft ||
                       GameOpt.ScrollMouseRight || GameOpt.ScrollKeybRight ||
                       GameOpt.ScrollMouseUp || GameOpt.ScrollKeybUp ||
                       GameOpt.ScrollMouseDown || GameOpt.ScrollKeybDown );
    int scr_ox = GameOpt.ScrOx;
    int scr_oy = GameOpt.ScrOy;
    int prev_scr_ox = scr_ox;
    int prev_scr_oy = scr_oy;

    if( is_scroll && AutoScroll.CanStop )
        AutoScroll.Active = false;

    // Check critter scroll lock
    if( AutoScroll.HardLockedCritter && !is_scroll )
    {
        CritterCl* cr = GetCritter( AutoScroll.HardLockedCritter );
        if( cr && ( cr->GetHexX() != screenHexX || cr->GetHexY() != screenHexY ) )
            ScrollToHex( cr->GetHexX(), cr->GetHexY(), 0.02f, true );
    }

    if( AutoScroll.SoftLockedCritter && !is_scroll )
    {
        CritterCl* cr = GetCritter( AutoScroll.SoftLockedCritter );
        if( cr && ( cr->GetHexX() != AutoScroll.CritterLastHexX || cr->GetHexY() != AutoScroll.CritterLastHexY ) )
        {
            int ox, oy;
            GetHexInterval( AutoScroll.CritterLastHexX, AutoScroll.CritterLastHexY, cr->GetHexX(), cr->GetHexY(), ox, oy );
            ScrollOffset( ox, oy, 0.02f, true );
            AutoScroll.CritterLastHexX = cr->GetHexX();
            AutoScroll.CritterLastHexY = cr->GetHexY();
        }
    }

    int xscroll = 0;
    int yscroll = 0;
    if( AutoScroll.Active )
    {
        AutoScroll.OffsXStep += AutoScroll.OffsX * AutoScroll.Speed * time_k;
        AutoScroll.OffsYStep += AutoScroll.OffsY * AutoScroll.Speed * time_k;
        xscroll = (int) AutoScroll.OffsXStep;
        yscroll = (int) AutoScroll.OffsYStep;
        if( xscroll > SCROLL_OX )
        {
            xscroll = SCROLL_OX;
            AutoScroll.OffsXStep = (float) SCROLL_OX;
        }
        if( xscroll < -SCROLL_OX )
        {
            xscroll = -SCROLL_OX;
            AutoScroll.OffsXStep = -(float) SCROLL_OX;
        }
        if( yscroll > SCROLL_OY )
        {
            yscroll = SCROLL_OY;
            AutoScroll.OffsYStep = (float) SCROLL_OY;
        }
        if( yscroll < -SCROLL_OY )
        {
            yscroll = -SCROLL_OY;
            AutoScroll.OffsYStep = -(float) SCROLL_OY;
        }

        AutoScroll.OffsX -= xscroll;
        AutoScroll.OffsY -= yscroll;
        AutoScroll.OffsXStep -= xscroll;
        AutoScroll.OffsYStep -= yscroll;
        if( !xscroll && !yscroll )
            return false;
        if( !DistSqrt( 0, 0, (int) AutoScroll.OffsX, (int) AutoScroll.OffsY ) )
            AutoScroll.Active = false;
    }
    else
    {
        if( !is_scroll )
            return false;

        if( GameOpt.ScrollMouseLeft || GameOpt.ScrollKeybLeft )
            xscroll += 1;
        if( GameOpt.ScrollMouseRight || GameOpt.ScrollKeybRight )
            xscroll -= 1;
        if( GameOpt.ScrollMouseUp || GameOpt.ScrollKeybUp )
            yscroll += 1;
        if( GameOpt.ScrollMouseDown || GameOpt.ScrollKeybDown )
            yscroll -= 1;
        if( !xscroll && !yscroll )
            return false;

        xscroll = (int) ( xscroll * GameOpt.ScrollStep * GameOpt.SpritesZoom * time_k );
        yscroll = (int) ( yscroll * ( GameOpt.ScrollStep * SCROLL_OY / SCROLL_OX ) * GameOpt.SpritesZoom * time_k );
    }
    scr_ox += xscroll;
    scr_oy += yscroll;

    if( GameOpt.ScrollCheck )
    {
        int xmod = 0;
        int ymod = 0;
        if( scr_ox - GameOpt.ScrOx > 0 )
            xmod = 1;
        if( scr_ox - GameOpt.ScrOx < 0 )
            xmod = -1;
        if( scr_oy - GameOpt.ScrOy > 0 )
            ymod = -1;
        if( scr_oy - GameOpt.ScrOy < 0 )
            ymod = 1;
        if( ( xmod || ymod ) && ScrollCheck( xmod, ymod ) )
        {
            if( xmod && ymod && !ScrollCheck( 0, ymod ) )
            {
                scr_ox = 0;
            }
            else if( xmod && ymod && !ScrollCheck( xmod, 0 ) )
            {
                scr_oy = 0;
            }
            else
            {
                if( xmod )
                    scr_ox = 0;
                if( ymod )
                    scr_oy = 0;
            }
        }
    }

    int xmod = 0;
    int ymod = 0;
    if( scr_ox >= SCROLL_OX )
    {
        xmod = 1;
        scr_ox -= SCROLL_OX;
        if( scr_ox > SCROLL_OX )
            scr_ox = SCROLL_OX;
    }
    else if( scr_ox <= -SCROLL_OX )
    {
        xmod = -1;
        scr_ox += SCROLL_OX;
        if( scr_ox < -SCROLL_OX )
            scr_ox = -SCROLL_OX;
    }
    if( scr_oy >= SCROLL_OY )
    {
        ymod = -2;
        scr_oy -= SCROLL_OY;
        if( scr_oy > SCROLL_OY )
            scr_oy = SCROLL_OY;
    }
    else if( scr_oy <= -SCROLL_OY )
    {
        ymod = 2;
        scr_oy += SCROLL_OY;
        if( scr_oy < -SCROLL_OY )
            scr_oy = -SCROLL_OY;
    }

    GameOpt.ScrOx = scr_ox;
    GameOpt.ScrOy = scr_oy;

    if( xmod || ymod )
    {
        RebuildMapOffset( xmod, ymod );

        if( GameOpt.ScrollCheck )
        {
            if( GameOpt.ScrOx > 0 && ScrollCheck( 1, 0 ) )
                GameOpt.ScrOx = 0;
            else if( GameOpt.ScrOx < 0 && ScrollCheck( -1, 0 ) )
                GameOpt.ScrOx = 0;
            if( GameOpt.ScrOy > 0 && ScrollCheck( 0, -1 ) )
                GameOpt.ScrOy = 0;
            else if( GameOpt.ScrOy < 0 && ScrollCheck( 0, 1 ) )
                GameOpt.ScrOy = 0;
        }
    }

    #ifdef FONLINE_CLIENT
    int final_scr_ox = GameOpt.ScrOx - prev_scr_ox + xmod * SCROLL_OX;
    int final_scr_oy = GameOpt.ScrOy - prev_scr_oy + ( -ymod / 2 ) * SCROLL_OY;
    if( final_scr_ox || final_scr_oy )
        Script::RaiseInternalEvent( ClientFunctions.ScreenScroll, final_scr_ox, final_scr_oy );
    #endif

    return xmod || ymod;
}

bool HexManager::ScrollCheckPos( int(&positions)[ 4 ], int dir1, int dir2 )
{
    int max_pos = wVisible * hVisible;
    for( int i = 0; i < 4; i++ )
    {
        int pos = positions[ i ];
        if( pos < 0 || pos >= max_pos )
            return true;

        ushort hx = viewField[ pos ].HexX;
        ushort hy = viewField[ pos ].HexY;
        if( hx >= maxHexX || hy >= maxHexY )
            return true;

        MoveHexByDir( hx, hy, dir1, maxHexX, maxHexY );
        if( GetField( hx, hy ).Flags.ScrollBlock )
            return true;
        if( dir2 >= 0 )
        {
            MoveHexByDir( hx, hy, dir2, maxHexX, maxHexY );
            if( GetField( hx, hy ).Flags.ScrollBlock )
                return true;
        }
    }
    return false;
}

bool HexManager::ScrollCheck( int xmod, int ymod )
{
    int positions_left[ 4 ] =
    {
        hTop* wVisible + wRight + VIEW_WIDTH,                            // Left top
        ( hTop + VIEW_HEIGHT - 1 ) * wVisible + wRight + VIEW_WIDTH,     // Left bottom
        ( hTop + 1 ) * wVisible + wRight + VIEW_WIDTH,                   // Left top 2
        ( hTop + VIEW_HEIGHT - 1 - 1 ) * wVisible + wRight + VIEW_WIDTH, // Left bottom 2
    };
    int positions_right[ 4 ] =
    {
        ( hTop + VIEW_HEIGHT - 1 ) * wVisible + wRight + 1,            // Right bottom
        hTop * wVisible + wRight + 1,                                  // Right top
        ( hTop + VIEW_HEIGHT - 1 - 1 ) * wVisible + wRight + 1,        // Right bottom 2
        ( hTop + 1 ) * wVisible + wRight + 1,                          // Right top 2
    };

    int dirs[ 8 ] = { 0, 5, 2, 3, 4, -1, 1, -1 };                      // Hexagonal
    if( !GameOpt.MapHexagonal )
        dirs[ 0 ] = 7, dirs[ 1 ] = -1, dirs[ 2 ] = 3, dirs[ 3 ] = -1,
        dirs[ 4 ] = 5, dirs[ 5 ] = -1, dirs[ 6 ] = 1, dirs[ 7 ] = -1;  // Square

    if( GameOpt.MapHexagonal )
    {
        if( ymod < 0 && ( ScrollCheckPos( positions_left, 0, 5 ) || ScrollCheckPos( positions_right, 5, 0 ) ) )
            return true;                                                                                               // Up
        else if( ymod > 0 && ( ScrollCheckPos( positions_left, 2, 3 ) || ScrollCheckPos( positions_right, 3, 2 ) ) )
            return true;                                                                                               // Down
        if( xmod > 0 && ( ScrollCheckPos( positions_left, 4, -1 ) || ScrollCheckPos( positions_right, 4, -1 ) ) )
            return true;                                                                                               // Left
        else if( xmod < 0 && ( ScrollCheckPos( positions_right, 1, -1 ) || ScrollCheckPos( positions_left, 1, -1 ) ) )
            return true;                                                                                               // Right
    }
    else
    {
        if( ymod < 0 && ( ScrollCheckPos( positions_left, 0, 6 ) || ScrollCheckPos( positions_right, 6, 0 ) ) )
            return true;                                                                                               // Up
        else if( ymod > 0 && ( ScrollCheckPos( positions_left, 2, 4 ) || ScrollCheckPos( positions_right, 4, 2 ) ) )
            return true;                                                                                               // Down
        if( xmod > 0 && ( ScrollCheckPos( positions_left, 6, 4 ) || ScrollCheckPos( positions_right, 4, 6 ) ) )
            return true;                                                                                               // Left
        else if( xmod < 0 && ( ScrollCheckPos( positions_right, 0, 2 ) || ScrollCheckPos( positions_left, 2, 0 ) ) )
            return true;                                                                                               // Right
    }

    // Add precise for zooming infelicity
    if( GameOpt.SpritesZoom != 1.0f )
    {
        for( int i = 0; i < 4; i++ )
            positions_left[ i ]--;
        for( int i = 0; i < 4; i++ )
            positions_right[ i ]++;

        if( GameOpt.MapHexagonal )
        {
            if( ymod < 0 && ( ScrollCheckPos( positions_left, 0, 5 ) || ScrollCheckPos( positions_right, 5, 0 ) ) )
                return true;                                                                                                   // Up
            else if( ymod > 0 && ( ScrollCheckPos( positions_left, 2, 3 ) || ScrollCheckPos( positions_right, 3, 2 ) ) )
                return true;                                                                                                   // Down
            if( xmod > 0 && ( ScrollCheckPos( positions_left, 4, -1 ) || ScrollCheckPos( positions_right, 4, -1 ) ) )
                return true;                                                                                                   // Left
            else if( xmod < 0 && ( ScrollCheckPos( positions_right, 1, -1 ) || ScrollCheckPos( positions_left, 1, -1 ) ) )
                return true;                                                                                                   // Right
        }
        else
        {
            if( ymod < 0 && ( ScrollCheckPos( positions_left, 0, 6 ) || ScrollCheckPos( positions_right, 6, 0 ) ) )
                return true;                                                                                                   // Up
            else if( ymod > 0 && ( ScrollCheckPos( positions_left, 2, 4 ) || ScrollCheckPos( positions_right, 4, 2 ) ) )
                return true;                                                                                                   // Down
            if( xmod > 0 && ( ScrollCheckPos( positions_left, 6, 4 ) || ScrollCheckPos( positions_right, 4, 6 ) ) )
                return true;                                                                                                   // Left
            else if( xmod < 0 && ( ScrollCheckPos( positions_right, 0, 2 ) || ScrollCheckPos( positions_left, 2, 0 ) ) )
                return true;                                                                                                   // Right
        }
    }
    return false;
}

void HexManager::ScrollToHex( int hx, int hy, float speed, bool can_stop )
{
    int sx, sy;
    GetScreenHexes( sx, sy );
    int ox, oy;
    GetHexInterval( sx, sy, hx, hy, ox, oy );
    AutoScroll.Active = false;
    ScrollOffset( ox, oy, speed, can_stop );
}

void HexManager::ScrollOffset( int ox, int oy, float speed, bool can_stop )
{
    if( !AutoScroll.Active )
    {
        AutoScroll.Active = true;
        AutoScroll.OffsX = 0;
        AutoScroll.OffsY = 0;
        AutoScroll.OffsXStep = 0.0;
        AutoScroll.OffsYStep = 0.0;
    }

    AutoScroll.CanStop = can_stop;
    AutoScroll.Speed = speed;
    AutoScroll.OffsX += -ox;
    AutoScroll.OffsY += -oy;
}

void HexManager::SetCritter( CritterCl* cr )
{
    if( !IsMapLoaded() )
        return;

    ushort hx = cr->GetHexX();
    ushort hy = cr->GetHexY();
    Field& f = GetField( hx, hy );

    if( cr->IsDead() )
    {
        f.AddDeadCrit( cr );
    }
    else
    {
        if( f.Crit && f.Crit != cr )
        {
            WriteLog( "Hex {} {} busy, critter old {}, new {}.\n", hx, hy, f.Crit->GetId(), cr->GetId() );
            return;
        }

        f.Crit = cr;
        SetMultihex( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), true );
    }

    if( IsHexToDraw( hx, hy ) && cr->Visible )
    {
        Sprite& spr = mainTree.InsertSprite( DRAW_ORDER_CRIT_AUTO( cr ), hx, hy, 0, HEX_OX, HEX_OY, &f.ScrX, &f.ScrY,
                                             0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid );
        spr.SetLight( CORNER_EAST_WEST, hexLight, maxHexX, maxHexY );
        cr->SprDraw = &spr;

        cr->SetSprRect();
        cr->FixLastHexes();

        int contour = 0;
        if( cr->GetId() == critterContourCrId )
            contour = critterContour;
        else if( !cr->IsDead() && !cr->IsChosen() )
            contour = crittersContour;
        spr.SetContour( contour, cr->ContourColor );

        f.AddSpriteToChain( &spr );
    }

    f.ProcessCache();
}

void HexManager::RemoveCritter( CritterCl* cr )
{
    if( !IsMapLoaded() )
        return;

    ushort hx = cr->GetHexX();
    ushort hy = cr->GetHexY();

    Field& f = GetField( hx, hy );
    if( f.Crit == cr )
    {
        f.Crit = nullptr;
        SetMultihex( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), false );
    }
    else
    {
        f.EraseDeadCrit( cr );
    }

    if( cr->IsChosen() || cr->IsHaveLightSources() )
        RebuildLight();
    if( cr->SprDrawValid )
        cr->SprDraw->Unvalidate();
    f.ProcessCache();
}

CritterCl* HexManager::GetCritter( uint crid )
{
    if( !crid )
        return nullptr;
    auto it = allCritters.find( crid );
    return it != allCritters.end() ? it->second : nullptr;
}

CritterCl* HexManager::GetChosen()
{
    if( !chosenId )
        return nullptr;
    auto it = allCritters.find( chosenId );
    return it != allCritters.end() ? it->second : nullptr;
}

void HexManager::AddCritter( CritterCl* cr )
{
    if( allCritters.count( cr->GetId() ) )
        return;
    allCritters.insert( std::make_pair( cr->GetId(), cr ) );
    if( cr->IsChosen() )
        chosenId = cr->GetId();
    SetCritter( cr );
}

void HexManager::DeleteCritter( uint crid )
{
    auto it = allCritters.find( crid );
    if( it == allCritters.end() )
        return;
    CritterCl* cr = it->second;
    if( cr->IsChosen() )
        chosenId = 0;
    RemoveCritter( cr );
    cr->DeleteAllItems();
    cr->IsDestroyed = true;
    Script::RemoveEventsEntity( cr );
    cr->Release();
    allCritters.erase( it );
}

void HexManager::DeleteCritters()
{
    for( auto it = allCritters.begin(), end = allCritters.end(); it != end; ++it )
    {
        CritterCl* cr = it->second;
        RemoveCritter( cr );
        cr->DeleteAllItems();
        cr->IsDestroyed = true;
        Script::RemoveEventsEntity( cr );
        cr->Release();
    }
    allCritters.clear();
    chosenId = 0;
}

void HexManager::GetCritters( ushort hx, ushort hy, CritVec& crits, int find_type )
{
    Field* f = &GetField( hx, hy );
    if( f->Crit && f->Crit->CheckFind( find_type ) )
        crits.push_back( f->Crit );
    if( f->DeadCrits )
    {
        for( auto it = f->DeadCrits->begin(), end = f->DeadCrits->end(); it != end; ++it )
            if( ( *it )->CheckFind( find_type ) )
                crits.push_back( *it );
    }
}

void HexManager::SetCritterContour( uint crid, int contour )
{
    if( critterContourCrId )
    {
        CritterCl* cr = GetCritter( critterContourCrId );
        if( cr && cr->SprDrawValid )
        {
            if( !cr->IsDead() && !cr->IsChosen() )
                cr->SprDraw->SetContour( crittersContour );
            else
                cr->SprDraw->SetContour( 0 );
        }
    }
    critterContourCrId = crid;
    critterContour = contour;
    if( crid )
    {
        CritterCl* cr = GetCritter( crid );
        if( cr && cr->SprDrawValid )
            cr->SprDraw->SetContour( contour );
    }
}

void HexManager::SetCrittersContour( int contour )
{
    if( crittersContour == contour )
        return;

    crittersContour = contour;

    for( auto it = allCritters.begin(), end = allCritters.end(); it != end; it++ )
    {
        CritterCl* cr = it->second;
        if( !cr->IsChosen() && cr->SprDrawValid && !cr->IsDead() && cr->GetId() != critterContourCrId )
            cr->SprDraw->SetContour( contour );
    }
}

bool HexManager::TransitCritter( CritterCl* cr, int hx, int hy, bool animate, bool force )
{
    if( !IsMapLoaded() || hx < 0 || hx >= maxHexX || hy < 0 || hy >= maxHexY )
        return false;
    if( cr->GetHexX() == hx && cr->GetHexY() == hy )
        return true;

    // Dead transit
    if( cr->IsDead() )
    {
        RemoveCritter( cr );
        cr->SetHexX( hx );
        cr->SetHexY( hy );
        SetCritter( cr );

        if( cr->IsChosen() || cr->IsHaveLightSources() )
            RebuildLight();
        return true;
    }

    // Not dead transit
    Field& f = GetField( hx, hy );

    if( f.Crit != nullptr ) // Hex busy
    {
        // Try move critter on busy hex in previous position
        if( force && f.Crit->IsLastHexes() )
            TransitCritter( f.Crit, f.Crit->PopLastHexX(), f.Crit->PopLastHexY(), false, true );
        if( f.Crit != nullptr )
        {
            // Try move in next game cycle
            return false;
        }
    }

    RemoveCritter( cr );

    int old_hx = cr->GetHexX();
    int old_hy = cr->GetHexY();
    cr->SetHexX( hx );
    cr->SetHexY( hy );
    int dir = GetFarDir( old_hx, old_hy, hx, hy );

    if( animate )
    {
        cr->Move( dir );
        if( DistGame( old_hx, old_hy, hx, hy ) > 1 )
        {
            ushort hx_ = hx;
            ushort hy_ = hy;
            MoveHexByDir( hx_, hy_, ReverseDir( dir ), maxHexX, maxHexY );
            int    ox, oy;
            GetHexInterval( hx_, hy_, old_hx, old_hy, ox, oy );
            cr->AddOffsExt( ox, oy );
        }
    }
    else
    {
        int ox, oy;
        GetHexInterval( hx, hy, old_hx, old_hy, ox, oy );
        cr->AddOffsExt( ox, oy );
    }

    SetCritter( cr );
    return true;
}

void HexManager::SetMultihex( ushort hx, ushort hy, uint multihex, bool set )
{
    if( IsMapLoaded() && multihex )
    {
        short* sx, * sy;
        GetHexOffsets( hx & 1, sx, sy );
        for( int i = 0, j = NumericalNumber( multihex ) * DIRS_COUNT; i < j; i++ )
        {
            short cx = (short) hx + sx[ i ];
            short cy = (short) hy + sy[ i ];
            if( cx >= 0 && cy >= 0 && cx < maxHexX && cy < maxHexY )
            {
                Field& neighbor = GetField( cx, cy );
                neighbor.Flags.IsMultihex = set;
                neighbor.ProcessCache();
            }
        }
    }
}

bool HexManager::GetHexPixel( int x, int y, ushort& hx, ushort& hy )
{
    if( !IsMapLoaded() )
        return false;

    float xf = (float) x - (float) GameOpt.ScrOx / GameOpt.SpritesZoom;
    float yf = (float) y - (float) GameOpt.ScrOy / GameOpt.SpritesZoom;
    float ox = (float) HEX_W / GameOpt.SpritesZoom;
    float oy = (float) HEX_REAL_H / GameOpt.SpritesZoom;
    int   y2 = 0;
    int   vpos = 0;

    for( int ty = 0; ty < hVisible; ty++ )
    {
        for( int tx = 0; tx < wVisible; tx++ )
        {
            vpos = y2 + tx;

            float x_ = viewField[ vpos ].ScrXf / GameOpt.SpritesZoom;
            float y_ = viewField[ vpos ].ScrYf / GameOpt.SpritesZoom;

            if( xf >= x_ && xf < x_ + ox && yf >= y_ && yf < y_ + oy )
            {
                int hx_ = viewField[ vpos ].HexX;
                int hy_ = viewField[ vpos ].HexY;

                // Correct with hex color mask
                if( picHexMask )
                {
                    uint r = ( SprMngr.GetPixColor( picHexMask->Ind[ 0 ], (int) ( xf - x_ ), (int) ( yf - y_ ) ) & 0x00FF0000 ) >> 16;
                    if( r == 50 )
                        MoveHexByDirUnsafe( hx_, hy_, GameOpt.MapHexagonal ? 5 : 6 );
                    else if( r == 100 )
                        MoveHexByDirUnsafe( hx_, hy_, GameOpt.MapHexagonal ? 0 : 0 );
                    else if( r == 150 )
                        MoveHexByDirUnsafe( hx_, hy_, GameOpt.MapHexagonal ? 3 : 4 );
                    else if( r == 200 )
                        MoveHexByDirUnsafe( hx_, hy_, GameOpt.MapHexagonal ? 2 : 2 );
                }

                if( hx_ >= 0 && hy_ >= 0 && hx_ < maxHexX && hy_ < maxHexY )
                {
                    hx = hx_;
                    hy = hy_;
                    return true;
                }
            }
        }
        y2 += wVisible;
    }

    return false;
}

ItemHex* HexManager::GetItemPixel( int x, int y, bool& item_egg )
{
    if( !IsMapLoaded() )
        return nullptr;

    ItemHexVec pix_item;
    ItemHexVec pix_item_egg;
    bool       is_egg = SprMngr.IsEggTransp( x, y );

    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        ItemHex* item = *it;
        ushort   hx = item->GetHexX();
        ushort   hy = item->GetHexY();

        if( item->IsFinishing() || !item->SprDrawValid )
            continue;

        #ifdef FONLINE_CLIENT
        if( item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent() )
            continue;
        if( item->IsScenery() && !GameOpt.ShowScen )
            continue;
        if( !item->IsAnyScenery() && !GameOpt.ShowItem )
            continue;
        if( item->IsWall() && !GameOpt.ShowWall )
            continue;
        #else // FONLINE_MAPPER
        bool is_fast = fastPids.count( item->GetProtoId() ) != 0;
        if( item->IsScenery() && !GameOpt.ShowScen && !is_fast )
            continue;
        if( !item->IsAnyScenery() && !GameOpt.ShowItem && !is_fast )
            continue;
        if( item->IsWall() && !GameOpt.ShowWall && !is_fast )
            continue;
        if( !GameOpt.ShowFast && is_fast )
            continue;
        if( ignorePids.count( item->GetProtoId() ) )
            continue;
        #endif

        SpriteInfo* si = SprMngr.GetSpriteInfo( item->SprId );
        if( !si )
            continue;

        int l = (int) ( ( *item->HexScrX + item->ScrX + si->OffsX + HEX_OX + GameOpt.ScrOx - si->Width / 2 ) / GameOpt.SpritesZoom );
        int r = (int) ( ( *item->HexScrX + item->ScrX + si->OffsX + HEX_OX + GameOpt.ScrOx + si->Width / 2 ) / GameOpt.SpritesZoom );
        int t = (int) ( ( *item->HexScrY + item->ScrY + si->OffsY + HEX_OY + GameOpt.ScrOy - si->Height ) / GameOpt.SpritesZoom );
        int b = (int) ( ( *item->HexScrY + item->ScrY + si->OffsY + HEX_OY + GameOpt.ScrOy ) / GameOpt.SpritesZoom );

        if( x >= l && x <= r && y >= t && y <= b )
        {
            Sprite* spr = item->SprDraw->GetIntersected( x - l, y - t );
            if( spr )
            {
                item->SprTemp = spr;
                if( is_egg && SprMngr.CompareHexEgg( hx, hy, item->GetEggType() ) )
                    pix_item_egg.push_back( item );
                else
                    pix_item.push_back( item );
            }
        }
    }

    // Sorters
    struct Sorter
    {
        static bool ByTreeIndex( ItemHex* o1, ItemHex* o2 )   { return o1->SprTemp->TreeIndex > o2->SprTemp->TreeIndex; }
        static bool ByTransparent( ItemHex* o1, ItemHex* o2 ) { return !o1->IsTransparent() && o2->IsTransparent(); }
    };

    // Egg items
    if( pix_item.empty() )
    {
        if( pix_item_egg.empty() )
            return nullptr;
        if( pix_item_egg.size() > 1 )
        {
            std::sort( pix_item_egg.begin(), pix_item_egg.end(), Sorter::ByTreeIndex );
            std::sort( pix_item_egg.begin(), pix_item_egg.end(), Sorter::ByTransparent );
        }
        item_egg = true;
        return pix_item_egg[ 0 ];
    }

    // Visible items
    if( pix_item.size() > 1 )
    {
        std::sort( pix_item.begin(), pix_item.end(), Sorter::ByTreeIndex );
        std::sort( pix_item.begin(), pix_item.end(), Sorter::ByTransparent );
    }
    item_egg = false;
    return pix_item[ 0 ];
}

CritterCl* HexManager::GetCritterPixel( int x, int y, bool ignore_dead_and_chosen )
{
    if( !IsMapLoaded() || !GameOpt.ShowCrit )
        return nullptr;

    CritVec crits;
    for( auto it = allCritters.begin(); it != allCritters.end(); it++ )
    {
        CritterCl* cr = it->second;
        if( !cr->Visible || cr->IsFinishing() || !cr->SprDrawValid )
            continue;
        if( ignore_dead_and_chosen && ( cr->IsDead() || cr->IsChosen() ) )
            continue;

        if( x >= ( cr->DRect.L + GameOpt.ScrOx ) / GameOpt.SpritesZoom && x <= ( cr->DRect.R + GameOpt.ScrOx ) / GameOpt.SpritesZoom &&
            y >= ( cr->DRect.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom && y <= ( cr->DRect.B + GameOpt.ScrOy ) / GameOpt.SpritesZoom &&
            SprMngr.IsPixNoTransp( cr->SprId, (int) ( x - ( cr->DRect.L + GameOpt.ScrOx ) / GameOpt.SpritesZoom ), (int) ( y - ( cr->DRect.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom ) ) )
        {
            crits.push_back( cr );
        }
    }

    if( crits.empty() )
        return nullptr;
    struct Sorter
    {
        static bool ByTreeIndex( CritterCl* cr1, CritterCl* cr2 ) { return cr1->SprDraw->TreeIndex > cr2->SprDraw->TreeIndex; } };
    if( crits.size() > 1 )
        std::sort( crits.begin(), crits.end(), Sorter::ByTreeIndex );
    return crits[ 0 ];
}

void HexManager::GetSmthPixel( int x, int y, ItemHex*& item, CritterCl*& cr )
{
    bool item_egg;
    item = GetItemPixel( x, y, item_egg );
    cr = GetCritterPixel( x, y, false );

    if( cr && item )
    {
        if( item->IsTransparent() || item_egg || item->SprDraw->TreeIndex <= cr->SprDraw->TreeIndex )
            item = nullptr;
        else
            cr = nullptr;
    }
}

bool HexManager::FindPath( CritterCl* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, UCharVec& steps, int cut )
{
    // Static data
    #define GRID( x, y )    grid[ ( ( MAX_FIND_PATH + 1 ) + ( y ) - grid_oy ) * ( MAX_FIND_PATH * 2 + 2 ) + ( ( MAX_FIND_PATH + 1 ) + ( x ) - grid_ox ) ]
    static int           grid_ox = 0, grid_oy = 0;
    static short*        grid = nullptr;
    static UShortPairVec coords;

    // Allocate temporary grid
    if( !grid )
    {
        grid = new short[ ( MAX_FIND_PATH * 2 + 2 ) * ( MAX_FIND_PATH * 2 + 2 ) ];
        if( !grid )
            return false;
    }

    if( !IsMapLoaded() )
        return false;
    if( start_x == end_x && start_y == end_y )
        return true;

    short numindex = 1;
    memzero( grid, ( MAX_FIND_PATH * 2 + 2 ) * ( MAX_FIND_PATH * 2 + 2 ) * sizeof( short ) );
    grid_ox = start_x;
    grid_oy = start_y;
    GRID( start_x, start_y ) = numindex;
    coords.clear();
    coords.push_back( std::make_pair( start_x, start_y ) );

    uint mh = ( cr ? cr->GetMultihex() : 0 );
    int  p = 0;
    while( true )
    {
        if( ++numindex > MAX_FIND_PATH )
            return false;

        int p_togo = (int) coords.size() - p;
        if( !p_togo )
            return false;

        for( int i = 0; i < p_togo; ++i, ++p )
        {
            int    hx = coords[ p ].first;
            int    hy = coords[ p ].second;

            short* sx, * sy;
            GetHexOffsets( hx & 1, sx, sy );

            for( int j = 0, jj = DIRS_COUNT; j < jj; j++ )
            {
                int nx = hx + sx[ j ];
                int ny = hy + sy[ j ];
                if( nx < 0 || ny < 0 || nx >= maxHexX || ny >= maxHexY || GRID( nx, ny ) )
                    continue;
                GRID( nx, ny ) = -1;

                if( !mh )
                {
                    if( GetField( nx, ny ).Flags.IsNotPassed )
                        continue;
                }
                else
                {
                    // Base hex
                    int nx_ = nx, ny_ = ny;
                    for( uint k = 0; k < mh; k++ )
                        MoveHexByDirUnsafe( nx_, ny_, j );
                    if( nx_ < 0 || ny_ < 0 || nx_ >= maxHexX || ny_ >= maxHexY )
                        continue;
                    if( GetField( nx_, ny_ ).Flags.IsNotPassed )
                        continue;

                    // Clock wise hexes
                    bool is_square_corner = ( !GameOpt.MapHexagonal && IS_DIR_CORNER( j ) );
                    uint steps_count = ( is_square_corner ? mh * 2 : mh );
                    bool not_passed = false;
                    int  dir_ = ( GameOpt.MapHexagonal ? ( ( j + 2 ) % 6 ) : ( ( j + 2 ) % 8 ) );
                    if( is_square_corner )
                        dir_ = ( dir_ + 1 ) % 8;
                    int nx__ = nx_, ny__ = ny_;
                    for( uint k = 0; k < steps_count && !not_passed; k++ )
                    {
                        MoveHexByDirUnsafe( nx__, ny__, dir_ );
                        not_passed = GetField( nx__, ny__ ).Flags.IsNotPassed;
                    }
                    if( not_passed )
                        continue;

                    // Counter clock wise hexes
                    dir_ = ( GameOpt.MapHexagonal ? ( ( j + 4 ) % 6 ) : ( ( j + 6 ) % 8 ) );
                    if( is_square_corner )
                        dir_ = ( dir_ + 7 ) % 8;
                    nx__ = nx_, ny__ = ny_;
                    for( uint k = 0; k < steps_count && !not_passed; k++ )
                    {
                        MoveHexByDirUnsafe( nx__, ny__, dir_ );
                        not_passed = GetField( nx__, ny__ ).Flags.IsNotPassed;
                    }
                    if( not_passed )
                        continue;
                }

                GRID( nx, ny ) = numindex;
                coords.push_back( std::make_pair( nx, ny ) );

                if( cut >= 0 && CheckDist( nx, ny, end_x, end_y, cut ) )
                {
                    end_x = nx;
                    end_y = ny;
                    return true;
                }

                if( cut < 0 && nx == end_x && ny == end_y )
                    goto label_FindOk;
            }
        }
    }
    if( cut >= 0 )
        return false;

label_FindOk:
    int x1 = coords.back().first;
    int y1 = coords.back().second;
    steps.resize( numindex - 1 );

    // From end
    if( GameOpt.MapHexagonal )
    {
        static bool switcher = false;
        if( !GameOpt.MapSmoothPath )
            switcher = false;

        while( numindex > 1 )
        {
            if( GameOpt.MapSmoothPath && numindex & 1 )
                switcher = !switcher;
            numindex--;

            if( switcher )
            {
                if( x1 & 1 )
                {
                    if( GRID( x1 - 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 3;
                        x1--;
                        y1--;
                        continue;
                    }                                                                                                // 0
                    if( GRID( x1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 2;
                        y1--;
                        continue;
                    }                                                                                                // 5
                    if( GRID( x1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 5;
                        y1++;
                        continue;
                    }                                                                                                // 2
                    if( GRID( x1 + 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 0;
                        x1++;
                        continue;
                    }                                                                                                // 3
                    if( GRID( x1 - 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 4;
                        x1--;
                        continue;
                    }                                                                                                // 1
                    if( GRID( x1 + 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 1;
                        x1++;
                        y1--;
                        continue;
                    }                                                                                                // 4
                }
                else
                {
                    if( GRID( x1 - 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 3;
                        x1--;
                        continue;
                    }                                                                                                // 0
                    if( GRID( x1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 2;
                        y1--;
                        continue;
                    }                                                                                                // 5
                    if( GRID( x1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 5;
                        y1++;
                        continue;
                    }                                                                                                // 2
                    if( GRID( x1 + 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 0;
                        x1++;
                        y1++;
                        continue;
                    }                                                                                                // 3
                    if( GRID( x1 - 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 4;
                        x1--;
                        y1++;
                        continue;
                    }                                                                                                // 1
                    if( GRID( x1 + 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 1;
                        x1++;
                        continue;
                    }                                                                                                // 4
                }
            }
            else
            {
                if( x1 & 1 )
                {
                    if( GRID( x1 - 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 4;
                        x1--;
                        continue;
                    }                                                                                                // 1
                    if( GRID( x1 + 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 1;
                        x1++;
                        y1--;
                        continue;
                    }                                                                                                // 4
                    if( GRID( x1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 2;
                        y1--;
                        continue;
                    }                                                                                                // 5
                    if( GRID( x1 - 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 3;
                        x1--;
                        y1--;
                        continue;
                    }                                                                                                // 0
                    if( GRID( x1 + 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 0;
                        x1++;
                        continue;
                    }                                                                                                // 3
                    if( GRID( x1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 5;
                        y1++;
                        continue;
                    }                                                                                                // 2
                }
                else
                {
                    if( GRID( x1 - 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 4;
                        x1--;
                        y1++;
                        continue;
                    }                                                                                                // 1
                    if( GRID( x1 + 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 1;
                        x1++;
                        continue;
                    }                                                                                                // 4
                    if( GRID( x1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 2;
                        y1--;
                        continue;
                    }                                                                                                // 5
                    if( GRID( x1 - 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 3;
                        x1--;
                        continue;
                    }                                                                                                // 0
                    if( GRID( x1 + 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 0;
                        x1++;
                        y1++;
                        continue;
                    }                                                                                                // 3
                    if( GRID( x1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 5;
                        y1++;
                        continue;
                    }                                                                                                // 2
                }
            }
            return false;
        }
    }
    else
    {
        // Smooth data
        int switch_count = 0, switch_begin = 0;
        if( GameOpt.MapSmoothPath )
        {
            int x2 = start_x, y2 = start_y;
            int dx = abs( x1 - x2 );
            int dy = abs( y1 - y2 );
            int d = MAX( dx, dy );
            int h1 = abs( dx - dy );
            int h2 = d - h1;
            switch_count = ( ( h1 && h2 ) ? MAX( h1, h2 ) / MIN( h1, h2 ) + 1 : 0 );
            if( h1 && h2 && switch_count < 2 )
                switch_count = 2;
            switch_begin = ( ( h1 && h2 ) ? MIN( h1, h2 ) % MAX( h1, h2 ) : 0 );
        }

        for( int i = switch_begin; numindex > 1; i++ )
        {
            numindex--;

            // Without smoothing
            if( !GameOpt.MapSmoothPath )
            {
                if( GRID( x1 - 1, y1  ) == numindex )
                {
                    steps[ numindex - 1 ] = 4;
                    x1--;
                    continue;
                }                                                                                            // 0
                if( GRID( x1, y1 - 1 ) == numindex )
                {
                    steps[ numindex - 1 ] = 2;
                    y1--;
                    continue;
                }                                                                                            // 6
                if( GRID( x1, y1 + 1 ) == numindex )
                {
                    steps[ numindex - 1 ] = 6;
                    y1++;
                    continue;
                }                                                                                            // 2
                if( GRID( x1 + 1, y1  ) == numindex )
                {
                    steps[ numindex - 1 ] = 0;
                    x1++;
                    continue;
                }                                                                                            // 4
                if( GRID( x1 - 1, y1 + 1 ) == numindex )
                {
                    steps[ numindex - 1 ] = 5;
                    x1--;
                    y1++;
                    continue;
                }                                                                                            // 1
                if( GRID( x1 + 1, y1 - 1 ) == numindex )
                {
                    steps[ numindex - 1 ] = 1;
                    x1++;
                    y1--;
                    continue;
                }                                                                                            // 5
                if( GRID( x1 + 1, y1 + 1 ) == numindex )
                {
                    steps[ numindex - 1 ] = 7;
                    x1++;
                    y1++;
                    continue;
                }                                                                                            // 3
                if( GRID( x1 - 1, y1 - 1 ) == numindex )
                {
                    steps[ numindex - 1 ] = 3;
                    x1--;
                    y1--;
                    continue;
                }                                                                                            // 7
            }
            // With smoothing
            else
            {
                if( switch_count < 2 || i % switch_count )
                {
                    if( GRID( x1 - 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 4;
                        x1--;
                        continue;
                    }                                                                                                // 0
                    if( GRID( x1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 6;
                        y1++;
                        continue;
                    }                                                                                                // 2
                    if( GRID( x1 + 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 0;
                        x1++;
                        continue;
                    }                                                                                                // 4
                    if( GRID( x1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 2;
                        y1--;
                        continue;
                    }                                                                                                // 6
                    if( GRID( x1 + 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 7;
                        x1++;
                        y1++;
                        continue;
                    }                                                                                                // 3
                    if( GRID( x1 - 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 3;
                        x1--;
                        y1--;
                        continue;
                    }                                                                                                // 7
                    if( GRID( x1 - 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 5;
                        x1--;
                        y1++;
                        continue;
                    }                                                                                                // 1
                    if( GRID( x1 + 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 1;
                        x1++;
                        y1--;
                        continue;
                    }                                                                                                // 5
                }
                else
                {
                    if( GRID( x1 + 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 7;
                        x1++;
                        y1++;
                        continue;
                    }                                                                                                // 3
                    if( GRID( x1 - 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 3;
                        x1--;
                        y1--;
                        continue;
                    }                                                                                                // 7
                    if( GRID( x1 - 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 4;
                        x1--;
                        continue;
                    }                                                                                                // 0
                    if( GRID( x1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 6;
                        y1++;
                        continue;
                    }                                                                                                // 2
                    if( GRID( x1 + 1, y1  ) == numindex )
                    {
                        steps[ numindex - 1 ] = 0;
                        x1++;
                        continue;
                    }                                                                                                // 4
                    if( GRID( x1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 2;
                        y1--;
                        continue;
                    }                                                                                                // 6
                    if( GRID( x1 - 1, y1 + 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 5;
                        x1--;
                        y1++;
                        continue;
                    }                                                                                                // 1
                    if( GRID( x1 + 1, y1 - 1 ) == numindex )
                    {
                        steps[ numindex - 1 ] = 1;
                        x1++;
                        y1--;
                        continue;
                    }                                                                                                // 5
                }
            }
            return false;
        }
    }
    return true;
}

bool HexManager::CutPath( CritterCl* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut )
{
    static UCharVec dummy;
    return FindPath( cr, start_x, start_y, end_x, end_y, dummy, cut );
}

bool HexManager::TraceBullet( ushort hx, ushort hy, ushort tx, ushort ty, uint dist, float angle, CritterCl* find_cr, bool find_cr_safe,
                              CritVec* critters, int find_type, UShortPair* pre_block, UShortPair* block, UShortPairVec* steps, bool check_passed )
{
    if( IsShowTrack() )
        ClearHexTrack();

    if( !dist )
        dist = DistGame( hx, hy, tx, ty );

    ushort     cx = hx;
    ushort     cy = hy;
    ushort     old_cx = cx;
    ushort     old_cy = cy;

    LineTracer line_tracer( hx, hy, tx, ty, maxHexX, maxHexY, angle, !GameOpt.MapHexagonal );

    for( uint i = 0; i < dist; i++ )
    {
        if( GameOpt.MapHexagonal )
            line_tracer.GetNextHex( cx, cy );
        else
            line_tracer.GetNextSquare( cx, cy );

        if( IsShowTrack() )
            GetHexTrack( cx, cy ) = ( cx == tx && cy == ty ? 1 : 2 );

        if( steps )
        {
            steps->push_back( std::make_pair( cx, cy ) );
            continue;
        }

        Field& f = GetField( cx, cy );
        if( check_passed && f.Flags.IsNotRaked )
            break;
        if( critters != nullptr )
            GetCritters( cx, cy, *critters, find_type );
        if( find_cr != nullptr && f.Crit )
        {
            CritterCl* cr = f.Crit;
            if( cr && cr == find_cr )
                return true;
            if( find_cr_safe )
                return false;
        }

        old_cx = cx;
        old_cy = cy;
    }

    if( pre_block )
    {
        ( *pre_block ).first = old_cx;
        ( *pre_block ).second = old_cy;
    }
    if( block )
    {
        ( *block ).first = cx;
        ( *block ).second = cy;
    }
    return false;
}

void HexManager::FindSetCenter( int cx, int cy )
{
    RUNTIME_ASSERT( viewField );

    RebuildMap( cx, cy );

    #ifdef FONLINE_CLIENT
    int    iw = VIEW_WIDTH / 2 + 2;
    int    ih = VIEW_HEIGHT / 2 + 2;
    ushort hx = cx;
    ushort hy = cy;
    int    dirs[ 2 ];

    // Up
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 0, dirs[ 1 ] = 5;
    else
        dirs[ 0 ] = 0, dirs[ 1 ] = 6;
    FindSetCenterDir( hx, hy, dirs, ih );
    // Down
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 3, dirs[ 1 ] = 2;
    else
        dirs[ 0 ] = 4, dirs[ 1 ] = 2;
    FindSetCenterDir( hx, hy, dirs, ih );
    // Right
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 1, dirs[ 1 ] = -1;
    else
        dirs[ 0 ] = 1, dirs[ 1 ] = -1;
    FindSetCenterDir( hx, hy, dirs, iw );
    // Left
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 4, dirs[ 1 ] = -1;
    else
        dirs[ 0 ] = 5, dirs[ 1 ] = -1;
    FindSetCenterDir( hx, hy, dirs, iw );

    // Up-Right
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 0, dirs[ 1 ] = -1;
    else
        dirs[ 0 ] = 0, dirs[ 1 ] = -1;
    FindSetCenterDir( hx, hy, dirs, ih );
    // Down-Left
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 3, dirs[ 1 ] = -1;
    else
        dirs[ 0 ] = 4, dirs[ 1 ] = -1;
    FindSetCenterDir( hx, hy, dirs, ih );
    // Up-Left
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 5, dirs[ 1 ] = -1;
    else
        dirs[ 0 ] = 6, dirs[ 1 ] = -1;
    FindSetCenterDir( hx, hy, dirs, ih );
    // Down-Right
    if( GameOpt.MapHexagonal )
        dirs[ 0 ] = 2, dirs[ 1 ] = -1;
    else
        dirs[ 0 ] = 2, dirs[ 1 ] = -1;
    FindSetCenterDir( hx, hy, dirs, ih );

    RebuildMap( hx, hy );
    #endif // FONLINE_CLIENT
}

void HexManager::FindSetCenterDir( ushort& hx, ushort& hy, int dirs[ 2 ], int steps )
{
    ushort sx = hx;
    ushort sy = hy;
    int    dirs_count = ( dirs[ 1 ] == -1 ? 1 : 2 );

    int    i;
    for( i = 0; i < steps; i++ )
    {
        if( !MoveHexByDir( sx, sy, dirs[ i % dirs_count ], maxHexX, maxHexY ) )
            break;

        GetHexTrack( sx, sy ) = 1;
        if( GetField( sx, sy ).Flags.ScrollBlock )
            break;
        GetHexTrack( sx, sy ) = 2;
    }

    for( ; i < steps; i++ )
        MoveHexByDir( hx, hy, ReverseDir( dirs[ i % dirs_count ] ), maxHexX, maxHexY );
}

bool HexManager::LoadMap( hash map_pid )
{
    WriteLog( "Load map...\n" );

    // Unload previous
    UnloadMap();

    // Check data sprites reloading
    if( curDataPrefix != GameOpt.MapDataPrefix )
        ReloadSprites();

    // Make name
    string map_name = _str( "{}.map", map_pid );

    // Find in cache
    uint   cache_len;
    uchar* cache = Crypt.GetCache( map_name, cache_len );
    if( !cache )
    {
        WriteLog( "Load map '{}' from cache fail.\n", map_name );
        return false;
    }

    FileManager fm;
    if( !fm.LoadStream( cache, cache_len ) )
    {
        WriteLog( "Load map '{}' from stream fail.\n", map_name );
        delete[] cache;
        return false;
    }
    delete[] cache;

    // Header
    uint   buf_len = fm.GetFsize();
    uchar* buf = Crypt.Uncompress( fm.GetBuf(), buf_len, 50 );
    if( !buf )
    {
        WriteLog( "Uncompress map fail.\n" );
        return false;
    }

    fm.UnloadFile();
    fm.LoadStream( buf, buf_len );
    delete[] buf;

    if( fm.GetBEUInt() != CLIENT_MAP_FORMAT_VER )
    {
        WriteLog( "Map format is deprecated.\n" );
        return false;
    }

    if( fm.GetBEUInt() != map_pid )
    {
        WriteLog( "Data truncated.\n" );
        return false;
    }

    // Data
    ushort maxhx = fm.GetBEUShort();
    ushort maxhy = fm.GetBEUShort();
    uint   tiles_len = fm.GetBEUInt();
    uint   scen_len = fm.GetBEUInt();

    // Create field
    ResizeField( maxhx, maxhy );

    // Tiles
    curHashTiles = ( tiles_len ? Crypt.MurmurHash2( fm.GetCurBuf(), tiles_len ) : maxhx * maxhy );
    for( uint i = 0; i < tiles_len / sizeof( ProtoMap::Tile ); i++ )
    {
        ProtoMap::Tile tile;
        if( !fm.CopyMem( &tile, sizeof( tile ) ) )
        {
            WriteLog( "Unable to read tile.\n" );
            continue;
        }
        if( tile.HexX >= maxHexX || tile.HexY >= maxHexY )
            continue;

        Field&     f = GetField( tile.HexX, tile.HexY );
        AnyFrames* anim = ResMngr.GetItemAnim( tile.Name );
        if( anim )
        {
            Field::Tile& ftile = f.AddTile( anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof );
            CheckTilesBorder( ftile, tile.IsRoof );
        }
    }

    // Roof indexes
    int roof_num = 1;
    for( int hx = 0; hx < maxHexX; hx++ )
    {
        for( int hy = 0; hy < maxHexY; hy++ )
        {
            if( GetField( hx, hy ).GetTilesCount( true ) )
            {
                MarkRoofNum( hx, hy, roof_num );
                roof_num++;
            }
        }
    }

    // Scenery
    curHashScen = ( scen_len ? Crypt.MurmurHash2( fm.GetCurBuf(), scen_len ) : maxhx * maxhy );
    uint scen_count = fm.GetLEUInt();
    for( uint i = 0; i < scen_count; i++ )
    {
        uint        id = fm.GetLEUInt();
        hash        proto_id = fm.GetLEUInt();
        uint        datas_size = fm.GetLEUInt();
        UCharVecVec props_data( datas_size );
        for( uint i = 0; i < datas_size; i++ )
        {
            uint data_size = fm.GetLEUInt();
            if( data_size )
            {
                props_data[ i ].resize( data_size );
                fm.CopyMem( &props_data[ i ][ 0 ], data_size );
            }
        }
        Properties props( Item::PropertiesRegistrator );
        props.RestoreData( props_data );

        GenerateItem( id, proto_id, props );
    }

    // Scroll blocks borders
    for( int hx = 0; hx < maxHexX; hx++ )
    {
        for( int hy = 0; hy < maxHexY; hy++ )
        {
            Field& f = GetField( hx, hy );
            if( f.Flags.ScrollBlock )
            {
                for( int i = 0; i < 6; i++ )
                {
                    ushort hx_ = hx, hy_ = hy;
                    MoveHexByDir( hx_, hy_, i, maxHexX, maxHexY );
                    GetField( hx_, hy_ ).Flags.IsNotPassed = true;
                }
            }
        }
    }

    // Visible
    ResizeView();

    // Light
    CollectLightSources();
    lightPoints.clear();
    lightPointsCount = 0;
    RealRebuildLight();
    requestRebuildLight = false;
    requestRenderLight = true;

    // Finish
    curPidMap = map_pid;
    curMapTime = -1;
    AutoScroll.Active = false;
    WriteLog( "Load map success.\n" );

    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( ClientFunctions.MapLoad );
    #endif

    return true;
}

void HexManager::UnloadMap()
{
    if( !IsMapLoaded() )
        return;

    WriteLog( "Unload map.\n" );

    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( ClientFunctions.MapUnload );
    #endif

    curPidMap = 0;
    curMapTime = -1;
    curHashTiles = 0;
    curHashScen = 0;

    crittersContour = 0;
    critterContour = 0;

    SAFEDELA( viewField );

    hTop = 0;
    hBottom = 0;
    wLeft = 0;
    wRight = 0;
    hVisible = 0;
    wVisible = 0;
    screenHexX = 0;
    screenHexY = 0;

    lightSources.clear();
    lightSourcesScen.clear();

    fogLookPoints.clear();
    fogShootPoints.clear();
    fogForceRerender = true;

    mainTree.Unvalidate();
    roofTree.Unvalidate();
    tilesTree.Unvalidate();
    roofRainTree.Unvalidate();

    for( auto it = rainData.begin(), end = rainData.end(); it != end; ++it )
        delete *it;
    rainData.clear();

    for( uint i = 0, j = (uint) hexItems.size(); i < j; i++ )
        hexItems[ i ]->Release();
    hexItems.clear();

    ResizeField( 0, 0 );
    DeleteCritters();

    #ifdef FONLINE_MAPPER
    TilesField.clear();
    RoofsField.clear();
    #endif
}

void HexManager::GetMapHash( hash map_pid, hash& hash_tiles, hash& hash_scen )
{
    WriteLog( "Get map info..." );

    hash_tiles = 0;
    hash_scen = 0;

    if( map_pid == curPidMap )
    {
        hash_tiles = curHashTiles;
        hash_scen = curHashScen;

        WriteLog( "Hashes of loaded map: tiles {}, scenery {}.\n", hash_tiles, hash_scen );
        return;
    }

    string map_name = _str( "{}.map", map_pid );

    uint   cache_len;
    uchar* cache = Crypt.GetCache( map_name, cache_len );
    if( !cache )
    {
        WriteLog( "Load map '{}' from cache fail.\n", map_name );
        return;
    }

    FileManager fm;
    if( !fm.LoadStream( cache, cache_len ) )
    {
        WriteLog( "Load map '{}' from stream fail.\n", map_name );
        delete[] cache;
        return;
    }
    delete[] cache;

    uint   buf_len = fm.GetFsize();
    uchar* buf = Crypt.Uncompress( fm.GetBuf(), buf_len, 50 );
    if( !buf )
    {
        WriteLog( "Uncompress map fail.\n" );
        return;
    }

    fm.LoadStream( buf, buf_len );
    delete[] buf;

    if( fm.GetBEUInt() != CLIENT_MAP_FORMAT_VER )
    {
        WriteLog( "Map format is not supported.\n" );
        return;
    }

    if( fm.GetBEUInt() != map_pid )
    {
        WriteLog( "Invalid proto number.\n" );
        return;
    }

    // Data
    ushort maxhx = fm.GetBEUShort();
    ushort maxhy = fm.GetBEUShort();
    uint   tiles_len = fm.GetBEUInt();
    uint   scen_len = fm.GetBEUInt();

    hash_tiles = ( tiles_len ? Crypt.MurmurHash2( fm.GetCurBuf(), tiles_len ) : maxhx * maxhy );
    fm.GoForward( tiles_len );
    hash_scen = ( scen_len ? Crypt.MurmurHash2( fm.GetCurBuf(), scen_len ) : maxhx * maxhy );

    WriteLog( "complete.\n" );
}

void HexManager::GenerateItem( uint id, hash proto_id, Properties& props )
{
    ProtoItem* proto = ProtoMngr.GetProtoItem( proto_id );
    RUNTIME_ASSERT( proto );

    ItemHex* scenery = new ItemHex( id, proto, props );
    Field&   f = GetField( scenery->GetHexX(), scenery->GetHexY() );
    scenery->HexScrX = &f.ScrX;
    scenery->HexScrY = &f.ScrY;

    if( scenery->GetIsLight() )
    {
        lightSourcesScen.push_back( LightSource( scenery->GetHexX(), scenery->GetHexY(), scenery->LightGetColor(),
                                                 scenery->LightGetDistance(), scenery->LightGetIntensity(), scenery->LightGetFlags() ) );
    }

    PushItem( scenery );

    if( !scenery->GetIsHidden() && !scenery->GetIsHiddenPicture() && !scenery->IsFullyTransparent() )
        ProcessHexBorders( scenery->Anim->GetSprId( 0 ), scenery->GetOffsetX(), scenery->GetOffsetY(), false );
}

int HexManager::GetDayTime()
{
    return Globals->GetHour() * 60 + Globals->GetMinute();
}

int HexManager::GetMapTime()
{
    if( curMapTime < 0 )
        return GetDayTime();
    return curMapTime;
}

int* HexManager::GetMapDayTime()
{
    return dayTime;
}

uchar* HexManager::GetMapDayColor()
{
    return dayColor;
}

void HexManager::OnResolutionChanged()
{
    fogForceRerender = true;
    ResizeView();
    RefreshMap();
}

#ifdef FONLINE_MAPPER
bool HexManager::SetProtoMap( ProtoMap& pmap )
{
    WriteLog( "Create map from prototype.\n" );

    UnloadMap();

    if( curDataPrefix != GameOpt.MapDataPrefix )
        ReloadSprites();

    ResizeField( pmap.GetWidth(), pmap.GetHeight() );

    curPidMap = 1;

    int day_time = pmap.GetCurDayTime();
    Globals->SetMinute( day_time % 60 );
    Globals->SetHour( day_time / 60 % 24 );
    uint color = GetColorDay( GetMapDayTime(), GetMapDayColor(), GetMapTime(), nullptr );
    SprMngr.SetSpritesColor( COLOR_GAME_RGB( ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF ) );

    CScriptArray* dt = pmap.GetDayTime();
    CScriptArray* dc = pmap.GetDayColor();
    for( int i = 0; i < 4; i++ )
        dayTime[ i ] = *(int*) dt->At( i );
    for( int i = 0; i < 12; i++ )
        dayColor[ i ] = *(uchar*) dc->At( i );
    dt->Release();
    dc->Release();

    // Tiles
    ushort width = pmap.GetWidth();
    ushort height = pmap.GetHeight();
    TilesField.resize( width * height );
    RoofsField.resize( width * height );
    for( auto& tile : pmap.Tiles )
    {
        if( !tile.IsRoof )
        {
            TilesField[ tile.HexY * width + tile.HexX ].push_back( tile );
            TilesField[ tile.HexY * width + tile.HexX ].back().IsSelected = false;
        }
        else
        {
            RoofsField[ tile.HexY * width + tile.HexX ].push_back( tile );
            RoofsField[ tile.HexY * width + tile.HexX ].back().IsSelected = false;
        }
    }
    for( ushort hy = 0; hy < height; hy++ )
    {
        for( ushort hx = 0; hx < width; hx++ )
        {
            Field& f = GetField( hx, hy );

            for( int r = 0; r <= 1; r++ )     // Tile/roof
            {
                ProtoMap::TileVec& tiles = GetTiles( hx, hy, r != 0 );

                for( uint i = 0, j = (uint) tiles.size(); i < j; i++ )
                {
                    ProtoMap::Tile& tile = tiles[ i ];
                    AnyFrames*      anim = ResMngr.GetItemAnim( tile.Name );
                    if( anim )
                    {
                        Field::Tile& ftile = f.AddTile( anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof );
                        CheckTilesBorder( ftile, tile.IsRoof );
                    }
                }
            }
        }
    }

    // Entities
    for( auto& entity : pmap.AllEntities )
    {
        if( entity->Type == EntityType::Item )
        {
            Item* entity_item = (Item*) entity;
            if( entity_item->GetAccessory() == ITEM_ACCESSORY_HEX )
            {
                GenerateItem( entity_item->Id, entity_item->GetProtoId(), entity_item->Props );
            }
            else if( entity_item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
            {
                CritterCl* cr = GetCritter( entity_item->GetCritId() );
                if( cr )
                    cr->AddItem( entity_item->Clone() );
            }
            else if( entity_item->GetAccessory() == ITEM_ACCESSORY_CONTAINER )
            {
                Item* cont = GetItemById( entity_item->GetContainerId() );
                if( cont )
                    cont->ContSetItem( entity_item->Clone() );
            }
            else
            {
                RUNTIME_ASSERT( !"Unreachable place" );
            }
        }
        else if( entity->Type == EntityType::CritterCl )
        {
            CritterCl* entity_cr = (CritterCl*) entity;
            CritterCl* cr = new CritterCl( entity_cr->Id, (ProtoCritter*) entity_cr->Proto );
            cr->Props = entity_cr->Props;
            cr->Init();
            AddCritter( cr );
        }
        else
        {
            RUNTIME_ASSERT( !"Unreachable place" );
        }
    }

    ResizeView();

    curHashTiles = hash( -1 );
    curHashScen = hash( -1 );
    WriteLog( "Create map from prototype complete.\n" );
    return true;
}

void HexManager::GetProtoMap( ProtoMap& pmap )
{
    pmap.SetWorkHexX( screenHexX );
    pmap.SetWorkHexY( screenHexY );
    pmap.SetCurDayTime( GetDayTime() );

    // Fill entities
    for( auto& entity : pmap.AllEntities )
        entity->Release();
    pmap.AllEntities.clear();
    std::function< void(Entity*) > fill_recursively = [ &fill_recursively, &pmap ] ( Entity * entity )
    {
        Entity* store_entity = nullptr;
        if( entity->Type == EntityType::ItemHex || entity->Type == EntityType::Item )
            store_entity = new Item( entity->Id, (ProtoItem*) entity->Proto );
        else if( entity->Type == EntityType::CritterCl )
            store_entity = new CritterCl( entity->Id, (ProtoCritter*) entity->Proto );
        else
            RUNTIME_ASSERT( !"Unreachable place" );
        store_entity->Props = entity->Props;
        pmap.AllEntities.push_back( store_entity );
        for( auto& child : entity->GetChildren() )
        {
            RUNTIME_ASSERT( child->Type == EntityType::Item );
            fill_recursively( child );
        }
    };
    for( auto& kv : allCritters )
        fill_recursively( kv.second );
    for( auto& item : hexItems )
        fill_recursively( item );

    // Fill tiles
    pmap.Tiles.clear();
    ushort width = GetWidth();
    ushort height = GetHeight();
    TilesField.resize( width * height );
    RoofsField.resize( width * height );
    for( ushort hy = 0; hy < height; hy++ )
    {
        for( ushort hx = 0; hx < width; hx++ )
        {
            ProtoMap::TileVec& tiles = TilesField[ hy * width + hx ];
            for( uint i = 0, j = (uint) tiles.size(); i < j; i++ )
                pmap.Tiles.push_back( tiles[ i ] );
            ProtoMap::TileVec& roofs = RoofsField[ hy * width + hx ];
            for( uint i = 0, j = (uint) roofs.size(); i < j; i++ )
                pmap.Tiles.push_back( roofs[ i ] );
        }
    }
}

void HexManager::ClearSelTiles()
{
    for( int hx = 0; hx < maxHexX; hx++ )
    {
        for( int hy = 0; hy < maxHexY; hy++ )
        {
            Field& f = GetField( hx, hy );
            if( f.GetTilesCount( false ) )
            {
                ProtoMap::TileVec& tiles = GetTiles( hx, hy, false );
                for( uint i = 0; i < tiles.size();)
                {
                    if( tiles[ i ].IsSelected )
                    {
                        tiles.erase( tiles.begin() + i );
                        f.EraseTile( i, false );
                    }
                    else
                        i++;
                }
            }
            if( f.GetTilesCount( true ) )
            {
                ProtoMap::TileVec& roofs = GetTiles( hx, hy, true );
                for( uint i = 0; i < roofs.size();)
                {
                    if( roofs[ i ].IsSelected )
                    {
                        roofs.erase( roofs.begin() + i );
                        f.EraseTile( i, true );
                    }
                    else
                        i++;
                }
            }
        }
    }
}

void HexManager::ParseSelTiles()
{
    bool borders_changed = false;
    for( int hx = 0; hx < maxHexX; hx++ )
    {
        for( int hy = 0; hy < maxHexY; hy++ )
        {
            for( int r = 0; r <= 1; r++ )
            {
                bool               roof = ( r == 1 );
                Field&             f = GetField( hx, hy );
                ProtoMap::TileVec& tiles = GetTiles( hx, hy, roof );
                for( size_t i = 0; i < tiles.size();)
                {
                    if( tiles[ i ].IsSelected )
                    {
                        tiles[ i ].IsSelected = false;
                        EraseTile( hx, hy, tiles[ i ].Layer, roof, (int) i );
                        i = 0;
                    }
                    else
                    {
                        i++;
                    }
                }
            }
        }
    }
}

void HexManager::SetTile( hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof, bool select )
{
    if( hx >= maxHexX || hy >= maxHexY )
        return;
    Field&     f = GetField( hx, hy );

    AnyFrames* anim = ResMngr.GetItemAnim( name );
    if( !anim )
        return;

    if( !select )
        EraseTile( hx, hy, layer, is_roof, ( uint ) - 1 );

    Field::Tile&       ftile = f.AddTile( anim, 0, 0, layer, is_roof );
    ProtoMap::TileVec& tiles = GetTiles( hx, hy, is_roof );
    tiles.push_back( ProtoMap::Tile( name, hx, hy, (char) ox, (char) oy, layer, is_roof ) );
    tiles.back().      IsSelected = select;

    if( CheckTilesBorder( ftile, is_roof ) )
    {
        ResizeView();
        RefreshMap();
    }
    else
    {
        if( is_roof )
            RebuildRoof();
        else
            RebuildTiles();
    }
}

void HexManager::EraseTile( ushort hx, ushort hy, uchar layer, bool is_roof, uint skip_index )
{
    Field& f = GetField( hx, hy );
    for( uint i = 0, j = f.GetTilesCount( is_roof ); i < j; i++ )
    {
        Field::Tile& tile = f.GetTile( i, is_roof );
        if( tile.Layer == layer && i != skip_index )
        {
            f.EraseTile( i, is_roof );
            ProtoMap::TileVec& tiles = GetTiles( hx, hy, is_roof );
            tiles.erase( tiles.begin() + i );
            break;
        }
    }
}

void HexManager::AddFastPid( hash pid )
{
    fastPids.insert( pid );
}

bool HexManager::IsFastPid( hash pid )
{
    return fastPids.count( pid ) != 0;
}

void HexManager::ClearFastPids()
{
    fastPids.clear();
}

void HexManager::AddIgnorePid( hash pid )
{
    ignorePids.insert( pid );
}

void HexManager::SwitchIgnorePid( hash pid )
{
    if( ignorePids.count( pid ) )
        ignorePids.erase( pid );
    else
        ignorePids.insert( pid );
}

bool HexManager::IsIgnorePid( hash pid )
{
    return ignorePids.count( pid ) != 0;
}

void HexManager::ClearIgnorePids()
{
    ignorePids.clear();
}

void HexManager::GetHexesRect( const Rect& rect, UShortPairVec& hexes )
{
    hexes.clear();

    if( GameOpt.MapHexagonal )
    {
        int x, y;
        GetHexInterval( rect.L, rect.T, rect.R, rect.B, x, y );
        x = -x;

        int dx = x / HEX_W;
        int dy = y / HEX_LINE_H;
        int adx = abs( dx );
        int ady = abs( dy );

        int hx, hy;
        for( int j = 0; j <= ady; j++ )
        {
            if( dy >= 0 )
            {
                hx = rect.L + j / 2 + ( j & 1 );
                hy = rect.T + ( j - ( hx - rect.L - ( ( rect.L & 1 ) ? 1 : 0 ) ) / 2 );
            }
            else
            {
                hx = rect.L - j / 2 - ( j & 1 );
                hy = rect.T - ( j - ( rect.L - hx - ( ( rect.L & 1 ) ? 0 : 1 ) ) / 2 );
            }

            for( int i = 0; i <= adx; i++ )
            {
                if( hx >= 0 && hy >= 0 && hx < maxHexX && hy < maxHexY )
                    hexes.push_back( std::make_pair( hx, hy ) );

                if( dx >= 0 )
                {
                    if( hx & 1 )
                        hy--;
                    hx++;
                }
                else
                {
                    hx--;
                    if( hx & 1 )
                        hy++;
                }
            }
        }
    }
    else
    {
        int rw, rh;        // Rect width/height
        GetHexInterval( rect.L, rect.T, rect.R, rect.B, rw, rh );
        if( !rw )
            rw = 1;
        if( !rh )
            rh = 1;

        int hw = abs( rw / ( HEX_W / 2 ) ) + ( ( rw % ( HEX_W / 2 ) ) ? 1 : 0 ) + ( abs( rw ) >= HEX_W / 2 ? 1 : 0 ); // Hexes width
        int hh = abs( rh / HEX_LINE_H ) + ( ( rh % HEX_LINE_H ) ? 1 : 0 ) + ( abs( rh ) >= HEX_LINE_H ? 1 : 0 );      // Hexes height
        int shx = rect.L;
        int shy = rect.T;

        for( int i = 0; i < hh; i++ )
        {
            int hx = shx;
            int hy = shy;

            if( rh > 0 )
            {
                if( rw > 0 )
                {
                    if( i & 1 )
                        shx++;
                    else
                        shy++;
                }
                else
                {
                    if( i & 1 )
                        shy++;
                    else
                        shx++;
                }
            }
            else
            {
                if( rw > 0 )
                {
                    if( i & 1 )
                        shy--;
                    else
                        shx--;
                }
                else
                {
                    if( i & 1 )
                        shx--;
                    else
                        shy--;
                }
            }

            for( int j = ( i & 1 ) ? 1 : 0; j < hw; j += 2 )
            {
                if( hx >= 0 && hy >= 0 && hx < maxHexX && hy < maxHexY )
                    hexes.push_back( std::make_pair( hx, hy ) );

                if( rw > 0 )
                    hx--, hy++;
                else
                    hx++, hy--;
            }
        }
    }
}

void HexManager::MarkPassedHexes()
{
    for( int hx = 0; hx < maxHexX; hx++ )
    {
        for( int hy = 0; hy < maxHexY; hy++ )
        {
            Field& f = GetField( hx, hy );
            char&  track = GetHexTrack( hx, hy );
            track = 0;
            if( f.Flags.IsNotPassed )
                track = 2;
            if( f.Flags.IsNotRaked )
                track = 1;
        }
    }
    RefreshMap();
}

#endif // FONLINE_MAPPER
