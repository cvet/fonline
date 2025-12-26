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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include <angelscript.h>

#ifdef AS_MAX_PORTABILITY
#define SCRIPT_GENERIC(name) asFUNCTION(name)
#define SCRIPT_FUNC(name) WRAP_FN(name)
#define SCRIPT_FUNC_EXT(name, params, ret) WRAP_FN_PR(name, params, ret)
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_METHOD_EXT(type, name, params, ret) WRAP_MFN_PR(type, name, params, ret)
#define SCRIPT_GENERIC_CONV AngelScript::asCALL_GENERIC
#define SCRIPT_FUNC_CONV AngelScript::asCALL_GENERIC
#define SCRIPT_FUNC_THIS_CONV AngelScript::asCALL_GENERIC
#define SCRIPT_METHOD_CONV AngelScript::asCALL_GENERIC
#else
#define SCRIPT_GENERIC(name) asFUNCTION(name)
#define SCRIPT_FUNC(name) AngelScript::asFUNCTION(name)
#define SCRIPT_FUNC_EXT(name, params, ret) AngelScript::asFUNCTIONPR(name, params, ret)
#define SCRIPT_FUNC_THIS(name) AngelScript::asFUNCTION(name)
#define SCRIPT_METHOD(type, name) AngelScript::asMETHOD(type, name)
#define SCRIPT_METHOD_EXT(type, name, params, ret) AngelScript::asMETHODPR(type, name, params, ret)
#define SCRIPT_GENERIC_CONV AngelScript::asCALL_GENERIC
#define SCRIPT_FUNC_CONV AngelScript::asCALL_CDECL
#define SCRIPT_FUNC_THIS_CONV AngelScript::asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD_CONV AngelScript::asCALL_THISCALL
#endif

// Generated in AngelScript autowrapper add-on

#define WRAP_FN(name) (AS_NAMESPACE_QUALIFIER gw::id(name).f<name /* avoid operator- bug */>())
#define WRAP_MFN(ClassType, name) (AS_NAMESPACE_QUALIFIER gw::id(&ClassType::name).f<&ClassType::name /* avoid operator- bug */>())
#define WRAP_OBJ_FIRST(name) (AS_NAMESPACE_QUALIFIER gw::id(name).of<name /* avoid operator- bug */>())
#define WRAP_OBJ_LAST(name) (AS_NAMESPACE_QUALIFIER gw::id(name).ol<name /* avoid operator- bug */>())
#define WRAP_FN_PR(name, Parameters, ReturnType) AngelScript::asFUNCTION((AS_NAMESPACE_QUALIFIER gw::Wrapper<ReturnType(*) Parameters>::f<name /* avoid operator- bug */>))
#define WRAP_MFN_PR(ClassType, name, Parameters, ReturnType) AngelScript::asFUNCTION((AS_NAMESPACE_QUALIFIER gw::Wrapper<ReturnType(ClassType::*) Parameters>::f<&ClassType::name /**/>))
#define WRAP_OBJ_FIRST_PR(name, Parameters, ReturnType) AngelScript::asFUNCTION((AS_NAMESPACE_QUALIFIER gw::ObjFirst<ReturnType(*) Parameters>::f<name /* avoid operator- bug */>))
#define WRAP_OBJ_LAST_PR(name, Parameters, ReturnType) AngelScript::asFUNCTION((AS_NAMESPACE_QUALIFIER gw::ObjLast<ReturnType(*) Parameters>::f<name /* avoid operator- bug */>))
#define WRAP_CON(ClassType, Parameters) AngelScript::asFUNCTION((AS_NAMESPACE_QUALIFIER gw::Constructor<ClassType Parameters>::f))
#define WRAP_DES(ClassType) AngelScript::asFUNCTION((AS_NAMESPACE_QUALIFIER gw::destroy<ClassType>))

BEGIN_AS_NAMESPACE

namespace gw
{
    template<typename T>
    class Proxy
    {
    public:
        T value;
        Proxy(T value) :
            value(value)
        {
        }
        static T cast(void* ptr) { return reinterpret_cast<Proxy<T>*>(&ptr)->value; }

    private:
        Proxy(const Proxy&);
        Proxy& operator=(const Proxy&);
    };

    template<typename T>
    struct Wrapper
    {
    };
    template<typename T>
    struct ObjFirst
    {
    };
    template<typename T>
    struct ObjLast
    {
    };
    template<typename T>
    struct Constructor
    {
    };

    template<typename T>
    void destroy(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
    {
        static_cast<T*>(gen->GetObject())->~T();
    }
    template<>
    struct Wrapper<void (*)(void)>
    {
        template<void (*fp)(void)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric*)
        {
            ((fp)());
        }
    };
    template<typename R>
    struct Wrapper<R (*)(void)>
    {
        template<R (*fp)(void)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)());
        }
    };
    template<typename T>
    struct Wrapper<void (T::*)(void)>
    {
        template<void (T::*fp)(void)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)());
        }
    };
    template<typename T, typename R>
    struct Wrapper<R (T::*)(void)>
    {
        template<R (T::*fp)(void)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)());
        }
    };
    template<typename T>
    struct Wrapper<void (T::*)(void) const>
    {
        template<void (T::*fp)(void) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)());
        }
    };
    template<typename T, typename R>
    struct Wrapper<R (T::*)(void) const>
    {
        template<R (T::*fp)(void) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)());
        }
    };
    template<typename T>
    struct ObjFirst<void (*)(T)>
    {
        template<void (*fp)(T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R>
    struct ObjFirst<R (*)(T)>
    {
        template<R (*fp)(T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T>
    struct ObjLast<void (*)(T)>
    {
        template<void (*fp)(T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R>
    struct ObjLast<R (*)(T)>
    {
        template<R (*fp)(T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T>
    struct Constructor<T()>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(); }
    };
    template<typename A0>
    struct Wrapper<void (*)(A0)>
    {
        template<void (*fp)(A0)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename R, typename A0>
    struct Wrapper<R (*)(A0)>
    {
        template<R (*fp)(A0)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename A0>
    struct Wrapper<void (T::*)(A0)>
    {
        template<void (T::*fp)(A0)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename R, typename A0>
    struct Wrapper<R (T::*)(A0)>
    {
        template<R (T::*fp)(A0)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename A0>
    struct Wrapper<void (T::*)(A0) const>
    {
        template<void (T::*fp)(A0) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename R, typename A0>
    struct Wrapper<R (T::*)(A0) const>
    {
        template<R (T::*fp)(A0) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename A0>
    struct ObjFirst<void (*)(T, A0)>
    {
        template<void (*fp)(T, A0)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename R, typename A0>
    struct ObjFirst<R (*)(T, A0)>
    {
        template<R (*fp)(T, A0)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value));
        }
    };
    template<typename T, typename A0>
    struct ObjLast<void (*)(A0, T)>
    {
        template<void (*fp)(A0, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0>
    struct ObjLast<R (*)(A0, T)>
    {
        template<R (*fp)(A0, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0>
    struct Constructor<T(A0)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value); }
    };
    template<typename A0, typename A1>
    struct Wrapper<void (*)(A0, A1)>
    {
        template<void (*fp)(A0, A1)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename R, typename A0, typename A1>
    struct Wrapper<R (*)(A0, A1)>
    {
        template<R (*fp)(A0, A1)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename A0, typename A1>
    struct Wrapper<void (T::*)(A0, A1)>
    {
        template<void (T::*fp)(A0, A1)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1>
    struct Wrapper<R (T::*)(A0, A1)>
    {
        template<R (T::*fp)(A0, A1)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename A0, typename A1>
    struct Wrapper<void (T::*)(A0, A1) const>
    {
        template<void (T::*fp)(A0, A1) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1>
    struct Wrapper<R (T::*)(A0, A1) const>
    {
        template<R (T::*fp)(A0, A1) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename A0, typename A1>
    struct ObjFirst<void (*)(T, A0, A1)>
    {
        template<void (*fp)(T, A0, A1)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1>
    struct ObjFirst<R (*)(T, A0, A1)>
    {
        template<R (*fp)(T, A0, A1)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value));
        }
    };
    template<typename T, typename A0, typename A1>
    struct ObjLast<void (*)(A0, A1, T)>
    {
        template<void (*fp)(A0, A1, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1>
    struct ObjLast<R (*)(A0, A1, T)>
    {
        template<R (*fp)(A0, A1, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1>
    struct Constructor<T(A0, A1)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value); }
    };
    template<typename A0, typename A1, typename A2>
    struct Wrapper<void (*)(A0, A1, A2)>
    {
        template<void (*fp)(A0, A1, A2)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2>
    struct Wrapper<R (*)(A0, A1, A2)>
    {
        template<R (*fp)(A0, A1, A2)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2>
    struct Wrapper<void (T::*)(A0, A1, A2)>
    {
        template<void (T::*fp)(A0, A1, A2)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2>
    struct Wrapper<R (T::*)(A0, A1, A2)>
    {
        template<R (T::*fp)(A0, A1, A2)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2>
    struct Wrapper<void (T::*)(A0, A1, A2) const>
    {
        template<void (T::*fp)(A0, A1, A2) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2>
    struct Wrapper<R (T::*)(A0, A1, A2) const>
    {
        template<R (T::*fp)(A0, A1, A2) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2>
    struct ObjFirst<void (*)(T, A0, A1, A2)>
    {
        template<void (*fp)(T, A0, A1, A2)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2>
    struct ObjFirst<R (*)(T, A0, A1, A2)>
    {
        template<R (*fp)(T, A0, A1, A2)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2>
    struct ObjLast<void (*)(A0, A1, A2, T)>
    {
        template<void (*fp)(A0, A1, A2, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2>
    struct ObjLast<R (*)(A0, A1, A2, T)>
    {
        template<R (*fp)(A0, A1, A2, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2>
    struct Constructor<T(A0, A1, A2)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3>
    struct Wrapper<void (*)(A0, A1, A2, A3)>
    {
        template<void (*fp)(A0, A1, A2, A3)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3>
    struct Wrapper<R (*)(A0, A1, A2, A3)>
    {
        template<R (*fp)(A0, A1, A2, A3)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3>
    struct Wrapper<void (T::*)(A0, A1, A2, A3)>
    {
        template<void (T::*fp)(A0, A1, A2, A3)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3>
    struct Wrapper<R (T::*)(A0, A1, A2, A3)>
    {
        template<R (T::*fp)(A0, A1, A2, A3)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3>
    struct Wrapper<void (T::*)(A0, A1, A2, A3) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3>
    struct Wrapper<R (T::*)(A0, A1, A2, A3) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3)>
    {
        template<void (*fp)(T, A0, A1, A2, A3)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3)>
    {
        template<R (*fp)(T, A0, A1, A2, A3)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3>
    struct ObjLast<void (*)(A0, A1, A2, A3, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3>
    struct ObjLast<R (*)(A0, A1, A2, A3, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3>
    struct Constructor<T(A0, A1, A2, A3)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4>
    struct Constructor<T(A0, A1, A2, A3, A4)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    struct Constructor<T(A0, A1, A2, A3, A4, A5)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen) { new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value); }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value);
        }
    };
    template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Wrapper<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Wrapper<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Wrapper<void (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29) const>
    {
        template<void (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Wrapper<R (T::*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29) const>
    {
        template<R (T::*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29) const>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((static_cast<T*>(gen->GetObject())->*fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct ObjFirst<void (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        template<void (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct ObjFirst<R (*)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        template<R (*fp)(T, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(Proxy<T>::cast(gen->GetObject()), static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value,
                static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct ObjLast<void (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, T)>
    {
        template<void (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            ((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value, static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value,
                static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct ObjLast<R (*)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, T)>
    {
        template<R (*fp)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, T)>
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>((fp)(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value, Proxy<T>::cast(gen->GetObject())));
        }
    };
    template<typename T, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
    struct Constructor<T(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29)>
    {
        static void f(AS_NAMESPACE_QUALIFIER asIScriptGeneric* gen)
        {
            new (gen->GetObject()) T(static_cast<Proxy<A0>*>(gen->GetAddressOfArg(0))->value, static_cast<Proxy<A1>*>(gen->GetAddressOfArg(1))->value, static_cast<Proxy<A2>*>(gen->GetAddressOfArg(2))->value, static_cast<Proxy<A3>*>(gen->GetAddressOfArg(3))->value, static_cast<Proxy<A4>*>(gen->GetAddressOfArg(4))->value, static_cast<Proxy<A5>*>(gen->GetAddressOfArg(5))->value, static_cast<Proxy<A6>*>(gen->GetAddressOfArg(6))->value, static_cast<Proxy<A7>*>(gen->GetAddressOfArg(7))->value, static_cast<Proxy<A8>*>(gen->GetAddressOfArg(8))->value, static_cast<Proxy<A9>*>(gen->GetAddressOfArg(9))->value, static_cast<Proxy<A10>*>(gen->GetAddressOfArg(10))->value, static_cast<Proxy<A11>*>(gen->GetAddressOfArg(11))->value, static_cast<Proxy<A12>*>(gen->GetAddressOfArg(12))->value, static_cast<Proxy<A13>*>(gen->GetAddressOfArg(13))->value, static_cast<Proxy<A14>*>(gen->GetAddressOfArg(14))->value, static_cast<Proxy<A15>*>(gen->GetAddressOfArg(15))->value,
                static_cast<Proxy<A16>*>(gen->GetAddressOfArg(16))->value, static_cast<Proxy<A17>*>(gen->GetAddressOfArg(17))->value, static_cast<Proxy<A18>*>(gen->GetAddressOfArg(18))->value, static_cast<Proxy<A19>*>(gen->GetAddressOfArg(19))->value, static_cast<Proxy<A20>*>(gen->GetAddressOfArg(20))->value, static_cast<Proxy<A21>*>(gen->GetAddressOfArg(21))->value, static_cast<Proxy<A22>*>(gen->GetAddressOfArg(22))->value, static_cast<Proxy<A23>*>(gen->GetAddressOfArg(23))->value, static_cast<Proxy<A24>*>(gen->GetAddressOfArg(24))->value, static_cast<Proxy<A25>*>(gen->GetAddressOfArg(25))->value, static_cast<Proxy<A26>*>(gen->GetAddressOfArg(26))->value, static_cast<Proxy<A27>*>(gen->GetAddressOfArg(27))->value, static_cast<Proxy<A28>*>(gen->GetAddressOfArg(28))->value, static_cast<Proxy<A29>*>(gen->GetAddressOfArg(29))->value);
        }
    };
    template<typename T>
    struct Id
    {
        template<T fn_ptr>
        AS_NAMESPACE_QUALIFIER asSFuncPtr f(void)
        {
            return asFUNCTION(&Wrapper<T>::template f<fn_ptr>);
        }
        template<T fn_ptr>
        AS_NAMESPACE_QUALIFIER asSFuncPtr of(void)
        {
            return asFUNCTION(&ObjFirst<T>::template f<fn_ptr>);
        }
        template<T fn_ptr>
        AS_NAMESPACE_QUALIFIER asSFuncPtr ol(void)
        {
            return asFUNCTION(&ObjLast<T>::template f<fn_ptr>);
        }
    };

    template<typename T>
    Id<T> id(T)
    {
        return Id<T>();
    }
} // end namespace gw

END_AS_NAMESPACE
