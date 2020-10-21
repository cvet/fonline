//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "MsgFiles.h"
#include "ServerScripting.h"

enum class TalkType
{
    None,
    Npc,
    Hex,
};

// Answers
static constexpr ushort DIALOG_END = 0;
static constexpr ushort DIALOG_BACK = 0xFFE1;
static constexpr ushort DIALOG_BARTER = 0xFFE2;

// Types
static constexpr uchar DR_NONE = 0;
static constexpr uchar DR_PROP_GLOBAL = 1;
static constexpr uchar DR_PROP_CRITTER = 2;
static constexpr uchar DR_PROP_CRITTER_DICT = 3;
static constexpr uchar DR_PROP_ITEM = 4;
static constexpr uchar DR_PROP_LOCATION = 5;
static constexpr uchar DR_PROP_MAP = 6;
static constexpr uchar DR_ITEM = 7;
static constexpr uchar DR_SCRIPT = 8;
static constexpr uchar DR_NO_RECHECK = 9;
static constexpr uchar DR_OR = 10;

// Who types
static constexpr uchar DR_WHO_NONE = 0;
static constexpr uchar DR_WHO_PLAYER = 1;
static constexpr uchar DR_WHO_NPC = 2;

struct DemandResult
{
    uchar Type {DR_NONE};
    uchar Who {DR_WHO_NONE};
    max_t ParamId {};
    bool NoRecheck {};
    bool RetValue {};
    char Op {};
    uchar ValuesCount {};
    int Value {};
    int ValueExt[5] {};
};
using DemandResultVec = vector<DemandResult>;

struct DialogAnswer
{
    uint Link {};
    uint TextId {};
    DemandResultVec Demands {};
    DemandResultVec Results {};
};
using AnswersVec = vector<DialogAnswer>;

struct Dialog
{
    uint Id {};
    uint TextId {};
    AnswersVec Answers {};
    bool NoShuffle {};
    ScriptFunc<string, Critter*, Critter*> DlgScriptFunc {};
};
using DialogsVec = vector<Dialog>;

struct DialogPack
{
    hash PackId {};
    string PackName {};
    DialogsVec Dialogs {};
    vector<uint> TextsLang {};
    vector<FOMsg*> Texts {};
    string Comment {};
};

struct TalkData
{
    TalkType Type {};
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

class DialogManager final
{
public:
    DialogManager() = delete;
    DialogManager(FileManager& file_mngr, ServerScriptSystem& script_sys);
    DialogManager(const DialogManager&) = delete;
    DialogManager(DialogManager&&) noexcept = default;
    auto operator=(const DialogManager&) = delete;
    auto operator=(DialogManager&&) noexcept = delete;
    ~DialogManager() = default;

    [[nodiscard]] auto GetDialog(hash pack_id) -> DialogPack*;
    [[nodiscard]] auto GetDialogByIndex(uint index) -> DialogPack*;

    [[nodiscard]] auto LoadDialogs() -> bool;
    [[nodiscard]] auto ParseDialog(const string& pack_name, const string& data) -> DialogPack*;
    [[nodiscard]] auto AddDialog(DialogPack* pack) -> bool;

    void EraseDialog(hash pack_id);

private:
    [[nodiscard]] auto GetNotAnswerAction(const string& str) -> ScriptFunc<string, Critter*, Critter*>;
    [[nodiscard]] auto GetDrType(const string& str) -> uchar;
    [[nodiscard]] auto GetWho(char who) -> uchar;
    [[nodiscard]] auto CheckOper(char oper) -> bool;

    [[nodiscard]] auto LoadDemandResult(istringstream& input, bool is_demand) -> DemandResult*;

    FileManager& _fileMngr;
    ServerScriptSystem& _scriptSys;
    map<hash, unique_ptr<DialogPack>> _dialogPacks {};
    bool _nonConstHelper {};
};
