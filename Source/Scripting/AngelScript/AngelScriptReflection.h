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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ScriptSystem.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(ScriptReflectionException);

class ScriptArray;

class ScriptType
{
public:
    explicit ScriptType(AngelScript::asITypeInfo* ti);

    void AddRef() const;
    void Release() const;

    auto GetName() const -> string;
    auto GetNameWithoutNamespace() const -> string;
    auto GetNamespace() const -> string;
    auto GetModule() const -> string;
    auto GetSize() const -> int;
    auto IsGlobal() const -> bool;
    auto IsClass() const -> bool;
    auto IsInterface() const -> bool;
    auto IsEnum() const -> bool;
    auto IsFunction() const -> bool;
    auto IsShared() const -> bool;
    auto GetBaseType() const -> ScriptType*;
    auto GetInterfaceCount() const -> int;
    auto GetInterface(int index) const -> ScriptType*;
    auto Implements(const ScriptType* other) const -> bool;
    auto Equals(const ScriptType* other) const -> bool;
    auto DerivesFrom(const ScriptType* other) const -> bool;
    auto GetMethodsCount() const -> int;
    auto GetMethodDeclaration(int index, bool include_object_name, bool include_namespace, bool include_param_names) const -> string;
    auto GetPropertiesCount() const -> int;
    auto GetPropertyDeclaration(int index, bool include_namespace) const -> string;
    auto GetEnumLength() const -> int;
    auto GetEnumNames() const -> ScriptArray*;
    auto GetEnumValues() const -> ScriptArray*;

    void Instantiate(void* out, int out_type_id) const;
    void InstantiateCopy(void* in, int in_type_id, void* out, int out_type_id) const;

protected:
    raw_ptr<AngelScript::asITypeInfo> _typeInfo;
    mutable int _refCount {1};
};

class ScriptTypeOf : public ScriptType
{
public:
    explicit ScriptTypeOf(AngelScript::asITypeInfo* ti);

    auto ConvertToType() -> ScriptType*;
};

void RegisterAngelScriptReflection(AngelScript::asIScriptEngine* engine);

FO_END_NAMESPACE();
