#include "Common.h"
#include "Client.h"

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

int FOClient::InitIface()
{
    WriteLog( "Interface initialization...\n" );

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

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================

void FOClient::GameDraw()
{
    // Move cursor
    if( GameOpt.ShowMoveCursor )
        HexMngr.SetCursorPos( GameOpt.MouseX, GameOpt.MouseY, Keyb::CtrlDwn, false );

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
    string text = msg;
    Script::RaiseInternalEvent( ClientFunctions.MessageBox, &text, mess_type, script_call );
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
                Str::Copy( tag, player ? player->GetName().c_str() : "" );
            }
            // Npc name
            else if( Str::CompareCase( tag, "nname" ) )
            {
                Str::Copy( tag, npc ? npc->GetName().c_str() : "" );
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
                    char* rnd_str = rnd[ Random( 0, (int) rnd.size() - 1 ) ];
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
                uint bind_id = Script::BindByFuncName( func_name, "string %s(string)", true );
                Str::Copy( tag, "<script function not found>" );
                if( bind_id )
                {
                    string script_lexems = lexems ? lexems : "";
                    Script::PrepareContext( bind_id, "Game" );
                    Script::SetArgObject( &script_lexems );
                    if( Script::RunPrepared() )
                    {
                        string& result = *(string*) Script::GetReturnedObject();
                        Str::Copy( tag, result.c_str() );
                    }
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

void FOClient::ShowMainScreen( int new_screen, CScriptDictionary* params /* = NULL */ )
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

    CScriptArray* arr = Script::CreateArray( "int[]" );
    Script::RaiseInternalEvent( ClientFunctions.GetActiveScreens, arr );
    Script::AssignScriptArrayInVector( active_screens, arr );
    arr->Release();

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

void FOClient::ShowScreen( int screen, CScriptDictionary* params /* = NULL */ )
{
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

void FOClient::RunScreenScript( bool show, int screen, CScriptDictionary* params )
{
    Script::RaiseInternalEvent( ClientFunctions.ScreenChange, show, screen, params );
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
                cur_color = ( f.Crit == Chosen ? 0xFF0000FF : 0xFFFF0000 );
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

void FOClient::GmapNullParams()
{
    GmapLoc.clear();
    GmapFog->Fill( 0 );
    DeleteCritters();
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
            Str::ParseLine( settings[ "SaveLoadPicture" ], ' ', pic_data, Str::AtoUI );

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
            UpdateFileActive = true;
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
