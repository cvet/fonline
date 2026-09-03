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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

FO_BEGIN_NAMESPACE

namespace global_data
{
    constexpr auto MAX_CALLBACKS = 40;
    using callback = void (*)();

    void create();
    void destroy();

    extern callback create_callbacks[MAX_CALLBACKS];
    extern callback delete_callbacks[MAX_CALLBACKS];
    extern int32_t callbacks_count;
}

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
            assert(FO_NAMESPACE global_data::callbacks_count < FO_NAMESPACE global_data::MAX_CALLBACKS); \
            FO_NAMESPACE global_data::create_callbacks[FO_NAMESPACE global_data::callbacks_count] = FO_CONCAT(Create_, class_name); \
            FO_NAMESPACE global_data::delete_callbacks[FO_NAMESPACE global_data::callbacks_count] = FO_CONCAT(Delete_, class_name); \
            FO_NAMESPACE global_data::callbacks_count++; \
        } \
    }; \
    static FO_CONCAT(Register_, class_name) FO_CONCAT(Register_Instance_, class_name)

FO_END_NAMESPACE
