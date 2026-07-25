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
#include "Logging.h"
#include "SafeArithmetics.h"
#include "TextFormatting.h"

FO_BEGIN_NAMESPACE

extern void LogToFile(string_view path, bool append)
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_path = path;
    LogToFile(utf8_path, append);
}

extern void LogToFile(u8string_view path, bool append)
{
    FO_STACK_TRACE_ENTRY();

    const u8string checked_path = u8string::FromChecked(path.native_view());
    const std::filesystem::path native_path {fs_make_path(checked_path)};

    if (!base_logging_detail::OpenLogFileNative(native_path, append)) {
        const u8string message = FormatUtf8("Can't create log file '{}'\n", checked_path);
        WriteBaseLog(message);
    }
}

auto fs_make_path(u8string_view path) -> std::u8string
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::u8string {path.native_view()};
}

auto fs_path_to_u8string(const std::filesystem::path& path) -> u8string
{
    FO_NO_STACK_TRACE_ENTRY();

    return u8string::FromChecked(path.generic_u8string());
}

auto fs_resolve_path(u8string_view path) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto resolved = std::filesystem::absolute(std::filesystem::path {fs_make_path(path)}, ec);
    return !ec ? fs_path_to_u8string(resolved) : fs_path_to_u8string(std::filesystem::path {fs_make_path(path)});
}

auto fs_exists(u8string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path {fs_make_path(path)}, ec) && !ec;
}

auto fs_is_dir(u8string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path {fs_make_path(path)}, ec) && !ec;
}

auto fs_is_absolute_path(u8string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !path.empty() && std::filesystem::path {fs_make_path(path)}.is_absolute();
}

auto fs_is_relative_path(u8string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return path.empty() || std::filesystem::path {fs_make_path(path)}.is_relative();
}

auto fs_combine_path(u8string_view base_path, u8string_view relative_path) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    return fs_path_to_u8string(std::filesystem::path {fs_make_path(base_path)} / std::filesystem::path {fs_make_path(relative_path)});
}

auto fs_combine_path(u8string_view base_path, string_view ascii_relative_path) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    const u8string relative_path = ascii_relative_path;
    return fs_combine_path(base_path, relative_path.view());
}

auto fs_make_writable_path(u8string_view user_writable_path, u8string_view relative) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    // Portable layout, or an already-absolute path: leave it as-is (written next to the exe / as given).
    if (user_writable_path.empty() || fs_is_absolute_path(relative)) {
        return u8string {relative};
    }

    // Installed layout: layer the relative writable path under the writable root.
    return fs_combine_path(user_writable_path, relative);
}

auto fs_create_directories(u8string_view dir) noexcept -> bool
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

auto fs_last_write_time(u8string_view path) noexcept -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto wt = std::filesystem::last_write_time(std::filesystem::path {fs_make_path(path)}, ec);
    return !ec ? wt.time_since_epoch().count() : 0;
}

auto fs_file_size(u8string_view path) noexcept -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto size = std::filesystem::file_size(std::filesystem::path {fs_make_path(path)}, ec);
    return !ec ? optional<uint64_t> {size} : std::nullopt;
}

auto fs_read_file_bytes(u8string_view path) -> optional<vector<byte>>
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto fs_path = std::filesystem::path {fs_make_path(path)};
    const auto file_size = std::filesystem::file_size(fs_path, ec);

    if (ec) {
        return std::nullopt;
    }

    FO_VERIFY_AND_THROW(std::cmp_less_equal(file_size, std::numeric_limits<size_t>::max()), "Disk file is too large to fit into memory buffer");

    std::ifstream file {fs_path, std::ios::binary};

    if (!file) {
        return std::nullopt;
    }

    vector<byte> content;
    content.resize(static_cast<size_t>(file_size));

    if (!content.empty()) {
        auto content_chars = make_ptr(content.data()).reinterpret_as<char>();
        file.read(content_chars.get(), static_cast<std::streamsize>(content.size()));

        if (!file || file.gcount() != static_cast<std::streamsize>(content.size())) {
            return std::nullopt;
        }
    }

    return content;
}

auto fs_read_file_text(u8string_view path) -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

    const auto bytes = fs_read_file_bytes(path);

    if (!bytes) {
        return std::nullopt;
    }

    return utf8_from_byte_span(*bytes);
}

auto fs_compare_file_bytes(u8string_view path, const_span<byte> content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto existing_content = fs_read_file_bytes(path);

    if (!existing_content || existing_content->size() != content.size()) {
        return false;
    }

    if (content.empty()) {
        return true;
    }

    return MemCompare((*existing_content).data(), content.data(), content.size());
}

auto fs_write_file_bytes(u8string_view path, const_span<byte> content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto dir = std::filesystem::path {fs_make_path(path)}.parent_path();
    const u8string dir_str = fs_path_to_u8string(dir);

    if (!dir.empty() && !fs_create_directories(dir_str.view())) {
        return false;
    }

    std::ofstream file {std::filesystem::path {fs_make_path(path)}, std::ios::binary | std::ios::trunc};

    if (!file) {
        return false;
    }

    if (!content.empty()) {
        auto content_chars = make_ptr(content.data()).reinterpret_as<const char>();
        file.write(content_chars.get(), static_cast<std::streamsize>(content.size()));
    }

    file.flush();
    return !!file;
}

auto fs_write_file_text(u8string_view path, u8string_view content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (content.empty()) {
        return fs_write_file_bytes(path, {});
    }

    return fs_write_file_bytes(path, utf8_to_byte_span(content));
}

auto fs_remove_file(u8string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto fs_path = std::filesystem::path {fs_make_path(path)};
    std::filesystem::remove(fs_path, ec);
    return !std::filesystem::exists(fs_path, ec) && !ec;
}

auto fs_remove_dir_tree(u8string_view dir) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto fs_dir = std::filesystem::path {fs_make_path(dir)};
    std::filesystem::remove_all(fs_dir, ec);
    return !std::filesystem::exists(fs_dir, ec) && !ec;
}

auto fs_touch_file(u8string_view path) noexcept -> bool
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

auto fs_rename(u8string_view from_path, u8string_view to_path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    std::filesystem::rename(std::filesystem::path {fs_make_path(from_path)}, std::filesystem::path {fs_make_path(to_path)}, ec);
    return !ec;
}

auto fs_open_ifstream(u8string_view path, std::ios::openmode mode) -> std::ifstream
{
    FO_STACK_TRACE_ENTRY();

    return std::ifstream {std::filesystem::path {fs_make_path(path)}, mode};
}

auto fs_hash_file(u8string_view path) -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    // FNV-1a 64.
    constexpr uint64_t offset = UINT64_C(0xcbf29ce484222325);
    constexpr uint64_t prime = UINT64_C(0x100000001b3);

    const auto step = [](uint64_t hash, const_span<byte> bytes) noexcept {
        for (const byte data_byte : bytes) {
            hash = (hash ^ std::to_integer<uint8_t>(data_byte)) * prime;
        }
        return hash;
    };

    auto stream = fs_open_ifstream(path);

    if (!stream) {
        return std::nullopt;
    }

    array<byte, 0x10000> buf {};
    uint64_t hash = offset;

    while (stream) {
        auto read_buf = make_nptr(buf.data());
        stream.read(read_buf.reinterpret_as<char>().get(), numeric_cast<std::streamsize>(buf.size()));

        const auto read_size = numeric_cast<size_t>(stream.gcount());

        if (read_size != 0) {
            hash = step(hash, {buf.data(), read_size});
        }

        if (stream.bad()) {
            return std::nullopt;
        }
    }

    return hash;
}

auto fs_hash_bytes(const_span<byte> data) noexcept -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    // FNV-1a 64.
    constexpr uint64_t offset = UINT64_C(0xcbf29ce484222325);
    constexpr uint64_t prime = UINT64_C(0x100000001b3);

    const auto step = [](uint64_t hash, const_span<byte> bytes) noexcept {
        for (const byte data_byte : bytes) {
            hash = (hash ^ std::to_integer<uint8_t>(data_byte)) * prime;
        }
        return hash;
    };

    if (data.empty()) {
        return offset;
    }

    return step(offset, data);
}

static void RecursiveDirLook(u8string_view base_dir, u8string_view cur_dir, bool recursive, const FsFileVisitor& visitor)
{
    FO_STACK_TRACE_ENTRY();

    const auto full_dir = std::filesystem::path {fs_make_path(base_dir)} / std::filesystem::path {fs_make_path(cur_dir)};
    const auto dir_iterator = std::filesystem::directory_iterator(full_dir, std::filesystem::directory_options::follow_directory_symlink);

    for (const auto& dir_entry : dir_iterator) {
        const u8string path = fs_path_to_u8string(dir_entry.path().filename());
        const std::u8string_view path_view = path.view().native_view();

        if (!path_view.empty() && path_view.front() != u8'.' && path_view.front() != u8'~') {
            if (dir_entry.is_directory()) {
                if (path_view.front() != u8'_' && recursive) {
                    const u8string next_dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(cur_dir)} / std::filesystem::path {fs_make_path(path.view())});
                    RecursiveDirLook(base_dir, next_dir.view(), recursive, visitor);
                }
            }
            else {
                const auto file_size = dir_entry.file_size();
                FO_VERIFY_AND_THROW(std::cmp_less_equal(file_size, std::numeric_limits<size_t>::max()), "Disk file is too large to fit into memory buffer");
                const u8string relative_path = fs_path_to_u8string(std::filesystem::path {fs_make_path(cur_dir)} / std::filesystem::path {fs_make_path(path.view())});
                visitor(relative_path.view(), static_cast<size_t>(file_size), dir_entry.last_write_time().time_since_epoch().count());
            }
        }
    }
}

void fs_iterate_dir(u8string_view dir, bool recursive, const FsFileVisitor& visitor)
{
    FO_STACK_TRACE_ENTRY();

    RecursiveDirLook(dir, u8"", recursive, visitor);
}

auto stream_read_exact(std::istream& stream, span<byte> buf) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return true;
    }

    const std::streamsize stream_len = numeric_cast<std::streamsize>(buf.size());
    auto target_chars = make_ptr(buf.data()).reinterpret_as<char>();
    stream.read(target_chars.get(), stream_len);
    return !!stream && stream.gcount() == stream_len;
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

auto stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool
{
    FO_STACK_TRACE_ENTRY();

    stream.clear();
    stream.seekg(offset, origin);
    return !!stream;
}

FO_END_NAMESPACE
