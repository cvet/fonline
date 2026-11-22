// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "BasicCore.h"

FO_BEGIN_NAMESPACE

namespace fatal
{
    [[noreturn]] void report_and_exit(string_view message) noexcept;
    [[noreturn]] void report_strong_assert_and_exit(string_view expression, string_view file, int32_t line) noexcept;
}

#define FO_BASIC_STRONG_ASSERT(expr) \
    if (!(expr)) [[unlikely]] { \
        FO_NAMESPACE fatal::report_strong_assert_and_exit(#expr, __FILE__, __LINE__); \
    }

FO_END_NAMESPACE
