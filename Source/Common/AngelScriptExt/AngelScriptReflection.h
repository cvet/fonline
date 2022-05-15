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

#ifndef SCRIPTTYPE_H
#define SCRIPTTYPE_H

#ifndef ANGELSCRIPT_H
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

#include <string>

BEGIN_AS_NAMESPACE

class CScriptArray;

class ScriptType
{
public:
    ScriptType(asITypeInfo* type);
    void AddRef() const;
    void Release() const;
    std::string GetName() const;
    std::string GetNameWithoutNamespace() const;
    std::string GetNamespace() const;
    std::string GetModule() const;
    asUINT GetSize() const;
    bool IsGlobal() const;
    bool IsClass() const;
    bool IsInterface() const;
    bool IsEnum() const;
    bool IsFunction() const;
    bool IsShared() const;
    ScriptType* GetBaseType() const;
    asUINT GetInterfaceCount() const;
    ScriptType* GetInterface(asUINT index) const;
    bool Implements(const ScriptType* other) const;
    bool Equals(const ScriptType* other);
    bool DerivesFrom(const ScriptType* other);
    void Instantiate(void* out, int out_type_id) const;
    void InstantiateCopy(void* in, int in_type_id, void* out, int out_type_id) const;
    asUINT GetMethodsCount() const;
    std::string GetMethodDeclaration(asUINT index, bool include_object_name, bool include_namespace, bool include_param_names) const;
    asUINT GetPropertiesCount() const;
    std::string GetPropertyDeclaration(asUINT index, bool include_namespace) const;
    asUINT GetEnumLength() const;
    CScriptArray* GetEnumNames() const;
    CScriptArray* GetEnumValues() const;

protected:
    mutable int refCount;
    asITypeInfo* objType;
};

class ScriptTypeOf : public ScriptType
{
public:
    ScriptTypeOf(asITypeInfo* type);
    ScriptType* ConvertToType() const;
};

void RegisterScriptReflection(asIScriptEngine* engine);
void RegisterScriptReflection_Native(asIScriptEngine* engine);
void RegisterScriptReflection_Generic(asIScriptEngine* engine);

END_AS_NAMESPACE

#endif
