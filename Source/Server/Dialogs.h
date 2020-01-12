#pragma once

#include "Common.h"
#include "Debugger.h"
#include "MsgFiles.h"

// Dialog flags
#define DIALOG_FLAG_NO_SHUFFLE (1)

// Answers
#define DIALOG_END (0)
#define DIALOG_BACK (0xFFE1)
#define DIALOG_BARTER (0xFFE2)

// Types
#define DR_NONE (0)
#define DR_PROP_GLOBAL (1)
#define DR_PROP_CRITTER (2)
#define DR_PROP_CRITTER_DICT (3)
#define DR_PROP_ITEM (4)
#define DR_PROP_LOCATION (5)
#define DR_PROP_MAP (6)
#define DR_ITEM (7)
#define DR_SCRIPT (8)
#define DR_NO_RECHECK (9)
#define DR_OR (10)

// Who types
#define DR_WHO_NONE (0)
#define DR_WHO_PLAYER (1)
#define DR_WHO_NPC (2)

class DemandResult
{
public:
    char Type; // Type of demand or result
    char Who; // Direction
    max_t ParamId; // Parameter Id
    bool NoRecheck; // Disable demand rechecking
    bool RetValue; // Reserved
    char Op; // Operation
    char ValuesCount; // Script values count

#ifdef FONLINE_NPCEDITOR
    string ValueStr; // Main value string
    string ParamName; // Parameter Name
    string ValuesNames[5]; // Values names
#else
    int Value; // Main value
    int ValueExt[5]; // Extra value
#endif

#ifdef FONLINE_NPCEDITOR
    DemandResult() :
        Type(DR_NONE), Who(DR_WHO_NONE), ParamId(0), NoRecheck(false), RetValue(false), Op(0), ValuesCount(0)
    {
    }
#else
    DemandResult() :
        Type(DR_NONE), Who(DR_WHO_NONE), ParamId(0), NoRecheck(false), RetValue(false), Op(0), Value(0), ValuesCount(0)
    {
        MEMORY_PROCESS(MEMORY_DIALOG, sizeof(DemandResult));
    }
    ~DemandResult() { MEMORY_PROCESS(MEMORY_DIALOG, -(int)sizeof(DemandResult)); }
#endif
};
typedef vector<DemandResult> DemandResultVec;

class DialogAnswer
{
public:
    uint Link;
    uint TextId;
    DemandResultVec Demands;
    DemandResultVec Results;

#ifdef FONLINE_NPCEDITOR
    DialogAnswer() : Link(0), TextId(0) {}
#else
    DialogAnswer() : Link(0), TextId(0) { MEMORY_PROCESS(MEMORY_DIALOG, sizeof(DialogAnswer)); }
    DialogAnswer(const DialogAnswer& r)
    {
        *this = r;
        MEMORY_PROCESS(MEMORY_DIALOG, sizeof(DialogAnswer));
    }
    ~DialogAnswer() { MEMORY_PROCESS(MEMORY_DIALOG, -(int)sizeof(DialogAnswer)); }
#endif
};
typedef vector<DialogAnswer> AnswersVec;

class Dialog
{
public:
    uint Id;
    uint TextId;
    AnswersVec Answers;
    uint Flags;
    bool RetVal;

#ifdef FONLINE_NPCEDITOR
    string DlgScript;
#else
    uint DlgScript;
#endif

    bool IsNoShuffle() { return Flags & DIALOG_FLAG_NO_SHUFFLE; }

    Dialog() : Id(0), TextId(0), Flags(0), RetVal(false)
#ifdef FONLINE_NPCEDITOR
    {
        DlgScript = "None";
    }
#else
    {
        DlgScript = 0;
        MEMORY_PROCESS(MEMORY_DIALOG, sizeof(Dialog));
    }
    Dialog(const Dialog& r)
    {
        *this = r;
        MEMORY_PROCESS(MEMORY_DIALOG, sizeof(Dialog));
    }
    ~Dialog() { MEMORY_PROCESS(MEMORY_DIALOG, -(int)sizeof(Dialog)); }
#endif
    bool operator==(const uint& r) { return Id == r; }
};
typedef vector<Dialog> DialogsVec;

struct DialogPack
{
    hash PackId;
    string PackName;
    DialogsVec Dialogs;
    UIntVec TextsLang;
    FOMsgVec Texts;
    string Comment;
};
typedef map<hash, DialogPack*> DialogPackMap;

struct Talking
{
    int TalkType;
#define TALK_NONE (0)
#define TALK_WITH_NPC (1)
#define TALK_WITH_HEX (2)
    uint TalkNpc;
    uint TalkHexMap;
    ushort TalkHexX, TalkHexY;

    hash DialogPackId;
    Dialog CurDialog;
    uint LastDialogId;
    uint StartTick;
    uint TalkTime;
    bool Barter;
    bool IgnoreDistance;
    string Lexems;
    bool Locked;

    void Clear()
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
    bool LoadDialogs();
    void Finish();
    DialogPack* ParseDialog(const string& pack_name, const string& data);
    bool AddDialog(DialogPack* pack);
    DialogPack* GetDialog(hash pack_id);
    DialogPack* GetDialogByIndex(uint index);
    void EraseDialog(hash pack_id);

private:
    DialogPackMap dialogPacks;
    string lastErrors;

    DemandResult* LoadDemandResult(istringstream& input, bool is_demand);
    bool CheckLockTime(int time);
    uint GetNotAnswerAction(const string& str, bool& ret_val);
    char GetDRType(const string& str);
    char GetWho(char who);
    bool CheckOper(char oper);
};

extern DialogManager DlgMngr;
