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
#include "Containers.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE();

// Engine exception handling
using ExceptionCallback = function<void(string_view message, string_view traceback, bool fatal_error)>;

[[noreturn]] extern void ReportExceptionAndExit(const std::exception& ex) noexcept;
extern void ReportExceptionAndContinue(const std::exception& ex) noexcept;
extern void SetExceptionCallback(ExceptionCallback callback) noexcept;
extern auto GetExceptionCallback() noexcept -> ExceptionCallback;
[[noreturn]] extern void ReportStrongAssertAndExit(string_view message, const char* file, int32 line) noexcept;
extern void ReportVerifyFailed(string_view message, const char* file, int32 line) noexcept;
extern auto GetRealStackTrace() -> string;
extern auto FormatStackTrace(const StackTraceData& st) -> string;

#define FO_DECLARE_EXCEPTION(exception_name) FO_DECLARE_EXCEPTION_EXT(exception_name, FO_NAMESPACE BaseEngineException)

// Todo: pass name to exceptions context args
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
            base_exception_name(#exception_name, nullptr, message, std::forward<Args>(args)...) \
        { \
        } \
        template<typename... Args> \
        exception_name(const FO_NAMESPACE StackTraceData& st, FO_NAMESPACE string_view message, Args&&... args) noexcept : \
            base_exception_name(#exception_name, &st, message, std::forward<Args>(args)...) \
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
    }

class BaseEngineException : public std::exception
{
public:
    BaseEngineException() = delete;
    auto operator=(const BaseEngineException&) = delete;
    auto operator=(BaseEngineException&&) noexcept = delete;
    ~BaseEngineException() override = default;

    template<typename... Args>
    explicit BaseEngineException(const char* name, const StackTraceData* st, string_view message, Args&&... args) noexcept :
        _name {name}
    {
        try {
            _message = _name;
            _message.append(": ");
            _message.append(message);

            const vector<string> params = {string(std::format("{}", std::forward<Args>(args)))...};

            for (const auto& param : params) {
                _message.append("\n- ");
                _message.append(param);
            }
        }
        catch (...) {
            // Do nothing
        }

        if (st != nullptr) {
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
        }
        catch (...) {
            // Do nothing
        }

        _stackTrace = other._stackTrace;
    }

    BaseEngineException(BaseEngineException&& other) noexcept :
        std::exception(other),
        _name {other._name}
    {
        _message = std::move(other._message);
        _stackTrace = other._stackTrace;
    }

    [[nodiscard]] auto what() const noexcept -> const char* override { return !_message.empty() ? _message.c_str() : _name; }
    [[nodiscard]] auto StackTrace() const noexcept -> const StackTraceData& { return _stackTrace; }

private:
    const char* _name;
    string _message {};
    StackTraceData _stackTrace {};
};

#if !FO_NO_EXTRA_ASSERTS
#define FO_RUNTIME_ASSERT(expr) \
    if (!(expr)) [[unlikely]] { \
        throw FO_NAMESPACE AssertationException(#expr, __FILE__, __LINE__); \
    }
#define FO_RUNTIME_ASSERT_STR(expr, str) \
    if (!(expr)) [[unlikely]] { \
        throw FO_NAMESPACE AssertationException(str, __FILE__, __LINE__); \
    }
#define FO_RUNTIME_VERIFY(expr, ...) \
    if (!(expr)) [[unlikely]] { \
        FO_NAMESPACE ReportVerifyFailed(#expr, __FILE__, __LINE__); \
        return __VA_ARGS__; \
    }
#define FO_STRONG_ASSERT(expr) \
    if (!(expr)) [[unlikely]] { \
        FO_NAMESPACE ReportStrongAssertAndExit(#expr, __FILE__, __LINE__); \
    }
#else
#define FO_RUNTIME_ASSERT(expr)
#define FO_RUNTIME_ASSERT_STR(expr, str)
#define FO_RUNTIME_VERIFY(expr, ...)
#define FO_STRONG_ASSERT(expr)
#endif

#define FO_UNREACHABLE_PLACE() throw FO_NAMESPACE UnreachablePlaceException(__FILE__, __LINE__)
#define FO_UNKNOWN_EXCEPTION() FO_NAMESPACE ReportStrongAssertAndExit("Unknown exception", __FILE__, __LINE__)

// Common exceptions
FO_DECLARE_EXCEPTION(GenericException);
FO_DECLARE_EXCEPTION(AssertationException);
FO_DECLARE_EXCEPTION(StrongAssertationException);
FO_DECLARE_EXCEPTION(VerifyFailedException);
FO_DECLARE_EXCEPTION(UnreachablePlaceException);
FO_DECLARE_EXCEPTION(NotSupportedException);
FO_DECLARE_EXCEPTION(NotImplementedException);
FO_DECLARE_EXCEPTION(InvalidCallException);
FO_DECLARE_EXCEPTION(InvalidOperationException);

FO_END_NAMESPACE();
