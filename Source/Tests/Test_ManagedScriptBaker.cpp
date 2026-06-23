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

#include "catch_amalgamated.hpp"

#include "Test_BakerHelpers.h"

#if FO_MANAGED_SCRIPTING
#include "ManagedScriptBaker.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_MANAGED_SCRIPTING

static void SetProcessEnv(string_view name, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    const string name_str {name};
    const string value_str {value};

#if FO_WINDOWS
    (void)_putenv_s(name_str.c_str(), value_str.c_str());
#else
    (void)setenv(name_str.c_str(), value_str.c_str(), 1);
#endif
}

static void UnsetProcessEnv(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    const string name_str {name};

#if FO_WINDOWS
    (void)_putenv_s(name_str.c_str(), "");
#else
    (void)unsetenv(name_str.c_str());
#endif
}

class ScopedEnvVar final
{
public:
    ScopedEnvVar(string name, string_view value) :
        _name {std::move(name)}
    {
        FO_STACK_TRACE_ENTRY();

        if (const char* old_value = std::getenv(_name.c_str()); old_value != nullptr) {
            _oldValue = old_value;
        }

        SetProcessEnv(_name, value);
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar(ScopedEnvVar&&) noexcept = delete;
    auto operator=(const ScopedEnvVar&) = delete;
    auto operator=(ScopedEnvVar&&) noexcept = delete;

    ~ScopedEnvVar()
    {
        FO_STACK_TRACE_ENTRY();

        if (_oldValue.has_value()) {
            SetProcessEnv(_name, *_oldValue);
        }
        else {
            UnsetProcessEnv(_name);
        }
    }

private:
    string _name {};
    optional<string> _oldValue {};
};

class ScopedUnsetEnvVar final
{
public:
    explicit ScopedUnsetEnvVar(string name) :
        _name {std::move(name)}
    {
        FO_STACK_TRACE_ENTRY();

        if (const char* old_value = std::getenv(_name.c_str()); old_value != nullptr) {
            _oldValue = old_value;
        }

        UnsetProcessEnv(_name);
    }

    ScopedUnsetEnvVar(const ScopedUnsetEnvVar&) = delete;
    ScopedUnsetEnvVar(ScopedUnsetEnvVar&&) noexcept = delete;
    auto operator=(const ScopedUnsetEnvVar&) = delete;
    auto operator=(ScopedUnsetEnvVar&&) noexcept = delete;

    ~ScopedUnsetEnvVar()
    {
        FO_STACK_TRACE_ENTRY();

        if (_oldValue.has_value()) {
            SetProcessEnv(_name, *_oldValue);
        }
        else {
            UnsetProcessEnv(_name);
        }
    }

private:
    string _name {};
    optional<string> _oldValue {};
};

class ScopedTempDirectory final
{
public:
    ScopedTempDirectory()
    {
        FO_STACK_TRACE_ENTRY();

        const std::filesystem::path base_dir = std::filesystem::temp_directory_path();
        const int64_t stamp = numeric_cast<int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        for (uint32_t attempt = 0; attempt < 100; attempt++) {
            std::error_code ec;
            const std::filesystem::path candidate = base_dir / strex("FOnlineManagedScriptBakerTest_{}_{}", stamp, attempt).str();

            if (std::filesystem::create_directory(candidate, ec) && !ec) {
                _path = candidate;
                return;
            }
        }

        throw ManagedScriptBakerException("Can't create temporary ManagedScriptBaker test directory");
    }

    ScopedTempDirectory(const ScopedTempDirectory&) = delete;
    ScopedTempDirectory(ScopedTempDirectory&&) noexcept = delete;
    auto operator=(const ScopedTempDirectory&) = delete;
    auto operator=(ScopedTempDirectory&&) noexcept = delete;

    ~ScopedTempDirectory()
    {
        FO_STACK_TRACE_ENTRY();

        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }

    [[nodiscard]] auto Path() const noexcept -> const std::filesystem::path& { return _path; }

private:
    std::filesystem::path _path {};
};

class ScopedCurrentPath final
{
public:
    explicit ScopedCurrentPath(const std::filesystem::path& path) :
        _oldPath {std::filesystem::current_path()}
    {
        FO_STACK_TRACE_ENTRY();

        std::filesystem::current_path(path);
    }

    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath(ScopedCurrentPath&&) noexcept = delete;
    auto operator=(const ScopedCurrentPath&) = delete;
    auto operator=(ScopedCurrentPath&&) noexcept = delete;

    ~ScopedCurrentPath()
    {
        FO_STACK_TRACE_ENTRY();

        std::filesystem::current_path(_oldPath);
    }

private:
    std::filesystem::path _oldPath {};
};

static void WriteTextFile(const std::filesystem::path& path, string_view text)
{
    FO_STACK_TRACE_ENTRY();

    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    REQUIRE(file.good());
    file.write(text.data(), numeric_cast<std::streamsize>(text.size()));
}

static auto ReadTextFile(const std::filesystem::path& path) -> string
{
    FO_STACK_TRACE_ENTRY();

    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());

    std::ostringstream str;
    str << file.rdbuf();
    return strex("{}", str.str()).str();
}

static auto WriteFakeManagedMsBuildScript(const std::filesystem::path& dir) -> std::filesystem::path
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    const std::filesystem::path script_path = dir / "FakeManagedMsBuild.cmd";
    WriteTextFile(script_path, R"(@echo off
if not defined FO_FAKE_MSBUILD_ROOT exit /b 1
set "ROOT=%FO_FAKE_MSBUILD_ROOT%"
mkdir "%ROOT%\ServerAssemblies" 2>nul
mkdir "%ROOT%\ClientAssemblies" 2>nul
mkdir "%ROOT%\MapperAssemblies" 2>nul
> "%ROOT%\ServerAssemblies\TestPack.Server.dll" echo entry-Server
> "%ROOT%\ServerAssemblies\SharedDependency.dll" echo helper-Server
> "%ROOT%\ServerAssemblies\TestPack.Server.pdb" echo pdb-Server
> "%ROOT%\ServerAssemblies\TestPack.Server.deps.json" echo deps-Server
> "%ROOT%\ClientAssemblies\TestPack.Client.dll" echo entry-Client
> "%ROOT%\ClientAssemblies\SharedDependency.dll" echo helper-Client
> "%ROOT%\ClientAssemblies\TestPack.Client.pdb" echo pdb-Client
> "%ROOT%\ClientAssemblies\TestPack.Client.deps.json" echo deps-Client
> "%ROOT%\MapperAssemblies\TestPack.Mapper.dll" echo entry-Mapper
> "%ROOT%\MapperAssemblies\SharedDependency.dll" echo helper-Mapper
> "%ROOT%\MapperAssemblies\TestPack.Mapper.pdb" echo pdb-Mapper
> "%ROOT%\MapperAssemblies\TestPack.Mapper.deps.json" echo deps-Mapper
exit /b 0
)");
#else
    const std::filesystem::path script_path = dir / "FakeManagedMsBuild.sh";
    WriteTextFile(script_path, R"(#!/bin/sh
if [ -z "$FO_FAKE_MSBUILD_ROOT" ]; then
    exit 1
fi
mkdir -p "$FO_FAKE_MSBUILD_ROOT/ServerAssemblies" "$FO_FAKE_MSBUILD_ROOT/ClientAssemblies" "$FO_FAKE_MSBUILD_ROOT/MapperAssemblies"
printf 'entry-Server\n' > "$FO_FAKE_MSBUILD_ROOT/ServerAssemblies/TestPack.Server.dll"
printf 'helper-Server\n' > "$FO_FAKE_MSBUILD_ROOT/ServerAssemblies/SharedDependency.dll"
printf 'pdb-Server\n' > "$FO_FAKE_MSBUILD_ROOT/ServerAssemblies/TestPack.Server.pdb"
printf 'deps-Server\n' > "$FO_FAKE_MSBUILD_ROOT/ServerAssemblies/TestPack.Server.deps.json"
printf 'entry-Client\n' > "$FO_FAKE_MSBUILD_ROOT/ClientAssemblies/TestPack.Client.dll"
printf 'helper-Client\n' > "$FO_FAKE_MSBUILD_ROOT/ClientAssemblies/SharedDependency.dll"
printf 'pdb-Client\n' > "$FO_FAKE_MSBUILD_ROOT/ClientAssemblies/TestPack.Client.pdb"
printf 'deps-Client\n' > "$FO_FAKE_MSBUILD_ROOT/ClientAssemblies/TestPack.Client.deps.json"
printf 'entry-Mapper\n' > "$FO_FAKE_MSBUILD_ROOT/MapperAssemblies/TestPack.Mapper.dll"
printf 'helper-Mapper\n' > "$FO_FAKE_MSBUILD_ROOT/MapperAssemblies/SharedDependency.dll"
printf 'pdb-Mapper\n' > "$FO_FAKE_MSBUILD_ROOT/MapperAssemblies/TestPack.Mapper.pdb"
printf 'deps-Mapper\n' > "$FO_FAKE_MSBUILD_ROOT/MapperAssemblies/TestPack.Mapper.deps.json"
)");
    std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#endif

    return script_path;
}

static auto BytesToText(const vector<uint8_t>& data) -> string
{
    FO_STACK_TRACE_ENTRY();

    return string {data.begin(), data.end()};
}

static auto MakeManagedGeneratedCs(string_view body) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("// <auto-generated />\n"
                 "// This file is generated automatically by ManagedScriptBaker from the current engine metadata\n"
                 "// and scripting content.\n"
                 "// Do not edit it manually; change engine/script/content sources and rebake instead.\n\n{}",
        body)
        .str();
}

static auto MakeMetadataBlob(const map<string, vector<vector<string>>>& sections) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> metadata;
    auto writer = DataWriter(metadata);
    writer.Write<uint16_t>(numeric_cast<uint16_t>(sections.size()));

    for (const auto& [section_name, entries] : sections) {
        writer.Write<uint16_t>(numeric_cast<uint16_t>(section_name.size()));
        writer.WritePtr(section_name.data(), section_name.size());
        writer.Write<uint32_t>(numeric_cast<uint32_t>(entries.size()));

        for (const vector<string>& entry : entries) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(entry.size()));

            for (const string& token : entry) {
                writer.Write<uint16_t>(numeric_cast<uint16_t>(token.size()));
                writer.WritePtr(token.data(), token.size());
            }
        }
    }

    return metadata;
}

#endif

TEST_CASE("ManagedScriptBaker")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    const std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    const std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    const std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";

    WriteTextFile(core_scripts_dir / "Attributes.cs", "namespace FOnline { public sealed class ModuleInitAttribute : System.Attribute { public ModuleInitAttribute(int priority = 0) {} } }\n");
    WriteTextFile(core_scripts_dir / "CoreTagged.cs", "namespace FOnline {\n    ///@ Enum CoreMirrorTag A\n    public static class CoreTagged {}\n}\n");
    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");

    const std::filesystem::path server_source = script_dir / "ServerOnly.cs";
    const std::filesystem::path client_source = script_dir / "ClientOnly.cs";
    const std::filesystem::path mapper_source = script_dir / "MapperOnly.cs";
    const std::filesystem::path shared_source = script_dir / "Shared.cs";
    const std::filesystem::path tilde_source = script_dir / "Tilde~1.cs";

    WriteTextFile(server_source,
        "#if SERVER\n"
        "namespace Demo { public static class ServerOnly {\n"
        "[ItemInit]\n"
        "public static void InitDoor(Item item, bool firstTime) {}\n"
        "[ItemStatic]\n"
        "public static bool UseStatic(Critter cr, StaticItem staticItem, Item item, object param) { return true; }\n"
        "[ItemTrigger]\n"
        "public static void EnterTrigger(Critter cr, StaticItem staticItem, bool entered, byte dir) {}\n"
        "[CritterInit]\n"
        "public static void InitCritter(Critter cr, bool firstTime) {}\n"
        "[MapInit]\n"
        "public static void InitMap(Map map, bool firstTime) {}\n"
        "[LocationInit]\n"
        "public static void InitLocation(Location loc, bool firstTime) {}\n"
        "} }\n"
        "#endif\n");
    WriteTextFile(client_source, "#if CLIENT\nnamespace Demo { public static class ClientOnly {} }\n#endif\n");
    WriteTextFile(mapper_source, "#if MAPPER\nnamespace Demo { public static class MapperOnly {} }\n#endif\n");
    WriteTextFile(shared_source, "namespace Demo { public static class Shared {} }\n");
    WriteTextFile(tilde_source, "namespace Demo { public static class TildePath {} }\n");
    WriteTextFile(script_dir / "Obsolete.gen.cs", "namespace Demo { public static class ObsoleteGeneratedCode {} }\n");
    WriteTextFile(script_dir / "ObsoleteOwned.gen.cs", MakeManagedGeneratedCs("namespace Demo { public static class ObsoleteOwnedGeneratedCode {} }\n"));
    WriteTextFile(script_dir / "Obsolete.gen.csproj", "<Project />\n");
    WriteTextFile(script_dir / "Obsolete.gen.sln", "Microsoft Visual Studio Solution File, Format Version 12.00\n");
    WriteTextFile(script_dir / "Obsolete.gen.txt", "stale generated sidecar\n");
    WriteTextFile(script_dir / "UnitManaged.Server.gen.csproj", "<Project />\n");

    ScopedCurrentPath current_path(temp_dir.Path());
    ScopedEnvVar dry_run {"FO_MANAGED_SCRIPT_BAKER_DRY_RUN", "1"};
    ScopedEnvVar source_dir {"FO_MANAGED_SOURCE_DIR", managed_source_dir.string()};
    ScopedEnvVar project_dir {"FO_MANAGED_PROJECT_DIR", script_dir.string()};
    ScopedEnvVar assemblies {"FO_MANAGED_ASSEMBLIES", "UnitManaged"};
    ScopedEnvVar sources {"FO_MANAGED_SOURCE", strex("UnitManaged,Server,{};UnitManaged,Client,{};UnitManaged,Mapper,{};UnitManaged,All,{};UnitManaged,All,{}", server_source.string(), client_source.string(), mapper_source.string(), shared_source.string(), tilde_source.string()).str()};
    ScopedEnvVar references {"FO_MANAGED_REFERENCES", "UnitManaged,Server,System.Xml;UnitManaged,All,System.Core"};
    ScopedEnvVar project_name {"FO_MANAGED_PROJECT_NAME", "UnitProject"};

    TestRig rig;
    rig.AddBakedFile("Metadata.fometa-server",
        MakeMetadataBlob({
            {"Entity", {{"ManagedInner", "HasProtos"}, {"ManagedGlobal"}}},
            {"EntityHolder", {{"Server", "Critter", "ManagedInner", "ManagedEntry"}, {"Server", "Game", "ManagedGlobal", "ManagedGlobal"}}},
            {"Event", {{"Game", "OnManagedTest", "int32", "", "value"}, {"Game", "OnManagedArray", "int32 []", "", "values"}, {"Game", "OnManagedDict", "string = > string", "", "values"}}},
            {"Property", {{"Game", "Server", "string", "ManagedTitle", "Mutable"}, {"Game", "Server", "ManagedRoute", "ManagedRouteValue", "Mutable"}, {"Game", "Server", "int32 []", "ManagedSteps", "Mutable"}, {"Critter", "Server", "int16", "ManagedSkill", "Mutable"}}},
            {"RefType", {{"ManagedRoute", "Step", "int32", "0", "Note", "string", "0", "Values", "int32[]", "0"}}},
        }));
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    ManagedScriptBaker baker(rig.MakeContext());
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Metadata.fometa-server"));
    CHECK(rig.Outputs.empty());

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    CHECK(rig.Outputs.contains("Assemblies/ServerAssemblies/TestPack.Server.dll"));
    CHECK(rig.Outputs.contains("Assemblies/ClientAssemblies/TestPack.Client.dll"));
    CHECK(rig.Outputs.contains("Assemblies/MapperAssemblies/TestPack.Mapper.dll"));

    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "UnitManaged.csproj"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "UnitManaged.Server.gen.csproj"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "UnitProject.csproj"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "UnitProject.gen.csproj"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "UnitProject.sln"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "UnitProject.gen.sln"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "ServerEnums.cs"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.Path() / "ServerEnums.gen.cs"));

    CHECK_FALSE(std::filesystem::exists(script_dir / "UnitManaged.Server.csproj"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "UnitManaged.Server.gen.csproj"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "UnitProject.csproj"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "UnitProject.sln"));
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.csproj"));
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.sln"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "ServerEnums.cs"));
    CHECK(std::filesystem::exists(script_dir / "ServerEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "ClientEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "MapperEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "Attributes.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "CoreTagged.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "Initializator.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "Native.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "Obsolete.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "ObsoleteOwned.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Obsolete.gen.csproj"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Obsolete.gen.sln"));
    CHECK(std::filesystem::exists(script_dir / "Obsolete.gen.txt"));

    const string solution = ReadTextFile(script_dir / "UnitProject.gen.sln");
    CHECK(solution.find("Microsoft Visual Studio Solution File, Format Version 12.00") != string::npos);
    CHECK(solution.find("# <auto-generated />") != string::npos);
    CHECK(solution.find("Do not edit it manually") != string::npos);
    CHECK(solution.find("Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"UnitProject\", \"UnitProject.gen.csproj\"") != string::npos);
    CHECK(solution.find("Server|AnyCPU = Server|AnyCPU") != string::npos);
    CHECK(solution.find("Client|AnyCPU = Client|AnyCPU") != string::npos);
    CHECK(solution.find("Mapper|AnyCPU = Mapper|AnyCPU") != string::npos);

    const string unified_project = ReadTextFile(script_dir / "UnitProject.gen.csproj");
    CHECK(unified_project.find("<auto-generated />") != string::npos);
    CHECK(unified_project.find("Do not edit it manually") != string::npos);
    CHECK(unified_project.find("<Configurations>Server;Client;Mapper</Configurations>") != string::npos);
    CHECK(unified_project.find("<AssemblyName>TestPack.Server</AssemblyName>") != string::npos);
    CHECK(unified_project.find("<AssemblyName>TestPack.Client</AssemblyName>") != string::npos);
    CHECK(unified_project.find("<AssemblyName>TestPack.Mapper</AssemblyName>") != string::npos);
    CHECK(unified_project.find("TRACE;SERVER") != string::npos);
    CHECK(unified_project.find("../../Baking/TestPack/Assemblies/ServerAssemblies/") != string::npos);
    CHECK(unified_project.find("ServerEnums.gen.cs") != string::npos);
    CHECK(unified_project.find("ClientEnums.gen.cs") != string::npos);
    CHECK(unified_project.find("MapperEnums.gen.cs") != string::npos);
    CHECK(unified_project.find("Attributes.gen.cs") != string::npos);
    CHECK(unified_project.find("Initializator.gen.cs") != string::npos);
    CHECK(unified_project.find("Native.gen.cs") != string::npos);
    CHECK(unified_project.find("CoreScripts/Native.cs") == string::npos);
    CHECK(unified_project.find("ServerOnly.cs") != string::npos);
    CHECK(unified_project.find("ClientOnly.cs") != string::npos);
    CHECK(unified_project.find("MapperOnly.cs") != string::npos);
    CHECK(unified_project.find("Shared.cs") != string::npos);
    CHECK(unified_project.find("Tilde~1.cs") != string::npos);
    CHECK(unified_project.find("System.Core") != string::npos);
    CHECK(unified_project.find("System.Xml") != string::npos);
    CHECK(unified_project.find("Obsolete.gen.cs") == string::npos);

    const string server_enums = ReadTextFile(script_dir / "ServerEnums.gen.cs");
    CHECK(server_enums.find("// <auto-generated />") == 0);
    CHECK(server_enums.find("Do not edit it manually") != string::npos);

    const string native_core = ReadTextFile(script_dir / "Native.gen.cs");
    CHECK(native_core.find("// <auto-generated />") == 0);
    CHECK(native_core.find("Do not edit it manually") != string::npos);
    CHECK(native_core.find("internal static class Native") != string::npos);

    const string tagged_core = ReadTextFile(script_dir / "CoreTagged.gen.cs");
    CHECK(tagged_core.find("public static class CoreTagged") != string::npos);
    CHECK(tagged_core.find("///@") == string::npos);

    const string managed_script_funcs = ReadTextFile(script_dir / "ManagedScriptFuncs.gen.txt");
    CHECK(managed_script_funcs.find("ServerOnly::InitDoor|ItemInit") != string::npos);
    CHECK(managed_script_funcs.find("ServerOnly::UseStatic|ItemStatic") != string::npos);
    CHECK(managed_script_funcs.find("ServerOnly::EnterTrigger|ItemTrigger") != string::npos);
    CHECK(managed_script_funcs.find("ServerOnly::InitCritter|CritterInit") != string::npos);
    CHECK(managed_script_funcs.find("ServerOnly::InitMap|MapInit") != string::npos);
    CHECK(managed_script_funcs.find("ServerOnly::InitLocation|LocationInit") != string::npos);

    const string server_entities = ReadTextFile(script_dir / "ServerEntities.gen.cs");
    CHECK(server_entities.find("NotImplementedException") == string::npos);
    CHECK(server_entities.find("NotSupportedException") == string::npos);
    CHECK(server_entities.find("public static string ManagedTitle") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetProperty(\n                    \"Server\",\n                    \"Game\",\n                    \"ManagedTitle\",\n                    IntPtr.Zero)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetProperty(\n                    \"Server\",\n                    \"Game\",\n                    \"ManagedTitle\",\n                    IntPtr.Zero,\n                    value)") != string::npos);
    CHECK(server_entities.find("public static ManagedRoute ManagedRouteValue") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetProperty(\n                    \"Server\",\n                    \"Game\",\n                    \"ManagedRouteValue\",\n                    IntPtr.Zero)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetProperty(\n                    \"Server\",\n                    \"Game\",\n                    \"ManagedRouteValue\",\n                    IntPtr.Zero,\n                    value)") != string::npos);
    CHECK(server_entities.find("public static List<int> ManagedSteps") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetProperty(\n                    \"Server\",\n                    \"Game\",\n                    \"ManagedSteps\",\n                    IntPtr.Zero)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetProperty(\n                    \"Server\",\n                    \"Game\",\n                    \"ManagedSteps\",\n                    IntPtr.Zero,\n                    value)") != string::npos);
    CHECK(server_entities.find("public static List<mpos> TraceHexLine") != string::npos);
    CHECK(server_entities.find("object __result = global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"TraceHexLine\",") != string::npos);
    CHECK(server_entities.find("return (List<mpos>)__result;") != string::npos);
    CHECK(server_entities.find("public static void DestroyEntities(List<ident> ids)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"DestroyEntities\",") != string::npos);
    CHECK(server_entities.find("public static void Destroy<T>(T? entity) where T : Entity") != string::npos);
    CHECK(server_entities.find("public static void Destroy<T>(System.Collections.Generic.List<T>? entities) where T : Entity") != string::npos);
    CHECK(server_entities.find("DestroyEntity(entities[__i]);") != string::npos);
    CHECK(server_entities.find("public static ManagedGlobal AddManagedGlobal()") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CreateInnerEntity(IntPtr.Zero, \"ManagedGlobal\", 0UL)") != string::npos);
    CHECK(server_entities.find("public static bool HasManagedGlobals()") != string::npos);
    CHECK(server_entities.find("public static System.Collections.Generic.List<ManagedGlobal> GetManagedGlobals()") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetInnerEntityAt(IntPtr.Zero, \"ManagedGlobal\", __i)") != string::npos);
    CHECK(server_entities.find("public ManagedInner AddManagedEntry(hstring pid)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CreateInnerEntity(_entityPtr, \"ManagedEntry\", pid.Value)") != string::npos);
    CHECK(server_entities.find("public bool HasManagedEntrys()") != string::npos);
    CHECK(server_entities.find("public System.Collections.Generic.List<ManagedInner> GetManagedEntrys()") != string::npos);
    CHECK(server_entities.find("public ManagedInner? GetManagedEntry(ident id)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetInnerEntity(_entityPtr, \"ManagedEntry\", id.value)") != string::npos);
    CHECK(server_entities.find("public static void AddPropertySetter(CritterProperty property, global::FOnline.PropertySetter<Critter, short> setter)") != string::npos);
    CHECK(server_entities.find("public static void AddPropertySetter(CritterProperty property, global::FOnline.PropertySetterWithProperty<Critter, CritterProperty, short> setter)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.AddPropertySetterWithProperty(\"Server\", \"Critter\", property.ToString(), setter);") != string::npos);
    CHECK(server_entities.find("public static Dictionary<string, string> ReadConfigSection(\n            string resourcePath,\n            string sectionName\n        )") != string::npos);
    CHECK(server_entities.find("object __result = global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"ReadConfigSection\",") != string::npos);
    CHECK(server_entities.find("return (Dictionary<string, string>)__result;") != string::npos);
    CHECK(server_entities.find("public static Dictionary<string, string> DbGetRecord(hstring collectionName, string id)") != string::npos);
    CHECK(server_entities.find("object __result = global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"DbGetRecord\",") != string::npos);
    CHECK(server_entities.find("public static void DbInsertRecord(\n            hstring collectionName,\n            string id,\n            Dictionary<string, string> keyValues\n        )") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"DbInsertRecord\",") != string::npos);
    CHECK(server_entities.find("public static uint StartTimeEvent(timespan delay, Callback_void func)") != string::npos);
    CHECK(server_entities.find("object __result = global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"StartTimeEvent\",") != string::npos);
    CHECK(server_entities.find("return (uint)__result;") != string::npos);
    CHECK(server_entities.find("public static uint DecodeUtf8(string text, ref int length)") != string::npos);
    CHECK(server_entities.find("object[] __args = new object[]\n            {\n                text,\n                length,\n            };") != string::npos);
    CHECK(server_entities.find("object[] __result = (object[])global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"DecodeUtf8\",") != string::npos);
    CHECK(server_entities.find("length = (int)__result[1];") != string::npos);
    CHECK(server_entities.find("return (uint)__result[0];") != string::npos);
    CHECK(server_entities.find("public static void GetHexInterval(mpos fromHex, mpos toHex, ref ipos hexOffset)") != string::npos);
    CHECK(server_entities.find("object[] __args = new object[]\n            {\n                fromHex,\n                toHex,\n                hexOffset,\n            };") != string::npos);
    CHECK(server_entities.find("object __result = global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"Game\",\n                \"GetHexInterval\",") != string::npos);
    CHECK(server_entities.find("hexOffset = (ipos)__result;") != string::npos);
    CHECK(server_entities.find("public static GameOnManagedTestEvent OnManagedTest") != string::npos);
    CHECK(server_entities.find("private static GameOnManagedTestEvent __event_OnManagedTest;") != string::npos);
    CHECK(server_entities.find("new GameOnManagedTestEvent(IntPtr.Zero)") != string::npos);

    const string server_events = ReadTextFile(script_dir / "ServerEvents.gen.cs");
    CHECK(server_events.find("NotImplementedException") == string::npos);
    CHECK(server_events.find("private readonly IntPtr _entityPtr;") != string::npos);
    CHECK(server_events.find("private GameOnManagedTestEventHandler? _handlers;") == string::npos);
    CHECK(server_events.find("private readonly Dictionary<(Delegate Handler, IntPtr Backend), IntPtr> _nativeSubscriptions") != string::npos);
    CHECK(server_events.find("{ if (handler == null)") == string::npos);
    CHECK(server_events.find("global::FOnline.Native.RequireEventAttribute(handler);\n            IntPtr backend = global::FOnline.Native.GetBackend(\"Server\");\n            (Delegate Handler, IntPtr Backend) key = ((Delegate)handler, backend);\n            if (_nativeSubscriptions.ContainsKey(key))") != string::npos);
    CHECK(server_events.find("_nativeSubscriptions[key] = global::FOnline.Native.SubscribeEvent(\n                \"Server\",\n                \"Game\",\n                \"OnManagedTest\",") != string::npos);
    CHECK(server_events.find("false,\n                (int)priority);") != string::npos);
    CHECK(server_events.find("global::FOnline.Native.UnsubscribeEvent(\n                    \"Server\",\n                    \"OnManagedTest\",\n                    _entityPtr,\n                    subscription);") != string::npos);
    CHECK(server_events.find("EventResult __result = (EventResult)global::FOnline.Native.FireEvent(\n                    \"Server\",\n                    \"Game\",\n                    \"OnManagedTest\",\n                    _entityPtr,\n                    __args);") != string::npos);
    CHECK(server_events.find("return EventResult.StopChain;") != string::npos);
    CHECK(server_events.find("public delegate void GameOnManagedArrayEventHandler(List<int> values)") != string::npos);
    CHECK(server_events.find("object[] __args = new object[]\n                {\n                    values,\n                };") != string::npos);
    CHECK(server_events.find("public delegate void GameOnManagedDictEventHandler(Dictionary<string, string> values)") != string::npos);

    const string client_settings = ReadTextFile(script_dir / "ClientSettings.gen.cs");
    CHECK(client_settings.find("public static List<int> View_GlobalDayColorTime") != string::npos);
    CHECK(client_settings.find("global::FOnline.Native.GetSettingIntList(") != string::npos);
    CHECK(client_settings.find("\"View.GlobalDayColorTime\"") != string::npos);
    CHECK(client_settings.find("public static List<byte> View_GlobalDayColor") != string::npos);
    CHECK(client_settings.find("global::FOnline.Native.GetSettingByteList(") != string::npos);
    CHECK(client_settings.find("\"View.GlobalDayColor\"") != string::npos);

    const string server_types = ReadTextFile(script_dir / "ServerTypes.gen.cs");
    CHECK(server_types.find("public partial struct hstring") != string::npos);
    CHECK(server_types.find("public static hstring FromString(string value)") != string::npos);
    CHECK(server_types.find("public partial class ManagedRoute") != string::npos);
    CHECK(server_types.find("public ManagedRoute()\n        {\n        }") != string::npos);
    CHECK(server_types.find("public int Step") != string::npos);
    CHECK(server_types.find("set;") != string::npos);
    CHECK(server_types.find("public List<int> Values") != string::npos);
    CHECK(server_types.find("private IntPtr _refPtr;") != string::npos);
    CHECK(server_types.find("public ushort GetSpeed()") != string::npos);
    CHECK(server_types.find("object __result = global::FOnline.Native.CallMethod(\n                \"Server\",\n                \"MovingContext\",\n                \"GetSpeed\",") != string::npos);
    CHECK(server_types.find("return (ushort)__result;") != string::npos);
    CHECK(server_entities.find("public partial class Entity") != string::npos);
    CHECK(server_entities.find("public ident Id\n        {\n            get\n            {\n                return new ident(global::FOnline.Native.GetEntityId(_entityPtr));") != string::npos);

    std::error_code ec;
    std::filesystem::remove(script_dir / "UnitProject.gen.csproj", ec);
    ec.clear();
    std::filesystem::remove(script_dir / "ServerEnums.gen.cs", ec);
    ec.clear();
    std::filesystem::remove(script_dir / "UnitProject.gen.sln", ec);
    ec.clear();
    std::filesystem::remove(script_dir / "Native.gen.cs", ec);
    WriteTextFile(script_dir / "NoWorkObsolete.gen.cs", MakeManagedGeneratedCs("namespace Demo { public static class NoWorkObsoleteGeneratedCode {} }\n"));

    ManagedScriptBaker no_work_baker(rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }));
    REQUIRE_NOTHROW(no_work_baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    CHECK(rig.Outputs.empty());
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.csproj"));
    CHECK(std::filesystem::exists(script_dir / "ServerEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.sln"));
    CHECK(std::filesystem::exists(script_dir / "Native.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "NoWorkObsolete.gen.cs"));
#endif
}

TEST_CASE("ManagedScriptBaker packs helper assemblies")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    const std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    const std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    const std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";
    const std::filesystem::path shared_source = script_dir / "Shared.cs";
    const std::filesystem::path fake_msbuild_root = temp_dir.Path() / "Baking" / "TestPack" / "Assemblies";
    const std::filesystem::path fake_msbuild = WriteFakeManagedMsBuildScript(temp_dir.Path());

    WriteTextFile(core_scripts_dir / "Attributes.cs", "namespace FOnline { public sealed class ModuleInitAttribute : System.Attribute { public ModuleInitAttribute(int priority = 0) {} } }\n");
    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(shared_source, "namespace Demo { public static class Shared {} }\n");

    ScopedCurrentPath current_path(temp_dir.Path());
    ScopedUnsetEnvVar dry_run {"FO_MANAGED_SCRIPT_BAKER_DRY_RUN"};
    ScopedEnvVar source_dir {"FO_MANAGED_SOURCE_DIR", managed_source_dir.string()};
    ScopedEnvVar project_dir {"FO_MANAGED_PROJECT_DIR", script_dir.string()};
    ScopedEnvVar assemblies {"FO_MANAGED_ASSEMBLIES", "UnitManaged"};
    ScopedEnvVar sources {"FO_MANAGED_SOURCE", strex("UnitManaged,All,{}", shared_source.string()).str()};
    ScopedEnvVar project_name {"FO_MANAGED_PROJECT_NAME", "UnitProject"};
    ScopedEnvVar msbuild {"FO_MANAGED_MSBUILD", fake_msbuild.string()};
    ScopedEnvVar msbuild_root {"FO_FAKE_MSBUILD_ROOT", fake_msbuild_root.string()};

    TestRig rig;
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    ManagedScriptBaker baker(rig.MakeContext());
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    CHECK(BytesToText(rig.Outputs.at("Assemblies/ServerAssemblies/TestPack.Server.dll")).find("entry-Server") != string::npos);
    CHECK(BytesToText(rig.Outputs.at("Assemblies/ServerAssemblies/SharedDependency.dll")).find("helper-Server") != string::npos);
    CHECK_FALSE(rig.Outputs.contains("Assemblies/ServerAssemblies/TestPack.Server.pdb"));
    CHECK_FALSE(rig.Outputs.contains("Assemblies/ServerAssemblies/TestPack.Server.deps.json"));

    CHECK(BytesToText(rig.Outputs.at("Assemblies/ClientAssemblies/TestPack.Client.dll")).find("entry-Client") != string::npos);
    CHECK(BytesToText(rig.Outputs.at("Assemblies/ClientAssemblies/SharedDependency.dll")).find("helper-Client") != string::npos);
    CHECK_FALSE(rig.Outputs.contains("Assemblies/ClientAssemblies/TestPack.Client.pdb"));
    CHECK_FALSE(rig.Outputs.contains("Assemblies/ClientAssemblies/TestPack.Client.deps.json"));

    CHECK(BytesToText(rig.Outputs.at("Assemblies/MapperAssemblies/TestPack.Mapper.dll")).find("entry-Mapper") != string::npos);
    CHECK(BytesToText(rig.Outputs.at("Assemblies/MapperAssemblies/SharedDependency.dll")).find("helper-Mapper") != string::npos);
    CHECK_FALSE(rig.Outputs.contains("Assemblies/MapperAssemblies/TestPack.Mapper.pdb"));
    CHECK_FALSE(rig.Outputs.contains("Assemblies/MapperAssemblies/TestPack.Mapper.deps.json"));
#endif
}

FO_END_NAMESPACE
