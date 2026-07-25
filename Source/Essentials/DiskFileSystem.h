//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHandling.h"
#include "SmartPointers.h"
#include "StackTrace.h"
#include "StringUtils.h"
#include "TextConversions.h"

FO_BEGIN_NAMESPACE

using FsFileVisitor = function<void(u8string_view, size_t, uint64_t)>;

// Base log destination
extern void LogToFile(string_view path, bool append = false);
extern void LogToFile(u8string_view path, bool append = false);
inline void LogToFile(const string& path, bool append = false)
{
    LogToFile(string_view {path}, append);
}
inline void LogToFile(const u8string& path, bool append = false)
{
    LogToFile(path.view(), append);
}
inline void LogToFile(const strex& path, bool append = false)
{
    LogToFile(static_cast<string_view>(path), append);
}
inline void LogToFile(const u8strex& path, bool append = false)
{
    LogToFile(static_cast<u8string_view>(path), append);
}

// Filesystem helpers
auto fs_make_path(u8string_view path) -> std::u8string;
auto fs_path_to_u8string(const std::filesystem::path& path) -> u8string;
auto fs_resolve_path(u8string_view path) -> u8string;
auto fs_exists(u8string_view path) noexcept -> bool;
auto fs_is_dir(u8string_view path) noexcept -> bool;
auto fs_is_absolute_path(u8string_view path) noexcept -> bool;
auto fs_is_relative_path(u8string_view path) noexcept -> bool;
auto fs_combine_path(u8string_view base_path, u8string_view relative_path) -> u8string;
auto fs_combine_path(u8string_view base_path, string_view ascii_relative_path) -> u8string;
auto fs_make_writable_path(u8string_view user_writable_path, u8string_view relative) -> u8string;
auto fs_create_directories(u8string_view dir) noexcept -> bool;
auto fs_last_write_time(u8string_view path) noexcept -> uint64_t;
auto fs_file_size(u8string_view path) noexcept -> optional<uint64_t>;
auto fs_hash_file(u8string_view path) -> optional<uint64_t>;
auto fs_hash_bytes(const_span<byte> data) noexcept -> uint64_t;
auto fs_read_file_bytes(u8string_view path) -> optional<vector<byte>>;
auto fs_read_file_text(u8string_view path) -> optional<u8string>;
auto fs_compare_file_bytes(u8string_view path, const_span<byte> content) -> bool;
auto fs_write_file_bytes(u8string_view path, const_span<byte> content) -> bool;
auto fs_write_file_text(u8string_view path, u8string_view content) -> bool;
auto fs_remove_file(u8string_view path) noexcept -> bool;
auto fs_remove_dir_tree(u8string_view dir) noexcept -> bool;
auto fs_touch_file(u8string_view path) noexcept -> bool;
auto fs_rename(u8string_view from_path, u8string_view to_path) noexcept -> bool;
auto fs_open_ifstream(u8string_view path, std::ios::openmode mode = std::ios::binary) -> std::ifstream;
void fs_iterate_dir(u8string_view dir, bool recursive, const FsFileVisitor& visitor);

// Stream helpers
auto stream_read_exact(std::istream& stream, span<byte> buf) -> bool;
auto stream_get_size(std::istream& stream) -> size_t;
auto stream_get_read_pos(std::istream& stream) -> size_t;
auto stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool;

FO_END_NAMESPACE
