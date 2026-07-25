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

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "SmartPointers.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

// Engine exception handling
using ExceptionCallback = function<void(u8string_view message, const CatchedStackTraceData& st, bool fatal_error)>;

namespace exception_detail
{
    template<typename T>
    using plain_exception_arg_t = std::remove_cvref_t<T>;

    template<typename T>
    [[nodiscard]] auto format_exception_arg(T&& value) -> u8string
    {
        using value_type = plain_exception_arg_t<T>;

        if constexpr (std::same_as<value_type, string>) {
            return u8string {value};
        }
        else if constexpr (std::same_as<value_type, string_view>) {
            return u8string {value};
        }
        else if constexpr (std::same_as<value_type, string_view_nt>) {
            return u8string {string_view {value}};
        }
        else if constexpr (std::same_as<value_type, u8string>) {
            return std::forward<T>(value);
        }
        else if constexpr (std::same_as<value_type, u8string_view>) {
            return u8string {value};
        }
        else if constexpr (std::same_as<value_type, u8string_view_nt>) {
            return u8string {value.view()};
        }
        else if constexpr (std::convertible_to<T, u8string_view>) {
            return u8string {static_cast<u8string_view>(value)};
        }
        else {
            const std::string formatted = std::format("{}", std::forward<T>(value));
            return utf8_from_char_span(formatted);
        }
    }
}

[[noreturn]] extern void ReportExceptionAndExit(const std::exception& ex) noexcept;
extern void ReportExceptionAndContinue(const std::exception& ex) noexcept;
[[nodiscard]] extern auto exception_message_utf8(const std::exception& ex) -> u8string;
extern void SetExceptionCallback(ExceptionCallback callback) noexcept;
extern auto GetExceptionCallback() noexcept -> ExceptionCallback;
extern void InstallCrashHandlerStackForThisThread() noexcept;

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
            base_exception_name(#exception_name, FO_NAMESPACE nptr<const FO_NAMESPACE StackTraceData> {}, message, std::forward<Args>(args)...) \
        { \
        } \
        template<typename... Args> \
        exception_name(const FO_NAMESPACE StackTraceData& st, FO_NAMESPACE string_view message, Args&&... args) noexcept : \
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
        exception_name(FO_NAMESPACE string_view derived_name, FO_NAMESPACE nptr<const FO_NAMESPACE StackTraceData> st, FO_NAMESPACE string_view message, Args&&... args) noexcept : \
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
    explicit BaseEngineException(string_view name, nptr<const StackTraceData> st, string_view message, Args&&... args) noexcept :
        _name {name}
    {
        try {
            _message = message;
            _params = {exception_detail::format_exception_arg(std::forward<Args>(args))...};

            _utf8Message.assign(_name);
            _utf8Message.append(": ");
            _utf8Message.append(message);

            for (const auto& param : _params) {
                _utf8Message.append(u8"\n- ");
                _utf8Message.append(param);
            }

            _whatMessage = utf8_to_char_string(_utf8Message);
        }
        catch (...) {
            // Do nothing
        }

        if (st) {
            _stackTrace = *st;
        }
        else {
            _stackTrace = GetStackTrace();
        }
    }

    BaseEngineException(const BaseEngineException& other) noexcept :
        std::exception(other),
        _name {other._name}
    {
        try {
            _message = other._message;
            _whatMessage = other._whatMessage;
            _utf8Message = other._utf8Message;
            _params = other._params;
        }
        catch (...) {
            // Do nothing
        }

        _stackTrace = other._stackTrace;
    }

    BaseEngineException(BaseEngineException&& other) noexcept = default;

    [[nodiscard]] auto what() const noexcept -> const char* override { return !_whatMessage.empty() ? _whatMessage.c_str() : _name.c_str(); }
    [[nodiscard]] auto what_utf8() const noexcept -> u8string_view { return !_utf8Message.empty() ? _utf8Message.view() : u8"Engine exception text is unavailable"; }
    [[nodiscard]] auto name() const noexcept -> const char* { return _name.c_str(); }
    [[nodiscard]] auto message() const noexcept -> string_view { return _message; }
    [[nodiscard]] auto params() const noexcept -> const_span<u8string> { return _params; }
    [[nodiscard]] auto stack_trace() const noexcept -> const StackTraceData& { return _stackTrace; }

private:
    string _name;
    string _message {};
    string _whatMessage {};
    u8string _utf8Message {};
    vector<u8string> _params {};
    StackTraceData _stackTrace {};
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
            FO_NAMESPACE ReportExceptionAndContinue(caught_ex); \
        } \
    }

#define FO_VERIFY_AND_RETURN(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE VerificationException(__VA_ARGS__); \
        } \
        catch (const FO_NAMESPACE VerificationException& caught_ex) { \
            FO_NAMESPACE ReportExceptionAndContinue(caught_ex); \
        } \
        return; \
    }

#define FO_VERIFY_AND_RETURN_VALUE(expr, ret, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE VerificationException(__VA_ARGS__); \
        } \
        catch (const FO_NAMESPACE VerificationException& caught_ex) { \
            FO_NAMESPACE ReportExceptionAndContinue(caught_ex); \
        } \
        return ret; \
    }

#define FO_STRONG_ASSERT(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        try { \
            throw FO_NAMESPACE StrongAssertationException(__VA_ARGS__, __FILE__, __LINE__); \
        } \
        catch (const FO_NAMESPACE StrongAssertationException& caught_ex) { \
            FO_NAMESPACE ReportExceptionAndExit(caught_ex); \
        } \
    }

#define FO_UNREACHABLE_PLACE() throw FO_NAMESPACE UnreachablePlaceException(__FILE__, __LINE__)

#define FO_UNKNOWN_EXCEPTION() \
    try { \
        throw FO_NAMESPACE StrongAssertationException("Unknown exception", __FILE__, __LINE__); \
    } \
    catch (const FO_NAMESPACE StrongAssertationException& caught_ex) { \
        FO_NAMESPACE ReportExceptionAndExit(caught_ex); \
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
