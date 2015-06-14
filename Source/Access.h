#ifndef __ACCESS__
#define __ACCESS__

#define ACCESS_CLIENT                ( 0 )
#define ACCESS_TESTER                ( 1 )
#define ACCESS_MODER                 ( 2 )
#define ACCESS_ADMIN                 ( 3 )

#ifdef DEV_VERSION
# define ACCESS_DEFAULT              ACCESS_ADMIN
#else
# define ACCESS_DEFAULT              ACCESS_CLIENT
#endif

#define CMD_EXIT                     ( 1 )
#define CMD_MYINFO                   ( 2 )
#define CMD_GAMEINFO                 ( 3 )
#define CMD_CRITID                   ( 4 )
#define CMD_MOVECRIT                 ( 5 )
#define CMD_KILLCRIT                 ( 6 )
#define CMD_DISCONCRIT               ( 7 )
#define CMD_TOGLOBAL                 ( 8 )
#define CMD_RESPAWN                  ( 9 )
#define CMD_PROPERTY                 ( 10 )
#define CMD_GETACCESS                ( 11 )
#define CMD_ADDITEM                  ( 12 )
#define CMD_ADDITEM_SELF             ( 14 )
#define CMD_ADDNPC                   ( 15 )
#define CMD_ADDLOCATION              ( 16 )
#define CMD_RELOADSCRIPTS            ( 17 )
#define CMD_LOADSCRIPT               ( 18 )
#define CMD_RELOAD_CLIENT_SCRIPTS    ( 19 )
#define CMD_RUNSCRIPT                ( 20 )
#define CMD_RELOADLOCATIONS          ( 21 )
#define CMD_LOADLOCATION             ( 22 )
#define CMD_RELOADMAPS               ( 23 )
#define CMD_LOADMAP                  ( 24 )
#define CMD_REGENMAP                 ( 25 )
#define CMD_RELOADDIALOGS            ( 26 )
#define CMD_LOADDIALOG               ( 27 )
#define CMD_RELOADTEXTS              ( 28 )
#define CMD_RELOADAI                 ( 29 )
#define CMD_SETTIME                  ( 32 )
#define CMD_BAN                      ( 33 )
#define CMD_DELETE_ACCOUNT           ( 34 )
#define CMD_CHANGE_PASSWORD          ( 35 )
#define CMD_DROP_UID                 ( 36 )
#define CMD_LOG                      ( 37 )
#define CMD_DEV_CONSOLE              ( 38 )

struct CmdDef
{
    char  cmd[ 20 ];
    uchar id;
};

const CmdDef cmdlist[] =
{
    { "exit", CMD_EXIT },
    { "myinfo", CMD_MYINFO },
    { "gameinfo", CMD_GAMEINFO },
    { "id", CMD_CRITID },
    { "move", CMD_MOVECRIT },
    { "kill", CMD_KILLCRIT },
    { "disconnect", CMD_DISCONCRIT },
    { "toglobal", CMD_TOGLOBAL },
    { "respawn", CMD_RESPAWN },
    { "prop", CMD_PROPERTY },
    { "getaccess", CMD_GETACCESS },
    { "additem", CMD_ADDITEM },
    { "additemself", CMD_ADDITEM_SELF },
    { "ais", CMD_ADDITEM_SELF },
    { "addnpc", CMD_ADDNPC },
    { "addloc", CMD_ADDLOCATION },
    { "reloadscripts", CMD_RELOADSCRIPTS },
    { "loadscript", CMD_LOADSCRIPT },
    { "load", CMD_LOADSCRIPT },
    { "reloadclientscripts", CMD_RELOAD_CLIENT_SCRIPTS },
    { "rcs", CMD_RELOAD_CLIENT_SCRIPTS },
    { "runscript", CMD_RUNSCRIPT },
    { "run", CMD_RUNSCRIPT },
    { "reloadlocations", CMD_RELOADLOCATIONS },
    { "loadlocation", CMD_LOADLOCATION },
    { "reloadmaps", CMD_RELOADMAPS },
    { "loadmap", CMD_LOADMAP },
    { "regenmap", CMD_REGENMAP },
    { "reloaddialogs", CMD_RELOADDIALOGS },
    { "loaddialog", CMD_LOADDIALOG },
    { "reloadtexts", CMD_RELOADTEXTS },
    { "reloadai", CMD_RELOADAI },
    { "settime", CMD_SETTIME },
    { "ban", CMD_BAN },
    { "deleteself", CMD_DELETE_ACCOUNT },
    { "changepassword", CMD_CHANGE_PASSWORD },
    { "changepass", CMD_CHANGE_PASSWORD },
    { "dropuid", CMD_DROP_UID },
    { "drop", CMD_DROP_UID },
    { "log", CMD_LOG },
    { "console", CMD_DEV_CONSOLE },
};

inline bool PackCommand( const char* str, BufferManager& buf, void ( * logcb )( const char* ), const char* name )
{
    char args[ MAX_FOTEXT ];
    Str::Copy( args, str );
    Str::Trim( args );

    char  cmd_str[ MAX_FOTEXT ];
    Str::Copy( cmd_str, args );
    char* space = Str::Substring( cmd_str, " " );
    if( space )
    {
        *space = 0;
        Str::EraseInterval( args, Str::Length( cmd_str ) );
    }

    uchar cmd = 0;
    for( uint cur_cmd = 0; cur_cmd < sizeof( cmdlist ) / sizeof( CmdDef ); cur_cmd++ )
        if( Str::CompareCase( cmd_str, cmdlist[ cur_cmd ].cmd ) )
            cmd = cmdlist[ cur_cmd ].id;
    if( !cmd )
        return false;

    uint msg = NETMSG_SEND_COMMAND;
    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( cmd );

    switch( cmd )
    {
    case CMD_EXIT:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_MYINFO:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_GAMEINFO:
    {
        int type;
        if( sscanf( args, "%d", &type ) != 1 )
        {
            logcb( "Invalid arguments. Example: <~gameinfo type>." );
            break;
        }
        msg_len += sizeof( type );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << type;
    }
    break;
    case CMD_CRITID:
    {
        char name[ MAX_FOTEXT ];
        if( sscanf( args, "%s", name ) != 1 )
        {
            logcb( "Invalid arguments. Example: <id name>." );
            break;
        }
        uint name_size_utf8 = UTF8_BUF_SIZE( MAX_NAME );
        name[ name_size_utf8 - 1 ] = 0;
        msg_len += name_size_utf8;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( name, name_size_utf8 );
    }
    break;
    case CMD_MOVECRIT:
    {
        uint   crid;
        ushort hex_x;
        ushort hex_y;
        if( sscanf( args, "%u%hu%hu", &crid, &hex_x, &hex_y ) != 3 )
        {
            logcb( "Invalid arguments. Example: <move crid hx hy>." );
            break;
        }
        msg_len += sizeof( crid ) + sizeof( hex_x ) + sizeof( hex_y );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << hex_x;
        buf << hex_y;
    }
    break;
    case CMD_KILLCRIT:
    {
        uint crid;
        if( sscanf( args, "%u", &crid ) != 1 )
        {
            logcb( "Invalid arguments. Example: <kill crid>." );
            break;
        }
        msg_len += sizeof( crid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
    }
    break;
    case CMD_DISCONCRIT:
    {
        uint crid;
        if( sscanf( args, "%u", &crid ) != 1 )
        {
            logcb( "Invalid arguments. Example: <disconnect crid>." );
            break;
        }
        msg_len += sizeof( crid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
    }
    break;
    case CMD_TOGLOBAL:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RESPAWN:
    {
        uint crid;
        if( sscanf( args, "%u", &crid ) != 1 )
        {
            logcb( "Invalid arguments. Example: <respawn crid>." );
            break;
        }
        msg_len += sizeof( crid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
    }
    break;
    case CMD_PROPERTY:
    {
        uint crid;
        char property_name[ 256 ];
        int  property_value;
        if( sscanf( args, "%u%s%d", &crid, property_name, &property_value ) != 3 )
        {
            logcb( "Invalid arguments. Example: <prop crid prop_name value>." );
            break;
        }
        msg_len += sizeof( uint ) + sizeof( property_name ) + sizeof( int );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf.Push( property_name, sizeof( property_name ) );
        buf << property_value;
    }
    break;
    case CMD_GETACCESS:
    {
        char name_access[ MAX_FOTEXT ];
        char pasw_access[ MAX_FOTEXT ];
        if( sscanf( args, "%s%s", name_access, pasw_access ) != 2 )
        {
            logcb( "Invalid arguments. Example: <getaccess name password>." );
            break;
        }
        Str::Replacement( name_access, '*', ' ' );
        Str::Replacement( pasw_access, '*', ' ' );

        uint name_size_utf8 = UTF8_BUF_SIZE( MAX_NAME );
        uint pasw_size_utf8 = UTF8_BUF_SIZE( 128 );
        msg_len += name_size_utf8 + pasw_size_utf8;
        name_access[ name_size_utf8 - 1 ] = 0;
        pasw_access[ pasw_size_utf8 - 1 ] = 0;

        buf << msg;
        buf << msg_len;
        buf << cmd;

        buf.Push( name_access, name_size_utf8 );
        buf.Push( pasw_access, pasw_size_utf8 );
    }
    break;
    case CMD_ADDITEM:
    {
        ushort hex_x;
        ushort hex_y;
        ushort pid;
        uint   count;
        if( sscanf( args, "%hu%hu%hu%u", &hex_x, &hex_y, &pid, &count ) != 4 )
        {
            logcb( "Invalid arguments. Example: <additem hx hy pid count>." );
            break;
        }
        msg_len += sizeof( hex_x ) + sizeof( hex_y ) + sizeof( pid ) + sizeof( count );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << pid;
        buf << count;
    }
    break;
    case CMD_ADDITEM_SELF:
    {
        ushort pid;
        uint   count;
        if( sscanf( args, "%hu%u", &pid, &count ) != 2 )
        {
            logcb( "Invalid arguments. Example: <additemself pid count>." );
            break;
        }
        msg_len += sizeof( pid ) + sizeof( count );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << pid;
        buf << count;
    }
    break;
    case CMD_ADDNPC:
    {
        ushort hex_x;
        ushort hex_y;
        uchar  dir;
        ushort pid;
        if( sscanf( args, "%hd%hd%hhd%hd", &hex_x, &hex_y, &dir, &pid ) != 4 )
        {
            logcb( "Invalid arguments. Example: <addnpc hx hy dir pid>." );
            break;
        }
        msg_len += sizeof( hex_x ) + sizeof( hex_y ) + sizeof( dir ) + sizeof( pid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << dir;
        buf << pid;
    }
    break;
    case CMD_ADDLOCATION:
    {
        ushort wx;
        ushort wy;
        ushort pid;
        if( sscanf( args, "%hd%hd%hd", &wx, &wy, &pid ) != 3 )
        {
            logcb( "Invalid arguments. Example: <addloc wx wy pid>." );
            break;
        }
        msg_len += sizeof( wx ) + sizeof( wy ) + sizeof( pid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << wx;
        buf << wy;
        buf << pid;
    }
    break;
    case CMD_RELOADSCRIPTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOADSCRIPT:
    {
        char script_name[ MAX_SCRIPT_NAME + 1 ];
        if( sscanf( args, "%s", script_name ) != 1 )
        {
            logcb( "Invalid arguments. Example: <loadscript name>." );
            break;
        }
        script_name[ MAX_SCRIPT_NAME ] = 0;
        msg_len += MAX_SCRIPT_NAME;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( script_name, MAX_SCRIPT_NAME );
    }
    break;
    case CMD_RELOAD_CLIENT_SCRIPTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RUNSCRIPT:
    {
        char script_name[ MAX_SCRIPT_NAME + 1 ];
        char func_name[ MAX_SCRIPT_NAME + 1 ];
        uint param0, param1, param2;
        if( sscanf( args, "%s%s%d%d%d", script_name, func_name, &param0, &param1, &param2 ) != 5 )
        {
            logcb( "Invalid arguments. Example: <runscript module func param0 param1 param2>." );
            break;
        }
        script_name[ MAX_SCRIPT_NAME ] = 0;
        func_name[ MAX_SCRIPT_NAME ] = 0;
        msg_len += MAX_SCRIPT_NAME * 2 + sizeof( uint ) * 3;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( script_name, MAX_SCRIPT_NAME );
        buf.Push( func_name, MAX_SCRIPT_NAME );
        buf << param0;
        buf << param1;
        buf << param2;
    }
    break;
    case CMD_RELOADLOCATIONS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOADLOCATION:
    {
        char loc_name[ MAX_FOPATH ];
        if( sscanf( args, "%s", loc_name ) != 1 )
        {
            logcb( "Invalid arguments. Example: <loadlocation loc_name>." );
            break;
        }
        msg_len += sizeof( loc_name );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( loc_name, sizeof( loc_name ) );
    }
    break;
    case CMD_RELOADMAPS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOADMAP:
    {
        char map_name[ MAX_FOPATH ];
        if( sscanf( args, "%s", map_name ) != 1 )
        {
            logcb( "Invalid arguments. Example: <loadmap map_name>." );
            break;
        }
        msg_len += sizeof( map_name );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( map_name, sizeof( map_name ) );
    }
    break;
    case CMD_REGENMAP:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RELOADDIALOGS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOADDIALOG:
    {
        char dlg_name[ 128 ];
        if( sscanf( args, "%s", dlg_name ) != 1 )
        {
            logcb( "Invalid arguments. Example: <loaddialog name>." );
            break;
        }
        dlg_name[ 127 ] = 0;
        msg_len += 128;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( dlg_name, 128 );
    }
    break;
    case CMD_RELOADTEXTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RELOADAI:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_SETTIME:
    {
        int multiplier;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        if( sscanf( args, "%d%d%d%d%d%d%d", &multiplier, &year, &month, &day, &hour, &minute, &second ) != 7 )
        {
            logcb( "Invalid arguments. Example: <settime tmul year month day hour minute second>." );
            break;
        }
        msg_len += sizeof( multiplier ) + sizeof( year ) + sizeof( month ) + sizeof( day ) + sizeof( hour ) + sizeof( minute ) + sizeof( second );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << multiplier;
        buf << year;
        buf << month;
        buf << day;
        buf << hour;
        buf << minute;
        buf << second;
    }
    break;
    case CMD_BAN:
    {
        istrstream str_( args );
        char       params[ 128 ] = { 0 };
        char       name[ 128 ] = { 0 };
        uint       ban_hours = 0;
        char       info[ 128 ] = { 0 };
        str_ >> params;
        if( !str_.fail() )
            str_ >> name;
        if( !str_.fail() )
            str_ >> ban_hours;
        if( !str_.fail() )
            Str::Copy( info, str_.str() );
        if( !Str::CompareCase( params, "add" ) && !Str::CompareCase( params, "add+" ) && !Str::CompareCase( params, "delete" ) && !Str::CompareCase( params, "list" ) )
        {
            logcb( "Invalid arguments. Example: <ban [add,add+,delete,list] [user] [hours] [comment]>." );
            break;
        }
        Str::Replacement( name, '*', ' ' );
        Str::Replacement( info, '$', '*' );
        while( info[ 0 ] == ' ' )
            Str::CopyBack( info );

        uint name_size_utf8 = UTF8_BUF_SIZE( MAX_NAME );
        uint params_size_utf8 = UTF8_BUF_SIZE( MAX_NAME );
        uint info_size_utf8 = UTF8_BUF_SIZE( MAX_CHAT_MESSAGE );
        name[ name_size_utf8 - 1 ] = 0;
        params[ params_size_utf8 - 1 ] = 0;
        info[ info_size_utf8 - 1 ] = 0;

        msg_len += name_size_utf8 + params_size_utf8 + info_size_utf8 + sizeof( ban_hours );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( name, name_size_utf8 );
        buf.Push( params, params_size_utf8 );
        buf << ban_hours;
        buf.Push( info, info_size_utf8 );
    }
    break;
    case CMD_DELETE_ACCOUNT:
    {
        if( !name )
        {
            logcb( "Can't execute this command." );
            break;
        }

        char pass[ 128 ];
        if( sscanf( args, "%s", pass ) != 1 )
        {
            logcb( "Invalid arguments. Example: <deleteself user_password>." );
            break;
        }
        Str::Replacement( pass, '*', ' ' );
        char pass_hash[ PASS_HASH_SIZE ];
        Crypt.ClientPassHash( name, pass, pass_hash );
        msg_len += PASS_HASH_SIZE;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( pass_hash, PASS_HASH_SIZE );
    }
    break;
    case CMD_CHANGE_PASSWORD:
    {
        if( !name )
        {
            logcb( "Can't execute this command." );
            break;
        }

        char pass[ MAX_FOTEXT ];
        char new_pass[ MAX_FOTEXT ];
        if( sscanf( args, "%s%s", pass, new_pass ) != 2 )
        {
            logcb( "Invalid arguments. Example: <changepassword current_password new_password>." );
            break;
        }
        Str::Replacement( pass, '*', ' ' );

        // Check the new password's validity
        uint pass_len = Str::LengthUTF8( new_pass );
        if( pass_len < MIN_NAME || pass_len < GameOpt.MinNameLength || pass_len > MAX_NAME || pass_len > GameOpt.MaxNameLength )
        {
            logcb( "Invalid new password." );
            break;
        }

        char pass_hash[ PASS_HASH_SIZE ];
        Crypt.ClientPassHash( name, pass, pass_hash );
        Str::Replacement( new_pass, '*', ' ' );
        char new_pass_hash[ PASS_HASH_SIZE ];
        Crypt.ClientPassHash( name, new_pass, new_pass_hash );
        msg_len += PASS_HASH_SIZE * 2;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( pass_hash, PASS_HASH_SIZE );
        buf.Push( new_pass_hash, PASS_HASH_SIZE );
    }
    break;
    case CMD_DROP_UID:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOG:
    {
        char flags[ 128 ];
        if( sscanf( args, "%s", flags ) != 1 )
        {
            logcb( "Invalid arguments. Example: <log flag>. Valid flags: '+' attach, '-' detach, '--' detach all." );
            break;
        }
        msg_len += 16;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( flags, 16 );
    }
    break;
    default:
        return false;
    }

    return true;
}

#endif // __ACCESS__
