#pragma once

#define WRAP_CDECL_TO_GENERIC_0(func) \
    static void func ## _Generic(asIScriptGeneric* gen) { func(); }
#define WRAP_CDECL_TO_GENERIC_1(func, arg1) \
    static void func ## _Generic(asIScriptGeneric* gen) { func(*(arg1*) gen->GetArgAddress(0)); }
#define WRAP_CDECL_TO_GENERIC_2(func, arg1, arg2) \
    static void func ## _Generic(asIScriptGeneric* gen) { func(*(arg1*) gen->GetArgAddress(0), *(arg2*) gen->GetArgAddress(1)); }
#define WRAP_CDECL_TO_GENERIC_3(func, arg1, arg2, arg3) \
    static void func ## _Generic(asIScriptGeneric* gen) { func(*(arg1*) gen->GetArgAddress(0), *(arg2*) gen->GetArgAddress(1), *(arg3*) gen->GetArgAddress(2)); }

#define WRAP_CDECL_TO_GENERIC_0_RET(func, ret) \
    static void func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(func()); }
#define WRAP_CDECL_TO_GENERIC_1_RET(func, arg1, ret) \
    static void func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(func(*(arg1*) gen->GetArgAddress(0))); }
#define WRAP_CDECL_TO_GENERIC_2_RET(func, arg1, arg2, ret) \
    static void func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(func(*(arg1*) gen->GetArgAddress(0), *(arg2*) gen->GetArgAddress(1))); }
#define WRAP_CDECL_TO_GENERIC_3_RET(func, arg1, arg2, arg3, ret) \
    static void func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(func(*(arg1*) gen->GetArgAddress(0), *(arg2*) gen->GetArgAddress(1), *(arg3*) gen->GetArgAddress(2))); }

#define WRAP_THISCALL_TO_GENERIC_0(type, func) \
    static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { ((type*)gen->GetObject())->func(); }
#define WRAP_THISCALL_TO_GENERIC_1(type, func, arg1) \
    static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { ((type*)gen->GetObject())->func(*(arg1*)gen->GetArgAddress(0)); }
#define WRAP_THISCALL_TO_GENERIC_2(type, func, arg1, arg2) \
    static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { ((type*)gen->GetObject())->func(*(arg1*)gen->GetArgAddress(0), *(arg2*)gen->GetArgAddress(1)); }

#define WRAP_THISCALL_TO_GENERIC_0_RET(type, func, ret) \
    static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(((type*)gen->GetObject())->func()); }
#define WRAP_THISCALL_TO_GENERIC_1_RET(type, func, arg1, ret) \
    static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(((type*)gen->GetObject())->func(*(arg1*)gen->GetArgAddress(0))); }
#define WRAP_THISCALL_TO_GENERIC_2_RET(type, func, arg1, arg2, ret) \
    static void type ## _ ## func ## _Generic(asIScriptGeneric* gen) { new(gen->GetAddressOfReturnLocation()) ret(((type*)gen->GetObject())->func(*(arg1*)gen->GetArgAddress(0), *(arg2*)gen->GetArgAddress(1))); }
