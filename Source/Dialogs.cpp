#include "Common.h"
#include "Dialogs.h"
#include "FileManager.h"
#include "IniParser.h"
#ifdef FONLINE_SERVER
# include "Script.h"
# include "Critter.h"
# include "Map.h"
#endif

DialogManager DlgMngr;

int GetPropEnumIndex( const char* str, bool is_demand, int& type, bool& is_hash )
{
    #ifdef FONLINE_SERVER
    Property* prop_global = GlobalVars::PropertiesRegistrator->Find( str );
    Property* prop_critter = Critter::PropertiesRegistrator->Find( str );
    Property* prop_item = Item::PropertiesRegistrator->Find( str );
    Property* prop_location = Location::PropertiesRegistrator->Find( str );
    Property* prop_map = Map::PropertiesRegistrator->Find( str );
    int       count = ( prop_global ? 1 : 0 ) + ( prop_critter ? 1 : 0 ) + ( prop_item ? 1 : 0 ) + ( prop_location ? 1 : 0 ) + ( prop_map ? 1 : 0 );
    if( count == 0 )
    {
        WriteLog( "DR property '%s' not found in GlobalVars/Critter/Item/Location/Map.\n", str );
        return -1;
    }
    else if( count > 1 )
    {
        WriteLog( "DR property '%s' found multiple instances in GlobalVars/Critter/Item/Location/Map.\n", str );
        return -1;
    }

    Property* prop = nullptr;
    if( prop_global )
        prop = prop_global, type = DR_PROP_GLOBAL;
    else if( prop_critter )
        prop = prop_critter, type = DR_PROP_CRITTER;
    else if( prop_item )
        prop = prop_item, type = DR_PROP_ITEM;
    else if( prop_location )
        prop = prop_location, type = DR_PROP_LOCATION;
    else if( prop_map )
        prop = prop_map, type = DR_PROP_MAP;

    if( type == DR_PROP_CRITTER && prop->IsDict() )
    {
        type = DR_PROP_CRITTER_DICT;
        if( prop->GetASObjectType()->GetSubTypeId( 0 ) != asTYPEID_UINT32 )
        {
            WriteLog( "DR property '%s' Dict must have 'uint' in key.\n", str );
            return -1;
        }
    }
    else
    {
        if( !prop->IsPOD() )
        {
            WriteLog( "DR property '%s' is not POD type.\n", str );
            return -1;
        }
    }

    if( is_demand && !prop->IsReadable() )
    {
        WriteLog( "DR property '%s' is not readable.\n", str );
        return -1;
    }
    else if( !is_demand && !prop->IsWritable() )
    {
        WriteLog( "DR property '%s' is not writable.\n", str );
        return -1;
    }

    is_hash = prop->IsHash();
    return prop->GetRegIndex();
    #else
    return 0;
    #endif
}

bool DialogManager::LoadDialogs()
{
    WriteLog( "Load dialogs...\n" );

    while( !dialogPacks.empty() )
        EraseDialog( dialogPacks.begin()->second->PackId );

    FilesCollection files = FilesCollection( "fodlg" );
    uint            files_loaded = 0;
    while( files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& file = files.GetNextFile( &file_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file '%s'.\n", file_name );
            continue;
        }

        DialogPack* pack = ParseDialog( file_name, (char*) file.GetBuf() );
        if( !pack )
        {
            WriteLog( "Unable to parse dialog '%s'.\n", file_name );
            continue;
        }

        if( !AddDialog( pack ) )
        {
            WriteLog( "Unable to add dialog '%s'.\n", file_name );
            continue;
        }

        files_loaded++;
    }

    WriteLog( "Load dialogs complete, count %u.\n", files_loaded );
    return files_loaded == files.GetFilesCount();
}

bool DialogManager::AddDialog( DialogPack* pack )
{
    if( dialogPacks.count( pack->PackId ) )
    {
        WriteLog( "Dialog '%s' already added.\n", pack->PackName.c_str() );
        return false;
    }

    uint pack_id = pack->PackId & DLGID_MASK;
    for( auto it = dialogPacks.begin(), end = dialogPacks.end(); it != end; ++it )
    {
        uint check_pack_id = it->first & DLGID_MASK;
        if( pack_id == check_pack_id )
        {
            WriteLog( "Name hash collision for dialogs  '%s' and  '%s'.\n", pack->PackName.c_str(), it->second->PackName.c_str() );
            return false;
        }
    }

    dialogPacks.insert( PAIR( pack->PackId, pack ) );
    return true;
}

DialogPack* DialogManager::GetDialog( hash pack_id )
{
    auto it = dialogPacks.find( pack_id );
    return it != dialogPacks.end() ? it->second : nullptr;
}

DialogPack* DialogManager::GetDialogByIndex( uint index )
{
    auto it = dialogPacks.begin();
    while( index-- && it != dialogPacks.end() )
        ++it;
    return it != dialogPacks.end() ? it->second : nullptr;
}

void DialogManager::EraseDialog( hash pack_id )
{
    auto it = dialogPacks.find( pack_id );
    if( it == dialogPacks.end() )
        return;

    delete it->second;
    dialogPacks.erase( it );
}

void DialogManager::Finish()
{
    WriteLog( "Dialog manager finish...\n" );
    while( !dialogPacks.empty() )
        EraseDialog( dialogPacks.begin()->second->PackId );
    WriteLog( "Dialog manager finish complete.\n" );
}

string ParseLangKey( const char* str )
{
    while( *str == ' ' )
        str++;
    return string( str );
}

DialogPack* DialogManager::ParseDialog( const char* pack_name, const char* data )
{
    IniParser fodlg( data );

    #define LOAD_FAIL( err )           { WriteLog( "Dialog '%s' - %s\n", pack_name, err ); goto load_false; }
    #define VERIFY_STR_ID( str_id )    ( uint( str_id ) <= ~DLGID_MASK )

    DialogPack* pack = new DialogPack();
    const char* dlg_buf = fodlg.GetAppContent( "dialog" );
    istrstream  input( dlg_buf );
    const char* lang_buf = nullptr;
    pack->PackId = Str::GetHash( pack_name );
    pack->PackName = pack_name;
    StrVec lang_apps;

    // Comment
    const char* comment = fodlg.GetAppContent( "comment" );
    if( comment )
        pack->Comment = comment;

    // Check dialog pack
    if( pack->PackId <= 0xFFFF )
        LOAD_FAIL( "Invalid hash for dialog name." );

    // Texts
    const char* lang_key = fodlg.GetStr( "data", "lang" );
    if( !lang_key )
        LOAD_FAIL( "Lang app not found." );

    Str::ParseLine( lang_key, ' ', lang_apps, ParseLangKey );
    if( !lang_apps.size() )
        LOAD_FAIL( "Lang app is empty." );

    for( uint i = 0, j = (uint) lang_apps.size(); i < j; i++ )
    {
        string& lang_app = lang_apps[ i ];
        if( lang_app.size() != 4 )
            LOAD_FAIL( "Language length not equal 4." );

        lang_buf = fodlg.GetAppContent( lang_app.c_str() );
        if( !lang_buf )
            LOAD_FAIL( "One of the lang section not found." );

        FOMsg temp_msg;
        if( !temp_msg.LoadFromString( lang_buf, Str::Length( lang_buf ) ) )
            LOAD_FAIL( "Load MSG fail." );

        if( temp_msg.GetStrNumUpper( 100000000 + ~DLGID_MASK ) )
            LOAD_FAIL( "Text have any text with index greather than 4000." );

        pack->TextsLang.push_back( *(uint*) lang_app.c_str() );
        pack->Texts.push_back( new FOMsg() );

        uint str_num = 0;
        while( str_num = temp_msg.GetStrNumUpper( str_num ) )
        {
            uint count = temp_msg.Count( str_num );
            uint new_str_num = DLG_STR_ID( pack->PackId, ( str_num < 100000000 ? str_num / 10 : str_num - 100000000 + 12000 ) );
            for( uint n = 0; n < count; n++ )
            {
                const char* str = temp_msg.GetStr( str_num, n );
                if( Str::Substring( str, "\n\\[" ) )
                {
                    char  str_copy[ MAX_FOTEXT ];
                    Str::Copy( str_copy, str );
                    char* s = str_copy;
                    while( s = Str::Substring( s, "\n\\[" ) )
                        Str::EraseInterval( s + 1, 1 );
                    pack->Texts[ i ]->AddStr( new_str_num, str_copy );
                }
                else
                {
                    pack->Texts[ i ]->AddStr( new_str_num, str );
                }
            }
        }
    }

    // Dialog
    if( !dlg_buf )
        LOAD_FAIL( "Dialog section not found." );

    char ch;
    input >> ch;
    if( ch != '&' )
        return nullptr;

    uint dlg_id;
    uint text_id;
    uint link;
    char read_str[ MAX_FOTEXT ];
    bool ret_val;

    #ifdef FONLINE_NPCEDITOR
    char script[ MAX_FOTEXT ];
    #else
    int  script;
    #endif
    uint flags;

    while( true )
    {
        input >> dlg_id;
        if( input.eof() )
            goto load_done;
        if( input.fail() )
            LOAD_FAIL( "Bad dialog id number." );
        input >> text_id;
        if( input.fail() )
            LOAD_FAIL( "Bad text link." );
        if( !VERIFY_STR_ID( text_id / 10 ) )
            LOAD_FAIL( "Invalid text link value." );
        input >> read_str;
        if( input.fail() )
            LOAD_FAIL( "Bad not answer action." );
        #ifdef FONLINE_NPCEDITOR
        Str::Copy( script, read_str );
        if( Str::Compare( script, "NOT_ANSWER_CLOSE_DIALOG" ) )
            Str::Copy( script, "None" );
        else if( Str::Compare( script, "NOT_ANSWER_BEGIN_BATTLE" ) )
            Str::Copy( script, "Attack" );
        ret_val = false;
        #else
        script = GetNotAnswerAction( read_str, ret_val );
        if( script < 0 )
        {
            WriteLog( "Unable to parse '%s'.\n", read_str );
            LOAD_FAIL( "Invalid not answer action." );
        }
        #endif
        input >> flags;
        if( input.fail() )
            LOAD_FAIL( "Bad flags." );

        Dialog current_dialog;
        current_dialog.Id = dlg_id;
        current_dialog.TextId = DLG_STR_ID( pack->PackId, text_id / 10 );
        current_dialog.DlgScript = script;
        current_dialog.Flags = flags;
        current_dialog.RetVal = ret_val;

        // Read answers
        input >> ch;
        if( input.fail() )
            LOAD_FAIL( "Dialog corrupted." );
        if( ch == '@' )     // End of current dialog node
        {
            pack->Dialogs.push_back( current_dialog );
            continue;
        }
        if( ch == '&' )     // End of all
        {
            pack->Dialogs.push_back( current_dialog );
            break;
        }
        if( ch != '#' )
            LOAD_FAIL( "Parse error 0." );

        while( !input.eof() )
        {
            input >> link;
            if( input.fail() )
                LOAD_FAIL( "Bad link in answer." );
            input >> text_id;
            if( input.fail() )
                LOAD_FAIL( "Bad text link in answer." );
            if( !VERIFY_STR_ID( text_id / 10 ) )
                LOAD_FAIL( "Invalid text link value in answer." );
            DialogAnswer current_answer;
            current_answer.Link = link;
            current_answer.TextId = DLG_STR_ID( pack->PackId, text_id / 10 );

            while( true )
            {
                input >> ch;
                if( input.fail() )
                    LOAD_FAIL( "Parse answer character fail." );

                // Demands
                if( ch == 'D' )
                {
                    DemandResult* d = LoadDemandResult( input, true );
                    if( !d )
                        LOAD_FAIL( "Demand not loaded." );
                    current_answer.Demands.push_back( *d );
                }
                // Results
                else if( ch == 'R' )
                {
                    DemandResult* r = LoadDemandResult( input, false );
                    if( !r )
                        LOAD_FAIL( "Result not loaded." );
                    current_answer.Results.push_back( *r );
                }
                else if( ch == '*' || ch == 'd' || ch == 'r' )
                {
                    LOAD_FAIL( "Found old token, update dialog file to actual format (resave in version 2.22)." );
                }
                else
                {
                    break;
                }
            }
            current_dialog.Answers.push_back( current_answer );

            if( ch == '#' )
                continue;           // Next
            if( ch == '@' )
                break;              // End of current dialog node
            if( ch == '&' )         // End of all
            {
                pack->Dialogs.push_back( current_dialog );
                goto load_done;
            }
        }
        pack->Dialogs.push_back( current_dialog );
    }

load_done:
    return pack;

load_false:
    WriteLog( "Dialog '%s' - Bad node %d.\n", pack_name, dlg_id );
    delete pack;
    return nullptr;
}

DemandResult* DialogManager::LoadDemandResult( istrstream& input, bool is_demand )
{
    bool  fail = false;
    char  who = DR_WHO_PLAYER;
    char  oper = '=';
    int   values_count = 0;
    char  svalue[ MAX_FOTEXT ] = { 0 };
    int   ivalue = 0;
    max_t id = 0;
    char  type_str[ MAX_FOTEXT ];
    char  name[ MAX_FOTEXT ] = { 0 };
    bool  no_recheck = false;
    bool  ret_value = false;

    #ifdef FONLINE_NPCEDITOR
    string script_val[ 5 ];
    #else
    int    script_val[ 5 ] = { 0, 0, 0, 0, 0 };
    #endif

    input >> type_str;
    if( input.fail() )
    {
        WriteLog( "Parse DR type fail.\n" );
        return nullptr;
    }

    int type = GetDRType( type_str );
    if( type == DR_NO_RECHECK )
    {
        no_recheck = true;
        input >> type_str;
        if( input.fail() )
        {
            WriteLog( "Parse DR type fail2.\n" );
            return nullptr;
        }
        type = GetDRType( type_str );
    }

    switch( type )
    {
    case DR_PROP_CRITTER:
    {
        // Who
        input >> who;
        who = GetWho( who );
        if( who == DR_WHO_NONE )
        {
            WriteLog( "Invalid DR property who '%c'.\n", who );
            fail = true;
        }

        // Name
        input >> name;
        bool is_hash = false;
        id = (max_t) GetPropEnumIndex( name, is_demand, type, is_hash );
        if( id == (max_t) -1 )
            fail = true;

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            WriteLog( "Invalid DR property oper '%c'.\n", oper );
            fail = true;
        }

        // Value
        input >> svalue;
        if( is_hash )
            ivalue = (int) Str::GetHash( svalue );
        else
            ivalue = ConvertParamValue( svalue, fail );
    }
    break;
    case DR_ITEM:
    {
        // Who
        input >> who;
        who = GetWho( who );
        if( who == DR_WHO_NONE )
        {
            WriteLog( "Invalid DR item who '%c'.\n", who );
            fail = true;
        }

        // Name
        input >> name;
        if( Str::Length( name ) > 4 && Str::CompareCount( name, "PID_", 4 ) )
        {
            const char* new_name = ConvertProtoIdByStr( name );
            if( new_name )
            {
                id = Str::GetHash( new_name );
                Str::Copy( name, new_name );
            }
            else
            {
                WriteLog( "Invalid DR item '%s'.\n", name );
                fail = true;
                id = 0;
            }
        }
        else
        {
            id = Str::GetHash( name );
        }

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            WriteLog( "Invalid DR item oper '%c'.\n", oper );
            fail = true;
        }

        // Value
        input >> svalue;
        ivalue = (int) Str::GetHash( svalue );
    }
    break;
    case DR_SCRIPT:
    {
        // Script name
        input >> name;

        // Values count
        input >> values_count;

        // Values
        #ifdef FONLINE_NPCEDITOR
        # define READ_SCRIPT_VALUE_( val )         \
            { input >> value_str; val = value_str; \
            }
        #else
        # define READ_SCRIPT_VALUE_( val )    { input >> value_str; val = ConvertParamValue( value_str, fail ); }
        #endif
        char value_str[ MAX_FOTEXT ];
        if( values_count > 0 )
            READ_SCRIPT_VALUE_( script_val[ 0 ] );
        if( values_count > 1 )
            READ_SCRIPT_VALUE_( script_val[ 1 ] );
        if( values_count > 2 )
            READ_SCRIPT_VALUE_( script_val[ 2 ] );
        if( values_count > 3 )
            READ_SCRIPT_VALUE_( script_val[ 3 ] );
        if( values_count > 4 )
            READ_SCRIPT_VALUE_( script_val[ 4 ] );
        if( values_count > 5 )
        {
            WriteLog( "Invalid DR script values count %d.\n", values_count );
            values_count = 0;
            fail = true;
        }

        #ifdef FONLINE_SERVER
        // Bind function
        # define BIND_D_FUNC( params )                                                            \
            do {                                                                                  \
                id = Script::BindByScriptName( name, "bool %s(Critter&,Critter@" params, false ); \
            } while( 0 )
        # define BIND_R_FUNC( params )                                                                               \
            do {                                                                                                     \
                if( ( id = Script::BindByScriptName( name, "uint %s(Critter&,Critter@" params, false, true ) ) > 0 ) \
                    ret_value = true;                                                                                \
                else                                                                                                 \
                    id = Script::BindByScriptName( name, "void %s(Critter&,Critter@" params, false );                \
            } while( 0 )
        switch( values_count )
        {
        case 1:
            if( is_demand )
                BIND_D_FUNC( ",int)" );
            else
                BIND_R_FUNC( ",int)" );
            break;
        case 2:
            if( is_demand )
                BIND_D_FUNC( ",int,int)" );
            else
                BIND_R_FUNC( ",int,int)" );
            break;
        case 3:
            if( is_demand )
                BIND_D_FUNC( ",int,int,int)" );
            else
                BIND_R_FUNC( ",int,int,int)" );
            break;
        case 4:
            if( is_demand )
                BIND_D_FUNC( ",int,int,int,int)" );
            else
                BIND_R_FUNC( ",int,int,int,int)" );
            break;
        case 5:
            if( is_demand )
                BIND_D_FUNC( ",int,int,int,int,int)" );
            else
                BIND_R_FUNC( ",int,int,int,int,int)" );
            break;
        default:
            if( is_demand )
                BIND_D_FUNC( ")" );
            else
                BIND_R_FUNC( ")" );
            break;
        }
        if( !id )
        {
            WriteLog( "Script '%s' bind error.\n", name );
            return nullptr;
        }
        #endif
    }
    break;
    case DR_OR:
        break;
    default:
        return nullptr;
    }

    // Validate parsing
    if( input.fail() )
    {
        WriteLog( "DR parse fail.\n" );
        fail = true;
    }

    // Fill
    static DemandResult result;
    result.Type = type;
    result.Who = who;
    result.ParamId = id;
    result.Op = oper;
    result.ValuesCount = values_count;
    result.NoRecheck = no_recheck;
    result.RetValue = ret_value;
    #ifdef FONLINE_NPCEDITOR
    result.ValueStr = svalue;
    result.ParamName = name;
    result.ValuesNames[ 0 ] = script_val[ 0 ];
    result.ValuesNames[ 1 ] = script_val[ 1 ];
    result.ValuesNames[ 2 ] = script_val[ 2 ];
    result.ValuesNames[ 3 ] = script_val[ 3 ];
    result.ValuesNames[ 4 ] = script_val[ 4 ];
    #else
    result.Value = ivalue;
    result.ValueExt[ 0 ] = script_val[ 0 ];
    result.ValueExt[ 1 ] = script_val[ 1 ];
    result.ValueExt[ 2 ] = script_val[ 2 ];
    result.ValueExt[ 3 ] = script_val[ 3 ];
    result.ValueExt[ 4 ] = script_val[ 4 ];
    if( fail )
        return nullptr;
    #endif
    return &result;
}

uint DialogManager::GetNotAnswerAction( const char* str, bool& ret_val )
{
    ret_val = false;

    if( Str::Compare( str, "NOT_ANSWER_CLOSE_DIALOG" ) || Str::Compare( str, "None" ) )
        return NOT_ANSWER_CLOSE_DIALOG;
    if( Str::Compare( str, "NOT_ANSWER_BEGIN_BATTLE" ) || Str::Compare( str, "Attack" ) )
        return NOT_ANSWER_BEGIN_BATTLE;

    #ifdef FONLINE_SERVER
    uint id = Script::BindByScriptName( str, "uint %s(Critter&,Critter@,string@)", false, true );
    if( id )
    {
        ret_val = true;
        return id;
    }
    return Script::BindByScriptName( str, "void %s(Critter&,Critter@,string@)", false );
    #endif

    return 0;
}

char DialogManager::GetDRType( const char* str )
{
    if( Str::Compare( str, "Property" ) || Str::Compare( str, "_param" ) )
        return DR_PROP_CRITTER;
    else if( Str::Compare( str, "Item" ) || Str::Compare( str, "_item" ) )
        return DR_ITEM;
    else if( Str::Compare( str, "Script" ) || Str::Compare( str, "_script" ) )
        return DR_SCRIPT;
    else if( Str::Compare( str, "NoRecheck" ) || Str::Compare( str, "no_recheck" ) )
        return DR_NO_RECHECK;
    else if( Str::Compare( str, "Or" ) || Str::Compare( str, "or" ) )
        return DR_OR;
    return DR_NONE;
}

char DialogManager::GetWho( char who )
{
    if( who == 'P' || who == 'p' )
        return DR_WHO_PLAYER;
    else if( who == 'N' || who == 'n' )
        return DR_WHO_PLAYER;
    return DR_WHO_NONE;
}

bool DialogManager::CheckOper( char oper )
{
    return oper == '>' || oper == '<' || oper == '=' || oper == '+' || oper == '-' || oper == '*' ||
           oper == '/' || oper == '=' || oper == '!' || oper == '}' || oper == '{';
}
