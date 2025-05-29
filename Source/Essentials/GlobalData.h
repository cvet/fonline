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

#include "BasicCore.h"

FO_BEGIN_NAMESPACE();

extern void CreateGlobalData();
extern void DeleteGlobalData();

#define FO_GLOBAL_DATA(class_name, instance_name) \
    static class_name* instance_name; \
    static void FO_CONCAT(Create_, class_name)() \
    { \
        assert(!(instance_name)); \
        (instance_name) = new class_name(); \
    } \
    static void FO_CONCAT(Delete_, class_name)() \
    { \
        delete (instance_name); \
        (instance_name) = nullptr; \
    } \
    struct FO_CONCAT(Register_, class_name) \
    { \
        FO_CONCAT(Register_, class_name)() \
        { \
            assert(FO_NAMESPACE GlobalDataCallbacksCount < FO_NAMESPACE MAX_GLOBAL_DATA_CALLBACKS); \
            FO_NAMESPACE CreateGlobalDataCallbacks[FO_NAMESPACE GlobalDataCallbacksCount] = FO_CONCAT(Create_, class_name); \
            FO_NAMESPACE DeleteGlobalDataCallbacks[FO_NAMESPACE GlobalDataCallbacksCount] = FO_CONCAT(Delete_, class_name); \
            FO_NAMESPACE GlobalDataCallbacksCount++; \
        } \
    }; \
    static FO_CONCAT(Register_, class_name) FO_CONCAT(Register_Instance_, class_name)

constexpr auto MAX_GLOBAL_DATA_CALLBACKS = 40;
using GlobalDataCallback = void (*)();
extern GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
extern GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
extern int32 GlobalDataCallbacksCount;

FO_END_NAMESPACE();
