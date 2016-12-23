#pragma once

template<typename R>
struct GenericWrapCdecl
{
    template<typename Fn, Fn fn> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn()); }
    template<typename Fn, Fn fn, typename T1> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0))); }
    template<typename Fn, Fn fn, typename T1, typename T2> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1))); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(2), *(T3*)gen->GetAddressOfArg(2))); }
};
template<>
struct GenericWrapCdecl<void>
{
    template<typename Fn, Fn fn> static void Call(asIScriptGeneric* gen) { fn(); }
    template<typename Fn, Fn fn, typename T1> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0)); }
    template<typename Fn, Fn fn, typename T1, typename T2> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1)); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(2), *(T3*)gen->GetAddressOfArg(2)); }
};
#define WRAP_CDECL_TO_GENERIC(ret, func, ...) static void func ## _Generic(asIScriptGeneric* gen) { GenericWrapCdecl<ret>::Call<decltype(&func), &func, ##__VA_ARGS__>(gen); }
#define WRAP_CDECL_TO_GENERIC_NAMED(ret, func, name, ...) static void name ## _Generic(asIScriptGeneric* gen) { GenericWrapCdecl<ret>::Call<decltype(&func), &func, ##__VA_ARGS__>(gen); }

template<typename T, typename R>
struct GenericWrapThiscall
{
    template<typename Fn, Fn fn> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R((((T*)gen->GetObject())->*fn)()); }
    template<typename Fn, Fn fn, typename T1> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R((((T*)gen->GetObject())->*fn)(*(T1*)gen->GetAddressOfArg(0))); }
    template<typename Fn, Fn fn, typename T1, typename T2> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R((((T*)gen->GetObject())->*fn)(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1))); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R((((T*)gen->GetObject())->*fn)(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2))); }
};
template<typename T>
struct GenericWrapThiscall<T, void>
{
    template<typename Fn, Fn fn> static void Call(asIScriptGeneric* gen) { (((T*)gen->GetObject())->*fn)(); }
    template<typename Fn, Fn fn, typename T1> static void Call(asIScriptGeneric* gen) { (((T*)gen->GetObject())->*fn)(*(T1*)gen->GetAddressOfArg(0)); }
    template<typename Fn, Fn fn, typename T1, typename T2> static void Call(asIScriptGeneric* gen) { (((T*)gen->GetObject())->*fn)(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1)); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3> static void Call(asIScriptGeneric* gen) { (((T*)gen->GetObject())->*fn)(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2)); }
};
#define WRAP_THISCALL_TO_GENERIC(ret, type, func, ...) static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { GenericWrapThiscall<type, ret>::Call<decltype(&type::func), &type::func, ##__VA_ARGS__>(gen); }
#define WRAP_THISCALL_TO_GENERIC_NAMED(ret, type, func, name, ...) static void name ## _Generic(asIScriptGeneric* gen) { GenericWrapThiscall<type, ret>::Call<decltype(&type::func), &type::func, ##__VA_ARGS__>(gen); }
