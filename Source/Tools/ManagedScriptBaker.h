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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#if FO_MANAGED_SCRIPTING

#include "Baker.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ManagedScriptBakerException);

class ManagedScriptBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME = "Managed";

    explicit ManagedScriptBaker(shared_ptr<BakingContext> ctx);
    ManagedScriptBaker(const ManagedScriptBaker&) = delete;
    ManagedScriptBaker(ManagedScriptBaker&&) noexcept = delete;
    auto operator=(const ManagedScriptBaker&) = delete;
    auto operator=(ManagedScriptBaker&&) noexcept = delete;
    ~ManagedScriptBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    // Order 3: after Metadata (1) — the only bake output the managed compile reads — and before the validation
    // bakers (Dialog 5, Proto 6, Map 7) that restore the compiled assembly to reflect over managed script funcs
    // (ScriptSystem::FindFunc). The Scripts pack's remaining baker is AngelScript (4), so the pack still completes
    // (and its assembly output is mounted into the shared bake output) at order 4, before those validators run.
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 3; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    static void GenerateTargetApiFiles(const EngineMetadata& meta, const std::filesystem::path& project_dir, string_view target_name);
    static void GenerateManagedHostProjectFile(const std::filesystem::path& project_dir, string_view target_framework, const std::filesystem::path& source_file);
    static void GenerateUnifiedProjectFile(const std::filesystem::path& project_dir, const std::filesystem::path& assemblies_output_dir, string_view pack_name, string_view project_name, string_view target_framework, const map<string, vector<std::filesystem::path>>& source_files, const map<string, vector<string>>& references);
    static void GenerateSolutionFile(const std::filesystem::path& project_dir, string_view solution_name, const vector<string>& project_names);
    static auto CollectSourceFiles(const FileCollection& files, const vector<std::filesystem::path>& dir_source_files, const vector<string>& extra_sources, string_view assembly_name, string_view target_name) -> vector<std::filesystem::path>;
    static auto CollectReferences(const vector<string>& extra_references, string_view assembly_name, string_view target_name) -> vector<string>;
    static auto GetManagedGeneratedDir(string_view dir_override) -> std::filesystem::path;
    static auto RunCommand(string_view command, string_view fail_message) -> void;
};

FO_END_NAMESPACE

#endif
