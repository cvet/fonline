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
#include "Containers.h"
#include "SmartPointers.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

// Engine exception handling
using exception_callback = copyable_function<void(string_view message, const catched_stack_trace_data& st, bool fatal_error)>;

[[noreturn]] extern void report_exception_and_exit(const std::exception& ex) noexcept;
extern void report_exception_and_continue(const std::exception& ex) noexcept;
extern void set_exception_callback(exception_callback callback) noexcept;
extern auto get_exception_callback() noexcept -> exception_callback;
extern void install_crash_handler_stack_for_this_thread() noexcept;

#define FO_DECLARE_EXCEPTION(exception_name) FO_DECLARE_EXCEPTION_EXT(exception_name, FO_NAMESPACE BaseEngineException)

#define FO_DECLARE_EXCEPTION_EXT(exception_name, base_exception_name) \
    class exception_name : public base_exception_name \
    { \
    public: \
        exception_name() = delete; \
        auto operator=(const exception_name&) = delete; \
        auto operator=(exception_name&&) noexcept = delete; \
        ~exception_name() override = default; \
        template<typename... Args> \
        explicit exception_name(FO_NAMESPACE string_view message, Args&&... args) noexcept : \
            base_exception_name(#exception_name, FO_NAMESPACE nptr<const FO_NAMESPACE stack_trace_data> {}, message, std::forward<Args>(args)...) \
        { \
        } \
        template<typename... Args> \
        exception_name(const FO_NAMESPACE stack_trace_data& st, FO_NAMESPACE string_view message, Args&&... args) noexcept : \
            base_exception_name(#exception_name, FO_NAMESPACE make_nptr(&st), message, std::forward<Args>(args)...) \
        { \
        } \
        exception_name(const exception_name& other) noexcept : \
            base_exception_name(other) \
        { \
        } \
        exception_name(exception_name&& other) noexcept : \
            base_exception_name(std::move(other)) \
        { \
        } \
\
    protected: \
        template<typename... Args> \
        exception_name(FO_NAMESPACE string_view derived_name, FO_NAMESPACE nptr<const FO_NAMESPACE stack_trace_data> st, FO_NAMESPACE string_view message, Args&&... args) noexcept : \
            base_exception_name(derived_name, st, message, std::forward<Args>(args)...) \
        { \
        } \
    }

class BaseEngineException : public std::exception
{
public:
    BaseEngineException() = delete;
    auto operator=(const BaseEngineException&) = delete;
    auto operator=(BaseEngineException&&) noexcept = delete;
    ~BaseEngineException() override = default;

    template<typename... Args>
    explicit BaseEngineException(string_view name, nptr<const stack_trace_data> st, string_view message, Args&&... args) noexcept :
        _name {name}
    {
        try {
            _extended_message.assign(_name);
            _extended_message.append(": ");
            _extended_message.append(message);
            _message = message;

            // Each context argument is formatted straight into its engine-allocated element; std::format
            // would build a std::allocator string first and copy it in
            _params.reserve(sizeof...(Args));
            ((void)std::format_to(std::back_inserter(_params.emplace_back()), "{}", std::forward<Args>(args)), ...);

            for (const auto& param : _params) {
                _extended_message.append("\n- ");
                _extended_message.append(param);
            }
        }
        catch (...) {
            // Do nothing
        }

        if (st) {
            _stack_trace = *st;
        }
        else {
            _stack_trace = get_stack_trace();
        }
    }

    BaseEngineException(const BaseEngineException& other) noexcept :
        std::exception(other),
        _name {other._name}
    {
        try {
            _message = other._message;
            _extended_message = other._extended_message;
            _params = other._params;
        }
        catch (...) {
            // Do nothing
        }

        _stack_trace = other._stack_trace;
    }

    BaseEngineException(BaseEngineException&& other) noexcept = default;

    [[nodiscard]] auto what() const noexcept -> const char* override { return !_extended_message.empty() ? _extended_message.c_str() : _name.c_str(); }
    [[nodiscard]] auto name() const noexcept -> const char* { return _name.c_str(); }
    [[nodiscard]] auto message() const noexcept -> string_view { return _message; }
    [[nodiscard]] auto params() const noexcept -> const_span<string> { return _params; }
    [[nodiscard]] auto stack_trace() const noexcept -> const stack_trace_data& { return _stack_trace; }

private:
    string _name;
    string _message {};
    string _extended_message {};
    vector<string> _params {};
    stack_trace_data _stack_trace {};
};

#define FO_VERIFY_AND_THROW(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        throw FO_NAMESPACE VerificationException(__VA_ARGS__); \
    }

#define FO_VERIFY_AND_CONTINUE(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE VerificationException(__VA_ARGS__); \
        } \
        catch (const FO_NAMESPACE VerificationException& caught_ex) { \
            FO_NAMESPACE report_exception_and_continue(caught_ex); \
        } \
    }

#define FO_VERIFY_AND_RETURN(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE VerificationException(__VA_ARGS__); \
        } \
        catch (const FO_NAMESPACE VerificationException& caught_ex) { \
            FO_NAMESPACE report_exception_and_continue(caught_ex); \
        } \
        return; \
    }

#define FO_VERIFY_AND_RETURN_VALUE(expr, ret, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE VerificationException(__VA_ARGS__); \
        } \
        catch (const FO_NAMESPACE VerificationException& caught_ex) { \
            FO_NAMESPACE report_exception_and_continue(caught_ex); \
        } \
        return ret; \
    }

#define FO_STRONG_ASSERT(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE StrongAssertationException(__VA_ARGS__, __FILE__, __LINE__); \
        } \
        catch (const FO_NAMESPACE StrongAssertationException& caught_ex) { \
            FO_NAMESPACE report_exception_and_exit(caught_ex); \
        } \
    }

#define FO_UNREACHABLE_PLACE() throw FO_NAMESPACE UnreachablePlaceException(__FILE__, __LINE__)

#define FO_UNKNOWN_EXCEPTION() \
    try { \
        throw FO_NAMESPACE StrongAssertationException("Unknown exception", __FILE__, __LINE__); \
    } \
    catch (const FO_NAMESPACE StrongAssertationException& caught_ex) { \
        FO_NAMESPACE report_exception_and_exit(caught_ex); \
    }

// Common exceptions
FO_DECLARE_EXCEPTION(GenericException);
FO_DECLARE_EXCEPTION(VerificationException);
FO_DECLARE_EXCEPTION(StrongAssertationException);
FO_DECLARE_EXCEPTION(UnreachablePlaceException);
FO_DECLARE_EXCEPTION(NotSupportedException);
FO_DECLARE_EXCEPTION(NotImplementedException);
FO_DECLARE_EXCEPTION(InvalidCallException);
FO_DECLARE_EXCEPTION(InvalidOperationException);

FO_END_NAMESPACE
