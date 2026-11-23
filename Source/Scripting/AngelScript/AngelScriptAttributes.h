// FOnline Engine
// SPDX-License-Identifier: MIT

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

constexpr size_t AS_FUNC_ATTRIBUTES_USER_DATA {1020};

struct ScriptFunctionAttributeUserData
{
    vector<string> Attributes {};
};

struct ParsedFunctionAttributeRecord
{
    string Namespace {};
    string ObjectType {};
    string Name {};
    uint32_t OverloadIndex {};
    vector<string> Attributes {};
    string SourceFile {};
    uint32_t SourceLine {};
};

void CleanupScriptFunctionAttributes(AngelScript::asIScriptFunction* raw_func);
auto GetFunctionAttributesUserData(ptr<const AngelScript::asIScriptFunction> func) noexcept -> nptr<const ScriptFunctionAttributeUserData>;
auto FindFunctionAttribute(ptr<const AngelScript::asIScriptFunction> func, string_view attribute) noexcept -> string_view;
auto HasFunctionAttribute(ptr<const AngelScript::asIScriptFunction> func, string_view attribute) noexcept -> bool;
void SetFunctionAttributes(ptr<AngelScript::asIScriptFunction> func, const vector<string>& attributes);
auto ParseFunctionAttributeRecords(ptr<Preprocessor::Context> pp_ctx, Preprocessor::LexemList& lexems, string& errors) -> vector<ParsedFunctionAttributeRecord>;
void SerializeFunctionAttributeRecords(data_writer& writer, const vector<ParsedFunctionAttributeRecord>& records);
auto DeserializeFunctionAttributeRecords(data_reader& reader) -> vector<ParsedFunctionAttributeRecord>;
auto BindFunctionAttributeRecords(ptr<AngelScript::asIScriptModule> mod, const vector<ParsedFunctionAttributeRecord>& records, nptr<const vector<string>> project_blocking_extras = nullptr) -> string;
auto ValidateAttributedFunctionUsage(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt = nullptr, nptr<const vector<string>> allowed_namespaces = nullptr, nptr<const vector<string>> project_blocking_extras = nullptr) -> string;
auto ValidateSpecialFunctionAttributes(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt = nullptr) -> string;
auto ValidateAdminRemoteCallAttributes(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt = nullptr) -> string;
auto ValidateEventSubscriptions(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt = nullptr) -> string;

FO_END_NAMESPACE

#endif
