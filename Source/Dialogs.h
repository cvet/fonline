#ifndef __DIALOGS__
#define __DIALOGS__

#include "Common.h"
#include "Vars.h"
#include "Text.h"
#include "MsgFiles.h"

// Special script
#define NOT_ANSWER_CLOSE_DIALOG    ( 0 )
#define NOT_ANSWER_BEGIN_BATTLE    ( 1 )

// Dialog flags
#define DIALOG_FLAG_NO_SHUFFLE     ( 1 )

// Answers
#define DIALOG_END                 ( 0 )
#define DIALOG_BACK                ( 0xFFE1 )
#define DIALOG_BARTER              ( 0xFFE2 )
#define DIALOG_ATTACK              ( 0xFFE3 )

// Types
#define DR_NONE                    ( 0 )
#define DR_PARAM                   ( 1 )
#define DR_ITEM                    ( 2 )
#define DR_VAR                     ( 3 )
#define DR_SCRIPT                  ( 4 )
#define DR_LOCK                    ( 5 )
#define DR_NO_RECHECK              ( 6 )
#define DR_OR                      ( 7 )

class DemandResult
{
public:
    char Type;                  // Type of demand or result
    char Who;                   // Direction ('p' - player, 'n' - npc)
    uint ParamId;               // Parameter Id
    bool NoRecheck;             // Disable demand rechecking
    bool RetValue;              // Reserved
    char Op;                    // Operation
    char ValuesCount;           // Script values count

    #ifdef FONLINE_NPCEDITOR
    string ValueStr;            // Main value string
    string ParamName;           // Parameter Name
    string ValuesNames[ 5 ];    // Values names
    #else
    int    Value;               // Main value
    int    ValueExt[ 5 ];       // Extra value
    #endif

    #ifdef FONLINE_NPCEDITOR
    DemandResult(): Type( DR_NONE ), Who( 'p' ), ParamId( 0 ), NoRecheck( false ), RetValue( false ), Op( 0 ), ValuesCount( 0 ) {}
    #else
    DemandResult(): Type( DR_NONE ), Who( 'p' ), ParamId( 0 ), NoRecheck( false ), RetValue( false ), Op( 0 ), Value( 0 ), ValuesCount( 0 ) { MEMORY_PROCESS( MEMORY_DIALOG, sizeof( DemandResult ) ); }
    DemandResult( const DemandResult& r )
    {
        *this = r;
        MEMORY_PROCESS( MEMORY_DIALOG, sizeof( DemandResult ) );
    }
    ~DemandResult() { MEMORY_PROCESS( MEMORY_DIALOG, -(int) sizeof( DemandResult ) ); }
    #endif
};
typedef vector< DemandResult > DemandResultVec;

class DialogAnswer
{
public:
    uint            Link;
    uint            TextId;
    DemandResultVec Demands;
    DemandResultVec Results;

    #ifdef FONLINE_NPCEDITOR
    DialogAnswer(): Link( 0 ), TextId( 0 ) {}
    #else
    DialogAnswer(): Link( 0 ), TextId( 0 ) { MEMORY_PROCESS( MEMORY_DIALOG, sizeof( DialogAnswer ) ); }
    DialogAnswer( const DialogAnswer& r )
    {
        *this = r;
        MEMORY_PROCESS( MEMORY_DIALOG, sizeof( DialogAnswer ) );
    }
    ~DialogAnswer() { MEMORY_PROCESS( MEMORY_DIALOG, -(int) sizeof( DialogAnswer ) ); }
    #endif
};
typedef vector< DialogAnswer > AnswersVec;

class Dialog
{
public:
    uint       Id;
    uint       TextId;
    AnswersVec Answers;
    uint       Flags;
    bool       RetVal;

    #ifdef FONLINE_NPCEDITOR
    string DlgScript;
    #else
    int    DlgScript;
    #endif

    bool IsNoShuffle() { return Flags & DIALOG_FLAG_NO_SHUFFLE; }

    Dialog(): Id( 0 ), TextId( 0 ), Flags( 0 ), RetVal( false )
              #ifdef FONLINE_NPCEDITOR
    { DlgScript = "None"; }
              #else
    {
        DlgScript = NOT_ANSWER_CLOSE_DIALOG;
        MEMORY_PROCESS( MEMORY_DIALOG, sizeof( Dialog ) );
    }
    Dialog( const Dialog& r )
    {
        *this = r;
        MEMORY_PROCESS( MEMORY_DIALOG, sizeof( Dialog ) );
    }
    ~Dialog() { MEMORY_PROCESS( MEMORY_DIALOG, -(int) sizeof( Dialog ) ); }
    #endif
    bool operator==( const uint& r ) { return Id == r; }
};
typedef vector< Dialog > DialogsVec;

struct DialogPack
{
    uint       PackId;
    string     PackName;
    DialogsVec Dialogs;
    StrVec     TextsLang;
    FOMsgVec   Texts;
    string     Comment;
};
typedef map< uint, DialogPack* > DialogPackMap;

struct Talking
{
    int    TalkType;
    #define TALK_NONE        ( 0 )
    #define TALK_WITH_NPC    ( 1 )
    #define TALK_WITH_HEX    ( 2 )
    uint   TalkNpc;
    uint   TalkHexMap;
    ushort TalkHexX, TalkHexY;

    uint   DialogPackId;
    Dialog CurDialog;
    uint   LastDialogId;
    uint   StartTick;
    uint   TalkTime;
    bool   Barter;
    bool   IgnoreDistance;
    string Lexems;
    bool   Locked;

    void   Clear()
    {
        TalkType = TALK_NONE;
        TalkNpc = 0;
        TalkHexMap = 0;
        TalkHexX = 0;
        TalkHexY = 0;
        DialogPackId = 0;
        LastDialogId = 0;
        StartTick = 0;
        TalkTime = 0;
        Barter = false;
        IgnoreDistance = false;
        Lexems = "";
        Locked = false;
    }
};

class DialogManager
{
public:
    bool        LoadDialogs();
    void        Finish();
    DialogPack* ParseDialog( const char* pack_name, const char* data );
    bool        AddDialog( DialogPack* pack );
    DialogPack* GetDialog( uint pack_id );
    DialogPack* GetDialogByIndex( uint index );
    void        EraseDialog( uint pack_id );
    ushort      GetTempVarId( const char* str );

private:
    DialogPackMap dialogPacks;
    string        lastErrors;

    DemandResult* LoadDemandResult( istrstream& input, bool is_demand );
    bool          CheckLockTime( int time );
    int           GetNotAnswerAction( const char* str, bool& ret_val );
    int           GetDRType( const char* str, bool& deprecated );
    bool          CheckOper( char oper );
    bool          CheckWho( char who );
};

extern DialogManager DlgMngr;

#endif // __DIALOGS__
