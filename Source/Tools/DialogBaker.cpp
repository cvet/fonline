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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "DialogBaker.h"
#include "Dialogs.h"
#include "ScriptSystem.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE();

class Critter;

DialogBaker::DialogBaker(const BakerSettings& settings, string pack_name, BakeCheckerCallback bake_checker, AsyncWriteDataCallback write_data, const FileSystem* baked_files) :
    BaseBaker(settings, std::move(pack_name), std::move(bake_checker), std::move(write_data), baked_files)
{
    FO_STACK_TRACE_ENTRY();
}

DialogBaker::~DialogBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void DialogBaker::BakeFiles(FileCollection files)
{
    FO_STACK_TRACE_ENTRY();

    vector<File> filtered_files;
    uint64 max_write_time = 0;
    bool write_any = false;

    while (files.MoveNext()) {
        auto file_header = files.GetCurFileHeader();
        const string ext = strex(file_header.GetPath()).getFileExtension();

        if (!IsExtSupported(ext)) {
            continue;
        }

        if (!_bakeChecker || _bakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
            write_any = true;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(files.GetCurFile());
    }

    if (!filtered_files.empty()) {
        for (const auto& lang_name : _settings->BakeLanguages) {
            if (!_bakeChecker || _bakeChecker(strex("Dialogs.{}.fotxtb", lang_name), max_write_time)) {
                write_any = true;
            }
        }
    }

    if (!write_any) {
        return;
    }

    auto server_engine = BakerEngine(PropertiesRelationType::ServerRelative);
    auto dialog_mngr = DialogManager(server_engine);
    auto script_sys = BakerScriptSystem(server_engine, *_bakedFiles);

    size_t errors = 0;
    vector<unique_ptr<DialogPack>> dialog_packs;

    for (const auto& file : filtered_files) {
        try {
            auto pack = dialog_mngr.ParseDialog(file.GetName(), file.GetStr());
            dialog_packs.emplace_back(std::move(pack));
        }
        catch (const DialogParseException& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
    }

    for (const auto& dlg_pack : dialog_packs) {
        for (const auto& dlg : dlg_pack->Dialogs) {
            if (dlg.DlgScriptFuncName) {
                if (!script_sys.CheckFunc<void, Critter*, Critter*, string*>(dlg.DlgScriptFuncName) && //
                    !script_sys.CheckFunc<uint32, Critter*, Critter*, string*>(dlg.DlgScriptFuncName)) {
                    // WriteLog("Dialog {} invalid start function {}", dlg_pack->PackName, dlg.DlgScriptFuncName);
                    // errors++;
                }
            }

            for (const auto& answer : dlg.Answers) {
                for (const auto& demand : answer.Demands) {
                    if (demand.Type == DR_SCRIPT) {
                        if ((demand.ValuesCount == 0 && !script_sys.CheckFunc<bool, Critter*, Critter*>(demand.AnswerScriptFuncName)) || //
                            (demand.ValuesCount == 1 && !script_sys.CheckFunc<bool, Critter*, Critter*, int32>(demand.AnswerScriptFuncName)) || //
                            (demand.ValuesCount == 2 && !script_sys.CheckFunc<bool, Critter*, Critter*, int32, int32>(demand.AnswerScriptFuncName)) || //
                            (demand.ValuesCount == 3 && !script_sys.CheckFunc<bool, Critter*, Critter*, int32, int32, int32>(demand.AnswerScriptFuncName)) || //
                            (demand.ValuesCount == 4 && !script_sys.CheckFunc<bool, Critter*, Critter*, int32, int32, int32, int32>(demand.AnswerScriptFuncName)) || //
                            (demand.ValuesCount == 5 && !script_sys.CheckFunc<bool, Critter*, Critter*, int32, int32, int32, int32, int32>(demand.AnswerScriptFuncName))) {
                            WriteLog("Dialog {} answer demand invalid function {}", dlg_pack->PackName, demand.AnswerScriptFuncName);
                            errors++;
                        }
                    }
                }

                for (const auto& result : answer.Results) {
                    if (result.Type == DR_SCRIPT) {
                        int32 not_found_count = 0;

                        if ((result.ValuesCount == 0 && !script_sys.CheckFunc<void, Critter*, Critter*>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 1 && !script_sys.CheckFunc<void, Critter*, Critter*, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 2 && !script_sys.CheckFunc<void, Critter*, Critter*, int32, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 3 && !script_sys.CheckFunc<void, Critter*, Critter*, int32, int32, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 4 && !script_sys.CheckFunc<void, Critter*, Critter*, int32, int32, int32, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 5 && !script_sys.CheckFunc<void, Critter*, Critter*, int32, int32, int32, int32, int32>(result.AnswerScriptFuncName))) {
                            not_found_count++;
                        }

                        if ((result.ValuesCount == 0 && !script_sys.CheckFunc<int32, Critter*, Critter*>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 1 && !script_sys.CheckFunc<int32, Critter*, Critter*, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 2 && !script_sys.CheckFunc<int32, Critter*, Critter*, int32, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 3 && !script_sys.CheckFunc<int32, Critter*, Critter*, int32, int32, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 4 && !script_sys.CheckFunc<int32, Critter*, Critter*, int32, int32, int32, int32>(result.AnswerScriptFuncName)) || //
                            (result.ValuesCount == 5 && !script_sys.CheckFunc<int32, Critter*, Critter*, int32, int32, int32, int32, int32>(result.AnswerScriptFuncName))) {
                            not_found_count++;
                        }

                        if (not_found_count != 1) {
                            WriteLog("Dialog {} answer result invalid function {}", dlg_pack->PackName, result.AnswerScriptFuncName);
                            errors++;
                        }
                    }
                }
            }
        }
    }

    // Texts
    vector<pair<string, map<string, TextPack>>> lang_packs;

    for (const auto& dlg_pack : dialog_packs) {
        for (const auto& dlg_pack_text : dlg_pack->Texts) {
            const string lang_pack = dlg_pack_text.first;

            if (std::find_if(_settings->BakeLanguages.begin(), _settings->BakeLanguages.end(), [&](auto&& l) { return l == lang_pack; }) == _settings->BakeLanguages.end()) {
                WriteLog(LogType::Warning, "Dialog {} contains unsupported language {}", dlg_pack->PackName, lang_pack);
                continue;
            }

            const auto it = std::ranges::find_if(lang_packs, [&](auto&& l) { return l.first == lang_pack; });

            if (it == lang_packs.end()) {
                auto dialogs_text_pack = map<string, TextPack>();
                dialogs_text_pack.emplace("Dialogs", dlg_pack_text.second);
                lang_packs.emplace_back(lang_pack, std::move(dialogs_text_pack));
            }
            else {
                auto& text_pack = it->second.at("Dialogs");

                if (!text_pack.CheckIntersections(dlg_pack_text.second)) {
                    text_pack.Merge(dlg_pack_text.second);
                }
                else {
                    WriteLog("Dialog {} text intersection detected", dlg_pack->PackName);
                    errors++;
                }
            }
        }
    }

    TextPack::FixPacks(_settings->BakeLanguages, lang_packs);

    if (errors != 0) {
        throw DialogBakerException("Errors during dialogs baking");
    }

    // Write data
    for (auto&& [lang_name, text_packs] : lang_packs) {
        auto text_pack_data = text_packs.at("Dialogs").GetBinaryData();
        _writeData(strex("Dialogs.{}.fotxtb", lang_name), text_pack_data);
    }

    for (const auto& file : filtered_files) {
        _writeData(file.GetPath(), file.GetData());
    }
}

FO_END_NAMESPACE();
