// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "ScriptSystem.h"

namespace AngelScript
{
    class asIScriptEngine;
    class asIScriptFunction;
    class asIScriptGeneric;
}

FO_BEGIN_NAMESPACE

class ScriptDataAccessor final : public DataAccessor
{
public:
    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override { return ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX; }
    [[nodiscard]] auto GetArraySize(ptr<void> data) const -> size_t override;
    [[nodiscard]] auto GetArrayElement(ptr<void> data, size_t index) const -> ptr<void> override;
    [[nodiscard]] auto GetDictSize(ptr<void> data) const -> size_t override;
    [[nodiscard]] auto GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>> override;
    [[nodiscard]] auto GetCallback(ptr<void> data) const -> unique_del_nptr<ScriptFuncDesc> override;

    void ClearArray(ptr<void> data) const override;
    void AddArrayElement(ptr<void> data, ptr<void> value) const override;
    void ClearDict(ptr<void> data) const override;
    void AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const override;
};

static constexpr ScriptDataAccessor SCRIPT_DATA_ACCESSOR {};

auto ResolveScriptFuncType(ptr<AngelScript::asIScriptEngine> as_engine, int32_t type_id, uint32_t flags = 0, bool is_ret = false) -> ComplexTypeDesc;
auto IndexScriptFunc(ptr<AngelScript::asIScriptFunction> func) -> ptr<ScriptFuncDesc>;
void ScriptGenericCall(ptr<AngelScript::asIScriptGeneric> gen, bool add_obj, const_span<ArgDesc> args_desc, const function<void(FuncCallData&)>& callback);
void ScriptFuncCall(ptr<AngelScript::asIScriptFunction> func, FuncCallData& call);

FO_END_NAMESPACE

#endif
