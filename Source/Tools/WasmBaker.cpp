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

#include "WasmBaker.h"

#if FO_WASM_SCRIPTING

#include "Application.h"
#include "ConfigFile.h"
#include "Properties.h"
#include "WasmApiBridge.h"
#include "WasmAssemblyScriptBaker.h"

#include <cstdlib>
#include <json.hpp>

FO_BEGIN_NAMESPACE

struct WasmCompileDescriptor
{
    string DescriptorPath {};
    string Frontend {};
    EngineSideKind TargetSide {};
    string SourcePath {};
    string OutputPath {};
    string Command {};
    string Compiler {};
    string CompilerArgs {};
    string BindingsPath {};
    vector<string> InputPaths {};
    uint64_t WriteTime {};
};

static auto MakeWasmBakerPath(string_view path) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(path).normalize_path_slashes().str();
}

static auto ParseWasmBakerTargetSide(string_view side, string_view descriptor_path) -> EngineSideKind
{
    FO_STACK_TRACE_ENTRY();

    if (side.empty() || strvex(side).compare_ignore_case("Server") || strvex(side).compare_ignore_case("ServerSide")) {
        return EngineSideKind::ServerSide;
    }
    if (strvex(side).compare_ignore_case("Client") || strvex(side).compare_ignore_case("ClientSide")) {
        return EngineSideKind::ClientSide;
    }
    if (strvex(side).compare_ignore_case("Mapper") || strvex(side).compare_ignore_case("MapperSide")) {
        return EngineSideKind::MapperSide;
    }

    throw WasmBakerException(strex("Invalid Target '{}' in '{}'", side, descriptor_path));
}

static auto MakeWasmBakerApiPath(EngineSideKind side) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("WasmApi/{}.json", GetWasmApiImportTableSideName(side));
}

static auto MakeWasmBakerBuildRoot(const BakingContext& context, string_view descriptor_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    string descriptor_name = strex(descriptor_path).erase_file_extension().str();
    descriptor_name = strex(descriptor_name).replace(':', '_').replace("..", "_").str();

    return strex(context.Settings->BakeOutput).combine_path(".wasm-build").combine_path(context.PackName).combine_path(descriptor_name).str();
}

static auto MakeWasmBakerApiBuildDir(const BakingContext& context) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(context.Settings->BakeOutput).combine_path(".wasm-build").combine_path(context.PackName).combine_path("_api").str();
}

static auto IsWasmBakerGenericFrontend(string_view frontend) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return frontend.empty() || strvex(frontend).compare_ignore_case("Generic") || strvex(frontend).compare_ignore_case("Command");
}

static auto ShouldCompileWasmBakerDescriptor(const WasmCompileDescriptor& desc) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !desc.Command.empty() || WasmAssemblyScriptBaker::IsFrontend(desc.Frontend);
}

static auto WasmBakerScalarToJson(WasmScalarKind kind) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    return string(WasmScalarKindToTypeName(kind));
}

static auto WasmBakerParamAbiToString(WasmApiParamAbiKind kind) -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmApiParamAbiKind::Scalar:
        return "Scalar";
    case WasmApiParamAbiKind::Utf8StringPointer:
        return "Utf8StringPointer";
    case WasmApiParamAbiKind::Utf8StringLength:
        return "Utf8StringLength";
    case WasmApiParamAbiKind::Utf8StringOutputPointer:
        return "Utf8StringOutputPointer";
    case WasmApiParamAbiKind::Utf8StringOutputLength:
        return "Utf8StringOutputLength";
    case WasmApiParamAbiKind::MutableValuePointer:
        return "MutableValuePointer";
    case WasmApiParamAbiKind::MutableValueLength:
        return "MutableValueLength";
    case WasmApiParamAbiKind::ArrayPointer:
        return "ArrayPointer";
    case WasmApiParamAbiKind::ArrayByteLength:
        return "ArrayByteLength";
    case WasmApiParamAbiKind::ArrayOutputPointer:
        return "ArrayOutputPointer";
    case WasmApiParamAbiKind::ArrayOutputByteLength:
        return "ArrayOutputByteLength";
    case WasmApiParamAbiKind::MutableArrayPointer:
        return "MutableArrayPointer";
    case WasmApiParamAbiKind::MutableArrayByteLength:
        return "MutableArrayByteLength";
    case WasmApiParamAbiKind::MutableArrayCapacityByteLength:
        return "MutableArrayCapacityByteLength";
    case WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer:
        return "MutableArrayRequiredByteLengthPointer";
    case WasmApiParamAbiKind::DictPointer:
        return "DictPointer";
    case WasmApiParamAbiKind::DictByteLength:
        return "DictByteLength";
    case WasmApiParamAbiKind::DictOutputPointer:
        return "DictOutputPointer";
    case WasmApiParamAbiKind::DictOutputByteLength:
        return "DictOutputByteLength";
    case WasmApiParamAbiKind::MutableDictPointer:
        return "MutableDictPointer";
    case WasmApiParamAbiKind::MutableDictByteLength:
        return "MutableDictByteLength";
    case WasmApiParamAbiKind::MutableDictCapacityByteLength:
        return "MutableDictCapacityByteLength";
    case WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer:
        return "MutableDictRequiredByteLengthPointer";
    case WasmApiParamAbiKind::CallbackPointer:
        return "CallbackPointer";
    case WasmApiParamAbiKind::CallbackLength:
        return "CallbackLength";
    case WasmApiParamAbiKind::MutableUtf8StringPointer:
        return "MutableUtf8StringPointer";
    case WasmApiParamAbiKind::MutableUtf8StringByteLength:
        return "MutableUtf8StringByteLength";
    case WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength:
        return "MutableUtf8StringCapacityByteLength";
    case WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer:
        return "MutableUtf8StringRequiredByteLengthPointer";
    case WasmApiParamAbiKind::ValuePointer:
        return "ValuePointer";
    case WasmApiParamAbiKind::ValueByteLength:
        return "ValueByteLength";
    case WasmApiParamAbiKind::ValueOutputPointer:
        return "ValueOutputPointer";
    case WasmApiParamAbiKind::ValueOutputByteLength:
        return "ValueOutputByteLength";
    default:
        break;
    }

    return "Unknown";
}

static auto WasmBakerResultAbiToString(WasmApiResultAbiKind kind) -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmApiResultAbiKind::None:
        return "None";
    case WasmApiResultAbiKind::Scalar:
        return "Scalar";
    case WasmApiResultAbiKind::Utf8String:
        return "Utf8String";
    case WasmApiResultAbiKind::Value:
        return "Value";
    case WasmApiResultAbiKind::Array:
        return "Array";
    case WasmApiResultAbiKind::Dict:
        return "Dict";
    default:
        break;
    }

    return "Unknown";
}

static auto WasmBakerReceiverKindToString(WasmApiReceiverKind kind) -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmApiReceiverKind::None:
        return "None";
    case WasmApiReceiverKind::Entity:
        return "Entity";
    case WasmApiReceiverKind::FixedType:
        return "FixedType";
    case WasmApiReceiverKind::RefType:
        return "RefType";
    default:
        break;
    }

    return "Unknown";
}

static auto WasmBakerPropertyAccessToString(WasmApiPropertyAccess access) -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (access) {
    case WasmApiPropertyAccess::Getter:
        return "Getter";
    case WasmApiPropertyAccess::Setter:
        return "Setter";
    default:
        break;
    }

    return "Unknown";
}

static auto WasmBakerComplexTypeToString(const ComplexTypeDesc& type) -> string
{
    FO_STACK_TRACE_ENTRY();

    switch (type.Kind) {
    case ComplexTypeKind::None:
        return "void";
    case ComplexTypeKind::Simple:
        return type.IsMutable ? strex("{}&", type.BaseType.Name).str() : type.BaseType.Name;
    case ComplexTypeKind::Array:
        return type.IsMutable ? strex("{}[]&", type.BaseType.Name).str() : strex("{}[]", type.BaseType.Name).str();
    case ComplexTypeKind::Dict:
        return type.IsMutable ? strex("{}=>{}&", type.KeyType.has_value() ? type.KeyType->Name : string {"unknown"}, type.BaseType.Name).str() : strex("{}=>{}", type.KeyType.has_value() ? type.KeyType->Name : string {"unknown"}, type.BaseType.Name).str();
    case ComplexTypeKind::DictOfArray:
        return type.IsMutable ? strex("{}=>{}[]&", type.KeyType.has_value() ? type.KeyType->Name : string {"unknown"}, type.BaseType.Name).str() : strex("{}=>{}[]", type.KeyType.has_value() ? type.KeyType->Name : string {"unknown"}, type.BaseType.Name).str();
    case ComplexTypeKind::Callback:
        return "callback";
    default:
        break;
    }

    return "unknown";
}

static auto WasmBakerScalarsToJson(const vector<WasmScalarKind>& values) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (WasmScalarKind value : values) {
        result.push_back(WasmBakerScalarToJson(value));
    }

    return result;
}

static auto WasmBakerScalarSpanToJson(const_span<WasmScalarKind> values) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (WasmScalarKind value : values) {
        result.push_back(WasmBakerScalarToJson(value));
    }

    return result;
}

static auto WasmBakerParamAbiToJson(const vector<WasmApiParamAbiKind>& values) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (WasmApiParamAbiKind value : values) {
        result.push_back(string(WasmBakerParamAbiToString(value)));
    }

    return result;
}

static auto WasmBakerParamAbiSpanToJson(const_span<WasmApiParamAbiKind> values) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (WasmApiParamAbiKind value : values) {
        result.push_back(string(WasmBakerParamAbiToString(value)));
    }

    return result;
}

static auto WasmBakerResultsToJson(WasmScalarKind result) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json results = nlohmann::json::array();

    if (result != WasmScalarKind::None) {
        results.push_back(WasmBakerScalarToJson(result));
    }

    return results;
}

static auto WasmBakerArgsToJson(const vector<ArgDesc>& args) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (const ArgDesc& arg : args) {
        nlohmann::json arg_json;
        arg_json["name"] = arg.Name;
        arg_json["type"] = WasmBakerComplexTypeToString(arg.Type);
        arg_json["nullable"] = arg.Nullable;
        arg_json["default"] = arg.DefaultValue;
        result.push_back(std::move(arg_json));
    }

    return result;
}

static auto WasmBakerStructFieldsToJson(const StructLayoutDesc* layout) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    if (layout == nullptr) {
        return result;
    }

    for (const FieldDesc& field : layout->Fields) {
        nlohmann::json field_json;
        field_json["name"] = field.Name;
        field_json["type"] = field.Type.Name;
        field_json["offset"] = field.Offset;
        result.push_back(std::move(field_json));
    }

    return result;
}

static auto WasmBakerEntityTypesToJson(const map<hstring, EntityTypeDesc>& types) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (const auto& [type_name, type] : types) {
        nlohmann::json type_json;
        type_json["name"] = type_name.as_str();
        type_json["exported"] = type.Exported;
        type_json["global"] = type.IsGlobal;
        type_json["hasProtos"] = type.HasProtos;
        type_json["hasStatics"] = type.HasStatics;
        type_json["hasAbstract"] = type.HasAbstract;
        result.push_back(std::move(type_json));
    }

    return result;
}

static auto WasmBakerBaseTypesToJson(const unordered_map<string, BaseTypeDesc>& types) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (const auto& [type_name, type] : types) {
        nlohmann::json type_json;
        type_json["name"] = type_name;
        type_json["size"] = type.Size;
        type_json["object"] = type.IsObject;
        type_json["entity"] = type.IsEntity;
        type_json["globalEntity"] = type.IsGlobalEntity;
        type_json["string"] = type.IsString;
        type_json["hashedString"] = type.IsHashedString;
        type_json["enum"] = type.IsEnum;
        type_json["primitive"] = type.IsPrimitive;
        type_json["int"] = type.IsInt;
        type_json["signedInt"] = type.IsSignedInt;
        type_json["float"] = type.IsFloat;
        type_json["bool"] = type.IsBool;
        type_json["struct"] = type.IsStruct;
        type_json["refType"] = type.IsRefType;
        type_json["fixedType"] = type.IsFixedType;
        type_json["entityProto"] = type.IsEntityProto;
        type_json["fields"] = WasmBakerStructFieldsToJson(type.StructLayout.get());
        result.push_back(std::move(type_json));
    }

    return result;
}

static auto WasmBakerRefTypesToJson(const unordered_map<string, RefTypeDesc>& types) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (const auto& [type_name, type] : types) {
        nlohmann::json type_json;
        type_json["name"] = type_name;
        type_json["dynamicLayout"] = type.IsDynamicLayout;
        type_json["methods"] = nlohmann::json::array();

        for (const MethodDesc& method : type.Methods) {
            nlohmann::json method_json;
            method_json["name"] = method.Name;
            method_json["args"] = WasmBakerArgsToJson(method.Args);
            method_json["return"] = WasmBakerComplexTypeToString(method.Ret);
            method_json["passOwnership"] = method.PassOwnership;
            method_json["returnNullable"] = method.ReturnNullable;
            type_json["methods"].push_back(std::move(method_json));
        }

        result.push_back(std::move(type_json));
    }

    return result;
}

static auto WasmBakerEnumsToJson(const unordered_map<string, unordered_map<string, int32_t>>& enums) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json result = nlohmann::json::array();

    for (const auto& [enum_name, values] : enums) {
        nlohmann::json enum_json;
        enum_json["name"] = enum_name;
        enum_json["values"] = nlohmann::json::object();

        for (const auto& [value_name, value] : values) {
            enum_json["values"][value_name.c_str()] = value;
        }

        result.push_back(std::move(enum_json));
    }

    return result;
}

static auto WasmBakerMethodToJson(const WasmApiMethodDesc& method) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json method_json;
    method_json["module"] = method.Module;
    method_json["import"] = method.Name;
    method_json["entity"] = method.EntityName;
    method_json["receiverKind"] = string(WasmBakerReceiverKindToString(method.ReceiverKind));
    method_json["hasReceiverHandle"] = method.HasReceiverHandle;
    method_json["params"] = WasmBakerScalarsToJson(method.Args);
    method_json["paramAbi"] = WasmBakerParamAbiToJson(method.ParamAbi);
    method_json["results"] = WasmBakerResultsToJson(method.Ret);
    method_json["resultAbi"] = string(WasmBakerResultAbiToString(method.ResultAbi));
    method_json["supported"] = method.Supported;
    method_json["unsupportedReason"] = method.UnsupportedReason;
    method_json["nativeSignature"] = MakeWasmApiNativeSignature(method);

    if (method.Method != nullptr) {
        method_json["scriptName"] = method.Method->Name;
        method_json["scriptArgs"] = WasmBakerArgsToJson(method.Method->Args);
        method_json["scriptReturn"] = WasmBakerComplexTypeToString(method.Method->Ret);
        method_json["passOwnership"] = method.Method->PassOwnership;
        method_json["returnNullable"] = method.Method->ReturnNullable;
    }

    return method_json;
}

static auto WasmBakerPropertyToJson(const WasmApiPropertyDesc& prop) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    nlohmann::json prop_json;
    prop_json["module"] = prop.Module;
    prop_json["import"] = prop.Name;
    prop_json["entity"] = prop.EntityName;
    prop_json["access"] = string(WasmBakerPropertyAccessToString(prop.Access));
    prop_json["receiverKind"] = string(WasmBakerReceiverKindToString(prop.ReceiverKind));
    prop_json["hasReceiverHandle"] = prop.HasReceiverHandle;
    prop_json["params"] = WasmBakerScalarSpanToJson(const_span<WasmScalarKind> {prop.Args.data(), prop.ArgsCount});
    prop_json["paramAbi"] = WasmBakerParamAbiSpanToJson(const_span<WasmApiParamAbiKind> {prop.ParamAbi.data(), prop.ArgsCount});
    prop_json["results"] = WasmBakerResultsToJson(prop.Ret);
    prop_json["resultAbi"] = string(WasmBakerResultAbiToString(prop.ResultAbi));
    prop_json["supported"] = prop.Supported;
    prop_json["unsupportedReason"] = prop.UnsupportedReason;
    prop_json["nativeSignature"] = MakeWasmApiPropertyNativeSignature(prop);

    if (prop.Prop != nullptr) {
        prop_json["property"] = prop.Prop->GetName();
        prop_json["propertyViewType"] = prop.Prop->GetViewTypeName();
        prop_json["propertyBaseType"] = prop.Prop->GetBaseTypeName();
        prop_json["nullable"] = prop.Prop->IsNullable();
        prop_json["mutable"] = prop.Prop->IsMutable();
        prop_json["virtual"] = prop.Prop->IsVirtual();
        prop_json["synced"] = prop.Prop->IsSynced();
    }

    return prop_json;
}

static auto BuildWasmBakerApiManifest(const EngineMetadata& meta) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    WasmApiImportTable table = BuildWasmApiImportTable(meta);
    nlohmann::json doc;
    doc["format"] = "fonline-wasm-api";
    doc["version"] = 1;
    doc["side"] = string(GetWasmApiImportTableSideName(table.Side));
    doc["module"] = string(WASM_ENGINE_API_MODULE);
    doc["imports"] = nlohmann::json::object();
    doc["imports"]["methods"] = nlohmann::json::array();
    doc["imports"]["properties"] = nlohmann::json::array();

    for (const WasmApiMethodDesc& method : table.Methods) {
        doc["imports"]["methods"].push_back(WasmBakerMethodToJson(method));
    }
    for (const WasmApiPropertyDesc& prop : table.Properties) {
        doc["imports"]["properties"].push_back(WasmBakerPropertyToJson(prop));
    }

    doc["types"] = nlohmann::json::object();
    doc["types"]["entities"] = WasmBakerEntityTypesToJson(meta.GetEntityTypes());
    doc["types"]["fixed"] = WasmBakerEntityTypesToJson(meta.GetFixedTypes());
    doc["types"]["base"] = WasmBakerBaseTypesToJson(meta.GetBaseTypes());
    doc["types"]["ref"] = WasmBakerRefTypesToJson(meta.GetRefTypes());
    doc["types"]["enums"] = WasmBakerEnumsToJson(meta.GetAllEnums());

    auto text = doc.dump(2);
    return vector<uint8_t> {text.begin(), text.end()};
}

static auto ParseWasmBakerDescriptor(const File& file) -> WasmCompileDescriptor
{
    FO_STACK_TRACE_ENTRY();

    ConfigFile cfg(file.GetStr());

    if (!cfg.HasSection("WasmScript")) {
        throw WasmBakerException(strex("WASM descriptor '{}' must contain [WasmScript]", file.GetPath()));
    }

    WasmCompileDescriptor desc;
    desc.DescriptorPath = file.GetPath();
    desc.Frontend = string(cfg.GetAsStr("WasmScript", "Frontend"));
    desc.TargetSide = ParseWasmBakerTargetSide(cfg.GetAsStr("WasmScript", "Target", "Server"), file.GetPath());
    desc.SourcePath = MakeWasmBakerPath(cfg.GetAsStr("WasmScript", "Source"));
    desc.OutputPath = MakeWasmBakerPath(cfg.GetAsStr("WasmScript", "Output"));
    desc.Command = string(cfg.GetAsStr("WasmScript", "Command"));
    desc.Compiler = string(cfg.GetAsStr("AssemblyScript", "Compiler", cfg.GetAsStr("WasmScript", "Compiler")));
    desc.CompilerArgs = string(cfg.GetAsStr("AssemblyScript", "Args", cfg.GetAsStr("WasmScript", "CompilerArgs")));
    desc.BindingsPath = MakeWasmBakerPath(cfg.GetAsStr("AssemblyScript", "Bindings", cfg.GetAsStr("WasmScript", "Bindings", WasmAssemblyScriptBaker::DEFAULT_BINDINGS_FILE)));
    desc.WriteTime = file.GetWriteTime();

    for (string input : strex(cfg.GetAsStr("WasmScript", "Inputs")).tokenize()) {
        input = MakeWasmBakerPath(input);

        if (!input.empty()) {
            desc.InputPaths.emplace_back(std::move(input));
        }
    }

    if (!IsWasmBakerGenericFrontend(desc.Frontend) && !WasmAssemblyScriptBaker::IsFrontend(desc.Frontend)) {
        throw WasmBakerException(strex("WASM descriptor '{}' has unknown Frontend '{}'", file.GetPath(), desc.Frontend));
    }
    if (!desc.OutputPath.empty() && strex(desc.OutputPath).get_file_extension() != "wasm") {
        throw WasmBakerException(strex("WASM descriptor '{}' Output must end with .wasm", file.GetPath()));
    }
    if (!desc.OutputPath.empty() && (fs_is_absolute_path(desc.OutputPath) || desc.OutputPath.starts_with('.') || desc.OutputPath.starts_with('/'))) {
        throw WasmBakerException(strex("WASM descriptor '{}' Output must be a resource-relative path", file.GetPath()));
    }
    if (!desc.BindingsPath.empty() && (fs_is_absolute_path(desc.BindingsPath) || desc.BindingsPath.starts_with('.') || desc.BindingsPath.find("..") != string::npos)) {
        throw WasmBakerException(strex("WASM descriptor '{}' AssemblyScript bindings path must be build-local", file.GetPath()));
    }
    if (ShouldCompileWasmBakerDescriptor(desc) && desc.SourcePath.empty()) {
        throw WasmBakerException(strex("WASM descriptor '{}' Command requires Source", file.GetPath()));
    }
    if (ShouldCompileWasmBakerDescriptor(desc) && desc.OutputPath.empty()) {
        throw WasmBakerException(strex("WASM descriptor '{}' Command requires Output", file.GetPath()));
    }

    return desc;
}

static auto StageWasmBakerInput(const FileCollection& files, string_view build_root, string_view input_path, uint64_t& max_write_time) -> string
{
    FO_STACK_TRACE_ENTRY();

    File input_file = files.FindFileByPath(input_path);

    if (input_file) {
        max_write_time = std::max(max_write_time, input_file.GetWriteTime());

        string staged_path = strex(build_root).combine_path(input_file.GetPath()).str();
        bool write_ok = fs_write_file(staged_path, input_file.GetData());
        FO_VERIFY_AND_THROW(write_ok, "Unable to stage WASM script input", input_file.GetPath());
        return staged_path;
    }

    if (fs_exists(input_path)) {
        max_write_time = std::max(max_write_time, fs_last_write_time(input_path));
        return string(input_path);
    }

    throw WasmBakerException(strex("WASM script input '{}' not found", input_path));
}

static auto QuoteWasmBakerShellArg(string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    string result {"\""};

    for (char ch : value) {
        if (ch == '"') {
            result += "\\\"";
        }
        else {
            result += ch;
        }
    }

    result += "\"";
    return result;
#else
    string result {"'"};

    for (const char ch : value) {
        if (ch == '\'') {
            result += "'\\''";
        }
        else {
            result += ch;
        }
    }

    result += "'";
    return result;
#endif

}

static void ReplaceWasmBakerPlaceholder(string& command, string_view placeholder, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    size_t pos = 0;

    while ((pos = command.find(placeholder, pos)) != string::npos) {
        command.replace(pos, placeholder.size(), value);
        pos += value.size();
    }
}

static auto ExpandWasmBakerCommand(string command, const BakingContext& context, const WasmCompileDescriptor& desc, string_view build_root, string_view api_path, string_view api_dir, string_view source_path, string_view output_path, string_view bindings_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    ReplaceWasmBakerPlaceholder(command, "{api}", QuoteWasmBakerShellArg(api_path));
    ReplaceWasmBakerPlaceholder(command, "{api_dir}", QuoteWasmBakerShellArg(api_dir));
    ReplaceWasmBakerPlaceholder(command, "{side}", GetWasmApiImportTableSideName(desc.TargetSide));
    ReplaceWasmBakerPlaceholder(command, "{pack}", context.PackName);
    ReplaceWasmBakerPlaceholder(command, "{build_dir}", QuoteWasmBakerShellArg(build_root));
    ReplaceWasmBakerPlaceholder(command, "{source}", QuoteWasmBakerShellArg(source_path));
    ReplaceWasmBakerPlaceholder(command, "{output}", QuoteWasmBakerShellArg(output_path));
    ReplaceWasmBakerPlaceholder(command, "{output_dir}", QuoteWasmBakerShellArg(strex(output_path).extract_dir()));
    ReplaceWasmBakerPlaceholder(command, "{bindings}", QuoteWasmBakerShellArg(bindings_path));
    ReplaceWasmBakerPlaceholder(command, "{bindings_dir}", QuoteWasmBakerShellArg(strex(bindings_path).extract_dir()));

    return command;
}

static void WriteWasmBakerManifestForCompiler(string_view api_dir, EngineSideKind side, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    string path = strex(api_dir).combine_path(strex("{}.json", GetWasmApiImportTableSideName(side))).str();
    bool write_ok = fs_write_file(path, data);
    FO_VERIFY_AND_THROW(write_ok, "Unable to write WASM API manifest", path);
}

static auto WriteWasmBakerAssemblyScriptBindings(const WasmCompileDescriptor& desc, const_span<uint8_t> api_manifest, string_view source_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    string_view api_manifest_text {reinterpret_cast<const char*>(api_manifest.data()), api_manifest.size()};
    string bindings_text = WasmAssemblyScriptBaker::BuildBindings(api_manifest_text);
    string bindings_rel_path = desc.BindingsPath.empty() ? string {WasmAssemblyScriptBaker::DEFAULT_BINDINGS_FILE} : desc.BindingsPath;
    string bindings_path = strex(source_path).extract_dir().combine_path(bindings_rel_path).str();
    bool write_ok = fs_write_file(bindings_path, bindings_text);
    FO_VERIFY_AND_THROW(write_ok, "Unable to write AssemblyScript WASM bindings", bindings_path);

    return bindings_path;
}

static auto MakeWasmBakerCompileCommand(const WasmCompileDescriptor& desc) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!desc.Command.empty()) {
        return desc.Command;
    }
    if (WasmAssemblyScriptBaker::IsFrontend(desc.Frontend)) {
        return WasmAssemblyScriptBaker::MakeDefaultCommand(WasmAssemblyScriptBaker::CompileOptions {
            .Compiler = desc.Compiler,
            .CompilerArgs = desc.CompilerArgs,
        });
    }

    return {};
}

static void CompileWasmBakerDescriptor(const FileCollection& files, const BakingContext& context, const WasmCompileDescriptor& desc, string_view api_dir, const_span<uint8_t> api_manifest)
{
    FO_STACK_TRACE_ENTRY();

    uint64_t max_write_time = desc.WriteTime;
    string build_root = MakeWasmBakerBuildRoot(context, desc.DescriptorPath);
    string source_path = StageWasmBakerInput(files, build_root, desc.SourcePath, max_write_time);

    for (const string& input_path : desc.InputPaths) {
        (void)StageWasmBakerInput(files, build_root, input_path, max_write_time);
    }

    if (context.BakeChecker && !context.BakeChecker(desc.OutputPath, max_write_time)) {
        return;
    }

    string output_path = strex(build_root).combine_path("out").combine_path(desc.OutputPath).str();
    string api_path = strex(api_dir).combine_path(strex("{}.json", GetWasmApiImportTableSideName(desc.TargetSide))).str();
    string bindings_path;

    if (WasmAssemblyScriptBaker::IsFrontend(desc.Frontend)) {
        bindings_path = WriteWasmBakerAssemblyScriptBindings(desc, api_manifest, source_path);
    }

    string compile_command = MakeWasmBakerCompileCommand(desc);
    string expanded_command = ExpandWasmBakerCommand(compile_command, context, desc, build_root, api_path, api_dir, source_path, output_path, bindings_path);

    bool output_dir_ok = fs_create_directories(strex(output_path).extract_dir());
    FO_VERIFY_AND_THROW(output_dir_ok, "Unable to create WASM compiler output directory", desc.DescriptorPath, output_path);

    WriteLog("Compile WASM script {}", desc.DescriptorPath);

    int32_t command_result = std::system(expanded_command.c_str());

    if (command_result != 0) {
        throw WasmBakerException(strex("WASM compiler command failed for '{}' with exit code {}", desc.DescriptorPath, command_result));
    }

    optional<string> output_data = fs_read_file(output_path);

    if (!output_data.has_value()) {
        throw WasmBakerException(strex("WASM compiler command for '{}' did not produce '{}'", desc.DescriptorPath, output_path));
    }

    context.WriteData(desc.OutputPath, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(output_data->data()), output_data->size()});

    string output_map_path = output_path + ".map";
    optional<string> output_map_data = fs_read_file(output_map_path);

    if (output_map_data.has_value()) {
        string resource_map_path = desc.OutputPath + ".map";

        if (!context.BakeChecker || context.BakeChecker(resource_map_path, max_write_time)) {
            context.WriteData(resource_map_path, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(output_map_data->data()), output_map_data->size()});
        }
    }
}

WasmBaker::WasmBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME)
{
    FO_STACK_TRACE_ENTRY();
}

WasmBaker::~WasmBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void WasmBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    vector<File> wasm_files;
    vector<File> descriptor_files;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            string ext = strex(file_header.GetPath()).get_file_extension();

            if (ext == "wasm") {
                if (_context->BakeChecker && !_context->BakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
                    continue;
                }

                wasm_files.emplace_back(File::Load(file_header));
                continue;
            }

            if (ext == "fowasm") {
                descriptor_files.emplace_back(File::Load(file_header));
                continue;
            }
        }
    }
    else {
        string ext = strex(target_path).get_file_extension();

        if (ext != "wasm" && ext != "fowasm") {
            return;
        }

        File file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }

        if (ext == "wasm") {
            if (_context->BakeChecker && !_context->BakeChecker(file.GetPath(), file.GetWriteTime())) {
                return;
            }

            wasm_files.emplace_back(std::move(file));
        }
        else {
            descriptor_files.emplace_back(std::move(file));
        }
    }

    for (const File& file : wasm_files) {
        _context->WriteData(file.GetPath(), file.GetData());
    }

    if (descriptor_files.empty()) {
        return;
    }

    vector<WasmCompileDescriptor> descriptors;
    descriptors.reserve(descriptor_files.size());

    for (const File& file : descriptor_files) {
        descriptors.emplace_back(ParseWasmBakerDescriptor(file));
    }

    bool need_server = std::ranges::any_of(descriptors, [](const WasmCompileDescriptor& desc) { return desc.TargetSide == EngineSideKind::ServerSide; });
    bool need_client = std::ranges::any_of(descriptors, [](const WasmCompileDescriptor& desc) { return desc.TargetSide == EngineSideKind::ClientSide; });
    bool need_mapper = std::ranges::any_of(descriptors, [](const WasmCompileDescriptor& desc) { return desc.TargetSide == EngineSideKind::MapperSide; });
    bool need_compiler_files = std::ranges::any_of(descriptors, [](const WasmCompileDescriptor& desc) { return ShouldCompileWasmBakerDescriptor(desc); });
    string api_dir = MakeWasmBakerApiBuildDir(*_context);
    map<EngineSideKind, vector<uint8_t>> manifests;

    auto write_manifest = [&](EngineSideKind side, const vector<uint8_t>& data) {
        uint64_t max_write_time = 0;

        for (const WasmCompileDescriptor& desc : descriptors) {
            if (desc.TargetSide == side) {
                max_write_time = std::max(max_write_time, desc.WriteTime);
            }
        }

        string api_path = MakeWasmBakerApiPath(side);

        if (!_context->BakeChecker || _context->BakeChecker(api_path, max_write_time)) {
            _context->WriteData(api_path, data);
        }

        if (need_compiler_files) {
            WriteWasmBakerManifestForCompiler(api_dir, side, data);
        }
    };

    if (need_server) {
        BakerServerEngine engine(*_context->BakedFiles);
        vector<uint8_t> data = BuildWasmBakerApiManifest(engine);
        write_manifest(EngineSideKind::ServerSide, data);
        manifests.emplace(EngineSideKind::ServerSide, std::move(data));
    }
    if (need_client) {
        BakerClientEngine engine(*_context->BakedFiles);
        vector<uint8_t> data = BuildWasmBakerApiManifest(engine);
        write_manifest(EngineSideKind::ClientSide, data);
        manifests.emplace(EngineSideKind::ClientSide, std::move(data));
    }
    if (need_mapper) {
        BakerMapperEngine engine(*_context->BakedFiles);
        vector<uint8_t> data = BuildWasmBakerApiManifest(engine);
        write_manifest(EngineSideKind::MapperSide, data);
        manifests.emplace(EngineSideKind::MapperSide, std::move(data));
    }

    for (const WasmCompileDescriptor& desc : descriptors) {
        if (!ShouldCompileWasmBakerDescriptor(desc)) {
            continue;
        }

        auto manifest_it = manifests.find(desc.TargetSide);
        FO_VERIFY_AND_THROW(manifest_it != manifests.end(), "WASM API manifest is missing for descriptor target", desc.DescriptorPath, GetWasmApiImportTableSideName(desc.TargetSide));

        CompileWasmBakerDescriptor(files, *_context, desc, api_dir, manifest_it->second);
    }
}

FO_END_NAMESPACE

#endif
