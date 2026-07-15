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
#include "TimeEvents.h"
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
    [[nodiscard]] auto GetPropertyRegistrator(hstring type_name) const noexcept -> nptr<const PropertyRegistrator>;
    [[nodiscard]] auto GetPropertyRegistrator(string_view type_name) const noexcept -> nptr<const PropertyRegistrator>;
    [[nodiscard]] auto GetPropertyRegistratorForEdit(string_view type_name) -> ptr<PropertyRegistrator>;
    [[nodiscard]] auto IsValidBaseType(string_view type_str) const noexcept -> bool;
    [[nodiscard]] auto GetBaseType(string_view type_str) const -> const BaseTypeDesc& override;
    [[nodiscard]] auto GetBaseTypes() const -> const auto& { return _baseTypes; }
    [[nodiscard]] auto ResolveComplexType(string_view type_str) const -> ComplexTypeDesc override;
    [[nodiscard]] auto ResolveComplexType(span<const string_view> tokens) const -> pair<ComplexTypeDesc, size_t>;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_value_name, nptr<bool> failed = nullptr) const -> int32_t override;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_name, string_view value_name, nptr<bool> failed = nullptr) const -> int32_t override;
    [[nodiscard]] auto ResolveEnumValueName(string_view enum_name, int32_t value, nptr<bool> failed = nullptr) const -> string_view override;
    [[nodiscard]] auto IsValidEntityType(hstring type_name) const noexcept -> bool;
    [[nodiscard]] auto IsValidEntityType(string_view type_name) const noexcept -> bool;
    [[nodiscard]] auto GetEntityType(hstring type_name) const -> const EntityTypeDesc&;
    [[nodiscard]] auto GetEntityTypes() const noexcept -> const map<hstring, EntityTypeDesc>&;
    [[nodiscard]] auto IsFixedType(hstring type_name) const noexcept -> bool;
    [[nodiscard]] auto IsFixedType(string_view type_name) const noexcept -> bool;
    [[nodiscard]] auto GetFixedType(hstring type_name) const -> const EntityTypeDesc&;
    [[nodiscard]] auto GetFixedTypes() const noexcept -> const map<hstring, EntityTypeDesc>&;
    [[nodiscard]] auto GetEntityHolderIdsProp(ptr<Entity> holder, hstring entry) const -> ptr<const Property>;
    [[nodiscard]] auto GetAllEnums() const noexcept -> const auto& { return _enums; }
    [[nodiscard]] auto GetOutboundRemoteCalls() const noexcept -> ptr<const unordered_map<hstring, RemoteCallDesc>> { return &_outboundRemoteCalls; }
    [[nodiscard]] auto GetInboundRemoteCalls() const noexcept -> ptr<const unordered_map<hstring, RemoteCallDesc>> { return &_inboundRemoteCalls; }
    [[nodiscard]] auto GetGameSetting(string_view name) const -> const BaseTypeDesc&;
    [[nodiscard]] auto GetGameSettings() const noexcept -> const auto& { return _gameSettings; }
    [[nodiscard]] auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> override;
    [[nodiscard]] auto GetAllProtos() const noexcept -> const auto& { return _protoMngr.GetAllProtos(); }
    [[nodiscard]] auto GetProtoItems() const noexcept -> const auto& { return _protoMngr.GetProtoItems(); }
    [[nodiscard]] auto GetProtoCritters() const noexcept -> const auto& { return _protoMngr.GetProtoCritters(); }
    [[nodiscard]] auto GetProtoMaps() const noexcept -> const auto& { return _protoMngr.GetProtoMaps(); }
    [[nodiscard]] auto GetProtoLocations() const noexcept -> const auto& { return _protoMngr.GetProtoLocations(); }
    [[nodiscard]] auto GetProtoItem(hstring proto_id) const noexcept -> nptr<const ProtoItem>;
    [[nodiscard]] auto GetProtoCritter(hstring proto_id) const noexcept -> nptr<const ProtoCritter>;
    [[nodiscard]] auto GetProtoMap(hstring proto_id) const noexcept -> nptr<const ProtoMap>;
    [[nodiscard]] auto GetProtoLocation(hstring proto_id) const noexcept -> nptr<const ProtoLocation>;
    [[nodiscard]] auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> nptr<const ProtoEntity> override;
    [[nodiscard]] auto GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&;
    [[nodiscard]] auto GetModelAnimDuration(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) const -> timespan;

    void RegisterSide(EngineSideKind side);
    auto RegisterEntityType(string_view name, bool exported, bool is_global, bool has_protos, bool has_statics, bool has_abstract) -> ptr<PropertyRegistrator>;
    auto RegisterFixedType(string_view name, bool exported) -> ptr<PropertyRegistrator>;
    void RegsiterEntityHolderEntry(string_view holder_type, string_view target_type, string_view entry, EntityHolderEntrySync sync, bool persistent);
    void RegisterEnumGroup(string_view name, string_view underlying_type, unordered_map<string, int32_t>&& key_values);
    void RegisterEnumEntry(string_view name, string_view entry_name, int32_t entry_value);
    void RegisterValueType(string_view name);
    void RegisterValueTypeLayout(string_view name, const vector<pair<string_view, string_view>>& layout);
    void RegisterRefType(string_view name);
    void RegisterRefTypeLayout(string_view name, const vector<vector<string_view>>& layout);
    void RegisterRefTypeMethods(string_view name, vector<MethodDesc>&& methods);
    void RegisterRefTypeMethod(string_view name, MethodDesc&& method);
    void RegisterEntityMethods(string_view entity_name, vector<MethodDesc>&& methods);
    void RegisterEntityMethod(string_view entity_name, MethodDesc&& method);
    void RegisterEntityEvents(string_view entity_name, vector<EntityEventDesc>&& events);
    void RegisterEntityEvent(string_view entity_name, EntityEventDesc&& event);
    void RegisterOutboundRemoteCall(RemoteCallDesc&& remote_call);
    void RegisterInboundRemoteCall(RemoteCallDesc&& remote_call);
    void RegisterGameSetting(string_view name, const BaseTypeDesc& type);
    void RegisterMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules);
    void RegisterMigrationRule(string_view rule_name, string_view extra_info, string_view target, string_view replacement);
    void RegisterProtos(const FileSystem& resources);
    void RegisterModelAnimInfo(const FileSystem& resources);
    void RegisterProto(hstring type_name, refcount_ptr<ProtoEntity> proto);
    void FinalizeRegistration();

    mutable HashStorage Hashes {};

private:
    auto RegisterBaseType(string_view type_str) -> ptr<BaseTypeDesc>;

    EngineSideKind _side {};
    bool _registrationFinalized {};
    map<hstring, EntityTypeDesc> _entityTypes {};
    map<hstring, EntityTypeDesc> _fixedTypes {};
    unordered_map<string, ptr<EntityTypeDesc>> _entityRelatives {};
    unordered_map<string_view, ptr<EntityTypeDesc>> _entityTypesByStr {};
    unordered_map<string_view, ptr<EntityTypeDesc>> _fixedTypesByStr {};
    ProtoManager _protoMngr;
    unordered_map<hstring, unordered_map<pair<CritterStateAnim, CritterActionAnim>, timespan>> _modelAnimDurations {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _entityEntries {};
    unordered_map<string, unordered_map<string, int32_t>> _enums {};
    unordered_map<string, unordered_map<int32_t, string>> _enumsRev {};
    unordered_map<string, int32_t> _enumsFullName {};
    unordered_map<string, ptr<const BaseTypeDesc>> _enumsUnderlyingType {};
    unordered_map<string, StructLayoutDesc> _structLayouts {};
    unordered_map<string, RefTypeDesc> _refTypes {};
    unordered_map<string, unique_ptr<PropertyRegistrator>> _dynamicRefTypeRegistrators {};
    unordered_map<string, BaseTypeDesc> _baseTypes {};
    unordered_map<hstring, RemoteCallDesc> _outboundRemoteCalls {};
    unordered_map<hstring, RemoteCallDesc> _inboundRemoteCalls {};
    unordered_map<string, ptr<const BaseTypeDesc>> _gameSettings {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _migrationRules {};
    string _emptyStr {};
};

class BaseEngine : public EngineMetadata, public ScriptSystem, public Entity, public GameProperties
{
public:
    using RemoteCallHandler = function<void(hstring, nptr<Entity>, span<uint8_t>)>;

    BaseEngine(const BaseEngine&) = delete;
    BaseEngine(BaseEngine&&) noexcept = delete;
    auto operator=(const BaseEngine&) = delete;
    auto operator=(BaseEngine&&) noexcept = delete;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return "Engine"; }
    [[nodiscard]] auto IsGlobal() const noexcept -> bool override { return true; }
    [[nodiscard]] auto GetImGui() noexcept -> ptr<ScriptImGui> { return _imgui; }

    auto Random(int32_t min_value, int32_t max_value) const -> int32_t;
    virtual void Shutdown() { }
    void FrameAdvance();

    virtual void ScheduleDelayedCallback(timespan delay, function<void()> body);
    virtual void RunScriptContext(const function<void()>& callback);

    void SendRemoteCall(hstring name, ptr<Entity> caller, const_span<uint8_t> data);
    [[nodiscard]] auto HasRemoteCallHandler(hstring name) const -> bool;
    void SetRemoteCallHandler(hstring name, RemoteCallHandler handler, bool replace = false);
    void VerifyBindedRemoteCalls() const noexcept(false);
    // Dispatch an inbound remote call to its registered handler. Normally invoked by the derived engine when a call
    // arrives over the network; also callable in-process (e.g. a scripting backend dispatching a locally serialized
    // call). Access only — no change to wire or runtime behavior.
    void HandleInboundRemoteCall(hstring name, nptr<Entity> caller, span<uint8_t> data);

    ptr<GlobalSettings> Settings;
    FileSystem Resources;
    GameTimer GameTime;
    TimeEventManager TimeEventMngr;
    unique_del_nptr<uint8_t> UserData {};

protected:
    explicit BaseEngine(ptr<GlobalSettings> settings, FileSystem&& resources, const MeatdataRegistrator& registrator);
    ~BaseEngine() override = default;

    virtual void HandleOutboundRemoteCall(hstring name, ptr<Entity> caller, const_span<uint8_t> data) { ignore_unused(name, caller, data); } // Managed by derived class

private:
    refcount_ptr<ScriptImGui> _imgui;
    mutable mutex _randomGeneratorLocker {};
    mutable std::mt19937 _randomGenerator FO_TSA_GUARDED_BY(_randomGeneratorLocker) {MakeSeededRandomGenerator()};
    unordered_map<hstring, RemoteCallHandler> _inboundRemoteCallHandlers {};
};

FO_END_NAMESPACE
