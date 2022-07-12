//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Application.h"
#include "FileSystem.h"
#include "StringUtils.h"

static auto GetPropEnumIndex(FOEngineBase* engine, string_view str, bool is_demand, uchar& type, bool& is_hash) -> uint
{
    const auto* prop_global = engine->GetPropertyRegistrator(GameProperties::ENTITY_CLASS_NAME)->Find(str);
    const auto* prop_critter = engine->GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME)->Find(str);
    const auto* prop_item = engine->GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME)->Find(str);
    const auto* prop_location = engine->GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME)->Find(str);
    const auto* prop_map = engine->GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME)->Find(str);

    auto count = 0;
    count += prop_global != nullptr ? 1 : 0;
    count += prop_critter != nullptr ? 1 : 0;
    count += prop_item != nullptr ? 1 : 0;
    count += prop_location != nullptr ? 1 : 0;
    count += prop_map != nullptr ? 1 : 0;

    if (count == 0) {
        throw DialogParseException("DR property not found in GlobalVars/Critter/Item/Location/Map", str);
    }
    if (count > 1) {
        throw DialogParseException("DR property found multiple instances in GlobalVars/Critter/Item/Location/Map", str);
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

    if (type == DR_PROP_CRITTER && prop->IsDict()) {
        type = DR_PROP_CRITTER_DICT;
        if (prop->GetDictKeyTypeName() != "uint") {
            throw DialogParseException("DR property Dict must have 'uint' in key", str);
        }
    }
    else {
        if (!prop->IsPlainData()) {
            throw DialogParseException("DR property is not PlainData type", str);
        }
    }

    if (prop->IsDisabled()) {
        throw DialogParseException("DR property is disabled", str);
    }
    if (!is_demand && prop->IsReadOnly()) {
        throw DialogParseException("DR property is read only", str);
    }

    is_hash = prop->IsBaseTypeHash();
    return prop->GetRegIndex();
}

DialogManager::DialogManager(FOEngineBase* engine) : _engine {engine}
{
}

void DialogManager::LoadFromResources()
{
    auto errors = 0;

    auto files = _engine->FileSys.FilterFiles("fodlg");
    while (files.MoveNext()) {
        try {
            auto file = files.GetCurFile();
            auto* pack = ParseDialog(file.GetName(), file.GetStr());
            AddDialog(pack);
        }
        catch (const DialogParseException& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
    }

    if (errors != 0) {
        throw DialogManagerException("Can't load all dialogs");
    }
}

void DialogManager::ValidateDialogs()
{
    // Todo: validate script entries, hashes
}

void DialogManager::AddDialog(DialogPack* pack)
{
    if (_dialogPacks.count(pack->PackId) != 0u) {
        throw DialogManagerException("Dialog already added", pack->PackName);
    }

    for (auto&& [pack_id, other_pack] : _dialogPacks) {
        if ((pack->PackId.as_uint() & DLGID_MASK) == (pack_id.as_uint() & DLGID_MASK)) {
            throw DialogManagerException("Name hash collision for dialogs", pack->PackName, other_pack->PackName);
        }
    }

    _dialogPacks.insert(std::make_pair(pack->PackId, pack));
}

auto DialogManager::GetDialog(hstring pack_id) -> DialogPack*
{
    const auto it = _dialogPacks.find(pack_id);
    return it != _dialogPacks.end() ? it->second.get() : nullptr;
}

auto DialogManager::GetDialogs() -> vector<DialogPack*>
{
    vector<DialogPack*> result;

    for (auto&& [pack_id, pack] : _dialogPacks) {
        result.emplace_back(pack.get());
    }

    return result;
}

auto DialogManager::ParseDialog(string_view pack_name, string_view data) -> DialogPack*
{
    auto fodlg = ConfigFile("", *_engine);
    fodlg.CollectContent();
    fodlg.AppendData(data);

    auto* pack = new DialogPack();
    auto dlg_buf = fodlg.GetAppContent("dialog");
    istringstream input(dlg_buf);
    string lang_buf;
    pack->PackId = _engine->ToHashedString(pack_name);
    pack->PackName = pack_name;
    vector<string> lang_apps;

    // Comment
    pack->Comment = fodlg.GetAppContent("comment");

    // Texts
    auto lang_key = fodlg.GetStr("data", "lang", "");
    if (lang_key.empty()) {
        throw DialogParseException("Lang app not found", pack_name);
    }

    // Check dialog pack
    if (pack->PackId.as_uint() <= 0xFFFF) {
        throw DialogParseException("Invalid hash for dialog name", pack_name);
    }

    lang_apps = _str(lang_key).split(' ');
    if (lang_apps.empty()) {
        throw DialogParseException("Lang app is empty", pack_name);
    }

    for (size_t i = 0; i < lang_apps.size(); i++) {
        auto& lang_app = lang_apps[i];
        if (lang_app.size() != 4) {
            throw DialogParseException("Language length not equal 4", pack_name);
        }

        lang_buf = fodlg.GetAppContent(lang_app);
        if (lang_buf.empty()) {
            throw DialogParseException("One of the lang section not found", pack_name);
        }

        FOMsg temp_msg;
        if (!temp_msg.LoadFromString(lang_buf, *_engine)) {
            throw DialogParseException("Load MSG fail", pack_name);
        }

        if (temp_msg.GetStrNumUpper(100000000 + ~DLGID_MASK) != 0u) {
            throw DialogParseException("Text have any text with index greather than 4000", pack_name);
        }

        pack->TextsLang.push_back(*reinterpret_cast<const uint*>(lang_app.c_str()));
        pack->Texts.push_back(FOMsg());

        uint str_num = 0;
        while ((str_num = temp_msg.GetStrNumUpper(str_num)) != 0u) {
            const auto count = temp_msg.Count(str_num);
            const auto new_str_num = DLG_STR_ID(pack->PackId.as_uint(), (str_num < 100000000 ? str_num / 10 : str_num - 100000000 + 12000));
            for (uint n = 0; n < count; n++) {
                pack->Texts[i].AddStr(new_str_num, _str(temp_msg.GetStr(str_num, n)).replace("\n\\[", "\n["));
            }
        }
    }

    // Dialog
    if (dlg_buf.empty()) {
        throw DialogParseException("Dialog section not found", pack_name);
    }

    char ch = 0;
    input >> ch;
    if (ch != '&') {
        return nullptr;
    }

    uint dlg_id = 0;
    uint text_id = 0;
    uint link = 0;
    string script;
    uint flags = 0;

    while (true) {
        input >> dlg_id;
        if (input.eof()) {
            break;
        }
        if (input.fail()) {
            throw DialogParseException("Bad dialog id number", pack_name);
        }

        input >> text_id;
        if (input.fail()) {
            throw DialogParseException("Bad text link", pack_name);
        }
        if (text_id / 10 > ~DLGID_MASK) {
            throw DialogParseException("Invalid text link value", pack_name);
        }

        input >> script;
        if (input.fail()) {
            throw DialogParseException("Bad not answer action", pack_name);
        }
        if (script == "NOT_ANSWER_CLOSE_DIALOG" || script == "None") {
            script = "";
        }

        input >> flags;
        if (input.fail()) {
            throw DialogParseException("Bad flags", pack_name);
        }

        Dialog current_dialog;
        current_dialog.Id = dlg_id;
        current_dialog.TextId = DLG_STR_ID(pack->PackId.as_uint(), text_id / 10);
        current_dialog.DlgScriptFunc = script;
        current_dialog.NoShuffle = ((flags & 1) == 1);

        // Read answers
        input >> ch;
        if (input.fail()) {
            throw DialogParseException("Dialog corrupted", pack_name);
        }

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
        if (ch != '#') {
            throw DialogParseException("Parse error 0", pack_name);
        }

        while (!input.eof()) {
            input >> link;
            if (input.fail()) {
                throw DialogParseException("Bad link in answer", pack_name);
            }

            input >> text_id;
            if (input.fail()) {
                throw DialogParseException("Bad text link in answer", pack_name);
            }

            if (text_id / 10 > ~DLGID_MASK) {
                throw DialogParseException("Invalid text link value in answer", pack_name);
            }

            DialogAnswer current_answer;
            current_answer.Link = link;
            current_answer.TextId = DLG_STR_ID(pack->PackId.as_uint(), text_id / 10);

            while (true) {
                input >> ch;
                if (input.fail()) {
                    throw DialogParseException("Parse answer character fail", pack_name);
                }

                // Demands
                if (ch == 'D') {
                    auto* d = LoadDemandResult(input, true);
                    if (d == nullptr) {
                        throw DialogParseException("Demand not loaded", pack_name);
                    }

                    current_answer.Demands.push_back(*d);
                }
                // Results
                else if (ch == 'R') {
                    auto* r = LoadDemandResult(input, false);
                    if (r == nullptr) {
                        throw DialogParseException("Result not loaded", pack_name);
                    }

                    current_answer.Results.push_back(*r);
                }
                else if (ch == '*' || ch == 'd' || ch == 'r') {
                    throw DialogParseException("Found old token, update dialog file to actual format (resave in version 2.22)", pack_name);
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

    char who = DR_WHO_PLAYER;
    auto oper = '=';
    int values_count = 0;
    string svalue;
    auto ivalue = 0;
    uint id_index = 0u;
    hstring id_hash;
    string type_str;
    string name;
    string script_name;
    auto no_recheck = false;
    auto ret_value = false;

    int script_val[5] = {0, 0, 0, 0, 0};

    input >> type_str;
    if (input.fail()) {
        throw DialogParseException("Parse DR type fail");
    }

    auto type = GetDrType(type_str);
    if (type == DR_NO_RECHECK) {
        no_recheck = true;
        input >> type_str;
        if (input.fail()) {
            throw DialogParseException("Parse DR type fail2");
        }
        type = GetDrType(type_str);
    }

    switch (type) {
    case DR_PROP_CRITTER: {
        // Who
        input >> who;
        who = GetWho(who);
        if (who == DR_WHO_NONE) {
            throw DialogParseException("Invalid DR property who", who);
        }

        // Name
        input >> name;
        auto is_hash = false;
        id_index = GetPropEnumIndex(_engine, name, is_demand, type, is_hash);

        // Operator
        input >> oper;
        if (!CheckOper(oper)) {
            throw DialogParseException("Invalid DR property oper", oper);
        }

        // Value
        input >> svalue;
        if (is_hash) {
            ivalue = _engine->ToHashedString(svalue).as_int();
        }
        else {
            ivalue = _engine->ResolveGenericValue(svalue);
        }
    } break;
    case DR_ITEM: {
        // Who
        input >> who;
        who = GetWho(who);
        if (who == DR_WHO_NONE) {
            throw DialogParseException("Invalid DR item who", who);
        }

        // Name
        input >> name;
        id_hash = _engine->ToHashedString(name);

        // Operator
        input >> oper;
        if (!CheckOper(oper)) {
            throw DialogParseException("Invalid DR item oper", oper);
        }

        // Value
        input >> svalue;
        ivalue = _engine->ResolveGenericValue(svalue);
    } break;
    case DR_SCRIPT: {
        // Script name
        input >> script_name;

        // Values count
        input >> values_count;

        // Values
        string value_str;
        if (values_count > 0) {
            input >> value_str;
            script_val[0] = _engine->ResolveGenericValue(value_str);
        }
        if (values_count > 1) {
            input >> value_str;
            script_val[1] = _engine->ResolveGenericValue(value_str);
        }
        if (values_count > 2) {
            input >> value_str;
            script_val[2] = _engine->ResolveGenericValue(value_str);
        }
        if (values_count > 3) {
            input >> value_str;
            script_val[3] = _engine->ResolveGenericValue(value_str);
        }
        if (values_count > 4) {
            input >> value_str;
            script_val[4] = _engine->ResolveGenericValue(value_str);
        }
        if (values_count > 5) {
            throw DialogParseException("Invalid values count", values_count);
        }
    } break;
    case DR_OR:
        break;
    default:
        throw DialogParseException("Invalid DR type");
    }

    // Validate parsing
    if (input.fail()) {
        throw DialogParseException("DR parse fail");
    }

    // Fill
    auto* result = new DemandResult();
    result->Type = type;
    result->Who = who;
    result->ParamIndex = id_index;
    result->ParamHash = id_hash;
    result->ParamFuncName = script_name;
    result->Op = oper;
    result->ValuesCount = static_cast<uchar>(values_count);
    result->NoRecheck = no_recheck;
    result->RetValue = ret_value;
    result->Value = ivalue;
    result->ValueExt[0] = script_val[0];
    result->ValueExt[1] = script_val[1];
    result->ValueExt[2] = script_val[2];
    result->ValueExt[3] = script_val[3];
    result->ValueExt[4] = script_val[4];
    return result;
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
