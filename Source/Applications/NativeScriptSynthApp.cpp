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

// NativeScriptSynthApp.cpp — standalone build-time tool
// (`LF_NativeScriptSynth`) that is the SOLE generator of every native
// scripting source file (codegen.py emits none). It produces:
//   - `NativeApi_ContextRpcMethods.h` and
//     `NativeApi.{Common,Server,Client,Mapper,Baker}.cppm` from
//     `EngineMetadata` populated by `Register{Server,Client,Mapper}StubMetadata`.
//   - `NativeBindings-{Common,Server,Client,Mapper,Baker}.cpp` — the
//     per-role dispatchers, built by scanning the user `.cppm` tree
//     (argv[2]) for `export module NativeScripts.User.<Role>.<Name>;`
//     plus each module's `void <Init>(const ModuleInitContext&)` entry.
//
// The stub registrations are compiled with `STUB_MODE=1` (set by
// codegen.py in `MetadataRegistration-<Role>Stub.cpp`), which avoids
// any `Server.h`/`Client.h`/`Mapper.h` includes — letting this tool
// link against `CommonLib` + `NativeScriptSynth` only, keeping source
// generation independent from the downstream NativeScripting libraries.
//
// Invocation: `LF_NativeScriptSynth <output_dir> [<native_scripts_dir>]`.
// The CMake `NativeApiGeneration` custom command (Codegen.cmake)
// drives it after `CodeGeneration`.
//
// Cross-side settings: each per-target bin starts with only its own
// side's settings; we merge the others in (`MergeSettings`) so every
// target's `.cppm` re-exports the full `NativeScripts::Settings::*`
// surface. Per-target wrapper bodies pick their own role bin (via
// the synth's `target_meta_lookup` callback) so server-only types
// don't leak into Client/Mapper wrappers; the Common target gates
// members through the Server∩Client intersection allowlist.

#include "Common.h"

#if FO_NATIVE_SCRIPTING

#include "Application.h"
#include "EngineBase.h"
#include "MetadataRegistration.h"
#include "NativeScriptSynth.h"

#include <filesystem>

FO_USING_NAMESPACE();

// Writes `body` to `<output_dir>/<file_name>` in binary mode (so line
// endings match codegen.py's output regardless of host platform).
// Mirrors the `write_file` helper used by `ASCompilerApp` /
// `BakerApp`.
static void WriteOutput(string_view output_dir, string_view file_name, string_view body)
{
    FO_STACK_TRACE_ENTRY();

    const string path = strex(output_dir).combine_path(file_name).str();
    const string dir = strex(path).extract_dir().str();

    if (!dir.empty()) {
        const bool dir_ok = fs::create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Failed to create the native script synthesis output directory", dir);
    }

    std::ofstream file {std::filesystem::path {fs::make_path(path)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(file, "Failed to open the native script synthesis output file", path);
    file.write(body.data(), static_cast<std::streamsize>(body.size()));
    FO_VERIFY_AND_THROW(file, "Failed to write the native script synthesis output file", path, body.size());

    logging::write("NativeScriptSynth: emitted {} ({} bytes)", file_name, body.size());
}

// Per-target `EngineMetadata` populated by the matching stub
// registration. The `EngineMetadata` constructor takes a
// `function<void()>` registrator that runs during construction — we
// need a pointer to the partially-constructed `EngineMetadata` to
// pass to the stub functions, so we follow the same inheritance
// trick `BakerServerEngine` uses in `Baker.cpp:770`.
//
// Common and Baker share the Server bin — server-side stub
// registration carries the union of metadata that user code
// addresses through the Common target.
class StandaloneServerMetadata final : public EngineMetadata
{
public:
    StandaloneServerMetadata() :
        EngineMetadata([this] { RegisterServerStubMetadata(this, nullptr); })
    {
    }
};

class StandaloneClientMetadata final : public EngineMetadata
{
public:
    StandaloneClientMetadata() :
        EngineMetadata([this] { RegisterClientStubMetadata(this, nullptr); })
    {
    }
};

class StandaloneMapperMetadata final : public EngineMetadata
{
public:
    StandaloneMapperMetadata() :
        EngineMetadata([this] { RegisterMapperStubMetadata(this, nullptr); })
    {
    }
};

static auto BuildMetadata(string_view target) -> unique_ptr<EngineMetadata>
{
    FO_STACK_TRACE_ENTRY();

    if (target == "Client") {
        return safe_alloc::make_unique<StandaloneClientMetadata>();
    }
    if (target == "Mapper") {
        return safe_alloc::make_unique<StandaloneMapperMetadata>();
    }
    return safe_alloc::make_unique<StandaloneServerMetadata>();
}

// Build the Common-target allowlist: for each entity registered
// in `server`, collect the set of member names (methods, events,
// property setter names `Set<Name>`) that ALSO appear on the same
// entity in `client`. Members in this intersection are the
// platform-safe surface — present regardless of which engine
// binary user code links into. Mirrors codegen.py's
// `_native_method_visible_in_target(method.target, 'Common')`
// which lets only `method.target == 'Common'` methods through.
//
// The result is consulted by the synth via the
// `is_member_visible` callback to gate per-target emission of
// wrapper class bodies for the Common target. Non-Common targets
// see the unfiltered per-bin metadata.
static auto BuildCommonAllowlist(const EngineMetadata& server, const EngineMetadata& client) -> unordered_map<string, unordered_set<string>>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, unordered_set<string>> result;
    for (const auto& [srv_hname, srv_desc] : server.GetEntityTypes()) {
        const string entity_name {srv_hname.as_str()};
        const auto& cli_types = client.GetEntityTypes();
        const auto cli_it = cli_types.find(client.Hashes.to_hashed_string(entity_name));
        if (cli_it == cli_types.end()) {
            continue;
        }
        unordered_set<string> cli_names;
        for (const auto& m : cli_it->second.Methods) {
            cli_names.insert(m.Name);
        }
        for (const auto& e : cli_it->second.Events) {
            cli_names.insert(e.Name);
        }
        {
            const auto& prop_reg = cli_it->second.PropRegistrar;
            for (size_t i = 0; i < prop_reg->GetPropertiesCount(); ++i) {
                nptr<const Property> prop = prop_reg->GetPropertyByIndex(numeric_cast<int32_t>(i));
                if (!prop) {
                    continue;
                }
                cli_names.insert("Set" + string {prop->GetName()});
            }
        }
        auto& entity_allow = result[entity_name];
        for (const auto& m : srv_desc.Methods) {
            if (cli_names.contains(m.Name)) {
                entity_allow.insert(m.Name);
            }
        }
        for (const auto& e : srv_desc.Events) {
            if (cli_names.contains(e.Name)) {
                entity_allow.insert(e.Name);
            }
        }
        {
            const auto& prop_reg = srv_desc.PropRegistrar;
            for (size_t i = 0; i < prop_reg->GetPropertiesCount(); ++i) {
                nptr<const Property> prop = prop_reg->GetPropertyByIndex(numeric_cast<int32_t>(i));
                if (!prop) {
                    continue;
                }
                const string prop_name {prop->GetName()};
                if (cli_names.contains("Set" + prop_name)) {
                    entity_allow.insert("Set" + prop_name);
                }
            }
        }
    }
    return result;
}

// Merge settings from `donor` into `recipient`, skipping entries
// already present in `recipient`. `EngineMetadata::FinalizeRegistration`
// is called only from `BaseEngine`'s constructor — a plain
// `EngineMetadata` instance leaves `_registrationFinalized` false
// even after its stub-registration callback runs, so post-hoc
// `RegisterGameSetting` / `MarkGameSettingAsExported` calls are
// legal. We use this to build a union-of-sides settings view on
// the Server bin for the Common cppm re-export, matching codegen.py
// which emits the union from its Python-side metadata.
static void MergeSettings(EngineMetadata& recipient, const EngineMetadata& donor)
{
    FO_STACK_TRACE_ENTRY();

    const auto& donor_initial_values = donor.GetGameSettingsInitialValues();

    for (const auto& [name, type_ptr] : donor.GetGameSettings()) {
        if (recipient.GetGameSettings().contains(name)) {
            continue;
        }

        // Registration writes the type and the initial value together, so a setting without one means the
        // donor metadata is already inconsistent
        const auto initial_value_it = donor_initial_values.find(name);
        FO_STRONG_ASSERT(initial_value_it != donor_initial_values.end(), "Game setting has no recorded initial value", name);
        recipient.RegisterGameSetting(name, *type_ptr, initial_value_it->second);
    }

    for (const auto& name : donor.GetExportedGameSettings()) {
        if (recipient.IsExportedGameSetting(name)) {
            continue;
        }
        // Order matters: `SetExportedGameSettingType` /
        // `SetExportedGameSettingTypeName` assert the setting is
        // already in `_exportedGameSettings`.
        recipient.MarkGameSettingAsExported(name);
        if (nptr<const BaseTypeDesc> type_desc = donor.FindExportedGameSettingType(name); type_desc) {
            recipient.SetExportedGameSettingType(name, *type_desc);
        }
        if (nptr<const string> type_name = donor.FindExportedGameSettingTypeName(name); type_name) {
            recipient.SetExportedGameSettingTypeName(name, *type_name);
        }
    }
}

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto NativeScriptSynthApp(int argc, char** argv) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

    try {
        CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
        InitApp(args, AppInitFlags::DisableLogTags);

        // argv layout: 0=exe, 1=output_dir, 2=native_scripts_dir
        // (optional — empty when the project has no user native
        // scripts; the dispatcher then emits empty bodies so the
        // `RegisterNativeScriptModules_<Role>` symbols still
        // resolve at engine link time).
        if (argc < 2) {
            logging::write("Usage: LF_NativeScriptSynth <output_dir> [<native_scripts_dir>]");
            exit_app(false);
        }

        const string output_dir = argv[1];
        const string native_scripts_dir = argc >= 3 ? string {argv[2]} : string {};
        logging::write("NativeScriptSynth: output dir = {}", output_dir);
        if (!native_scripts_dir.empty()) {
            logging::write("NativeScriptSynth: native scripts dir = {}", native_scripts_dir);
        }

        // Build per-target metadata via the matching stub
        // registration. Each bin carries its side's entity types,
        // methods, properties, events, and (initially) its own
        // settings. Settings are then unioned across sides so each
        // target's `.cppm` re-exports the same flat
        // `NativeScripts::Settings::*` surface codegen.py emits.
        auto meta_server = BuildMetadata("Server");
        auto meta_client = BuildMetadata("Client");
        auto meta_mapper = BuildMetadata("Mapper");

        // For the `.cppm` Settings re-export codegen.py emits the
        // UNION of all sides' settings on every target — Common-
        // target user code is allowed to read `Settings::Client_Foo`
        // even though `Client_Foo` is client-side. Replicate that
        // breadth by merging the other sides' settings into each
        // target bin before synth.
        MergeSettings(*meta_server, *meta_client);
        MergeSettings(*meta_server, *meta_mapper);
        MergeSettings(*meta_client, *meta_server);
        MergeSettings(*meta_client, *meta_mapper);
        MergeSettings(*meta_mapper, *meta_server);
        MergeSettings(*meta_mapper, *meta_client);

        // The in-memory native API union and NativeApi_ContextRpcMethods.h
        // come off the server bin — it has the union of entity types user
        // code addresses through the Common / Baker target.
        // Per-target metadata lookup — wrapper class bodies pick
        // their methods/properties/events from the matching role's
        // bin so Server-only types don't leak into Client/Mapper
        // wrappers (e.g. `OnStaticItemWalk` event referencing
        // `fo::StaticItem*`). Common target reads the union (server
        // bin) which carries everything Common-targeted user code
        // should see.
        const auto target_meta_lookup = [&](string_view target) -> const EngineMetadata* {
            if (target == "Server") {
                return meta_server.get();
            }
            if (target == "Client") {
                return meta_client.get();
            }
            if (target == "Mapper") {
                return meta_mapper.get();
            }
            // "Common" → union bin (also the default since the
            // single-arg synth uses the passed `meta` for missing
            // lookups).
            return nullptr;
        };
        // Common-target intersection allowlist. Build it from
        // Server∩Client so the Common wrapper only references
        // members present on both sides (no server-only types
        // leak in).
        const auto common_allowlist = BuildCommonAllowlist(*meta_server, *meta_client);
        const auto is_member_visible = [&](string_view target, string_view entity, string_view member) -> bool {
            if (target != "Common") {
                return true;
            }
            const auto it = common_allowlist.find(string(entity));
            if (it == common_allowlist.end()) {
                return false;
            }
            return it->second.contains(string(member));
        };
        WriteOutput(output_dir, "NativeApi_ContextRpcMethods.h", SynthesizeNativeApiContextRpcMethods(*meta_server));

        // Complete per-target module interfaces. Client/Mapper use their
        // own bin (so `import NativeApi.Client;` resolves to
        // CritterView etc., not Critter). Common / Server / Baker
        // share the server bin — Common and Baker live in the
        // Common-target wrapper space which is server-side-flavored.
        for (const auto& target : {"Common", "Server", "Client", "Mapper", "Baker"}) {
            ptr<EngineMetadata> target_meta = meta_server;
            if (string_view {target} == "Client") {
                target_meta = meta_client;
            }
            else if (string_view {target} == "Mapper") {
                target_meta = meta_mapper;
            }
            // The surface is synthesized per target rather than once and shared: the role library
            // compiles only its own target's bodies, and the rest would just be preprocessed away
            const string native_api_surface = SynthesizeNativeApiSurface(*meta_server, target_meta_lookup, is_member_visible, target);
            const string stub_name = strex("NativeApi.{}.cppm", target).str();
            WriteOutput(output_dir, stub_name, SynthesizeNativeApiModule(target, *target_meta, native_api_surface));
        }

        // Per-role dispatcher (`NativeBindings-<Role>.cpp`). Scan
        // the user `.cppm` tree for `export module
        // NativeScripts.User.<Role>.<Name>;` + the module's
        // `void <Init>(const ModuleInitContext&)` entry points,
        // bucket by role, emit the dispatcher for every role
        // (empty body when a role has no user modules so the
        // `RegisterNativeScriptModules_<Role>` symbol still
        // resolves). This was codegen.py's last native-scripting
        // output — moving it here completes the codegen retirement.
        const auto role_modules = ScanNativeScriptModules(std::filesystem::path {fs::make_path(native_scripts_dir)});

        for (const auto& role : GetNativeScriptRoles()) {
            const auto it = role_modules.find(role);
            const auto& modules = it != role_modules.end() ? it->second : vector<NativeScriptModuleInit> {};
            const string bindings_name = "NativeBindings-" + role + ".cpp";
            WriteOutput(output_dir, bindings_name, SynthesizeNativeBindings(role, modules));
        }

        logging::write("NativeScriptSynth: done");
        exit_app(true);
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

#endif // FO_NATIVE_SCRIPTING
