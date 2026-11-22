// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "BasicCore.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

// The base tier of the logging module: writes straight to the sink, with no formatting and no callbacks,
// so a crash or out-of-memory path can still report when the layers above it cannot run
namespace logging
{
    void to_file(string_view path, bool append = false);
    [[nodiscard]] auto get_file_path() -> std::string;
    void set_async_writing(bool enabled);
    void suspend_async_writing() noexcept;
    void write_base(string_view message, const stack_trace::catched_data* st = nullptr) noexcept;
    void safe_write_stack_trace(const stack_trace::data& st) noexcept;
}

FO_END_NAMESPACE
