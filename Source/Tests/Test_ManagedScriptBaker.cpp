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
#include "ManagedAssemblyReferences.h"
#include "ManagedInteropAbi.h"
#include "ManagedRuntime.h"
#include "ManagedScriptBackend.h"
#include "ManagedScriptBaker.h"
#include "ManagedScripting.h"

FO_DISABLE_WARNINGS_PUSH()
#include <mono/metadata/object.h>
FO_DISABLE_WARNINGS_POP()
#endif

FO_BEGIN_NAMESPACE

#if FO_MANAGED_SCRIPTING

class ManagedBackendTestMetadata final : public EngineMetadata, public ScriptSystem
{
public:
    ManagedBackendTestMetadata() :
        EngineMetadata {[] { }}
    {
        FO_STACK_TRACE_ENTRY();

        RegisterSide(EngineSideKind::ServerSide);
    }
};

static void SetProcessEnv(string_view name, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    string name_str {name};
    string value_str {value};

#if FO_WINDOWS
    (void)_putenv_s(name_str.c_str(), value_str.c_str());
#else
    (void)setenv(name_str.c_str(), value_str.c_str(), 1);
#endif
}

static void UnsetProcessEnv(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    string name_str {name};

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

class ScopedTempDirectory final
{
public:
    ScopedTempDirectory()
    {
        FO_STACK_TRACE_ENTRY();

        std::filesystem::path base_dir = std::filesystem::temp_directory_path();
        int64_t stamp = numeric_cast<int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        for (uint32_t attempt = 0; attempt < 100; attempt++) {
            std::error_code ec;
            std::filesystem::path candidate = base_dir / fs::make_path(strex("FOnlineManagedScriptBakerTest_{}_{}", stamp, attempt));

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
        (void)std::filesystem::remove_all(_path, ec);
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

    stringstream str;
    str << file.rdbuf();
    return string {str.view()};
}

static auto WriteFakeManagedMsBuildScript(const std::filesystem::path& dir) -> std::filesystem::path
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    std::filesystem::path script_path = dir / "FakeManagedMsBuild.cmd";
    WriteTextFile(script_path, R"(@echo off
if not defined FO_FAKE_MSBUILD_ROOT exit /b 1
echo %* | findstr /C:"-verbosity:quiet" >nul || exit /b 2
if defined FO_FAKE_MSBUILD_FAIL (
    echo error CS0000: fake compile failure
    exit /b 3
)
echo fake-msbuild-output
set "ROOT=%FO_FAKE_MSBUILD_ROOT%"
for %%T in (Server Client Mapper) do (
    mkdir "%ROOT%\%%TAssemblies" 2>nul
    copy /Y "%FO_FAKE_MSBUILD_FIXTURES%\%%TAssemblies\*.dll" "%ROOT%\%%TAssemblies\" >nul || exit /b 4
    > "%ROOT%\%%TAssemblies\TestPack.%%T.pdb" echo pdb-%%T
    > "%ROOT%\%%TAssemblies\TestPack.%%T.deps.json" echo deps-%%T
)
exit /b 0
)");
#else
    std::filesystem::path script_path = dir / "FakeManagedMsBuild.sh";
    WriteTextFile(script_path, R"(#!/bin/sh
if [ -z "$FO_FAKE_MSBUILD_ROOT" ]; then
    exit 1
fi
case " $* " in
    *" -verbosity:quiet "*) ;;
    *) exit 2 ;;
esac
if [ -n "$FO_FAKE_MSBUILD_FAIL" ]; then
    echo "error CS0000: fake compile failure"
    exit 3
fi
echo fake-msbuild-output
for target in Server Client Mapper; do
    mkdir -p "$FO_FAKE_MSBUILD_ROOT/${target}Assemblies"
    cp "$FO_FAKE_MSBUILD_FIXTURES/${target}Assemblies/"*.dll "$FO_FAKE_MSBUILD_ROOT/${target}Assemblies/" || exit 4
    printf 'pdb-%s\n' "$target" > "$FO_FAKE_MSBUILD_ROOT/${target}Assemblies/TestPack.${target}.pdb"
    printf 'deps-%s\n' "$target" > "$FO_FAKE_MSBUILD_ROOT/${target}Assemblies/TestPack.${target}.deps.json"
done
)");
    std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#endif

    return script_path;
}

static auto MakeManagedGeneratedCs(string_view body) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("// <auto-generated />\n"
                 "// This file is generated automatically by ManagedScriptBaker from the current engine metadata\n"
                 "// and scripting content.\n"
                 "// Do not edit it manually; change engine/script/content sources and rebake instead.\n"
                 "// clang-format off\n\n{}",
        body)
        .str();
}

struct ManagedAssemblyImageShape
{
    bool Pe32Plus {};
    bool LargeHeaps {};
    uint32_t TypeRefRows {};
};

static void AppendU16(vector<uint8_t>& out, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    out.emplace_back(numeric_cast<uint8_t>(value & 0xFF));
    out.emplace_back(numeric_cast<uint8_t>((value >> 8) & 0xFF));
}

static void AppendU32(vector<uint8_t>& out, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    AppendU16(out, value & 0xFFFF);
    AppendU16(out, value >> 16);
}

static void AppendIndex(vector<uint8_t>& out, uint32_t value, bool wide)
{
    FO_STACK_TRACE_ENTRY();

    if (wide) {
        AppendU32(out, value);
    }
    else {
        AppendU16(out, value);
    }
}

static void PadToFour(vector<uint8_t>& out)
{
    FO_STACK_TRACE_ENTRY();

    while (out.size() % 4 != 0) {
        out.emplace_back(0);
    }
}

static void WriteU32At(vector<uint8_t>& out, size_t offset, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < 4; i++) {
        out[offset + i] = numeric_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

// Builds the smallest PE image the metadata reader accepts: one section holding the CLI header and a metadata
// root with the tables and strings streams, laid out per ECMA-335 independently of the reader's own code
static auto MakeManagedAssemblyImage(string_view name, const vector<string>& references, ManagedAssemblyImageShape shape = {}) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> strings {0};

    auto add_string = [&strings](string_view value) -> uint32_t {
        uint32_t offset = numeric_cast<uint32_t>(strings.size());
        strings.insert(strings.end(), value.begin(), value.end());
        strings.emplace_back(0);
        return offset;
    };

    uint32_t name_index = add_string(name);
    vector<uint32_t> reference_indices;

    for (const string& reference : references) {
        reference_indices.emplace_back(add_string(reference));
    }

    PadToFour(strings);

    uint32_t reference_rows = numeric_cast<uint32_t>(references.size());
    bool wide_resolution_scope = std::max(reference_rows, shape.TypeRefRows) >= (uint32_t {1} << 14);
    vector<uint8_t> tables;
    AppendU32(tables, 0);
    tables.emplace_back(2);
    tables.emplace_back(0);
    tables.emplace_back(numeric_cast<uint8_t>(shape.LargeHeaps ? 0x07 : 0x00));
    tables.emplace_back(1);
    uint64_t valid = (uint64_t {1} << 0x00) | (uint64_t {1} << 0x20) | (uint64_t {1} << 0x23) | (shape.TypeRefRows != 0 ? uint64_t {1} << 0x01 : 0);
    AppendU32(tables, numeric_cast<uint32_t>(valid & 0xFFFFFFFF));
    AppendU32(tables, numeric_cast<uint32_t>(valid >> 32));
    AppendU32(tables, 0);
    AppendU32(tables, 0);
    AppendU32(tables, 1);

    if (shape.TypeRefRows != 0) {
        AppendU32(tables, shape.TypeRefRows);
    }

    AppendU32(tables, 1);
    AppendU32(tables, reference_rows);

    // Module: Generation, Name, Mvid, EncId, EncBaseId
    AppendU16(tables, 0);
    AppendIndex(tables, name_index, shape.LargeHeaps);
    AppendIndex(tables, 0, shape.LargeHeaps);
    AppendIndex(tables, 0, shape.LargeHeaps);
    AppendIndex(tables, 0, shape.LargeHeaps);

    // TypeRef: ResolutionScope coded index, TypeName, TypeNamespace
    for (uint32_t row = 0; row < shape.TypeRefRows; row++) {
        AppendIndex(tables, 0, wide_resolution_scope);
        AppendIndex(tables, name_index, shape.LargeHeaps);
        AppendIndex(tables, 0, shape.LargeHeaps);
    }

    // Assembly: HashAlgId, four version parts, Flags, PublicKey, Name, Culture
    AppendU32(tables, 0x8004);

    for (int32_t part = 0; part < 4; part++) {
        AppendU16(tables, 1);
    }

    AppendU32(tables, 0);
    AppendIndex(tables, 0, shape.LargeHeaps);
    AppendIndex(tables, name_index, shape.LargeHeaps);
    AppendIndex(tables, 0, shape.LargeHeaps);

    // AssemblyRef: four version parts, Flags, PublicKeyOrToken, Name, Culture, HashValue
    for (uint32_t reference_index : reference_indices) {
        for (int32_t part = 0; part < 4; part++) {
            AppendU16(tables, 1);
        }

        AppendU32(tables, 0);
        AppendIndex(tables, 0, shape.LargeHeaps);
        AppendIndex(tables, reference_index, shape.LargeHeaps);
        AppendIndex(tables, 0, shape.LargeHeaps);
        AppendIndex(tables, 0, shape.LargeHeaps);
    }

    PadToFour(tables);

    constexpr uint32_t metadata_headers_size = 64;
    vector<uint8_t> metadata;
    AppendU32(metadata, 0x424A5342);
    AppendU16(metadata, 1);
    AppendU16(metadata, 1);
    AppendU32(metadata, 0);
    AppendU32(metadata, 12);
    string_view version = "v4.0.30319";
    metadata.insert(metadata.end(), version.begin(), version.end());
    PadToFour(metadata);
    AppendU16(metadata, 0);
    AppendU16(metadata, 2);
    AppendU32(metadata, metadata_headers_size);
    AppendU32(metadata, numeric_cast<uint32_t>(tables.size()));
    metadata.insert(metadata.end(), {'#', '~', 0, 0});
    AppendU32(metadata, metadata_headers_size + numeric_cast<uint32_t>(tables.size()));
    AppendU32(metadata, numeric_cast<uint32_t>(strings.size()));
    metadata.insert(metadata.end(), {'#', 'S', 't', 'r', 'i', 'n', 'g', 's', 0, 0, 0, 0});
    REQUIRE(metadata.size() == metadata_headers_size);
    metadata.insert(metadata.end(), tables.begin(), tables.end());
    metadata.insert(metadata.end(), strings.begin(), strings.end());

    constexpr uint32_t section_rva = 0x2000;
    constexpr uint32_t section_file_offset = 0x200;
    constexpr uint32_t cli_header_size = 72;
    vector<uint8_t> section;
    AppendU32(section, cli_header_size);
    AppendU16(section, 2);
    AppendU16(section, 5);
    AppendU32(section, section_rva + cli_header_size);
    AppendU32(section, numeric_cast<uint32_t>(metadata.size()));
    section.resize(cli_header_size, 0);
    section.insert(section.end(), metadata.begin(), metadata.end());

    uint32_t optional_header_size = shape.Pe32Plus ? 240 : 224;
    size_t optional_header_offset = 0x80 + 24;
    vector<uint8_t> image(section_file_offset, 0);
    image[0] = 'M';
    image[1] = 'Z';
    WriteU32At(image, 0x3C, 0x80);
    WriteU32At(image, 0x80, 0x00004550);
    image[0x84] = 0x4C;
    image[0x85] = 0x01;
    image[0x86] = 1;
    image[0x80 + 20] = numeric_cast<uint8_t>(optional_header_size & 0xFF);
    image[optional_header_offset] = 0x0B;
    image[optional_header_offset + 1] = numeric_cast<uint8_t>(shape.Pe32Plus ? 0x02 : 0x01);
    size_t directory_count_offset = optional_header_offset + (shape.Pe32Plus ? 108 : 92);
    WriteU32At(image, directory_count_offset, 16);
    WriteU32At(image, directory_count_offset + 4 + 14 * 8, section_rva);
    WriteU32At(image, directory_count_offset + 8 + 14 * 8, cli_header_size);

    size_t section_header_offset = optional_header_offset + optional_header_size;
    image[section_header_offset] = '.';
    image[section_header_offset + 1] = 't';
    WriteU32At(image, section_header_offset + 8, numeric_cast<uint32_t>(section.size()));
    WriteU32At(image, section_header_offset + 12, section_rva);
    WriteU32At(image, section_header_offset + 16, numeric_cast<uint32_t>(section.size()));
    WriteU32At(image, section_header_offset + 20, section_file_offset);

    image.insert(image.end(), section.begin(), section.end());
    return image;
}

static void WriteBinaryFile(const std::filesystem::path& path, const vector<uint8_t>& data)
{
    FO_STACK_TRACE_ENTRY();

    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    REQUIRE(file.good());
    auto data_chars = make_ptr(data.data()).reinterpret_as<const char>();
    file.write(data_chars.get(), numeric_cast<std::streamsize>(data.size()));
}

#endif

TEST_CASE("Managed runtime resources are restored into a content-addressed cache")
{
#if FO_MANAGED_SCRIPTING
    ScopedTempDirectory temp_dir;
    auto resource_root = temp_dir.Path() / "Resources";
    auto cache_root = temp_dir.Path() / "Cache";
    WriteTextFile(resource_root / "ManagedRuntime" / "lib" / "netcoreapp" / "System.Private.CoreLib.dll", "managed-corelib\n");
    WriteTextFile(resource_root / "ManagedRuntime" / "runtime.manifest", "manifest\n");

    FileSystem resources;
    resources.AddDirSource(resource_root.string(), true);

    auto restored = RestoreManagedRuntimeResources(resources, cache_root.string());
    REQUIRE(restored.has_value());
    CHECK(restored->parent_path() == cache_root / "ManagedRuntime");
    auto restored_corelib = *restored / "lib" / "netcoreapp" / "System.Private.CoreLib.dll";
    CHECK(ReadTextFile(restored_corelib) == "managed-corelib\n");

    WriteTextFile(restored_corelib, "damaged\n");
    auto restored_again = RestoreManagedRuntimeResources(resources, cache_root.string());
    REQUIRE(restored_again == restored);
    CHECK(ReadTextFile(restored_corelib) == "managed-corelib\n");
#endif
}

TEST_CASE("Managed assembly references are read from the metadata tables")
{
#if FO_MANAGED_SCRIPTING
    bool pe32_plus = GENERATE(false, true);
    bool large_heaps = GENERATE(false, true);
    // 0x4000 type references widen the ResolutionScope coded index, which shifts every table after TypeRef
    uint32_t type_ref_rows = GENERATE(uint32_t {0}, uint32_t {3}, uint32_t {0x4000});
    vector<string> references {"System.Runtime", "System.Collections", "Unit.Helper"};
    vector<uint8_t> image = MakeManagedAssemblyImage("Unit.Scripts", references, ManagedAssemblyImageShape {.Pe32Plus = pe32_plus, .LargeHeaps = large_heaps, .TypeRefRows = type_ref_rows});

    ManagedAssemblyIdentity identity = ReadManagedAssemblyIdentity(image);

    CHECK(identity.Name == "Unit.Scripts");
    CHECK(identity.References == references);
    CHECK(ReadManagedAssemblyIdentity(MakeManagedAssemblyImage("Leaf", {})).References.empty());
#endif
}

TEST_CASE("Managed assembly reader rejects images that are not managed assemblies")
{
#if FO_MANAGED_SCRIPTING
    vector<uint8_t> image = MakeManagedAssemblyImage("Unit.Scripts", {"System.Runtime"});

    SECTION("Truncated")
    {
        for (size_t length : {size_t {0}, size_t {0x40}, size_t {0x100}, size_t {0x248}, image.size() - 8}) {
            vector<uint8_t> truncated {image.begin(), image.begin() + numeric_cast<ptrdiff_t>(length)};
            CHECK_THROWS_AS(ReadManagedAssemblyIdentity(truncated), ManagedAssemblyReferencesException);
        }
    }

    SECTION("NativePe")
    {
        // A zero CLR runtime header directory is how a native DLL answers
        size_t directory_count_offset = 0x80 + 24 + 92;
        std::fill_n(image.begin() + numeric_cast<ptrdiff_t>(directory_count_offset + 4 + 14 * 8), 8, uint8_t {0});
        CHECK_THROWS_WITH(ReadManagedAssemblyIdentity(image), Catch::Matchers::ContainsSubstring("native PE"));
    }

    SECTION("Text")
    {
        string_view text = "entry-Server\n";
        CHECK_THROWS_AS(ReadManagedAssemblyIdentity(vector<uint8_t> {text.begin(), text.end()}), ManagedAssemblyReferencesException);
    }
#endif
}

TEST_CASE("Managed runtime selection follows assembly references")
{
#if FO_MANAGED_SCRIPTING
    map<string, ManagedAssemblyIdentity> runtime {
        {"System.Private.CoreLib", ManagedAssemblyIdentity {.Name = "System.Private.CoreLib", .References = {}}},
        {"System.Runtime", ManagedAssemblyIdentity {.Name = "System.Runtime", .References = {"System.Private.CoreLib", "System.Private.Uri"}}},
        {"System.Private.Uri", ManagedAssemblyIdentity {.Name = "System.Private.Uri", .References = {"System.Private.CoreLib"}}},
        {"System.Linq", ManagedAssemblyIdentity {.Name = "System.Linq", .References = {"System.Collections"}}},
        {"System.Collections", ManagedAssemblyIdentity {.Name = "System.Collections", .References = {"System.Linq"}}},
        {"System.Xml", ManagedAssemblyIdentity {.Name = "System.Xml", .References = {"System.Private.CoreLib"}}},
        {"Renamed.File", ManagedAssemblyIdentity {.Name = "Other.Assembly", .References = {}}},
        {"mscorlib", ManagedAssemblyIdentity {.Name = "mscorlib", .References = {"System.Runtime", "System.Security.Permissions"}}},
    };
    vector<string> lookups;

    auto find_runtime_assembly = [&](string_view name) -> optional<ManagedAssemblyIdentity> {
        lookups.emplace_back(name);
        auto it = runtime.find(name);
        return it != runtime.end() ? optional<ManagedAssemblyIdentity> {it->second} : std::nullopt;
    };

    SECTION("ClosureOverPackAndRuntime")
    {
        vector<ManagedAssemblyIdentity> pack {
            ManagedAssemblyIdentity {.Name = "Scripts.Server", .References = {"System.Runtime", "Unit.Helper", "System.Linq"}},
            ManagedAssemblyIdentity {.Name = "Unit.Helper", .References = {"System.Runtime"}},
        };

        set<string> selected = CollectReferencedRuntimeAssemblies(pack, find_runtime_assembly);

        // A pack assembly is never looked up in the runtime, a cycle ends, and an unreferenced library stays out
        CHECK(selected == set<string> {"System.Collections", "System.Linq", "System.Private.CoreLib", "System.Private.Uri", "System.Runtime"});
        CHECK(std::ranges::count(lookups, "Unit.Helper") == 0);
        CHECK(std::ranges::count(lookups, "System.Runtime") == 1);
    }

    SECTION("CoreLibWithoutReferences")
    {
        CHECK(CollectReferencedRuntimeAssemblies({}, find_runtime_assembly) == set<string> {"System.Private.CoreLib"});
    }

    SECTION("MissingCoreLib")
    {
        runtime.erase("System.Private.CoreLib");
        CHECK_THROWS_WITH(CollectReferencedRuntimeAssemblies({}, find_runtime_assembly), Catch::Matchers::ContainsSubstring("does not publish CoreLib"));
    }

    SECTION("UnresolvedReference")
    {
        vector<ManagedAssemblyIdentity> pack {ManagedAssemblyIdentity {.Name = "Scripts.Client", .References = {"System.Runtime", "Missing.Library"}}};
        CHECK_THROWS_WITH(CollectReferencedRuntimeAssemblies(pack, find_runtime_assembly), Catch::Matchers::ContainsSubstring("satisfied neither") && Catch::Matchers::ContainsSubstring("Missing.Library") && Catch::Matchers::ContainsSubstring("Scripts.Client"));
    }

    SECTION("FacadeForwardsOutsideTheRuntime")
    {
        vector<ManagedAssemblyIdentity> pack {ManagedAssemblyIdentity {.Name = "NetStandard.Helper", .References = {"mscorlib"}}};
        CHECK(CollectReferencedRuntimeAssemblies(pack, find_runtime_assembly) == set<string> {"System.Private.CoreLib", "System.Private.Uri", "System.Runtime", "mscorlib"});
    }

    SECTION("FileDefinesAnotherAssembly")
    {
        vector<ManagedAssemblyIdentity> pack {ManagedAssemblyIdentity {.Name = "Scripts.Client", .References = {"Renamed.File"}}};
        CHECK_THROWS_WITH(CollectReferencedRuntimeAssemblies(pack, find_runtime_assembly), Catch::Matchers::ContainsSubstring("another assembly"));
    }

    SECTION("CoreLibDefinesAnotherAssembly")
    {
        runtime.at("System.Private.CoreLib").Name = "Wrong.CoreLib";
        CHECK_THROWS_WITH(CollectReferencedRuntimeAssemblies({}, find_runtime_assembly), Catch::Matchers::ContainsSubstring("CoreLib file defines another assembly"));
    }
#endif
}

TEST_CASE("Managed assembly reader reads every published class library")
{
#if FO_MANAGED_SCRIPTING
    // The synthetic images share the reader's reading of ECMA-335, so the real runtime is the independent check
    optional<std::filesystem::path> runtime_dir = FindManagedRuntimeDirectory();
    REQUIRE(runtime_dir.has_value());

    map<string, ManagedAssemblyIdentity> class_libraries;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(*runtime_dir / "lib" / "netcoreapp")) {
        if (entry.path().extension() != ".dll") {
            continue;
        }

        INFO(entry.path().string());
        optional<string> image = fs::read_file(fs::path_to_string(entry.path()));
        REQUIRE(image.has_value());
        ManagedAssemblyIdentity identity = ReadManagedAssemblyIdentity(vector<uint8_t>(image->begin(), image->end()));
        CHECK(identity.Name == strex("{}", entry.path().stem().string()).str());
        class_libraries.emplace(identity.Name, std::move(identity));
    }

    REQUIRE(class_libraries.contains("System.Private.CoreLib"));
    CHECK(class_libraries.at("System.Private.CoreLib").References.empty());
    REQUIRE(class_libraries.contains("System.Runtime"));
    CHECK(std::ranges::count(class_libraries.at("System.Runtime").References, "System.Private.CoreLib") == 1);

    auto find_runtime_assembly = [&class_libraries](string_view name) -> optional<ManagedAssemblyIdentity> {
        auto it = class_libraries.find(name);
        return it != class_libraries.end() ? optional<ManagedAssemblyIdentity> {it->second} : std::nullopt;
    };

    // Every class library selected at once: the facades' references into unpublished packages must not fail it
    vector<ManagedAssemblyIdentity> everything;

    for (const auto& [name, identity] : class_libraries) {
        everything.emplace_back(ManagedAssemblyIdentity {.Name = strex("Unit.Root.{}", name).str(), .References = {name}});
    }

    CHECK(CollectReferencedRuntimeAssemblies(everything, find_runtime_assembly).size() == class_libraries.size());
#endif
}

TEST_CASE("Managed scripting rejects metadata without a script system")
{
#if FO_MANAGED_SCRIPTING
    EngineMetadata metadata {[] { }};
    FileSystem resources;
    CHECK_THROWS_WITH(InitManagedScripting(&metadata, &resources, "Cache"), Catch::Matchers::ContainsSubstring("Managed scripting requires a script system"));
#endif
}

TEST_CASE("Managed scripting releases adopted persistent GC handles during backend shutdown")
{
#if FO_MANAGED_SCRIPTING
    ManagedBackendTestMetadata metadata;
    FileSystem resources;
    InitManagedScripting(&metadata, &resources, "Cache");

    nptr<ManagedScriptBackend> backend = metadata.GetBackend<ManagedScriptBackend>(ScriptSystemBackend::MANAGED_BACKEND_INDEX);
    REQUIRE(backend);

    MonoDomain* domain = static_cast<MonoDomain*>(backend->GetDomain());
    REQUIRE(domain != nullptr);
    MonoString* value = mono_string_new(domain, "persistent-handle-lifecycle");
    REQUIRE(value != nullptr);
    uint32_t gc_handle = mono_gchandle_new(reinterpret_cast<MonoObject*>(value), false);
    REQUIRE(gc_handle != 0);
    REQUIRE(mono_gchandle_get_target(gc_handle) == reinterpret_cast<MonoObject*>(value));

    bool handle_adopted = false;
    bool backend_shutdown = false;
    auto cleanup = scope_exit([&]() noexcept {
        if (!handle_adopted) {
            mono_gchandle_free(gc_handle);
        }
        if (!backend_shutdown) {
            metadata.ShutdownBackends();
        }
    });

    backend->AdoptPersistentGcHandle(gc_handle);
    handle_adopted = true;
    metadata.ShutdownBackends();
    backend_shutdown = true;

    CHECK(mono_gchandle_get_target(gc_handle) == nullptr);
#endif
}

TEST_CASE("Managed ABI native frames align packed slots and copy back only outputs")
{
#if FO_MANAGED_SCRIPTING
    alignas(std::max_align_t) array<uint8_t, 40> bytes {};
    span<uint8_t> packed {bytes.data() + 1, 32};
    array<ManagedAbiSlot, 3> args {{
        {.Kind = ManagedAbiValueKind::UInt8, .Size = 1, .Offset = 0},
        {.Kind = ManagedAbiValueKind::Float64, .Mutable = true, .Size = 8, .Offset = 1},
        {.Kind = ManagedAbiValueKind::Handle, .Size = MANAGED_ABI_HANDLE_SLOT_SIZE, .Offset = 9},
    }};
    ManagedAbiSlot ret {.Kind = ManagedAbiValueKind::Int64, .Size = 8, .Offset = 17};
    float64_t input = 12.5;
    uint64_t handle = 0x12345678;
    packed[0] = 7;
    memory::copy(packed.data() + 1, &input, sizeof(input));
    memory::copy(packed.data() + 9, &handle, sizeof(handle));

    ManagedAbiNativeFrame frame = BuildManagedAbiNativeFrame(packed, args, ret);

    for (size_t i = 0; i < args.size(); i++) {
        REQUIRE(reinterpret_cast<uintptr_t>(GetManagedAbiNativeFrameArg(frame, i).get()) % alignof(std::max_align_t) == 0);
    }

    REQUIRE(reinterpret_cast<uintptr_t>(GetManagedAbiNativeFrameResult(frame).get()) % alignof(std::max_align_t) == 0);
    CHECK(*GetManagedAbiNativeFrameArg(frame, 0).reinterpret_as<uint8_t>() == 7);
    CHECK(*GetManagedAbiNativeFrameArg(frame, 1).reinterpret_as<float64_t>() == input);
    CHECK(*GetManagedAbiNativeFrameArg(frame, 2).reinterpret_as<uint64_t>() == handle);
    *GetManagedAbiNativeFrameArg(frame, 0).reinterpret_as<uint8_t>() = 99;
    *GetManagedAbiNativeFrameArg(frame, 1).reinterpret_as<float64_t>() = -25.0;
    *GetManagedAbiNativeFrameResult(frame).reinterpret_as<int64_t>() = -123456789;
    CopyBackManagedAbiNativeFrame(frame);

    float64_t rewritten = 0;
    int64_t result = 0;
    memory::copy(&rewritten, packed.data() + 1, sizeof(rewritten));
    memory::copy(&result, packed.data() + 17, sizeof(result));
    CHECK(packed[0] == 7);
    CHECK(rewritten == -25.0);
    CHECK(result == -123456789);
    CHECK(bytes.front() == 0);
    CHECK(bytes.back() == 0);

    ManagedAbiNativeFrame event_frame = BuildManagedAbiNativeFrame(packed, args);
    CHECK_FALSE(GetManagedAbiNativeFrameResult(event_frame));
    CHECK_THROWS(GetManagedAbiNativeFrameArg(frame, args.size()));
    CHECK_THROWS(BuildManagedAbiNativeFrame(packed.first(2), args));
#endif
}

TEST_CASE("ManagedScriptBaker")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path managed_reference = managed_source_dir / "References" / "ManagedDependency.dll";
    std::filesystem::path managed_analyzer = managed_source_dir / "Analyzers" / "ManagedAnalyzer.csproj";
    std::filesystem::path banned_symbols = managed_source_dir / "Analyzers" / "BannedSymbols.txt";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";

    WriteTextFile(core_scripts_dir / "Attributes.cs", "namespace FOnline { public sealed class ModuleInitAttribute : System.Attribute { public ModuleInitAttribute(int priority = 0) {} } }\n");
    WriteTextFile(core_scripts_dir / "CoreTagged.cs", "namespace FOnline {\n    ///@ Enum CoreMirrorTag A\n    public static class CoreTagged {}\n}\n");
    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(managed_reference, "managed-reference\n");
    WriteTextFile(managed_analyzer, "<Project />\n");
    WriteTextFile(managed_source_dir / "Compiler" / "UnitCompiler.csproj", "<Project />\n");
    WriteTextFile(banned_symbols, "M:System.Environment.Exit(System.Int32);Process lifetime is not the script layer's decision.\n");

    std::filesystem::path server_source = script_dir / "ServerOnly.cs";
    std::filesystem::path client_source = script_dir / "ClientOnly.cs";
    std::filesystem::path mapper_source = script_dir / "MapperOnly.cs";
    std::filesystem::path shared_source = script_dir / "Shared.cs";
    std::filesystem::path tilde_source = script_dir / "Tilde~1.cs";

    WriteTextFile(server_source,
        "#if SERVER\n"
        "namespace Demo { public static class ServerOnly {\n"
        "[ItemInit]\n"
        "public static void InitDoor(Item item, bool firstTime) {}\n"
        "[ItemStatic]\n"
        "public static bool UseStatic(Critter cr, StaticItem staticItem, Item item, any param) { return true; }\n"
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

    std::filesystem::path work_dir = temp_dir.Path() / "Work";
    std::filesystem::create_directories(work_dir);
    WriteTextFile(temp_dir.Path() / "ManagedRoot.fomain", "");
    ScopedCurrentPath current_path(work_dir);

    TestRig rig;
    rig.Settings.ApplyConfigAtPath("ManagedRoot.fomain", temp_dir.Path().string());
    OverrideSetting(rig.Settings.Baking.BakeOutput, string {"Baking"});
    OverrideSetting(rig.Settings.ManagedScript.BakerDryRun, true);
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {"ManagedSupport/CoreScripts", "Scripts/Managed"});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, "Scripts/Managed");
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ExtraSources,
        vector<string> {
            "UnitManaged,Server,Scripts/Managed/ServerOnly.cs",
            "UnitManaged,Client,Scripts/Managed/ClientOnly.cs",
            "UnitManaged,Mapper,Scripts/Managed/MapperOnly.cs",
            "UnitManaged,All,Scripts/Managed/Shared.cs",
            "UnitManaged,All,Scripts/Managed/Tilde~1.cs",
        });
    OverrideSetting(rig.Settings.ManagedScript.ExtraReferences, vector<string> {"UnitManaged,Server,System.Xml", "UnitManaged,Server,ManagedSupport/References/ManagedDependency.dll", "UnitManaged,All,System.Core", "UnitManaged,Server,ManagedSupport/Compiler/UnitCompiler.csproj"});
    OverrideSetting(rig.Settings.ManagedScript.Analyzers, vector<string> {"ManagedSupport/Analyzers/ManagedAnalyzer.csproj"});
    OverrideSetting(rig.Settings.ManagedScript.AnalyzerPackages, vector<string> {"Unit.Analyzer,1.2.3", "Unit.Banned.Analyzer,4.5.6"});
    OverrideSetting(rig.Settings.ManagedScript.AdditionalFiles, vector<string> {"ManagedSupport/Analyzers/BannedSymbols.txt"});
    OverrideSetting(rig.Settings.ManagedScript.AnalysisLevel, "10.0");
    OverrideSetting(rig.Settings.ManagedScript.AnalysisMode, "All");
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitProject");
    rig.AddBakedFile("Metadata.fometa-server",
        MakeMetadataBlob({
            {"Entity", {{"ManagedInner", "HasProtos"}, {"ManagedGlobal"}}},
            {"EntityHolder", {{"Server", "Critter", "ManagedInner", "ManagedEntry"}, {"Server", "Game", "ManagedGlobal", "ManagedGlobal"}}},
            {"Event", {{"Game", "OnManagedTest", "int32", "", "value"}, {"Critter", "OnManagedTouched", "Critter", "", "other", "int32", "", "power"}, {"Game", "OnManagedArray", "int32 []", "", "values"}, {"Game", "OnManagedDict", "string = > string", "", "values"}, {"Game", "OnManagedMutablePosition", "int32", "", "first", "int32", "", "second", "int32 &", "", "third"}}},
            {"Property",
                {{"Game", "Server", "string", "ManagedTitle", "Mutable"}, {"Game", "Server", "ManagedRoute", "ManagedRouteValue", "Mutable"}, {"Game", "Server", "int32 []", "ManagedSteps", "Mutable"}, {"Game", "Server", "uint16", "ManagedNarrowLimit", "Mutable"}, {"Critter", "Server", "int16", "ManagedSkill", "Mutable"}, {"Critter", "Server", "int8", "ManagedInt8", "Mutable"}, {"Critter", "Server", "uint8", "ManagedUInt8", "Mutable"}, {"Critter", "Server", "int32", "ManagedInt32", "Mutable"}, {"Critter", "Server", "uint32", "ManagedUInt32", "Mutable"}, {"Critter", "Server", "int64", "ManagedInt64", "Mutable"}, {"Critter", "Server", "uint64", "ManagedUInt64", "Mutable"}, {"Critter", "Server", "float32", "ManagedFloat32", "Mutable"}, {"Critter", "Server", "float64", "ManagedFloat64", "Mutable"}, {"Critter", "Server", "bool", "ManagedBool", "Mutable"}, {"Critter", "Server", "CritterCondition", "ManagedEnum", "Mutable"}, {"Critter", "Server", "bool", "ManagedProbe", "Component"},
                    {"Critter", "Server", "int32", "ManagedProbe.Value"}, {"Critter", "Server", "mpos", "ManagedHex", "Mutable"}, {"Critter", "Server", "ucolor", "ManagedTint", "Mutable"}, {"Critter", "Server", "hstring=>hstring[]", "ManagedCheckpointEntries", "Mutable"}, {"Critter", "Server", "int32=>string[]", "ManagedTextGroups", "Mutable"}, {"Critter", "Server", "any", "ManagedAnyValue", "Mutable"}, {"Critter", "Server", "any[]", "ManagedAnyList", "Mutable"}}},
            {"RefType", {{"ManagedRoute", "Step", "int32", "0", "Note", "string", "0", "Values", "int32[]", "0", "Checkpoint", "bool", "1", "Component", "Checkpoint.Index", "int32", "0", "Checkpoint.Label", "string", "0"}}},
            {"RemoteCall", {{"ManagedMoveProbe", "UnitManaged.cs", "In", "int32", "", "step", "mpos", "", "hex", "Limits", "0", "0"}, {"ManagedMoveProbeTwin", "UnitManaged.cs", "In", "int32", "", "step", "mpos", "", "hex", "Limits", "0", "0"}, {"ManagedTextProbe", "UnitManaged.cs", "In", "string", "", "text", "Limits", "0", "0"}}},
        }));
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    ManagedScriptBaker baker(rig.MakeContext());
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Metadata.fometa-server"));
    CHECK(rig.Outputs.empty());

    int32_t blocked_output_path = GENERATE(0, 1, 2);
    CAPTURE(blocked_output_path);

    if (blocked_output_path) {
        auto blocked_output = script_dir / (blocked_output_path == 1 ? "ServerEnums.gen.cs" : "ServerEnums.gen.cs.tmp");
        REQUIRE(std::filesystem::create_directory(blocked_output));
        CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(std::filesystem::is_directory(blocked_output));
        if (blocked_output_path == 1) {
            CHECK_FALSE(std::filesystem::exists(script_dir / "ServerEnums.gen.cs.tmp"));
        }
        return;
    }

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    CHECK(rig.Outputs.contains("Assemblies/Assemblies-server/TestPack.Server.dll"));
    CHECK(rig.Outputs.contains("Assemblies/Assemblies-client/TestPack.Client.dll"));
    CHECK(rig.Outputs.contains("Assemblies/Assemblies-mapper/TestPack.Mapper.dll"));

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
    CHECK(std::filesystem::exists(script_dir / "FOnline.ManagedHost.gen.csproj"));
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.sln"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "ServerEnums.cs"));
    CHECK(std::filesystem::exists(script_dir / "ServerEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "ServerAbi.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "ClientEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "MapperEnums.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(work_dir / "Scripts"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Attributes.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "CoreTagged.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Initializator.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Native.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "Obsolete.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "ObsoleteOwned.gen.cs"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Obsolete.gen.csproj"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "Obsolete.gen.sln"));
    CHECK(std::filesystem::exists(script_dir / "Obsolete.gen.txt"));

    string solution = ReadTextFile(script_dir / "UnitProject.gen.sln");
    CHECK(solution.find("Microsoft Visual Studio Solution File, Format Version 12.00") != string::npos);
    CHECK(solution.find("# <auto-generated />") != string::npos);
    CHECK(solution.find("Do not edit it manually") != string::npos);
    CHECK(solution.find("Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"UnitProject\", \"UnitProject.gen.csproj\"") != string::npos);
    CHECK(solution.find("Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"FOnline.ManagedHost\", \"FOnline.ManagedHost.gen.csproj\"") != string::npos);
    CHECK(solution.find("Server|AnyCPU = Server|AnyCPU") != string::npos);
    CHECK(solution.find("Client|AnyCPU = Client|AnyCPU") != string::npos);
    CHECK(solution.find("Mapper|AnyCPU = Mapper|AnyCPU") != string::npos);

    string unified_project = ReadTextFile(script_dir / "UnitProject.gen.csproj");
    CHECK(unified_project.find("<auto-generated />") != string::npos);
    CHECK(unified_project.find("Do not edit it manually") != string::npos);
    CHECK(unified_project.find("<Configurations>Server;Client;Mapper</Configurations>") != string::npos);
    CHECK(unified_project.find("<AssemblyName>TestPack.Server</AssemblyName>") != string::npos);
    CHECK(unified_project.find("<AssemblyName>TestPack.Client</AssemblyName>") != string::npos);
    CHECK(unified_project.find("<AssemblyName>TestPack.Mapper</AssemblyName>") != string::npos);
    CHECK(unified_project.find("TRACE;SERVER") != string::npos);
    CHECK(unified_project.find("<OutputPath>$(FOnlineBakeRoot)/") != string::npos);
    CHECK(unified_project.find("/TestPack/Assemblies/ServerAssemblies/</OutputPath>") != string::npos);
    CHECK(unified_project.find("ServerEnums.gen.cs") != string::npos);
    CHECK(unified_project.find("ServerAbi.gen.cs") != string::npos);
    CHECK(unified_project.find("ClientEnums.gen.cs") != string::npos);
    CHECK(unified_project.find("MapperEnums.gen.cs") != string::npos);
    CHECK(unified_project.find("CoreScripts/Attributes.cs") != string::npos);
    CHECK(unified_project.find("CoreScripts/Initializator.cs") != string::npos);
    CHECK(unified_project.find("CoreScripts/Native.cs") != string::npos);
    CHECK(unified_project.find("Native.gen.cs") == string::npos);
    CHECK(unified_project.find("ServerOnly.cs") != string::npos);
    CHECK(unified_project.find("ClientOnly.cs") != string::npos);
    CHECK(unified_project.find("MapperOnly.cs") != string::npos);
    CHECK(unified_project.find("Shared.cs") != string::npos);
    CHECK(unified_project.find("Tilde~1.cs") != string::npos);
    CHECK(unified_project.find("System.Core") != string::npos);
    CHECK(unified_project.find("../../ManagedSupport/References/ManagedDependency.dll") != string::npos);
    CHECK(unified_project.find("../../ManagedSupport/Analyzers/ManagedAnalyzer.csproj") != string::npos);
    CHECK(unified_project.find("System.Xml") != string::npos);
    CHECK(unified_project.find("Obsolete.gen.cs") != string::npos);
    CHECK(unified_project.find("<ProjectReference Include=\"FOnline.ManagedHost.gen.csproj\" />") != string::npos);

    // A project among the extra references builds as itself, and only the target naming it copies its package assemblies
    CHECK(unified_project.find("<ProjectReference Include=\"../../ManagedSupport/Compiler/UnitCompiler.csproj\" GlobalPropertiesToRemove=\"OutputPath;Configuration;Platform\" />") != string::npos);
    CHECK(unified_project.find("<Reference Include=\"UnitCompiler\">") == string::npos);
    CHECK(unified_project.find("<DefineConstants>TRACE;SERVER</DefineConstants>\n    <CopyLocalLockFileAssemblies>true</CopyLocalLockFileAssemblies>\n    <SatelliteResourceLanguages>en</SatelliteResourceLanguages>") != string::npos);
    CHECK(unified_project.find("<DefineConstants>TRACE;CLIENT</DefineConstants>\n  </PropertyGroup>") != string::npos);
    CHECK(unified_project.find("<DefineConstants>TRACE;MAPPER</DefineConstants>\n  </PropertyGroup>") != string::npos);

    // Every target keeps its portable PDB inside the assembly, so script frames carry file and line wherever it runs
    for (string_view target : {"Server", "Client", "Mapper"}) {
        CHECK(unified_project.find(strex("== '{}|AnyCPU' \">\n    <DebugType>embedded</DebugType>", target).str()) != string::npos);
    }

    // The analysis profile: level and mode as properties, analyzer packages as private package references,
    // and the analyzer configuration file as an AdditionalFiles item beside them
    CHECK(unified_project.find("<EnableNETAnalyzers>true</EnableNETAnalyzers>") != string::npos);
    CHECK(unified_project.find("<AnalysisLevel>10.0</AnalysisLevel>") != string::npos);
    CHECK(unified_project.find("<AnalysisMode>All</AnalysisMode>") != string::npos);
    CHECK(unified_project.find("<PackageReference Include=\"Unit.Analyzer\" Version=\"1.2.3\" PrivateAssets=\"all\" />") != string::npos);
    CHECK(unified_project.find("<PackageReference Include=\"Unit.Banned.Analyzer\" Version=\"4.5.6\" PrivateAssets=\"all\" />") != string::npos);
    CHECK(unified_project.find("<AdditionalFiles Include=\"../../ManagedSupport/Analyzers/BannedSymbols.txt\" />") != string::npos);

    string managed_host_project = ReadTextFile(script_dir / "FOnline.ManagedHost.gen.csproj");
    CHECK(managed_host_project.find("<AssemblyName>FOnline.ManagedHost</AssemblyName>") != string::npos);
    // Restore writes project.assets.json per intermediate directory, and the script project shares this directory, so
    // the host restores into its own one, named before the SDK props read it
    size_t host_intermediate_pos = managed_host_project.find("<BaseIntermediateOutputPath>obj/FOnline.ManagedHost/</BaseIntermediateOutputPath>");
    size_t host_sdk_props_pos = managed_host_project.find("<Import Project=\"Sdk.props\" Sdk=\"Microsoft.NET.Sdk\" />");
    CHECK(host_intermediate_pos != string::npos);
    CHECK(host_sdk_props_pos != string::npos);
    CHECK(host_intermediate_pos < host_sdk_props_pos);
    CHECK(managed_host_project.find("<Import Project=\"Sdk.targets\" Sdk=\"Microsoft.NET.Sdk\" />") != string::npos);
    CHECK(managed_host_project.find("<Project Sdk=") == string::npos);
    CHECK(managed_host_project.find("ManagedHost/ManagedLoadContextHost.cs") != string::npos);
    CHECK(managed_host_project.find("<DebugType>embedded</DebugType>") != string::npos);
    // The profile covers the script project only; the host compiles engine-owned source
    CHECK(managed_host_project.find("<AnalysisMode>") == string::npos);
    CHECK(managed_host_project.find("<PackageReference") == string::npos);

    string server_enums = ReadTextFile(script_dir / "ServerEnums.gen.cs");
    CHECK(server_enums.find("// <auto-generated />") == 0);
    CHECK(server_enums.find("Do not edit it manually") != string::npos);
    CHECK(server_enums.find("namespace FOnline;\n\nusing System;") != string::npos);
    CHECK(server_enums.find("namespace FOnline\n{") == string::npos);

    string server_entities = ReadTextFile(script_dir / "ServerEntities.gen.cs");
    CHECK(server_entities.find("NotImplementedException") == string::npos);
    CHECK(server_entities.find("NotSupportedException") == string::npos);
    CHECK(server_entities.find("public static string ManagedTitle") != string::npos);
    // A value no fixed layout carries stays boxed, but travels by registrar index: no owner or property name crosses
    CHECK(server_entities.find("public static string ManagedTitle\n    {\n        get\n        {\n            return (string)global::FOnline.Native.GetProperty(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetProperty(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("\"ManagedTitle\",") == string::npos);
    CHECK(server_entities.find("public static ManagedRoute ManagedRouteValue") != string::npos);
    CHECK(server_entities.find("public static ManagedRoute ManagedRouteValue\n    {\n        get\n        {\n            return (ManagedRoute)global::FOnline.Native.GetProperty(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("public static List<int> ManagedSteps") != string::npos);
    // An array of fixed-size values crosses as raw bytes into the list's own storage
    CHECK(server_entities.find("public static List<int> ManagedSteps\n    {\n        get\n        {\n            return global::FOnline.Native.GetPropertyList<int>(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetPropertyList<int>(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("public static ushort ManagedNarrowLimit") != string::npos);
    CHECK(server_entities.find("return global::FOnline.Native.GetPropertyValue<ushort>(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetPropertyValue<ushort>(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find("public short ManagedSkill") != string::npos);
    CHECK(server_entities.find("return (short)global::FOnline.Native.GetEntityValueAsInt(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetEntityValueAsInt(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetPropertyValue<bool>(_entityPtr, ") != string::npos);
    for (string_view scalar_type : {"sbyte", "byte", "short"}) {
        CHECK(server_entities.find(strex("return ({})global::FOnline.Native.GetEntityValueAsInt(_entityPtr, ", scalar_type).str()) != string::npos);
    }

    for (string_view scalar_type : {"int", "uint", "long", "ulong", "float", "double", "bool", "CritterCondition"}) {
        CHECK(server_entities.find(strex("global::FOnline.Native.GetPropertyValue<{}>(_entityPtr, ", scalar_type).str()) != string::npos);
    }

    CHECK(server_entities.find("bool HasManagedProbe\n    {\n        get\n        {\n            return global::FOnline.Native.GetPropertyValue<bool>(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetPropertyValue<CritterCondition>(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetPropertyValue<mpos>(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetPropertyValue<mpos>(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetPropertyValue<ucolor>(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("public Dictionary<hstring, List<hstring>> ManagedCheckpointEntries") != string::npos);
    CHECK(server_entities.find("public Dictionary<hstring, List<hstring>> ManagedCheckpointEntries\n    {\n        get\n        {\n            return (Dictionary<hstring, List<hstring>>)global::FOnline.Native.GetProperty(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.SetProperty(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("public Dictionary<int, List<string>> ManagedTextGroups") != string::npos);
    CHECK(server_entities.find("public Dictionary<int, List<string>> ManagedTextGroups\n    {\n        get\n        {\n            return (Dictionary<int, List<string>>)global::FOnline.Native.GetProperty(_entityPtr, ") != string::npos);
    // `any` is a type of its own on the script surface, not the string the engine stores it as
    CHECK(server_entities.find("public any ManagedAnyValue\n    {\n        get\n        {\n            return (any)global::FOnline.Native.GetProperty(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("public List<any> ManagedAnyList\n    {\n        get\n        {\n            return (List<any>)global::FOnline.Native.GetProperty(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("public any GetAsAny<TProp>(TProp prop) where TProp : unmanaged, System.Enum") != string::npos);
    CHECK(server_entities.find("public void SetAsAny<TProp>(TProp prop, any value) where TProp : unmanaged, System.Enum") != string::npos);
    CHECK(server_entities.find("public string ManagedAnyValue") == string::npos);
    CHECK(server_entities.find("public static List<mpos> TraceHexLine") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CallMethodBoxed(") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CallMethodIndexed(") != string::npos);
    CHECK(server_entities.find("Native.CallMethod(\n") == string::npos);
    CHECK(server_entities.find("\"TraceHexLine\"") == string::npos);
    CHECK(server_entities.find("return (List<mpos>)__result;") != string::npos);
    CHECK(server_entities.find("public static void DestroyEntities(List<Entity> entities)") != string::npos);
    CHECK(server_entities.find("public static void DestroyEntities(List<ident> ids)") == string::npos);
    CHECK(server_entities.find("\"DestroyEntities\"") == string::npos);
    CHECK(server_entities.find("Convert.ToInt32") == string::npos);
    CHECK(server_entities.find("global::FOnline.Native.EnumToInt32(prop)") != string::npos);
    CHECK(server_entities.find("internal static class PropertyCallbackAdapters") != string::npos);
    CHECK(server_entities.find("public static int DivRem(") != string::npos);
    CHECK(server_entities.find("ReadUnaligned<int>(") != string::npos);
    CHECK(server_entities.find("public static void Destroy<T>(T? entity) where T : Entity") != string::npos);
    CHECK(server_entities.find("public static void Destroy<T>(System.Collections.Generic.List<T>? entities) where T : Entity") != string::npos);
    CHECK(server_entities.find("DestroyEntity(entities[__i]);") != string::npos);
    CHECK(server_entities.find("public static ManagedGlobal AddManagedGlobal()") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CreateInnerEntity(IntPtr.Zero, ") != string::npos);
    CHECK(server_entities.find(", IntPtr.Zero);") != string::npos);
    CHECK(server_entities.find("CreateInnerEntity(IntPtr.Zero, \"ManagedGlobal\"") == string::npos);
    CHECK(server_entities.find("public static bool HasManagedGlobals()") != string::npos);
    CHECK(server_entities.find("public static System.Collections.Generic.List<ManagedGlobal> GetManagedGlobals()") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.FillInnerEntities(") != string::npos);
    CHECK(server_entities.find("GetInnerEntityAt") == string::npos);
    CHECK(server_entities.find("GetInnerEntityCount") == string::npos);
    CHECK(server_entities.find("public ManagedInner AddManagedEntry(hstring pid)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.CreateInnerEntity(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("CreateInnerEntity(_entityPtr, \"ManagedEntry\"") == string::npos);
    CHECK(server_entities.find("public bool HasManagedEntrys()") != string::npos);
    CHECK(server_entities.find("public System.Collections.Generic.List<ManagedInner> GetManagedEntrys()") != string::npos);
    CHECK(server_entities.find("public ManagedInner? GetManagedEntry(ident id)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.GetInnerEntity(_entityPtr, ") != string::npos);
    CHECK(server_entities.find("GetInnerEntity(_entityPtr, \"ManagedEntry\"") == string::npos);
    CHECK(server_entities.find("public static void AddPropertySetter(CritterProperty property, global::System.Func<Critter, global::System.Threading.Tasks.Task> setter)") != string::npos);
    CHECK(server_entities.find("public static void AddPropertyDeferredSetter(CritterProperty property, global::System.Func<Critter, global::System.Threading.Tasks.Task> setter)") != string::npos);
    CHECK(server_entities.find("public static void AddPropertySetter(CritterProperty property, global::FOnline.PropertySetter<Critter, short> setter)") != string::npos);
    CHECK(server_entities.find("public static void AddPropertySetter(CritterProperty property, global::FOnline.PropertySetterWithProperty<Critter, CritterProperty, short> setter)") != string::npos);
    CHECK(server_entities.find("global::FOnline.Native.AddPropertySetterWithProperty(\"Critter\", property.ToString(), setter);") != string::npos);
    CHECK(server_entities.find("public static Dictionary<string, string> ReadConfigSection(\n        string resourcePath,\n        string sectionName\n    )") != string::npos);
    CHECK(server_entities.find("object __result = global::FOnline.Native.CallMethodBoxed(") != string::npos);
    CHECK(server_entities.find("return (Dictionary<string, string>)__result;") != string::npos);
    CHECK(server_entities.find("public static Dictionary<string, string> DbGetRecord(hstring collectionName, string id)") != string::npos);
    CHECK(server_entities.find("\"ReadConfigSection\"") == string::npos);
    CHECK(server_entities.find("\"DbGetRecord\"") == string::npos);
    CHECK(server_entities.find("public static void DbInsertRecord(\n        hstring collectionName,\n        string id,\n        Dictionary<string, string> keyValues\n    )") != string::npos);
    CHECK(server_entities.find("\"DbInsertRecord\"") == string::npos);
    CHECK(server_entities.find("public static uint StartTimeEvent(timespan delay, Callback_void? func)") != string::npos);
    CHECK(server_entities.find("public static uint StartTimeEvent(timespan delay, Callback_voidAsync? func)") != string::npos);
    CHECK(server_entities.find("public static int CountTimeEvent(Callback_voidAsync? func)") != string::npos);
    CHECK(server_entities.find("public static void StopTimeEvent(Callback_voidAsync? func)") != string::npos);
    CHECK(server_entities.find("public static void RepeatTimeEvent(Callback_voidAsync? func, timespan repeat)") != string::npos);
    CHECK(server_entities.find("Callback_void_CritterAsync? func") != string::npos);

    CHECK(server_entities.find("return (uint)__result;") != string::npos);
    CHECK(server_entities.find("public static uint DecodeUtf8(string text, ref int length)") != string::npos);
    CHECK(server_entities.find("object?[] __args = new object?[]\n        {\n            text,\n            length,\n        };") != string::npos);
    CHECK(server_entities.find("object[] __result = (object[])global::FOnline.Native.CallMethodBoxed(") != string::npos);
    CHECK(server_entities.find("\"DecodeUtf8\"") == string::npos);
    CHECK(server_entities.find("length = (int)__result[1];") != string::npos);
    CHECK(server_entities.find("return (uint)__result[0];") != string::npos);
    CHECK(server_entities.find("public static void GetHexInterval(mpos fromHex, mpos toHex, ref ipos hexOffset)") != string::npos);
    CHECK(server_entities.find("Unsafe.WriteUnaligned(ref __frame[") != string::npos);
    CHECK(server_entities.find("object?[] __args = new object?[]\n        {\n            fromHex,\n            toHex,\n            hexOffset,\n        };") == string::npos);
    CHECK(server_entities.find("\"GetHexInterval\"") == string::npos);
    CHECK(server_entities.find("hexOffset = global::System.Runtime.CompilerServices.Unsafe.ReadUnaligned<ipos>(ref __frame[") != string::npos);
    CHECK(server_entities.find("public static GameOnManagedTestEvent OnManagedTest") != string::npos);
    CHECK(server_entities.find("__event_OnManagedTest") == string::npos);
    CHECK(server_entities.find("return new GameOnManagedTestEvent(IntPtr.Zero);") != string::npos);

    string server_abi = ReadTextFile(script_dir / "ServerAbi.gen.cs");
    CHECK(server_abi.find("global::FOnline.Native.RegisterWrapperFactory<Critter>(static nativePtr => new Critter(nativePtr));") != string::npos);
    CHECK(server_abi.find("global::FOnline.Native.RegisterWrapperFactory<Game>") == string::npos);
    CHECK(server_abi.find("static partial void BindGeneratedAbi()") != string::npos);
    CHECK(server_abi.find("global::FOnline.Native.BindAbi(") != string::npos);
    CHECK(server_abi.find("internal const int GeneratorIdentity = ") != string::npos);
    CHECK(server_abi.find("internal static class ManagedAbi") != string::npos);

    string server_events = ReadTextFile(script_dir / "ServerEvents.gen.cs");
    CHECK(server_events.find("NotImplementedException") == string::npos);
    CHECK(server_events.find("private readonly IntPtr _entityPtr;") != string::npos);
    CHECK(server_events.find("private GameOnManagedTestEventHandler? _handlers;") == string::npos);
    // Subscriptions live on the entity: the accessor is a stateless struct, so every wrapper reaches the same ones
    CHECK(server_events.find("public readonly struct GameOnManagedTestEvent\n") != string::npos);
    CHECK(server_events.find("_nativeSubscriptions") == string::npos);
    CHECK(server_events.find("public delegate global::System.Threading.Tasks.Task GameOnManagedTestEventHandlerAsync") != string::npos);
    CHECK(server_events.find("public delegate global::System.Threading.Tasks.Task<EventResult> GameOnManagedTestEventHandlerAsyncResult") != string::npos);
    CHECK(server_events.find("public void Subscribe(\n        GameOnManagedTestEventHandlerAsync handler") != string::npos);
    CHECK(server_events.find("public void Subscribe(\n        GameOnManagedTestEventHandlerAsyncResult handler") != string::npos);
    CHECK(server_events.find("public void Unsubscribe(GameOnManagedTestEventHandlerAsyncResult handler)") != string::npos);
    CHECK(server_events.find("{ if (handler == null)") == string::npos);
    CHECK(server_events.find("global::FOnline.Native.RequireEventAttribute(handler);\n        global::FOnline.Native.SubscribeEvent(\n") != string::npos);
    CHECK(server_events.find("\"OnManagedTest\"") == string::npos);
    CHECK(server_events.find("false,\n            (int)priority);") != string::npos);
    CHECK(server_events.find("global::FOnline.Native.UnsubscribeEvent(\n") != string::npos);
    CHECK(server_events.find("_entityPtr,\n            handler);") != string::npos);
    CHECK(server_events.find("global::FOnline.Native.UnsubscribeAllEvents(") != string::npos);
    CHECK(server_events.find("FireEventIndexed(") != string::npos);
    CHECK(server_events.find("FireEventBoxed(") != string::npos);
    CHECK(server_events.find("internal static void AdaptInvoke(global::System.Delegate handler, bool hasExplicitResult, global::System.IntPtr entityPtr, ref byte frame, int frameSize, ref int result)") != string::npos);
    CHECK(server_events.find("return (int)EventResult") == string::npos);

    // An entity argument rides the event frame as a pointer slot: the adapter wraps it, Fire writes the wrapper's pointer
    CHECK(server_events.find("Critter __a0 = global::FOnline.Native.WrapEntityNotNull<Critter>((global::System.IntPtr)global::System.Runtime.CompilerServices.Unsafe.ReadUnaligned<long>(ref global::System.Runtime.CompilerServices.Unsafe.Add(ref frame, 0)));") != string::npos);
    CHECK(server_events.find("int __a1 = global::System.Runtime.CompilerServices.Unsafe.ReadUnaligned<int>(ref global::System.Runtime.CompilerServices.Unsafe.Add(ref frame, 8));") != string::npos);
    CHECK(server_events.find("global::System.Runtime.CompilerServices.Unsafe.WriteUnaligned(ref __frame[0], (long)other.EntityPtr);") != string::npos);
    CHECK(server_events.find("return EventResult.StopChain;") != string::npos);
    CHECK(server_events.find("public delegate void GameOnManagedArrayEventHandler(List<int> values)") != string::npos);
    CHECK(server_events.find("object?[] __args = new object?[]\n            {\n                values,\n            };") != string::npos);
    CHECK(server_events.find("public delegate void GameOnManagedDictEventHandler(Dictionary<string, string> values)") != string::npos);
    CHECK(server_events.find("public delegate void GameOnManagedMutablePositionEventHandler(") != string::npos);
    CHECK(server_events.find("ref int third") != string::npos);
    CHECK(server_events.find("ReadUnaligned<int>(ref __frame[") != string::npos);
    CHECK(server_events.find("third = global::FOnline.Native.UnboxArg<int>(__args[2]);") == string::npos);
    CHECK(server_events.find("third = (int)__args[0];") == string::npos);

    string client_settings = ReadTextFile(script_dir / "ClientSettings.gen.cs");
    CHECK(client_settings.find("    public static class View\n    {\n") != string::npos);
    CHECK(client_settings.find("public static List<int> GlobalDayColorTime") != string::npos);
    CHECK(client_settings.find("View_GlobalDayColor") == string::npos);
    CHECK(client_settings.find("global::FOnline.Native.GetSettingIntList(") != string::npos);
    CHECK(client_settings.find("\"View.GlobalDayColorTime\"") != string::npos);
    CHECK(client_settings.find("public static List<byte> GlobalDayColor\n") != string::npos);
    CHECK(client_settings.find("global::FOnline.Native.GetSettingByteList(") != string::npos);
    CHECK(client_settings.find("\"View.GlobalDayColor\"") != string::npos);
    CHECK(client_settings.find("global::FOnline.Native.GetSettingValue<") != string::npos);
    CHECK(client_settings.find("global::FOnline.Native.SetSettingValue<") == string::npos);

    string client_types = ReadTextFile(script_dir / "ClientTypes.gen.cs");
    CHECK(client_types.find("public static MapSpriteHolder __Factory()") != string::npos);
    CHECK(client_types.find("global::FOnline.Native.CallMethodBoxed(") != string::npos);
    CHECK(client_types.find("\"__Factory\"") == string::npos);

    string server_types = ReadTextFile(script_dir / "ServerTypes.gen.cs");
    CHECK(server_types.find("public delegate global::System.Threading.Tasks.Task Callback_voidAsync();") != string::npos);
    CHECK(server_types.find("internal static class CallbackAdapters") != string::npos);
    CHECK(server_types.find("internal static void Adapt_Callback_void(global::System.Delegate handler, ref byte frame, int frameSize)") != string::npos);
    CHECK(server_types.find("internal static void Adapt_Callback_void_Critter(global::System.Delegate handler, ref byte frame, int frameSize)") != string::npos);
    CHECK(server_types.find("if (handler is Callback_void_Critter typed) {") != string::npos);
    CHECK(server_types.find("if (handler is Callback_void_CritterAsync typedAsync) {") != string::npos);
    CHECK(server_types.find("if (handler is global::System.Action<Critter> action) {") != string::npos);
    CHECK(server_types.find("if (handler is global::System.Func<Critter, global::System.Threading.Tasks.Task> asyncFunc) {") != string::npos);
    CHECK(server_types.find("global::FOnline.Native.WrapEntityNotNull<Critter>((global::System.IntPtr)global::System.Runtime.CompilerServices.Unsafe.ReadUnaligned<long>(ref global::System.Runtime.CompilerServices.Unsafe.Add(ref frame, 0)))") != string::npos);
    CHECK(server_types.find("global::FOnline.Native.InvokeCallback(handler, __args);") != string::npos);

    // An inbound remote call gets an adapter over the calling Player plus its wire arguments, once per signature and
    // without the delegate-type branches it has no type for; a string argument keeps the call on the boxed path
    string_view rpc_adapter = "internal static void Adapt_Callback_void_Player_int32_mpos(global::System.Delegate handler, ref byte frame, int frameSize)";
    size_t rpc_adapter_pos = server_types.find(rpc_adapter);
    CHECK(rpc_adapter_pos != string::npos);
    CHECK(server_types.find(rpc_adapter, rpc_adapter_pos + 1) == string::npos);
    CHECK(server_types.find("if (handler is global::System.Action<Player, int, mpos> action) {") != string::npos);
    CHECK(server_types.find("if (handler is Callback_void_Player_int32_mpos typed) {") == string::npos);
    CHECK(server_types.find("Adapt_Callback_void_Player_string") == string::npos);
    CHECK(server_types.find("public delegate global::System.Threading.Tasks.Task Callback_bool") == string::npos);
    CHECK(server_types.find("public partial struct hstring") != string::npos);
    CHECK(server_types.find("public System.IntPtr Value;") != string::npos);
    CHECK(server_types.find("LayoutKind.Sequential, Size = 8") != string::npos);
    CHECK(server_types.find("public static hstring FromString(string value)") != string::npos);
    CHECK(server_types.find("public ulong Value;") == string::npos);
    CHECK(server_types.find("[global::System.Runtime.InteropServices.StructLayout(global::System.Runtime.InteropServices.LayoutKind.Sequential)]\npublic partial struct mpos") != string::npos);
    CHECK(server_types.find("[global::System.Runtime.InteropServices.StructLayout(global::System.Runtime.InteropServices.LayoutKind.Sequential)]\npublic partial struct ucolor") != string::npos);
    CHECK(server_types.find("[global::System.Runtime.InteropServices.StructLayout(global::System.Runtime.InteropServices.LayoutKind.Sequential)]\npublic partial struct ipos") != string::npos);

    // A value type converts to and from `any` field by field, nested value types flattened, as GenericType_AnyConv does
    CHECK(server_types.find("public static implicit operator global::FOnline.any(mpos value)\n    {\n        return global::FOnline.any.FromFields(new string[]\n        {\n            global::FOnline.any.FieldText(value.x),\n            global::FOnline.any.FieldText(value.y),\n        });\n    }") != string::npos);
    CHECK(server_types.find("public static explicit operator mpos(global::FOnline.any value)\n    {\n        string[] fields = value.SplitFields(2, \"mpos\");\n        mpos result = default;\n        result.x = global::FOnline.any.FieldInt16(fields[0]);\n        result.y = global::FOnline.any.FieldInt16(fields[1]);\n        return result;\n    }") != string::npos);
    CHECK(server_types.find("global::FOnline.any.FieldText(value.Collection.Name),") != string::npos);
    CHECK(server_types.find("result.Collection.Name = global::FOnline.any.FieldHash(fields[0]);") != string::npos);

    for (string_view target : {"Server", "Client", "Mapper"}) {
        string types = ReadTextFile(script_dir / fs::make_path(strex("{}Types.gen.cs", target).str()));
        CHECK(types.find("public partial struct hdir") != string::npos);
        CHECK(types.find("public sbyte value;") != string::npos);
        CHECK(types.find("public hdir(") == string::npos);
        CHECK(types.find("public partial struct mdir") != string::npos);
        CHECK(types.find("public short angle;") != string::npos);
        CHECK(types.find("public mdir(") == string::npos);
    }

    CHECK(server_types.find("public partial class ManagedRoute") != string::npos);
    CHECK(server_types.find("public ManagedRoute()\n    {\n    }") != string::npos);
    CHECK(server_types.find("public int Step") != string::npos);
    CHECK(server_types.find("set;") != string::npos);
    CHECK(server_types.find("public List<int> Values") != string::npos);
    CHECK(server_types.find("public bool Checkpoint") != string::npos);
    CHECK(server_types.find("public int CheckpointIndex") != string::npos);
    CHECK(server_types.find("public string CheckpointLabel") != string::npos);
    CHECK(server_types.find("private IntPtr _refPtr;") != string::npos);
    CHECK(server_types.find("public ushort GetSpeed()") != string::npos);
    CHECK(server_types.find("global::FOnline.Native.CallMethodIndexed(") != string::npos);
    CHECK(server_types.find("ReadUnaligned<ushort>(") != string::npos);
    CHECK(server_types.find("\"GetSpeed\"") == string::npos);
    CHECK(server_types.find("Native.CallMethod(\n") == string::npos);
    CHECK(server_entities.find("public partial class Entity : System.IEquatable<Entity>") != string::npos);
    CHECK(server_entities.find("private readonly bool[]? _backendAlive;") == string::npos);
    CHECK(server_entities.find("private readonly IntPtr _backend;") == string::npos);
    CHECK(server_entities.find("Entity wrapper belongs to a different managed backend") == string::npos);
    CHECK(server_entities.find("        _trackerId = global::FOnline.EntityWrapperTracker.Register(this, entityPtr);\n") != string::npos);
    CHECK(server_entities.find("            if (global::FOnline.Native.IsBackendAlive) {\n                global::FOnline.Native.ReleaseEntity(_entityPtrValue);\n            }\n") != string::npos);
    CHECK(server_entities.find("        global::FOnline.EntityWrapperTracker.Unregister(_trackerId);\n") != string::npos);
    CHECK(server_entities.find("return !object.ReferenceEquals(other, null) && _entityPtrValue == other._entityPtrValue;") != string::npos);
    CHECK(server_entities.find("public static bool operator ==(Entity? left, Entity? right)") != string::npos);
    CHECK(server_entities.find("return _entityPtrValue.GetHashCode();") != string::npos);
    CHECK(server_entities.find("public ident Id\n    {\n        get\n        {\n            return new ident(global::FOnline.Native.GetEntityId(_entityPtr));") != string::npos);

    std::error_code ec;
    (void)std::filesystem::remove(script_dir / "UnitProject.gen.csproj", ec);
    ec.clear();
    (void)std::filesystem::remove(script_dir / "ServerEnums.gen.cs", ec);
    ec.clear();
    (void)std::filesystem::remove(script_dir / "UnitProject.gen.sln", ec);
    WriteTextFile(script_dir / "NoWorkObsolete.gen.cs", MakeManagedGeneratedCs("namespace Demo { public static class NoWorkObsoleteGeneratedCode {} }\n"));

    unordered_set<string> checked_output_paths;
    ManagedScriptBaker no_work_baker(rig.MakeContext("TestPack", [&](string_view path, uint64_t) {
        checked_output_paths.emplace(path);
        return false;
    }));
    REQUIRE_NOTHROW(no_work_baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    CHECK(rig.Outputs.empty());
    CHECK(checked_output_paths.contains("Assemblies/Assemblies-server/FOnline.ManagedHost.dll"));
    CHECK(checked_output_paths.contains("Assemblies/Assemblies-client/FOnline.ManagedHost.dll"));
    CHECK(checked_output_paths.contains("Assemblies/Assemblies-mapper/FOnline.ManagedHost.dll"));
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.csproj"));
    CHECK(std::filesystem::exists(script_dir / "ServerEnums.gen.cs"));
    CHECK(std::filesystem::exists(script_dir / "UnitProject.gen.sln"));
    CHECK_FALSE(std::filesystem::exists(script_dir / "NoWorkObsolete.gen.cs"));
#endif
}

TEST_CASE("ManagedScriptBaker project output preserves absolute and relocatable bake roots")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    bool absolute_output = GENERATE(false, true);
    CAPTURE(absolute_output);

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts";
    std::filesystem::path bake_output = absolute_output ? temp_dir.Path() / fs::make_path("External Bake & Данные") : std::filesystem::path {fs::make_path("Relocated Bake & Данные")};

    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(script_dir / "Shared.cs", "namespace Demo { public static class Shared {} }\n");

    ScopedCurrentPath current_path(temp_dir.Path());

    TestRig rig;
    OverrideSetting(rig.Settings.ManagedScript.BakerDryRun, true);
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {fs::path_to_string(core_scripts_dir), fs::path_to_string(script_dir)});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, fs::path_to_string(script_dir));
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitProject");
    OverrideSetting(rig.Settings.Baking.BakeOutput, fs::path_to_string(bake_output));
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    ManagedScriptBaker baker(rig.MakeContext());
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    string project = ReadTextFile(script_dir / "UnitProject.gen.csproj");
    string expected_root = absolute_output ? strex("{}/External Bake &amp; Данные", fs::path_to_string(temp_dir.Path())).str() : "$(FOnlineBakeRoot)/Relocated Bake &amp; Данные";

    for (string_view target : array<string_view, 3> {"Server", "Client", "Mapper"}) {
        string expected_output = strex("<OutputPath>{}/TestPack/Assemblies/{}Assemblies/</OutputPath>", expected_root, target).str();
        CHECK(project.find(expected_output) != string::npos);
    }

    CHECK(project.find("<FOnlineBakeRoot Condition=\" '$(FOnlineBakeRoot)' == '' \">$(MSBuildThisFileDirectory)..</FOnlineBakeRoot>") != string::npos);
    CHECK((project.find("<OutputPath>$(FOnlineBakeRoot)/") == string::npos) == absolute_output);
#endif
}

TEST_CASE("ManagedScriptBaker rejects flattened dynamic RefType property collisions")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";

    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(script_dir / "Shared.cs", "namespace Demo { public static class Shared {} }\n");

    ScopedCurrentPath current_path(temp_dir.Path());

    TestRig rig;
    OverrideSetting(rig.Settings.Baking.BakeOutput, string {"Baking"});
    OverrideSetting(rig.Settings.ManagedScript.BakerDryRun, true);
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {string(core_scripts_dir.string()), string(script_dir.string())});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, script_dir.string());
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitCollision");
    rig.AddBakedFile("Metadata.fometa-server",
        MakeMetadataBlob({
            {"RefType", {{"CollisionRoute", "Checkpoint", "bool", "1", "Component", "Checkpoint.Index", "int32", "0", "CheckpointIndex", "string", "0"}}},
        }));
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    ManagedScriptBaker baker(rig.MakeContext());
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Metadata.fometa-server"));
    REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Managed dynamic RefType property name collision"));
#endif
}

TEST_CASE("ManagedScriptBaker rebakes when an editorconfig above the sources changes")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";
    std::filesystem::path editor_config = temp_dir.Path() / ".editorconfig";

    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(script_dir / "Shared.cs", "namespace Demo { public static class Shared {} }\n");
    WriteTextFile(editor_config, "root = true\n");

    // An .editorconfig above the sources must move the bake stamp. Its write time is set rather than
    // rewritten, because two writes can land inside one filesystem timestamp tick
    auto future_time = std::filesystem::last_write_time(editor_config) + std::chrono::hours(24);
    std::filesystem::last_write_time(editor_config, future_time);

    ScopedCurrentPath current_path(temp_dir.Path());

    TestRig rig;
    OverrideSetting(rig.Settings.Baking.BakeOutput, string {"Baking"});
    OverrideSetting(rig.Settings.ManagedScript.BakerDryRun, true);
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {string(core_scripts_dir.string()), string(script_dir.string())});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, script_dir.string());
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitEditorConfig");
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    vector<uint64_t> stamps;
    ManagedScriptBaker baker(rig.MakeContext("TestPack", [&stamps](string_view path, uint64_t write_time) {
        ignore_unused(path);
        stamps.emplace_back(write_time);
        return false;
    }));
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    REQUIRE(!stamps.empty());
    CHECK(std::ranges::max(stamps) == fs::last_write_time(strex("{}", editor_config.string()).str()));
#endif
}

TEST_CASE("ManagedScriptBaker stamp includes generated API files so a generator-only change rebakes")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";

    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static partial class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(script_dir / "Shared.cs", "namespace Demo { public static class Shared {} }\n");

    ScopedCurrentPath current_path(temp_dir.Path());

    TestRig rig;
    OverrideSetting(rig.Settings.Baking.BakeOutput, string {"Baking"});
    OverrideSetting(rig.Settings.ManagedScript.BakerDryRun, true);
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {string(core_scripts_dir.string()), string(script_dir.string())});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, script_dir.string());
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitAbiStamp");
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    vector<pair<string, uint64_t>> first_checks;
    ManagedScriptBaker baker(rig.MakeContext("TestPack", [&first_checks](string_view path, uint64_t write_time) {
        first_checks.emplace_back(string {path}, write_time);
        return false;
    }));
    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    auto abi_path = script_dir / "ServerAbi.gen.cs";
    REQUIRE(std::filesystem::exists(abi_path));
    REQUIRE(!first_checks.empty());

    uint64_t first_stamp = 0;

    for (const auto& [path, write_time] : first_checks) {
        first_stamp = std::max(first_stamp, write_time);
    }

    CHECK(first_stamp >= fs::last_write_time(strex("{}", abi_path.string()).str()));

    auto future_time = std::filesystem::last_write_time(abi_path) + std::chrono::hours(24);
    std::filesystem::last_write_time(abi_path, future_time);
    uint64_t generated_stamp = fs::last_write_time(strex("{}", abi_path.string()).str());

    vector<pair<string, uint64_t>> second_checks;
    ManagedScriptBaker rebaker(rig.MakeContext("TestPack", [&second_checks](string_view path, uint64_t write_time) {
        second_checks.emplace_back(string {path}, write_time);
        return false;
    }));
    REQUIRE_NOTHROW(rebaker.BakeFiles(rig.GetAllSourceFiles(), ""));
    REQUIRE(!second_checks.empty());

    uint64_t second_stamp = 0;
    bool saw_entry_assembly = false;

    for (const auto& [path, write_time] : second_checks) {
        second_stamp = std::max(second_stamp, write_time);

        if (path.find("TestPack.Server.dll") != string::npos) {
            saw_entry_assembly = true;
            CHECK(write_time >= generated_stamp);
        }
    }

    CHECK(saw_entry_assembly);
    CHECK(second_stamp >= generated_stamp);
    CHECK(second_stamp > first_stamp);
#endif
}

TEST_CASE("ManagedScriptBaker rejects an analyzer package without an exact version")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";

    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(script_dir / "Shared.cs", "namespace Demo { public static class Shared {} }\n");

    ScopedCurrentPath current_path(temp_dir.Path());

    TestRig rig;
    OverrideSetting(rig.Settings.Baking.BakeOutput, string {"Baking"});
    OverrideSetting(rig.Settings.ManagedScript.BakerDryRun, true);
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {string(core_scripts_dir.string()), string(script_dir.string())});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, script_dir.string());
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitPackages");
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    // A wildcard version, a range and a missing one each make the reported rule set depend on the day the
    // build ran, so the baker refuses them instead of emitting a project whose analysis silently drifts
    string_view rejected_entry = GENERATE("Unit.Analyzer,1.2.*", "Unit.Analyzer,[1.2.3,2.0.0)", "Unit.Analyzer,", "Unit.Analyzer", ",1.2.3");
    OverrideSetting(rig.Settings.ManagedScript.AnalyzerPackages, vector<string> {string(rejected_entry)});

    ManagedScriptBaker baker(rig.MakeContext());
    REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Analyzer package"));
#endif
}

TEST_CASE("ManagedScriptBaker packs helper assemblies")
{
#if FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    ScopedTempDirectory temp_dir;
    std::filesystem::path managed_source_dir = temp_dir.Path() / "ManagedSupport";
    std::filesystem::path core_scripts_dir = managed_source_dir / "CoreScripts";
    std::filesystem::path managed_host_source = managed_source_dir / "ManagedHost" / "ManagedLoadContextHost.cs";
    std::filesystem::path script_dir = temp_dir.Path() / "Scripts" / "Managed";
    std::filesystem::path shared_source = script_dir / "Shared.cs";
    std::filesystem::path fake_msbuild_root = temp_dir.Path() / "Baking" / "TestPack" / "Assemblies";
    std::filesystem::path fake_msbuild = WriteFakeManagedMsBuildScript(temp_dir.Path());
    std::filesystem::path managed_runtime_dir = temp_dir.Path() / "ManagedRuntime";

    WriteTextFile(core_scripts_dir / "Attributes.cs", "namespace FOnline { public sealed class ModuleInitAttribute : System.Attribute { public ModuleInitAttribute(int priority = 0) {} } }\n");
    WriteTextFile(core_scripts_dir / "Initializator.cs", "namespace FOnline { public static class Initializator { static void Initialize() {} } }\n");
    WriteTextFile(core_scripts_dir / "Native.cs", "namespace FOnline { internal static class Native {} }\n");
    WriteTextFile(managed_host_source, "namespace FOnline.ManagedHost { public static class ManagedLoadContextHost {} }\n");
    WriteTextFile(shared_source, "namespace Demo { public static class Shared {} }\n");

    // Each target reaches System.Runtime, only the client reaches System.Linq, and nothing reaches System.Xml
    std::filesystem::path fixtures_dir = temp_dir.Path() / "MsBuildFixtures";
    map<string, vector<uint8_t>> fixture_images {
        {"ServerAssemblies/TestPack.Server.dll", MakeManagedAssemblyImage("TestPack.Server", {"System.Runtime", "SharedDependency"})},
        {"ClientAssemblies/TestPack.Client.dll", MakeManagedAssemblyImage("TestPack.Client", {"System.Runtime", "System.Linq", "SharedDependency"})},
        {"MapperAssemblies/TestPack.Mapper.dll", MakeManagedAssemblyImage("TestPack.Mapper", {"System.Runtime"})},
    };

    for (string_view target : {"Server", "Client", "Mapper"}) {
        fixture_images.emplace(strex("{}Assemblies/SharedDependency.dll", target).str(), MakeManagedAssemblyImage("SharedDependency", {"System.Private.CoreLib"}));
        fixture_images.emplace(strex("{}Assemblies/FOnline.ManagedHost.dll", target).str(), MakeManagedAssemblyImage("FOnline.ManagedHost", {"System.Runtime"}));
    }

    for (const auto& [fixture_path, fixture_image] : fixture_images) {
        WriteBinaryFile(fixtures_dir / fs::make_path(fixture_path), fixture_image);
    }

    std::filesystem::path class_library_dir = managed_runtime_dir / "lib" / "netcoreapp";
    map<string, string> manifest_lines;

    for (string_view assembly_name : {"System.Linq", "System.Private.CoreLib", "System.Runtime", "System.Xml"}) {
        vector<string> references = assembly_name == "System.Private.CoreLib" ? vector<string> {} : vector<string> {"System.Private.CoreLib"};

        if (assembly_name == "System.Linq") {
            references.emplace_back("System.Runtime");
        }

        WriteBinaryFile(class_library_dir / fs::make_path(strex("{}.dll", assembly_name)), MakeManagedAssemblyImage(assembly_name, references));
        manifest_lines.emplace(assembly_name, strex("{}  lib/netcoreapp/{}.dll\n", string(64, numeric_cast<char>('a' + manifest_lines.size())), assembly_name).str());
    }

    string runtime_manifest;

    for (const string& manifest_line : manifest_lines | std::views::values) {
        runtime_manifest += manifest_line;
    }

    WriteTextFile(class_library_dir / "coreclr.dll", "native-coreclr\n");
    WriteTextFile(managed_runtime_dir / "runtime.manifest", runtime_manifest);

    ScopedCurrentPath current_path(temp_dir.Path());
    // The fake msbuild helper is a spawned child process; env is the legitimate channel to parameterize it
    ScopedEnvVar msbuild_root {"FO_FAKE_MSBUILD_ROOT", fake_msbuild_root.string()};
    ScopedEnvVar msbuild_fixtures {"FO_FAKE_MSBUILD_FIXTURES", fixtures_dir.string()};
    ScopedEnvVar managed_runtime {"FO_MANAGED_RUNTIME", managed_runtime_dir.string()};

    TestRig rig;
    OverrideSetting(rig.Settings.Baking.BakeOutput, string {"Baking"});
    OverrideSetting(rig.Settings.ManagedScript.Dirs, vector<string> {string(core_scripts_dir.string()), string(script_dir.string())});
    OverrideSetting(rig.Settings.ManagedScript.GeneratedDir, script_dir.string());
    OverrideSetting(rig.Settings.ManagedScript.Assemblies, vector<string> {"UnitManaged"});
    OverrideSetting(rig.Settings.ManagedScript.ExtraSources, vector<string> {strex("UnitManaged,All,{}", shared_source.string()).str()});
    OverrideSetting(rig.Settings.ManagedScript.ProjectName, "UnitProject");
    OverrideSetting(rig.Settings.ManagedScript.MsBuild, fake_msbuild.string());
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-client", MakeEmptyMetadataBlob());
    rig.AddBakedFile("Metadata.fometa-mapper", MakeEmptyMetadataBlob());

    vector<string> log_messages;
    logging::set_callback("managed-script-baker-compiler-output-test", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { log_messages.emplace_back(message); });
    auto remove_log_callback = scope_exit([]() noexcept { logging::set_callback("managed-script-baker-compiler-output-test", {}); });

    auto logged = [&log_messages](string_view text) { return std::ranges::any_of(log_messages, [text](const string& message) { return message.find(text) != string::npos; }); };

    // The compiler runs without a console of its own, so its output must reach the log or a failed bake explains nothing
    SECTION("CompilerFailureReachesTheLog")
    {
        ScopedEnvVar fail_compile {"FO_FAKE_MSBUILD_FAIL", "1"};
        ManagedScriptBaker failing_baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(failing_baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("compilation failed"));
        CHECK(logged("error CS0000: fake compile failure"));
    }

    SECTION("AssemblyDirectoryIsOneSpelling")
    {
        // One spelling for both readers: a bake-output fallback that matches nothing leaves every annotated
        // script function unverified whenever an incremental bake skips the up-to-date script pack
        for (string_view target : {"Server", "Client", "Mapper"}) {
            INFO(target);
            CHECK(MakeManagedAssemblyResourceDir(target) == strex("Assemblies/Assemblies-{}", strex(target).lower()).str());
        }

        ManagedScriptBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        for (string_view target : {"Server", "Client", "Mapper"}) {
            INFO(target);
            CHECK(rig.Outputs.contains(strex("{}/{}.dll", MakeManagedAssemblyResourceDir(target), strex("TestPack.{}", target)).str()));
        }
    }

    SECTION("CompiledAssembliesArePacked")
    {
        ManagedScriptBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(logged("fake-msbuild-output"));

        for (string_view target : {"Server", "Client", "Mapper"}) {
            string target_lower = strex(target).lower().str();

            for (string_view assembly_name : {strex("TestPack.{}", target).str(), string {"SharedDependency"}, string {"FOnline.ManagedHost"}}) {
                INFO(target << " " << assembly_name);
                CHECK(rig.Outputs.at(strex("Assemblies/Assemblies-{}/{}.dll", target_lower, assembly_name).str()) == fixture_images.at(strex("{}Assemblies/{}.dll", target, assembly_name).str()));
            }

            CHECK_FALSE(rig.Outputs.contains(strex("Assemblies/Assemblies-{}/TestPack.{}.pdb", target_lower, target).str()));
            CHECK_FALSE(rig.Outputs.contains(strex("Assemblies/Assemblies-{}/TestPack.{}.deps.json", target_lower, target).str()));
        }

        // The payload is the union of every target's closure, and the manifest lists exactly what shipped
        for (string_view assembly_name : {"System.Linq", "System.Private.CoreLib", "System.Runtime"}) {
            CHECK(rig.GetOutputText(strex("ManagedRuntime/lib/netcoreapp/{}.dll", assembly_name).str()) == ReadTextFile(class_library_dir / fs::make_path(strex("{}.dll", assembly_name).str())));
        }

        CHECK_FALSE(rig.Outputs.contains("ManagedRuntime/lib/netcoreapp/System.Xml.dll"));
        CHECK_FALSE(rig.Outputs.contains("ManagedRuntime/lib/netcoreapp/coreclr.dll"));
        CHECK(rig.GetOutputText("ManagedRuntime/runtime.manifest") == manifest_lines.at("System.Linq") + manifest_lines.at("System.Private.CoreLib") + manifest_lines.at("System.Runtime"));
    }

    SECTION("UpToDateTargetShipsItsBakedAssemblies")
    {
        // The mapper is not rebuilt, so its selection has to come from the assembly its earlier bake left in the pack
        WriteBinaryFile(fake_msbuild_root / "Assemblies-mapper" / "TestPack.Mapper.dll", MakeManagedAssemblyImage("TestPack.Mapper", {"System.Xml"}));
        ManagedScriptBaker baker(rig.MakeContext("TestPack", [](string_view path, uint64_t) { return !path.starts_with("Assemblies/Assemblies-mapper/"); }));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        CHECK_FALSE(rig.Outputs.contains("Assemblies/Assemblies-mapper/TestPack.Mapper.dll"));
        CHECK(rig.Outputs.contains("ManagedRuntime/lib/netcoreapp/System.Xml.dll"));
        CHECK(rig.Outputs.contains("ManagedRuntime/lib/netcoreapp/System.Linq.dll"));
        CHECK(rig.GetOutputText("ManagedRuntime/runtime.manifest").find("System.Xml.dll") != string::npos);
    }

    // The bake deletes every output no baker claimed, so an assembly the build copies beside the entry assembly (a
    // package or project reference) must be claimed as well, both when it is built and when its target is up to date
    SECTION("EveryPackedAssemblyIsClaimed")
    {
        set<string> claimed_paths;
        ManagedScriptBaker baker(rig.MakeContext("TestPack", [&claimed_paths](string_view path, uint64_t) {
            claimed_paths.emplace(path);
            return true;
        }));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.contains("Assemblies/Assemblies-server/SharedDependency.dll"));

        for (const string& output_path : rig.Outputs | std::views::keys) {
            INFO(output_path);
            CHECK(claimed_paths.contains(output_path));
        }
    }

    SECTION("UpToDateTargetClaimsItsBakedAssemblies")
    {
        WriteBinaryFile(fake_msbuild_root / "Assemblies-mapper" / "TestPack.Mapper.dll", MakeManagedAssemblyImage("TestPack.Mapper", {"System.Runtime", "SharedDependency"}));
        WriteBinaryFile(fake_msbuild_root / "Assemblies-mapper" / "SharedDependency.dll", MakeManagedAssemblyImage("SharedDependency", {"System.Private.CoreLib"}));

        set<string> claimed_paths;
        ManagedScriptBaker baker(rig.MakeContext("TestPack", [&claimed_paths](string_view path, uint64_t) {
            claimed_paths.emplace(path);
            return !path.starts_with("Assemblies/Assemblies-mapper/");
        }));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        CHECK_FALSE(rig.Outputs.contains("Assemblies/Assemblies-mapper/SharedDependency.dll"));
        CHECK(claimed_paths.contains("Assemblies/Assemblies-mapper/SharedDependency.dll"));
    }

    SECTION("DiscoveryDeclaresOnlyTheSelectedPayload")
    {
        // The on-demand data source learns the pack's files from this pass, so it must name what a bake would ship
        for (const auto& [fixture_path, fixture_image] : fixture_images) {
            constexpr string_view build_dir_suffix = "Assemblies/";
            size_t separator = fixture_path.find(build_dir_suffix);
            string baked_dir = strex("Assemblies-{}", strex(fixture_path.substr(0, separator)).lower()).str();
            WriteBinaryFile(fake_msbuild_root / fs::make_path(baked_dir) / fs::make_path(fixture_path.substr(separator + build_dir_suffix.size())), fixture_image);
        }

        set<string> declared_paths;
        shared_ptr<BakingContext> context = rig.MakeContext("TestPack", [&declared_paths](string_view path, uint64_t) {
            declared_paths.emplace(path);
            return false;
        });
        context->OutputDiscovery = true;
        ManagedScriptBaker baker(std::move(context));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        CHECK_FALSE(logged("fake-msbuild-output"));
        CHECK(rig.Outputs.empty());
        CHECK(declared_paths.contains("ManagedRuntime/runtime.manifest"));
        CHECK(declared_paths.contains("ManagedRuntime/lib/netcoreapp/System.Linq.dll"));
        CHECK(declared_paths.contains("ManagedRuntime/lib/netcoreapp/System.Runtime.dll"));
        CHECK_FALSE(declared_paths.contains("ManagedRuntime/lib/netcoreapp/System.Xml.dll"));
    }

    SECTION("UnresolvedPackReferenceFailsTheBake")
    {
        WriteBinaryFile(fixtures_dir / "ServerAssemblies" / "TestPack.Server.dll", MakeManagedAssemblyImage("TestPack.Server", {"System.Runtime", "Missing.Library"}));
        ManagedScriptBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Missing.Library") && Catch::Matchers::ContainsSubstring("TestPack.Server"));
    }

    SECTION("AssemblyFileNameMustMatchItsIdentity")
    {
        WriteBinaryFile(fixtures_dir / "ServerAssemblies" / "Alias.dll", MakeManagedAssemblyImage("Real.Name", {}));
        ManagedScriptBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Alias.dll") && Catch::Matchers::ContainsSubstring("Real.Name"));
    }

    SECTION("DuplicateRuntimeManifestAssemblyFailsTheBake")
    {
        WriteTextFile(managed_runtime_dir / "runtime.manifest", runtime_manifest + manifest_lines.at("System.Runtime"));
        ManagedScriptBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Duplicate Managed runtime payload assembly") && Catch::Matchers::ContainsSubstring("System.Runtime"));
    }

    SECTION("InvalidRuntimeManifestDigestFailsTheBake")
    {
        string invalid_manifest = runtime_manifest;
        invalid_manifest.front() = 'g';
        WriteTextFile(managed_runtime_dir / "runtime.manifest", invalid_manifest);
        ManagedScriptBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Invalid Managed runtime payload digest"));
    }
#endif
}

FO_END_NAMESPACE
