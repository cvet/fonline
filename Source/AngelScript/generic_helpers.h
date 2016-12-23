#pragma once

template<typename R>
struct GenericWrapCdecl
{
    template<typename Fn, Fn fn> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn()); }
    template<typename Fn, Fn fn, typename T1> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0))); }
    template<typename Fn, Fn fn, typename T1, typename T2> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1))); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2))); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3, typename T4> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2), *(T4*)gen->GetAddressOfArg(3))); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3, typename T4, typename T5> static void Call(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) R(fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2), *(T4*)gen->GetAddressOfArg(3), *(T5*)gen->GetAddressOfArg(4))); }
};
template<>
struct GenericWrapCdecl<void>
{
    template<typename Fn, Fn fn> static void Call(asIScriptGeneric* gen) { fn(); }
    template<typename Fn, Fn fn, typename T1> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0)); }
    template<typename Fn, Fn fn, typename T1, typename T2> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1)); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2)); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3, typename T4> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2), *(T4*)gen->GetAddressOfArg(3)); }
    template<typename Fn, Fn fn, typename T1, typename T2, typename T3, typename T4, typename T5> static void Call(asIScriptGeneric* gen) { fn(*(T1*)gen->GetAddressOfArg(0), *(T2*)gen->GetAddressOfArg(1), *(T3*)gen->GetAddressOfArg(2), *(T4*)gen->GetAddressOfArg(3), *(T5*)gen->GetAddressOfArg(4)); }
};

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

#define WRAP_CDECL_TO_GENERIC(ret, func, ...) static void func ## _Generic(asIScriptGeneric* gen) { GenericWrapCdecl<ret>::Call<decltype(&func), &func, ##__VA_ARGS__>(gen); }
#define WRAP_THISCALL_TO_GENERIC(ret, type, func, ...) static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { GenericWrapThiscall<type, ret>::Call<decltype(&type::func), &type::func, ##__VA_ARGS__>(gen); }

#if AS_MAX_PORTABILITY
#define AUTO_CONV_FUNC(ret, func, ...) asFUNCTION((GenericWrapCdecl<ret>::Call<decltype(&func), &func, ##__VA_ARGS__>))
#define AUTO_CONV_METHOD(ret, type, func, ...) asFUNCTION((GenericWrapThiscall<type, ret>::Call<decltype(&func), &func, ##__VA_ARGS__>))
#define AUTO_CONV_TYPE(conv) asCALL_GENERIC
#else
#define AUTO_CONV_FUNC(ret, func, ...) asFUNCTION((func))
#define AUTO_CONV_METHOD(ret, func, ...) asMETHOD(type, func)
#define AUTO_CONV_TYPE(conv) conv
#endif
