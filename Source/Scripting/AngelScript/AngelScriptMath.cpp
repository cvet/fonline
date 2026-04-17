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

#include "AngelScriptMath.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptHelpers.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

static auto FractionF(float32_t v) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float32_t int_part;
    return std::modf(v, &int_part);
}

static auto CloseTo(float32_t a, float32_t b, float32_t epsilon) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return is_float_equal(a, b, epsilon);
}

static auto CloseTo(float64_t a, float64_t b, float64_t epsilon) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return is_float_equal(a, b, epsilon);
}

void RegisterAngelScriptMath(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    FO_AS_VERIFY(as_engine->SetDefaultNamespace("math"));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("bool closeTo(float, float, float = 0.00001f)", FO_SCRIPT_FUNC_EXT(CloseTo, (float32_t, float32_t, float32_t), bool), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("bool closeTo(double, double, double = 0.0000000001)", FO_SCRIPT_FUNC_EXT(CloseTo, (float64_t, float64_t, float64_t), bool), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float cos(float)", FO_SCRIPT_FUNC_EXT(cosf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float sin(float)", FO_SCRIPT_FUNC_EXT(sinf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float tan(float)", FO_SCRIPT_FUNC_EXT(tanf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float acos(float)", FO_SCRIPT_FUNC_EXT(acosf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float asin(float)", FO_SCRIPT_FUNC_EXT(asinf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float atan(float)", FO_SCRIPT_FUNC_EXT(atanf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float atan2(float,float)", FO_SCRIPT_FUNC_EXT(atan2f, (float32_t, float32_t), float32_t), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float cosh(float)", FO_SCRIPT_FUNC_EXT(coshf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float sinh(float)", FO_SCRIPT_FUNC_EXT(sinhf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float tanh(float)", FO_SCRIPT_FUNC_EXT(tanhf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float log(float)", FO_SCRIPT_FUNC_EXT(logf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float log2(float)", FO_SCRIPT_FUNC_EXT(log2f, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float log10(float)", FO_SCRIPT_FUNC_EXT(log10f, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float pow(float, float)", FO_SCRIPT_FUNC_EXT(powf, (float32_t, float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float sqrt(float)", FO_SCRIPT_FUNC_EXT(sqrtf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float ceil(float)", FO_SCRIPT_FUNC_EXT(ceilf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float abs(float)", FO_SCRIPT_FUNC_EXT(fabsf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float floor(float)", FO_SCRIPT_FUNC_EXT(floorf, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("float fraction(float)", FO_SCRIPT_FUNC_EXT(FractionF, (float32_t), float32_t), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->SetDefaultNamespace(""));
}

FO_END_NAMESPACE

#endif
