//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Settings.h"

#include "scriptarray/scriptarray.h"

struct DeferredCall
{
    uint Id;
    uint FireFullSecond;
    hash FuncNum;
    uint BindId;
    bool IsValue;
    bool ValueSigned;
    int Value;
    bool IsValues;
    bool ValuesSigned;
    IntVec Values;
    bool Saved;
};
typedef list<DeferredCall> DeferredCallList;

class ScriptInvoker
{
    friend class Script;

private:
    TimerSettings& settings;
    DeferredCallList deferredCalls;

    ScriptInvoker(TimerSettings& sett);
    uint AddDeferredCall(uint delay, bool saved, asIScriptFunction* func, int* value, CScriptArray* values);
    bool IsDeferredCallPending(uint id);
    bool CancelDeferredCall(uint id);
    bool GetDeferredCallData(uint id, DeferredCall& data);
    void GetDeferredCallsList(IntVec& ids);
    void Process();
    void RunDeferredCall(DeferredCall& call);
    string GetStatistics();

    // Todo: rework FONLINE_
    /*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
        bool LoadDeferredCalls();
    #endif*/

public:
    static uint Global_DeferredCall(uint delay, asIScriptFunction* func);
    static uint Global_DeferredCallWithValue(uint delay, asIScriptFunction* func, int value);
    static uint Global_DeferredCallWithValues(uint delay, asIScriptFunction* func, CScriptArray* values);
    static uint Global_SavedDeferredCall(uint delay, asIScriptFunction* func);
    static uint Global_SavedDeferredCallWithValue(uint delay, asIScriptFunction* func, int value);
    static uint Global_SavedDeferredCallWithValues(uint delay, asIScriptFunction* func, CScriptArray* values);
    static bool Global_IsDeferredCallPending(uint id);
    static bool Global_CancelDeferredCall(uint id);
    static bool Global_GetDeferredCallData(uint id, uint& delay, CScriptArray* values);
    static uint Global_GetDeferredCallsList(CScriptArray* ids);
};
