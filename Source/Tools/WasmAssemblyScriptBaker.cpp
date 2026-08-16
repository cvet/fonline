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

#include "WasmAssemblyScriptBaker.h"

#if FO_WASM_SCRIPTING

#include <json.hpp>

FO_BEGIN_NAMESPACE

static auto WasmAssemblyScriptToEngineString(string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

    return string {value.begin(), value.end()};
}

static void WasmAssemblyScriptAppend(string& output, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    output.append(value.data(), value.size());
}

static auto WasmAssemblyScriptSanitizeIdentifier(string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;
    result.reserve(value.size() + 1);

    for (char ch : value) {
        bool valid_char = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
        result.push_back(valid_char ? ch : '_');
    }

    if (result.empty() || (result.front() >= '0' && result.front() <= '9')) {
        result.insert(result.begin(), '_');
    }

    if (result == "as" || result == "break" || result == "case" || result == "class" || result == "const" || result == "continue" || result == "declare" || result == "default" || result == "do" || result == "else" || result == "enum" || result == "export" || result == "extends" || result == "false" || result == "for" || result == "function" || result == "if" || result == "import" || result == "let" || result == "namespace" || result == "new" || result == "null" || result == "return" || result == "static" || result == "switch" || result == "this" || result == "true" || result == "type" || result == "var" || result == "void" || result == "while") {
        result.insert(result.begin(), '_');
    }

    return result;
}

static auto WasmAssemblyScriptScalarTypeName(const nlohmann::json& value) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (!value.is_string()) {
        return "i32";
    }

    const auto& type_name = value.get_ref<const nlohmann::json::string_t&>();

    if (type_name == "i32") {
        return "i32";
    }
    if (type_name == "i64") {
        return "i64";
    }
    if (type_name == "f32") {
        return "f32";
    }
    if (type_name == "f64") {
        return "f64";
    }

    return "i32";
}

static void WasmAssemblyScriptAppendRuntimeImports(string& output)
{
    FO_STACK_TRACE_ENTRY();

    WasmAssemblyScriptAppend(output,
        "@external(\"fonline\", \"log_i32\")\n"
        "export declare function log_i32(value: i32): void;\n"
        "@external(\"fonline\", \"log_i64\")\n"
        "export declare function log_i64(value: i64): void;\n"
        "@external(\"fonline\", \"log_f32\")\n"
        "export declare function log_f32(value: f32): void;\n"
        "@external(\"fonline\", \"log_f64\")\n"
        "export declare function log_f64(value: f64): void;\n"
        "@external(\"fonline\", \"log_utf8\")\n"
        "export declare function log_utf8(ptr: i32, len: i32): void;\n"
        "@external(\"fonline\", \"get_side\")\n"
        "export declare function get_side(): i32;\n"
        "@external(\"fonline\", \"get_frame_time_ms\")\n"
        "export declare function get_frame_time_ms(): i64;\n"
        "@external(\"fonline\", \"get_frame_delta_time_ms\")\n"
        "export declare function get_frame_delta_time_ms(): i64;\n"
        "@external(\"fonline\", \"is_time_synchronized\")\n"
        "export declare function is_time_synchronized(): i32;\n"
        "@external(\"fonline\", \"get_synchronized_time_ms\")\n"
        "export declare function get_synchronized_time_ms(): i64;\n"
        "@external(\"fonline\", \"callback_retain\")\n"
        "export declare function callback_retain(ptr: i32, len: i32): i32;\n"
        "@external(\"fonline\", \"callback_release\")\n"
        "export declare function callback_release(ptr: i32, len: i32): i32;\n\n");
}

static void WasmAssemblyScriptAppendHandleAliases(const nlohmann::json& doc, string& output)
{
    FO_STACK_TRACE_ENTRY();

    WasmAssemblyScriptAppend(output,
        "export type ident = i64;\n"
        "export type hstring = i64;\n"
        "export type ref_handle = i64;\n\n");

    nlohmann::json types_json = doc.value("types", nlohmann::json::object());

    auto append_aliases = [&output](const nlohmann::json& entries) {
        FO_STACK_TRACE_ENTRY();

        if (!entries.is_array()) {
            return;
        }

        for (const nlohmann::json& entry : entries) {
            if (!entry.is_object()) {
                continue;
            }

            if (!entry.contains("name") || !entry["name"].is_string()) {
                continue;
            }

            const auto& raw_name = entry["name"].get_ref<const nlohmann::json::string_t&>();

            if (raw_name.empty()) {
                continue;
            }

            string alias_name = WasmAssemblyScriptSanitizeIdentifier(WasmAssemblyScriptToEngineString(raw_name));
            WasmAssemblyScriptAppend(output, strex("export type {} = i64;\n", alias_name));
        }
    };

    append_aliases(types_json.value("entities", nlohmann::json::array()));
    append_aliases(types_json.value("fixed", nlohmann::json::array()));
    append_aliases(types_json.value("ref", nlohmann::json::array()));
    WasmAssemblyScriptAppend(output, "\n");
}

static void WasmAssemblyScriptAppendEnums(const nlohmann::json& doc, string& output)
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json types_json = doc.value("types", nlohmann::json::object());
    nlohmann::json enums_json = types_json.value("enums", nlohmann::json::array());

    if (!enums_json.is_array()) {
        return;
    }

    for (const nlohmann::json& enum_json : enums_json) {
        if (!enum_json.is_object()) {
            continue;
        }

        if (!enum_json.contains("name") || !enum_json["name"].is_string()) {
            continue;
        }

        const auto& raw_name = enum_json["name"].get_ref<const nlohmann::json::string_t&>();

        if (raw_name.empty()) {
            continue;
        }

        string enum_name = WasmAssemblyScriptSanitizeIdentifier(WasmAssemblyScriptToEngineString(raw_name));
        WasmAssemblyScriptAppend(output, strex("export const enum {} {{\n", enum_name));

        nlohmann::json values_json = enum_json.value("values", nlohmann::json::object());

        if (values_json.is_object()) {
            for (auto it = values_json.begin(); it != values_json.end(); ++it) {
                string value_name = WasmAssemblyScriptSanitizeIdentifier(WasmAssemblyScriptToEngineString(it.key()));
                int32_t value = it.value().is_number_integer() ? it.value().get<int32_t>() : 0;
                WasmAssemblyScriptAppend(output, strex("    {} = {},\n", value_name, value));
            }
        }

        WasmAssemblyScriptAppend(output, "}\n\n");
    }
}

static void WasmAssemblyScriptAppendImport(const nlohmann::json& import_json, string& output, size_t& unsupported_count)
{
    FO_STACK_TRACE_ENTRY();

    if (!import_json.is_object()) {
        return;
    }

    bool supported = import_json.value("supported", false);
    if (!import_json.contains("import") || !import_json["import"].is_string() || !import_json.contains("module") || !import_json["module"].is_string()) {
        return;
    }

    const auto& raw_import_name = import_json["import"].get_ref<const nlohmann::json::string_t&>();
    const auto& raw_module_name = import_json["module"].get_ref<const nlohmann::json::string_t&>();

    if (!supported) {
        unsupported_count++;
        return;
    }
    if (raw_import_name.empty() || raw_module_name.empty()) {
        return;
    }

    string import_name = WasmAssemblyScriptToEngineString(raw_import_name);
    string module_name = WasmAssemblyScriptToEngineString(raw_module_name);
    string function_name = WasmAssemblyScriptSanitizeIdentifier(import_name);
    nlohmann::json params_json = import_json.value("params", nlohmann::json::array());
    nlohmann::json results_json = import_json.value("results", nlohmann::json::array());

    WasmAssemblyScriptAppend(output, strex("@external(\"{}\", \"{}\")\n", module_name, import_name));
    WasmAssemblyScriptAppend(output, strex("export declare function {}(", function_name));

    if (params_json.is_array()) {
        for (size_t arg_index = 0; arg_index < params_json.size(); arg_index++) {
            if (arg_index != 0) {
                WasmAssemblyScriptAppend(output, ", ");
            }

            WasmAssemblyScriptAppend(output, strex("arg{}: {}", arg_index, WasmAssemblyScriptScalarTypeName(params_json[arg_index])));
        }
    }

    string_view result_type = results_json.is_array() && !results_json.empty() ? WasmAssemblyScriptScalarTypeName(results_json.front()) : string_view {"void"};
    WasmAssemblyScriptAppend(output, strex("): {};\n", result_type));
}

static void WasmAssemblyScriptAppendApiImports(const nlohmann::json& doc, string& output)
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json imports_json = doc.value("imports", nlohmann::json::object());
    nlohmann::json methods_json = imports_json.value("methods", nlohmann::json::array());
    nlohmann::json properties_json = imports_json.value("properties", nlohmann::json::array());
    size_t unsupported_count = 0;

    WasmAssemblyScriptAppend(output, "export namespace Api {\n");

    if (methods_json.is_array()) {
        for (const nlohmann::json& method_json : methods_json) {
            string declaration;
            WasmAssemblyScriptAppendImport(method_json, declaration, unsupported_count);

            if (!declaration.empty()) {
                for (string_view line : strvex(declaration).split('\n')) {
                    if (!line.empty()) {
                        WasmAssemblyScriptAppend(output, strex("    {}\n", line));
                    }
                }
            }
        }
    }
    if (properties_json.is_array()) {
        for (const nlohmann::json& property_json : properties_json) {
            string declaration;
            WasmAssemblyScriptAppendImport(property_json, declaration, unsupported_count);

            if (!declaration.empty()) {
                for (string_view line : strvex(declaration).split('\n')) {
                    if (!line.empty()) {
                        WasmAssemblyScriptAppend(output, strex("    {}\n", line));
                    }
                }
            }
        }
    }

    WasmAssemblyScriptAppend(output, "}\n");

    if (unsupported_count != 0) {
        WasmAssemblyScriptAppend(output, strex("\n// {} metadata entries are intentionally omitted because their WASM ABI is not supported yet.\n", unsupported_count));
    }
}

auto WasmAssemblyScriptBaker::IsFrontend(string_view frontend) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return strvex(frontend).compare_ignore_case(FRONTEND_NAME) || strvex(frontend).compare_ignore_case("as") || strvex(frontend).compare_ignore_case("asc");
}

auto WasmAssemblyScriptBaker::BuildBindings(string_view api_manifest_json) -> string
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json doc = nlohmann::json::parse(api_manifest_json.begin(), api_manifest_json.end());
    string output;

    WasmAssemblyScriptAppend(output,
        "// Generated by WasmAssemblyScriptBaker. Do not edit.\n"
        "// Source: bake-time EngineMetadata WASM API manifest.\n\n");

    WasmAssemblyScriptAppendHandleAliases(doc, output);
    WasmAssemblyScriptAppendEnums(doc, output);
    WasmAssemblyScriptAppendRuntimeImports(output);
    WasmAssemblyScriptAppendApiImports(doc, output);

    return output;
}

auto WasmAssemblyScriptBaker::MakeDefaultCommand(const CompileOptions& options) -> string
{
    FO_STACK_TRACE_ENTRY();

    string compiler = !options.Compiler.empty() ? options.Compiler : string {"npm exec --yes --package assemblyscript -- asc"};
    string compiler_args = !options.CompilerArgs.empty() ? options.CompilerArgs : string {"--optimize"};

    return strex("{} {} --outFile {} {}", compiler, "{source}", "{output}", compiler_args).str();
}

FO_END_NAMESPACE

#endif
