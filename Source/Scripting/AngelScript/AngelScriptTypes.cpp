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

#include "AngelScriptTypes.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptString.h"
#include "EngineBase.h"
#include "Geometry.h"
#include "TextPack.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

template<typename T>
static void Type_Construct(T* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) T();
}

template<typename T>
static void Type_ConstructCopy(T* self, const T& other)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) T(other);
}

template<typename T>
static auto Type_GetStr(const T& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("{}", self).str();
}

template<typename T>
static auto Type_AnyConv(const T& self) -> any_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return any_t(strex("{}", self).str());
}

template<typename T>
static auto Type_Cmp(const T& self, const T& other) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (self < other) {
        return -1;
    }
    else if (other < self) {
        return 1;
    }

    return 0;
}

template<typename T>
static auto Type_FastCompare(ptr<const void> a, ptr<const void> b) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return Type_Cmp<T>(*cast_from_void<const T*>(a.get()), *cast_from_void<const T*>(b.get()));
}

template<typename T, typename U = T>
static auto Type_Equals(const T& self, const U& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return self == other;
}

static auto GetMutableStructFieldStorage(ptr<void> obj, size_t offset) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<uint8_t> bytes = cast_from_void<uint8_t*>(obj.get_no_const());
    return bytes.get_no_const() + offset;
}

static auto ReadRequiredHandleSlot(ptr<void> slot) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto nullable_obj = NativeDataProvider::ReadHandleSlot(slot);
    FO_STRONG_ASSERT(nullable_obj, "Required handle slot holds a null object");
    return nullable_obj.as_ptr();
}

static void ReturnGenericDynamicRefType(ptr<AngelScript::asIScriptGeneric> gen, refcount_ptr<DynamicRefTypeInstance>&& ref_instance) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) void*(ReleaseScriptOwnership(std::move(ref_instance)));
}

static void ReturnGenericDynamicRefType(ptr<AngelScript::asIScriptGeneric> gen, ptr<DynamicRefTypeInstance> ref_instance) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) DynamicRefTypeInstance*(ref_instance.get());
}

static auto GetGenericAddressArgObject(ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT arg_index) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    return ReadRequiredHandleSlot(GetGenericAddressArg(gen, arg_index));
}

template<typename T>
static auto GetGenericAddressArgObject(ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT arg_index) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<T*>(GetGenericAddressArgObject(gen, arg_index).get());
}

static void DefaultInitStructFields(ptr<void> obj, const BaseTypeDesc& type)
{
    FO_NO_STACK_TRACE_ENTRY();

    MemFill(obj.get(), 0, type.Size);

    if (type.StructLayout) {
        for (const auto& field : type.StructLayout->Fields) {
            auto field_storage = GetMutableStructFieldStorage(obj, field.Offset);

            if (field.Type.IsHashedString) {
                new (field_storage.get()) hstring();
            }
            else if (field.Type.IsStruct) {
                DefaultInitStructFields(field_storage, field.Type);
            }
        }
    }
}

static void GenericType_Construct(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<void>(gen);

    DefaultInitStructFields(obj, *type);
}

static void GenericType_ConstructCopy(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<void>(gen);
    auto other = GetGenericAddressArgObject(gen, 0);

    MemCopy(obj.get(), other.get(), type->Size);
}

static void GenericType_ConstructArgs(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<void>(gen);
    AngelScript::asUINT index = 0;

    VisitBaseTypePrimitive(obj.get(), *type, [&index, &gen](auto&& v) {
        auto arg = GetGenericAddressArgAs<const void>(gen, index);
        MemCopy(&v, arg.get(), sizeof(v));
        index++;
    });
}

static void GenericType_GetStr(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<const void>(gen);

    string str;
    VisitBaseTypePrimitive(obj.get(), *type, [&str](auto&& v) {
        if (!str.empty()) {
            str += " ";
        }

        str += strex("{}", v);
    });

    new (gen->GetAddressOfReturnLocation()) string(std::move(str));
}

static void GenericType_AnyConv(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<const void>(gen);

    any_t str;
    VisitBaseTypePrimitive(obj.get(), *type, [&str](auto&& v) {
        if (!str.empty()) {
            str += " ";
        }

        str += strex("{}", v);
    });

    new (gen->GetAddressOfReturnLocation()) any_t(std::move(str));
}

static void GenericType_AnyConvRev(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<const any_t>(gen);
    const auto tokens = strvex(*obj).split(' ');
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);

    ptr<void> result = gen->GetAddressOfReturnLocation();
    size_t index = 0;

    VisitBaseTypePrimitive(result.get(), *type, [&index, &tokens, meta](auto&& v) {
        using T = std::decay_t<decltype(v)>;

        if (index >= tokens.size()) {
            throw ScriptException("Invalid cast to any (values less then needed)");
        }

        if constexpr (std::is_same_v<T, bool>) {
            v = strvex(tokens[index]).to_bool();
        }
        else if constexpr (std::is_integral_v<T>) {
            v = numeric_cast<T>(strvex(tokens[index]).to_int64());
        }
        else if constexpr (std::is_floating_point_v<T>) {
            v = numeric_cast<T>(strvex(tokens[index]).to_float64());
        }
        else if constexpr (std::is_same_v<T, hstring>) {
            v = meta->Hashes.ToHashedString(tokens[index]);
        }

        index++;
    });

    if (index != tokens.size()) {
        throw ScriptException("Invalid cast to any (values more then needed)");
    }
}

static void GenericType_Cmp(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<const void>(gen);
    auto other = GetGenericAddressArgObject(gen, 0);

    int32_t cmp = 0;

    if (VisitBaseTypePrimitive(obj, other, *type, [](auto&& x, auto&& y) -> bool { return x < y; })) {
        cmp = -1;
    }
    else if (VisitBaseTypePrimitive(other, obj, *type, [](auto&& x, auto&& y) -> bool { return x < y; })) {
        cmp = 1;
    }

    new (gen->GetAddressOfReturnLocation()) int32_t(cmp);
}

static void GenericType_Equals(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    auto obj = GetGenericObjectAs<const void>(gen);
    auto other = GetGenericAddressArgObject(gen, 0);

    const auto equals = MemCompare(obj.get(), other.get(), type->Size);
    new (gen->GetAddressOfReturnLocation()) bool(equals);
}

static void GenericType_GetZero(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = GetGenericAuxiliaryAs<const BaseTypeDesc>(gen);
    ptr<void> result = gen->GetAddressOfReturnLocation();
    VisitBaseTypePrimitive(result.get(), *type, [](auto&& v) { v = {}; });
}

static void HashedString_Construct(hstring* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) hstring();
}

static void HashedString_Destruct(hstring* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    self->~hstring();
}

static void HashedString_ConstructCopy(hstring* self, const hstring& other)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) hstring(other);
}

static void HashedString_Assign(hstring& self, const hstring& other)
{
    FO_NO_STACK_TRACE_ENTRY();

    self = other;
}

template<typename T>
static auto HstringWrapper_HstringCast(const T& self) -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.underlying_value();
}

template<typename T>
static auto HstringWrapper_StringCast(const T& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("{}", self);
}

template<typename T>
static auto HstringWrapper_AnyConv(const T& self) -> any_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return any_t(strex("{}", self).str());
}

template<typename T>
static void HstringWrapper_AnyConvRev(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto self = GetGenericObjectAs<const any_t>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    new (gen->GetAddressOfReturnLocation()) T(meta->Hashes.ToHashedString(*self));
}

static void TextPackKey_ConstructFromGen(AngelScript::asIScriptGeneric* gen, bool hstring_key1)
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    auto self = GetGenericObjectAs<TextPackKey>(gen);
    auto collection = GetGenericAddressArgObject<const TextPackName>(gen, 0);
    const auto arg_count = gen->GetArgCount();

    string_view key1;
    string_view key2;
    string_view key3;

    if (hstring_key1) {
        auto key = GetGenericAddressArgObject<const hstring>(gen, 1);
        key1 = key->as_str();
    }
    else {
        auto key = GetGenericAddressArgObject<const string>(gen, 1);
        key1 = *key;
    }

    if (arg_count >= 3) {
        auto key = GetGenericAddressArgObject<const string>(gen, 2);
        key2 = *key;
    }
    if (arg_count >= 4) {
        auto key = GetGenericAddressArgObject<const string>(gen, 3);
        key3 = *key;
    }

    new (self.get()) TextPackKey(TextPackKey::FromPack(meta->Hashes, collection->underlying_value(), key1, key2, key3));
}

static void TextPackKey_Construct1(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    TextPackKey_ConstructFromGen(gen, false);
}

static void TextPackKey_ConstructH1(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    TextPackKey_ConstructFromGen(gen, true);
}

static void TextPackKey_Construct2(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    TextPackKey_ConstructFromGen(gen, false);
}

static void TextPackKey_ConstructH2(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    TextPackKey_ConstructFromGen(gen, true);
}

static void TextPackKey_Construct3(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    TextPackKey_ConstructFromGen(gen, false);
}

static void TextPackKey_ConstructH3(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    TextPackKey_ConstructFromGen(gen, true);
}

static auto HashedString_EqualsString(const hstring& self, const string& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_str() == other;
}

static auto HashedString_StringCast(const hstring& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return string(self.as_str());
}

static auto HashedString_StringConv(const hstring& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return string(self.as_str());
}

static auto HashedString_GetString(const hstring& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return string(self.as_str());
}

static auto HashedString_GetHash(const hstring& self) -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_int64();
}

static void String_ToHashedString(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto str = GetGenericObjectAs<const string>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    const auto hstr = meta->Hashes.ToHashedString(*str);
    new (gen->GetAddressOfReturnLocation()) hstring(hstr);
}

static auto HashedString_GetUHash(const hstring& self) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_uint64();
}

static void Any_Construct(any_t* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) any_t();
}

static void Any_Destruct(any_t* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    self->~any_t();
}

template<typename T>
static void Any_ConstructFrom(any_t* self, const T& other)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) any_t(strex("{}", other).str());
}

static void Any_ConstructCopy(any_t* self, const any_t& other)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) any_t(other);
}

static auto Any_Assign(any_t& self, const any_t& other) -> any_t&
{
    FO_NO_STACK_TRACE_ENTRY();

    self = other;
    return self;
}

static auto Any_MakeEnumValue(ptr<const EngineMetadata> meta, string_view enum_name, int32_t enum_value) -> any_t
{
    FO_NO_STACK_TRACE_ENTRY();

    bool failed = false;
    const string_view enum_value_name = meta->ResolveEnumValueName(enum_name, enum_value, &failed);

    if (failed) {
        throw ScriptException("Invalid enum value for any conversion", enum_name, enum_value);
    }

    return any_t {strex("{}::{}", enum_name, enum_value_name).str()};
}

static void Any_ConstructFromEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    auto enum_name = GetGenericAuxiliaryAs<const string>(gen);
    auto enum_value_ptr = GetGenericAddressArgAs<const void>(gen, 0);
    const auto enum_value = ReadEnumValueAsInt32(enum_value_ptr, meta->GetBaseType(*enum_name));
    auto self = GetGenericObjectAs<any_t>(gen);

    new (self.get()) any_t(Any_MakeEnumValue(meta, *enum_name, enum_value));
}

static auto Any_ResolveEnumValue(const any_t& self, ptr<const EngineMetadata> meta, string_view enum_name) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto self_view = string_view {self};
    bool failed = false;
    int32_t enum_value = 0;

    if (const auto sep_pos = self_view.find("::"); sep_pos != string_view::npos) {
        const auto parsed_enum_name = self_view.substr(0, sep_pos);
        const auto parsed_value_name = self_view.substr(sep_pos + 2);

        if (parsed_enum_name != enum_name) {
            throw ScriptException("Invalid enum type for any conversion", enum_name, self_view);
        }

        enum_value = meta->ResolveEnumValue(enum_name, parsed_value_name, &failed);
    }
    else {
        enum_value = meta->ResolveEnumValue(enum_name, self_view, &failed);
    }

    if (failed) {
        throw ScriptException("Invalid enum value for any conversion", enum_name, self_view);
    }

    return enum_value;
}

static void Any_ConvEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    auto enum_name = GetGenericAuxiliaryAs<const string>(gen);
    auto self = GetGenericObjectAs<const any_t>(gen);
    const auto enum_value = Any_ResolveEnumValue(*self, meta, *enum_name);

    ptr<void> return_value = gen->GetAddressOfReturnLocation();
    WriteEnumValueFromInt32(return_value, meta->GetBaseType(*enum_name), enum_value);
}

template<typename T>
static auto Any_Conv(const any_t& self) -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (std::same_as<T, bool>) {
        return strvex(self).to_bool();
    }
    else if constexpr (some_strong_type<T>) {
        return T {numeric_cast<typename T::underlying_type>(strvex(self).to_int64())};
    }
    else if constexpr (std::integral<T>) {
        if (strvex(self).is_explicit_bool()) {
            return strvex(self).to_bool() ? 1 : 0;
        }
        return numeric_cast<T>(strvex(self).to_int64());
    }
    else if constexpr (std::floating_point<T>) {
        return numeric_cast<T>(strvex(self).to_float64());
    }
    else if constexpr (std::same_as<T, string>) {
        return self;
    }
    else {
        T value = {};
        istringstream istr(self);
        istr >> value;
        return value;
    }
}

template<typename T>
static void Any_ConvGen(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto self = GetGenericObjectAs<const any_t>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);

    if constexpr (std::integral<T>) {
        const auto self_view = string_view {*self};

        T result;

        if (self_view.empty()) {
            result = T {};
        }
        else if (strvex(self_view).is_explicit_bool()) {
            result = strvex(self_view).to_bool() ? 1 : 0;
        }
        else if (strvex(self_view).is_number()) {
            result = numeric_cast<T>(strvex(self_view).to_int64());
        }
        else {
            bool failed = false;
            const auto resolved = meta->ResolveEnumValue(self_view, &failed);

            if (failed) {
                throw ScriptException("Invalid int value for any conversion", self_view);
            }

            result = numeric_cast<T>(resolved);
        }

        new (gen->GetAddressOfReturnLocation()) T(result);
    }
    else if constexpr (std::same_as<T, hstring>) {
        new (gen->GetAddressOfReturnLocation()) hstring(meta->Hashes.ToHashedString(*self));
    }
    else {
        static_assert(always_false_v<T>, "Unsupported type for any conversion");
    }
}

template<typename T>
static auto Any_ConvFrom(const T& self) -> any_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return any_t(strex("{}", self).str());
}

static void Ucolor_ConstructRawRgba(ucolor* self, uint32_t rgba)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) ucolor {rgba};
}

static void Ucolor_ConstructRgba(ucolor* self, int32_t r, int32_t g, int32_t b, int32_t a)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto clamped_r = numeric_cast<uint8_t>(std::clamp(r, 0, 255));
    const auto clamped_g = numeric_cast<uint8_t>(std::clamp(g, 0, 255));
    const auto clamped_b = numeric_cast<uint8_t>(std::clamp(b, 0, 255));
    const auto clamped_a = numeric_cast<uint8_t>(std::clamp(a, 0, 255));

    new (self) ucolor {clamped_r, clamped_g, clamped_b, clamped_a};
}

template<typename T>
static void Time_ConstructWithPlace(T* self, int64_t value, int32_t place)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (place == 0) {
        new (self) T {std::chrono::nanoseconds {value}};
    }
    else if (place == 1) {
        new (self) T {std::chrono::microseconds {value}};
    }
    else if (place == 2) {
        new (self) T {std::chrono::milliseconds {value}};
    }
    else if (place == 3) {
        new (self) T {std::chrono::seconds {value}};
    }
    else {
        throw ScriptException("Invalid time place", place);
    }
}

static void Ipos_ConstructXandY(ipos32* self, int32_t x, int32_t y)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) ipos32 {x, y};
}

static auto Ipos_AddAssignIpos(ipos32& self, const ipos32& pos) -> ipos32&
{
    FO_NO_STACK_TRACE_ENTRY();

    self.x += pos.x;
    self.y += pos.y;
    return self;
}

static auto Ipos_SubAssignIpos(ipos32& self, const ipos32& pos) -> ipos32&
{
    FO_NO_STACK_TRACE_ENTRY();

    self.x -= pos.x;
    self.y -= pos.y;
    return self;
}

static auto Ipos_AddIpos(const ipos32& self, const ipos32& pos) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return ipos32 {self.x + pos.x, self.y + pos.y};
}

static auto Ipos_SubIpos(const ipos32& self, const ipos32& pos) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return ipos32 {self.x - pos.x, self.y - pos.y};
}

static auto Ipos_NegIpos(const ipos32& self) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return ipos32 {-self.x, -self.y};
}

static auto Ipos_AddAssignIsize(ipos32& self, const isize32& size) -> ipos32&
{
    FO_NO_STACK_TRACE_ENTRY();

    self.x += size.width;
    self.y += size.height;
    return self;
}

static auto Ipos_SubAssignIsize(ipos32& self, const isize32& size) -> ipos32&
{
    FO_NO_STACK_TRACE_ENTRY();

    self.x -= size.width;
    self.y -= size.height;
    return self;
}

static auto Ipos_AddIsize(const ipos32& self, const isize32& size) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return ipos32 {self.x + size.width, self.y + size.height};
}

static auto Ipos_SubIsize(const ipos32& self, const isize32& size) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return ipos32 {self.x - size.width, self.y - size.height};
}

static auto Ipos_FitToSize(const ipos32& self, isize32 size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.x >= 0 && self.y >= 0 && self.x < size.width && self.y < size.height;
}

static auto Ipos_FitToRect(const ipos32& self, irect32 rect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.x >= rect.x && self.y >= rect.y && self.x < rect.x + rect.width && self.y < rect.y + rect.height;
}

static void Isize_ConstructWandH(isize32* self, int32_t width, int32_t height)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) isize32 {width, height};
}

static void Irect_ConstructXandYandWandH(irect32* self, int32_t x, int32_t y, int32_t width, int32_t height)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) irect32 {x, y, width, height};
}

static void Fpos_ConstructXandY(fpos32* self, float32_t x, float32_t y)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) fpos32 {x, y};
}

static auto Fpos_AddAssignFpos(fpos32& self, const fpos32& pos) -> fpos32&
{
    FO_NO_STACK_TRACE_ENTRY();

    self.x += pos.x;
    self.y += pos.y;
    return self;
}

static auto Fpos_SubAssignFpos(fpos32& self, const fpos32& pos) -> fpos32&
{
    FO_NO_STACK_TRACE_ENTRY();

    self.x -= pos.x;
    self.y -= pos.y;
    return self;
}

static auto Fpos_AddFpos(const fpos32& self, const fpos32& pos) -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return fpos32 {self.x + pos.x, self.y + pos.y};
}

static auto Fpos_SubFpos(const fpos32& self, const fpos32& pos) -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return fpos32 {self.x - pos.x, self.y - pos.y};
}

static auto Fpos_NegFpos(const fpos32& self) -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return fpos32 {-self.x, -self.y};
}

static void Fsize_ConstructWandH(fsize32* self, float32_t width, float32_t height)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) fsize32 {width, height};
}

static void Mpos_ConstructXandY(mpos* self, int32_t x, int32_t y)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) mpos {numeric_cast<int16_t>(x), numeric_cast<int16_t>(y)};
}

static auto Mpos_FitToSize(const mpos& self, msize size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return size.is_valid_pos(self);
}

static void Hdir_ConstructValue(hdir* self, int32_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) hdir(value);
}

static auto Hdir_GetValue(const hdir& self) -> int8_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.value();
}

static void Hdir_SetValue(hdir& self, int8_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    self = hdir(value);
}

static auto Hdir_ToMdir(const hdir& self) -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return mdir(self);
}

static void Global_GetRandomHdir(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    static thread_local std::mt19937 rng {std::random_device {}()};
    const auto dir = hdir(static_cast<int8_t>(rng() % GameSettings::MAP_DIR_COUNT));
    new (gen->GetAddressOfReturnLocation()) hdir(dir);
}

static auto Mdir_GetAngle(const mdir& self) -> int16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.angle();
}

static void Mdir_SetAngle(mdir& self, int16_t angle)
{
    FO_NO_STACK_TRACE_ENTRY();

    self = mdir(angle);
}

static void Mdir_ConstructAngle(mdir* self, int32_t angle)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) mdir(numeric_cast<int16_t>(angle));
}

static void Mdir_ConstructHdir(mdir* self, hdir dir)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) mdir(dir);
}

static auto Mdir_GetHex(const mdir& self) -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.hex();
}

static auto Mdir_IncHex(const mdir& self) -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.incHex();
}

static auto Mdir_DecHex(const mdir& self) -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.decHex();
}

static auto Mdir_RotateHex(const mdir& self, int32_t steps) -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.rotateHex(steps);
}

static auto Mdir_Reverse(const mdir& self) -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.reverse();
}

static void RefType_Factory(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto method = GetGenericAuxiliaryAs<const MethodDesc>(gen);
    FO_VERIFY_AND_THROW(method->Call, "Method call function is null");

    ScriptGenericCall(gen, false, [&](FuncCallData& call) { method->Call(call); });
}

static void RefType_MethodCall(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto method = GetGenericAuxiliaryAs<const MethodDesc>(gen);
    FO_VERIFY_AND_THROW(method->Call, "Method call function is null");

    ScriptGenericCall(gen, true, [&](FuncCallData& call) { method->Call(call); });
}

static void RefType_Equals(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto obj = GetGenericObjectAs<const void>(gen);
    auto other = GetGenericAddressArgObject(gen, 0);

    new (gen->GetAddressOfReturnLocation()) bool(obj == other);
}

static void DynamicRefType_AddRef(const DynamicRefTypeInstance* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(self != nullptr, "Script object instance is null");
    ptr<const DynamicRefTypeInstance> self_ref = self;
    self_ref->AddRef();
}

static void DynamicRefType_Release(const DynamicRefTypeInstance* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(self != nullptr, "Script object instance is null");
    ptr<const DynamicRefTypeInstance> self_ref = self;
    self_ref->Release();
}

static void DynamicRefType_Factory(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto registrator = GetGenericAuxiliaryAs<const PropertyRegistrator>(gen);

    auto ref_instance = SafeAlloc::MakeRefCounted<DynamicRefTypeInstance>(registrator);
    ReturnGenericDynamicRefType(gen, std::move(ref_instance));
}

static void DynamicRefType_GetProperty(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto self = GetGenericObjectAs<DynamicRefTypeInstance>(gen);
    auto prop = GetGenericAuxiliaryAs<const Property>(gen);

    PropertyRawData prop_data;
    prop_data.Pass(self->GetRawData(prop));
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    ConvertPropsToScriptObject(prop, prop_data, gen->GetAddressOfReturnLocation(), as_engine);
}

static void DynamicRefType_GetComponent(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto self = GetGenericObjectAs<DynamicRefTypeInstance>(gen);

    ReturnGenericDynamicRefType(gen, self);
}

static void DynamicRefType_SetProperty(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto self = GetGenericObjectAs<DynamicRefTypeInstance>(gen);
    auto prop = GetGenericAuxiliaryAs<const Property>(gen);

    auto prop_data = ConvertScriptToPropsObject(prop, GetGenericAddressArg(gen, 0));
    self->SetValue(prop, prop_data);
}

template<typename T>
static void Global_GetConstant(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto value = GetGenericAuxiliaryAs<const T>(gen);
    new (gen->GetAddressOfReturnLocation()) T(*value);
}

void RegisterAngelScriptTypes(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto backend = GetScriptBackend(as_engine);
    nptr<const EngineMetadata> meta = backend->GetMetadata();

    // Register hstring
    FO_AS_VERIFY(as_engine->RegisterObjectType("hstring", sizeof(hstring), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<hstring>()));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(HashedString_Construct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_CONSTRUCT, "void f(const hstring &in)", FO_SCRIPT_FUNC_THIS(HashedString_ConstructCopy), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_DESTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(HashedString_Destruct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "hstring& opAssign(const hstring &in)", FO_SCRIPT_FUNC_THIS(HashedString_Assign), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "int opCmp(const hstring &in) const", FO_SCRIPT_FUNC_THIS(Type_Cmp<hstring>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "bool opEquals(const hstring &in) const", FO_SCRIPT_FUNC_THIS(Type_Equals<hstring>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "bool opEquals(const string &in) const", FO_SCRIPT_FUNC_THIS(HashedString_EqualsString), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "string opImplCast() const", FO_SCRIPT_FUNC_THIS(HashedString_StringCast), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "string opImplConv() const", FO_SCRIPT_FUNC_THIS(HashedString_StringConv), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "string get_str() const", FO_SCRIPT_FUNC_THIS(HashedString_GetString), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "int64 get_hash() const", FO_SCRIPT_FUNC_THIS(HashedString_GetHash), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "uint64 get_uhash() const", FO_SCRIPT_FUNC_THIS(HashedString_GetUHash), FO_SCRIPT_FUNC_THIS_CONV));
    static constexpr hstring empty_hstring;
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hstring get_EMPTY_HSTRING()", FO_SCRIPT_GENERIC(Global_GetConstant<hstring>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&empty_hstring)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("string", "hstring hstr() const", FO_SCRIPT_GENERIC(String_ToHashedString), FO_SCRIPT_GENERIC_CONV));

    // Register any
    FO_AS_VERIFY(as_engine->RegisterObjectType("any", sizeof(any_t), AngelScript::asOBJ_VALUE | AngelScript::asGetTypeTraits<any_t>()));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Any_Construct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const any &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructCopy), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const bool &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<bool>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int8 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int8_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint8 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint8_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int16 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int16_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint16 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint16_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int32_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint32_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int64 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int64_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint64 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint64_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const float &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<float32_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const double &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<float64_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_DESTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Any_Destruct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "any& opAssign(const any &in)", FO_SCRIPT_FUNC_THIS(Any_Assign), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "bool opEquals(const any &in) const", FO_SCRIPT_FUNC_THIS(Type_Equals<any_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "bool opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<bool>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int8 opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<int8_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint8 opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<uint8_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int16 opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<int16_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint16 opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<uint16_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<int32_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<uint32_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int64 opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<int64_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint64 opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<uint64_t>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "float opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<float32_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "double opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<float64_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "string opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<string>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "hstring opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<hstring>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("string", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_ConvFrom<string>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_ConvFrom<hstring>), FO_SCRIPT_FUNC_THIS_CONV));

    for (const auto& enum_name : meta->GetAllEnums() | std::views::keys) {
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, strex("void f({} value)", enum_name).c_str(), FO_SCRIPT_GENERIC(Any_ConstructFromEnum), FO_SCRIPT_GENERIC_CONV, cast_to_void(&enum_name)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", strex("{} opImplConv() const", enum_name).c_str(), FO_SCRIPT_GENERIC(Any_ConvEnum), FO_SCRIPT_GENERIC_CONV, cast_to_void(&enum_name)));
    }

    RegisterAngelScriptStringAnyExtensions(as_engine);

    // Built-in value types
    unordered_set<string> registered_types;

    const auto register_engine_type = [&]<typename T>(string_view name, AngelScript::asDWORD class_flags = AngelScript::asOBJ_APP_CLASS_ALLINTS) {
        const string name_str {name};

        registered_types.emplace(name_str);
        FO_AS_VERIFY(as_engine->RegisterObjectType(name_str.c_str(), sizeof(T), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | class_flags | AngelScript::asGetTypeTraits<T>()));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name_str.c_str(), AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Type_Construct<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name_str.c_str(), AngelScript::asBEHAVE_CONSTRUCT, strex("void f(const {}&in other)", name).c_str(), FO_SCRIPT_FUNC_THIS(Type_ConstructCopy<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("int opCmp(const {}&in other) const", name).c_str(), FO_SCRIPT_FUNC_THIS(Type_Cmp<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("bool opEquals(const {}&in other) const", name).c_str(), FO_SCRIPT_FUNC_THIS(Type_Equals<T>), FO_SCRIPT_FUNC_THIS_CONV));
        nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoByName(name_str.c_str());
        FO_VERIFY_AND_THROW(!!type_info, "Missing type info for just-registered value type");
        SetScriptTypeFastCompare(type_info.as_ptr(), &Type_FastCompare<T>);
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), "string get_str() const", FO_SCRIPT_FUNC_THIS(Type_GetStr<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Type_AnyConv<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", strex("{} opImplConv() const", name).c_str(), FO_SCRIPT_FUNC_THIS(Any_Conv<T>), FO_SCRIPT_FUNC_THIS_CONV));
        static constexpr T ZERO_VALUE;
        FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("{} get_ZERO_{}()", name, strex(name).upper()).c_str(), FO_SCRIPT_GENERIC(Global_GetConstant<T>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&ZERO_VALUE)));
    };

    register_engine_type.operator()<ident_t>("ident");
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ident", "int64 value", 0 /*offsetof(ident_t, _value)*/));

    register_engine_type.operator()<ucolor>("ucolor");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("ucolor", AngelScript::asBEHAVE_CONSTRUCT, "void f(uint rgba)", FO_SCRIPT_FUNC_THIS(Ucolor_ConstructRawRgba), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("ucolor", AngelScript::asBEHAVE_CONSTRUCT, "void f(int r, int g, int b, int a = 255)", FO_SCRIPT_FUNC_THIS(Ucolor_ConstructRgba), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ucolor", "uint8 red", offsetof(ucolor, comp.r)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ucolor", "uint8 green", offsetof(ucolor, comp.g)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ucolor", "uint8 blue", offsetof(ucolor, comp.b)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ucolor", "uint8 alpha", offsetof(ucolor, comp.a)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ucolor", "uint value", offsetof(ucolor, rgba)));

    register_engine_type.operator()<timespan>("timespan");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("timespan", AngelScript::asBEHAVE_CONSTRUCT, "void f(int64 value, int place)", FO_SCRIPT_FUNC_THIS((Time_ConstructWithPlace<timespan>)), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "timespan& opAddAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(timespan, operator+=, (const timespan&), timespan&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "timespan& opSubAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(timespan, operator-=, (const timespan&), timespan&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "timespan opAdd(const timespan &in) const", FO_SCRIPT_METHOD_EXT(timespan, operator+, (const timespan&) const, timespan), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "timespan opSub(const timespan &in) const", FO_SCRIPT_METHOD_EXT(timespan, operator-, (const timespan&) const, timespan), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_nanoseconds() const", FO_SCRIPT_METHOD_EXT(timespan, nanoseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_microseconds() const", FO_SCRIPT_METHOD_EXT(timespan, microseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_milliseconds() const", FO_SCRIPT_METHOD_EXT(timespan, milliseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_seconds() const", FO_SCRIPT_METHOD_EXT(timespan, seconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));

    register_engine_type.operator()<nanotime>("nanotime");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("nanotime", AngelScript::asBEHAVE_CONSTRUCT, "void f(int64 value, int place)", FO_SCRIPT_FUNC_THIS((Time_ConstructWithPlace<nanotime>)), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime& opAddAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(nanotime, operator+=, (const timespan&), nanotime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime& opSubAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(nanotime, operator-=, (const timespan&), nanotime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime opAdd(const timespan &in) const", FO_SCRIPT_METHOD_EXT(nanotime, operator+, (const timespan&) const, nanotime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime opSub(const timespan &in) const", FO_SCRIPT_METHOD_EXT(nanotime, operator-, (const timespan&) const, nanotime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "timespan opSub(const nanotime &in) const", FO_SCRIPT_METHOD_EXT(nanotime, operator-, (const nanotime&) const, timespan), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_nanoseconds() const", FO_SCRIPT_METHOD_EXT(nanotime, nanoseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_microseconds() const", FO_SCRIPT_METHOD_EXT(nanotime, microseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_milliseconds() const", FO_SCRIPT_METHOD_EXT(nanotime, milliseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_seconds() const", FO_SCRIPT_METHOD_EXT(nanotime, seconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "timespan get_timeSinceEpoch() const", FO_SCRIPT_METHOD_EXT(nanotime, duration_value, () const, timespan), FO_SCRIPT_METHOD_CONV));

    register_engine_type.operator()<synctime>("synctime");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("synctime", AngelScript::asBEHAVE_CONSTRUCT, "void f(int64 value, int place)", FO_SCRIPT_FUNC_THIS((Time_ConstructWithPlace<synctime>)), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime& opAddAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(synctime, operator+=, (const timespan&), synctime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime& opSubAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(synctime, operator-=, (const timespan&), synctime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime opAdd(const timespan &in) const", FO_SCRIPT_METHOD_EXT(synctime, operator+, (const timespan&) const, synctime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime opSub(const timespan &in) const", FO_SCRIPT_METHOD_EXT(synctime, operator-, (const timespan&) const, synctime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "timespan opSub(const synctime &in) const", FO_SCRIPT_METHOD_EXT(synctime, operator-, (const synctime&) const, timespan), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "int64 get_milliseconds() const", FO_SCRIPT_METHOD_EXT(synctime, milliseconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "int64 get_seconds() const", FO_SCRIPT_METHOD_EXT(synctime, seconds, () const, int64_t), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "timespan get_timeSinceEpoch() const", FO_SCRIPT_METHOD_EXT(synctime, duration_value, () const, timespan), FO_SCRIPT_METHOD_CONV));

    register_engine_type.operator()<ipos32>("ipos");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("ipos", AngelScript::asBEHAVE_CONSTRUCT, "void f(int x, int y)", FO_SCRIPT_FUNC_THIS(Ipos_ConstructXandY), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ipos", "int x", offsetof(ipos32, x)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("ipos", "int y", offsetof(ipos32, y)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos& opAddAssign(const ipos &in)", FO_SCRIPT_FUNC_THIS(Ipos_AddAssignIpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos& opSubAssign(const ipos &in)", FO_SCRIPT_FUNC_THIS(Ipos_SubAssignIpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos opAdd(const ipos &in) const", FO_SCRIPT_FUNC_THIS(Ipos_AddIpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos opSub(const ipos &in) const", FO_SCRIPT_FUNC_THIS(Ipos_SubIpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos opNeg() const", FO_SCRIPT_FUNC_THIS(Ipos_NegIpos), FO_SCRIPT_FUNC_THIS_CONV));

    register_engine_type.operator()<isize32>("isize");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("isize", AngelScript::asBEHAVE_CONSTRUCT, "void f(int width, int height)", FO_SCRIPT_FUNC_THIS(Isize_ConstructWandH), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("isize", "int width", offsetof(isize32, width)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("isize", "int height", offsetof(isize32, height)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos& opAddAssign(const isize &in)", FO_SCRIPT_FUNC_THIS(Ipos_AddAssignIsize), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos& opSubAssign(const isize &in)", FO_SCRIPT_FUNC_THIS(Ipos_SubAssignIsize), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos opAdd(const isize &in) const", FO_SCRIPT_FUNC_THIS(Ipos_AddIsize), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "ipos opSub(const isize &in) const", FO_SCRIPT_FUNC_THIS(Ipos_SubIsize), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "bool fitTo(isize size) const", FO_SCRIPT_FUNC_THIS(Ipos_FitToSize), FO_SCRIPT_FUNC_THIS_CONV));

    register_engine_type.operator()<irect32>("irect");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("irect", AngelScript::asBEHAVE_CONSTRUCT, "void f(int x, int y, int width, int height)", FO_SCRIPT_FUNC_THIS(Irect_ConstructXandYandWandH), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("irect", "int x", offsetof(irect32, x)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("irect", "int y", offsetof(irect32, y)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("irect", "int width", offsetof(irect32, width)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("irect", "int height", offsetof(irect32, height)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("ipos", "bool fitTo(irect rect) const", FO_SCRIPT_FUNC_THIS(Ipos_FitToRect), FO_SCRIPT_FUNC_THIS_CONV));

    register_engine_type.operator()<fpos32>("fpos", AngelScript::asOBJ_APP_CLASS_ALLFLOATS);
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("fpos", AngelScript::asBEHAVE_CONSTRUCT, "void f(float x, float y)", FO_SCRIPT_FUNC_THIS(Fpos_ConstructXandY), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("fpos", "float x", offsetof(fpos32, x)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("fpos", "float y", offsetof(fpos32, y)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos& opAddAssign(const fpos &in)", FO_SCRIPT_FUNC_THIS(Fpos_AddAssignFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos& opSubAssign(const fpos &in)", FO_SCRIPT_FUNC_THIS(Fpos_SubAssignFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos opAdd(const fpos &in) const", FO_SCRIPT_FUNC_THIS(Fpos_AddFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos opSub(const fpos &in) const", FO_SCRIPT_FUNC_THIS(Fpos_SubFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos opNeg() const", FO_SCRIPT_FUNC_THIS(Fpos_NegFpos), FO_SCRIPT_FUNC_THIS_CONV));

    register_engine_type.operator()<fsize32>("fsize", AngelScript::asOBJ_APP_CLASS_ALLFLOATS);
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("fsize", AngelScript::asBEHAVE_CONSTRUCT, "void f(float width, float height)", FO_SCRIPT_FUNC_THIS(Fsize_ConstructWandH), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("fsize", "float width", offsetof(fsize32, width)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("fsize", "float height", offsetof(fsize32, height)));

    register_engine_type.operator()<mpos>("mpos");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("mpos", AngelScript::asBEHAVE_CONSTRUCT, "void f(int x, int y)", FO_SCRIPT_FUNC_THIS(Mpos_ConstructXandY), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("mpos", "int16 x", offsetof(mpos, x)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("mpos", "int16 y", offsetof(mpos, y)));

    register_engine_type.operator()<msize>("msize");
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("msize", "int16 width", offsetof(msize, width)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("msize", "int16 height", offsetof(msize, height)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mpos", "bool fitTo(msize size) const", FO_SCRIPT_FUNC_THIS(Mpos_FitToSize), FO_SCRIPT_FUNC_THIS_CONV));

    registered_types.emplace("hdir");
    FO_AS_VERIFY(as_engine->RegisterObjectType("hdir", sizeof(hdir), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<hdir>()));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hdir", AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Type_Construct<hdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hdir", AngelScript::asBEHAVE_CONSTRUCT, "void f(const hdir&in other)", FO_SCRIPT_FUNC_THIS(Type_ConstructCopy<hdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hdir", AngelScript::asBEHAVE_CONSTRUCT, "void f(int value)", FO_SCRIPT_FUNC_THIS(Hdir_ConstructValue), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hdir", "int8 get_value() const", FO_SCRIPT_FUNC_THIS(Hdir_GetValue), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hdir", "void set_value(int8 value)", FO_SCRIPT_FUNC_THIS(Hdir_SetValue), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hdir", "bool opEquals(const hdir&in other) const", FO_SCRIPT_FUNC_THIS(Type_Equals<hdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hdir", "string get_str() const", FO_SCRIPT_FUNC_THIS(Type_GetStr<hdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hdir", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Type_AnyConv<hdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "hdir opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<hdir>), FO_SCRIPT_FUNC_THIS_CONV));
    static constexpr hdir HDIR_ZERO_VALUE;
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_ZERO_HDIR()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_ZERO_VALUE)));
    static constexpr auto HDIR_NE = hdir::NorthEast;
    static constexpr auto HDIR_E = hdir::East;
    static constexpr auto HDIR_SE = hdir::SouthEast;
    static constexpr auto HDIR_SW = hdir::SouthWest;
    static constexpr auto HDIR_W = hdir::West;
    static constexpr auto HDIR_NW = hdir::NorthWest;
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_NorthEast()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_NE)));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_East()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_E)));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_SouthEast()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_SE)));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_SouthWest()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_SW)));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_West()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_W)));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_NorthWest()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_NW)));
#if FO_GEOMETRY == 2
    static constexpr auto HDIR_S = hdir::South;
    static constexpr auto HDIR_N = hdir::North;
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_South()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_S)));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_North()", FO_SCRIPT_GENERIC(Global_GetConstant<hdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&HDIR_N)));
#endif
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hdir get_HDIR_Random()", FO_SCRIPT_GENERIC(Global_GetRandomHdir), FO_SCRIPT_GENERIC_CONV));

    registered_types.emplace("mdir");
    FO_AS_VERIFY(as_engine->RegisterObjectType("mdir", sizeof(mdir), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<mdir>()));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("mdir", AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Type_Construct<mdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("mdir", AngelScript::asBEHAVE_CONSTRUCT, "void f(const mdir&in other)", FO_SCRIPT_FUNC_THIS(Type_ConstructCopy<mdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("mdir", AngelScript::asBEHAVE_CONSTRUCT, "void f(int angle)", FO_SCRIPT_FUNC_THIS(Mdir_ConstructAngle), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("mdir", AngelScript::asBEHAVE_CONSTRUCT, "void f(hdir dir)", FO_SCRIPT_FUNC_THIS(Mdir_ConstructHdir), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "int16 get_angle() const", FO_SCRIPT_FUNC_THIS(Mdir_GetAngle), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "void set_angle(int16 angle)", FO_SCRIPT_FUNC_THIS(Mdir_SetAngle), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "bool opEquals(const mdir&in other) const", FO_SCRIPT_FUNC_THIS(Type_Equals<mdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "string get_str() const", FO_SCRIPT_FUNC_THIS(Type_GetStr<mdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Type_AnyConv<mdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "mdir opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<mdir>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hdir", "mdir opImplConv() const", FO_SCRIPT_FUNC_THIS(Hdir_ToMdir), FO_SCRIPT_FUNC_THIS_CONV));
    static constexpr mdir MDIR_ZERO_VALUE;
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("mdir get_ZERO_MDIR()", FO_SCRIPT_GENERIC(Global_GetConstant<mdir>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&MDIR_ZERO_VALUE)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "hdir get_hex() const", FO_SCRIPT_FUNC_THIS(Mdir_GetHex), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "mdir incHex() const", FO_SCRIPT_FUNC_THIS(Mdir_IncHex), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "mdir decHex() const", FO_SCRIPT_FUNC_THIS(Mdir_DecHex), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "mdir rotateHex(int steps) const", FO_SCRIPT_FUNC_THIS(Mdir_RotateHex), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("mdir", "mdir reverse() const", FO_SCRIPT_FUNC_THIS(Mdir_Reverse), FO_SCRIPT_FUNC_THIS_CONV));

    // Value types
    const auto register_generic_type = [&](const BaseTypeDesc& type) {
        struct SimpleClass
        {
            int32_t a {};
        };

        FO_AS_VERIFY(as_engine->RegisterObjectType(type.Name.c_str(), numeric_cast<int32_t>(type.Size), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<SimpleClass>()));
    };

    const auto register_metadata_type_common = [&](const BaseTypeDesc& type) {
        const string_view name = type.Name;
        auto layout = type.StructLayout.as_ptr();

        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(type.Name.c_str(), AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_GENERIC(GenericType_Construct), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(type.Name.c_str(), AngelScript::asBEHAVE_CONSTRUCT, strex("void f(const {}&in other)", name).c_str(), FO_SCRIPT_GENERIC(GenericType_ConstructCopy), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));

        string ctor_decl;

        for (size_t i = 0; i < layout->Fields.size(); i++) {
            ptr<const FieldDesc> field = &layout->Fields[i];
            ctor_decl += strex("{}{} {}", i > 0 ? ", " : "", MakeScriptTypeName(field->Type), field->Name);
            FO_AS_VERIFY(as_engine->RegisterObjectProperty(type.Name.c_str(), strex("{} {}", MakeScriptTypeName(field->Type), field->Name).c_str(), numeric_cast<int32_t>(field->Offset)));
        }

        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(type.Name.c_str(), AngelScript::asBEHAVE_CONSTRUCT, strex("void f({})", ctor_decl).c_str(), FO_SCRIPT_GENERIC(GenericType_ConstructArgs), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(type.Name.c_str(), strex("bool opEquals(const {}&in other) const", name).c_str(), FO_SCRIPT_GENERIC(GenericType_Equals), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("{} get_ZERO_{}()", name, strex(name).upper()).c_str(), FO_SCRIPT_GENERIC(GenericType_GetZero), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
    };

    const auto register_generic_type_body = [&](const BaseTypeDesc& type) {
        const string_view name = type.Name;
        register_metadata_type_common(type);

        if (type.IsSimpleStruct) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(type.Name.c_str(), strex("int opCmp(const {}&in other) const", name).c_str(), FO_SCRIPT_GENERIC(GenericType_Cmp), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        }

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(type.Name.c_str(), "string get_str() const", FO_SCRIPT_GENERIC(GenericType_GetStr), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(type.Name.c_str(), "any opImplConv() const", FO_SCRIPT_GENERIC(GenericType_AnyConv), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", strex("{} opImplConv() const", name).c_str(), FO_SCRIPT_GENERIC(GenericType_AnyConvRev), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
    };

    for (const auto& type : meta->GetBaseTypes() | std::views::values) {
        // Types may intersect so register in two steps
        if (type.IsStruct && !registered_types.contains(type.Name)) {
            register_generic_type(type);
        }
    }

    // TextPackName
    {
        const auto& type = meta->GetBaseType("TextPackName");
        registered_types.emplace(type.Name);
        register_metadata_type_common(type);
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "int opCmp(const TextPackName&in other) const", FO_SCRIPT_FUNC_THIS(Type_Cmp<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "string get_str() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_StringCast<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "string opImplCast() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_StringCast<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "string opImplConv() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_StringCast<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "hstring opImplCast() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_HstringCast<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "hstring opImplConv() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_HstringCast<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackName", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_AnyConv<TextPackName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "TextPackName opImplConv() const", FO_SCRIPT_GENERIC(HstringWrapper_AnyConvRev<TextPackName>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
    }

    // LanguageName
    {
        const auto& type = meta->GetBaseType("LanguageName");
        registered_types.emplace(type.Name);
        register_metadata_type_common(type);
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "int opCmp(const LanguageName&in other) const", FO_SCRIPT_FUNC_THIS(Type_Cmp<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "string get_str() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_StringCast<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "string opImplCast() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_StringCast<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "string opImplConv() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_StringCast<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "hstring opImplCast() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_HstringCast<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "hstring opImplConv() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_HstringCast<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("LanguageName", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(HstringWrapper_AnyConv<LanguageName>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "LanguageName opImplConv() const", FO_SCRIPT_GENERIC(HstringWrapper_AnyConvRev<LanguageName>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
    }

    // TextPackKey
    {
        const auto& type = meta->GetBaseType("TextPackKey");
        registered_types.emplace(type.Name);
        register_metadata_type_common(type);
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("TextPackKey", AngelScript::asBEHAVE_CONSTRUCT, "void f(const TextPackName &in collection, const string &in key1)", FO_SCRIPT_GENERIC(TextPackKey_Construct1), FO_SCRIPT_GENERIC_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("TextPackKey", AngelScript::asBEHAVE_CONSTRUCT, "void f(const TextPackName &in collection, const hstring &in key1)", FO_SCRIPT_GENERIC(TextPackKey_ConstructH1), FO_SCRIPT_GENERIC_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("TextPackKey", AngelScript::asBEHAVE_CONSTRUCT, "void f(const TextPackName &in collection, const string &in key1, const string &in key2)", FO_SCRIPT_GENERIC(TextPackKey_Construct2), FO_SCRIPT_GENERIC_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("TextPackKey", AngelScript::asBEHAVE_CONSTRUCT, "void f(const TextPackName &in collection, const hstring &in key1, const string &in key2)", FO_SCRIPT_GENERIC(TextPackKey_ConstructH2), FO_SCRIPT_GENERIC_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("TextPackKey", AngelScript::asBEHAVE_CONSTRUCT, "void f(const TextPackName &in collection, const string &in key1, const string &in key2, const string &in key3)", FO_SCRIPT_GENERIC(TextPackKey_Construct3), FO_SCRIPT_GENERIC_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("TextPackKey", AngelScript::asBEHAVE_CONSTRUCT, "void f(const TextPackName &in collection, const hstring &in key1, const string &in key2, const string &in key3)", FO_SCRIPT_GENERIC(TextPackKey_ConstructH3), FO_SCRIPT_GENERIC_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackKey", "int opCmp(const TextPackKey&in other) const", FO_SCRIPT_FUNC_THIS(Type_Cmp<TextPackKey>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackKey", "string get_str() const", FO_SCRIPT_GENERIC(GenericType_GetStr), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("TextPackKey", "any opImplConv() const", FO_SCRIPT_GENERIC(GenericType_AnyConv), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "TextPackKey opImplConv() const", FO_SCRIPT_GENERIC(GenericType_AnyConvRev), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
    }

    for (const auto& type : meta->GetBaseTypes() | std::views::values) {
        if (type.IsStruct && !registered_types.contains(type.Name)) {
            register_generic_type_body(type);
        }
    }

    // Ref types
    const auto register_ref_type = [&](const BaseTypeDesc& type) { FO_AS_VERIFY(as_engine->RegisterObjectType(type.Name.c_str(), 0, AngelScript::asOBJ_REF)); };

    const auto register_ref_type_body = [&](const BaseTypeDesc& type) {
        const string name = type.Name;
        auto ref_type = type.RefType.as_ptr();

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(type.Name.c_str(), strex("bool opEquals(const {}@+ other) const", name).c_str(), FO_SCRIPT_GENERIC(RefType_Equals), FO_SCRIPT_GENERIC_CONV));

        if (ref_type->FieldsRegistrator) {
            FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(type.Name.c_str(), AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_FUNC_THIS(DynamicRefType_AddRef), FO_SCRIPT_FUNC_THIS_CONV));
            FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(type.Name.c_str(), AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_FUNC_THIS(DynamicRefType_Release), FO_SCRIPT_FUNC_THIS_CONV));
            FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(type.Name.c_str(), AngelScript::asBEHAVE_FACTORY, strex("{}@ f()", name).c_str(), FO_SCRIPT_GENERIC(DynamicRefType_Factory), FO_SCRIPT_GENERIC_CONV, cast_to_void(ref_type->FieldsRegistrator.get())));

            for (const auto& [component_name, component_prop] : ref_type->FieldsRegistrator->GetComponents()) {
                const auto component_type = strex("{}{}Component", name, component_name).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(type.Name.c_str(), strex("{}@ get_{}() const", component_type, component_name).c_str(), FO_SCRIPT_GENERIC(DynamicRefType_GetComponent), FO_SCRIPT_GENERIC_CONV, cast_to_void(component_prop.get())));
            }

            for (size_t i = 1; i < ref_type->FieldsRegistrator->GetPropertiesCount(); i++) {
                nptr<const Property> nullable_prop = ref_type->FieldsRegistrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
                FO_VERIFY_AND_THROW(nullable_prop, "Missing ref type property by index");
                auto prop = nullable_prop.as_ptr();

                if (prop->IsComponentItself()) {
                    continue;
                }

                const string_view handle_str = prop->IsArray() || prop->IsDict() || prop->IsBaseTypeRefType() ? string_view {"@"} : (prop->IsBaseTypeProtoReference() ? string_view {"@+"} : string_view {});
                const string_view set_handle_str = !handle_str.empty() && handle_str[0] == '@' ? (prop->IsNullable() ? string_view {"@?+"} : string_view {"@+"}) : handle_str;
                const auto decl_get = strex("{}{} get_{}() const", MakeScriptPropertyName(prop), handle_str, prop->GetNameWithoutComponent()).str();
                const auto decl_set = strex("void set_{}({}{})", prop->GetNameWithoutComponent(), MakeScriptPropertyName(prop), set_handle_str).str();

                const auto host_type = prop->IsInComponent() ? strex("{}{}Component", name, prop->GetComponentName()).str() : string {name};

                FO_AS_VERIFY(as_engine->RegisterObjectMethod(host_type.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(DynamicRefType_GetProperty), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop.get())));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(host_type.c_str(), decl_set.c_str(), FO_SCRIPT_GENERIC(DynamicRefType_SetProperty), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop.get())));
            }
        }

        for (const auto& method : ref_type->Methods) {
            if (method.Name == "__AddRef") {
                FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name.c_str(), AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_GENERIC(RefType_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)));
            }
            else if (method.Name == "__Release") {
                FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name.c_str(), AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_GENERIC(RefType_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)));
            }
            else if (method.Name == "__Factory") {
                FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name.c_str(), AngelScript::asBEHAVE_FACTORY, strex("{}@ f()", name).c_str(), FO_SCRIPT_GENERIC(RefType_Factory), FO_SCRIPT_GENERIC_CONV, cast_to_void(&ref_type->Methods[2])));
            }
            else if (!strvex(method.Name).starts_with("__")) {
                const string getset = strex("{}", method.Getter ? "get_" : (method.Setter ? "set_" : ""));
                const string decl = strex("{} {}{}({})", MakeScriptReturnName(method.Ret, method.PassOwnership, method.ReturnNullable), getset, method.Name, MakeScriptArgsName(method.Args));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(name.c_str(), decl.c_str(), FO_SCRIPT_GENERIC(RefType_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)));
            }
        }
    };

    for (const auto& type : meta->GetBaseTypes() | std::views::values) {
        if (type.IsRefType) {
            register_ref_type(type);
        }
    }
    for (const auto& type : meta->GetBaseTypes() | std::views::values) {
        if (type.IsRefType) {
            register_ref_type_body(type);
        }
    }
}

FO_END_NAMESPACE

#endif
