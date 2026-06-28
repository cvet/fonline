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

#include "AngelScriptHelpers.h"
#include "Application.h"

#include <angelscript.h>
#include <preprocessor.h>

FO_BEGIN_NAMESPACE

static constexpr AngelScript::asPWORD AS_PREPROCESSOR_LNT_USER_DATA = 5;

static auto IsSameScriptFunction(nptr<AngelScript::asIScriptFunction> lhs, ptr<AngelScript::asIScriptFunction> rhs) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!lhs) {
        return false;
    }

    return lhs.as_ptr() == rhs;
}

static auto IsInstructionAtOrBefore(nptr<const AngelScript::asDWORD> lhs, ptr<const AngelScript::asDWORD> rhs) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!lhs) {
        return false;
    }

    auto lhs_ptr = lhs.as_ptr();
    return lhs_ptr == rhs || lhs_ptr < rhs;
}

static auto IsInstructionAtOrAfter(nptr<const AngelScript::asDWORD> lhs, ptr<const AngelScript::asDWORD> rhs) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!lhs) {
        return false;
    }

    auto lhs_ptr = lhs.as_ptr();
    return lhs_ptr == rhs || rhs < lhs_ptr;
}

static auto InstructionWordAt(ptr<const AngelScript::asDWORD> instruction, size_t word_offset) noexcept -> ptr<const AngelScript::asDWORD>
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<const AngelScript::asDWORD> instruction_word = instruction.get() + word_offset;
    return instruction_word;
}

static auto ByteCodeSpan(ptr<const AngelScript::asDWORD> bytecode, size_t length) noexcept -> const_span<AngelScript::asDWORD>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(length != 0, "Bytecode span length must not be zero");

    return {bytecode.get(), length};
}

static auto ByteCodeInstructionAt(const_span<AngelScript::asDWORD> bytecode, size_t pos) noexcept -> ptr<const AngelScript::asDWORD>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(pos < bytecode.size(), "Bytecode instruction position is out of bounds");

    return &bytecode[pos];
}

template<typename T>
static auto ReadInstructionValue(ptr<const AngelScript::asDWORD> instruction, size_t word_offset) noexcept -> T
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    T value {};
    ptr<T> value_ptr = &value;
    auto instruction_word = InstructionWordAt(instruction, word_offset);
    MemCopy(value_ptr.get(), instruction_word.get(), sizeof(value));
    return value;
}

static auto ReadInstructionFunctionId(ptr<const AngelScript::asDWORD> instruction, size_t word_offset) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return ReadInstructionValue<int32_t>(instruction, word_offset);
}

static auto ReadInstructionPointer(ptr<const AngelScript::asDWORD> instruction, size_t word_offset) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    static_assert(sizeof(AngelScript::asPWORD) == sizeof(void*));

    const AngelScript::asPWORD address = ReadInstructionValue<AngelScript::asPWORD>(instruction, word_offset);
    return std::bit_cast<void*>(address);
}

static auto ReadInstructionFunctionPtr(ptr<const AngelScript::asDWORD> instruction, size_t word_offset) noexcept -> nptr<AngelScript::asIScriptFunction>
{
    FO_STACK_TRACE_ENTRY();

    return cast_from_void<AngelScript::asIScriptFunction*>(ReadInstructionPointer(instruction, word_offset).get());
}

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

static constexpr array DIRECT_CALL_BLOCKING_ATTRIBUTES {
    string_view {"Event"},
    string_view {"TimeEvent"},
    string_view {"AnimCallback"},
    string_view {"PropertyGetter"},
    string_view {"PropertySetter"},
    string_view {"ServerRemoteCall"},
    string_view {"ClientRemoteCall"},
    string_view {"AdminRemoteCall"},
    string_view {"ItemTrigger"},
    string_view {"ItemStatic"},
    string_view {"ModuleInit"},
};

static auto IsDirectCallBlockingAttribute(string_view base_name, nptr<const vector<string>> project_extras) noexcept -> bool
{
    for (const auto& name : DIRECT_CALL_BLOCKING_ATTRIBUTES) {
        if (base_name == name) {
            return true;
        }
    }

    if (project_extras) {
        for (const auto& name : *project_extras) {
            if (base_name == name) {
                return true;
            }
        }
    }

    return false;
}

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
    CallbackAttributeRule {.AttributeName = "PropertySetter", .UsageName = "property setter API (AddPropertySetter)", .MethodName = "AddPropertySetter", .ObjectTypeSuffix = {}},
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

static auto CountNewlines(LexemIt begin, LexemIt end) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    uint32_t count = 0;

    for (auto it = begin; it != end; ++it) {
        ptr<const Preprocessor::Lexem> lex = &*it;

        if (lex->Type == Preprocessor::NEWLINE) {
            count += 1;
        }
        if (lex->Type == Preprocessor::COMMENT) {
            count += numeric_cast<uint32_t>(std::ranges::count(lex->Value, '\n'));
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

static auto FormatAttributeError(nptr<const Preprocessor::LineNumberTranslator> lnt, uint32_t line, string_view message) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!lnt) {
        return strex("({},1): error : {}", line, message).str();
    }

    const string_view orig_file = Preprocessor::ResolveOriginalFile(line, lnt.get());
    const auto orig_line = Preprocessor::ResolveOriginalLine(line, lnt.get());
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

void SetFunctionAttributes(ptr<AngelScript::asIScriptFunction> func, const vector<string>& attributes)
{
    FO_STACK_TRACE_ENTRY();

    if (attributes.empty()) {
        return;
    }

    if (nptr<ScriptFunctionAttributeUserData> nullable_old_user_data = cast_from_void<ScriptFunctionAttributeUserData*>(func->GetUserData(AS_FUNC_ATTRIBUTES_USER_DATA)); nullable_old_user_data) {
        auto old_user_data = nullable_old_user_data.as_ptr();
        FO_VERIFY_AND_THROW(old_user_data->Attributes == attributes, "AngelScript function attributes were registered twice with different data");
        return;
    }

    auto user_data = SafeAlloc::MakeUnique<ScriptFunctionAttributeUserData>();
    user_data->Attributes = attributes;

    ptr<ScriptFunctionAttributeUserData> released_user_data = std::move(user_data).release();
    ptr<void> user_data_slot = cast_to_void(released_user_data.get());
    nptr<const ScriptFunctionAttributeUserData> old_user_data = cast_from_void<ScriptFunctionAttributeUserData*>(func->SetUserData(user_data_slot.get(), AS_FUNC_ATTRIBUTES_USER_DATA));
    FO_VERIFY_AND_THROW(!old_user_data, "Old user data is already set");
}

static void SetFunctionAttributesWithVirtualMirror(nptr<AngelScript::asIScriptFunction> nullable_func, const vector<string>& attributes, nptr<const vector<string>> project_blocking_extras)
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_func) {
        return;
    }

    auto func = nullable_func.as_ptr();
    SetFunctionAttributes(func, attributes);

    nptr<AngelScript::asITypeInfo> nullable_ti = func->GetObjectType();

    if (!nullable_ti) {
        return;
    }

    auto ti = nullable_ti.as_ptr();

    vector<string> blocking_only;
    blocking_only.reserve(attributes.size());

    for (const auto& attr : attributes) {
        if (IsDirectCallBlockingAttribute(GetAttributeBaseName(attr), project_blocking_extras)) {
            blocking_only.emplace_back(attr);
        }
    }

    if (blocking_only.empty()) {
        return;
    }

    for (AngelScript::asUINT i = 0; i < ti->GetMethodCount(); i++) {
        nptr<AngelScript::asIScriptFunction> method_func = ti->GetMethodByIndex(i, false);

        if (!IsSameScriptFunction(method_func, func)) {
            continue;
        }

        nptr<AngelScript::asIScriptFunction> nullable_virtual_func = ti->GetMethodByIndex(i, true);

        if (nullable_virtual_func && !IsSameScriptFunction(nullable_virtual_func, func)) {
            auto virtual_func = nullable_virtual_func.as_ptr();
            SetFunctionAttributes(virtual_func, blocking_only);
        }

        return;
    }
}

static auto IsAttributedScriptFunction(nptr<const AngelScript::asIScriptFunction> nullable_func) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_func) {
        return false;
    }

    auto func = nullable_func.as_ptr();
    return func->GetFuncType() == AngelScript::asFUNC_SCRIPT || func->GetFuncType() == AngelScript::asFUNC_VIRTUAL;
}

static auto CollectModuleScriptFunctions(ptr<AngelScript::asIScriptModule> mod) -> vector<ptr<AngelScript::asIScriptFunction>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ptr<AngelScript::asIScriptFunction>> funcs;

    for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        nptr<AngelScript::asIScriptFunction> nullable_func = mod->GetFunctionByIndex(i);

        if (IsAttributedScriptFunction(nullable_func)) {
            auto func = nullable_func.as_ptr();
            funcs.emplace_back(func);
        }
    }

    for (AngelScript::asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
        nptr<AngelScript::asITypeInfo> nullable_ti = mod->GetObjectTypeByIndex(i);

        if (!nullable_ti) {
            continue;
        }

        auto ti = nullable_ti.as_ptr();

        for (AngelScript::asUINT j = 0; j < ti->GetMethodCount(); j++) {
            nptr<AngelScript::asIScriptFunction> nullable_func = ti->GetMethodByIndex(j, false);

            if (IsAttributedScriptFunction(nullable_func)) {
                auto func = nullable_func.as_ptr();
                funcs.emplace_back(func);
            }
        }
    }

    return funcs;
}

static auto FindModuleObjectType(ptr<AngelScript::asIScriptModule> mod, string_view ns, string_view object_type_name) -> nptr<AngelScript::asITypeInfo>
{
    FO_STACK_TRACE_ENTRY();

    for (AngelScript::asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
        nptr<AngelScript::asITypeInfo> nullable_ti = mod->GetObjectTypeByIndex(i);

        if (!nullable_ti) {
            continue;
        }

        auto ti = nullable_ti.as_ptr();

        const nptr<const char> current_name_ptr = ti->GetName();
        const nptr<const char> current_ns_ptr = ti->GetNamespace();
        const auto current_name = current_name_ptr ? string_view {current_name_ptr.get()} : string_view {};
        const auto current_ns = current_ns_ptr ? string_view {current_ns_ptr.get()} : string_view {};

        if (current_name == object_type_name && current_ns == ns) {
            return ti;
        }
    }

    return nullptr;
}

static auto ResolveDeclaredFunctionSourceLocation(nptr<const AngelScript::asIScriptFunction> nullable_func, nptr<const Preprocessor::LineNumberTranslator> lnt) -> optional<pair<string, uint32_t>>
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_func) {
        return std::nullopt;
    }

    auto func = nullable_func.as_ptr();

    int row = 0;
    int column = 1;
    nptr<const char> section;

    if (func->GetDeclaredAt(section.get_pp(), &row, &column) < 0 || row <= 0) {
        return std::nullopt;
    }

    if (lnt) {
        const auto line = numeric_cast<uint32_t>(row);
        return pair {string {Preprocessor::ResolveOriginalFile(line, lnt.get())}, Preprocessor::ResolveOriginalLine(line, lnt.get())};
    }

    return pair {section ? string {section.get()} : string {}, numeric_cast<uint32_t>(row)};
}

static auto HasAttribute(nptr<const ScriptFunctionAttributeUserData> nullable_user_data, string_view attribute) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_user_data) {
        return false;
    }

    auto user_data = nullable_user_data.as_ptr();

    for (const auto& attr : user_data->Attributes) {
        if (GetAttributeBaseName(attr) == attribute) {
            return true;
        }
    }

    return false;
}

static auto FindAttribute(nptr<const ScriptFunctionAttributeUserData> nullable_user_data, string_view attribute) noexcept -> nptr<const string>
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_user_data) {
        return nullptr;
    }

    auto user_data = nullable_user_data.as_ptr();

    for (const auto& attr : user_data->Attributes) {
        if (GetAttributeBaseName(attr) == attribute) {
            return &attr;
        }
    }

    return nullptr;
}

static auto GetFunctionDeclarationString(nptr<const AngelScript::asIScriptFunction> nullable_func) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_func) {
        return "<unknown>";
    }

    auto func = nullable_func.as_ptr();
    const nptr<const char> declaration = func->GetDeclaration(true, true, false);
    return declaration ? declaration.get() : "<unknown>";
}

static auto ResolveInstructionLocation(ptr<const AngelScript::asIScriptFunction> func, ptr<const AngelScript::asDWORD> instruction) -> optional<ScriptBytecodeLocation>
{
    FO_STACK_TRACE_ENTRY();

    nptr<const AngelScript::asDWORD> best_instruction {};
    ScriptBytecodeLocation best_location;
    const auto line_entry_count = numeric_cast<AngelScript::asUINT>(std::max(func->GetLineEntryCount(), 0));

    for (AngelScript::asUINT i = 0; i < line_entry_count; i++) {
        int row = 0;
        int column = 1;
        nptr<const char> section;
        nptr<const AngelScript::asDWORD> line_instruction;

        if (func->GetLineEntry(i, &row, &column, section.get_pp(), line_instruction.get_pp()) < 0 || row <= 0) {
            continue;
        }

        if (IsInstructionAtOrBefore(line_instruction, instruction) && (!best_instruction || IsInstructionAtOrAfter(line_instruction, best_instruction.as_ptr()))) {
            best_instruction = line_instruction;
            best_location = ScriptBytecodeLocation {
                .Section = section ? section.get() : "<unknown>",
                .Row = row,
                .Column = std::max(column, 1),
            };
        }
    }

    if (best_instruction) {
        return best_location;
    }

    int row = 0;
    int column = 1;
    nptr<const char> section;
    if (func->GetDeclaredAt(section.get_pp(), &row, &column) >= 0 && row > 0) {
        return ScriptBytecodeLocation {
            .Section = section ? section.get() : "<unknown>",
            .Row = row,
            .Column = std::max(column, 1),
        };
    }

    return std::nullopt;
}

static auto FormatUsageErrorLocation(const ScriptBytecodeLocation& location, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (lnt) {
        const auto line = numeric_cast<uint32_t>(location.Row);
        return strex("{}({},1)", Preprocessor::ResolveOriginalFile(line, lnt.get()), Preprocessor::ResolveOriginalLine(line, lnt.get())).str();
    }

    return strex("{}({},{})", location.Section, location.Row, location.Column).str();
}

static auto ShouldSkipAttributedUsageValidation(ptr<const AngelScript::asIScriptFunction> caller, nptr<const vector<string>> allowed_namespaces) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!allowed_namespaces) {
        return false;
    }

    const nptr<const char> caller_ns = caller->GetNamespace();
    return IsScriptNamespaceAllowed(caller_ns ? caller_ns.get() : "", *allowed_namespaces);
}

static auto MakeAttributedUsageError(ptr<const AngelScript::asIScriptFunction> caller, ptr<const AngelScript::asIScriptFunction> callee, ptr<const AngelScript::asDWORD> instruction, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
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

static auto MakeMarkerPropagationError(ptr<const AngelScript::asIScriptFunction> caller, ptr<const AngelScript::asIScriptFunction> callee, string_view marker_name, ptr<const AngelScript::asDWORD> instruction, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto caller_decl = GetFunctionDeclarationString(caller);
    const auto callee_decl = GetFunctionDeclarationString(callee);
    auto message = strex("Function '{}' is marked [[{}]] but is called from '{}' which does not carry the same marker; add [[{}]] to the caller to propagate it", callee_decl, marker_name, caller_decl, marker_name).str();

    if (const auto location = ResolveInstructionLocation(caller, instruction); location.has_value()) {
        return strex("{}: error : {}", FormatUsageErrorLocation(*location, lnt), message).str();
    }

    return message;
}

static auto ResolveInstructionFunction(ptr<const AngelScript::asDWORD> instruction, ptr<AngelScript::asIScriptEngine> engine) noexcept -> nptr<AngelScript::asIScriptFunction>
{
    FO_STACK_TRACE_ENTRY();

    const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8_t>(*instruction));

    switch (opcode) {
    case AngelScript::asBC_CALL:
    case AngelScript::asBC_CALLSYS:
    case AngelScript::asBC_Thiscall1:
    case AngelScript::asBC_CALLINTF:
        return engine->GetFunctionById(ReadInstructionFunctionId(instruction, 1));
    case AngelScript::asBC_ALLOC:
        return engine->GetFunctionById(ReadInstructionFunctionId(instruction, AS_PTR_SIZE + 1));
    case AngelScript::asBC_FuncPtr:
        return ReadInstructionFunctionPtr(instruction, 1);
    default:
        return nullptr;
    }
}

static void CleanupScriptFunctionAttributeUserData(ptr<ScriptFunctionAttributeUserData> user_data) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_user_data = adopt_unique_ptr(user_data);
    ignore_unused(owned_user_data);
}

void CleanupScriptFunctionAttributes(AngelScript::asIScriptFunction* raw_func)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_func != nullptr, "Missing script function for attribute cleanup");
    ptr<AngelScript::asIScriptFunction> func = raw_func;
    nptr<ScriptFunctionAttributeUserData> user_data = cast_from_void<ScriptFunctionAttributeUserData*>(func->GetUserData(AS_FUNC_ATTRIBUTES_USER_DATA));
    if (user_data) {
        CleanupScriptFunctionAttributeUserData(user_data.as_ptr());
    }
}

auto GetFunctionAttributesUserData(ptr<const AngelScript::asIScriptFunction> func) noexcept -> nptr<const ScriptFunctionAttributeUserData>
{
    FO_STACK_TRACE_ENTRY();

    return cast_from_void<ScriptFunctionAttributeUserData*>(func->GetUserData(AS_FUNC_ATTRIBUTES_USER_DATA));
}

auto FindFunctionAttribute(ptr<const AngelScript::asIScriptFunction> func, string_view attribute) noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (nptr<const string> raw_attr = FindAttribute(GetFunctionAttributesUserData(func), attribute); raw_attr) {
        return *raw_attr;
    }

    return {};
}

auto HasFunctionAttribute(ptr<const AngelScript::asIScriptFunction> func, string_view attribute) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return HasAttribute(GetFunctionAttributesUserData(func), attribute);
}

auto ParseFunctionAttributeRecords(ptr<Preprocessor::Context> pp_ctx, Preprocessor::LexemList& lexems, string& errors) -> vector<ParsedFunctionAttributeRecord>
{
    FO_STACK_TRACE_ENTRY();

    vector<ParsedFunctionAttributeRecord> records;
    vector<LexemIt> tokens_to_strip;
    vector<ScriptSourceScopeKind> scope_stack;
    vector<string> namespace_stack;
    vector<string> type_stack;
    map<tuple<string, string, string>, uint32_t> overload_indexes;
    nptr<const Preprocessor::LineNumberTranslator> lnt = Preprocessor::GetLineNumberTranslator(pp_ctx.get_no_const());
    uint32_t line = 1;

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
                        .SourceFile = lnt ? string {Preprocessor::ResolveOriginalFile(attr_line, lnt.get())} : string {},
                        .SourceLine = lnt ? Preprocessor::ResolveOriginalLine(attr_line, lnt.get()) : attr_line,
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

    writer.Write<uint32_t>(numeric_cast<uint32_t>(records.size()));

    for (const auto& record : records) {
        writer.WriteString(record.Namespace);
        writer.WriteString(record.ObjectType);
        writer.WriteString(record.Name);
        writer.Write<uint32_t>(record.OverloadIndex);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(record.Attributes.size()));

        for (const auto& attr : record.Attributes) {
            writer.WriteString(attr);
        }

        writer.WriteString(record.SourceFile);
        writer.Write<uint32_t>(record.SourceLine);
    }
}

auto DeserializeFunctionAttributeRecords(DataReader& reader) -> vector<ParsedFunctionAttributeRecord>
{
    FO_STACK_TRACE_ENTRY();

    vector<ParsedFunctionAttributeRecord> records;
    const auto count = reader.Read<uint32_t>();
    records.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        ParsedFunctionAttributeRecord record;
        record.Namespace = reader.ReadString();
        record.ObjectType = reader.ReadString();
        record.Name = reader.ReadString();
        record.OverloadIndex = reader.Read<uint32_t>();

        const auto attrs_count = reader.Read<uint32_t>();
        record.Attributes.reserve(attrs_count);

        for (uint32_t j = 0; j < attrs_count; j++) {
            record.Attributes.emplace_back(reader.ReadString());
        }

        record.SourceFile = reader.ReadString();
        record.SourceLine = reader.Read<uint32_t>();
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

auto BindFunctionAttributeRecords(ptr<AngelScript::asIScriptModule> mod, const vector<ParsedFunctionAttributeRecord>& records, nptr<const vector<string>> project_blocking_extras) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (records.empty()) {
        return {};
    }

    ptr<AngelScript::asIScriptEngine> engine = mod->GetEngine();
    nptr<const Preprocessor::LineNumberTranslator> lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(engine->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));

    for (const auto& record : records) {
        nptr<AngelScript::asIScriptFunction> nullable_target_func {};

        if (record.ObjectType.empty()) {
            uint32_t ordinal = 0;

            for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
                nptr<AngelScript::asIScriptFunction> nullable_func = mod->GetFunctionByIndex(i);

                if (!IsAttributedScriptFunction(nullable_func)) {
                    continue;
                }

                auto func = nullable_func.as_ptr();
                const nptr<const char> func_name = func->GetName();
                const nptr<const AngelScript::asITypeInfo> object_type = func->GetObjectType();

                if (object_type || !func_name || string_view {func_name.get()} != record.Name) {
                    continue;
                }

                const nptr<const char> func_ns = func->GetNamespace();
                const auto ns = func_ns ? string_view {func_ns.get()} : string_view {};

                if (ns != record.Namespace) {
                    continue;
                }

                if (ordinal++ == record.OverloadIndex) {
                    nullable_target_func = nullable_func;
                    break;
                }
            }
        }
        else if (nptr<AngelScript::asITypeInfo> nullable_ti = FindModuleObjectType(mod, record.Namespace, record.ObjectType); nullable_ti) {
            auto ti = nullable_ti.as_ptr();
            uint32_t ordinal = 0;

            for (AngelScript::asUINT j = 0; j < ti->GetMethodCount(); j++) {
                nptr<AngelScript::asIScriptFunction> nullable_func = ti->GetMethodByIndex(j, false);

                if (!IsAttributedScriptFunction(nullable_func)) {
                    continue;
                }

                auto func = nullable_func.as_ptr();
                const nptr<const char> func_name = func->GetName();

                if (!func_name || string_view {func_name.get()} != record.Name) {
                    continue;
                }

                if (ordinal++ == record.OverloadIndex) {
                    nullable_target_func = nullable_func;
                    break;
                }
            }
        }

        if (!nullable_target_func && !record.SourceFile.empty()) {
            nptr<AngelScript::asIScriptFunction> fallback_func {};

            for (ptr<AngelScript::asIScriptFunction> func : CollectModuleScriptFunctions(mod)) {
                const nptr<const char> func_name = func->GetName();

                if (!func_name || string_view {func_name.get()} != record.Name) {
                    continue;
                }

                const auto location = ResolveDeclaredFunctionSourceLocation(func, lnt);

                if (!location.has_value() || location->first != record.SourceFile || location->second != record.SourceLine) {
                    continue;
                }

                if (fallback_func && !IsSameScriptFunction(fallback_func, func)) {
                    fallback_func = nullptr;
                    break;
                }

                fallback_func = func;
            }

            nullable_target_func = fallback_func;
        }

        if (!nullable_target_func) {
            if (!record.SourceFile.empty()) {
                return strex("{}({},1): error : Can't bind attributes to function {} [overload {}]", record.SourceFile, record.SourceLine, FormatRecordFunctionName(record), record.OverloadIndex).str();
            }

            return strex("Can't bind attributes to function {} [overload {}]", FormatRecordFunctionName(record), record.OverloadIndex).str();
        }

        auto target_func = nullable_target_func.as_ptr();
        SetFunctionAttributesWithVirtualMirror(target_func, record.Attributes, project_blocking_extras);
    }

    return {};
}

static auto ClassifyFunctionAttributes(ptr<const AngelScript::asIScriptFunction> func, bool& has_blocking, vector<string_view>& markers, nptr<const vector<string>> project_blocking_extras) -> void
{
    FO_STACK_TRACE_ENTRY();

    has_blocking = false;
    markers.clear();

    nptr<const ScriptFunctionAttributeUserData> nullable_user_data = GetFunctionAttributesUserData(func);

    if (!nullable_user_data) {
        return;
    }

    auto user_data = nullable_user_data.as_ptr();

    for (const auto& attr : user_data->Attributes) {
        const auto base = GetAttributeBaseName(attr);

        if (IsDirectCallBlockingAttribute(base, project_blocking_extras)) {
            has_blocking = true;
        }
        else {
            markers.emplace_back(base);
        }
    }
}

auto ValidateAttributedFunctionUsage(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt, nptr<const vector<string>> allowed_namespaces, nptr<const vector<string>> project_blocking_extras) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto funcs = CollectModuleScriptFunctions(mod);
    ptr<AngelScript::asIScriptEngine> engine = mod->GetEngine();

    string errors;
    vector<string_view> caller_markers;
    vector<string_view> target_markers;
    bool caller_has_blocking {};
    bool target_has_blocking {};

    for (ptr<AngelScript::asIScriptFunction> caller : funcs) {
        AngelScript::asUINT bc_length = 0;
        nptr<const AngelScript::asDWORD> bc = caller->GetByteCode(&bc_length);

        if (!bc || bc_length == 0) {
            continue;
        }

        const_span<AngelScript::asDWORD> bytecode = ByteCodeSpan(bc.as_ptr(), bc_length);
        ClassifyFunctionAttributes(caller, caller_has_blocking, caller_markers, project_blocking_extras);

        for (AngelScript::asUINT pos = 0; pos < bc_length;) {
            auto instruction = ByteCodeInstructionAt(bytecode, pos);
            const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8_t>(*instruction));
            const auto instr_size = AngelScript::asBCTypeSize[AngelScript::asBCInfo[opcode].type];

            if (opcode == AngelScript::asBC_CALL || opcode == AngelScript::asBC_CALLSYS || opcode == AngelScript::asBC_CALLINTF || opcode == AngelScript::asBC_Thiscall1) {
                nptr<const AngelScript::asIScriptFunction> nullable_target = engine->GetFunctionById(ReadInstructionFunctionId(instruction, 1));
                FO_VERIFY_AND_THROW(nullable_target, "Called function not found");

                auto target = nullable_target.as_ptr();
                ClassifyFunctionAttributes(target, target_has_blocking, target_markers, project_blocking_extras);

                // Rule 1: direct-call-blocking attribute → caller must live in an allowed namespace.
                if (target_has_blocking) {
                    if (!ShouldSkipAttributedUsageValidation(caller, allowed_namespaces)) {
                        if (!errors.empty()) {
                            errors.append("\n");
                        }

                        errors.append(MakeAttributedUsageError(caller, target, instruction, lnt));
                    }
                }

                // Rule 2: marker attribute (any non-blocking attribute, e.g. [[Async]]) →
                // caller must carry the same marker.
                for (const auto& marker : target_markers) {
                    const bool caller_has_marker = std::find(caller_markers.begin(), caller_markers.end(), marker) != caller_markers.end();

                    if (!caller_has_marker) {
                        if (!errors.empty()) {
                            errors.append("\n");
                        }

                        errors.append(MakeMarkerPropagationError(caller, target, marker, instruction, lnt));
                    }
                }
            }

            pos += instr_size;
        }
    }

    return errors;
}

static auto TryParseAttributePriority(string_view raw_attribute, string_view attribute_name, int32_t& priority) noexcept -> bool
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

    auto parsed_priority = int32_t {};
    const char* const begin = args.data();
    const char* const end = begin + args.size();
    const auto [parsed_end, ec] = std::from_chars(begin, end, parsed_priority);

    if (ec != std::errc {} || parsed_end != end) {
        return false;
    }

    priority = parsed_priority;
    return true;
}

static auto MakeSpecialAttributeError(ptr<const AngelScript::asIScriptFunction> func, string_view raw_attribute, string_view details, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto func_decl = GetFunctionDeclarationString(func);
    auto message = strex("Invalid attribute '[[{}]]' on function '{}': {}", raw_attribute, func_decl, details).str();

    if (const auto location = ResolveDeclaredFunctionSourceLocation(func, lnt); location.has_value()) {
        return strex("{}({},1): error : {}", location->first, location->second, message).str();
    }

    return message;
}

static void AppendSpecialAttributeError(string& errors, ptr<const AngelScript::asIScriptFunction> func, string_view raw_attribute, string_view details, nptr<const Preprocessor::LineNumberTranslator> lnt)
{
    FO_STACK_TRACE_ENTRY();

    if (!errors.empty()) {
        errors.append("\n");
    }

    errors.append(MakeSpecialAttributeError(func, raw_attribute, details, lnt));
}

static void ValidateSpecialAttribute(string& errors, ptr<const AngelScript::asIScriptFunction> func, string_view raw_attribute, string_view attribute_name, nptr<const Preprocessor::LineNumberTranslator> lnt)
{
    FO_STACK_TRACE_ENTRY();

    if (raw_attribute.empty()) {
        return;
    }

    int32_t priority {};

    if (!TryParseAttributePriority(raw_attribute, attribute_name, priority)) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "expected an optional single integer priority argument", lnt);
        return;
    }

    const nptr<const AngelScript::asITypeInfo> object_type = func->GetObjectType();

    if (object_type) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "only global functions can use this attribute", lnt);
    }
    if (func->GetParamCount() != 0) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "function must not have parameters", lnt);
    }
    if (func->GetReturnTypeId() != AngelScript::asTYPEID_VOID) {
        AppendSpecialAttributeError(errors, func, raw_attribute, "function must return void", lnt);
    }
}

auto ValidateSpecialFunctionAttributes(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    string errors;

    for (ptr<AngelScript::asIScriptFunction> func : CollectModuleScriptFunctions(mod)) {
        ValidateSpecialAttribute(errors, func, FindFunctionAttribute(func, "ModuleInit"), "ModuleInit", lnt);
    }

    return errors;
}

static auto IsScriptTypeNamed(ptr<AngelScript::asIScriptEngine> engine, int32_t type_id, string_view type_name) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<const AngelScript::asITypeInfo> nullable_type_info = engine->GetTypeInfoById(type_id);

    if (!nullable_type_info) {
        return false;
    }

    auto type_info = nullable_type_info.as_ptr();
    return string_view(type_info->GetName()) == type_name;
}

static auto IsSupportedAdminRemoteCallArgType(ptr<AngelScript::asIScriptEngine> engine, int32_t type_id, bool expect_any) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return expect_any ? IsScriptTypeNamed(engine, type_id, "any") : type_id == AngelScript::asTYPEID_INT32;
}

static auto IsSupportedAdminRemoteCallSignature(ptr<const AngelScript::asIScriptFunction> func) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> engine = func->GetEngine();
    const nptr<const AngelScript::asITypeInfo> object_type = func->GetObjectType();

    if (object_type || func->GetReturnTypeId() != AngelScript::asTYPEID_VOID) {
        return false;
    }

    const auto param_count = func->GetParamCount();
    AngelScript::asUINT first_payload_index = 0;

    if (param_count > 0) {
        int32_t first_param_type_id = 0;
        AngelScript::asDWORD first_param_flags = 0;
        nptr<const char> first_param_name;

        if (func->GetParam(0, &first_param_type_id, &first_param_flags, first_param_name.get_pp()) < 0) {
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

    int32_t first_payload_type_id = 0;
    AngelScript::asDWORD first_payload_flags = 0;
    nptr<const char> first_payload_name;

    if (func->GetParam(first_payload_index, &first_payload_type_id, &first_payload_flags, first_payload_name.get_pp()) < 0) {
        return false;
    }

    ignore_unused(first_payload_flags);
    ignore_unused(first_payload_name);

    const bool expect_any = IsScriptTypeNamed(engine, first_payload_type_id, "any");

    if (!expect_any && first_payload_type_id != AngelScript::asTYPEID_INT32) {
        return false;
    }

    for (AngelScript::asUINT index = first_payload_index + 1; index < param_count; index++) {
        int32_t param_type_id = 0;
        AngelScript::asDWORD param_flags = 0;
        nptr<const char> param_name;

        if (func->GetParam(index, &param_type_id, &param_flags, param_name.get_pp()) < 0) {
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

auto ValidateAdminRemoteCallAttributes(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    string errors;

    for (ptr<AngelScript::asIScriptFunction> func : CollectModuleScriptFunctions(mod)) {
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

static auto MatchesCallbackAttributeRule(nptr<const AngelScript::asIScriptFunction> nullable_func, const CallbackAttributeRule& rule) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_func) {
        return false;
    }

    auto func = nullable_func.as_ptr();
    const nptr<const char> func_name = func->GetName();

    if ((func->GetFuncType() != AngelScript::asFUNC_SYSTEM && func->GetFuncType() != AngelScript::asFUNC_IMPORTED) || !func_name || string_view {func_name.get()} != rule.MethodName) {
        return false;
    }

    if (rule.ObjectTypeSuffix.empty()) {
        return true;
    }

    const nptr<const char> object_name = func->GetObjectName();
    const nptr<const AngelScript::asITypeInfo> object_type = func->GetObjectType();
    return object_type && object_name && string_view {object_name.get()}.ends_with(rule.ObjectTypeSuffix);
}

static auto FindCallbackAttributeRuleByUsage(nptr<const AngelScript::asIScriptFunction> func) noexcept -> nptr<const CallbackAttributeRule>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = std::ranges::find_if(CALLBACK_ATTRIBUTE_RULES, [func](const auto& rule) { return MatchesCallbackAttributeRule(func, rule); });
    return it != CALLBACK_ATTRIBUTE_RULES.end() ? &*it : nullptr;
}

static auto FindCallbackAttributeRuleByAttribute(ptr<const AngelScript::asIScriptFunction> func) noexcept -> nptr<const CallbackAttributeRule>
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

static auto IsDelegateFactoryFunction(nptr<const AngelScript::asIScriptFunction> nullable_func) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_func) {
        return false;
    }

    auto func = nullable_func.as_ptr();

    if (func->GetFuncType() != AngelScript::asFUNC_SYSTEM) {
        return false;
    }

    const nptr<const char> name = func->GetName();
    return name && string_view {name.get()} == "$dlgte";
}

static auto MakeRestrictedCallbackUsageError(ptr<const AngelScript::asIScriptFunction> caller, ptr<const AngelScript::asIScriptFunction> callback, const optional<ScriptBytecodeLocation>& location, const CallbackAttributeRule& rule, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
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

static auto MakeMissingCallbackAttributeError(ptr<const AngelScript::asIScriptFunction> caller, ptr<const AngelScript::asIScriptFunction> callback, const optional<ScriptBytecodeLocation>& location, const CallbackAttributeRule& rule, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
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

auto ValidateEventSubscriptions(ptr<AngelScript::asIScriptModule> mod, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    FO_STACK_TRACE_ENTRY();

    string errors;
    const auto funcs = CollectModuleScriptFunctions(mod);
    ptr<AngelScript::asIScriptEngine> engine = mod->GetEngine();

    for (ptr<AngelScript::asIScriptFunction> caller : funcs) {
        AngelScript::asUINT bc_length = 0;
        nptr<const AngelScript::asDWORD> bc = caller->GetByteCode(&bc_length);

        if (!bc || bc_length == 0) {
            continue;
        }

        struct PendingCallbackTarget
        {
            nptr<const AngelScript::asIScriptFunction> Target {};
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
            if (!pending.Target) {
                return;
            }

            auto pending_target = pending.Target.as_ptr();

            if (nptr<const CallbackAttributeRule> nullable_restricted_rule = FindCallbackAttributeRuleByAttribute(pending_target); nullable_restricted_rule) {
                auto restricted_rule = nullable_restricted_rule.as_ptr();
                append_error(MakeRestrictedCallbackUsageError(caller, pending_target, pending.Location, *restricted_rule, lnt));
            }
        };

        const auto clear_pending = [&]() { pending = {}; };

        const_span<AngelScript::asDWORD> bytecode = ByteCodeSpan(bc.as_ptr(), bc_length);

        for (AngelScript::asUINT pos = 0; pos < bc_length;) {
            auto instruction = ByteCodeInstructionAt(bytecode, pos);
            const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8_t>(*instruction));
            const auto instr_size = AngelScript::asBCTypeSize[AngelScript::asBCInfo[opcode].type];

            if (pending.Target) {
                const auto current_location = ResolveInstructionLocation(caller, instruction);

                if (pending.Location.has_value() && current_location.has_value() && !IsSameSourceLine(pending.Location, current_location)) {
                    report_restricted_usage();
                    clear_pending();
                }
            }

            if (opcode == AngelScript::asBC_FuncPtr) {
                report_restricted_usage();
                pending.Target = ResolveInstructionFunction(instruction, engine);
                pending.Location = ResolveInstructionLocation(caller, instruction);
                pending.WrappedInDelegate = false;
            }
            else if (IsFunctionCallInstruction(opcode)) {
                auto target = ResolveInstructionFunction(instruction, engine);

                if (pending.Target && IsDelegateFactoryFunction(target)) {
                    pending.WrappedInDelegate = true;
                }

                if (pending.Target) {
                    auto pending_target = pending.Target.as_ptr();
                    const auto call_location = ResolveInstructionLocation(caller, instruction);
                    const auto same_line = pending.WrappedInDelegate || (!pending.Location.has_value() || !call_location.has_value()) ? true : IsSameSourceLine(pending.Location, call_location);
                    auto nullable_usage_rule = FindCallbackAttributeRuleByUsage(target);

                    if (nullable_usage_rule && same_line) {
                        auto usage_rule = nullable_usage_rule.as_ptr();

                        if (usage_rule->AttributeName == "Event") {
                            if (nptr<const CallbackAttributeRule> nullable_restricted_rule = FindCallbackAttributeRuleByAttribute(pending_target); nullable_restricted_rule) {
                                auto restricted_rule = nullable_restricted_rule.as_ptr();
                                if (restricted_rule->AttributeName == usage_rule->AttributeName) {
                                    clear_pending();
                                }
                                else {
                                    append_error(MakeRestrictedCallbackUsageError(caller, pending_target, pending.Location, *restricted_rule, lnt));
                                    clear_pending();
                                }
                            }
                            else {
                                append_error(MakeMissingCallbackAttributeError(caller, pending_target, pending.Location, *usage_rule, lnt));
                                clear_pending();
                            }
                        }
                        else {
                            if (nptr<const CallbackAttributeRule> nullable_restricted_rule = FindCallbackAttributeRuleByAttribute(pending_target); nullable_restricted_rule) {
                                auto restricted_rule = nullable_restricted_rule.as_ptr();
                                if (restricted_rule->AttributeName == usage_rule->AttributeName) {
                                    clear_pending();
                                }
                                else {
                                    append_error(MakeRestrictedCallbackUsageError(caller, pending_target, pending.Location, *restricted_rule, lnt));
                                    clear_pending();
                                }
                            }
                            else {
                                append_error(MakeMissingCallbackAttributeError(caller, pending_target, pending.Location, *usage_rule, lnt));
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
