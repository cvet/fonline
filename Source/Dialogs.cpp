#include "Common.h"
#include "Dialogs.h"
#include "FileManager.h"
#include "IniParser.h"
#ifdef FONLINE_SERVER
# include "ConstantsManager.h"
# include "Critter.h"
# include "Vars.h"
#endif

DialogManager DlgMngr;

int ReadValue( const char* str )
{
    #ifdef FONLINE_SERVER
    return ConstantsManager::GetDefineValue( str );
    #else
    if( Str::IsNumber( str ) )
        return Str::AtoI( str );
    if( Str::CompareCase( str, "true" ) )
        return 1;
    else if( Str::CompareCase( str, "false" ) )
        return 0;
    return -1;
    #endif
}

int GetParamId( const char* str, bool is_demand )
{
    #ifdef FONLINE_SERVER
    Property* prop = Critter::PropertiesRegistrator->Find( str );
    if( !prop )
        WriteLog( "DR property<%s> not found.\n", str );
    else if( !prop->IsPOD() && !prop->IsDict() )
        WriteLog( "DR property<%s> is not POD or Dict type.\n", str );
    else if( prop->IsDict() && prop->GetASObjectType()->GetSubTypeId( 0 ) != asTYPEID_UINT32 )
        WriteLog( "DR property<%s> Dict must have 'uint' in key.\n", str );
    else if( is_demand && !prop->IsReadable() )
        WriteLog( "DR property<%s> is not readable.\n", str );
    else if( !is_demand && !prop->IsWritable() )
        WriteLog( "DR property<%s> is not writable.\n", str );
    else
        return prop->GetEnumValue();
    return -1;
    #else
    return 0;
    #endif
}

ushort GetTempVarId( const char* str )
{
    #ifdef FONLINE_SERVER
    ushort tid = VarMngr.GetTemplateVarId( str );
    if( !tid )
        WriteLog( "Template var not found, name<%s>.\n", str );
    return tid;
    #else
    return 1;
    #endif
}

bool DialogManager::LoadDialogs()
{
    WriteLog( "Load dialogs...\n" );

    while( !dialogPacks.empty() )
        EraseDialog( dialogPacks.begin()->second->PackId );

    FilesCollection files = FilesCollection( PT_SERVER_DIALOGS, "fodlg", false );
    uint            files_loaded = 0;
    while( files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& file = files.GetNextFile( &file_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file<%s>.\n", file_name );
            continue;
        }

        DialogPack* pack = ParseDialog( file_name, (char*) file.GetBuf() );
        if( !pack )
        {
            WriteLog( "Unable to parse dialog<%s>.\n", file_name );
            continue;
        }

        if( !AddDialog( pack ) )
        {
            WriteLog( "Unable to add dialog<%s>.\n", file_name );
            continue;
        }

        files_loaded++;
    }

    WriteLog( "Load dialogs complete, loaded<%u/%u>.\n", files_loaded, files.GetFilesCount() );
    return files_loaded == files.GetFilesCount();
}

bool DialogManager::AddDialog( DialogPack* pack )
{
    if( dialogPacks.count( pack->PackId ) )
    {
        WriteLog( "Dialog<%s> already added.\n", pack->PackName.c_str() );
        return false;
    }

    uint pack_id = pack->PackId & DLGID_MASK;
    for( auto it = dialogPacks.begin(), end = dialogPacks.end(); it != end; ++it )
    {
        uint check_pack_id = ( *it ).first & DLGID_MASK;
        if( pack_id == check_pack_id )
        {
            WriteLog( "Name hash collision for dialogs <%s> and <%s>.\n", pack->PackName.c_str(), ( *it ).second->PackName.c_str() );
            return false;
        }
    }

    dialogPacks.insert( PAIR( pack->PackId, pack ) );
    return true;
}

DialogPack* DialogManager::GetDialog( hash pack_id )
{
    auto it = dialogPacks.find( pack_id );
    return it != dialogPacks.end() ? ( *it ).second : NULL;
}

DialogPack* DialogManager::GetDialogByIndex( uint index )
{
    auto it = dialogPacks.begin();
    while( index-- && it != dialogPacks.end() )
        ++it;
    return it != dialogPacks.end() ? ( *it ).second : NULL;
}

void DialogManager::EraseDialog( hash pack_id )
{
    auto it = dialogPacks.find( pack_id );
    if( it == dialogPacks.end() )
        return;

    delete ( *it ).second;
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
    IniParser fodlg;
    fodlg.LoadFilePtr( data, Str::Length( data ) );

    #define LOAD_FAIL( err )           { WriteLog( "Dialog<%s> - %s\n", pack_name, err ); goto load_false; }
    #define VERIFY_STR_ID( str_id )    ( uint( str_id ) <= ~DLGID_MASK )

    DialogPack* pack = new DialogPack();
    char*       dlg_buf = fodlg.GetApp( "dialog" );
    istrstream  input( dlg_buf, Str::Length( dlg_buf ) );
    char*       lang_buf = NULL;
    pack->PackId = Str::GetHash( pack_name );
    pack->PackName = pack_name;
    StrVec lang_apps;

    // Comment
    char* comment = fodlg.GetApp( "comment" );
    if( comment )
        pack->Comment = comment;
    SAFEDELA( comment );

    // Check dialog pack
    if( pack->PackId <= 0xFFFF )
        LOAD_FAIL( "Invalid hash for dialog name." );

    // Texts
    char lang_key[ MAX_FOTEXT ];
    if( !fodlg.GetStr( "data", "lang", "russ", lang_key ) )
        LOAD_FAIL( "Lang app not found." );

    Str::ParseLine( lang_key, ' ', lang_apps, ParseLangKey );
    if( !lang_apps.size() )
        LOAD_FAIL( "Lang app is empty." );

    for( uint i = 0, j = (uint) lang_apps.size(); i < j; i++ )
    {
        string& lang_app = lang_apps[ i ];
        if( lang_app.size() != 4 )
            LOAD_FAIL( "Language length not equal 4." );

        lang_buf = fodlg.GetApp( lang_app.c_str() );
        if( !lang_buf )
            LOAD_FAIL( "One of the lang section not found." );

        FOMsg temp_msg;
        temp_msg.LoadFromString( lang_buf, Str::Length( lang_buf ) );
        SAFEDELA( lang_buf );

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
        return NULL;

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
        if( Str::CompareCase( script, "NOT_ANSWER_CLOSE_DIALOG" ) )
            Str::Copy( script, "None" );
        else if( Str::CompareCase( script, "NOT_ANSWER_BEGIN_BATTLE" ) )
            Str::Copy( script, "Attack" );
        ret_val = false;
        #else
        script = GetNotAnswerAction( read_str, ret_val );
        if( script < 0 )
        {
            WriteLog( "Unable to parse<%s>.\n", read_str );
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
            LOAD_FAIL( "Parse error0." );

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
    SAFEDELA( dlg_buf );
    SAFEDELA( lang_buf );
    return pack;

load_false:
    WriteLog( "Dialog<%s> - Bad node<%d>.\n", pack_name, dlg_id );
    delete pack;
    SAFEDELA( dlg_buf );
    SAFEDELA( lang_buf );
    return NULL;
}

DemandResult* DialogManager::LoadDemandResult( istrstream& input, bool is_demand )
{
    int   errors = 0;
    char  who = 'p';
    char  oper = '=';
    int   values_count = 0;
    char  svalue[ MAX_FOTEXT ] = { 0 };
    int   ivalue = 0;
    max_t id = 0;
    char  type_str[ MAX_FOTEXT ];
    char  name[ MAX_FOTEXT ] = { 0 };
    bool  no_recheck = false;
    bool  ret_value = false;

    #define READ_VALUE    input >> svalue; ivalue = ReadValue( svalue )

    #ifdef FONLINE_NPCEDITOR
    string script_val[ 5 ];
    #else
    int script_val[ 5 ] = { 0, 0, 0, 0, 0 };
    #endif

    input >> type_str;
    if( input.fail() )
    {
        WriteLog( "Parse DR type fail.\n" );
        return NULL;
    }

    int type = GetDRType( type_str );
    if( type == DR_NO_RECHECK )
    {
        no_recheck = true;
        input >> type_str;
        if( input.fail() )
        {
            WriteLog( "Parse DR type fail2.\n" );
            return NULL;
        }
        type = GetDRType( type_str );
    }

    switch( type )
    {
    case DR_PARAM:
    {
        // Who
        input >> who;
        if( !CheckWho( who ) )
        {
            WriteLog( "Invalid DR param who<%c>.\n", who );
            errors++;
        }

        // Name
        input >> name;
        id = GetParamId( name, is_demand );
        if( id == -1 )
            errors++;

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            WriteLog( "Invalid DR param oper<%c>.\n", oper );
            errors++;
        }

        // Value
        READ_VALUE;
    }
    break;
    case DR_VAR:
    {
        // Who
        input >> who;
        if( !CheckWho( who ) )
        {
            WriteLog( "Invalid DR var who<%c>.\n", who );
            errors++;
        }

        // Name
        input >> name;
        if( ( id = GetTempVarId( name ) ) == 0 )
        {
            WriteLog( "Invalid DR var name<%s>.\n", name );
            errors++;
        }

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            WriteLog( "Invalid DR var oper<%c>.\n", oper );
            errors++;
        }

        // Value
        READ_VALUE;
    }
    break;
    case DR_ITEM:
    {
        // Who
        input >> who;
        if( !CheckWho( who ) )
        {
            WriteLog( "Invalid DR item who<%c>.\n", who );
            errors++;
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
                WriteLog( "Invalid DR item<%s>.\n", name );
                errors++;
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
            WriteLog( "Invalid DR item oper<%c>.\n", oper );
            errors++;
        }

        // Value
        READ_VALUE;
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
        # define READ_SCRIPT_VALUE_( val )    { input >> value_str; val = ReadValue( value_str ); }
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
            WriteLog( "Invalid DR script values count<%d>.\n", values_count );
            values_count = 0;
            errors++;
        }

        #ifdef FONLINE_SERVER
        // Bind function
        # define BIND_D_FUNC( params )                                                \
            do {                                                                      \
                id = Script::Bind( name, "bool %s(Critter&,Critter@" params, false ); \
            } while( 0 )
        # define BIND_R_FUNC( params )                                                                   \
            do {                                                                                         \
                if( ( id = Script::Bind( name, "uint %s(Critter&,Critter@" params, false, true ) ) > 0 ) \
                    ret_value = true;                                                                    \
                else                                                                                     \
                    id = Script::Bind( name, "void %s(Critter&,Critter@" params, false );                \
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
        if( id <= 0 )
        {
            WriteLog( "Script<%s> bind error.\n", name );
            return NULL;
        }
        #endif
    }
    break;
    case DR_LOCK:
    {
        READ_VALUE;
    }
    break;
    case DR_OR:
        break;
    default:
        return NULL;
    }

    // Validate parsing
    if( input.fail() )
    {
        WriteLog( "DR parse fail.\n" );
        errors++;
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
    if( errors )
        return NULL;
    #endif
    return &result;
}

int DialogManager::GetNotAnswerAction( const char* str, bool& ret_val )
{
    ret_val = false;

    if( Str::CompareCase( str, "NOT_ANSWER_CLOSE_DIALOG" ) || Str::CompareCase( str, "None" ) )
        return NOT_ANSWER_CLOSE_DIALOG;
    else if( Str::CompareCase( str, "NOT_ANSWER_BEGIN_BATTLE" ) || Str::CompareCase( str, "Attack" ) )
        return NOT_ANSWER_BEGIN_BATTLE;
    #ifdef FONLINE_SERVER
    else
    {
        int id = Script::Bind( str, "uint %s(Critter&,Critter@,string@)", false, true );
        if( id > 0 )
        {
            ret_val = true;
            return id;
        }
        return Script::Bind( str, "void %s(Critter&,Critter@,string@)", false );
    }
    #endif // FONLINE_SERVER

    return -1;
}

int DialogManager::GetDRType( const char* str )
{
    if( Str::CompareCase( str, "_param" ) )
        return DR_PARAM;
    else if( Str::CompareCase( str, "_item" ) )
        return DR_ITEM;
    else if( Str::CompareCase( str, "_lock" ) )
        return DR_LOCK;
    else if( Str::CompareCase( str, "_script" ) )
        return DR_SCRIPT;
    else if( Str::CompareCase( str, "_var" ) )
        return DR_VAR;
    else if( Str::CompareCase( str, "no_recheck" ) )
        return DR_NO_RECHECK;
    else if( Str::CompareCase( str, "or" ) )
        return DR_OR;
    return DR_NONE;
}

bool DialogManager::CheckOper( char oper )
{
    return oper == '>' || oper == '<' || oper == '=' || oper == '+' || oper == '-' || oper == '*' ||
           oper == '/' || oper == '=' || oper == '!' || oper == '}' || oper == '{';
}

bool DialogManager::CheckWho( char who )
{
    return who == 'p' || who == 'n';
}
