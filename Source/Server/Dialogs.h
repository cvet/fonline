#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "MsgFiles.h"

#define TALK_NONE (0)
#define TALK_WITH_NPC (1)
#define TALK_WITH_HEX (2)

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
    char Type {DR_NONE};
    char Who {DR_WHO_NONE};
    max_t ParamId {};
    bool NoRecheck {};
    bool RetValue {};
    char Op {};
    char ValuesCount {};
    int Value {};
    int ValueExt[5] {};
};
using DemandResultVec = vector<DemandResult>;

class DialogAnswer
{
public:
    uint Link {};
    uint TextId {};
    DemandResultVec Demands {};
    DemandResultVec Results {};
};
using AnswersVec = vector<DialogAnswer>;

class Dialog
{
public:
    bool operator==(uint id) { return id == Id; }
    bool IsNoShuffle() { return Flags & DIALOG_FLAG_NO_SHUFFLE; }

    uint Id {};
    uint TextId {};
    AnswersVec Answers {};
    uint Flags {};
    bool RetVal {};
    uint DlgScript {};
};
using DialogsVec = vector<Dialog>;

struct DialogPack : public NonCopyable
{
    hash PackId {};
    string PackName {};
    DialogsVec Dialogs {};
    UIntVec TextsLang {};
    FOMsgVec Texts {};
    string Comment {};
};

struct Talking : public NonCopyable
{
    int TalkType {TALK_NONE};
    uint TalkNpc {};
    uint TalkHexMap {};
    ushort TalkHexX {};
    ushort TalkHexY {};
    hash DialogPackId {};
    Dialog CurDialog {};
    uint LastDialogId {};
    uint StartTick {};
    uint TalkTime {};
    bool Barter {};
    bool IgnoreDistance {};
    string Lexems {};
    bool Locked {};
};

class DialogManager : public NonCopyable
{
public:
    bool LoadDialogs(FileManager& file_mngr);
    DialogPack* ParseDialog(const string& pack_name, const string& data);
    bool AddDialog(DialogPack* pack);
    DialogPack* GetDialog(hash pack_id);
    DialogPack* GetDialogByIndex(uint index);
    void EraseDialog(hash pack_id);

private:
    DemandResult* LoadDemandResult(istringstream& input, bool is_demand);
    uint GetNotAnswerAction(const string& str, bool& ret_val);
    char GetDRType(const string& str);
    char GetWho(char who);
    bool CheckOper(char oper);

    map<hash, unique_ptr<DialogPack>> dialogPacks {};
};
