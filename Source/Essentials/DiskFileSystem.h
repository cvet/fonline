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

using FsFileVisitor = function<void(string_view, size_t, uint64_t)>;

// Filesystem helpers
auto fs_make_path(string_view path) -> std::u8string;
auto fs_path_to_string(const std::filesystem::path& path) -> string;
auto fs_resolve_path(string_view path) -> string;
auto fs_exists(string_view path) noexcept -> bool;
auto fs_is_dir(string_view path) noexcept -> bool;
auto fs_is_absolute_path(string_view path) noexcept -> bool;
auto fs_is_relative_path(string_view path) noexcept -> bool;
auto fs_is_contained_relative_path(string_view path) noexcept -> bool;
auto fs_make_writable_path(string_view user_writable_path, string_view relative) -> string;
auto fs_create_directories(string_view dir) noexcept -> bool;
auto fs_last_write_time(string_view path) noexcept -> uint64_t;
auto fs_file_size(string_view path) noexcept -> optional<uint64_t>;
auto fs_available_space(string_view path) noexcept -> optional<uint64_t>;
auto fs_hash_file(string_view path) -> optional<uint64_t>;
auto fs_hash_data(const_span<uint8_t> data) noexcept -> uint64_t;
auto fs_read_file(string_view path) -> optional<string>;
auto fs_read_file_bounded(string_view path, size_t max_size) -> optional<string>;
auto fs_compare_file_content(string_view path, const_span<uint8_t> content) -> bool;
auto fs_write_file(string_view path, string_view content) -> bool;
auto fs_write_file(string_view path, const_span<uint8_t> content) -> bool;
auto fs_remove_file(string_view path) noexcept -> bool;
auto fs_remove_dir_tree(string_view dir) noexcept -> bool;
auto fs_touch_file(string_view path) noexcept -> bool;
auto fs_rename(string_view from_path, string_view to_path) noexcept -> bool;
auto fs_open_ifstream(string_view path, std::ios::openmode mode = std::ios::binary) -> std::ifstream;
void fs_iterate_dir(string_view dir, bool recursive, const FsFileVisitor& visitor);
auto fs_list_dir_file_names(string_view dir) noexcept -> vector<string>;

// Stream helpers
auto stream_read_exact(std::istream& stream, span<uint8_t> buf) -> bool;
auto stream_get_size(std::istream& stream) -> size_t;
auto stream_get_read_pos(std::istream& stream) -> size_t;
auto stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool;

// A disk file open for reading, for the pack formats. Everything above works on whole files instead
class disk_read_file final
{
public:
    disk_read_file() noexcept = default;
    // A path that cannot be opened leaves the handle closed rather than throwing
    explicit disk_read_file(string_view path) noexcept;
    disk_read_file(const disk_read_file&) = delete;
    disk_read_file(disk_read_file&& other) noexcept;
    auto operator=(const disk_read_file&) = delete;
    auto operator=(disk_read_file&& other) noexcept -> disk_read_file&;
    ~disk_read_file();

    [[nodiscard]] explicit operator bool() const noexcept { return _descriptor >= 0; }
    [[nodiscard]] auto get_size() const noexcept -> uint64_t { return _size; }

    // Reads exactly the requested span, or fails: a short read of a committed extent is a corrupt file. The
    // offset travels in the call, so one open file serves several threads without a cursor to share
    auto read_at(uint64_t offset, span<uint8_t> buf) const noexcept -> bool;
    // Ends the file's life for every reader, so it belongs with destruction rather than beside a read
    void close() noexcept;

private:
    int32_t _descriptor {-1};
    uint64_t _size {};
};

// A disk file open for writing, truncated to empty on open. One cursor and therefore one thread: writes append
// at it and a header patch rewinds it, so a writer is never shared
class disk_write_file final
{
public:
    disk_write_file() noexcept = default;
    // A path that cannot be opened leaves the handle closed
    explicit disk_write_file(string_view path) noexcept;
    disk_write_file(const disk_write_file&) = delete;
    disk_write_file(disk_write_file&& other) noexcept;
    auto operator=(const disk_write_file&) = delete;
    auto operator=(disk_write_file&& other) noexcept -> disk_write_file&;
    ~disk_write_file();

    [[nodiscard]] explicit operator bool() const noexcept { return _descriptor >= 0; }

    auto write(const_span<uint8_t> buf) noexcept -> bool;
    // Rewinds to patch a header the writer could not fill in until everything after it was known
    auto seek_to_begin() noexcept -> bool;
    // Claims the space up front so a long write fails early instead of part way through
    auto preallocate(uint64_t size) noexcept -> bool;
    auto flush() noexcept -> bool;
    void close() noexcept;

private:
    int32_t _descriptor {-1};
};

FO_END_NAMESPACE
