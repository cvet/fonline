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
#include "ExceptionHandling.h"
#include "SmartPointers.h"
#include "StackTrace.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

namespace fs
{
    using file_visitor = function<void(string_view, size_t, uint64_t)>;

    // Filesystem helpers
    auto make_path(string_view path) -> std::u8string;
    auto path_to_string(const std::filesystem::path& path) -> string;
    auto resolve_path(string_view path) -> string;
    auto exists(string_view path) noexcept -> bool;
    auto is_dir(string_view path) noexcept -> bool;
    auto is_absolute_path(string_view path) noexcept -> bool;
    auto is_relative_path(string_view path) noexcept -> bool;
    auto make_writable_path(string_view user_writable_path, string_view relative) -> string;
    auto create_directories(string_view dir) noexcept -> bool;
    auto last_write_time(string_view path) noexcept -> uint64_t;
    auto file_size(string_view path) noexcept -> optional<uint64_t>;
    auto hash_file(string_view path) -> optional<uint64_t>;
    auto hash_data(const_span<uint8_t> data) noexcept -> uint64_t;
    auto read_file(string_view path) -> optional<string>;
    auto read_file_bounded(string_view path, size_t max_size) -> optional<string>;
    auto compare_file_content(string_view path, const_span<uint8_t> content) -> bool;
    auto write_file(string_view path, string_view content) -> bool;
    auto write_file(string_view path, const_span<uint8_t> content) -> bool;
    auto remove_file(string_view path) noexcept -> bool;
    auto remove_dir_tree(string_view dir) noexcept -> bool;
    auto touch_file(string_view path) noexcept -> bool;
    auto rename(string_view from_path, string_view to_path) noexcept -> bool;
    auto open_ifstream(string_view path, std::ios::openmode mode = std::ios::binary) -> std::ifstream;
    void iterate_dir(string_view dir, bool recursive, const file_visitor& visitor);

    // Stream helpers
    auto stream_read_exact(std::istream& stream, span<uint8_t> buf) -> bool;
    auto stream_get_size(std::istream& stream) -> size_t;
    auto stream_get_read_pos(std::istream& stream) -> size_t;
    auto stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool;
}

FO_END_NAMESPACE
