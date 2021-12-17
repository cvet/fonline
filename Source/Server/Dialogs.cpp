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

#include "Dialogs.h"
#include "Critter.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Item.h"
#include "Location.h"
#include "Log.h"
#include "Map.h"
#include "Server.h"
#include "ServerEntity.h"
#include "StringUtils.h"

static auto GetPropEnumIndex(FOServer* engine, string_view str, bool is_demand, uchar& type, bool& is_hash) -> int
{
    auto* prop_global = engine->GetPropertyRegistrator("Globals")->Find(str);
    auto* prop_critter = engine->GetPropertyRegistrator("Critter")->Find(str);
    auto* prop_item = engine->GetPropertyRegistrator("Item")->Find(str);
    auto* prop_location = engine->GetPropertyRegistrator("Location")->Find(str);
    auto* prop_map = engine->GetPropertyRegistrator("Map")->Find(str);

    auto count = 0;
    count += prop_global != nullptr ? 1 : 0;
    count += prop_critter != nullptr ? 1 : 0;
    count += prop_item != nullptr ? 1 : 0;
    count += prop_location != nullptr ? 1 : 0;
    count += prop_map != nullptr ? 1 : 0;

    if (count == 0) {
        WriteLog("DR property '{}' not found in GlobalVars/Critter/Item/Location/Map.\n", str);
        return -1;
    }
    if (count > 1) {
        WriteLog("DR property '{}' found multiple instances in GlobalVars/Critter/Item/Location/Map.\n", str);
        return -1;
    }

    const Property* prop = nullptr;
    if (prop_global != nullptr) {
        prop = prop_global;
        type = DR_PROP_GLOBAL;
    }
    else if (prop_critter != nullptr) {
        prop = prop_critter;
        type = DR_PROP_CRITTER;
    }
    else if (prop_item != nullptr) {
        prop = prop_item;
        type = DR_PROP_ITEM;
    }
    else if (prop_location != nullptr) {
        prop = prop_location;
        type = DR_PROP_LOCATION;
    }
    else if (prop_map != nullptr) {
        prop = prop_map;
        type = DR_PROP_MAP;
    }

    /*if (type == DR_PROP_CRITTER && prop->IsDict())
    {
        type = DR_PROP_CRITTER_DICT;
        if (prop->GetASObjectType()->GetSubTypeId(0) != asTYPEID_UINT32)
        {
            WriteLog("DR property '{}' Dict must have 'uint' in key.\n", str);
            return -1;
        }
    }
    else
    {
        if (!prop->IsPlainData())
        {
            WriteLog("DR property '{}' is not PlainData type.\n", str);
            return -1;
        }
    }*/

    if (is_demand && !prop->IsReadable()) {
        WriteLog("DR property '{}' is not readable.\n", str);
        return -1;
    }
    if (!is_demand && !prop->IsWritable()) {
        WriteLog("DR property '{}' is not writable.\n", str);
        return -1;
    }

    is_hash = prop->IsHash();
    return prop->GetRegIndex();
}

DialogManager::DialogManager(FOServer* engine) : _engine {engine}
{
}

auto DialogManager::LoadDialogs() -> bool
{
    WriteLog("Load dialogs...\n");

    _dialogPacks.clear();

    auto files = _engine->FileMngr.FilterFiles("fodlg");
    uint files_loaded = 0;
    while (files.MoveNext()) {
        auto file = files.GetCurFile();
        if (!file) {
            WriteLog("Unable to open file '{}'.\n", file.GetName());
            continue;
        }

        string pack_data = file.GetCStr();
        auto* pack = ParseDialog(file.GetName(), pack_data);
        if (pack == nullptr) {
            WriteLog("Unable to parse dialog '{}'.\n", file.GetName());
            continue;
        }

        if (!AddDialog(pack)) {
            WriteLog("Unable to add dialog '{}'.\n", file.GetName());
            continue;
        }

        files_loaded++;
    }

    WriteLog("Load dialogs complete, count {}.\n", files_loaded);
    return files_loaded == files.GetFilesCount();
}

auto DialogManager::AddDialog(DialogPack* pack) -> bool
{
    if (_dialogPacks.count(pack->PackId) != 0u) {
        WriteLog("Dialog '{}' already added.\n", pack->PackName);
        return false;
    }

    const auto pack_id = static_cast<hash>(pack->PackId & DLGID_MASK);
    for (auto& [fst, snd] : _dialogPacks) {
        const auto check_pack_id = static_cast<hash>(fst & DLGID_MASK);
        if (pack_id == check_pack_id) {
            WriteLog("Name hash collision for dialogs '{}' and '{}'.\n", pack->PackName, snd->PackName);
            return false;
        }
    }

    _dialogPacks.insert(std::make_pair(pack->PackId, pack));
    return true;
}

auto DialogManager::GetDialog(hash pack_id) -> DialogPack*
{
    const auto it = _dialogPacks.find(pack_id);
    return it != _dialogPacks.end() ? it->second.get() : nullptr;
}

auto DialogManager::GetDialogByIndex(uint index) -> DialogPack*
{
    auto it = _dialogPacks.begin();
    while (index-- != 0u && it != _dialogPacks.end()) {
        ++it;
    }
    return it != _dialogPacks.end() ? it->second.get() : nullptr;
}

void DialogManager::EraseDialog(hash pack_id)
{
    const auto it = _dialogPacks.find(pack_id);
    if (it != _dialogPacks.end()) {
        _dialogPacks.erase(it);
    }
}

auto DialogManager::ParseDialog(string_view pack_name, string_view data) -> DialogPack*
{
    ConfigFile fodlg {""};
    fodlg.CollectContent();
    fodlg.AppendData(data);

#define LOAD_FAIL(err) \
    { \
        WriteLog("Dialog '{}' - {}\n", pack_name, err); \
        delete pack; \
        return nullptr; \
    }
#define VERIFY_STR_ID(str_id) (uint(str_id) <= ~DLGID_MASK)

    auto* pack = new DialogPack();
    auto dlg_buf = fodlg.GetAppContent("dialog");
    istringstream input(dlg_buf);
    string lang_buf;
    pack->PackId = _str(pack_name).toHash();
    pack->PackName = pack_name;
    vector<string> lang_apps;

    // Comment
    pack->Comment = fodlg.GetAppContent("comment");

    // Texts
    auto lang_key = fodlg.GetStr("data", "lang", "");
    if (lang_key.empty())
        LOAD_FAIL("Lang app not found.")

    // Check dialog pack
    if (pack->PackId <= 0xFFFF)
        LOAD_FAIL("Invalid hash for dialog name.")

    lang_apps = _str(lang_key).split(' ');
    if (lang_apps.empty())
        LOAD_FAIL("Lang app is empty.")

    for (size_t i = 0; i < lang_apps.size(); i++) {
        auto& lang_app = lang_apps[i];
        if (lang_app.size() != 4)
            LOAD_FAIL("Language length not equal 4.")

        lang_buf = fodlg.GetAppContent(lang_app);
        if (lang_buf.empty())
            LOAD_FAIL("One of the lang section not found.")

        FOMsg temp_msg;
        if (!temp_msg.LoadFromString(lang_buf))
            LOAD_FAIL("Load MSG fail.")

        if (temp_msg.GetStrNumUpper(100000000 + ~DLGID_MASK) != 0u)
            LOAD_FAIL("Text have any text with index greather than 4000.")

        pack->TextsLang.push_back(*reinterpret_cast<const uint*>(lang_app.c_str()));
        pack->Texts.push_back(new FOMsg());

        uint str_num = 0;
        while ((str_num = temp_msg.GetStrNumUpper(str_num)) != 0u) {
            const auto count = temp_msg.Count(str_num);
            const auto new_str_num = DLG_STR_ID(pack->PackId, (str_num < 100000000 ? str_num / 10 : str_num - 100000000 + 12000));
            for (uint n = 0; n < count; n++) {
                pack->Texts[i]->AddStr(new_str_num, _str(temp_msg.GetStr(str_num, n)).replace("\n\\[", "\n["));
            }
        }
    }

    // Dialog
    if (dlg_buf.empty())
        LOAD_FAIL("Dialog section not found.")

    char ch = 0;
    input >> ch;
    if (ch != '&') {
        return nullptr;
    }

    uint dlg_id = 0;
    uint text_id = 0;
    uint link = 0;
    string read_str;

    ScriptFunc<string, Critter*, Critter*> script;
    uint flags = 0;

    while (true) {
        input >> dlg_id;
        if (input.eof()) {
            break;
        }
        if (input.fail())
            LOAD_FAIL("Bad dialog id number.")
        input >> text_id;
        if (input.fail())
            LOAD_FAIL("Bad text link.")
        if (!VERIFY_STR_ID(text_id / 10))
            LOAD_FAIL("Invalid text link value.")
        input >> read_str;
        if (input.fail())
            LOAD_FAIL("Bad not answer action.")
        script = GetNotAnswerAction(read_str);
        if (!script) {
            WriteLog("Unable to parse '{}'.\n", read_str);
            LOAD_FAIL("Invalid not answer action.")
        }
        input >> flags;
        if (input.fail())
            LOAD_FAIL("Bad flags.")

        Dialog current_dialog;
        current_dialog.Id = dlg_id;
        current_dialog.TextId = DLG_STR_ID(pack->PackId, text_id / 10);
        current_dialog.DlgScriptFunc = script;
        current_dialog.NoShuffle = ((flags & 1) == 1);

        // Read answers
        input >> ch;
        if (input.fail())
            LOAD_FAIL("Dialog corrupted.")
        if (ch == '@') // End of current dialog node
        {
            pack->Dialogs.push_back(current_dialog);
            continue;
        }
        if (ch == '&') // End of all
        {
            pack->Dialogs.push_back(current_dialog);
            break;
        }
        if (ch != '#')
            LOAD_FAIL("Parse error 0.")

        while (!input.eof()) {
            input >> link;
            if (input.fail())
                LOAD_FAIL("Bad link in answer.")
            input >> text_id;
            if (input.fail())
                LOAD_FAIL("Bad text link in answer.")
            if (!VERIFY_STR_ID(text_id / 10))
                LOAD_FAIL("Invalid text link value in answer.")
            DialogAnswer current_answer;
            current_answer.Link = link;
            current_answer.TextId = DLG_STR_ID(pack->PackId, text_id / 10);

            while (true) {
                input >> ch;
                if (input.fail())
                    LOAD_FAIL("Parse answer character fail.")

                // Demands
                if (ch == 'D') {
                    auto* d = LoadDemandResult(input, true);
                    if (d == nullptr)
                        LOAD_FAIL("Demand not loaded.")

                    current_answer.Demands.push_back(*d);
                }
                // Results
                else if (ch == 'R') {
                    auto* r = LoadDemandResult(input, false);
                    if (r == nullptr)
                        LOAD_FAIL("Result not loaded.")

                    current_answer.Results.push_back(*r);
                }
                else if (ch == '*' || ch == 'd' || ch == 'r') {
                    LOAD_FAIL("Found old token, update dialog file to actual format (resave in version 2.22).")
                }
                else {
                    break;
                }
            }
            current_dialog.Answers.push_back(current_answer);

            if (ch == '#') {
                continue; // Next
            }
            if (ch == '@') {
                break; // End of current dialog node
            }
            if (ch == '&') // End of all
            {
                pack->Dialogs.push_back(current_dialog);
                break;
            }
        }
        pack->Dialogs.push_back(current_dialog);
    }

    return pack;
}

auto DialogManager::LoadDemandResult(istringstream& input, bool is_demand) -> DemandResult*
{
    NON_CONST_METHOD_HINT();

    auto fail = false;
    char who = DR_WHO_PLAYER;
    auto oper = '=';
    uchar values_count = 0u;
    string svalue;
    auto ivalue = 0;
    max_t id = 0;
    string type_str;
    string name;
    auto no_recheck = false;
    const auto ret_value = false;

    int script_val[5] = {0, 0, 0, 0, 0};

    input >> type_str;
    if (input.fail()) {
        WriteLog("Parse DR type fail.\n");
        return nullptr;
    }

    auto type = GetDrType(type_str);
    if (type == DR_NO_RECHECK) {
        no_recheck = true;
        input >> type_str;
        if (input.fail()) {
            WriteLog("Parse DR type fail2.\n");
            return nullptr;
        }
        type = GetDrType(type_str);
    }

    switch (type) {
    case DR_PROP_CRITTER: {
        // Who
        input >> who;
        who = GetWho(who);
        if (who == DR_WHO_NONE) {
            WriteLog("Invalid DR property who '{}'.\n", who);
            fail = true;
        }

        // Name
        input >> name;
        auto is_hash = false;
        id = static_cast<max_t>(GetPropEnumIndex(_engine, name, is_demand, type, is_hash));
        if (id == static_cast<max_t>(-1)) {
            fail = true;
        }

        // Operator
        input >> oper;
        if (!CheckOper(oper)) {
            WriteLog("Invalid DR property oper '{}'.\n", oper);
            fail = true;
        }

        // Value
        input >> svalue;
        if (is_hash) {
            ivalue = static_cast<int>(_str(svalue).toHash());
        }
        else {
            ivalue = GenericUtils::ConvertParamValue(svalue, fail);
        }
    } break;
    case DR_ITEM: {
        // Who
        input >> who;
        who = GetWho(who);
        if (who == DR_WHO_NONE) {
            WriteLog("Invalid DR item who '{}'.\n", who);
            fail = true;
        }

        // Name
        input >> name;
        id = _str(name).toHash();
        // Todo: check item name on DR_ITEM

        // Operator
        input >> oper;
        if (!CheckOper(oper)) {
            WriteLog("Invalid DR item oper '{}'.\n", oper);
            fail = true;
        }

        // Value
        input >> svalue;
        ivalue = GenericUtils::ConvertParamValue(svalue, fail);
    } break;
    case DR_SCRIPT: {
        // Script name
        input >> name;

        // Values count
        input >> values_count;

// Values
#define READ_SCRIPT_VALUE_(val) \
    { \
        input >> value_str; \
        (val) = GenericUtils::ConvertParamValue(value_str, fail); \
    }
        char value_str[1024];
        if (values_count > 0)
            READ_SCRIPT_VALUE_(script_val[0])
        if (values_count > 1)
            READ_SCRIPT_VALUE_(script_val[1])
        if (values_count > 2)
            READ_SCRIPT_VALUE_(script_val[2])
        if (values_count > 3)
            READ_SCRIPT_VALUE_(script_val[3])
        if (values_count > 4)
            READ_SCRIPT_VALUE_(script_val[4])
        if (values_count > 5) {
            WriteLog("Invalid DR script values count {}.\n", values_count);
            values_count = 0;
            fail = true;
        }

        /*// Bind function
#define BIND_D_FUNC(params) \
    do \
    { \
        id = scriptSys.BindByFuncName(name, "bool %s(Critter, Critter" params, false); \
    } while (0)
#define BIND_R_FUNC(params) \
    do \
    { \
        if ((id = scriptSys.BindByFuncName(name, "uint %s(Critter, Critter" params, false, true)) > 0) \
            ret_value = true; \
        else \
            id = scriptSys.BindByFuncName(name, "void %s(Critter, Critter" params, false); \
    } while (0)
        switch (values_count)
        {
        case 1:
            if (is_demand)
                BIND_D_FUNC(", int)");
            else
                BIND_R_FUNC(", int)");
            break;
        case 2:
            if (is_demand)
                BIND_D_FUNC(", int, int)");
            else
                BIND_R_FUNC(", int,int)");
            break;
        case 3:
            if (is_demand)
                BIND_D_FUNC(", int, int, int)");
            else
                BIND_R_FUNC(", int, int, int)");
            break;
        case 4:
            if (is_demand)
                BIND_D_FUNC(", int, int, int, int)");
            else
                BIND_R_FUNC(", int, int, int, int)");
            break;
        case 5:
            if (is_demand)
                BIND_D_FUNC(", int, int, int, int ,int)");
            else
                BIND_R_FUNC(", int, int, int, int, int)");
            break;
        default:
            if (is_demand)
                BIND_D_FUNC(")");
            else
                BIND_R_FUNC(")");
            break;
        }
        if (!id)
        {
            WriteLog("Script '{}' bind error.\n", name);
            return nullptr;
        }*/
    } break;
    case DR_OR:
        break;
    default:
        return nullptr;
    }

    // Validate parsing
    if (input.fail()) {
        WriteLog("DR parse fail.\n");
        fail = true;
    }

    // Fill
    auto* result = new DemandResult();
    result->Type = type;
    result->Who = who;
    result->ParamId = id;
    result->Op = oper;
    result->ValuesCount = values_count;
    result->NoRecheck = no_recheck;
    result->RetValue = ret_value;
    result->Value = ivalue;
    result->ValueExt[0] = script_val[0];
    result->ValueExt[1] = script_val[1];
    result->ValueExt[2] = script_val[2];
    result->ValueExt[3] = script_val[3];
    result->ValueExt[4] = script_val[4];

    if (fail) {
        return nullptr;
    }
    return result;
}

auto DialogManager::GetNotAnswerAction(string_view str) -> ScriptFunc<string, Critter*, Critter*>
{
    NON_CONST_METHOD_HINT();

    if (str == "NOT_ANSWER_CLOSE_DIALOG" || str == "None") {
        return {};
    }

    return _engine->ScriptSys->FindFunc<string, Critter*, Critter*>(str);
}

auto DialogManager::GetDrType(string_view str) -> uchar
{
    if (str == "Property" || str == "_param") {
        return DR_PROP_CRITTER;
    }
    if (str == "Item" || str == "_item") {
        return DR_ITEM;
    }
    if (str == "Script" || str == "_script") {
        return DR_SCRIPT;
    }
    if (str == "NoRecheck" || str == "no_recheck") {
        return DR_NO_RECHECK;
    }
    if (str == "Or" || str == "or") {
        return DR_OR;
    }
    return DR_NONE;
}

auto DialogManager::GetWho(char who) -> uchar
{
    if (who == 'P' || who == 'p') {
        return DR_WHO_PLAYER;
    }
    if (who == 'N' || who == 'n') {
        return DR_WHO_PLAYER;
    }
    return DR_WHO_NONE;
}

auto DialogManager::CheckOper(char oper) -> bool
{
    return oper == '>' || oper == '<' || oper == '=' || oper == '+' || oper == '-' || oper == '*' || oper == '/' || oper == '=' || oper == '!' || oper == '}' || oper == '{';
}
