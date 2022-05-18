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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Properties.h"
#include "ScriptSystem.h"
#include "Settings.h"

DECLARE_EXCEPTION(DataRegistrationException);
DECLARE_EXCEPTION(EnumResolveException);
DECLARE_EXCEPTION(HashResolveException);
DECLARE_EXCEPTION(HashCollisionException);

class FOEngineBase : public NameResolver, public Entity, public GameProperties
{
public:
    FOEngineBase(const FOEngineBase&) = delete;
    FOEngineBase(FOEngineBase&&) noexcept = delete;
    auto operator=(const FOEngineBase&) = delete;
    auto operator=(FOEngineBase&&) noexcept = delete;

    [[nodiscard]] auto GetName() const -> string_view override { return "Engine"; }
    [[nodiscard]] auto IsGlobal() const -> bool override { return true; }
    [[nodiscard]] auto GetPropertyRegistrator(string_view class_name) const -> const PropertyRegistrator*;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int override;
    [[nodiscard]] auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int override;
    [[nodiscard]] auto ResolveEnumValueName(string_view enum_name, int value, bool* failed = nullptr) const -> string override;
    [[nodiscard]] auto ToHashedString(string_view s) const -> hstring override;
    [[nodiscard]] auto ResolveHash(hstring::hash_t h, bool* failed = nullptr) const -> hstring override;
    [[nodiscard]] auto ResolveGenericValue(string_view str, bool* failed = nullptr) -> int override;
    [[nodiscard]] auto GetAllPropertyRegistrators() const -> const auto& { return _registrators; }
    [[nodiscard]] auto GetAllEnums() const -> const auto& { return _enums; }

    auto GetOrCreatePropertyRegistrator(string_view class_name) -> PropertyRegistrator*;
    void AddEnumGroup(string_view name, const type_info& underlying_type, unordered_map<string, int>&& key_values);
    void FinalizeDataRegistration();

    GlobalSettings& Settings;
    ScriptSystem* ScriptSys {};
    FileSystem FileSys {};

#if !FO_SINGLEPLAYER
    vector<uchar> RestoreInfoBin {};
#endif

protected:
    using RegisterDataCallback = std::function<ScriptSystem*()>;

    FOEngineBase(GlobalSettings& settings, PropertiesRelationType props_relation, const RegisterDataCallback& register_data_callback);
    ~FOEngineBase() override = default;

private:
    const PropertiesRelationType _propsRelation;
    bool _registrationFinalized {};
    unordered_map<string, const PropertyRegistrator*> _registrators {};
    unordered_map<string, unordered_map<string, int>> _enums {};
    unordered_map<string, unordered_map<int, string>> _enumsRev {};
    unordered_map<string, int> _enumsFull {};
    unordered_map<string, const type_info*> _enumTypes {};
    mutable unordered_map<hstring::hash_t, hstring::entry> _hashStorage {};
};
