#include "Common.h"
#include "Client.h"

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

int FOClient::InitIface()
{
    WriteLog( "Interface initialization...\n" );

    // Barter
    BarterPlayerId = 0;
    BarterK = 0;
    BarterIsPlayers = false;
    BarterOpponentHide = false;
    BarterOffer = false;
    BarterOpponentOffer = false;

    // Minimap
    LmapZoom = 2;
    LmapSwitchHi = false;
    LmapPrepareNextTick = 0;

    // Global map
    GmapFog.Create( GM__MAXZONEX, GM__MAXZONEY, nullptr );

    // PickUp
    PupTransferType = 0;
    PupContId = 0;
    PupContPid = 0;
    PupCount = 0;
    Item::ClearItems( PupCont2 );

    // Pipboy
    Automaps.clear();

    // Save/Load
    if( Singleplayer )
    {
        SaveLoadDataSlots.clear();
        SaveLoadDraft = SprMngr.CreateRenderTarget( false, false, false, SAVE_LOAD_IMAGE_WIDTH, SAVE_LOAD_IMAGE_HEIGHT, true );
        SaveLoadProcessDraft = false;
        SaveLoadDraftValid = false;
    }

    // Hex field sprites
    HexMngr.ReloadSprites();

    WriteLog( "Interface initialization complete.\n" );
    return 0;
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

    // Text on head
    for( auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end(); it++ )
        it->second->DrawTextOnHead();

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
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_WRONG_LOGIN ) );
        return false;
    }

    if( !Str::IsValidUTF8( GameOpt.Name->c_str() ) || Str::Substring( GameOpt.Name->c_str(), "*" ) )
    {
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_NAME_WRONG_CHARS ) );
        return false;
    }

    uint pass_len_utf8 = Str::LengthUTF8( Password.c_str() );
    if( pass_len_utf8 < MIN_NAME || pass_len_utf8 < GameOpt.MinNameLength ||
        pass_len_utf8 > MAX_NAME || pass_len_utf8 > GameOpt.MaxNameLength )
    {
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_WRONG_PASS ) );
        return false;
    }

    if( !Str::IsValidUTF8( Password.c_str() ) || Str::Substring( Password.c_str(), "*" ) )
    {
        AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_PASS_WRONG_CHARS ) );
        return false;
    }

    // Save login and password
    Crypt.SetCache( "__name", (uchar*) GameOpt.Name->c_str(), (uint) GameOpt.Name->length() + 1 );
    Crypt.SetCache( "__pass", (uchar*) Password.c_str(), (uint) Password.length() + 1 );

    AddMess( FOMB_GAME, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_NET_CONNECTION ) );
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

void FOClient::ShowMainScreen( int new_screen, ScriptDictionary* params /* = NULL */ )
{
    while( GetActiveScreen() != SCREEN_NONE )
        HideScreen( SCREEN_NONE );

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
    switch( screen )
    {
    case SCREEN__PICKUP:
        CollectContItems();
        break;
    default:
        break;
    }

    RunScreenScript( true, screen, params );
}

void FOClient::HideScreen( int screen )
{
    if( screen == SCREEN_NONE )
        screen = GetActiveScreen();
    if( screen == SCREEN_NONE )
        return;

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
int   FOClient::GmapGroupCurX, FOClient::GmapGroupCurY, FOClient::GmapGroupToX, FOClient::GmapGroupToY;
bool  FOClient::GmapWait;
float FOClient::GmapGroupSpeed;

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
    GmapLoc.clear();
    SAFEREL( GmapCar.Car );
    GmapCar.MasterId = 0;
    GmapFog.Fill( 0 );
    GmapActive = false;
    DeleteCritters();
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

#define SAVE_LOAD_LINES_PER_SLOT    ( 3 )
void FOClient::SaveLoadCollect()
{
    /*// Collect singleplayer saves
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
                                       day, CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_MONTH( month ) ), year, hour, minute,
                                       CurLang.Msg[ TEXTMSG_LOCATIONS ].GetStr( STR_LOC_MAP_NAME( CurMapLocPid, CurMapIndexInLoc ) ) );
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
       SaveLoadShowDraft();*/
}

void FOClient::SaveLoadSaveGame( const char* name )
{
    /*// Get name of new save
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
       HideScreen( SCREEN_NONE );*/
}

void FOClient::SaveLoadFillDraft()
{
    /*SaveLoadProcessDraft = false;
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
       }*/
}

void FOClient::SaveLoadShowDraft()
{
    /*SaveLoadDraftValid = false;
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
       }*/
}

void FOClient::SaveLoadProcessDone()
{
    /*if( SaveLoadSave )
       {
        SaveLoadProcessDraft = true;

        // ShowScreen( SCREEN__SAY );
        // SayType = DIALOGSAY_SAVE;
        // SayTitle = CurLang.Msg[ TEXTMSG_GAME ].GetStr( STR_SAVE_LOAD_TYPE_RECORD_NAME );
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
            HideScreen( SCREEN_NONE );
            WaitPing();
        }
       }*/
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================
