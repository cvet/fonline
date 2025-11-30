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

#include "AngelScriptBaker.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Application.h"

FO_BEGIN_NAMESPACE();

extern vector<uint8> Init_AngelScriptCompiler_ServerScriptSystem(const vector<File>&);
extern vector<uint8> Init_AngelScriptCompiler_ClientScriptSystem(const vector<File>&);
extern vector<uint8> Init_AngelScriptCompiler_MapperScriptSystem(const vector<File>&);

unordered_set<string> CompilerPassedMessages;

AngelScriptBaker::AngelScriptBaker(BakerData& data) :
    BaseBaker(data)
{
    FO_STACK_TRACE_ENTRY();
}

AngelScriptBaker::~AngelScriptBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void AngelScriptBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    if (!target_path.empty() && !strex(target_path).get_file_extension().starts_with("fos-")) {
        return;
    }

    // Collect files
    vector<File> filtered_files;
    uint64 max_write_time = 0;

    for (const auto& file_header : files) {
        const string ext = strex(file_header.GetPath()).get_file_extension();

        if (ext != "fos") {
            continue;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(File::Load(file_header));
    }

    if (filtered_files.empty()) {
        return;
    }

    const bool bake_server = !_bakeChecker || _bakeChecker(_resPackName + ".fos-bin-server", max_write_time);
    const bool bake_client = !_bakeChecker || _bakeChecker(_resPackName + ".fos-bin-client", max_write_time);
    const bool bake_mapper = !_bakeChecker || _bakeChecker(_resPackName + ".fos-bin-mapper", max_write_time);

    if (!bake_server && !bake_client && !bake_mapper) {
        return;
    }

    // Process files
    vector<std::future<void>> file_bakings;

    if (bake_server) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            auto data = Init_AngelScriptCompiler_ServerScriptSystem(filtered_files);
            _writeData(_resPackName + ".fos-bin-server", data);
        }));
    }
    if (bake_client) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            auto data = Init_AngelScriptCompiler_ClientScriptSystem(filtered_files);
            _writeData(_resPackName + ".fos-bin-client", data);
        }));
    }
    if (bake_mapper) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            auto data = Init_AngelScriptCompiler_MapperScriptSystem(filtered_files);
            _writeData(_resPackName + ".fos-bin-mapper", data);
        }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const ScriptCompilerException&) {
            errors++;
        }
        catch (const std::exception& ex) {
            WriteLog("AngelScript baking error: {}", ex.what());
            errors++;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw AngelScriptBakerException("Errors during scripts compilation");
    }
}

FO_END_NAMESPACE();

#endif
