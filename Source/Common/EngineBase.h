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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "Properties.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"
#include "Settings.h"
#include "Timer.h"

DECLARE_EXCEPTION(DataRegistrationException);

class FOEngineBase : public HashStorage, public NameResolver, public Entity, public GameProperties
{
public:
    FOEngineBase(const FOEngineBase&) = delete;
    FOEngineBase(FOEngineBase&&) noexcept = delete;
    auto operator=(const FOEngineBase&) = delete;
    auto operator=(FOEngineBase&&) noexcept = delete;

    [[nodiscard]] auto GetName() const -> string_view override { return "Engine"; }
    [[nodiscard]] auto IsGlobal() const -> bool override { return true; }
    [[nodiscard]] auto GetPropertiesRelation() const -> PropertiesRelationType { return _propsRelation; }
    [[nodiscard]] auto GetPropertyRegistrator(string_view class_name) const -> const PropertyRegistrator*;
    [[nodiscard]] auto ResolveEnumValue(const string& enum_value_name, bool* failed = nullptr) const -> int override;
    [[nodiscard]] auto ResolveEnumValue(const string& enum_name, const string& value_name, bool* failed = nullptr) const -> int override;
    [[nodiscard]] auto ResolveEnumValueName(const string& enum_name, int value, bool* failed = nullptr) const -> const string& override;
    [[nodiscard]] auto ResolveGenericValue(const string& str, bool* failed = nullptr) -> int override;
    [[nodiscard]] auto GetAllPropertyRegistrators() const -> const auto& { return _registrators; }
    [[nodiscard]] auto GetAllEnums() const -> const auto& { return _enums; }
    [[nodiscard]] auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const -> optional<hstring> override;

    auto GetOrCreatePropertyRegistrator(string_view class_name) -> PropertyRegistrator*;
    void AddEnumGroup(string_view name, size_t size, unordered_map<string, int>&& key_values);
    void SetMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules);
    void FinalizeDataRegistration();

    GlobalSettings& Settings;
    GeometryHelper Geometry;
    GameTimer GameTime;
    ProtoManager ProtoMngr;
    ScriptSystem* ScriptSys {};
    FileSystem Resources {};
    unique_del_ptr<void> UserData {};

protected:
    FOEngineBase(GlobalSettings& settings, PropertiesRelationType props_relation);
    ~FOEngineBase() override = default;

private:
    const PropertiesRelationType _propsRelation;
    bool _registrationFinalized {};
    unordered_map<string, const PropertyRegistrator*> _registrators {};
    unordered_map<string, unordered_map<string, int>> _enums {};
    unordered_map<string, unordered_map<int, string>> _enumsRev {};
    unordered_map<string, int> _enumsFull {};
    unordered_map<string, size_t> _enumSizes {};
    unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _migrationRules {};
    string _emptyStr {};
};
