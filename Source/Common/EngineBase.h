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

#include "EntityProperties.h"
#include "Geometry.h"
#include "Properties.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"
#include "Settings.h"
#include "TimeEventManager.h"
#include "Timer.h"

FO_BEGIN_NAMESPACE

class ScriptImGui;

class EngineMetadata : public NameResolver
{
public:
    using MeatdataRegistrator = function<void()>;

    explicit EngineMetadata(const MeatdataRegistrator& registrator);
    EngineMetadata(const EngineMetadata&) = delete;
    EngineMetadata(EngineMetadata&&) noexcept = delete;
    auto operator=(const EngineMetadata&) = delete;
    auto operator=(EngineMetadata&&) noexcept = delete;
    ~EngineMetadata() override = default;

    [[nodiscard]] auto GetSide() const noexcept -> EngineSideKind { return _side; }
    [[nodiscard]] auto GetPropertyRegistrator(hstring type_name) const noexcept -> const PropertyRegistrator*;
    [[nodiscard]] auto GetPropertyRegistrator(string_view type_name) const noexcept -> const PropertyRegistrator*;
    [[nodiscard]] auto GetPropertyRegistratorForEdit(string_view type_name) -> PropertyRegistrator*;
    [[nodiscard]] auto IsValidBaseType(string_view type_str) const noexcept -> bool;
    [[nodiscard]] auto GetBaseType(string_view type_str) const -> const BaseTypeDesc& override;
    [[nodiscard]] auto GetBaseTypes() const -> const auto& { return _baseTypes; }
    [[nodiscard]] auto ResolveComplexType(string_view type_str) const -> ComplexTypeDesc override;
    [[nodiscard]] auto ResolveComplexType(span<const string_view> tokens) const -> pair<ComplexTypeDesc, size_t>;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int32_t override;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int32_t override;
    [[nodiscard]] auto ResolveEnumValueName(string_view enum_name, int32_t value, bool* failed = nullptr) const -> const string& override;
    [[nodiscard]] auto IsValidEntityType(hstring type_name) const noexcept -> bool;
    [[nodiscard]] auto IsValidEntityType(string_view type_name) const noexcept -> bool;
    [[nodiscard]] auto GetEntityType(hstring type_name) const -> const EntityTypeDesc&;
    [[nodiscard]] auto GetEntityTypes() const noexcept -> const map<hstring, EntityTypeDesc>&;
    [[nodiscard]] auto IsEntityRelative(string_view type_name) const noexcept -> bool { return _entityRelatives.contains(string(type_name)); }
    [[nodiscard]] auto IsFixedType(hstring type_name) const noexcept -> bool;
    [[nodiscard]] auto IsFixedType(string_view type_name) const noexcept -> bool;
    [[nodiscard]] auto GetFixedType(hstring type_name) const -> const EntityTypeDesc&;
    [[nodiscard]] auto GetFixedTypes() const noexcept -> const map<hstring, EntityTypeDesc>&;
    [[nodiscard]] auto GetEntityHolderIdsProp(Entity* holder, hstring entry) const -> const Property*;
    [[nodiscard]] auto GetAllEnums() const noexcept -> const auto& { return _enums; }
    [[nodiscard]] auto GetEnumUnderlyingType(string_view name) const noexcept -> const BaseTypeDesc*
    {
        const auto it = _enumsUnderlyingType.find(string(name));
        return it != _enumsUnderlyingType.end() ? it->second.get() : nullptr;
    }
    [[nodiscard]] auto GetOutboundRemoteCalls() const noexcept -> auto& { return _outboundRemoteCalls; }
    [[nodiscard]] auto GetInboundRemoteCalls() const noexcept -> auto& { return _inboundRemoteCalls; }
    [[nodiscard]] auto GetGameSetting(string_view name) const -> const BaseTypeDesc&;
    [[nodiscard]] auto GetGameSettings() const noexcept -> const auto& { return _gameSettings; }
    // Exported = declared via the engine-side `///@ Export*` tag family
    // (`ExportEnum`, `ExportSettings`, etc.) in engine source. The
    // complement is script-declared via the unqualified forms
    // (`///@ Enum`, `///@ Setting`) in `.fos` / `.cppm` files. The
    // NativeScript bake uses this distinction when synthesizing the
    // per-target module stub — engine-exported settings emit a
    // direct-field getter (`engine->Settings.<Field>`), script-declared
    // ones round-trip through `GetCustomSetting/SetCustomSetting`
    // and need both the getter and the `Set_<Group_Key>` setter.
    [[nodiscard]] auto IsExportedEnum(string_view name) const noexcept -> bool { return _exportedEnums.contains(string(name)); }
    [[nodiscard]] auto IsExportedGameSetting(string_view name) const noexcept -> bool { return _exportedGameSettings.contains(string(name)); }
    [[nodiscard]] auto GetExportedGameSettings() const noexcept -> const auto& { return _exportedGameSettings; }
    // Look up the `BaseTypeDesc` recorded for an engine-exported
    // setting. Codegen emits the type alongside `MarkGameSettingAsExported`
    // when the setting's value type is a primitive base type — that
    // covers the bulk of `///@ ExportSettings`. Complex types
    // (`arr.string`, `dict.string.string`, ...) skip the annotation;
    // callers should fall through to the generated NativeApi.<Role>
    // module surface for those.
    [[nodiscard]] auto FindExportedGameSettingType(string_view name) const noexcept -> const BaseTypeDesc*
    {
        const auto it = _exportedGameSettingsType.find(string(name));
        return it != _exportedGameSettingsType.end() ? it->second.get() : nullptr;
    }
    // Raw value-type string for an engine-exported setting (e.g.
    // `int32`, `arr.string`, `dict.string.string`). Codegen emits
    // this alongside `MarkGameSettingAsExported` for every
    // exported setting — including complex types that
    // `_exportedGameSettingsType` (BaseTypeDesc-only) can't
    // represent. NativeScriptSynth uses this to call
    // `ResolveComplexType` + `ComplexTypeToCpp` and emit the
    // matching `vector<X>` / `map<K, V>` accessor.
    [[nodiscard]] auto FindExportedGameSettingTypeName(string_view name) const noexcept -> const string*
    {
        const auto it = _exportedGameSettingsTypeName.find(string(name));
        return it != _exportedGameSettingsTypeName.end() ? &it->second : nullptr;
    }
    [[nodiscard]] auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> override;
    [[nodiscard]] auto GetRefTypes() const noexcept -> const auto& { return _refTypes; }
    [[nodiscard]] auto GetStructLayouts() const noexcept -> const auto& { return _structLayouts; }
    [[nodiscard]] auto GetAllProtos() const noexcept -> const auto& { return _protoMngr.GetAllProtos(); }
    [[nodiscard]] auto GetProtoItems() const noexcept -> const auto& { return _protoMngr.GetProtoItems(); }
    [[nodiscard]] auto GetProtoCritters() const noexcept -> const auto& { return _protoMngr.GetProtoCritters(); }
    [[nodiscard]] auto GetProtoMaps() const noexcept -> const auto& { return _protoMngr.GetProtoMaps(); }
    [[nodiscard]] auto GetProtoLocations() const noexcept -> const auto& { return _protoMngr.GetProtoLocations(); }
    [[nodiscard]] auto GetProtoItem(hstring proto_id) const noexcept -> const ProtoItem*;
    [[nodiscard]] auto GetProtoCritter(hstring proto_id) const noexcept -> const ProtoCritter*;
    [[nodiscard]] auto GetProtoMap(hstring proto_id) const noexcept -> const ProtoMap*;
    [[nodiscard]] auto GetProtoLocation(hstring proto_id) const noexcept -> const ProtoLocation*;
    [[nodiscard]] auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> const ProtoEntity* override;
    [[nodiscard]] auto GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&;

    void RegisterSide(EngineSideKind side);
    auto RegisterEntityType(string_view name, bool exported, bool is_global, bool has_protos, bool has_statics, bool has_abstract) -> PropertyRegistrator*;
    // Annotate the per-role C++ class names for a registered entity type.
    // Codegen emits this after `RegisterEntityType` for
    // `///@ ExportEntity <Name> <Server> <Client>` — captures the pair
    // so NativeScriptSynth can emit `using ::fo::<Server>;` /
    // `using ::fo::<Client>;` in `NativeApi.<Target>.cppm`. Either name
    // may be empty for engine-only entities; empty strings tell the
    // baker to skip that role's re-export.
    void SetEntityClassNames(string_view name, string_view server_class, string_view client_class);
    auto RegisterFixedType(string_view name, bool exported) -> PropertyRegistrator*;
    void RegsiterEntityHolderEntry(string_view holder_type, string_view target_type, string_view entry, EntityHolderEntrySync sync, bool persistent);
    void RegisterEnumGroup(string_view name, string_view underlying_type, unordered_map<string, int32_t>&& key_values);
    void RegisterEnumEntry(string_view name, string_view entry_name, int32_t entry_value);
    void RegisterValueType(string_view name);
    void RegisterValueTypeLayout(string_view name, const vector<pair<string_view, string_view>>& layout);
    // Annotate the C++ alias name for a registered ValueType. Codegen
    // emits this after `RegisterValueType` for `///@ ExportValueType`
    // tags whose meta name differs from the C++ source name (e.g. meta
    // `ident` / C++ `ident_t`, meta `ipos` / C++ `ipos32`).
    // NativeScriptSynth uses the annotation when emitting `using ::fo::*`
    // re-exports in `NativeApi.<Target>.cppm` so the cppm references
    // the actual engine-source alias.
    void SetValueTypeNativeType(string_view name, string_view native_type);
    void RegisterRefType(string_view name);
    void RegisterRefTypeLayout(string_view name, const vector<pair<string_view, string_view>>& layout);
    void RegisterRefTypeMethods(string_view name, vector<MethodDesc>&& methods);
    void RegisterRefTypeMethod(string_view name, MethodDesc&& method);
    // Annotate the target role (`Common` / `Server` / `Client` / `Mapper`)
    // for a registered RefType. Codegen emits this after `RegisterRefType`
    // from the `///@ ExportRefType <Target> <Name>` first token.
    // NativeScriptSynth uses it to skip server-only / client-only ref
    // types when emitting `NativeApi.<Target>.cppm`.
    void SetRefTypeTarget(string_view name, string_view target);
    void RegisterEntityMethods(string_view entity_name, vector<MethodDesc>&& methods);
    void RegisterEntityMethod(string_view entity_name, MethodDesc&& method);
    void RegisterEntityEvents(string_view entity_name, vector<EntityEventDesc>&& events);
    void RegisterEntityEvent(string_view entity_name, EntityEventDesc&& event);
    void RegisterOutboundRemoteCall(RemoteCallDesc&& remote_call);
    void RegisterInboundRemoteCall(RemoteCallDesc&& remote_call);
    void RegisterGameSetting(string_view name, const BaseTypeDesc& type);
    void MarkGameSettingAsExported(string_view name);
    // Annotate the BaseTypeDesc for an engine-exported setting whose
    // value type resolves to a primitive base. Codegen emits this
    // alongside `MarkGameSettingAsExported` so the baker can synthesise
    // `engine->Settings.<Field>` accessors with the right return type.
    // Skipped for complex types (`arr.*` / `dict.*`) — codegen.py's
    // broader type mapper still owns those.
    void SetExportedGameSettingType(string_view name, const BaseTypeDesc& type);
    // Record the raw value-type string (`int32`, `arr.string`,
    // `dict.string.string`, ...) for an engine-exported setting.
    // Captures every exported setting, including complex types
    // that `SetExportedGameSettingType` (BaseTypeDesc-only) skips.
    void SetExportedGameSettingTypeName(string_view name, string_view type_name);
    void MarkEnumAsExported(string_view name);
    void RegisterMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules);
    void RegisterMigrationRule(string_view rule_name, string_view extra_info, string_view target, string_view replacement);
    void RegisterProtos(const FileSystem& resources);
    void RegisterProto(hstring type_name, const refcount_ptr<ProtoEntity>& proto);
    void FinalizeRegistration();

    mutable HashStorage Hashes {};

private:
    auto RegisterBaseType(string_view type_str) -> BaseTypeDesc&;

    EngineSideKind _side {};
    bool _registrationFinalized {};
    ProtoManager _protoMngr;
    map<hstring, EntityTypeDesc> _entityTypes {};
    map<hstring, EntityTypeDesc> _fixedTypes {};
    unordered_map<string, raw_ptr<EntityTypeDesc>> _entityRelatives {};
    unordered_map<string_view, raw_ptr<EntityTypeDesc>> _entityTypesByStr {};
    unordered_map<string_view, raw_ptr<EntityTypeDesc>> _fixedTypesByStr {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _entityEntries {};
    unordered_map<string, unordered_map<string, int32_t>> _enums {};
    unordered_map<string, unordered_map<int32_t, string>> _enumsRev {};
    unordered_map<string, int32_t> _enumsFullName {};
    unordered_map<string, raw_ptr<const BaseTypeDesc>> _enumsUnderlyingType {};
    unordered_map<string, StructLayoutDesc> _structLayouts {};
    unordered_map<string, RefTypeDesc> _refTypes {};
    unordered_map<string, unique_ptr<PropertyRegistrator>> _dynamicRefTypeRegistrators {};
    unordered_map<string, BaseTypeDesc> _baseTypes {};
    unordered_map<hstring, RemoteCallDesc> _outboundRemoteCalls {};
    unordered_map<hstring, RemoteCallDesc> _inboundRemoteCalls {};
    unordered_map<string, raw_ptr<const BaseTypeDesc>> _gameSettings {};
    unordered_set<string> _exportedGameSettings {};
    unordered_map<string, raw_ptr<const BaseTypeDesc>> _exportedGameSettingsType {};
    unordered_map<string, string> _exportedGameSettingsTypeName {};
    unordered_set<string> _exportedEnums {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _migrationRules {};
    string _emptyStr {};
};

class BaseEngine : public EngineMetadata, public ScriptSystem, public Entity, public GameProperties
{
public:
    using RemoteCallHandler = function<void(hstring, Entity*, span<uint8_t>)>;
    enum class RemoteCallHandlerMode : uint8_t
    {
        // Reject any existing handler.
        Strict,
        // Register a handler that a later authoritative backend may replace.
        Fallback,
        // Replace only a fallback; reject duplicate authoritative handlers.
        OverrideFallback,
    };

    BaseEngine(const BaseEngine&) = delete;
    BaseEngine(BaseEngine&&) noexcept = delete;
    auto operator=(const BaseEngine&) = delete;
    auto operator=(BaseEngine&&) noexcept = delete;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return "Engine"; }
    [[nodiscard]] auto IsGlobal() const noexcept -> bool override { return true; }
    [[nodiscard]] auto GetImGui() noexcept -> ScriptImGui* { return _imgui.get(); }
    [[nodiscard]] auto Random(int32_t min_value, int32_t max_value) const -> int32_t;

    virtual void Shutdown() { }
    void FrameAdvance();

    void SendRemoteCall(hstring name, Entity* caller, const_span<uint8_t> data);
    void SetRemoteCallHandler(hstring name, RemoteCallHandler handler, RemoteCallHandlerMode mode = RemoteCallHandlerMode::Strict);
    void VerifyBindedRemoteCalls() const noexcept(false);
    // Dispatch a pre-decoded inbound RemoteCall to the registered handler.
    // Derived `ServerEngine` / `ClientEngine` call this from the network
    // packet handler. Also useful for synthetic in-process tests of
    // inbound binders (no networking required).
    void HandleInboundRemoteCall(hstring name, Entity* caller, span<uint8_t> data);

    GlobalSettings& Settings;
    FileSystem Resources;
    GameTimer GameTime;
    TimeEventManager TimeEventMngr;
    unique_del_ptr<uint8_t> UserData {};

protected:
    explicit BaseEngine(GlobalSettings& settings, FileSystem&& resources, const MeatdataRegistrator& registrator);
    ~BaseEngine() override = default;

    virtual void HandleOutboundRemoteCall(hstring name, Entity* caller, const_span<uint8_t> data) { ignore_unused(name, caller, data); } // Managed by derived class

private:
    refcount_ptr<ScriptImGui> _imgui;
    mutable std::mt19937 _randomGenerator {MakeSeededRandomGenerator()};
    unordered_map<hstring, RemoteCallHandler> _inboundRemoteCallHandlers {};
    unordered_set<hstring> _fallbackInboundRemoteCallHandlers {};
};

FO_END_NAMESPACE
