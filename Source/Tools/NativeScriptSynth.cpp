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

// NativeScriptSynth.cpp — code emitters for the native scripting
// surface (`NativeApi.<Target>.cppm`,
// `NativeApi_ContextRpcMethods.h`, `NativeBindings-<Target>.cpp`).
// Driven by the standalone build-time tool `LF_NativeScriptSynth`
// (Engine/Source/Applications/NativeScriptSynthApp.cpp), which emits
// these files at engine codegen time, before `NativeScripting` /
// `NativeScripts_<Role>` libs compile. codegen.py no longer emits
// any native scripting files — see Docs/NativeScripting.md.
//
// Inputs: `EngineMetadata` reads (entity types / methods /
// properties / events / settings / value types / ref types / enums /
// RemoteCalls) plus, for the dispatcher, a pre-scanned user-module
// list. No `BakingContext`, `FileCollection`, or `Baker.h`
// dependency — this TU links against `CommonLib` only.

#include "NativeScriptSynth.h"

#if FO_NATIVE_SCRIPTING

FO_BEGIN_NAMESPACE

// Map a codegen meta type to a C++ type usable in the native wrapper
// surface. Mirrors `_native_meta_to_cpp` in codegen.py — see that
// function for the contract details. Returns nullopt for unsupported
// types so callers (wrapper method / property / event emitters) can
// skip the entry.
//
// Args:
//   `target` — current build target (`Common` / `Server` / `Client` /
//     `Mapper`). Decides which engine subclass an entity meta type
//     resolves to (e.g. meta `Critter` on Server → `fo::Critter`, on
//     Client → `fo::CritterView`).
//   `as_engine_ptr` — when true, entity meta types collapse to raw
//     engine pointers (`fo::CritterView*`) instead of the wrapper
//     class (`NativeScripts::Critter`). Used for extern declarations
//     and types nested inside containers where the engine signature
//     must match exactly.
//   `pass_in` — when true, the type is used as a function INPUT.
//     Engine convention: `string` / `any` pass-in become `string_view`;
//     `arr.X` / `dict.K.V` pass-in collapse to `readonly_vector<X>` /
//     `readonly_map<K, V>` (= `const vector<T>&` / `const map<K, V>&`).
static auto MetaToCpp(const EngineMetadata& meta, string_view meta_type, string_view target, bool as_engine_ptr, bool pass_in) -> optional<string>;

// Per-target engine class for an entity meta type. Mirrors
// `_native_entity_engine_class` in codegen.py. `Game` resolves to the
// matching engine handle (`ServerEngine` / `ClientEngine` /
// `MapperEngine` / `BaseEngine`); other entities resolve to the
// `ServerClassName` / `ClientClassName` recorded by
// `SetEntityClassNames`.
static auto EntityEngineClass(const EngineMetadata& meta, string_view entity, string_view target) -> string
{
    if (entity == "Game") {
        if (target == "Server") {
            return "FO_NAMESPACE_NAME::ServerEngine";
        }
        if (target == "Client") {
            return "FO_NAMESPACE_NAME::ClientEngine";
        }
        if (target == "Mapper") {
            return "FO_NAMESPACE_NAME::MapperEngine";
        }
        return "FO_NAMESPACE_NAME::BaseEngine";
    }
    const auto& type_desc = meta.GetEntityType(meta.Hashes.ToHashedString(entity));
    if (target == "Server") {
        if (type_desc.ServerClassName.empty()) {
            return string("FO_NAMESPACE_NAME::") + string(entity);
        }
        return "FO_NAMESPACE_NAME::" + type_desc.ServerClassName;
    }
    if (target == "Client" || target == "Mapper") {
        if (type_desc.ClientClassName.empty()) {
            return string("FO_NAMESPACE_NAME::") + string(entity);
        }
        return "FO_NAMESPACE_NAME::" + type_desc.ClientClassName;
    }
    // Common (and the Baker shim): use the specific class only
    // when it's symmetric across server/client (e.g. `ImGui` /
    // `ScriptImGui`); otherwise the wrapper's engine class
    // collapses to `fo::Entity` since neither Server.h nor
    // Client.h is included for the Common target. Mirrors
    // codegen.py:_native_wrapper_engine_class.
    if (!type_desc.ServerClassName.empty() && type_desc.ServerClassName == type_desc.ClientClassName) {
        return "FO_NAMESPACE_NAME::" + type_desc.ServerClassName;
    }
    return "FO_NAMESPACE_NAME::Entity";
}

// Split a meta type into (base, is_ref). Trailing `.ref` segment
// flags a reference-passing slot.
static auto StripRef(string_view meta_type) -> pair<string, bool>
{
    if (meta_type.ends_with(".ref")) {
        return {string(meta_type.substr(0, meta_type.size() - 4)), true};
    }
    return {string(meta_type), false};
}

// Split a string on a single-character delimiter, returning the parts.
static auto SplitOn(string_view s, char delim) -> vector<string>
{
    vector<string> result;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delim) {
            result.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    result.emplace_back(s.substr(start));
    return result;
}

// Lookup the `NativeType` annotation for a meta value type. Returns
// the annotated alias when codegen recorded one (`ident` →
// `ident_t`), otherwise echoes the meta name. Returns nullopt if
// the name isn't a registered value type.
static auto ResolveValueTypeNative(const EngineMetadata& meta, string_view name) -> optional<string>
{
    const auto& layouts = meta.GetStructLayouts();
    const auto it = layouts.find(string(name));
    if (it == layouts.end()) {
        return std::nullopt;
    }
    return it->second.NativeType.empty() ? string(name) : it->second.NativeType;
}

static auto MetaToCpp(const EngineMetadata& meta, string_view meta_type, string_view target, bool as_engine_ptr, bool pass_in) -> optional<string>
{
    static const unordered_map<string_view, string_view> kSimpleTypeMap = {
        {"void", "void"},
        {"bool", "bool"},
        {"int8", "int8_t"},
        {"uint8", "uint8_t"},
        {"int16", "int16_t"},
        {"uint16", "uint16_t"},
        {"int32", "int32_t"},
        {"uint32", "uint32_t"},
        {"int64", "int64_t"},
        {"uint64", "uint64_t"},
        {"float32", "FO_NAMESPACE_NAME::float32_t"},
        {"float64", "FO_NAMESPACE_NAME::float64_t"},
        {"string", "FO_NAMESPACE_NAME::string"},
        {"hstring", "FO_NAMESPACE_NAME::hstring"},
        {"any", "FO_NAMESPACE_NAME::any_t"},
    };

    const auto [base, is_ref] = StripRef(meta_type);
    const auto parts = SplitOn(base, '.');
    const string& head = parts[0];

    // Engine convention: pass-in string / any becomes `string_view`.
    // Return types and ref parameters stay `string` / `any_t`.
    if ((head == "string" || head == "any") && pass_in && !is_ref) {
        string result = "FO_NAMESPACE_NAME::string_view";
        if (is_ref) {
            result += "&";
        }
        return result;
    }

    string result;
    if (const auto it = kSimpleTypeMap.find(head); it != kSimpleTypeMap.end()) {
        result = string(it->second);
    }
    else if (meta.IsValidEntityType(head)) {
        if (as_engine_ptr) {
            result = EntityEngineClass(meta, head, target) + "*";
        }
        else {
            result = string("NativeScripts::") + head;
        }
    }
    else if (head == "Entity") {
        result = as_engine_ptr ? "FO_NAMESPACE_NAME::Entity*" : "NativeScripts::Entity";
    }
    else if (head == "SELF_ENTITY") {
        // Inside callback signatures `ScriptSelfEntity*` is the
        // engine's base `Entity*` typedef. Callbacks always marshal
        // through the base pointer; user code downcasts in the
        // lambda body.
        result = "FO_NAMESPACE_NAME::Entity*";
    }
    else if (const auto vt = ResolveValueTypeNative(meta, head); vt.has_value()) {
        result = "FO_NAMESPACE_NAME::" + *vt;
    }
    else if (meta.IsEntityRelative(head) || (meta.GetRefTypes().contains(head) && !meta.GetRefTypes().at(head).Target.empty())) {
        // Heap-allocated engine objects accessed by pointer (engine
        // ref types like `DialogPack` + auto-synthesized entity
        // relatives like `ProtoCritter`, `StaticCritter`).
        result = "FO_NAMESPACE_NAME::" + head + "*";
    }
    else if (head == "arr" && parts.size() >= 2) {
        // Recurse on the inner type. Always pass as_engine_ptr=true
        // so the engine instantiation matches — engine functions
        // return `vector<CritterView*>` etc.
        string inner_meta = parts[1];
        for (size_t i = 2; i < parts.size(); ++i) {
            inner_meta += "." + parts[i];
        }
        const auto inner = MetaToCpp(meta, inner_meta, target, true, false);
        if (!inner.has_value()) {
            return std::nullopt;
        }
        if (as_engine_ptr && pass_in && !is_ref) {
            result = "FO_NAMESPACE_NAME::readonly_vector<" + *inner + ">";
        }
        else {
            result = "FO_NAMESPACE_NAME::vector<" + *inner + ">";
        }
    }
    else if (head == "dict" && parts.size() >= 3) {
        const auto key = MetaToCpp(meta, parts[1], target, true, false);
        string value_meta = parts[2];
        for (size_t i = 3; i < parts.size(); ++i) {
            value_meta += "." + parts[i];
        }
        const auto value = MetaToCpp(meta, value_meta, target, true, false);
        if (!key.has_value() || !value.has_value()) {
            return std::nullopt;
        }
        if (as_engine_ptr && pass_in && !is_ref) {
            result = "FO_NAMESPACE_NAME::readonly_map<" + *key + ", " + *value + ">";
        }
        else {
            result = "FO_NAMESPACE_NAME::map<" + *key + ", " + *value + ">";
        }
    }
    else if (head == "callback" && parts.size() >= 2) {
        // callback.<Ret>|<Arg1>|<Arg2>|... — pipe-delimited inner.
        string inner_raw = parts[1];
        for (size_t i = 2; i < parts.size(); ++i) {
            inner_raw += "." + parts[i];
        }
        const auto inner_parts = SplitOn(inner_raw, '|');
        vector<string> bridged;
        bridged.reserve(inner_parts.size());
        for (const auto& p : inner_parts) {
            if (p.empty()) {
                continue;
            }
            const auto mapped = MetaToCpp(meta, p, target, true, false);
            if (!mapped.has_value()) {
                return std::nullopt;
            }
            bridged.emplace_back(*mapped);
        }
        result = "FO_NAMESPACE_NAME::ScriptFunc<";
        for (size_t i = 0; i < bridged.size(); ++i) {
            if (i != 0) {
                result += ", ";
            }
            result += bridged[i];
        }
        result += ">";
    }
    else if (meta.GetAllEnums().contains(head) && meta.IsExportedEnum(head)) {
        // Engine-exported enums live in `fo::`. Script-declared enums
        // live in `NativeScripts::` and would need a different
        // resolution path — codegen currently emits them as bare
        // names with no namespace prefix from user code.
        result = "FO_NAMESPACE_NAME::" + head;
    }
    else {
        return std::nullopt;
    }

    if (is_ref) {
        result += "&";
    }
    return result;
}

// Map an engine-resolved `ComplexTypeDesc` (the form `MethodDesc::Args` /
// `MethodDesc::Ret` carry at runtime) to a C++ type string. Dispatches
// on `Kind` and chains through `MetaToCpp` for the base/key types.
// Returns nullopt on unsupported types so callers can skip the method.
static auto ComplexTypeToCpp(const EngineMetadata& meta, const ComplexTypeDesc& type, string_view target, bool as_engine_ptr, bool pass_in) -> optional<string>
{
    if (type.Kind == ComplexTypeKind::None) {
        return string("void");
    }

    auto base_name = [](const BaseTypeDesc& bt) -> string { return bt.Name; };

    switch (type.Kind) {
    case ComplexTypeKind::Simple:
        return MetaToCpp(meta, base_name(type.BaseType), target, as_engine_ptr, pass_in);
    case ComplexTypeKind::Array: {
        const auto inner = MetaToCpp(meta, base_name(type.BaseType), target, true, false);
        if (!inner.has_value()) {
            return std::nullopt;
        }
        if (as_engine_ptr && pass_in) {
            return string("FO_NAMESPACE_NAME::readonly_vector<") + *inner + ">";
        }
        return string("FO_NAMESPACE_NAME::vector<") + *inner + ">";
    }
    case ComplexTypeKind::Dict: {
        if (!type.KeyType.has_value()) {
            return std::nullopt;
        }
        const auto key = MetaToCpp(meta, base_name(*type.KeyType), target, true, false);
        const auto value = MetaToCpp(meta, base_name(type.BaseType), target, true, false);
        if (!key.has_value() || !value.has_value()) {
            return std::nullopt;
        }
        if (as_engine_ptr && pass_in) {
            return string("FO_NAMESPACE_NAME::readonly_map<") + *key + ", " + *value + ">";
        }
        return string("FO_NAMESPACE_NAME::map<") + *key + ", " + *value + ">";
    }
    case ComplexTypeKind::DictOfArray: {
        if (!type.KeyType.has_value()) {
            return std::nullopt;
        }
        const auto key = MetaToCpp(meta, base_name(*type.KeyType), target, true, false);
        const auto value = MetaToCpp(meta, base_name(type.BaseType), target, true, false);
        if (!key.has_value() || !value.has_value()) {
            return std::nullopt;
        }
        const string inner_vec = string("FO_NAMESPACE_NAME::vector<") + *value + ">";
        if (as_engine_ptr && pass_in) {
            return string("FO_NAMESPACE_NAME::readonly_map<") + *key + ", " + inner_vec + ">";
        }
        return string("FO_NAMESPACE_NAME::map<") + *key + ", " + inner_vec + ">";
    }
    case ComplexTypeKind::Callback: {
        if (!type.CallbackArgs) {
            return std::nullopt;
        }
        string result = "FO_NAMESPACE_NAME::ScriptFunc<";
        bool first = true;
        for (const auto& arg : *type.CallbackArgs) {
            const auto mapped = ComplexTypeToCpp(meta, arg, target, true, false);
            if (!mapped.has_value()) {
                return std::nullopt;
            }
            if (!first) {
                result += ", ";
            }
            result += *mapped;
            first = false;
        }
        result += ">";
        return result;
    }
    case ComplexTypeKind::None:
        return string("void");
    }
    return std::nullopt;
}

// Synthesize `NativeApi_ContextRpcMethods.h` — the file
// `NativeScriptCore.h` `#include`-s inside the `struct ModuleInitContext`
// body. Each line is an inline member definition of `ModuleInitContext`
// — no namespace wrapper, no class qualifier, no `static`. Per
// native-side `///@ RemoteCall` tag we emit two methods on the
// context:
//
//   - `<Name>(caller, args...)` — outbound, forwards to
//     `SendRemoteCall<Args...>(name, caller, args...)`.
//   - `Bind_<Name>(handler)` — inbound binder template, forwards
//     to `BindRemoteCall<Args...>(name, handler)`.
//
// Both are sugar over the generic `SendRemoteCall<Args...>` /
// `BindRemoteCall<Args...>` member templates declared earlier in
// the struct body — call sites become `ctx.<Name>(caller, args...)` /
// `ctx.Bind_<Name>(handler)`. Mirrors `_native_render_context_rpc_methods`
// in codegen.py.
//
// User-origin native RemoteCalls are identified by
// `RemoteCallDesc::SubsystemHint == "native"` — codegen.py emits this
// tag for `///@ RemoteCall` declarations in `.cppm` files; AS-baked
// RemoteCalls from `.fos` files carry the matching `.fos` hint
// instead and are skipped here (AS owns their binding through the
// `BindAngelScriptRemoteCalls` path).
auto SynthesizeNativeApiContextRpcMethods(const EngineMetadata& meta) -> string
{
    string body;
    body.reserve(2048);

    body += "// Generated by codegen.py — do not edit.\n";
    body += "// This file is `#include`-d INSIDE the `struct ModuleInitContext` body\n";
    body += "// in `NativeScriptCore.h`. Each line is an inline member definition of\n";
    body += "// `ModuleInitContext` — no namespace wrapper, no class qualifier.\n";
    body += "//\n";
    body += "// Adds typed `<Name>(caller, args...)` outbound and `Bind_<Name>(handler)`\n";
    body += "// inbound wrappers for every native-declared `///@ RemoteCall` tag.\n";
    body += "// Both forward to the generic `SendRemoteCall<Args...>` / `BindRemoteCall<Args...>`\n";
    body += "// member templates declared earlier in the struct body — call sites become\n";
    body += "// `ctx.<Name>(caller, args...)` / `ctx.Bind_<Name>(handler)`.\n";
    body += "\n";

    // Collect native-origin RemoteCalls. Outbound and inbound
    // metadata maps each have one entry per RemoteCall name (on the
    // appropriate side); we dedupe by name across both maps so the
    // emit is symmetric.
    struct RcEntry
    {
        string Name;
        vector<ArgDesc> Args;
    };
    unordered_map<string, RcEntry> by_name;
    auto collect = [&](const auto& map) {
        for (const auto& [_hname, desc] : map) {
            if (desc.SubsystemHint != "native") {
                continue;
            }
            const string name = desc.Name.as_str();
            if (by_name.contains(name)) {
                continue;
            }
            by_name.emplace(name, RcEntry {name, desc.Args});
        }
    };
    collect(meta.GetOutboundRemoteCalls());
    collect(meta.GetInboundRemoteCalls());

    // Stable emission order — sort by name lexicographically. Codegen
    // emits in parse-order but the runtime metadata map is
    // unordered, so we settle for deterministic-by-name.
    vector<string> names;
    names.reserve(by_name.size());
    for (const auto& [name, _] : by_name) {
        names.emplace_back(name);
    }
    std::ranges::sort(names);

    for (const auto& name : names) {
        const auto& rc = by_name.at(name);

        vector<string> cpp_types;
        vector<pair<string, string>> cpp_params; // (type, name)
        bool skip = false;
        for (const auto& arg : rc.Args) {
            const auto cpp = ComplexTypeToCpp(meta, arg.Type, "Common", false, false);
            if (!cpp.has_value()) {
                body += "// skipped remote call: " + name + " (unsupported arg " + arg.Name + ")\n";
                skip = true;
                break;
            }
            cpp_types.emplace_back(*cpp);
            cpp_params.emplace_back(*cpp, arg.Name);
        }
        if (skip) {
            continue;
        }

        string type_list;
        for (size_t i = 0; i < cpp_types.size(); ++i) {
            if (i != 0) {
                type_list += ", ";
            }
            type_list += cpp_types[i];
        }
        string params_suffix;
        for (const auto& [type, pname] : cpp_params) {
            params_suffix += ", " + type + " " + pname;
        }
        string args_forward;
        for (const auto& [_type, pname] : cpp_params) {
            args_forward += ", " + pname;
        }

        // Outbound member function — compile-time-checked args,
        // forwards to the generic SendRemoteCall on the same context.
        body += "void " + name + "(::FO_NAMESPACE_NAME::Entity* caller" + params_suffix + ") const {\n";
        body += "    SendRemoteCall<" + type_list + ">(\"" + name + "\", caller" + args_forward + ");\n";
        body += "}\n";

        // Inbound binder template — handler signature enforced by
        // the generic BindRemoteCall<Args...> instantiation.
        body += "template<typename Handler>\n";
        body += "void Bind_" + name + "(Handler handler) const {\n";
        body += "    BindRemoteCall<" + type_list + ">(\"" + name + "\", std::move(handler));\n";
        body += "}\n";
    }

    // Codegen-emitted version has a blank line separator after the
    // header comment; mirror it so diff(codegen, baker) only shows
    // parse-order vs lexicographic-order divergences for the
    // RemoteCall declarations themselves.
    return body;
}

// Synthesize the declarations owned by each NativeApi.<Role> module.
// The caller embeds this union surface in a named-module purview with
// one NATIVE_SCRIPTS_TARGET_<ROLE> selector active.
auto SynthesizeNativeApiSurface(const EngineMetadata& meta) -> string
{
    // Single-bin call — uses `meta` for every per-target wrapper
    // body. Suitable when the caller doesn't have separate
    // role-specific bins (the in-baker-chain path). Stand-alone
    // builds with proper Server/Client/Mapper bins should call the
    // multi-bin overload to avoid cross-target type leakage.
    return SynthesizeNativeApiSurface(meta, [](string_view) -> const EngineMetadata* { return nullptr; });
}

auto SynthesizeNativeApiSurface(const EngineMetadata& meta, const function<const EngineMetadata*(string_view target_name)>& target_meta_lookup) -> string
{
    // Two-arg call — accept every member visible in the picked-up
    // per-target metadata. Equivalent to a member-allowlist that
    // always returns true.
    return SynthesizeNativeApiSurface(meta, target_meta_lookup, [](string_view, string_view, string_view) { return true; });
}

auto SynthesizeNativeApiSurface(const EngineMetadata& meta, const function<const EngineMetadata*(string_view target_name)>& target_meta_lookup, const function<bool(string_view target_name, string_view entity, string_view member)>& is_member_visible) -> string
{
    string body;
    body.reserve(8192);

    unordered_set<string> auto_property_enums;
    for (const auto& [type_name, _desc] : meta.GetEntityTypes()) {
        auto_property_enums.emplace(type_name.as_str() + "Property");
    }

    // Open the NativeScripts namespace and emit the complete union surface.
    body += "namespace NativeScripts {\n";
    body += "\n";

    // Wrapper class forward decls — every entity in metadata + the
    // generic `Entity` class so wrapper bodies can refer to each
    // other in any order regardless of per-target gating. Iterate
    // `meta.GetEntityTypes()` and sort lexicographically so the
    // emission is deterministic across bake runs.
    vector<string> wrapper_entity_names;
    for (const auto& [type_name, _type_desc] : meta.GetEntityTypes()) {
        wrapper_entity_names.emplace_back(type_name.as_str());
    }
    std::ranges::sort(wrapper_entity_names);
    body += "class Entity;\n";
    for (const auto& entity : wrapper_entity_names) {
        body += "class ";
        body += entity;
        body += ";\n";
    }
    body += "\n";
    body += "// Per-role public context. Engine startup and generated dispatchers pass\n";
    body += "// ModuleInitContextBase; this lightweight derived view adds the target-specific\n";
    body += "// Game wrapper without duplicating the shared context type across modules.\n";
    body += "struct ModuleInitContext : ModuleInitContextBase {\n";
    body += "    ModuleInitContext() noexcept = default;\n";
    body += "    ModuleInitContext(const ModuleInitContextBase& ctx) noexcept : ModuleInitContextBase {ctx} { }\n";
    body += "    [[nodiscard]] auto GetGame() const noexcept -> Game;\n";
    body += "};\n";
    body += "\n";

    // RefType aliases — heap-allocated engine objects accessed by
    // pointer. The script side spells them as plain pointer aliases
    // so user code writes `DialogPack dp = ...;` not
    // `fo::DialogPack* dp = ...;`. Iterate `meta->GetRefTypes()`
    // (engine-exported only; user-origin `///@ RefType` tags from
    // `.fos` files have empty `Target` and no `fo::<Name>` alias to
    // chain through).
    vector<string> ref_alias_names;
    for (const auto& [name, desc] : meta.GetRefTypes()) {
        if (desc.Target.empty()) {
            continue;
        }
        ref_alias_names.emplace_back(name);
    }
    std::ranges::sort(ref_alias_names);
    for (const auto& name : ref_alias_names) {
        body += "using ";
        body += name;
        body += " = FO_NAMESPACE_NAME::";
        body += name;
        body += "*;\n";
    }
    if (!ref_alias_names.empty()) {
        body += "\n";
    }

    // User-origin enum bodies — script-declared `///@ Enum` tags in
    // user `.cppm` files. The role modules emit the full `enum class
    // Name : underlying { Key = Value, ... };` so user code can
    // reference values as `Name::Key`. Engine-exported enums
    // (`///@ ExportEnum`) live in the engine source under their
    // own headers and are forward-declared earlier in the file.
    // Skip auto-property enums (`<Entity>Property`) — those are
    // `using ScriptEnum_uint16` aliases owned by ScriptSystem.h
    // that would conflict with an `enum class` definition.
    vector<string> user_enum_names;
    for (const auto& [enum_name, _entries] : meta.GetAllEnums()) {
        if (meta.IsExportedEnum(enum_name)) {
            continue;
        }
        if (auto_property_enums.contains(enum_name)) {
            continue;
        }
        user_enum_names.emplace_back(enum_name);
    }
    std::ranges::sort(user_enum_names);
    for (const auto& enum_name : user_enum_names) {
        const BaseTypeDesc* underlying = meta.GetEnumUnderlyingType(enum_name);
        if (!underlying) {
            continue;
        }
        string cpp_type;
        if (underlying->IsInt8) {
            cpp_type = "int8_t";
        }
        else if (underlying->IsUInt8) {
            cpp_type = "uint8_t";
        }
        else if (underlying->IsInt16) {
            cpp_type = "int16_t";
        }
        else if (underlying->IsUInt16) {
            cpp_type = "uint16_t";
        }
        else if (underlying->IsInt32) {
            cpp_type = "int32_t";
        }
        else if (underlying->IsUInt32) {
            cpp_type = "uint32_t";
        }
        else if (underlying->IsInt64) {
            cpp_type = "int64_t";
        }
        else if (underlying->IsUInt64) {
            cpp_type = "uint64_t";
        }
        else {
            cpp_type = "int32_t";
        }

        const auto& entries = meta.GetAllEnums().at(enum_name);
        // Stable order — entries come from an unordered_map so sort
        // by value (matches `///@ Enum` source order when keys are
        // implicitly numbered 0, 1, 2, ...).
        vector<pair<string, int32_t>> ordered_entries;
        ordered_entries.reserve(entries.size());
        for (const auto& [key, value] : entries) {
            ordered_entries.emplace_back(key, value);
        }
        std::ranges::sort(ordered_entries, [](const auto& lhs, const auto& rhs) { return lhs.second != rhs.second ? lhs.second < rhs.second : lhs.first < rhs.first; });

        body += "enum class ";
        body += enum_name;
        body += " : ";
        body += cpp_type;
        body += "\n{\n";
        for (const auto& [key, value] : ordered_entries) {
            body += "    ";
            body += key;
            body += " = ";
            body += strex("{}", value).str();
            body += ",\n";
        }
        body += "};\n";
        body += "\n";
    }

    // Settings accessors — mirror codegen.py's `_native_render_settings_block`.
    //
    // Engine-exported settings (`///@ ExportSettings`) resolve to a
    // C++ field on `GlobalSettings` (`engine->Settings.<Field>`,
    // where `Field` is the post-dot part of the setting name).
    // User-origin settings (`///@ Setting` in user `.cppm` files)
    // have no struct field — they round-trip through
    // `engine->Settings.GetCustomSetting(name)` /
    // `SetCustomSetting(name, any_t(...))` with type-specific
    // parsing helpers from `strvex` / `any_t` / `strex`.
    // Inside `NativeScripts::Settings` the engine's `float32_t` /
    // `float64_t` typedefs aren't visible (they live in `fo::`), so
    // qualify them. Integer types are stdlib globals so plain names
    // work; string/hstring already qualify themselves.
    auto base_type_to_cpp = [](const BaseTypeDesc& bt) -> string {
        if (bt.IsBool) {
            return "bool";
        }
        if (bt.IsInt8) {
            return "int8_t";
        }
        if (bt.IsUInt8) {
            return "uint8_t";
        }
        if (bt.IsInt16) {
            return "int16_t";
        }
        if (bt.IsUInt16) {
            return "uint16_t";
        }
        if (bt.IsInt32) {
            return "int32_t";
        }
        if (bt.IsUInt32) {
            return "uint32_t";
        }
        if (bt.IsInt64) {
            return "int64_t";
        }
        if (bt.IsUInt64) {
            return "uint64_t";
        }
        if (bt.IsSingleFloat) {
            return "FO_NAMESPACE_NAME::float32_t";
        }
        if (bt.IsDoubleFloat) {
            return "FO_NAMESPACE_NAME::float64_t";
        }
        if (bt.IsString) {
            return "FO_NAMESPACE_NAME::string";
        }
        if (bt.IsHashedString) {
            return "FO_NAMESPACE_NAME::hstring";
        }
        return "";
    };
    // Map a BaseTypeDesc to the strvex/any parser snippet for
    // GetCustomSetting-based user-origin settings. Mirrors
    // codegen.py:3704-3722. Returns empty for unsupported types.
    auto user_parse_expr = [](const BaseTypeDesc& bt, const string& setting_name, const string& cpp_type) -> string {
        const string getter = "engine->Settings.GetCustomSetting(\"" + setting_name + "\")";
        if (bt.IsBool) {
            return "FO_NAMESPACE_NAME::strvex(" + getter + ").to_bool()";
        }
        if (bt.IsString) {
            return getter;
        }
        if (bt.IsHashedString) {
            return ""; // hstring isn't a documented user setting path; skip
        }
        if (bt.IsSingleFloat) {
            return "FO_NAMESPACE_NAME::strvex(" + getter + ").to_float32()";
        }
        if (bt.IsDoubleFloat) {
            return "FO_NAMESPACE_NAME::strvex(" + getter + ").to_float64()";
        }
        if (bt.IsInt8 || bt.IsUInt8 || bt.IsInt16 || bt.IsUInt16 || bt.IsInt32 || bt.IsUInt32 || bt.IsInt64 || bt.IsUInt64) {
            return "FO_NAMESPACE_NAME::numeric_cast<" + cpp_type + ">(" + "FO_NAMESPACE_NAME::strvex(" + getter + ").to_int64())";
        }
        return "";
    };
    // Map a BaseTypeDesc to the `any_t(...)` set-expression used in
    // `engine->Settings.SetCustomSetting(name, ...)`. Mirrors
    // codegen.py:3731-3737.
    auto user_set_expr = [](const BaseTypeDesc& bt) -> string {
        if (bt.IsString) {
            return "FO_NAMESPACE_NAME::any_t(FO_NAMESPACE_NAME::string(value))";
        }
        // bool / numeric / float all round-trip through strex("{}", value).
        return "FO_NAMESPACE_NAME::any_t(FO_NAMESPACE_NAME::strex(\"{}\", value).str())";
    };

    vector<pair<string, string>> setting_emit; // (symbol, cpp_type)
    string settings_body;

    // Engine-exported settings (primitive types only — complex
    // types like `arr.string` aren't tracked in
    // `_exportedGameSettingsType` because the metadata stub only
    // records `BaseTypeDesc*`. Complex-typed engine settings stay
    // out of the cppm re-export until the metadata pipeline tracks
    // them — see `synth gap` note in Codegen.cmake).
    vector<string> exported_setting_names;
    exported_setting_names.reserve(meta.GetExportedGameSettings().size());
    for (const auto& setting_name : meta.GetExportedGameSettings()) {
        exported_setting_names.emplace_back(setting_name);
    }
    std::ranges::sort(exported_setting_names);
    for (const auto& setting_name : exported_setting_names) {
        const auto dot = setting_name.find('.');
        if (dot == string::npos) {
            continue;
        }
        const string field_name = setting_name.substr(dot + 1);
        string symbol = setting_name;
        std::ranges::replace(symbol, '.', '_');

        // Primitive fast path — `_exportedGameSettingsType`
        // populated for every primitive-typed exported setting.
        string cpp_type;
        if (const BaseTypeDesc* prim = meta.FindExportedGameSettingType(setting_name); prim != nullptr) {
            cpp_type = base_type_to_cpp(*prim);
        }
        // Fall through to the full meta-type mapper for anything
        // `base_type_to_cpp` doesn't recognize — covers complex
        // types (`arr.string`, `dict.string.string`) and value-
        // type bases (`ucolor`, `ipos32`, ...). codegen records
        // the raw value-type string in dot-syntax which
        // `MetaToCpp` handles directly.
        if (cpp_type.empty()) {
            if (const string* type_name = meta.FindExportedGameSettingTypeName(setting_name); type_name != nullptr) {
                if (const auto mapped = MetaToCpp(meta, *type_name, "Common", false, false); mapped.has_value()) {
                    cpp_type = *mapped;
                }
            }
        }
        if (cpp_type.empty()) {
            continue;
        }

        // Inline body: `engine->Settings.<Field>`.
        settings_body += "[[nodiscard]] inline auto ";
        settings_body += symbol;
        settings_body += "(::FO_NAMESPACE_NAME::BaseEngine* engine) noexcept -> ";
        settings_body += cpp_type;
        settings_body += " { return engine->Settings.";
        settings_body += field_name;
        settings_body += "; }\n";
        setting_emit.emplace_back(std::move(symbol), cpp_type);
    }

    // User-origin settings — emit getter + `Set_<Symbol>` setter via
    // `GetCustomSetting` / `SetCustomSetting` with type-specific
    // parsing.
    vector<string> user_setting_names;
    user_setting_names.reserve(meta.GetGameSettings().size());
    for (const auto& [name, _type_ptr] : meta.GetGameSettings()) {
        user_setting_names.emplace_back(name);
    }
    std::ranges::sort(user_setting_names);
    for (const auto& setting_name : user_setting_names) {
        const auto& gs_map = meta.GetGameSettings();
        const auto it = gs_map.find(setting_name);
        if (it == gs_map.end() || it->second == nullptr) {
            continue;
        }
        const BaseTypeDesc& type = *it->second;
        const string cpp_type = base_type_to_cpp(type);
        if (cpp_type.empty()) {
            continue;
        }
        const string parse_expr = user_parse_expr(type, setting_name, cpp_type);
        if (parse_expr.empty()) {
            continue;
        }
        string symbol = setting_name;
        std::ranges::replace(symbol, '.', '_');
        // Getter — not `noexcept` (parsing can throw).
        settings_body += "[[nodiscard]] inline auto ";
        settings_body += symbol;
        settings_body += "(::FO_NAMESPACE_NAME::BaseEngine* engine) -> ";
        settings_body += cpp_type;
        settings_body += " { return ";
        settings_body += parse_expr;
        settings_body += "; }\n";
        // Setter — write through any_t-stringified value.
        const string set_expr = user_set_expr(type);
        settings_body += "inline void Set_";
        settings_body += symbol;
        settings_body += "(::FO_NAMESPACE_NAME::BaseEngine* engine, ";
        settings_body += cpp_type;
        settings_body += " value) { engine->Settings.SetCustomSetting(\"";
        settings_body += setting_name;
        settings_body += "\", ";
        settings_body += set_expr;
        settings_body += "); }\n";
        setting_emit.emplace_back(symbol, cpp_type);
        setting_emit.emplace_back("Set_" + symbol, cpp_type);
    }

    if (!setting_emit.empty()) {
        // Wrap in `namespace Settings { ... }` so accessors live at
        // `NativeScripts::Settings::<Group_Field>` — matches
        // codegen.py's API and the cppm re-exports
        // (`using ::NativeScripts::Settings::Foo;`).
        body += "namespace Settings {\n";
        body += settings_body;
        body += "} // namespace Settings\n";
        body += "\n";
    }

    // Generic `Entity` wrapper — base for the per-target wrapper
    // classes. Always available; engine `fo::Entity` is the
    // common base of every script-visible entity type.
    body += "class Entity : public EntityWrapper<::FO_NAMESPACE_NAME::Entity> {\n";
    body += "public:\n";
    body += "    using EntityWrapper::EntityWrapper;\n";
    body += "};\n";
    body += "\n";

    // Per-target wrapper classes. Each entity gets a body keyed on
    // `NATIVE_SCRIPTS_TARGET_<Target>` with the matching engine class
    // (resolved through `EntityEngineClass`) as its EntityWrapper parameter.
    // Declarations and out-of-class definitions for methods, properties and
    // events are emitted from the matching role metadata below.
    static const vector<pair<string_view, string_view>> kTargetGuards {
        {"Common", "NATIVE_SCRIPTS_TARGET_COMMON"},
        {"Server", "NATIVE_SCRIPTS_TARGET_SERVER"},
        {"Client", "NATIVE_SCRIPTS_TARGET_CLIENT"},
        {"Mapper", "NATIVE_SCRIPTS_TARGET_MAPPER"},
    };
    // Helper: build the meta-type string for a `Property` so we can
    // feed it through `MetaToCpp`. Mirrors the codegen-side shape
    // (`arr.<inner>` / `dict.<key>.<inner>` / `dict.<key>.arr.<inner>`).
    auto property_meta_type = [](const FO_NAMESPACE Property& prop) -> string {
        const string& base_name = prop.GetBaseType().Name;
        if (prop.IsDictOfArray()) {
            return "dict." + prop.GetDictKeyTypeName() + ".arr." + base_name;
        }
        if (prop.IsDict()) {
            return "dict." + prop.GetDictKeyTypeName() + "." + base_name;
        }
        if (prop.IsArray()) {
            return "arr." + base_name;
        }
        return base_name;
    };

    // Helper: get the C++ Get-accessor return type for a Property.
    // Mirrors `_native_property_get_type` in codegen.py — `string` and
    // `any_t` properties return `string_view` (aliases the underlying
    // storage); array/dict/non-string scalar properties return the
    // regular C++ type from `MetaToCpp`.
    auto property_get_type = [&](const FO_NAMESPACE Property& prop, string_view target_name) -> optional<string> {
        if (!prop.IsArray() && !prop.IsDict() && !prop.IsDictOfArray()) {
            const string& base_name = prop.GetBaseType().Name;
            if (base_name == "string" || base_name == "any") {
                return string("FO_NAMESPACE_NAME::string_view");
            }
        }
        return MetaToCpp(meta, property_meta_type(prop), target_name, false, false);
    };

    // Helper: get the C++ Set-accessor param type for a Property.
    auto property_set_type = [&](const FO_NAMESPACE Property& prop, string_view target_name) -> optional<string> { return MetaToCpp(meta, property_meta_type(prop), target_name, false, false); };

    // Helper: emit an event accessor declaration / definition pair.
    // Returns the proxy type (`EventProxy<flag, Args...>` for engine
    // events, `DynamicEventProxy<flag, Args...>` for user-origin
    // events) and a body string to splice into the class.
    auto build_event_proxy_type = [&](string_view target_name, bool is_global, bool exported, const vector<ArgDesc>& args) -> optional<string> {
        const string flag = is_global ? "true" : "false";
        string args_part = flag;
        for (const auto& a : args) {
            const auto mapped = ComplexTypeToCpp(meta, a.Type, target_name, false, true);
            if (!mapped.has_value()) {
                return std::nullopt;
            }
            args_part += ", " + *mapped;
        }
        if (exported) {
            return "::NativeScripts::EventProxy<" + args_part + ">";
        }
        return "::NativeScripts::DynamicEventProxy<" + args_part + ">";
    };

    // Helper: emit the out-of-class definition for a single method.
    // Mirrors `_native_render_method_out_of_class` in codegen.py.
    // Returns nullopt when the method has an arg/return type that
    // `MetaToCpp` / `ComplexTypeToCpp` can't map (e.g. types not
    // exposed through metadata yet); the caller emits a `// skipped`
    // comment instead.
    auto emit_method_definition = [&](string_view entity, string_view target_name, const MethodDesc& method) -> optional<string> {
        const auto ret_cpp = ComplexTypeToCpp(meta, method.Ret, target_name, false, false);
        if (!ret_cpp.has_value()) {
            return std::nullopt;
        }
        vector<pair<string, string>> arg_types;
        for (const auto& arg : method.Args) {
            const auto arg_cpp = ComplexTypeToCpp(meta, arg.Type, target_name, false, true);
            if (!arg_cpp.has_value()) {
                return std::nullopt;
            }
            arg_types.emplace_back(*arg_cpp, arg.Name);
        }

        // `_self` for `Game` is the engine handle itself; for other
        // entity wrappers the engine comes through `_self->GetEngine()`.
        const string meta_expr = entity == "Game" ? "static_cast<const ::FO_NAMESPACE_NAME::EngineMetadata*>(static_cast<const ::FO_NAMESPACE_NAME::BaseEngine*>(_self))" : "static_cast<const ::FO_NAMESPACE_NAME::EngineMetadata*>(static_cast<const ::FO_NAMESPACE_NAME::BaseEngine*>(_self->GetEngine()))";

        string params;
        for (size_t i = 0; i < arg_types.size(); ++i) {
            if (i != 0) {
                params += ", ";
            }
            params += arg_types[i].first;
            params += " ";
            params += arg_types[i].second;
        }
        string forward_args = "_self";
        for (const auto& [_t, name] : arg_types) {
            forward_args += ", ";
            forward_args += name;
        }
        const string lookup_line = "const ::FO_NAMESPACE_NAME::MethodDesc* m_ = "
                                   "::NativeScripts::Detail::LookupEntityMethod(" +
            meta_expr + ", \"" + string(entity) + "\", \"" + method.Name + "\");";

        string body_expr;
        if (*ret_cpp == "void") {
            body_expr = lookup_line + " ::NativeScripts::Detail::DispatchMethodVoid(m_, " + forward_args + ");";
        }
        else if (ret_cpp->starts_with("NativeScripts::")) {
            // Wrapper-typed return — dispatch yields the engine
            // pointer, which the wrapper braced-init wraps.
            const auto engine_ret = ComplexTypeToCpp(meta, method.Ret, target_name, true, false);
            if (!engine_ret.has_value()) {
                return std::nullopt;
            }
            body_expr = lookup_line + " auto r_ = ::NativeScripts::Detail::DispatchMethod<" + *engine_ret + ">(m_, " + forward_args + "); return " + *ret_cpp + " { r_ };";
        }
        else {
            body_expr = lookup_line + " return ::NativeScripts::Detail::DispatchMethod<" + *ret_cpp + ">(m_, " + forward_args + ");";
        }

        return "inline auto " + string(entity) + "::" + method.Name + "(" + params + ") const -> " + *ret_cpp + " { " + body_expr + " }";
    };

    for (const auto& [target_name, guard] : kTargetGuards) {
        body += "#if defined(";
        body += guard;
        body += ")\n";

        // Phase 1: class declarations with inline method decls.
        for (const auto& entity : wrapper_entity_names) {
            const string engine_class = EntityEngineClass(meta, entity, target_name);
            body += "// --- ";
            body += entity;
            body += " (";
            body += target_name;
            body += " target) ---\n";
            body += "class ";
            body += entity;
            body += " : public EntityWrapper<::";
            body += engine_class;
            body += "> {\n";
            body += "public:\n";
            body += "    using EntityWrapper::EntityWrapper;\n";
            if (entity == "Game" && target_name != "Common") {
                body += "    ";
                body += entity;
                body += "(::FO_NAMESPACE_NAME::BaseEngine* engine) noexcept :\n";
                body += "        EntityWrapper {static_cast<::";
                body += engine_class;
                body += "*>(engine)} {}\n";
            }
            // Property accessor declarations: Get / Set / IsNonEmpty
            // per registered property. The baker emits the unified
            // runtime-lookup pattern (`FindProperty` + `GetValueFast`
            // / `SetValue` / `GetRawDataSize`) for both engine-origin
            // and user-origin properties — that works regardless of
            // which path registered the Property and matches what
            // codegen.py emits for user-origin entries. (Engine-origin
            // properties also get a faster `_self->Get<Name>()` path
            // from codegen.py; the baker doesn't currently distinguish
            // them.)
            //
            // Pre-pass: collect method names for property dedup —
            // when a method shares a base name with a property's
            // setter (e.g. method `SetDir` + property `Dir` →
            // setter name `SetDir`), we'd emit `SetDir` twice. The
            // method wins (matches codegen.py's
            // `_native_render_class_method_decls` dedup at
            // codegen.py:3547-3570). Also adds `'Set' + name`
            // so a method named `Dir` shadows property `Dir`'s
            // setter.
            // Pick the target-specific bin when available so the
            // wrapper body's methods/properties/events come from
            // the matching role's metadata. Avoids cross-target
            // type leakage (e.g. Server-only `fo::StaticItem`
            // appearing in the Client wrapper).
            const EngineMetadata* per_target = target_meta_lookup(target_name);
            const EngineMetadata& body_meta = per_target != nullptr ? *per_target : meta;
            const auto& type_desc = body_meta.GetEntityType(body_meta.Hashes.ToHashedString(entity));
            unordered_set<string> visible_method_names;
            for (const auto& method : type_desc.Methods) {
                visible_method_names.insert(method.Name);
                visible_method_names.insert("Set" + method.Name);
            }
            if (type_desc.PropRegistrator) {
                const auto* prop_reg = type_desc.PropRegistrator.get();
                const size_t prop_count = prop_reg->GetPropertiesCount();
                for (size_t i = 0; i < prop_count; ++i) {
                    const auto* prop = prop_reg->GetPropertyByIndexUnsafe(i);
                    if (!prop) {
                        continue;
                    }
                    if (visible_method_names.contains("Get" + prop->GetName()) || visible_method_names.contains("Set" + prop->GetName())) {
                        body += "    // skipped property (overshadowed by method): " + prop->GetName() + "\n";
                        continue;
                    }
                    // Per-target allowlist gate (intersection
                    // filter on Common target). Properties are
                    // queried under their setter name
                    // (`Set<Name>`) — matches the convention used
                    // for method dedup.
                    if (!is_member_visible(target_name, entity, "Set" + prop->GetName())) {
                        continue;
                    }
                    const auto get_cpp = property_get_type(*prop, target_name);
                    const auto set_cpp = property_set_type(*prop, target_name);
                    if (!get_cpp.has_value() || !set_cpp.has_value()) {
                        body += "    // skipped property " + prop->GetName() + "\n";
                        continue;
                    }
                    body += "    [[nodiscard]] auto Get";
                    body += prop->GetName();
                    body += "() const -> ";
                    body += *get_cpp;
                    body += ";\n";
                    body += "    auto Set";
                    body += prop->GetName();
                    body += "(";
                    body += *set_cpp;
                    body += " value) const -> void;\n";
                    body += "    [[nodiscard]] auto IsNonEmpty";
                    body += prop->GetName();
                    body += "() const noexcept -> bool;\n";
                }
            }
            // Event declarations. Engine-origin events (`Exported`)
            // expose an `EventProxy<...>` proxy that wraps the
            // engine-side `EntityEventWrapper<...>` member. User-
            // origin events use `DynamicEventProxy` which routes
            // Subscribe/Fire through the runtime event system by
            // string name.
            for (const auto& event_desc : type_desc.Events) {
                if (!is_member_visible(target_name, entity, event_desc.Name)) {
                    continue;
                }
                const auto proxy = build_event_proxy_type(target_name, type_desc.IsGlobal, event_desc.Exported, event_desc.Args);
                if (!proxy.has_value()) {
                    body += "    // skipped event " + event_desc.Name + "\n";
                    continue;
                }
                body += "    [[nodiscard]] auto ";
                body += event_desc.Name;
                body += "() const noexcept -> ";
                body += *proxy;
                body += ";\n";
            }
            // Method declarations — one inline forward per registered
            // method. Skip when any arg / return type can't be mapped
            // through `ComplexTypeToCpp` (e.g. references types not
            // exposed via metadata yet). The matching out-of-class
            // definition emits later, after every wrapper class is
            // declared, so bodies can freely return / accept other
            // wrapper types by value.
            for (const auto& method : type_desc.Methods) {
                // Per-method target gate — matches codegen.py's
                // `_native_method_visible_in_target(method.Target,
                // build_target)`. Common-tagged methods show
                // everywhere; per-side methods only on matching
                // build (Client tag visible in Mapper too).
                if (!method.Target.empty()) {
                    const bool ok = method.Target == "Common" || method.Target == target_name || (method.Target == "Client" && target_name == "Mapper");
                    if (!ok) {
                        continue;
                    }
                }
                if (!is_member_visible(target_name, entity, method.Name)) {
                    continue;
                }
                const auto ret_cpp = ComplexTypeToCpp(meta, method.Ret, target_name, false, false);
                if (!ret_cpp.has_value()) {
                    body += "    // skipped method " + method.Name + " (unsupported return type)\n";
                    continue;
                }
                vector<pair<string, string>> arg_types; // (cpp_type, name)
                bool ok = true;
                for (const auto& arg : method.Args) {
                    const auto arg_cpp = ComplexTypeToCpp(meta, arg.Type, target_name, false, true);
                    if (!arg_cpp.has_value()) {
                        ok = false;
                        break;
                    }
                    arg_types.emplace_back(*arg_cpp, arg.Name);
                }
                if (!ok) {
                    body += "    // skipped method " + method.Name + " (unsupported arg type)\n";
                    continue;
                }
                // `[[nodiscard]]` only when the method returns a
                // value the caller would normally want — void-return
                // methods don't need the attribute.
                if (*ret_cpp != "void") {
                    body += "    [[nodiscard]] auto ";
                }
                else {
                    body += "    auto ";
                }
                body += method.Name;
                body += "(";
                for (size_t i = 0; i < arg_types.size(); ++i) {
                    if (i != 0) {
                        body += ", ";
                    }
                    body += arg_types[i].first;
                    body += " ";
                    body += arg_types[i].second;
                }
                body += ") const -> ";
                body += *ret_cpp;
                body += ";\n";
            }
            body += "};\n";
        }

        // Phase 2: out-of-class method + property definitions. All
        // wrapper class declarations for this target are emitted by
        // now, so bodies can freely return / accept other wrappers
        // by value (each type is complete at this point). Same
        // per-target meta bin as the decl phase — bodies must come
        // from the same source the decls did, otherwise we'd emit
        // a body for a member the class doesn't declare (or vice
        // versa).
        const EngineMetadata* body_per_target = target_meta_lookup(target_name);
        const EngineMetadata& phase2_meta = body_per_target != nullptr ? *body_per_target : meta;
        for (const auto& entity : wrapper_entity_names) {
            const auto& type_desc = phase2_meta.GetEntityType(phase2_meta.Hashes.ToHashedString(entity));

            // Property bodies — unified runtime-lookup path for all
            // properties (engine + user). The Property pointer is
            // resolved per-call through `FindProperty(name)` because
            // a process can host multiple engine instances with
            // their own PropertyRegistrators; a static cache would
            // hand out a pointer owned by one registrator to a
            // Properties owned by another.
            // Match the in-class dedup: if a method shadows the
            // property's Get/Set name, the in-class block skipped
            // the decl — the body must also be skipped to avoid an
            // out-of-class definition for a member that doesn't
            // exist on the class.
            unordered_set<string> body_visible_method_names;
            for (const auto& method : type_desc.Methods) {
                body_visible_method_names.insert(method.Name);
                body_visible_method_names.insert("Set" + method.Name);
            }
            if (type_desc.PropRegistrator) {
                const auto* prop_reg = type_desc.PropRegistrator.get();
                const size_t prop_count = prop_reg->GetPropertiesCount();
                for (size_t i = 0; i < prop_count; ++i) {
                    const auto* prop = prop_reg->GetPropertyByIndexUnsafe(i);
                    if (!prop) {
                        continue;
                    }
                    if (body_visible_method_names.contains("Get" + prop->GetName()) || body_visible_method_names.contains("Set" + prop->GetName())) {
                        continue;
                    }
                    if (!is_member_visible(target_name, entity, "Set" + prop->GetName())) {
                        continue;
                    }
                    const auto get_cpp = property_get_type(*prop, target_name);
                    const auto set_cpp = property_set_type(*prop, target_name);
                    if (!get_cpp.has_value() || !set_cpp.has_value()) {
                        body += "// skipped property body: " + entity + "::" + prop->GetName() + "\n";
                        continue;
                    }
                    const string lookup_init = "const ::FO_NAMESPACE_NAME::Property* prop = _self->GetProperties().GetRegistrator()->FindProperty(\"" + prop->GetName() + "\")";
                    // Get
                    body += "inline auto " + entity + "::Get" + prop->GetName() + "() const -> " + *get_cpp + " {\n";
                    body += "    " + lookup_init + ";\n";
                    body += "    return _self->GetProperties().GetValueFast<" + *set_cpp + ">(prop);\n";
                    body += "}\n";
                    // Set
                    body += "inline auto " + entity + "::Set" + prop->GetName() + "(" + *set_cpp + " value) const -> void {\n";
                    body += "    " + lookup_init + ";\n";
                    body += "    _self->GetPropertiesForEdit().SetValue(prop, value);\n";
                    body += "}\n";
                    // IsNonEmpty
                    body += "inline auto " + entity + "::IsNonEmpty" + prop->GetName() + "() const noexcept -> bool {\n";
                    body += "    " + lookup_init + ";\n";
                    body += "    return _self->GetProperties().GetRawDataSize(prop) != 0;\n";
                    body += "}\n";
                }
            }

            // Event bodies — `EventProxy {&_self->OnX}` for engine-
            // origin events; `DynamicEventProxy {static_cast<Entity*>
            // (_self), "OnX"}` for user-origin. Engine-origin events
            // only resolve on targets that include the engine class
            // header (the proxy points at an `EntityEventWrapper`
            // member that lives on the role-specific engine class);
            // `Common` target has no such member to address, so skip
            // those bodies in the Common module.
            for (const auto& event_desc : type_desc.Events) {
                if (!is_member_visible(target_name, entity, event_desc.Name)) {
                    continue;
                }
                if (event_desc.Exported && target_name == "Common") {
                    body += "// skipped engine event body on Common target: " + entity + "::" + event_desc.Name + "\n";
                    continue;
                }
                const auto proxy = build_event_proxy_type(target_name, type_desc.IsGlobal, event_desc.Exported, event_desc.Args);
                if (!proxy.has_value()) {
                    body += "// skipped event body: " + entity + "::" + event_desc.Name + "\n";
                    continue;
                }
                body += "inline auto " + entity + "::" + event_desc.Name + "() const noexcept -> " + *proxy + " { return " + *proxy + " {";
                if (event_desc.Exported) {
                    body += "&_self->" + event_desc.Name;
                }
                else {
                    body += "static_cast<::FO_NAMESPACE_NAME::Entity*>(_self), \"" + event_desc.Name + "\"";
                }
                body += "}; }\n";
            }

            for (const auto& method : type_desc.Methods) {
                if (!method.Target.empty()) {
                    const bool ok = method.Target == "Common" || method.Target == target_name || (method.Target == "Client" && target_name == "Mapper");
                    if (!ok) {
                        continue;
                    }
                }
                if (!is_member_visible(target_name, entity, method.Name)) {
                    continue;
                }
                const auto def = emit_method_definition(entity, target_name, method);
                if (!def.has_value()) {
                    body += "// skipped definition: " + entity + "::" + method.Name + "\n";
                    continue;
                }
                body += *def;
                body += "\n";
            }
        }

        // `ModuleInitContext::GetGame()` — per-target body so user
        // code can write `auto game = ctx.GetGame();` without the
        // manual `Game {ctx.Engine}` cast. The per-target Game
        // wrapper has the `BaseEngine*` accepting ctor (added
        // earlier) so the braced-init works regardless of the
        // wrapper's underlying engine class.
        body += "inline auto ModuleInitContext::GetGame() const noexcept -> Game { return Game {Engine}; }\n";

        body += "#endif // ";
        body += guard;
        body += "\n";
        body += "\n";
    }

    // Fallback wrappers for translation units that don't define any
    // target macro (e.g. unit-test code that just wants typenames to
    // be visible). Mirrors codegen.py — emits the Common-target
    // wrapper definitions inside a guard that excludes all four
    // target macros. Methods/properties/events use the Common-target
    // emission rules.
    body += "#if !defined(NATIVE_SCRIPTS_TARGET_COMMON) && \\\n";
    body += "    !defined(NATIVE_SCRIPTS_TARGET_SERVER) && \\\n";
    body += "    !defined(NATIVE_SCRIPTS_TARGET_CLIENT) && \\\n";
    body += "    !defined(NATIVE_SCRIPTS_TARGET_MAPPER)\n";
    for (const auto& entity : wrapper_entity_names) {
        const string engine_class = EntityEngineClass(meta, entity, "Common");
        body += "// --- ";
        body += entity;
        body += " (fallback / no target) ---\n";
        body += "class ";
        body += entity;
        body += " : public EntityWrapper<::";
        body += engine_class;
        body += "> {\n";
        body += "public:\n";
        body += "    using EntityWrapper::EntityWrapper;\n";
        body += "};\n";
    }
    body += "inline auto ModuleInitContext::GetGame() const noexcept -> Game { return Game {Engine}; }\n";
    body += "#endif // fallback wrappers\n";
    body += "\n";

    body += "} // namespace NativeScripts\n";
    body += "\n";

    return body;
}

// Synthesize the per-role dispatcher body that imports each user
// `.cppm` module and calls its `void <Init>(const ModuleInitContext&)`
// entry point. The `RegisterNativeScriptModules_<Target>` symbol
// is always emitted (even with empty `modules`) — engine startup
// glue forward-declares and calls it unconditionally, so the
// linker needs the symbol regardless of whether any user module
// exists in the role.
auto SynthesizeNativeBindings(string_view target, const vector<NativeScriptModuleInit>& modules) -> string
{
    string body;
    body.reserve(512 + modules.size() * 128);
    body += "// Generated by LF_NativeScriptSynth — do not edit.\n";
    body += "// Per-role dispatcher for the Native scripting ";
    body += target;
    body += " user library.\n";
    body += "// Calls each module init function — every user `.cppm` declares an exported\n";
    body += "// `void <Name>(const ModuleInitContext&)` that LF_NativeScriptSynth pattern-matched\n";
    body += "// while scanning the user source tree.\n";
    body += "#include \"NativeScriptCore.h\"\n";
    body += "\n";
    body += "#if FO_NATIVE_SCRIPTING\n";
    body += "\n";

    // Deduplicate module imports — multiple init functions can
    // declare in the same `.cppm` file; the dispatcher only needs
    // one `import` per module.
    unordered_set<string> imported_modules;
    for (const auto& entry : modules) {
        if (imported_modules.insert(entry.Module).second) {
            body += "import ";
            body += entry.Module;
            body += ";\n";
        }
    }
    if (!imported_modules.empty()) {
        body += "\n";
    }

    body += "// Linked into the matching engine library; invoked by ";
    body += target;
    body += " startup\n";
    body += "// glue via the dispatcher pointer passed to InitNativeScripting().\n";
    body += "extern \"C++\" void RegisterNativeScriptModules_";
    body += target;
    body += "(const ::NativeScripts::ModuleInitContextBase& ctx);\n";
    body += "void RegisterNativeScriptModules_";
    body += target;
    body += "(const ::NativeScripts::ModuleInitContextBase& ctx)\n";
    body += "{\n";
    if (modules.empty()) {
        body += "    (void) ctx; // no user modules under FO_NATIVE_SCRIPTS_DIR/";
        body += target;
        body += "/\n";
    }
    else {
        for (const auto& entry : modules) {
            body += "    ::";
            body += entry.Function;
            body += "(ctx); // (";
            body += entry.SourceFileName;
            body += ", module ";
            body += entry.Module;
            body += ")\n";
        }
    }
    body += "}\n";
    body += "\n";
    body += "#endif\n";
    return body;
}

// Synthesize the complete per-role module from the in-memory union
// surface. The shared core remains in the global module; wrappers,
// settings and user enums attach to the named module.
auto SynthesizeNativeApiModule(string_view target, const EngineMetadata& meta, string_view native_api_surface) -> string
{
    const string target_upper = strex(target).upper().str();
    string body;
    body.reserve(native_api_surface.size() + 2048);

    body += "// Generated by LF_NativeScriptSynth — do not edit.\n";
    body += "// Complete per-target C++20 module interface for native scripting.\n";
    body += "// Owns the role-specific wrappers, settings and user enums; user .cppm files only need\n";
    body += "// `import NativeApi.";
    body += target;
    body += ";` to get everything.\n";
    body += "\n";
    body += "module;\n";
    body += "\n";
    body += "// Select the matching wrapper block while synthesised declarations are\n";
    body += "// compiled in this module interface unit.\n";
    body += "#ifndef NATIVE_SCRIPTS_TARGET_";
    body += target_upper;
    body += "\n";
    body += "#  define NATIVE_SCRIPTS_TARGET_";
    body += target_upper;
    body += " 1\n";
    body += "#endif\n";
    body += "\n";
    body += "#include \"NativeScriptCore.h\"\n";
    body += "#if __has_include(\"ImGuiStuff.h\")\n";
    body += "#  include \"ImGuiStuff.h\"\n";
    body += "#endif\n";
    body += "#if __has_include(\"EntityProperties.h\")\n";
    body += "#  include \"EntityProperties.h\"\n";
    body += "#endif\n";
    body += "#if defined(NATIVE_SCRIPTS_TARGET_SERVER) && __has_include(\"Server.h\")\n";
    body += "#  include \"Server.h\"\n";
    body += "#endif\n";
    body += "#if defined(NATIVE_SCRIPTS_TARGET_CLIENT) && __has_include(\"Client.h\")\n";
    body += "#  include \"Client.h\"\n";
    body += "#endif\n";
    body += "#if defined(NATIVE_SCRIPTS_TARGET_MAPPER) && __has_include(\"Mapper.h\")\n";
    body += "#  include \"Mapper.h\"\n";
    body += "#endif\n";
    body += "\n";
    body += "export module NativeApi.";
    body += target;
    body += ";\n";
    body += "\n";
    body += "// Role-owned wrapper surface. Keeping these declarations in the named-module\n";
    body += "// purview gives Common/Server/Client/Mapper distinct, isolated wrapper types.\n";
    body += "export {\n";
    body += native_api_surface;
    body += "}\n";
    body += "\n";
    body += "// Re-export target-neutral bridge primitives declared by NativeScriptCore.h\n";
    body += "// in the global module fragment.\n";
    body += "export namespace NativeScripts\n";
    body += "{\n";
    body += "    using ::NativeScripts::ModuleInitContextBase;\n";
    body += "    using ::NativeScripts::ModuleInitFn;\n";
    body += "\n";
    body += "    using ::NativeScripts::EntityWrapper;\n";
    body += "    using ::NativeScripts::EventProxy;\n";
    body += "    using ::NativeScripts::DynamicEventProxy;\n";
    body += "    using ::NativeScripts::MakeScriptFunc;\n";
    body += "    using ::NativeScripts::MakeGlobalScriptFunc;\n";
    body += "    using ::NativeScripts::SendRemoteCall;\n";
    body += "    using ::NativeScripts::BindRemoteCall;\n";
    body += "}\n";
    body += "\n";

    body += "// `NativeScripts::Detail` low-level RemoteCall codec templates.\n";
    body += "export namespace NativeScripts::Detail\n";
    body += "{\n";
    body += "    using ::NativeScripts::Detail::WriteRemoteCallArg;\n";
    body += "    using ::NativeScripts::Detail::ReadRemoteCallArg;\n";
    body += "}\n";
    body += "\n";

    // Engine `fo::` namespace re-export. C++20 modules don't propagate
    // `using namespace` directives across `import`, so each name user
    // code reaches into has to come through `export using` here. The
    // base set is hardcoded (engine plumbing types in Common.h that
    // aren't tagged in metadata); ValueType and RefType names come
    // from `EngineMetadata` so the surface auto-grows as new
    // `///@ ExportValueType` / `///@ ExportRefType` registrations
    // accrete in the engine.
    //
    // Per-role engine entity classes (`ServerEngine` etc., plus
    // `Player`/`PlayerView` flavors) are also baked in here for the
    // target's role — `EntityTypeDesc` doesn't currently carry the
    // engine class name, so we keep these hardcoded until metadata
    // gets extended.
    vector<string> fo_re_exports;
    unordered_set<string> fo_seen;
    auto add_fo = [&](string_view name) {
        if (!name.empty() && fo_seen.insert(string(name)).second) {
            fo_re_exports.emplace_back(name);
        }
    };

    // Universal base set — these live in Common.h or its direct
    // dependencies (Essentials/ExtendedTypes.h, Common/Geometry.h,
    // TimeRelated.h, NetBuffer.h) which every role compiles.
    for (const auto* name : {"vector", "map", "unordered_map", "set", "unordered_set", "string", "string_view", "hstring", "strex", "strvex", "any_t", "timespan", "DataReader", "DataWriter", "BaseEngine", "Entity", "GlobalSettings", "EngineMetadata", "numeric_cast"}) {
        add_fo(name);
    }
    // Per-role engine handle. ServerEngine and ClientEngine are
    // forward-declared in the global module fragment but their
    // full definitions live under the matching role's include — only
    // re-export the one the target's stub will actually have.
    if (target == "Server") {
        add_fo("ServerEngine");
    }
    if (target == "Client" || target == "Mapper") {
        add_fo("ClientEngine");
    }
    if (target == "Mapper") {
        add_fo("MapperEngine");
    }

    // Per-target game entity classes (Critter / Player / Item / Map /
    // Location + their View counterparts + ImGui surrogate). Server
    // target sees `EntityTypeDesc::ServerClassName`; Client / Mapper
    // see `ClientClassName`. Common / Baker have no per-role wrapper
    // surface and skip these entirely (the `Server.h`/`Client.h`
    // includes aren't on their include path either). Empty class
    // names mean the entity has no native-script wrapper for that
    // role and the re-export is skipped.
    if (target == "Server") {
        for (const auto& [_type_name, type_desc] : meta.GetEntityTypes()) {
            if (!type_desc.ServerClassName.empty()) {
                add_fo(type_desc.ServerClassName);
            }
        }
    }
    else if (target == "Client" || target == "Mapper") {
        for (const auto& [_type_name, type_desc] : meta.GetEntityTypes()) {
            if (!type_desc.ClientClassName.empty()) {
                add_fo(type_desc.ClientClassName);
            }
        }
    }

    // Value types from `///@ ExportValueType` tags — iterate
    // `meta->GetStructLayouts()` which contains exactly the set
    // registered through `RegisterValueType`. Use the `NativeType`
    // annotation (set by codegen via `SetValueTypeNativeType` when
    // the C++ source declares a using-alias differing from the meta
    // name — e.g. meta `ident` / C++ `ident_t`, meta `ipos` / C++
    // `ipos32`). Falls back to the meta name when no override is
    // recorded. Some types live in role-specific engine headers;
    // skip those when emitting for targets that don't include the
    // header (`TextFormat` → Client/FontManager.h, `GamepadState`
    // → Frontend/Application.h).
    static const unordered_set<string> kClientOnlyValueTypes {"TextFormat", "GamepadState"};
    vector<string> vt_names;
    for (const auto& [meta_name, layout] : meta.GetStructLayouts()) {
        if (kClientOnlyValueTypes.contains(meta_name) && target != "Client" && target != "Mapper") {
            continue;
        }
        vt_names.emplace_back(layout.NativeType.empty() ? meta_name : layout.NativeType);
    }
    std::ranges::sort(vt_names);
    for (const auto& name : vt_names) {
        add_fo(name);
    }

    // Ref types from `///@ ExportRefType <Target> <Name>` tags. The
    // `Target` annotation (set by codegen via `SetRefTypeTarget`) is
    // populated for engine-exported ref types only — user-origin
    // `///@ RefType` tags from `.fos` files flow through the AS
    // baker's metadata bin and land in `_refTypes` with empty
    // `Target`. User-origin types are AS-baked classes without a
    // matching `fo::<Name>` C++ alias, so re-exporting them would
    // emit a `using ::fo::<Name>;` that fails to compile. Skip those
    // entirely (the user-origin surface reaches native scripts
    // through `///@ Property` accessors on the ref-type instance,
    // not through a `fo::` namespace name). For engine ref types,
    // `Common` re-exports everywhere; role-specific ones only on
    // the matching cppm.
    vector<string> rt_names;
    for (const auto& [name, desc] : meta.GetRefTypes()) {
        if (desc.Target.empty()) {
            continue;
        }
        if (desc.Target != "Common" && desc.Target != target) {
            continue;
        }
        rt_names.emplace_back(name);
    }
    std::ranges::sort(rt_names);
    for (const auto& name : rt_names) {
        add_fo(name);
    }
    body += "// Engine `fo::` namespace re-export — names user code reaches into directly.\n";
    body += "// `using namespace` directives don't cross module boundaries, so we re-export\n";
    body += "// each name through `export using` here. User .cppm files can then do\n";
    body += "// `using namespace fo;` after `import NativeApi.<Role>;` to get the same\n";
    body += "// unqualified ergonomics of the former generated header.\n";
    body += "export namespace fo\n";
    body += "{\n";
    for (const auto& name : fo_re_exports) {
        body += "    using ::FO_NAMESPACE_NAME::";
        body += name;
        body += ";\n";
    }
    body += "}\n";
    body += "\n";
    body += "// Sentinel constants for compile-time validation in importer TUs.\n";
    body += "export namespace NativeApi::";
    body += target;
    body += "\n";
    body += "{\n";
    body += "    // Identifier visible at compile time to importers — a `static_assert`\n";
    body += "    // against `kTargetName` catches mismatched import (e.g. a Common-target\n";
    body += "    // user TU accidentally importing `NativeApi.Server`).\n";
    body += "    inline constexpr const char* kTargetName = \"";
    body += target;
    body += "\";\n";
    body += "\n";
    body += "    // Bump on pipeline-shape changes that importers should detect.\n";
    body += "    inline constexpr int32_t kPipelineVersion = 2;\n";
    body += "}\n";
    return body;
}

FO_END_NAMESPACE

#endif // FO_NATIVE_SCRIPTING
