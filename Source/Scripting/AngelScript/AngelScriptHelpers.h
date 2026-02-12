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

#include "AngelScriptArray.h"
#include "AngelScriptDict.h"
#include "Entity.h"
#include "Properties.h"
#include "ScriptSystem.h"

#include <angelscript.h>

#ifdef AS_MAX_PORTABILITY
#include "AngelScriptWrappedCall-Include.h"
#endif

FO_BEGIN_NAMESPACE

#define FO_AS_VERIFY(expr) \
    as_result = expr; \
    if (as_result < 0) { \
        ThrowScriptCoreException(__FILE__, __LINE__, as_result); \
    }

#ifdef AS_MAX_PORTABILITY
#define FO_SCRIPT_GENERIC(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_FUNC(name) WRAP_FN(name)
#define FO_SCRIPT_FUNC_EXT(name, params, ret) WRAP_FN_PR(name, params, ret)
#define FO_SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define FO_SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define FO_SCRIPT_METHOD_EXT(type, name, params, ret) WRAP_MFN_PR(type, name, params, ret)
#define FO_SCRIPT_GENERIC_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_FUNC_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_FUNC_THIS_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_METHOD_CONV AngelScript::asCALL_GENERIC
#else
#define FO_SCRIPT_GENERIC(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_FUNC(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_FUNC_EXT(name, params, ret) AngelScript::asFUNCTIONPR(name, params, ret)
#define FO_SCRIPT_FUNC_THIS(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_METHOD(type, name) AngelScript::asMETHOD(type, name)
#define FO_SCRIPT_METHOD_EXT(type, name, params, ret) AngelScript::asMETHODPR(type, name, params, ret)
#define FO_SCRIPT_GENERIC_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_FUNC_CONV AngelScript::asCALL_CDECL
#define FO_SCRIPT_FUNC_THIS_CONV AngelScript::asCALL_CDECL_OBJFIRST
#define FO_SCRIPT_METHOD_CONV AngelScript::asCALL_THISCALL
#endif

class BaseEngine;
class AngelScriptBackend;

[[noreturn]] void ThrowScriptCoreException(string_view file, int32 line, int32 result);
auto GetScriptBackend(BaseEngine* engine) -> AngelScriptBackend*;
auto GetScriptBackend(AngelScript::asIScriptEngine* as_engine) -> AngelScriptBackend*;
auto GetEngineMetadata(AngelScript::asIScriptEngine* as_engine) -> const EngineMetadata*;
auto GetGameEngine(AngelScript::asIScriptEngine* as_engine) -> BaseEngine*;
void CheckScriptEntityNonNull(const Entity* entity);
void CheckScriptEntityNonDestroyed(const Entity* entity);
auto MakeScriptTypeName(const BaseTypeDesc& type) -> string;
auto MakeScriptTypeName(const ComplexTypeDesc& type) -> string;
auto MakeScriptArgName(const ComplexTypeDesc& type) -> string;
auto MakeScriptArgsName(const_span<ArgDesc> args) -> string;
auto MakeScriptReturnName(const ComplexTypeDesc& type, bool pass_ownership = false) -> string;
auto MakeScriptPropertyName(const Property* prop) -> string;
auto CreateScriptArray(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptArray*;
auto CreateScriptDict(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptDict*;
auto CalcConstructAddrSpace(const Property* prop) -> size_t;
void FreeConstructAddrSpace(const Property* prop, void* construct_addr);
void ConvertPropsToScriptObject(const Property* prop, PropertyRawData& prop_data, void* construct_addr, AngelScript::asIScriptEngine* as_engine);
auto ConvertScriptToPropsObject(const Property* prop, void* as_obj) -> PropertyRawData;
auto GetScriptObjectInfo(const void* ptr, int32 type_id) -> string;
auto GetScriptFuncName(const AngelScript::asIScriptFunction* func, HashResolver& hash_resolver) -> hstring;

FO_END_NAMESPACE

#endif
