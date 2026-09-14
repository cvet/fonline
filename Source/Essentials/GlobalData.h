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
    using callback = void (*)() noexcept;

    // True when this call built the set, false when it already existed: an entry point that did not build the set
    // must not tear it down, since whoever did is still using it
    auto create() -> bool;
    void destroy();

    // Reaching for global data outside its lifetime is a startup or teardown ordering defect, and carrying on
    // with a null pointer only moves the crash somewhere the cause is no longer visible, so it ends the run
    [[noreturn]] void report_misuse_and_exit(const char* class_name, const char* misuse) noexcept;

    extern callback create_callbacks[MAX_CALLBACKS];
    extern callback delete_callbacks[MAX_CALLBACKS];
    extern int32_t callbacks_count;

    // Holds one global data instance, as a raw pointer because this header sits above SmartPointers in the
    // Essentials order. Constant-initialized, so the static constructor that registers it always finds it
    template<typename T>
    class instance
    {
    public:
        constexpr explicit instance(const char* class_name) noexcept :
            _name {class_name}
        {
        }
        instance(const instance&) = delete;
        instance(instance&&) noexcept = delete;
        auto operator=(const instance&) = delete;
        auto operator=(instance&&) noexcept = delete;
        ~instance() = default;

        // The only unchecked question: whether the data is there. The log paths ask it because they must keep
        // working before the first create and after the last delete
        [[nodiscard]] auto is_created() const noexcept -> bool { return _instance != nullptr; }

        [[nodiscard]] auto operator->() const noexcept -> T*
        {
            if (_instance == nullptr) [[unlikely]] {
                report_misuse_and_exit(_name, "accessed outside its lifetime");
            }

            return _instance;
        }

        [[nodiscard]] auto operator*() const noexcept -> T& { return *operator->(); }

        // Called by the create and delete sweeps, which serialize them. Deliberately noexcept: a constructor
        // that throws would leave a half-built set of globals, and nothing can run on one
        void create_instance() noexcept
        {
            if (_instance != nullptr) [[unlikely]] {
                report_misuse_and_exit(_name, "created twice");
            }

            _instance = new T();
        }

        void delete_instance() noexcept
        {
            delete _instance;
            _instance = nullptr;
        }

    private:
        T* _instance {};
        const char* _name;
    };
}

#define FO_GLOBAL_DATA(class_name, instance_name) \
    static FO_NAMESPACE global_data::instance<class_name> instance_name {#class_name}; \
    static void FO_CONCAT(Create_, class_name)() noexcept \
    { \
        (instance_name).create_instance(); \
    } \
    static void FO_CONCAT(Delete_, class_name)() noexcept \
    { \
        (instance_name).delete_instance(); \
    } \
    struct FO_CONCAT(Register_, class_name) \
    { \
        FO_CONCAT(Register_, class_name)() \
        { \
            if (FO_NAMESPACE global_data::callbacks_count >= FO_NAMESPACE global_data::MAX_CALLBACKS) { \
                FO_NAMESPACE global_data::report_misuse_and_exit(#class_name, "does not fit: the callback table is full"); \
            } \
            FO_NAMESPACE global_data::create_callbacks[FO_NAMESPACE global_data::callbacks_count] = FO_CONCAT(Create_, class_name); \
            FO_NAMESPACE global_data::delete_callbacks[FO_NAMESPACE global_data::callbacks_count] = FO_CONCAT(Delete_, class_name); \
            FO_NAMESPACE global_data::callbacks_count++; \
        } \
    }; \
    static FO_CONCAT(Register_, class_name) FO_CONCAT(Register_Instance_, class_name)

FO_END_NAMESPACE
