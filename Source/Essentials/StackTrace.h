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

namespace stack_trace
{
    inline constexpr size_t MAX_NATIVE_FRAMES = 128;
    inline constexpr size_t RESOLVE_CACHE_MAX_ENTRIES = 4096;

    using native_frame_address = uintptr_t;

    struct frame
    {
        enum class frame_type : uint8_t
        {
            native,
            script,
        };

        frame_type type {frame_type::native};
        std::string function {};
        std::string file {};
        uint32_t line {};
    };

    struct script_layer
    {
        std::vector<frame> script_frames {};
        std::array<native_frame_address, MAX_NATIVE_FRAMES> birth_native_frames {};
        uint32_t birth_native_frame_count {};
        bool birth_native_truncated {};
    };

    struct data
    {
        std::array<native_frame_address, MAX_NATIVE_FRAMES> native_frames {};
        uint32_t native_frame_count {};
        bool native_truncated {};
        std::shared_ptr<const std::vector<script_layer>> script_layers {};
    };

    struct catched_data
    {
        optional<data> origin {};
        data catched {};
    };

    using script_provider = std::function<void(std::vector<script_layer>& out_layers)>;

    void set_script_provider(script_provider provider) noexcept;
    auto has_script_provider() noexcept -> bool;
    auto get() noexcept -> data;
    void capture_native_frames(std::array<native_frame_address, MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip = 0) noexcept;
    void clear_resolved_cache() noexcept;
    auto get_resolved_cache_size() noexcept -> size_t;
    auto resolve(const data& st) -> std::vector<frame>;
    auto get_entry(uint32_t deep) noexcept -> std::optional<frame>;
    auto format(const data& st) -> std::string;
    auto format(const catched_data& st) -> std::string;
}

#if FO_TRACY
#define FO_STACK_TRACE_ENTRY() ZoneScoped
#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#else
#define FO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY_NAMED(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()

FO_END_NAMESPACE
