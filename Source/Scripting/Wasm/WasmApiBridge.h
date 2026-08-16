//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/ /_/\__, /_/_/ /_/\___/
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
#include "WasmImports.h"

#if FO_WASM_SCRIPTING

FO_BEGIN_NAMESPACE

class EngineMetadata;
class BaseEngine;
class Property;

static constexpr string_view WASM_ENGINE_API_MODULE = "fonline.api";

enum class WasmApiReceiverKind : uint8_t
{
    None,
    Entity,
    FixedType,
    RefType,
};

enum class WasmApiParamAbiKind : int32_t
{
    Scalar = 0,
    Utf8StringPointer = 1,
    Utf8StringLength = 2,
    Utf8StringOutputPointer = 3,
    Utf8StringOutputLength = 4,
    MutableValuePointer = 5,
    MutableValueLength = 6,
    ArrayPointer = 7,
    ArrayByteLength = 8,
    ArrayOutputPointer = 9,
    ArrayOutputByteLength = 10,
    MutableArrayPointer = 11,
    MutableArrayByteLength = 12,
    MutableArrayCapacityByteLength = 13,
    MutableArrayRequiredByteLengthPointer = 14,
    DictPointer = 15,
    DictByteLength = 16,
    DictOutputPointer = 17,
    DictOutputByteLength = 18,
    MutableDictPointer = 19,
    MutableDictByteLength = 20,
    MutableDictCapacityByteLength = 21,
    MutableDictRequiredByteLengthPointer = 22,
    CallbackPointer = 23,
    CallbackLength = 24,
    MutableUtf8StringPointer = 25,
    MutableUtf8StringByteLength = 26,
    MutableUtf8StringCapacityByteLength = 27,
    MutableUtf8StringRequiredByteLengthPointer = 28,
    ValuePointer = 29,
    ValueByteLength = 30,
    ValueOutputPointer = 31,
    ValueOutputByteLength = 32,
};

enum class WasmApiResultAbiKind : uint8_t
{
    None,
    Scalar,
    Utf8String,
    Value,
    Array,
    Dict,
};

struct WasmApiMethodDesc
{
    string Module {};
    string Name {};
    string EntityName {};
    nptr<const MethodDesc> Method {};
    vector<WasmScalarKind> Args {};
    vector<WasmApiParamAbiKind> ParamAbi {};
    WasmScalarKind Ret {};
    WasmApiResultAbiKind ResultAbi {};
    WasmApiReceiverKind ReceiverKind {};
    bool HasReceiverHandle {};
    bool Supported {};
    string UnsupportedReason {};
};

enum class WasmApiPropertyAccess : uint8_t
{
    Getter,
    Setter,
};

struct WasmApiPropertyDesc
{
    string Module {};
    string Name {};
    string EntityName {};
    nptr<const Property> Prop {};
    WasmApiPropertyAccess Access {};
    array<WasmScalarKind, MAX_CALL_ARGS> Args {};
    array<WasmApiParamAbiKind, MAX_CALL_ARGS> ParamAbi {};
    size_t ArgsCount {};
    WasmScalarKind Ret {};
    WasmApiResultAbiKind ResultAbi {};
    WasmApiReceiverKind ReceiverKind {};
    bool HasReceiverHandle {};
    bool Supported {};
    string UnsupportedReason {};
};

struct WasmApiImportTable
{
    EngineSideKind Side {};
    vector<WasmApiMethodDesc> Methods {};
    vector<WasmApiPropertyDesc> Properties {};
};

[[nodiscard]] auto BuildWasmApiMethodDescs(const EngineMetadata& meta) -> vector<WasmApiMethodDesc>;
[[nodiscard]] auto BuildWasmApiPropertyDescs(const EngineMetadata& meta) -> vector<WasmApiPropertyDesc>;
[[nodiscard]] auto BuildWasmApiImportTable(const EngineMetadata& meta) -> WasmApiImportTable;
[[nodiscard]] auto GetWasmApiImportTableSideName(EngineSideKind side) noexcept -> string_view;
[[nodiscard]] auto CountWasmApiSupportedMethods(const WasmApiImportTable& table) noexcept -> size_t;
[[nodiscard]] auto CountWasmApiSupportedProperties(const WasmApiImportTable& table) noexcept -> size_t;
[[nodiscard]] auto FindWasmApiMethodDesc(const vector<WasmApiMethodDesc>& methods, string_view module_name, string_view import_name) noexcept -> nptr<const WasmApiMethodDesc>;
[[nodiscard]] auto FindWasmApiPropertyDesc(const vector<WasmApiPropertyDesc>& properties, string_view module_name, string_view import_name) noexcept -> nptr<const WasmApiPropertyDesc>;
[[nodiscard]] auto MakeWasmApiImportName(string_view entity_name, const MethodDesc& method, WasmApiReceiverKind receiver_kind = WasmApiReceiverKind::None) -> string;
[[nodiscard]] auto MakeWasmApiNativeSignature(const WasmApiMethodDesc& desc) -> string;
[[nodiscard]] auto MakeWasmApiPropertyImportName(string_view entity_name, const Property& prop, WasmApiPropertyAccess access, WasmApiReceiverKind receiver_kind = WasmApiReceiverKind::None) -> string;
[[nodiscard]] auto MakeWasmApiPropertyNativeSignature(const WasmApiPropertyDesc& desc) -> string;
[[nodiscard]] auto TryResolveWasmApiScalarKind(const ComplexTypeDesc& type) noexcept -> WasmScalarKind;
[[nodiscard]] auto TryResolveWasmApiScalarKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind;
[[nodiscard]] auto ValidateWasmApiMethodSignature(const WasmApiMethodDesc& desc, const_span<WasmScalarKind> args, const_span<WasmScalarKind> results) noexcept -> bool;
[[nodiscard]] auto ValidateWasmApiPropertySignature(const WasmApiPropertyDesc& desc, const_span<WasmScalarKind> args, const_span<WasmScalarKind> results) noexcept -> bool;
void CallWasmApiMethod(BaseEngine& engine, const WasmApiMethodDesc& desc, const_span<uint64_t> raw_args, uint64_t* raw_result);
void CallWasmApiProperty(BaseEngine& engine, const WasmApiPropertyDesc& desc, const_span<uint64_t> raw_args, uint64_t* raw_result);

FO_END_NAMESPACE

#endif
