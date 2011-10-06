#ifndef __ACCESS__
#define __ACCESS__

#define ACCESS_CLIENT                ( 0 )
#define ACCESS_TESTER                ( 1 )
#define ACCESS_MODER                 ( 2 )
#define ACCESS_ADMIN                 ( 3 )

#ifdef DEV_VESRION
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
#define CMD_PARAM                    ( 10 )
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
#define CMD_CHECKVAR                 ( 30 )
#define CMD_SETVAR                   ( 31 )
#define CMD_SETTIME                  ( 32 )
#define CMD_BAN                      ( 33 )
#define CMD_DELETE_ACCOUNT           ( 34 )
#define CMD_CHANGE_PASSWORD          ( 35 )
#define CMD_DROP_UID                 ( 36 )
#define CMD_LOG                      ( 37 )

#endif // __ACCESS__
