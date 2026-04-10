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

#include "AngelScriptAttributes.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include <angelscript.h>
#include <preprocessor.h>

FO_BEGIN_NAMESPACE

static constexpr AngelScript::asPWORD AS_PREPROCESSOR_LNT_USER_DATA = 5;

enum class ScriptSourceScopeKind
{
    Namespace,
    Type,
    Other,
};

using LexemIt = Preprocessor::LexemList::const_iterator;

struct ParsedNamespaceDecl
{
    string Name {};
    LexemIt OpenBrace;
};

struct ParsedTypeDecl
{
    string Name {};
    LexemIt OpenBrace;
};

struct ParsedFunctionDecl
{
    string Name {};
    LexemIt Terminator;
    bool HasBody {};
};

struct ParsedAttributeSeq
{
    vector<string> Attributes {};
    vector<LexemIt> TokensToStrip {};
    LexemIt AfterAttributes;
};

struct CallbackAttributeRule
{
    string_view AttributeName {};
    string_view UsageName {};
    string_view MethodName {};
    string_view ObjectTypeSuffix {};
};

struct ScriptBytecodeLocation
{
    string Section {};
    int Row {};
    int Column {1};
};

static constexpr array CALLBACK_ATTRIBUTE_RULES {
    CallbackAttributeRule {.AttributeName = "Event", .UsageName = "Subscribe/Unsubscribe", .MethodName = "Subscribe", .ObjectTypeSuffix = "Event"},
    CallbackAttributeRule {.AttributeName = "Event", .UsageName = "Subscribe/Unsubscribe", .MethodName = "Unsubscribe", .ObjectTypeSuffix = "Event"},
    CallbackAttributeRule {.AttributeName = "TimeEvent", .UsageName = "time-event APIs (StartTimeEvent, StopTimeEvent, CountTimeEvent, RepeatTimeEvent, SetTimeEventData)", .MethodName = "StartTimeEvent", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "TimeEvent", .UsageName = "time-event APIs (StartTimeEvent, StopTimeEvent, CountTimeEvent, RepeatTimeEvent, SetTimeEventData)", .MethodName = "StopTimeEvent", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "TimeEvent", .UsageName = "time-event APIs (StartTimeEvent, StopTimeEvent, CountTimeEvent, RepeatTimeEvent, SetTimeEventData)", .MethodName = "CountTimeEvent", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "TimeEvent", .UsageName = "time-event APIs (StartTimeEvent, StopTimeEvent, CountTimeEvent, RepeatTimeEvent, SetTimeEventData)", .MethodName = "RepeatTimeEvent", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "TimeEvent", .UsageName = "time-event APIs (StartTimeEvent, StopTimeEvent, CountTimeEvent, RepeatTimeEvent, SetTimeEventData)", .MethodName = "SetTimeEventData", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "AnimCallback", .UsageName = "AddAnimCallback", .MethodName = "AddAnimCallback", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "PropertyGetter", .UsageName = "property getter APIs (SetPropertyGetter)", .MethodName = "SetPropertyGetter", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "PropertySetter", .UsageName = "property setter APIs (AddPropertySetter, AddPropertyDeferredSetter)", .MethodName = "AddPropertySetter", .ObjectTypeSuffix = {}},
    CallbackAttributeRule {.AttributeName = "PropertySetter", .UsageName = "property setter APIs (AddPropertySetter, AddPropertyDeferredSetter)", .MethodName = "AddPropertyDeferredSetter", .ObjectTypeSuffix = {}},
};

static auto IsWhitespaceLexem(const Preprocessor::Lexem& lex) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return lex.Type == Preprocessor::WHITESPACE || lex.Type == Preprocessor::NEWLINE;
}

static auto IsLexem(const Preprocessor::Lexem& lex, Preprocessor::LexemType type, string_view value = {}) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return lex.Type == type && (value.empty() || string_view(lex.Value) == value);
}

static auto NextSignificantLexem(LexemIt it, const Preprocessor::LexemList& lexems) -> LexemIt
{
    FO_STACK_TRACE_ENTRY();

    while (it != lexems.end() && IsWhitespaceLexem(*it)) {
        ++it;
    }

    return it;
}

static auto CountNewlines(LexemIt begin, LexemIt end) -> uint32
{
    FO_STACK_TRACE_ENTRY();

    uint32 count = 0;

    for (auto it = begin; it != end; ++it) {
        const auto& lex = *it;

        if (lex.Type == Preprocessor::NEWLINE) {
            count += 1;
        }
        if (lex.Type == Preprocessor::COMMENT) {
            count += numeric_cast<uint32>(std::ranges::count(lex.Value, '\n'));
        }
    }

    return count;
}

static void AppendAttributeLexems(LexemIt begin, LexemIt end, string& text, vector<LexemIt>& tokens)
{
    FO_STACK_TRACE_ENTRY();

    for (auto it = begin; it != end; ++it) {
        if (IsWhitespaceLexem(*it)) {
            continue;
        }

        text.append(it->Value);
        tokens.emplace_back(it);
    }
}

static auto GetAttributeBaseName(string_view attribute) noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (const auto paren_pos = attribute.find('('); paren_pos != string_view::npos) {
        return attribute.substr(0, paren_pos);
    }

    return attribute;
}

static auto MakeNamespaceName(const vector<string>& namespace_stack) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;

    for (const auto& ns : namespace_stack) {
        if (!result.empty()) {
            result.append("::");
        }

        result.append(ns);
    }

    return result;
}

static auto MakeCurrentTypeName(const vector<string>& type_stack) -> string
{
    FO_STACK_TRACE_ENTRY();

    return type_stack.empty() ? string {} : type_stack.back();
}

static auto FormatAttributeError(const Preprocessor::LineNumberTranslator* lnt, uint32 line, string_view message) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (lnt == nullptr) {
        return strex("({},1): error : {}", line, message).str();
    }

    const auto& orig_file = Preprocessor::ResolveOriginalFile(line, lnt);
    const auto orig_line = Preprocessor::ResolveOriginalLine(line, lnt);
    return strex("{}({},1): error : {}", orig_file, orig_line, message).str();
}

static auto TryParseNamespaceDecl(LexemIt start, const Preprocessor::LexemList& lexems) -> optional<ParsedNamespaceDecl>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = NextSignificantLexem(start, lexems);

    if (it == lexems.end() || !IsLexem(*it, Preprocessor::IDENTIFIER, "namespace")) {
        return std::nullopt;
    }

    const auto name_it = NextSignificantLexem(std::next(it), lexems);

    if (name_it == lexems.end() || name_it->Type != Preprocessor::IDENTIFIER) {
        return std::nullopt;
    }

    const auto brace_it = NextSignificantLexem(std::next(name_it), lexems);

    if (brace_it == lexems.end() || !IsLexem(*brace_it, Preprocessor::OPEN, "{")) {
        return std::nullopt;
    }

    ParsedNamespaceDecl result;
    result.Name = name_it->Value;
    result.OpenBrace = brace_it;
    return result;
}

static auto TryParseTypeDecl(LexemIt start, const Preprocessor::LexemList& lexems) -> optional<ParsedTypeDecl>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = NextSignificantLexem(start, lexems);

    if (it == lexems.end() || it->Type != Preprocessor::IDENTIFIER) {
        return std::nullopt;
    }

    const auto keyword = string_view(it->Value);

    if (keyword != "class" && keyword != "interface" && keyword != "mixin") {
        return std::nullopt;
    }

    const auto name_it = NextSignificantLexem(std::next(it), lexems);

    if (name_it == lexems.end() || name_it->Type != Preprocessor::IDENTIFIER) {
        return std::nullopt;
    }

    auto brace_it = NextSignificantLexem(std::next(name_it), lexems);

    while (brace_it != lexems.end() && !IsLexem(*brace_it, Preprocessor::OPEN, "{")) {
        brace_it = NextSignificantLexem(std::next(brace_it), lexems);
    }

    if (brace_it == lexems.end()) {
        return std::nullopt;
    }

    ParsedTypeDecl result;
    result.Name = name_it->Value;
    result.OpenBrace = brace_it;
    return result;
}

static auto TryParseAttributeSequence(LexemIt start, const Preprocessor::LexemList& lexems) -> optional<ParsedAttributeSeq>
{
    FO_STACK_TRACE_ENTRY();

    auto it = NextSignificantLexem(start, lexems);

    if (it == lexems.end() || !IsLexem(*it, Preprocessor::OPEN, "[")) {
        return std::nullopt;
    }

    ParsedAttributeSeq result;

    while (it != lexems.end()) {
        if (!IsLexem(*it, Preprocessor::OPEN, "[")) {
            break;
        }

        auto open2_it = NextSignificantLexem(std::next(it), lexems);

        if (open2_it == lexems.end() || !IsLexem(*open2_it, Preprocessor::OPEN, "[")) {
            return std::nullopt;
        }

        auto name_it = NextSignificantLexem(std::next(open2_it), lexems);

        if (name_it == lexems.end()) {
            return std::nullopt;
        }

        if (name_it->Type != Preprocessor::IDENTIFIER) {
            return std::nullopt;
        }

        string attr_text;
        vector<LexemIt> attr_tokens;
        AppendAttributeLexems(name_it, std::next(name_it), attr_text, attr_tokens);

        auto close1_it = NextSignificantLexem(std::next(name_it), lexems);

        if (close1_it != lexems.end() && IsLexem(*close1_it, Preprocessor::OPEN, "(")) {
            int paren_depth = 0;
            auto args_end_it = close1_it;

            for (; args_end_it != lexems.end(); ++args_end_it) {
                if (IsLexem(*args_end_it, Preprocessor::OPEN, "(")) {
                    paren_depth++;
                }
                else if (IsLexem(*args_end_it, Preprocessor::CLOSE, ")")) {
                    paren_depth--;

                    if (paren_depth == 0) {
                        ++args_end_it;
                        break;
                    }
                }
            }

            if (paren_depth != 0) {
                return std::nullopt;
            }

            AppendAttributeLexems(close1_it, args_end_it, attr_text, attr_tokens);
            close1_it = NextSignificantLexem(args_end_it, lexems);
        }

        auto close2_it = close1_it != lexems.end() ? NextSignificantLexem(std::next(close1_it), lexems) : lexems.end();

        if (close1_it == lexems.end() || !IsLexem(*close1_it, Preprocessor::CLOSE, "]") || close2_it == lexems.end() || !IsLexem(*close2_it, Preprocessor::CLOSE, "]")) {
            return std::nullopt;
        }

        result.Attributes.emplace_back(std::move(attr_text));
        result.TokensToStrip.emplace_back(it);
        result.TokensToStrip.emplace_back(open2_it);
        result.TokensToStrip.insert(result.TokensToStrip.end(), attr_tokens.begin(), attr_tokens.end());
        result.TokensToStrip.emplace_back(close1_it);
        result.TokensToStrip.emplace_back(close2_it);
        it = NextSignificantLexem(std::next(close2_it), lexems);
    }

    if (result.Attributes.empty()) {
        return std::nullopt;
    }

    result.AfterAttributes = it;
    return result;
}

static auto TryParseFunctionDecl(LexemIt start, const Preprocessor::LexemList& lexems) -> optional<ParsedFunctionDecl>
{
    FO_STACK_TRACE_ENTRY();

    auto it = NextSignificantLexem(start, lexems);

    if (it == lexems.end()) {
        return std::nullopt;
    }

    if (it->Type == Preprocessor::IDENTIFIER) {
        const auto keyword = string_view(it->Value);

        if (keyword == "namespace" || keyword == "class" || keyword == "interface" || keyword == "enum" || keyword == "mixin" || keyword == "funcdef" || keyword == "typedef") {
            return std::nullopt;
        }
    }

    auto last_sig = lexems.end();
    bool seen_open_paren = false;
    int paren_depth = 0;
    string func_name;

    for (; it != lexems.end(); ++it) {
        if (IsWhitespaceLexem(*it)) {
            continue;
        }

        if (!seen_open_paren) {
            if (it->Type == Preprocessor::SEMICOLON || IsLexem(*it, Preprocessor::OPEN, "{") || IsLexem(*it, Preprocessor::CLOSE, "}") || string_view(it->Value) == "=") {
                return std::nullopt;
            }
        }

        if (IsLexem(*it, Preprocessor::OPEN, "(")) {
            if (!seen_open_paren) {
                if (last_sig == lexems.end() || last_sig->Type != Preprocessor::IDENTIFIER) {
                    return std::nullopt;
                }

                const auto name = string_view(last_sig->Value);

                if (name == "if" || name == "for" || name == "while" || name == "switch" || name == "catch") {
                    return std::nullopt;
                }

                func_name = last_sig->Value;
                seen_open_paren = true;
            }

            paren_depth++;
        }
        else if (IsLexem(*it, Preprocessor::CLOSE, ")")) {
            if (!seen_open_paren || paren_depth == 0) {
                return std::nullopt;
            }

            paren_depth--;
        }
        else if (seen_open_paren && paren_depth == 0) {
            if (IsLexem(*it, Preprocessor::OPEN, "{")) {
                return ParsedFunctionDecl {.Name = std::move(func_name), .Terminator = it, .HasBody = true};
            }
            if (it->Type == Preprocessor::SEMICOLON) {
                return ParsedFunctionDecl {.Name = std::move(func_name), .Terminator = it, .HasBody = false};
            }
            if (string_view(it->Value) == "=") {
                return std::nullopt;
            }
        }

        last_sig = it;
    }

    return std::nullopt;
}

static void SetFunctionAttributes(AngelScript::asIScriptFunction* func, const vector<string>& attributes)
{
    FO_STACK_TRACE_ENTRY();

    if (attributes.empty()) {
        return;
    }

    auto user_data = SafeAlloc::MakeUnique<ScriptFunctionAttributeUserData>();
    user_data->Attributes = attributes;

    const auto* old_user_data = cast_from_void<ScriptFunctionAttributeUserData*>(func->SetUserData(cast_to_void(user_data.release()), AS_FUNC_ATTRIBUTES_USER_DATA));
    FO_RUNTIME_ASSERT(!old_user_data);
}

static void SetFunctionAttributesWithVirtualMirror(AngelScript::asIScriptFunction* func, const vector<string>& attributes)
{
    FO_STACK_TRACE_ENTRY();

    if (func == nullptr) {
        return;
    }

    SetFunctionAttributes(func, attributes);

    const auto* ti = func->GetObjectType();

    if (ti == nullptr) {
        return;
    }

    for (AngelScript::asUINT i = 0; i < ti->GetMethodCount(); i++) {
        if (ti->GetMethodByIndex(i, false) != func) {
            continue;
        }

        if (auto* virtual_func = ti->GetMethodByIndex(i, true); virtual_func != nullptr && virtual_func != func) {
            SetFunctionAttributes(virtual_func, attributes);
        }

        return;
    }
}

static auto IsAttributedScriptFunction(const AngelScript::asIScriptFunction* func) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return func != nullptr && (func->GetFuncType() == AngelScript::asFUNC_SCRIPT || func->GetFuncType() == AngelScript::asFUNC_VIRTUAL);
}

static auto CollectModuleScriptFunctions(AngelScript::asIScriptModule* mod) -> vector<AngelScript::asIScriptFunction*>
{
    FO_STACK_TRACE_ENTRY();

    vector<AngelScript::asIScriptFunction*> funcs;

    for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        if (auto* func = mod->GetFunctionByIndex(i); IsAttributedScriptFunction(func)) {
            funcs.emplace_back(func);
        }
    }

    for (AngelScript::asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
        const auto* ti = mod->GetObjectTypeByIndex(i);

        if (ti == nullptr) {
            continue;
        }

        for (AngelScript::asUINT j = 0; j < ti->GetMethodCount(); j++) {
            if (auto* func = ti->GetMethodByIndex(j, false); IsAttributedScriptFunction(func)) {
                funcs.emplace_back(func);
            }
        }
    }

    return funcs;
}

static auto FindModuleObjectType(AngelScript::asIScriptModule* mod, string_view ns, string_view object_type_name) -> AngelScript::asITypeInfo*
{
    FO_STACK_TRACE_ENTRY();

    for (AngelScript::asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
        auto* ti = mod->GetObjectTypeByIndex(i);

        if (ti == nullptr) {
            continue;
        }

        const auto current_name = ti->GetName() != nullptr ? string_view(ti->GetName()) : string_view {};
        const auto current_ns = ti->GetNamespace() != nullptr ? string_view(ti->GetNamespace()) : string_view {};

        if (current_name == object_type_name && current_ns == ns) {
            return ti;
        }
    }

    return nullptr;
}

static auto ResolveDeclaredFunctionSourceLocation(const AngelScript::asIScriptFunction* func, const Preprocessor::LineNumberTranslator* lnt) -> optional<pair<string, uint32>>
{
    FO_STACK_TRACE_ENTRY();

    if (func == nullptr) {
        return std::nullopt;
    }

    int row = 0;
    int column = 1;
    const char* section = nullptr;

    if (func->GetDeclaredAt(&section, &row, &column) < 0 || row <= 0) {
        return std::nullopt;
    }

    if (lnt != nullptr) {
        const auto line = numeric_cast<uint32>(row);
        return pair {string {Preprocessor::ResolveOriginalFile(line, lnt)}, Preprocessor::ResolveOriginalLine(line, lnt)};
    }

    return pair {section != nullptr ? string {section} : string {}, numeric_cast<uint32>(row)};
}

static auto HasAttribute(const ScriptFunctionAttributeUserData* user_data, string_view attribute) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (user_data == nullptr) {
        return false;
    }

    for (const auto& attr : user_data->Attributes) {
        if (GetAttributeBaseName(attr) == attribute) {
            return true;
        }
    }

    return false;
}

static auto FindAttribute(const ScriptFunctionAttributeUserData* user_data, string_view attribute) noexcept -> const string*
{
    FO_STACK_TRACE_ENTRY();

    if (user_data == nullptr) {
        return nullptr;
    }

    for (const auto& attr : user_data->Attributes) {
        if (GetAttributeBaseName(attr) == attribute) {
            return &attr;
        }
    }

    return nullptr;
}

static auto GetFunctionDeclarationString(const AngelScript::asIScriptFunction* func) -> string
{
    FO_STACK_TRACE_ENTRY();

    return func != nullptr && func->GetDeclaration(true, true, false) != nullptr ? func->GetDeclaration(true, true, false) : "<unknown>";
}

static auto ResolveInstructionLocation(const AngelScript::asIScriptFunction* func, const AngelScript::asDWORD* instruction) -> optional<ScriptBytecodeLocation>
{
    FO_STACK_TRACE_ENTRY();

    const AngelScript::asDWORD* best_instruction = nullptr;
    ScriptBytecodeLocation best_location;
    const auto line_entry_count = numeric_cast<AngelScript::asUINT>(std::max(func->GetLineEntryCount(), 0));

    for (AngelScript::asUINT i = 0; i < line_entry_count; i++) {
        int row = 0;
        int column = 1;
        const char* section = nullptr;
        const AngelScript::asDWORD* line_instruction = nullptr;

        if (func->GetLineEntry(i, &row, &column, &section, &line_instruction) < 0 || row <= 0 || line_instruction == nullptr) {
            continue;
        }

        if (line_instruction <= instruction && (best_instruction == nullptr || line_instruction >= best_instruction)) {
            best_instruction = line_instruction;
            best_location = ScriptBytecodeLocation {
                .Section = section != nullptr ? section : "<unknown>",
                .Row = row,
                .Column = std::max(column, 1),
            };
        }
    }

    if (best_instruction != nullptr) {
        return best_location;
    }

    int row = 0;
    int column = 1;
    const char* section = nullptr;
    if (func->GetDeclaredAt(&section, &row, &column) >= 0 && row > 0) {
        return ScriptBytecodeLocation {
            .Section = section != nullptr ? section : "<unknown>",
            .Row = row,
            .Column = std::max(column, 1),
        };
    }

    return std::nullopt;
}

static auto FormatUsageErrorLocation(const ScriptBytecodeLocation& location, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (lnt != nullptr) {
        const auto line = numeric_cast<uint32>(location.Row);
        return strex("{}({},1)", Preprocessor::ResolveOriginalFile(line, lnt), Preprocessor::ResolveOriginalLine(line, lnt)).str();
    }

    return strex("{}({},{})", location.Section, location.Row, location.Column).str();
}

static auto MakeAttributedUsageError(const AngelScript::asIScriptFunction* caller, const AngelScript::asIScriptFunction* callee, const AngelScript::asDWORD* instruction, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto caller_decl = GetFunctionDeclarationString(caller);
    const auto callee_decl = GetFunctionDeclarationString(callee);
    auto message = strex("Attributed function '{}' cannot be called from function '{}'", callee_decl, caller_decl).str();

    if (const auto location = ResolveInstructionLocation(caller, instruction); location.has_value()) {
        return strex("{}: error : {}", FormatUsageErrorLocation(*location, lnt), message).str();
    }

    return message;
}

static auto ResolveInstructionFunction(const AngelScript::asDWORD* instruction, AngelScript::asIScriptEngine* engine) noexcept -> AngelScript::asIScriptFunction*
{
    FO_STACK_TRACE_ENTRY();

    const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8>(*instruction));

    switch (opcode) {
    case AngelScript::asBC_CALL:
    case AngelScript::asBC_CALLSYS:
    case AngelScript::asBC_Thiscall1:
    case AngelScript::asBC_CALLINTF:
        return engine->GetFunctionById(*reinterpret_cast<const int*>(instruction + 1));
    case AngelScript::asBC_ALLOC:
        return engine->GetFunctionById(*reinterpret_cast<const int*>(instruction + AS_PTR_SIZE + 1));
    case AngelScript::asBC_FuncPtr:
        return reinterpret_cast<AngelScript::asIScriptFunction*>(*reinterpret_cast<const AngelScript::asPWORD*>(instruction + 1));
    default:
        return nullptr;
    }
}

static void SerializeString(DataWriter& writer, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<uint32>(numeric_cast<uint32>(value.length()));
    writer.WritePtr(value.data(), value.length());
}

static auto DeserializeString(DataReader& reader) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto value_len = reader.Read<uint32>();
    string value;
    value.resize(value_len);

    if (value_len != 0) {
        reader.ReadPtr(value.data(), value_len);
    }

    return value;
}

void CleanupScriptFunctionAttributes(AngelScript::asIScriptFunction* func)
{
    FO_STACK_TRACE_ENTRY();

    const auto* user_data = cast_from_void<ScriptFunctionAttributeUserData*>(func->GetUserData(AS_FUNC_ATTRIBUTES_USER_DATA));
    delete user_data;
}

auto GetFunctionAttributesUserData(const AngelScript::asIScriptFunction* func) noexcept -> const ScriptFunctionAttributeUserData*
{
    FO_STACK_TRACE_ENTRY();

    return func != nullptr ? cast_from_void<ScriptFunctionAttributeUserData*>(func->GetUserData(AS_FUNC_ATTRIBUTES_USER_DATA)) : nullptr;
}

auto FindFunctionAttribute(const AngelScript::asIScriptFunction* func, string_view attribute) noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (const auto* raw_attr = FindAttribute(GetFunctionAttributesUserData(func), attribute); raw_attr != nullptr) {
        return *raw_attr;
    }

    return {};
}

auto HasFunctionAttribute(const AngelScript::asIScriptFunction* func, string_view attribute) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return HasAttribute(GetFunctionAttributesUserData(func), attribute);
}

auto ParseFunctionAttributeRecords(Preprocessor::Context* pp_ctx, Preprocessor::LexemList& lexems, string& errors) -> vector<ParsedFunctionAttributeRecord>
{
    FO_STACK_TRACE_ENTRY();

    vector<ParsedFunctionAttributeRecord> records;
    vector<LexemIt> tokens_to_strip;
    vector<ScriptSourceScopeKind> scope_stack;
    vector<string> namespace_stack;
    vector<string> type_stack;
    map<tuple<string, string, string>, uint32> overload_indexes;
    const auto* lnt = Preprocessor::GetLineNumberTranslator(pp_ctx);
    uint32 line = 1;

    for (LexemIt it = lexems.cbegin(); it != lexems.cend();) {
        if (it->Type == Preprocessor::NEWLINE) {
            line++;
            ++it;
            continue;
        }

        if (IsWhitespaceLexem(*it)) {
            ++it;
            continue;
        }

        const bool inside_other_scope = std::ranges::find(scope_stack, ScriptSourceScopeKind::Other) != scope_stack.end();

        if (!inside_other_scope) {
            if (auto attrs = TryParseAttributeSequence(it, lexems); attrs.has_value()) {
                const auto attr_line = line;

                if (auto func = TryParseFunctionDecl(attrs->AfterAttributes, lexems); func.has_value()) {
                    const auto ns = MakeNamespaceName(namespace_stack);
                    const auto ti = MakeCurrentTypeName(type_stack);
                    const auto ordinal = overload_indexes[{ns, ti, func->Name}]++;

                    records.emplace_back(ParsedFunctionAttributeRecord {
                        .Namespace = ns,
                        .ObjectType = ti,
                        .Name = func->Name,
                        .OverloadIndex = ordinal,
                        .Attributes = std::move(attrs->Attributes),
                        .SourceFile = lnt != nullptr ? string {Preprocessor::ResolveOriginalFile(attr_line, lnt)} : string {},
                        .SourceLine = lnt != nullptr ? Preprocessor::ResolveOriginalLine(attr_line, lnt) : attr_line,
                    });

                    tokens_to_strip.insert(tokens_to_strip.end(), attrs->TokensToStrip.begin(), attrs->TokensToStrip.end());
                    const auto next_it = std::next(func->Terminator);
                    line += CountNewlines(it, next_it);

                    if (func->HasBody) {
                        scope_stack.emplace_back(ScriptSourceScopeKind::Other);
                    }

                    it = next_it;
                    continue;
                }

                if (!errors.empty()) {
                    errors.append("\n");
                }

                errors.append(FormatAttributeError(lnt, attr_line, "Function attribute must be placed directly before a function declaration"));

                const auto next_it = attrs->AfterAttributes;
                line += CountNewlines(it, next_it);
                it = next_it;
                continue;
            }

            if (auto ns = TryParseNamespaceDecl(it, lexems); ns.has_value()) {
                const auto next_it = std::next(ns->OpenBrace);
                line += CountNewlines(it, next_it);
                scope_stack.emplace_back(ScriptSourceScopeKind::Namespace);
                namespace_stack.emplace_back(ns->Name);
                it = next_it;
                continue;
            }

            if (auto type = TryParseTypeDecl(it, lexems); type.has_value()) {
                const auto next_it = std::next(type->OpenBrace);
                line += CountNewlines(it, next_it);
                scope_stack.emplace_back(ScriptSourceScopeKind::Type);
                type_stack.emplace_back(type->Name);
                it = next_it;
                continue;
            }

            if (auto func = TryParseFunctionDecl(it, lexems); func.has_value()) {
                const auto ns = MakeNamespaceName(namespace_stack);
                const auto ti = MakeCurrentTypeName(type_stack);
                overload_indexes[{ns, ti, func->Name}]++;

                const auto next_it = std::next(func->Terminator);
                line += CountNewlines(it, next_it);

                if (func->HasBody) {
                    scope_stack.emplace_back(ScriptSourceScopeKind::Other);
                }

                it = next_it;
                continue;
            }
        }

        if (IsLexem(*it, Preprocessor::OPEN, "{")) {
            scope_stack.emplace_back(ScriptSourceScopeKind::Other);
        }
        else if (IsLexem(*it, Preprocessor::CLOSE, "}")) {
            if (!scope_stack.empty()) {
                const auto kind = scope_stack.back();
                scope_stack.pop_back();

                if (kind == ScriptSourceScopeKind::Namespace && !namespace_stack.empty()) {
                    namespace_stack.pop_back();
                }
                else if (kind == ScriptSourceScopeKind::Type && !type_stack.empty()) {
                    type_stack.pop_back();
                }
            }
        }

        ++it;
    }

    for (const auto& token_it : tokens_to_strip) {
        lexems.erase(token_it);
    }

    return records;
}

void SerializeFunctionAttributeRecords(DataWriter& writer, const vector<ParsedFunctionAttributeRecord>& records)
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<uint32>(numeric_cast<uint32>(records.size()));

    for (const auto& record : records) {
        SerializeString(writer, record.Namespace);
        SerializeString(writer, record.ObjectType);
        SerializeString(writer, record.Name);
        writer.Write<uint32>(record.OverloadIndex);
        writer.Write<uint32>(numeric_cast<uint32>(record.Attributes.size()));

        for (const auto& attr : record.Attributes) {
            SerializeString(writer, attr);
        }

        SerializeString(writer, record.SourceFile);
        writer.Write<uint32>(record.SourceLine);
    }
}

auto DeserializeFunctionAttributeRecords(DataReader& reader) -> vector<ParsedFunctionAttributeRecord>
{
    FO_STACK_TRACE_ENTRY();

    vector<ParsedFunctionAttributeRecord> records;
    const auto count = reader.Read<uint32>();
    records.reserve(count);

    for (uint32 i = 0; i < count; i++) {
        ParsedFunctionAttributeRecord record;
        record.Namespace = DeserializeString(reader);
        record.ObjectType = DeserializeString(reader);
        record.Name = DeserializeString(reader);
        record.OverloadIndex = reader.Read<uint32>();

        const auto attrs_count = reader.Read<uint32>();
        record.Attributes.reserve(attrs_count);

        for (uint32 j = 0; j < attrs_count; j++) {
            record.Attributes.emplace_back(DeserializeString(reader));
        }

        record.SourceFile = DeserializeString(reader);
        record.SourceLine = reader.Read<uint32>();
        records.emplace_back(std::move(record));
    }

    return records;
}

static auto FormatRecordFunctionName(const ParsedFunctionAttributeRecord& record) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;

    if (!record.Namespace.empty()) {
        result.append(record.Namespace);
        result.append("::");
    }
    if (!record.ObjectType.empty()) {
        result.append(record.ObjectType);
        result.append("::");
    }

    result.append(record.Name);
    return result;
}

auto BindFunctionAttributeRecords(AngelScript::asIScriptModule* mod, const vector<ParsedFunctionAttributeRecord>& records) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (records.empty()) {
        return {};
    }

    const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(mod->GetEngine()->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));

    for (const auto& record : records) {
        AngelScript::asIScriptFunction* target_func = nullptr;

        if (record.ObjectType.empty()) {
            uint32 ordinal = 0;

            for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
                auto* func = mod->GetFunctionByIndex(i);
                if (!IsAttributedScriptFunction(func) || func->GetObjectType() != nullptr || func->GetName() == nullptr || string_view(func->GetName()) != record.Name) {
                    continue;
                }

                const auto ns = func->GetNamespace() != nullptr ? string_view(func->GetNamespace()) : string_view {};

                if (ns != record.Namespace) {
                    continue;
                }

                if (ordinal++ == record.OverloadIndex) {
                    target_func = func;
                    break;
                }
            }
        }
        else if (const auto* ti = FindModuleObjectType(mod, record.Namespace, record.ObjectType); ti != nullptr) {
            uint32 ordinal = 0;

            for (AngelScript::asUINT j = 0; j < ti->GetMethodCount(); j++) {
                auto* func = ti->GetMethodByIndex(j, false);

                if (!IsAttributedScriptFunction(func) || func->GetName() == nullptr || string_view(func->GetName()) != record.Name) {
                    continue;
                }

                if (ordinal++ == record.OverloadIndex) {
                    target_func = func;
                    break;
                }
            }
        }

        if (target_func == nullptr && !record.SourceFile.empty()) {
            AngelScript::asIScriptFunction* fallback_func = nullptr;

            for (auto* func : CollectModuleScriptFunctions(mod)) {
                if (func->GetName() == nullptr || string_view(func->GetName()) != record.Name) {
                    continue;
                }

                const auto location = ResolveDeclaredFunctionSourceLocation(func, lnt);

                if (!location.has_value() || location->first != record.SourceFile || location->second != record.SourceLine) {
                    continue;
                }

                if (fallback_func != nullptr && fallback_func != func) {
                    fallback_func = nullptr;
                    break;
                }

                fallback_func = func;
            }

            target_func = fallback_func;
        }

        if (target_func == nullptr) {
            if (!record.SourceFile.empty()) {
                return strex("{}({},1): error : Can't bind attributes to function {} [overload {}]", record.SourceFile, record.SourceLine, FormatRecordFunctionName(record), record.OverloadIndex).str();
            }

            return strex("Can't bind attributes to function {} [overload {}]", FormatRecordFunctionName(record), record.OverloadIndex).str();
        }

        SetFunctionAttributesWithVirtualMirror(target_func, record.Attributes);
    }

    return {};
}

auto ValidateAttributedFunctionUsage(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    map<int, const AngelScript::asIScriptFunction*> attributed_funcs_by_id;
    const auto funcs = CollectModuleScriptFunctions(mod);

    for (const auto* func : funcs) {
        if (GetFunctionAttributesUserData(func) == nullptr || HasFunctionAttribute(func, "ServerRemoteCall") || HasFunctionAttribute(func, "ClientRemoteCall") || HasFunctionAttribute(func, "AdminRemoteCall") || HasFunctionAttribute(func, "ItemTrigger") || HasFunctionAttribute(func, "ItemStatic")) {
            continue;
        }

        attributed_funcs_by_id.emplace(func->GetId(), func);
    }

    if (attributed_funcs_by_id.empty()) {
        return {};
    }

    string errors;

    for (auto* caller : funcs) {
        AngelScript::asUINT bc_length = 0;
        const auto* bc = caller->GetByteCode(&bc_length);

        if (bc == nullptr || bc_length == 0) {
            continue;
        }

        for (AngelScript::asUINT pos = 0; pos < bc_length;) {
            const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8>(bc[pos]));
            const auto instr_size = AngelScript::asBCTypeSize[AngelScript::asBCInfo[opcode].type];
            const auto* instruction = bc + pos;

            if (opcode == AngelScript::asBC_CALL || opcode == AngelScript::asBC_CALLSYS || opcode == AngelScript::asBC_CALLINTF || opcode == AngelScript::asBC_Thiscall1) {
                const auto* target = mod->GetEngine()->GetFunctionById(*reinterpret_cast<const int*>(instruction + 1));
                FO_RUNTIME_ASSERT(target);

                if (const auto it = attributed_funcs_by_id.find(target->GetId()); it != attributed_funcs_by_id.end()) {
                    if (!errors.empty()) {
                        errors.append("\n");
                    }

                    errors.append(MakeAttributedUsageError(caller, it->second, instruction, lnt));
                }
            }

            pos += instr_size;
        }
    }

    return errors;
}

static auto TryParseAttributePriority(string_view raw_attribute, string_view attribute_name, int32& priority) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (GetAttributeBaseName(raw_attribute) != attribute_name) {
        return false;
    }

    priority = 0;

    if (raw_attribute.length() == attribute_name.length()) {
        return true;
    }
    if (raw_attribute.length() <= attribute_name.length() + 2 || raw_attribute[attribute_name.length()] != '(' || raw_attribute.back() != ')') {
        return false;
    }

    const auto args = raw_attribute.substr(attribute_name.length() + 1, raw_attribute.length() - attribute_name.length() - 2);

    if (args.empty() || args.find(',') != string_view::npos) {
        return false;
    }

    auto parsed_priority = int32 {};
    const auto* begin = args.data();
    const auto* end = begin + args.length();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_priority);

    if (ec != std::errc {} || ptr != end) {
        return false;
    }

    priority = parsed_priority;
    return true;
}

static auto MakeSpecialAttributeError(const AngelScript::asIScriptFunction* func, string_view raw_attribute, string_view details, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto func_decl = GetFunctionDeclarationString(func);
    auto message = strex("Invalid attribute '[[{}]]' on function '{}': {}", raw_attribute, func_decl, details).str();

    if (const auto location = ResolveDeclaredFunctionSourceLocation(func, lnt); location.has_value()) {
        return strex("{}({},1): error : {}", location->first, location->second, message).str();
    }

    return message;
}

static void AppendSpecialAttributeError(string& errors, const AngelScript::asIScriptFunction* func, string_view raw_attribute, string_view details, const Preprocessor::LineNumberTranslator* lnt)
{
    FO_STACK_TRACE_ENTRY();

    if (!errors.empty()) {
        errors.append("\n");
    }

    errors.append(MakeSpecialAttributeError(func, raw_attribute, details, lnt));
}

static void ValidateSpecialAttribute(string& errors, const AngelScript::asIScriptFunction* func, string_view raw_attribute, string_view attribute_name, const Preprocessor::LineNumberTranslator* lnt)
{
    FO_STACK_TRACE_ENTRY();

    if (raw_attribute.empty()) {
        return;
    }

    int32 priority {};

    if (!TryParseAttributePriority(raw_attribute, attribute_name, priority)) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "expected an optional single integer priority argument", lnt);
        return;
    }

    if (func->GetObjectType() != nullptr) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "only global functions can use this attribute", lnt);
    }
    if (func->GetParamCount() != 0) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "function must not have parameters", lnt);
    }
    if (func->GetReturnTypeId() != AngelScript::asTYPEID_VOID) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "function must return void", lnt);
    }
}

auto ValidateSpecialFunctionAttributes(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    string errors;

    for (const auto* func : CollectModuleScriptFunctions(mod)) {
        ValidateSpecialAttribute(errors, func, FindFunctionAttribute(func, "ModuleInit"), "ModuleInit", lnt);
    }

    return errors;
}

static auto IsScriptTypeNamed(AngelScript::asIScriptEngine* engine, int32 type_id, string_view type_name) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto* type_info = engine->GetTypeInfoById(type_id);
    return type_info != nullptr && string_view(type_info->GetName()) == type_name;
}

static auto IsSupportedAdminRemoteCallArgType(AngelScript::asIScriptEngine* engine, int32 type_id, bool expect_any) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return expect_any ? IsScriptTypeNamed(engine, type_id, "any") : type_id == AngelScript::asTYPEID_INT32;
}

static auto IsSupportedAdminRemoteCallSignature(const AngelScript::asIScriptFunction* func) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = func->GetEngine();

    if (func->GetObjectType() != nullptr || func->GetReturnTypeId() != AngelScript::asTYPEID_VOID) {
        return false;
    }

    const auto param_count = func->GetParamCount();
    AngelScript::asUINT first_payload_index = 0;

    if (param_count > 0) {
        int32 first_param_type_id = 0;
        AngelScript::asDWORD first_param_flags = 0;
        const char* first_param_name = nullptr;

        if (func->GetParam(0, &first_param_type_id, &first_param_flags, &first_param_name) < 0) {
            return false;
        }

        ignore_unused(first_param_flags);
        ignore_unused(first_param_name);

        if (IsScriptTypeNamed(engine, first_param_type_id, "Player") || IsScriptTypeNamed(engine, first_param_type_id, "Critter")) {
            first_payload_index = 1;
        }
    }

    const auto payload_count = param_count - first_payload_index;

    if (payload_count > 3) {
        return false;
    }
    if (payload_count == 0) {
        return true;
    }

    int32 first_payload_type_id = 0;
    AngelScript::asDWORD first_payload_flags = 0;
    const char* first_payload_name = nullptr;

    if (func->GetParam(first_payload_index, &first_payload_type_id, &first_payload_flags, &first_payload_name) < 0) {
        return false;
    }

    ignore_unused(first_payload_flags);
    ignore_unused(first_payload_name);

    const bool expect_any = IsScriptTypeNamed(engine, first_payload_type_id, "any");

    if (!expect_any && first_payload_type_id != AngelScript::asTYPEID_INT32) {
        return false;
    }

    for (AngelScript::asUINT index = first_payload_index + 1; index < param_count; index++) {
        int32 param_type_id = 0;
        AngelScript::asDWORD param_flags = 0;
        const char* param_name = nullptr;

        if (func->GetParam(index, &param_type_id, &param_flags, &param_name) < 0) {
            return false;
        }

        ignore_unused(param_flags);
        ignore_unused(param_name);

        if (!IsSupportedAdminRemoteCallArgType(engine, param_type_id, expect_any)) {
            return false;
        }
    }

    return true;
}

auto ValidateAdminRemoteCallAttributes(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    string errors;

    for (const auto* func : CollectModuleScriptFunctions(mod)) {
        const auto raw_attribute = FindFunctionAttribute(func, "AdminRemoteCall");

        if (raw_attribute.empty()) {
            continue;
        }

        if (!IsSupportedAdminRemoteCallSignature(func)) {
            AppendSpecialAttributeError(errors, func, raw_attribute, "function must be a global void ~run entrypoint with one of the supported signatures: (), (int[, int[, int]]), (any[, any[, any]]), (Player[, int...|any...]), or (Critter[, int...|any...])", lnt);
        }
    }

    return errors;
}

static auto IsSameSourceLine(const optional<ScriptBytecodeLocation>& left, const optional<ScriptBytecodeLocation>& right) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return left.has_value() && right.has_value() && left->Row == right->Row && left->Section == right->Section;
}

static auto MatchesCallbackAttributeRule(const AngelScript::asIScriptFunction* func, const CallbackAttributeRule& rule) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (func == nullptr || (func->GetFuncType() != AngelScript::asFUNC_SYSTEM && func->GetFuncType() != AngelScript::asFUNC_IMPORTED) || func->GetName() == nullptr || string_view(func->GetName()) != rule.MethodName) {
        return false;
    }
    if (rule.ObjectTypeSuffix.empty()) {
        return true;
    }

    return func->GetObjectType() != nullptr && func->GetObjectName() != nullptr && string_view(func->GetObjectName()).ends_with(rule.ObjectTypeSuffix);
}

static auto FindCallbackAttributeRuleByUsage(const AngelScript::asIScriptFunction* func) noexcept -> const CallbackAttributeRule*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = std::ranges::find_if(CALLBACK_ATTRIBUTE_RULES, [func](const auto& rule) { return MatchesCallbackAttributeRule(func, rule); });
    return it != CALLBACK_ATTRIBUTE_RULES.end() ? &*it : nullptr;
}

static auto FindCallbackAttributeRuleByAttribute(const AngelScript::asIScriptFunction* func) noexcept -> const CallbackAttributeRule*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = std::ranges::find_if(CALLBACK_ATTRIBUTE_RULES, [func](const auto& rule) { return HasFunctionAttribute(func, rule.AttributeName); });
    return it != CALLBACK_ATTRIBUTE_RULES.end() ? &*it : nullptr;
}

static auto IsFunctionCallInstruction(AngelScript::asEBCInstr opcode) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    switch (opcode) {
    case AngelScript::asBC_CALL:
    case AngelScript::asBC_CALLSYS:
    case AngelScript::asBC_Thiscall1:
    case AngelScript::asBC_CALLINTF:
    case AngelScript::asBC_CALLBND:
    case AngelScript::asBC_CallPtr:
        return true;
    default:
        return false;
    }
}

static auto IsDelegateFactoryFunction(const AngelScript::asIScriptFunction* func) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return func != nullptr && func->GetFuncType() == AngelScript::asFUNC_SYSTEM && func->GetName() != nullptr && string_view(func->GetName()) == "$dlgte";
}

static auto MakeRestrictedCallbackUsageError(const AngelScript::asIScriptFunction* caller, const AngelScript::asIScriptFunction* callback, const optional<ScriptBytecodeLocation>& location, const CallbackAttributeRule& rule, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto callback_decl = GetFunctionDeclarationString(callback);
    const auto caller_decl = GetFunctionDeclarationString(caller);
    auto message = strex("Functions marked [[{}]] can only be passed to {}, '{}' is used outside of {} in script function '{}'", rule.AttributeName, rule.UsageName, callback_decl, rule.UsageName, caller_decl).str();

    if (location.has_value()) {
        return strex("{}: error : {}", FormatUsageErrorLocation(*location, lnt), message).str();
    }

    return message;
}

static auto MakeMissingCallbackAttributeError(const AngelScript::asIScriptFunction* caller, const AngelScript::asIScriptFunction* callback, const optional<ScriptBytecodeLocation>& location, const CallbackAttributeRule& rule, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto callback_decl = GetFunctionDeclarationString(callback);
    const auto caller_decl = GetFunctionDeclarationString(caller);
    auto message = strex("Functions passed to {} must be marked [[{}]], '{}' is used without [[{}]] in script function '{}'", rule.UsageName, rule.AttributeName, callback_decl, rule.AttributeName, caller_decl).str();

    if (location.has_value()) {
        return strex("{}: error : {}", FormatUsageErrorLocation(*location, lnt), message).str();
    }

    return message;
}

auto ValidateEventSubscriptions(AngelScript::asIScriptModule* mod, const Preprocessor::LineNumberTranslator* lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    string errors;
    const auto funcs = CollectModuleScriptFunctions(mod);

    for (auto* caller : funcs) {
        AngelScript::asUINT bc_length = 0;
        const auto* bc = caller->GetByteCode(&bc_length);

        if (bc == nullptr || bc_length == 0) {
            continue;
        }

        struct PendingCallbackTarget
        {
            const AngelScript::asIScriptFunction* Target {};
            optional<ScriptBytecodeLocation> Location {};
            bool WrappedInDelegate {};
        };

        auto pending = PendingCallbackTarget {};

        const auto append_error = [&](string error) {
            if (!errors.empty()) {
                errors.append("\n");
            }

            errors.append(error);
        };

        const auto report_restricted_usage = [&]() {
            if (pending.Target == nullptr) {
                return;
            }

            if (const auto* restricted_rule = FindCallbackAttributeRuleByAttribute(pending.Target); restricted_rule != nullptr) {
                append_error(MakeRestrictedCallbackUsageError(caller, pending.Target, pending.Location, *restricted_rule, lnt));
            }
        };

        const auto clear_pending = [&]() { pending = {}; };

        for (AngelScript::asUINT pos = 0; pos < bc_length;) {
            const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8>(bc[pos]));
            const auto instr_size = AngelScript::asBCTypeSize[AngelScript::asBCInfo[opcode].type];
            const auto* instruction = bc + pos;

            if (pending.Target != nullptr) {
                const auto current_location = ResolveInstructionLocation(caller, instruction);

                if (pending.Location.has_value() && current_location.has_value() && !IsSameSourceLine(pending.Location, current_location)) {
                    report_restricted_usage();
                    clear_pending();
                }
            }

            if (opcode == AngelScript::asBC_FuncPtr) {
                report_restricted_usage();
                pending.Target = ResolveInstructionFunction(instruction, mod->GetEngine());
                pending.Location = ResolveInstructionLocation(caller, instruction);
                pending.WrappedInDelegate = false;
            }
            else if (IsFunctionCallInstruction(opcode)) {
                const auto* target = ResolveInstructionFunction(instruction, mod->GetEngine());

                if (pending.Target != nullptr && IsDelegateFactoryFunction(target)) {
                    pending.WrappedInDelegate = true;
                }

                if (pending.Target != nullptr) {
                    const auto call_location = ResolveInstructionLocation(caller, instruction);
                    const auto same_line = pending.WrappedInDelegate || (!pending.Location.has_value() || !call_location.has_value()) ? true : IsSameSourceLine(pending.Location, call_location);
                    const auto* usage_rule = FindCallbackAttributeRuleByUsage(target);

                    if (usage_rule != nullptr && same_line) {
                        if (usage_rule->AttributeName == "Event") {
                            if (const auto* restricted_rule = FindCallbackAttributeRuleByAttribute(pending.Target); restricted_rule != nullptr) {
                                if (restricted_rule->AttributeName == usage_rule->AttributeName) {
                                    clear_pending();
                                }
                                else {
                                    append_error(MakeRestrictedCallbackUsageError(caller, pending.Target, pending.Location, *restricted_rule, lnt));
                                    clear_pending();
                                }
                            }
                            else {
                                append_error(MakeMissingCallbackAttributeError(caller, pending.Target, pending.Location, *usage_rule, lnt));
                                clear_pending();
                            }
                        }
                        else {
                            if (const auto* restricted_rule = FindCallbackAttributeRuleByAttribute(pending.Target); restricted_rule != nullptr) {
                                if (restricted_rule->AttributeName == usage_rule->AttributeName) {
                                    clear_pending();
                                }
                                else {
                                    append_error(MakeRestrictedCallbackUsageError(caller, pending.Target, pending.Location, *restricted_rule, lnt));
                                    clear_pending();
                                }
                            }
                            else {
                                append_error(MakeMissingCallbackAttributeError(caller, pending.Target, pending.Location, *usage_rule, lnt));
                                clear_pending();
                            }
                        }
                    }
                }
            }

            pos += instr_size;
        }

        report_restricted_usage();
    }

    return errors;
}

FO_END_NAMESPACE

#endif
