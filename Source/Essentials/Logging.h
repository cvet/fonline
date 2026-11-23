// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "BaseLogging.h"
#include "BasicCore.h"
#include "SmartPointers.h"
#include "StackTrace.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

namespace logging
{
    enum class type : uint8_t
    {
        info,
        info_section,
        warning,
        error,
    };

    using callback = function<void(type, string_view, nptr<const stack_trace::catched_data>)>;

    // Write formatted text
    void write_message(type msg_type, string_view message, nptr<const stack_trace::catched_data> st = nullptr) noexcept;

    template<typename... Args>
    void write(std::format_string<Args...>&& format, Args&&... args) noexcept
    {
        write_message(type::info, strex(strex::safe_format, std::move(format), std::forward<Args>(args)...));
    }

    template<typename... Args>
    void write(type msg_type, std::format_string<Args...>&& format, Args&&... args) noexcept
    {
        write_message(msg_type, strex(strex::safe_format, std::move(format), std::forward<Args>(args)...));
    }

    inline void write(string_view str) noexcept
    {
        write_message(type::info, strex(strex::safe_format, "{}", str));
    }

    // Control
    void set_callback(string_view key, callback log_callback);
    void disable_tags();
}

FO_END_NAMESPACE
