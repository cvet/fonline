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
    [[nodiscard]] auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int32 override;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int32 override;
    [[nodiscard]] auto ResolveEnumValueName(string_view enum_name, int32 value, bool* failed = nullptr) const -> const string& override;
    [[nodiscard]] auto ResolveGenericValue(string_view str, bool* failed = nullptr) const -> int32 override;
    [[nodiscard]] auto IsValidEntityType(hstring type_name) const noexcept -> bool;
    [[nodiscard]] auto IsValidEntityType(string_view type_name) const noexcept -> bool;
    [[nodiscard]] auto GetEntityType(hstring type_name) const -> const EntityTypeDesc&;
    [[nodiscard]] auto GetEntityTypes() const noexcept -> const map<hstring, EntityTypeDesc>&;
    [[nodiscard]] auto GetEntityHolderIdsProp(Entity* holder, hstring entry) const -> const Property*;
    [[nodiscard]] auto GetAllEnums() const noexcept -> const auto& { return _enums; }
    [[nodiscard]] auto GetOutboundRemoteCalls() const noexcept -> auto& { return _outboundRemoteCalls; }
    [[nodiscard]] auto GetInboundRemoteCalls() const noexcept -> auto& { return _inboundRemoteCalls; }
    [[nodiscard]] auto GetGameSetting(string_view name) const -> const BaseTypeDesc&;
    [[nodiscard]] auto GetGameSettings() const noexcept -> const auto& { return _gameSettings; }
    [[nodiscard]] auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> override;

    void RegisterSide(EngineSideKind side);
    auto RegisterEntityType(string_view name, bool exported, bool is_global, bool has_protos, bool has_statics, bool has_abstract) -> PropertyRegistrator*;
    void RegsiterEntityHolderEntry(string_view holder_type, string_view target_type, string_view entry, EntityHolderEntrySync sync);
    void RegisterEnumGroup(string_view name, string_view underlying_type, unordered_map<string, int32>&& key_values);
    void RegisterEnumEntry(string_view name, string_view entry_name, int32 entry_value);
    void RegisterValueType(string_view name);
    void RegisterValueTypeLayout(string_view name, const vector<pair<string_view, string_view>>& layout);
    void RegisterRefType(string_view name);
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
    void FinalizeRegistration();

    mutable HashStorage Hashes;

private:
    void RegisterBaseType(string_view type_str);

    EngineSideKind _side {};
    bool _registrationFinalized {};
    map<hstring, EntityTypeDesc> _entityTypes {};
    unordered_map<string, raw_ptr<EntityTypeDesc>> _entityRelatives {};
    unordered_map<string_view, raw_ptr<EntityTypeDesc>> _entityTypesByStr {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _entityEntries {};
    unordered_map<string, unordered_map<string, int32>> _enums {};
    unordered_map<string, unordered_map<int32, string>> _enumsRev {};
    unordered_map<string, int32> _enumsFullName {};
    unordered_map<string, raw_ptr<const BaseTypeDesc>> _enumsUnderlyingType {};
    unordered_map<string, StructLayoutDesc> _structLayouts {};
    unordered_map<string, RefTypeDesc> _refTypes {};
    unordered_map<string, BaseTypeDesc> _baseTypes {};
    unordered_map<hstring, RemoteCallDesc> _outboundRemoteCalls {};
    unordered_map<hstring, RemoteCallDesc> _inboundRemoteCalls {};
    unordered_map<string, raw_ptr<const BaseTypeDesc>> _gameSettings {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _migrationRules {};
    string _emptyStr {};
};

class BaseEngine : public EngineMetadata, public ScriptSystem, public Entity, public GameProperties
{
public:
    using RemoteCallHandler = function<void(hstring, Entity*, span<uint8>)>;

    BaseEngine(const BaseEngine&) = delete;
    BaseEngine(BaseEngine&&) noexcept = delete;
    auto operator=(const BaseEngine&) = delete;
    auto operator=(BaseEngine&&) noexcept = delete;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return "Engine"; }
    [[nodiscard]] auto IsGlobal() const noexcept -> bool override { return true; }

    virtual void Shutdown() { }
    void FrameAdvance();

    void SendRemoteCall(hstring name, Entity* caller, const_span<uint8> data);
    void SetRemoteCallHandler(hstring name, RemoteCallHandler handler);
    void VerifyBindedRemoteCalls() const noexcept(false);

    GlobalSettings& Settings;
    FileSystem Resources;
    GeometryHelper Geometry;
    GameTimer GameTime;
    ProtoManager ProtoMngr;
    TimeEventManager TimeEventMngr;
    unique_del_ptr<uint8> UserData {};

protected:
    explicit BaseEngine(GlobalSettings& settings, FileSystem&& resources, const MeatdataRegistrator& registrator);
    ~BaseEngine() override = default;

    virtual void HandleOutboundRemoteCall(hstring name, Entity* caller, const_span<uint8> data) { ignore_unused(name, caller, data); } // Managed by derived class
    void HandleInboundRemoteCall(hstring name, Entity* caller, span<uint8> data); // Called by derived class

private:
    unordered_map<hstring, RemoteCallHandler> _inboundRemoteCallHandlers {};
};

FO_END_NAMESPACE
