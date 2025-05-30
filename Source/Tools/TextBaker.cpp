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

#include "TextBaker.h"
#include "ConfigFile.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE();

TextBaker::TextBaker(const BakerSettings& settings, string pack_name, BakeCheckerCallback bake_checker, AsyncWriteDataCallback write_data, const FileSystem* baked_files) :
    BaseBaker(settings, std::move(pack_name), std::move(bake_checker), std::move(write_data), baked_files)
{
    FO_STACK_TRACE_ENTRY();

    if (_settings->BakeLanguages.empty()) {
        throw TextBakerException("No bake languages specified");
    }
}

TextBaker::~TextBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void TextBaker::BakeFiles(FileCollection files)
{
    FO_STACK_TRACE_ENTRY();

    vector<File> filtered_files;

    while (files.MoveNext()) {
        auto file_header = files.GetCurFileHeader();
        const string ext = strex(file_header.GetPath()).getFileExtension();

        if (!IsExtSupported(ext)) {
            continue;
        }
        if (_bakeChecker && !_bakeChecker(file_header.GetPath() + "b", file_header.GetWriteTime())) {
            continue;
        }

        filtered_files.emplace_back(files.GetCurFile());
    }

    if (filtered_files.empty()) {
        return;
    }

    // Find languages
    const auto& default_lang = _settings->BakeLanguages.front();
    set<string> languages;
    set<string> all_languages;

    for (const auto& file : filtered_files) {
        const auto file_name = file.GetName();

        const auto sep = file_name.find('.');
        FO_RUNTIME_ASSERT(sep != string::npos);

        const auto lang_name = file_name.substr(sep + 1);
        FO_RUNTIME_ASSERT(!lang_name.empty());

        if (all_languages.emplace(lang_name).second) {
            if (std::find(_settings->BakeLanguages.begin(), _settings->BakeLanguages.end(), lang_name) == _settings->BakeLanguages.end()) {
                WriteLog(LogType::Warning, "Unsupported language: {}. Skip", lang_name);
            }
            else {
                languages.emplace(lang_name);
            }
        }
    }

    if (languages.count(default_lang) == 0) {
        throw TextBakerException("Default language not found");
    }

    // Parse texts
    vector<pair<string, map<string, TextPack>>> lang_packs;
    HashStorage hashes;

    for (const auto& target_lang : languages) {
        map<string, TextPack> lang_pack;

        for (const auto& file : filtered_files) {
            const auto file_name = file.GetName();

            const auto sep = file_name.find('.');
            FO_RUNTIME_ASSERT(sep != string::npos);

            const auto text_pack_name = file_name.substr(0, sep);
            const auto lang_name = file_name.substr(sep + 1);
            FO_RUNTIME_ASSERT(!text_pack_name.empty());
            FO_RUNTIME_ASSERT(!lang_name.empty());

            if (lang_name == target_lang) {
                TextPack text_pack;

                if (!text_pack.LoadFromString(file.GetStr(), hashes)) {
                    throw LanguagePackException("Invalid text file", file.GetPath());
                }

                lang_pack.emplace(text_pack_name, std::move(text_pack));
            }
        }

        if (target_lang == default_lang) {
            // Default language should be first
            lang_packs.insert(lang_packs.begin(), std::make_pair(target_lang, std::move(lang_pack)));
        }
        else {
            lang_packs.emplace_back(std::make_pair(target_lang, std::move(lang_pack)));
        }
    }

    TextPack::FixPacks(_settings->BakeLanguages, lang_packs);

    // Save parsed packs
    for (auto&& [lang_name, lang_pack] : lang_packs) {
        for (auto&& [pack_name, text_pack] : lang_pack) {
            _writeData(strex("{}.{}.fotxtb", pack_name, lang_name), text_pack.GetBinaryData());
        }
    }
}

FO_END_NAMESPACE();
