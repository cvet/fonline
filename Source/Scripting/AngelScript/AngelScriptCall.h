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

#if FO_ANGELSCRIPT_SCRIPTING

#include "ScriptSystem.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

struct ScriptDataAccessor final : DataAccessor
{
    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32 override { return ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX; }
    [[nodiscard]] auto GetArraySize(void* data) const -> size_t override;
    [[nodiscard]] auto GetArrayElement(void* data, size_t index) const -> void* override;
    [[nodiscard]] auto GetDictSize(void* data) const -> size_t override;
    [[nodiscard]] auto GetDictElement(void* data, size_t index) const -> pair<void*, void*> override;
    [[nodiscard]] auto GetCallback(void* data) const -> unique_del_ptr<ScriptFuncDesc> override;

    void ClearArray(void* data) const override;
    void AddArrayElement(void* data, void* value) const override;
    void ClearDict(void* data) const override;
    void AddDictElement(void* data, void* key, void* value) const override;
};

static constexpr ScriptDataAccessor SCRIPT_DATA_ACCESSOR;

auto IndexScriptFunc(AngelScript::asIScriptFunction* func) -> ScriptFuncDesc*;
void ScriptGenericCall(AngelScript::asIScriptGeneric* gen, bool add_obj, const function<void(FuncCallData&)>& callback);
void ScriptFuncCall(AngelScript::asIScriptFunction* func, FuncCallData& call);

FO_END_NAMESPACE

#endif
