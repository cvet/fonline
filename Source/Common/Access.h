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
#define CMD_DISCONCRIT               ( 7 )
#define CMD_TOGLOBAL                 ( 8 )
#define CMD_PROPERTY                 ( 10 )
#define CMD_GETACCESS                ( 11 )
#define CMD_ADDITEM                  ( 12 )
#define CMD_ADDITEM_SELF             ( 14 )
#define CMD_ADDNPC                   ( 15 )
#define CMD_ADDLOCATION              ( 16 )
#define CMD_RELOADSCRIPTS            ( 17 )
#define CMD_RELOAD_CLIENT_SCRIPTS    ( 19 )
#define CMD_RUNSCRIPT                ( 20 )
#define CMD_RELOAD_PROTOS            ( 21 )
#define CMD_REGENMAP                 ( 25 )
#define CMD_RELOADDIALOGS            ( 26 )
#define CMD_LOADDIALOG               ( 27 )
#define CMD_RELOADTEXTS              ( 28 )
#define CMD_SETTIME                  ( 32 )
#define CMD_BAN                      ( 33 )
#define CMD_DELETE_ACCOUNT           ( 34 )
#define CMD_CHANGE_PASSWORD          ( 35 )
#define CMD_LOG                      ( 37 )
#define CMD_DEV_EXEC                 ( 38 )
#define CMD_DEV_FUNC                 ( 39 )
#define CMD_DEV_GVAR                 ( 40 )

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
    { "disconnect", CMD_DISCONCRIT },
    { "toglobal", CMD_TOGLOBAL },
    { "prop", CMD_PROPERTY },
    { "getaccess", CMD_GETACCESS },
    { "additem", CMD_ADDITEM },
    { "additemself", CMD_ADDITEM_SELF },
    { "ais", CMD_ADDITEM_SELF },
    { "addnpc", CMD_ADDNPC },
    { "addloc", CMD_ADDLOCATION },
    { "reloadscripts", CMD_RELOADSCRIPTS },
    { "reloadclientscripts", CMD_RELOAD_CLIENT_SCRIPTS },
    { "rcs", CMD_RELOAD_CLIENT_SCRIPTS },
    { "runscript", CMD_RUNSCRIPT },
    { "run", CMD_RUNSCRIPT },
    { "reloadprotos", CMD_RELOAD_PROTOS },
    { "regenmap", CMD_REGENMAP },
    { "reloaddialogs", CMD_RELOADDIALOGS },
    { "loaddialog", CMD_LOADDIALOG },
    { "reloadtexts", CMD_RELOADTEXTS },
    { "settime", CMD_SETTIME },
    { "ban", CMD_BAN },
    { "deleteself", CMD_DELETE_ACCOUNT },
    { "changepassword", CMD_CHANGE_PASSWORD },
    { "changepass", CMD_CHANGE_PASSWORD },
    { "log", CMD_LOG },
    { "exec", CMD_DEV_EXEC },
    { "func", CMD_DEV_FUNC },
    { "gvar", CMD_DEV_GVAR },
};

inline bool PackCommand( const string& str, BufferManager& buf, LogFunc logcb, const string& name )
{
    string args = _str( str ).trim();
    string cmd_str = args;
    size_t space = cmd_str.find( ' ' );
    if( space != string::npos )
    {
        cmd_str = args.substr( 0, space );
        args.erase( 0, cmd_str.length() );
    }
    istringstream args_str( args );

    uchar         cmd = 0;
    for( uint cur_cmd = 0; cur_cmd < sizeof( cmdlist ) / sizeof( CmdDef ); cur_cmd++ )
        if( _str( cmd_str ).compareIgnoreCase( cmdlist[ cur_cmd ].cmd ) )
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
        if( !( args_str >> type ) )
        {
            logcb( "Invalid arguments. Example: gameinfo type." );
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
        string name;
        if( !( args_str >> name ) )
        {
            logcb( "Invalid arguments. Example: id name." );
            break;
        }
        msg_len += BufferManager::StringLenSize;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name;
    }
    break;
    case CMD_MOVECRIT:
    {
        uint   crid;
        ushort hex_x;
        ushort hex_y;
        if( !( args_str >> crid >> hex_x >> hex_y ) )
        {
            logcb( "Invalid arguments. Example: move crid hx hy." );
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
    case CMD_DISCONCRIT:
    {
        uint crid;
        if( !( args_str >> crid ) )
        {
            logcb( "Invalid arguments. Example: disconnect crid." );
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
    case CMD_PROPERTY:
    {
        uint   crid;
        string property_name;
        int    property_value;
        if( !( args_str >> crid >> property_name >> property_value ) )
        {
            logcb( "Invalid arguments. Example: prop crid prop_name value." );
            break;
        }
        msg_len += sizeof( uint ) + BufferManager::StringLenSize + (uint) property_name.length() + sizeof( int );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << property_name;
        buf << property_value;
    }
    break;
    case CMD_GETACCESS:
    {
        string name_access;
        string pasw_access;
        if( !( args_str >> name_access >> pasw_access ) )
        {
            logcb( "Invalid arguments. Example: getaccess name password." );
            break;
        }
        name_access = _str( name_access ).replace( '*', ' ' );
        pasw_access = _str( pasw_access ).replace( '*', ' ' );
        msg_len += BufferManager::StringLenSize * 2 + (uint) ( name_access.length() + pasw_access.length() );
        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name_access;
        buf << pasw_access;
    }
    break;
    case CMD_ADDITEM:
    {
        ushort hex_x;
        ushort hex_y;
        string proto_name;
        uint   count;
        if( !( args_str >> hex_x >> hex_y >> proto_name >> count ) )
        {
            logcb( "Invalid arguments. Example: additem hx hy name count." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
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
        string proto_name;
        uint   count;
        if( !( args_str >> proto_name >> count ) )
        {
            logcb( "Invalid arguments. Example: additemself name count." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
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
        string proto_name;
        if( !( args_str >> hex_x >> hex_y >> dir >> proto_name ) )
        {
            logcb( "Invalid arguments. Example: addnpc hx hy dir name." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
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
        string proto_name;
        if( !( args_str >> wx >> wy >> proto_name ) )
        {
            logcb( "Invalid arguments. Example: addloc wx wy name." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
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
    case CMD_RELOAD_CLIENT_SCRIPTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RUNSCRIPT:
    {
        string func_name;
        uint   param0, param1, param2;
        if( !( args_str >> func_name >> param0 >> param1 >> param2 ) )
        {
            logcb( "Invalid arguments. Example: runscript module::func param0 param1 param2." );
            break;
        }
        msg_len += BufferManager::StringLenSize + (uint) func_name.length() + sizeof( uint ) * 3;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << func_name;
        buf << param0;
        buf << param1;
        buf << param2;
    }
    break;
    case CMD_RELOAD_PROTOS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
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
        string dlg_name;
        if( !( args_str >> dlg_name ) )
        {
            logcb( "Invalid arguments. Example: loaddialog name." );
            break;
        }
        msg_len += BufferManager::StringLenSize + (uint) dlg_name.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << dlg_name;
    }
    break;
    case CMD_RELOADTEXTS:
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
        if( !( args_str >> multiplier >> year >> month >> day >> hour >> minute >> second ) )
        {
            logcb( "Invalid arguments. Example: settime tmul year month day hour minute second." );
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
        string params;
        string name;
        uint   ban_hours;
        string info;
        args_str >> params;
        if( !args_str.fail() )
            args_str >> name;
        if( !args_str.fail() )
            args_str >> ban_hours;
        if( !args_str.fail() )
            info = args_str.str();
        if( !_str( params ).compareIgnoreCase( "add" ) && !_str( params ).compareIgnoreCase( "add+" ) &&
            !_str( params ).compareIgnoreCase( "delete" ) && !_str( params ).compareIgnoreCase( "list" ) )
        {
            logcb( "Invalid arguments. Example: ban [add,add+,delete,list] [user] [hours] [comment]." );
            break;
        }
        name = _str( name ).replace( '*', ' ' ).trim();
        info = _str( info ).replace( '$', '*' ).trim();
        msg_len += BufferManager::StringLenSize * 3 + (uint) ( name.length() + params.length() + info.length() ) + sizeof( ban_hours );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name;
        buf << params;
        buf << ban_hours;
        buf << info;
    }
    break;
    case CMD_DELETE_ACCOUNT:
    {
        if( name.empty() )
        {
            logcb( "Can't execute this command." );
            break;
        }

        string pass;
        if( !( args_str >> pass ) )
        {
            logcb( "Invalid arguments. Example: deleteself user_password." );
            break;
        }
        pass = _str( pass ).replace( '*', ' ' );
        string pass_hash = Crypt.ClientPassHash( name, pass );
        msg_len += PASS_HASH_SIZE;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( pass_hash.c_str(), PASS_HASH_SIZE );
    }
    break;
    case CMD_CHANGE_PASSWORD:
    {
        if( name.empty() )
        {
            logcb( "Can't execute this command." );
            break;
        }

        string pass;
        string new_pass;
        if( !( args_str >> pass >> new_pass ) )
        {
            logcb( "Invalid arguments. Example: changepassword current_password new_password." );
            break;
        }
        pass = _str( pass ).replace( '*', ' ' );

        // Check the new password's validity
        uint pass_len = _str( new_pass ).lengthUtf8();
        if( pass_len < MIN_NAME || pass_len < GameOpt.MinNameLength || pass_len > MAX_NAME || pass_len > GameOpt.MaxNameLength )
        {
            logcb( "Invalid new password." );
            break;
        }

        string pass_hash = Crypt.ClientPassHash( name, pass );
        new_pass = _str( new_pass ).replace( '*', ' ' );
        string new_pass_hash = Crypt.ClientPassHash( name, new_pass );
        msg_len += PASS_HASH_SIZE * 2;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( pass_hash.c_str(), PASS_HASH_SIZE );
        buf.Push( new_pass_hash.c_str(), PASS_HASH_SIZE );
    }
    break;
    case CMD_LOG:
    {
        string flags;
        if( !( args_str >> flags ) )
        {
            logcb( "Invalid arguments. Example: log flag. Valid flags: '+' attach, '-' detach, '--' detach all." );
            break;
        }
        msg_len += BufferManager::StringLenSize + (uint) flags.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << flags;
    }
    break;
    case CMD_DEV_EXEC:
    case CMD_DEV_FUNC:
    case CMD_DEV_GVAR:
    {
        if( args.empty() )
            break;

        msg_len += BufferManager::StringLenSize + (uint) args.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << args;
    }
    break;
    default:
        return false;
    }

    return true;
}

#endif // __ACCESS__
