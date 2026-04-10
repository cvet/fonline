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

#include "DataSerialization.h"

namespace AngelScript
{
    class asIScriptFunction;
    class asIScriptModule;
}

namespace Preprocessor
{
    class Context;
    class Lexem;
    using LexemList = std::list<Lexem>;
    class LineNumberTranslator;
}

FO_BEGIN_NAMESPACE

constexpr size_t AS_FUNC_ATTRIBUTES_USER_DATA = 1020;

struct ScriptFunctionAttributeUserData
{
    vector<string> Attributes {};
};

struct ParsedFunctionAttributeRecord
{
    string Namespace {};
    string ObjectType {};
    string Name {};
    uint32 OverloadIndex {};
    vector<string> Attributes {};
    string SourceFile {};
    uint32 SourceLine {};
};

void CleanupScriptFunctionAttributes(AngelScript::asIScriptFunction* func);
auto GetFunctionAttributesUserData(const AngelScript::asIScriptFunction* func) noexcept -> const ScriptFunctionAttributeUserData*;
auto FindFunctionAttribute(const AngelScript::asIScriptFunction* func, string_view attribute) noexcept -> string_view;
auto HasFunctionAttribute(const AngelScript::asIScriptFunction* func, string_view attribute) noexcept -> bool;
auto ParseFunctionAttributeRecords(Preprocessor::Context* pp_ctx, Preprocessor::LexemList& lexems, string& errors) -> vector<ParsedFunctionAttributeRecord>;
void SerializeFunctionAttributeRecords(DataWriter& writer, const vector<ParsedFunctionAttributeRecord>& records);
auto DeserializeFunctionAttributeRecords(DataReader& reader) -> vector<ParsedFunctionAttributeRecord>;
auto BindFunctionAttributeRecords(AngelScript::asIScriptModule* mod, const vector<ParsedFunctionAttributeRecord>& records) -> string;
auto ValidateAttributedFunctionUsage(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt = nullptr) -> string;
auto ValidateSpecialFunctionAttributes(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt = nullptr) -> string;
auto ValidateAdminRemoteCallAttributes(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt = nullptr) -> string;
auto ValidateEventSubscriptions(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt = nullptr) -> string;

FO_END_NAMESPACE

#endif
