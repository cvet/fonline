// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_MANAGED_SCRIPTING

#include "Baker.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ManagedScriptBakerException);

// Analysis and post-compile profile of the generated managed script project, from the ManagedScript.* settings.
// An empty field emits nothing, and the profile covers the script project only, never the managed host
struct ManagedProjectAnalysis
{
    string Level {};
    string Mode {};
    vector<string> AnalyzerProjects {};
    vector<pair<string, string>> AnalyzerPackages {};
    vector<string> AdditionalFiles {};
    string PatchPointWeaver {};
};

class ManagedScriptBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Managed"};

    explicit ManagedScriptBaker(shared_ptr<BakingContext> ctx);
    ManagedScriptBaker(const ManagedScriptBaker&) = delete;
    ManagedScriptBaker(ManagedScriptBaker&&) noexcept = delete;
    auto operator=(const ManagedScriptBaker&) = delete;
    auto operator=(ManagedScriptBaker&&) noexcept = delete;
    ~ManagedScriptBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    // Order 3: after Metadata (1), which the managed compile reads, and before the validation bakers that
    // reflect over the compiled assembly
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 3; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    static void GenerateTargetApiFiles(const EngineMetadata& meta, const std::filesystem::path& project_dir, string_view target_name);
    static void GenerateManagedHostProjectFile(const std::filesystem::path& project_dir, string_view target_framework, const std::filesystem::path& source_file);
    static void GenerateUnifiedProjectFile(const std::filesystem::path& project_dir, string_view assemblies_dir, string_view pack_name, string_view project_name, string_view target_framework, const map<string, vector<std::filesystem::path>>& source_files, const map<string, vector<string>>& references, const ManagedProjectAnalysis& analysis);
    static void GenerateSolutionFile(const std::filesystem::path& project_dir, string_view solution_name, const vector<string>& project_names);
    static auto CollectSourceFiles(const FileCollection& files, const vector<std::filesystem::path>& dir_source_files, const vector<string>& extra_sources, string_view assembly_name, string_view target_name, const std::filesystem::path& config_dir) -> vector<std::filesystem::path>;
    static auto CollectReferences(const vector<string>& extra_references, string_view assembly_name, string_view target_name, const std::filesystem::path& config_dir) -> vector<string>;
    static auto GetManagedGeneratedDir(string_view dir_override, const std::filesystem::path& config_dir) -> std::filesystem::path;
    static auto RunCommand(string_view command, string_view fail_message) -> void;
};

FO_END_NAMESPACE

#endif
