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

#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

auto fs_make_path(string_view path) -> std::u8string
{
    FO_NO_STACK_TRACE_ENTRY();

    return {path.begin(), path.end()};
}

auto fs_path_to_string(const std::filesystem::path& path) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto u8_str = path.u8string();
    return strex(string(u8_str.begin(), u8_str.end())).normalize_path_slashes();
}

auto fs_resolve_path(string_view path) -> string
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto resolved = std::filesystem::absolute(std::filesystem::path {fs_make_path(path)}, ec);
    return !ec ? fs_path_to_string(resolved) : strex(path).normalize_path_slashes();
}

auto fs_exists(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path {fs_make_path(path)}, ec) && !ec;
}

auto fs_is_dir(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path {fs_make_path(path)}, ec) && !ec;
}

auto fs_create_directories(string_view dir) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (dir.empty()) {
        return true;
    }

    std::error_code ec;
    const auto fs_dir = std::filesystem::path {fs_make_path(dir)};
    std::filesystem::create_directories(fs_dir, ec);
    return std::filesystem::exists(fs_dir, ec) && !ec && std::filesystem::is_directory(fs_dir, ec) && !ec;
}

auto fs_last_write_time(string_view path) noexcept -> uint64
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto wt = std::filesystem::last_write_time(std::filesystem::path {fs_make_path(path)}, ec);
    return !ec ? wt.time_since_epoch().count() : 0;
}

auto fs_file_size(string_view path) noexcept -> optional<uint64>
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto size = std::filesystem::file_size(std::filesystem::path {fs_make_path(path)}, ec);
    return !ec ? optional<uint64> {size} : std::nullopt;
}

auto fs_read_file(string_view path) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto fs_path = std::filesystem::path {fs_make_path(path)};
    const auto file_size = std::filesystem::file_size(fs_path, ec);

    if (ec) {
        return std::nullopt;
    }

    FO_RUNTIME_ASSERT(std::cmp_less_equal(file_size, std::numeric_limits<size_t>::max()));

    std::ifstream file {fs_path, std::ios::binary};

    if (!file) {
        return std::nullopt;
    }

    string content;
    content.resize(static_cast<size_t>(file_size));

    if (!content.empty()) {
        file.read(content.data(), static_cast<std::streamsize>(content.size()));

        if (!file || file.gcount() != static_cast<std::streamsize>(content.size())) {
            return std::nullopt;
        }
    }

    return content;
}

auto fs_compare_file_content(string_view path, const_span<uint8> content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto existing_content = fs_read_file(path);

    if (!existing_content || existing_content->size() != content.size()) {
        return false;
    }

    return MemCompare(existing_content->data(), content.data(), content.size());
}

auto fs_write_file(string_view path, string_view content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto dir = strex(path).extract_dir().str();

    if (!dir.empty() && !fs_create_directories(dir)) {
        return false;
    }

    std::ofstream file {std::filesystem::path {fs_make_path(path)}, std::ios::binary | std::ios::trunc};

    if (!file) {
        return false;
    }

    if (!content.empty()) {
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    file.flush();
    return !!file;
}

auto fs_write_file(string_view path, const_span<uint8> content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto dir = strex(path).extract_dir().str();

    if (!dir.empty() && !fs_create_directories(dir)) {
        return false;
    }

    std::ofstream file {std::filesystem::path {fs_make_path(path)}, std::ios::binary | std::ios::trunc};

    if (!file) {
        return false;
    }

    if (!content.empty()) {
        file.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    file.flush();
    return !!file;
}

auto fs_remove_file(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto fs_path = std::filesystem::path {fs_make_path(path)};
    std::filesystem::remove(fs_path, ec);
    return !std::filesystem::exists(fs_path, ec) && !ec;
}

auto fs_remove_dir_tree(string_view dir) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto fs_dir = std::filesystem::path {fs_make_path(dir)};
    std::filesystem::remove_all(fs_dir, ec);
    return !std::filesystem::exists(fs_dir, ec) && !ec;
}

auto fs_touch_file(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto fs_path = std::filesystem::path {fs_make_path(path)};
    std::error_code ec;
    const bool exists = std::filesystem::exists(fs_path, ec);

    if (ec) {
        return false;
    }

    if (exists) {
        std::filesystem::last_write_time(fs_path, std::filesystem::file_time_type::clock::now(), ec);
        return !ec;
    }

    std::ofstream new_file {fs_path};
    return !!new_file;
}

auto fs_rename(string_view from_path, string_view to_path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    std::filesystem::rename(std::filesystem::path {fs_make_path(from_path)}, std::filesystem::path {fs_make_path(to_path)}, ec);
    return !ec;
}

auto fs_open_ifstream(string_view path, std::ios::openmode mode) -> std::ifstream
{
    FO_STACK_TRACE_ENTRY();

    return {std::filesystem::path {fs_make_path(path)}, mode};
}

static void RecursiveDirLook(string_view base_dir, string_view cur_dir, bool recursive, const FsFileVisitor& visitor)
{
    FO_STACK_TRACE_ENTRY();

    const auto full_dir = std::filesystem::path {fs_make_path(strex(base_dir).combine_path(cur_dir))};
    const auto dir_iterator = std::filesystem::directory_iterator(full_dir, std::filesystem::directory_options::follow_directory_symlink);

    for (const auto& dir_entry : dir_iterator) {
        const auto u8_str = dir_entry.path().filename().u8string();
        const auto path = string(u8_str.begin(), u8_str.end());

        if (!path.empty() && path.front() != '.' && path.front() != '~') {
            if (dir_entry.is_directory()) {
                if (path.front() != '_' && recursive) {
                    RecursiveDirLook(base_dir, strex(cur_dir).combine_path(path), recursive, visitor);
                }
            }
            else {
                const auto file_size = dir_entry.file_size();
                FO_RUNTIME_ASSERT(std::cmp_less_equal(file_size, std::numeric_limits<size_t>::max()));
                visitor(strex(cur_dir).combine_path(path), static_cast<size_t>(file_size), dir_entry.last_write_time().time_since_epoch().count());
            }
        }
    }
}

void fs_iterate_dir(string_view dir, bool recursive, const FsFileVisitor& visitor)
{
    FO_STACK_TRACE_ENTRY();

    RecursiveDirLook(dir, "", recursive, visitor);
}

auto stream_read_exact(std::istream& stream, void* buf, size_t len) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return true;
    }

    stream.read(static_cast<char*>(buf), static_cast<std::streamsize>(len));
    return !!stream && stream.gcount() == static_cast<std::streamsize>(len);
}

auto stream_get_size(std::istream& stream) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const auto cur_pos = stream.tellg();

    if (cur_pos < 0) {
        return 0;
    }

    stream.clear();
    stream.seekg(0, std::ios_base::end);

    if (!stream) {
        return 0;
    }

    const auto end_pos = stream.tellg();

    if (end_pos < 0) {
        return 0;
    }

    stream.clear();
    stream.seekg(cur_pos, std::ios_base::beg);

    if (!stream) {
        return 0;
    }

    return static_cast<size_t>(end_pos);
}

auto stream_get_read_pos(std::istream& stream) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const auto pos = stream.tellg();
    return pos >= 0 ? static_cast<size_t>(pos) : 0;
}

auto stream_set_read_pos(std::istream& stream, int32 offset, std::ios_base::seekdir origin) -> bool
{
    FO_STACK_TRACE_ENTRY();

    stream.clear();
    stream.seekg(offset, origin);
    return !!stream;
}

FO_END_NAMESPACE
