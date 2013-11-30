#include "StdAfx.h"
#include "Dialogs.h"
#include "ConstantsManager.h"
#include "FileManager.h"
#include "IniParser.h"

DialogManager DlgMngr;

bool DialogManager::LoadDialogs( const char* list_name )
{
    WriteLog( "Load Dialogs..." );

    if( !list_name )
    {
        WriteLog( "List name nullptr.\n" );
        return false;
    }

    WriteLog( "from list<%s>.\n", list_name );

    FileManager lst;
    if( !lst.LoadFile( list_name, PT_SERVER_DIALOGS ) )
    {
        WriteLog( "File not found.\n" );
        return false;
    }

    istrstream str( (char*) lst.GetBuf() );
    int        dlg_count = 0;
    int        dlg_loaded = 0;
    while( !str.eof() )
    {
        char ch;
        str >> ch;
        if( ch != '$' )
            continue;

        dlg_count++;

        uint dlg_id;
        str >> dlg_id;
        if( str.fail() )
        {
            WriteLog( "Unable to read id of dialog.\n" );
            continue;
        }

        char dlg_name[ MAX_FOTEXT ];
        str >> dlg_name;
        if( str.fail() )
        {
            WriteLog( "Unable to read name of dialog.\n" );
            continue;
        }

        if( DialogsPacks.count( dlg_id ) )
        {
            WriteLog( "Dialog id<%u> is already parsed.\n", dlg_id );
            continue;
        }

        char name[ 256 ];
        Str::Copy( name, dlg_name );
        Str::Append( name, DIALOG_FILE_EXT );

        FileManager fdlg;
        if( !fdlg.LoadFile( name, PT_SERVER_DIALOGS ) )
        {
            WriteLog( "Unable to open dialog file, id<%u>, name<%s>.\n", dlg_id, name );
            continue;
        }

        DialogPack* pack = ParseDialog( dlg_name, dlg_id, (char*) fdlg.GetBuf() );
        if( !pack )
        {
            WriteLog( "Unable to parse dialog, id<%u>, path<%s>.\n", dlg_id, dlg_name );
            continue;
        }

        if( !AddDialogs( pack ) )
        {
            WriteLog( "Unable to add dialogs pack, id<%u>, path<%s>.\n", dlg_id, dlg_name );
            continue;
        }

        dlg_loaded++;
    }

    WriteLog( "Loading dialogs finish, loaded<%u/%u>.\n", dlg_loaded, dlg_count );
    return dlg_count == dlg_loaded;
}

void DialogManager::SaveList( const char* list_path, const char* list_name )
{
    if( !list_path || !list_name )
        return;
    char full_path[ 1024 ];
    Str::Copy( full_path, list_path );
    Str::Append( full_path, list_name );

    FileManager fm;

    fm.SetStr( "**************************************************************************************\n" );
    fm.SetStr( "***  DIALOGS  *********  COUNT: %08d  ********************************************\n", DlgPacksNames.size() );
    fm.SetStr( "**************************************************************************************\n" );

    for( auto it = DlgPacksNames.begin(); it != DlgPacksNames.end(); ++it )
        fm.SetStr( "$\t%u\t%s\n", ( *it ).second, ( *it ).first.c_str() );

    fm.SetStr( "**************************************************************************************\n" );
    fm.SetStr( "**************************************************************************************\n" );
    fm.SetStr( "**************************************************************************************\n" );

    fm.SaveOutBufToFile( full_path, -1 );
}

bool DialogManager::AddDialogs( DialogPack* pack )
{
    if( DialogsPacks.find( pack->PackId ) != DialogsPacks.end() )
        return false;
    if( DlgPacksNames.find( pack->PackName ) != DlgPacksNames.end() )
        return false;

    DialogsPacks.insert( PAIR( pack->PackId, pack ) );
    DlgPacksNames.insert( PAIR( string( pack->PackName ), pack->PackId ) );
    return true;
}

DialogPack* DialogManager::GetDialogPack( uint num_pack )
{
    auto it = DialogsPacks.find( num_pack );
    return it == DialogsPacks.end() ? NULL : ( *it ).second;
}

DialogsVec* DialogManager::GetDialogs( uint num_pack )
{
    //	auto it=std::find(DialogsPacks.begin(),DialogsPacks.end(),num_pack);
    //	return it!=DialogsPacks.end()?&(*it):NULL;
    auto it = DialogsPacks.find( num_pack );
    return it == DialogsPacks.end() ? NULL : &( *it ).second->Dialogs;
}

void DialogManager::EraseDialogs( uint num_pack )
{
    auto it = DialogsPacks.find( num_pack );
    if( it == DialogsPacks.end() )
        return;

    DlgPacksNames.erase( ( *it ).second->PackName );
    delete ( *it ).second;
    DialogsPacks.erase( it );
}

void DialogManager::EraseDialogs( string name_pack )
{
    auto it = DlgPacksNames.find( name_pack );
    if( it == DlgPacksNames.end() )
        return;
    EraseDialogs( ( *it ).second );
}

void DialogManager::Finish()
{
    WriteLog( "Dialog manager finish...\n" );
    DialogsPacks.clear();
    DlgPacksNames.clear();
    WriteLog( "Dialog manager finish complete.\n" );
}

string ParseLangKey( const char* str )
{
    while( *str == ' ' )
        str++;
    return string( str );
}

DialogPack* DialogManager::ParseDialog( const char* name, uint id, const char* data )
{
    LastErrors = "";

    if( !data )
    {
        AddError( "Data nullptr." );
        return NULL;
    }

    IniParser fodlg;
    if( !fodlg.LoadFilePtr( data, Str::Length( data ) ) )
    {
        AddError( "Internal error." );
        return NULL;
    }

    #define LOAD_FAIL( err )    { AddError( err ); goto load_false; }
    DialogPack* pack = new DialogPack( id, string( name ) );
    char*       dlg_buf = fodlg.GetApp( "dialog" );
    istrstream  input( dlg_buf, Str::Length( dlg_buf ) );
    char*       lang_buf = NULL;
    pack->PackId = id;
    pack->PackName = name;
    StrVec& lang = pack->TextsLang;

    // Comment
    char* comment = fodlg.GetApp( "comment" );
    if( comment )
        pack->Comment = comment;
    SAFEDELA( comment );

    // Texts
    char lang_key[ 256 ];
    if( !fodlg.GetStr( "data", "lang", "russ", lang_key ) )
        LOAD_FAIL( "Lang app not found." );

    Str::ParseLine< StrVec, string ( * )( const char* ) >( lang_key, ' ', lang, ParseLangKey );
    if( !lang.size() )
        LOAD_FAIL( "Lang app is empty." );

    for( uint i = 0, j = (uint) lang.size(); i < j; i++ )
    {
        string& l = lang[ i ];
        lang_buf = fodlg.GetApp( l.c_str() );
        if( !lang_buf )
            LOAD_FAIL( "One of the lang section not found." );
        pack->Texts.push_back( new FOMsg );
        pack->Texts[ i ]->LoadMsgFileBuf( lang_buf, Str::Length( lang_buf ) );
        SAFEDELA( lang_buf );
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
    char read_str[ 256 ];
    bool ret_val;

    #ifdef FONLINE_NPCEDITOR
    char script[ 1024 ];
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
        current_dialog.TextId = text_id;
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
                LOAD_FAIL( "Bad link in dialog." );
            input >> text_id;
            if( input.fail() )
                LOAD_FAIL( "Bad text link in dialog." );
            DialogAnswer current_answer;
            current_answer.Link = link;
            current_answer.TextId = text_id;

            bool read_demands = true;           // Deprecated
            while( true )
            {
                input >> ch;
                if( input.fail() )
                    LOAD_FAIL( "Parse answer character fail." );

                // Demands, results; Deprecated
                if( ch == 'd' )
                    read_demands = true;
                else if( ch == 'r' )
                    read_demands = false;
                else if( ch == '*' )
                {
                    DemandResult* dr = LoadDemandResult( input, read_demands );
                    if( !dr )
                        LOAD_FAIL( "Demand or result not loaded." );
                    if( read_demands )
                        current_answer.Demands.push_back( *dr );
                    else
                        current_answer.Results.push_back( *dr );
                }
                // Demands
                else if( ch == 'D' )
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
                // Flags
                /*else if(ch=='F')
                   {
                        input >> read_str;
                        if(input.fail()) LOAD_FAIL("Parse flags fail.");
                        if(Str::CompareCase(read_str,"no_recheck_demand") && current_dialog.Answers.size()) current_answer.NoRecheck=true;
                   }*/
                else
                    break;
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
    AddError( "Bad node<%d>.", dlg_id );
    WriteLogF( _FUNC_, " - Errors:\n%s", LastErrors.c_str() );
    delete pack;
    SAFEDELA( dlg_buf );
    SAFEDELA( lang_buf );
    return NULL;
}

DemandResult* DialogManager::LoadDemandResult( istrstream& input, bool is_demand )
{
    int  errors = 0;
    char who = 'p';
    char oper = '=';
    int  values_count = 0;
    char svalue[ 256 ] = { 0 };
    int  ivalue = 0;
    int  id = 0;
    char type_str[ 256 ];
    char name[ 256 ] = { 0 };
    bool no_recheck = false;
    bool ret_value = false;

    #define READ_VALUE    if( !deprecated ) { input >> svalue; ivalue = ConstantsManager::GetDefineValue( svalue ); } else { input >> ivalue; Str::Format( svalue, "%d", ivalue ); }

    #ifdef FONLINE_NPCEDITOR
    string script_val[ 5 ];
    #else
    int    script_val[ 5 ] = { 0, 0, 0, 0, 0 };
    #endif

    input >> type_str;
    if( input.fail() )
    {
        AddError( "Parse dr type fail." );
        return NULL;
    }

    bool deprecated = false;
    int  type = GetDRType( type_str, deprecated );

    if( type == DR_NO_RECHECK )
    {
        no_recheck = true;
        input >> type_str;
        if( input.fail() )
        {
            AddError( "Parse dr type fail2." );
            return NULL;
        }
        type = GetDRType( type_str, deprecated );
    }

    switch( type )
    {
    case DR_PARAM:
    {
        // Who
        if( !Str::CompareCase( type_str, "loy" ) && !Str::CompareCase( type_str, "kill" ) )         // Deprecated
        {
            input >> who;
            if( !CheckWho( who ) )
            {
                AddError( "Invalid DR param who<%c>.", who );
                errors++;
            }
        }

        // Name
        input >> name;
        if( ( id = ConstantsManager::GetParamId( name ) ) < 0 )
        {
            AddError( "Invalid DR parameter<%s>.", name );
            errors++;
        }

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            AddError( "Invalid DR param oper<%c>.", oper );
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
            AddError( "Invalid DR var who<%c>.", who );
            errors++;
        }

        // Name
        input >> name;
        if( ( id = GetTempVarId( name ) ) == 0 )
        {
            AddError( "Invalid DR var name<%s>.", name );
            errors++;
        }

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            AddError( "Invalid DR var oper<%c>.", oper );
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
            AddError( "Invalid DR item who<%c>.", who );
            errors++;
        }

        // Name
        input >> name;
        id = ConstantsManager::GetItemPid( name );
        if( id == -1 )
        {
            id = atoi( name );
            const char* name_ = ConstantsManager::GetItemName( id );
            if( !name_ )
            {
                AddError( "Invalid DR item<%s>.", name );
                errors++;
            }
            if( name_ != 0 )
                Str::Copy( name, name_ );
        }

        // Operator
        input >> oper;
        if( !CheckOper( oper ) )
        {
            AddError( "Invalid DR item oper<%c>.", oper );
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

        // Operator, not used
        if( deprecated && is_demand )
            input >> oper;

        // Values count
        input >> values_count;

        // Values
        if( !deprecated )
        {
            #ifdef FONLINE_NPCEDITOR
            # define READ_SCRIPT_VALUE_( val )         \
                { input >> value_str; val = value_str; \
                }
            #else
            # define READ_SCRIPT_VALUE_( val )       { input >> value_str; val = ConstantsManager::GetDefineValue( value_str ); }
            #endif
            char value_str[ 256 ];
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
                AddError( "Invalid DR script values count<%d>.", values_count );
                values_count = 0;
                errors++;
            }
        }
        else
        {
            char ch = *input.str();
            input.rdbuf()->freeze( false );
            if( ch == ' ' )
            {
                #ifdef FONLINE_NPCEDITOR
                # define READ_SCRIPT_VALUE( val )    { input >> value_int; char buf[ 64 ]; val = itoa( value_int, buf, 10 ); }
                #else
                # define READ_SCRIPT_VALUE( val )    { input >> value_int; val = value_int; }
                #endif
                int value_int;
                if( values_count > 0 )
                    READ_SCRIPT_VALUE( script_val[ 0 ] );
                if( values_count > 1 )
                    READ_SCRIPT_VALUE( script_val[ 1 ] );
                if( values_count > 2 )
                    READ_SCRIPT_VALUE( script_val[ 2 ] );
                if( values_count > 3 )
                    READ_SCRIPT_VALUE( script_val[ 3 ] );
                if( values_count > 4 )
                    READ_SCRIPT_VALUE( script_val[ 4 ] );
                if( values_count > 5 )
                {
                    AddError( "Invalid DR script values count<%d>.", values_count );
                    values_count = 0;
                    errors++;
                }
            }
            else
            {
                #ifdef FONLINE_NPCEDITOR
                char buf[ 64 ];
                script_val[ 0 ] = _itoa( values_count, buf, 10 );
                #else
                script_val[ 0 ] = values_count;
                #endif
                values_count = 1;
            }
        }

        #ifdef FONLINE_SERVER
        // Bind function
        # define BIND_D_FUNC( params )               { id = Script::Bind( name, "bool %s(Critter&,Critter@" params, false ); }
        # define BIND_R_FUNC( params )                                                                                     \
            { if( ( id = Script::Bind( name, "uint %s(Critter&,Critter@" params, false, true ) ) > 0 ) { ret_value = true; \
              } else                                                                                                       \
                  id = Script::Bind( name, "void %s(Critter&,Critter@" params, false ); }
        switch( values_count )
        {
        case 1:
            if( is_demand )
                BIND_D_FUNC( ",int)" ) else
                    BIND_R_FUNC( ",int)" ) break;
        case 2:
            if( is_demand )
                BIND_D_FUNC( ",int,int)" ) else
                    BIND_R_FUNC( ",int,int)" ) break;
        case 3:
            if( is_demand )
                BIND_D_FUNC( ",int,int,int)" ) else
                    BIND_R_FUNC( ",int,int,int)" ) break;
        case 4:
            if( is_demand )
                BIND_D_FUNC( ",int,int,int,int)" ) else
                    BIND_R_FUNC( ",int,int,int,int)" ) break;
        case 5:
            if( is_demand )
                BIND_D_FUNC( ",int,int,int,int,int)" ) else
                    BIND_R_FUNC( ",int,int,int,int,int)" ) break;
        default:
            if( is_demand )
                BIND_D_FUNC( ")" ) else
                    BIND_R_FUNC( ")" ) break;
        }
        if( id <= 0 )
        {
            WriteLogF( _FUNC_, " - Script<%s> bind error.\n", name );
            return NULL;
        }
        if( id > 0xFFFF )
            WriteLogF( _FUNC_, " - Id greater than 0xFFFF.\n" );
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
        AddError( "DR parse fail." );
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

ushort DialogManager::GetTempVarId( const char* str )
{
    ushort tid = VarMngr.GetTemplateVarId( str );
    if( !tid )
        WriteLogF( _FUNC_, " - Template var not found, name<%s>.\n", str );
    return tid;
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

int DialogManager::GetDRType( const char* str, bool& deprecated )
{
    deprecated = false;
    if( !str )
        return DR_NONE;
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
    else if( Str::CompareCase( str, "stat" ) )
    {
        deprecated = true;
        return DR_PARAM;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "skill" ) )
    {
        deprecated = true;
        return DR_PARAM;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "perk" ) )
    {
        deprecated = true;
        return DR_PARAM;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "var" ) )
    {
        deprecated = true;
        return DR_VAR;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "gvar" ) )
    {
        deprecated = true;
        return DR_VAR;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "lvar" ) )
    {
        deprecated = true;
        return DR_VAR;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "uvar" ) )
    {
        deprecated = true;
        return DR_VAR;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "item" ) )
    {
        deprecated = true;
        return DR_ITEM;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "lock" ) )
    {
        deprecated = true;
        return DR_LOCK;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "script" ) )
    {
        deprecated = true;
        return DR_SCRIPT;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "kill" ) )
    {
        deprecated = true;
        return DR_PARAM;
    }                                                                               // Deprecated
    else if( Str::CompareCase( str, "loy" ) )
    {
        deprecated = true;
        return DR_PARAM;
    }                                                                               // Deprecated
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

void DialogManager::AddError( const char* fmt, ... )
{
    static char res[ 1024 ];

    va_list list;
    va_start( list, fmt );
    vsprintf( res, fmt, list );
    va_end( list );

    LastErrors += res;
    LastErrors += "\n";
}
