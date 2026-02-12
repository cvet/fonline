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
static auto Type_Cmp(const T& self, const T& other) -> int32
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

template<typename T, typename U = T>
static auto Type_Equals(const T& self, const U& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return self == other;
}

static void GenericType_Construct(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    auto* obj = gen->GetObject();

    MemFill(obj, 0, type.Size);
}

static void GenericType_ConstructCopy(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    auto* obj = gen->GetObject();
    const auto* other = *static_cast<void**>(gen->GetAddressOfArg(0));

    MemCopy(obj, other, type.Size);
}

static void GenericType_ConstructArgs(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    auto* obj = gen->GetObject();
    AngelScript::asUINT index = 0;

    VisitBaseTypePrimitive(obj, type, [&index, &gen](auto&& v) {
        MemCopy(&v, gen->GetAddressOfArg(index), sizeof(v));
        index++;
    });
}

static void GenericType_GetStr(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    const auto* obj = gen->GetObject();

    string str;
    VisitBaseTypePrimitive(obj, type, [&str](auto&& v) {
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

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    const auto* obj = gen->GetObject();

    any_t str;
    VisitBaseTypePrimitive(obj, type, [&str](auto&& v) {
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

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    const auto& obj = *cast_from_void<any_t*>(gen->GetObject());
    const auto tokens = strvex(obj).split(' ');

    void* result = gen->GetAddressOfReturnLocation();
    size_t index = 0;

    VisitBaseTypePrimitive(result, type, [&index, &tokens](auto&& v) {
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

        index++;
    });

    if (index != tokens.size()) {
        throw ScriptException("Invalid cast to any (values more then needed)");
    }
}

static void GenericType_Cmp(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    const auto* obj = gen->GetObject();
    const auto* other = *static_cast<void**>(gen->GetAddressOfArg(0));

    int32 cmp = 0;

    if (VisitBaseTypePrimitive(obj, other, type, [](auto&& x, auto&& y) -> bool { return x < y; })) {
        cmp = -1;
    }
    else if (VisitBaseTypePrimitive(other, obj, type, [](auto&& x, auto&& y) -> bool { return x < y; })) {
        cmp = 1;
    }

    new (gen->GetAddressOfReturnLocation()) int32(cmp);
}

static void GenericType_Equals(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    const auto* obj = gen->GetObject();
    const auto* other = *static_cast<void**>(gen->GetAddressOfArg(0));

    const auto equals = MemCompare(obj, other, type.Size);
    new (gen->GetAddressOfReturnLocation()) bool(equals);
}

static void GenericType_GetZero(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& type = *cast_from_void<const BaseTypeDesc*>(gen->GetAuxiliary());
    void* result = gen->GetAddressOfReturnLocation();
    VisitBaseTypePrimitive(result, type, [](auto&& v) { v = {}; });
}

static void HashedString_Construct(hstring* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) hstring();
}

static void HashedString_ConstructFromString(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* str = *cast_from_void<const string**>(gen->GetAddressOfArg(0));
    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto hstr = meta->Hashes.ToHashedString(*str);
    new (gen->GetObject()) hstring(hstr);
}

static void HashedString_Destruct(hstring* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    self->~hstring();
}

static void HashedString_IsValidHash(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto hash = *cast_from_void<const hstring::hash_t*>(gen->GetAddressOfArg(0));
    const auto* meta = GetEngineMetadata(gen->GetEngine());
    bool failed = false;
    const auto hstr = meta->Hashes.ResolveHash(hash, &failed);
    ignore_unused(hstr);
    new (gen->GetAddressOfReturnLocation()) bool(!failed);
}

static void HashedString_CreateFromHash(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto hash = *cast_from_void<const hstring::hash_t*>(gen->GetAddressOfArg(0));
    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto hstr = meta->Hashes.ResolveHash(hash);
    new (gen->GetAddressOfReturnLocation()) hstring(hstr);
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

static auto HashedString_EqualsString(const hstring& self, const string& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_str() == other;
}

static auto HashedString_StringCast(const hstring& self) -> const string&
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_str();
}

static auto HashedString_StringConv(const hstring& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_str();
}

static auto HashedString_GetString(const hstring& self) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return string(self.as_str());
}

static auto HashedString_GetHash(const hstring& self) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_int32();
}

static auto HashedString_GetUHash(const hstring& self) -> uint32
{
    FO_NO_STACK_TRACE_ENTRY();

    return self.as_uint32();
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

template<typename T>
static auto Any_Conv(const any_t& self) -> T
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (std::same_as<T, bool>) {
        return strex(self).to_bool();
    }
    else if constexpr (some_strong_type<T>) {
        return T {numeric_cast<typename T::underlying_type>(strex(self).to_int64())};
    }
    else if constexpr (std::integral<T>) {
        if (strex(self).is_explicit_bool()) {
            return strex(self).to_bool() ? 1 : 0;
        }
        return numeric_cast<T>(strex(self).to_int64());
    }
    else if constexpr (std::floating_point<T>) {
        return numeric_cast<T>(strex(self).to_float64());
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

    if constexpr (std::same_as<T, hstring>) {
        const auto* self = cast_from_void<any_t*>(gen->GetObject());
        const auto* meta = GetEngineMetadata(gen->GetEngine());
        const auto hstr = meta->Hashes.ToHashedString(*self);
        new (gen->GetAddressOfReturnLocation()) hstring(hstr);
    }
}

template<typename T>
static auto Any_ConvFrom(const T& self) -> any_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return any_t(strex("{}", self).str());
}

static void Ucolor_ConstructRawRgba(ucolor* self, uint32 rgba)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) ucolor {rgba};
}

static void Ucolor_ConstructRgba(ucolor* self, int32 r, int32 g, int32 b, int32 a)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto clamped_r = numeric_cast<uint8>(std::clamp(r, 0, 255));
    const auto clamped_g = numeric_cast<uint8>(std::clamp(g, 0, 255));
    const auto clamped_b = numeric_cast<uint8>(std::clamp(b, 0, 255));
    const auto clamped_a = numeric_cast<uint8>(std::clamp(a, 0, 255));

    new (self) ucolor {clamped_r, clamped_g, clamped_b, clamped_a};
}

template<typename T>
static void Time_ConstructWithPlace(T* self, int64 value, int32 place)
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

static void Ipos_ConstructXandY(ipos32* self, int32 x, int32 y)
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

    return self.x >= rect.x && self.y >= rect.x && self.x < rect.x + rect.width && self.y < rect.y + rect.height;
}

static void Isize_ConstructWandH(isize32* self, int32 width, int32 height)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) isize32 {width, height};
}

static void Irect_ConstructXandYandWandH(irect32* self, int32 x, int32 y, int32 width, int32 height)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) irect32 {x, y, width, height};
}

static void Fpos_ConstructXandY(fpos32* self, float32 x, float32 y)
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

static void Fsize_ConstructWandH(fsize32* self, float32 width, float32 height)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) fsize32 {width, height};
}

static void Mpos_ConstructXandY(mpos* self, int32 x, int32 y)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (self) mpos {numeric_cast<int16>(x), numeric_cast<int16>(y)};
}

static auto Mpos_FitToSize(const mpos& self, msize size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return size.is_valid_pos(self);
}

static void RefType_Factory(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& mehtod = *cast_from_void<const MethodDesc*>(gen->GetAuxiliary());
    FO_RUNTIME_ASSERT(mehtod.Call);

    ScriptGenericCall(gen, false, [&](FuncCallData& call) { mehtod.Call(call); });
}

static void RefType_MethodCall(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& mehtod = *cast_from_void<const MethodDesc*>(gen->GetAuxiliary());
    FO_RUNTIME_ASSERT(mehtod.Call);

    ScriptGenericCall(gen, true, [&](FuncCallData& call) { mehtod.Call(call); });
}

template<typename T>
static void Global_GetZero(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) T(*cast_from_void<const T*>(gen->GetAuxiliary()));
}

void RegisterAngelScriptTypes(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    auto* backend = GetScriptBackend(as_engine);
    const auto* meta = backend->GetMetadata();

    // Register hstring
    FO_AS_VERIFY(as_engine->RegisterObjectType("hstring", sizeof(hstring), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<hstring>()));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(HashedString_Construct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_CONSTRUCT, "void f(const hstring &in)", FO_SCRIPT_FUNC_THIS(HashedString_ConstructCopy), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_CONSTRUCT, "void f(const string &in)", FO_SCRIPT_GENERIC(HashedString_ConstructFromString), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("hstring", AngelScript::asBEHAVE_DESTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(HashedString_Destruct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("bool hstring_isValidHash(int h)", FO_SCRIPT_GENERIC(HashedString_IsValidHash), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hstring hstring_fromHash(int h)", FO_SCRIPT_GENERIC(HashedString_CreateFromHash), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "hstring& opAssign(const hstring &in)", FO_SCRIPT_FUNC_THIS(HashedString_Assign), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "int opCmp(const hstring &in) const", FO_SCRIPT_FUNC_THIS(Type_Cmp<hstring>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "bool opEquals(const hstring &in) const", FO_SCRIPT_FUNC_THIS(Type_Equals<hstring>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "bool opEquals(const string &in) const", FO_SCRIPT_FUNC_THIS(HashedString_EqualsString), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "const string& opImplCast() const", FO_SCRIPT_FUNC_THIS(HashedString_StringCast), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "string opImplConv() const", FO_SCRIPT_FUNC_THIS(HashedString_StringConv), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "string get_str() const", FO_SCRIPT_FUNC_THIS(HashedString_GetString), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "int get_hash() const", FO_SCRIPT_FUNC_THIS(HashedString_GetHash), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "uint get_uhash() const", FO_SCRIPT_FUNC_THIS(HashedString_GetUHash), FO_SCRIPT_FUNC_THIS_CONV));
    static constexpr hstring empty_hstring;
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("hstring get_EMPTY_HSTRING()", FO_SCRIPT_GENERIC(Global_GetZero<hstring>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&empty_hstring)));

    // Register any
    FO_AS_VERIFY(as_engine->RegisterObjectType("any", sizeof(any_t), AngelScript::asOBJ_VALUE | AngelScript::asGetTypeTraits<string>()));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Any_Construct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const any &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructCopy), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const bool &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<bool>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int8 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int8>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint8 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint8>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int16 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int16>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint16 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint16>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int32>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint32>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const int64 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<int64>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const uint64 &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<uint64>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const float &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<float32>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_CONSTRUCT, "void f(const double &in)", FO_SCRIPT_FUNC_THIS(Any_ConstructFrom<float64>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("any", AngelScript::asBEHAVE_DESTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Any_Destruct), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "any& opAssign(const any &in)", FO_SCRIPT_FUNC_THIS(Any_Assign), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "bool opEquals(const any &in) const", FO_SCRIPT_FUNC_THIS(Type_Equals<any_t>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "bool opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<bool>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int8 opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<int8>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint8 opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<uint8>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int16 opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<int16>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint16 opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<uint16>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<int32>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<uint32>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "int64 opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<int64>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "uint64 opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<uint64>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "float opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<float32>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "double opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<float64>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "string opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_Conv<string>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", "hstring opImplConv() const", FO_SCRIPT_GENERIC(Any_ConvGen<hstring>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("string", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_ConvFrom<string>), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("hstring", "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Any_ConvFrom<hstring>), FO_SCRIPT_FUNC_THIS_CONV));
    RegisterAngelScriptStringAnyExtensions(as_engine);

    // Built-in value types
    unordered_set<string> registered_types;

    const auto register_engine_type = [&]<typename T>(const char* name) {
        registered_types.emplace(name);
        FO_AS_VERIFY(as_engine->RegisterObjectType(name, sizeof(T), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<T>()));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(Type_Construct<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_CONSTRUCT, strex("void f(const {}&in other)", name).c_str(), FO_SCRIPT_FUNC_THIS(Type_ConstructCopy<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("int opCmp(const {}&in other) const", name).c_str(), FO_SCRIPT_FUNC_THIS(Type_Cmp<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("bool opEquals(const {}&in other) const", name).c_str(), FO_SCRIPT_FUNC_THIS(Type_Equals<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "string get_str() const", FO_SCRIPT_FUNC_THIS(Type_GetStr<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "any opImplConv() const", FO_SCRIPT_FUNC_THIS(Type_AnyConv<T>), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", strex("{} opImplConv() const", name).c_str(), FO_SCRIPT_FUNC_THIS(Any_Conv<T>), FO_SCRIPT_FUNC_THIS_CONV));
        static constexpr T ZERO_VALUE;
        FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("{} get_ZERO_{}()", name, strex(name).upper()).c_str(), FO_SCRIPT_GENERIC(Global_GetZero<T>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&ZERO_VALUE)));
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
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_nanoseconds() const", FO_SCRIPT_METHOD_EXT(timespan, nanoseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_microseconds() const", FO_SCRIPT_METHOD_EXT(timespan, microseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_milliseconds() const", FO_SCRIPT_METHOD_EXT(timespan, milliseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("timespan", "int64 get_seconds() const", FO_SCRIPT_METHOD_EXT(timespan, seconds, () const, int64), FO_SCRIPT_METHOD_CONV));

    register_engine_type.operator()<nanotime>("nanotime");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("nanotime", AngelScript::asBEHAVE_CONSTRUCT, "void f(int64 value, int place)", FO_SCRIPT_FUNC_THIS((Time_ConstructWithPlace<nanotime>)), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime& opAddAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(nanotime, operator+=, (const timespan&), nanotime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime& opSubAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(nanotime, operator-=, (const timespan&), nanotime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime opAdd(const timespan &in) const", FO_SCRIPT_METHOD_EXT(nanotime, operator+, (const timespan&) const, nanotime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "nanotime opSub(const timespan &in) const", FO_SCRIPT_METHOD_EXT(nanotime, operator-, (const timespan&) const, nanotime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "timespan opSub(const nanotime &in) const", FO_SCRIPT_METHOD_EXT(nanotime, operator-, (const nanotime&) const, timespan), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_nanoseconds() const", FO_SCRIPT_METHOD_EXT(nanotime, nanoseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_microseconds() const", FO_SCRIPT_METHOD_EXT(nanotime, microseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_milliseconds() const", FO_SCRIPT_METHOD_EXT(nanotime, milliseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "int64 get_seconds() const", FO_SCRIPT_METHOD_EXT(nanotime, seconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("nanotime", "timespan get_timeSinceEpoch() const", FO_SCRIPT_METHOD_EXT(nanotime, duration_value, () const, timespan), FO_SCRIPT_METHOD_CONV));

    register_engine_type.operator()<synctime>("synctime");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("synctime", AngelScript::asBEHAVE_CONSTRUCT, "void f(int64 value, int place)", FO_SCRIPT_FUNC_THIS((Time_ConstructWithPlace<synctime>)), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime& opAddAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(synctime, operator+=, (const timespan&), synctime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime& opSubAssign(const timespan &in)", FO_SCRIPT_METHOD_EXT(synctime, operator-=, (const timespan&), synctime&), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime opAdd(const timespan &in) const", FO_SCRIPT_METHOD_EXT(synctime, operator+, (const timespan&) const, synctime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "synctime opSub(const timespan &in) const", FO_SCRIPT_METHOD_EXT(synctime, operator-, (const timespan&) const, synctime), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "timespan opSub(const synctime &in) const", FO_SCRIPT_METHOD_EXT(synctime, operator-, (const synctime&) const, timespan), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "int64 get_milliseconds() const", FO_SCRIPT_METHOD_EXT(synctime, milliseconds, () const, int64), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("synctime", "int64 get_seconds() const", FO_SCRIPT_METHOD_EXT(synctime, seconds, () const, int64), FO_SCRIPT_METHOD_CONV));
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

    register_engine_type.operator()<fpos32>("fpos");
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("fpos", AngelScript::asBEHAVE_CONSTRUCT, "void f(float x, float y)", FO_SCRIPT_FUNC_THIS(Fpos_ConstructXandY), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("fpos", "float x", offsetof(fpos32, x)));
    FO_AS_VERIFY(as_engine->RegisterObjectProperty("fpos", "float y", offsetof(fpos32, y)));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos& opAddAssign(const fpos &in)", FO_SCRIPT_FUNC_THIS(Fpos_AddAssignFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos& opSubAssign(const fpos &in)", FO_SCRIPT_FUNC_THIS(Fpos_SubAssignFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos opAdd(const fpos &in) const", FO_SCRIPT_FUNC_THIS(Fpos_AddFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos opSub(const fpos &in) const", FO_SCRIPT_FUNC_THIS(Fpos_SubFpos), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("fpos", "fpos opNeg() const", FO_SCRIPT_FUNC_THIS(Fpos_NegFpos), FO_SCRIPT_FUNC_THIS_CONV));

    register_engine_type.operator()<fsize32>("fsize");
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

    // Value types
    const auto register_generic_type = [&](const BaseTypeDesc& type) {
        struct SimpleClass
        {
            int32 a {};
        };

        const char* name = type.Name.c_str();
        FO_AS_VERIFY(as_engine->RegisterObjectType(name, numeric_cast<int32>(type.Size), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS_ALLINTS | AngelScript::asGetTypeTraits<SimpleClass>()));
    };

    const auto register_generic_type_body = [&](const BaseTypeDesc& type) {
        const char* name = type.Name.c_str();
        const auto& layout = *type.StructLayout;

        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_CONSTRUCT, "void f()", FO_SCRIPT_GENERIC(GenericType_Construct), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_CONSTRUCT, strex("void f(const {}&in other)", name).c_str(), FO_SCRIPT_GENERIC(GenericType_ConstructCopy), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));

        string ctor_decl;

        for (size_t i = 0; i < layout.Fields.size(); i++) {
            const auto& field = layout.Fields[i];
            ctor_decl += strex("{}{} {}", i > 0 ? ", " : "", MakeScriptTypeName(field.Type), field.Name);
            FO_AS_VERIFY(as_engine->RegisterObjectProperty(name, strex("{} {}", MakeScriptTypeName(field.Type), field.Name).c_str(), numeric_cast<int32>(field.Offset)));
        }

        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_CONSTRUCT, strex("void f({})", ctor_decl).c_str(), FO_SCRIPT_GENERIC(GenericType_ConstructArgs), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));

        if (type.IsSimpleStruct) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("int opCmp(const {}&in other) const", name).c_str(), FO_SCRIPT_GENERIC(GenericType_Cmp), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        }

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("bool opEquals(const {}&in other) const", name).c_str(), FO_SCRIPT_GENERIC(GenericType_Equals), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "string get_str() const", FO_SCRIPT_GENERIC(GenericType_GetStr), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "any opImplConv() const", FO_SCRIPT_GENERIC(GenericType_AnyConv), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("any", strex("{} opImplConv() const", name).c_str(), FO_SCRIPT_GENERIC(GenericType_AnyConvRev), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));

        FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("{} get_ZERO_{}()", name, strex(name).upper()).c_str(), FO_SCRIPT_GENERIC(GenericType_GetZero), FO_SCRIPT_GENERIC_CONV, cast_to_void(&type)));
    };

    for (const auto& type : meta->GetBaseTypes() | std::views::values) {
        // Types may intersect so register in two steps
        if (type.IsStruct && !registered_types.contains(type.Name)) {
            register_generic_type(type);
        }
    }
    for (const auto& type : meta->GetBaseTypes() | std::views::values) {
        if (type.IsStruct && !registered_types.contains(type.Name)) {
            register_generic_type_body(type);
        }
    }

    // Ref types
    const auto register_ref_type = [&](const BaseTypeDesc& type) {
        const char* name = type.Name.c_str();
        FO_AS_VERIFY(as_engine->RegisterObjectType(name, 0, AngelScript::asOBJ_REF));
    };

    const auto register_ref_type_body = [&](const BaseTypeDesc& type) {
        const char* name = type.Name.c_str();
        const auto& ref_type = *type.RefType;

        for (const auto& method : ref_type.Methods) {
            if (method.Name == "__AddRef") {
                FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_GENERIC(RefType_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)));
            }
            else if (method.Name == "__Release") {
                FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_GENERIC(RefType_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)));
            }
            else if (method.Name == "__Factory") {
                FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_FACTORY, strex("{}@ f()", name).c_str(), FO_SCRIPT_GENERIC(RefType_Factory), FO_SCRIPT_GENERIC_CONV, cast_to_void(&ref_type.Methods[2])));
            }
            else if (!strvex(method.Name).starts_with("__")) {
                const string getset = strex("{}", method.Getter ? "get_" : (method.Setter ? "set_" : ""));
                const string decl = strex("{} {}{}({})", MakeScriptReturnName(method.Ret, method.PassOwnership), getset, method.Name, MakeScriptArgsName(method.Args));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, decl.c_str(), FO_SCRIPT_GENERIC(RefType_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)));
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
