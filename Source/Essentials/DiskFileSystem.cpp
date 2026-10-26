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

#include "DiskFileSystem.h"

#include "Posix.h"
#include "SafeArithmetics.h"
#include "WinApi.h"

FO_BEGIN_NAMESPACE

static auto fs_path_error(string_view path) noexcept -> std::error_code;
static void fs_validate_path(string_view path);
static auto fs_make_io_path(string_view path, std::error_code& ec, bool force_extended = false) noexcept -> std::filesystem::path;
static auto fs_make_native_path(string_view path, std::error_code& ec) noexcept -> std::filesystem::path;
static auto fs_native_path_to_string(const std::filesystem::path& path, std::error_code& ec) noexcept -> string;
static auto fs_parent_path(const std::filesystem::path& path, std::error_code& ec) noexcept -> std::filesystem::path;
static auto fs_make_io_string(string_view path, std::error_code& ec) noexcept -> string;
static auto fs_open_read_stream(const std::filesystem::path& path) noexcept -> optional<std::ifstream>;
static auto fs_read_stream_chunk(std::ifstream& stream, span<char> buffer) noexcept -> optional<size_t>;
static auto fs_combine_native_paths(const std::filesystem::path& parent, string_view name, std::error_code& ec) noexcept -> std::filesystem::path;

fs::exclusive_file_lock::exclusive_file_lock(exclusive_file_lock&& other) noexcept :
    _descriptor {std::exchange(other._descriptor, -1)}
{
}

auto fs::exclusive_file_lock::operator=(exclusive_file_lock&& other) noexcept -> exclusive_file_lock&
{
    if (this != &other) {
        reset();
        _descriptor = std::exchange(other._descriptor, -1);
    }

    return *this;
}

fs::exclusive_file_lock::~exclusive_file_lock()
{
    reset();
}

void fs::exclusive_file_lock::reset() noexcept
{
    if (_descriptor >= 0) {
#if FO_WINDOWS
        winapi::close_exclusive_file(_descriptor);
#elif !FO_WEB
        posix::close_exclusive_file(_descriptor);
#endif
        _descriptor = -1;
    }
}

auto fs::try_lock_file(string_view path) noexcept -> exclusive_file_lock
{
    FO_TRACE_ZONE(FileSystem);

    if (path.empty() || path.ends_with('/') || path.ends_with('\\')) {
        return {};
    }

#if FO_WEB
    return {};
#else
    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec, true);

    if (ec) {
        return {};
    }

    string lock_path = fs_native_path_to_string(fs_path, ec);

    if (ec) {
        return {};
    }

#if FO_WINDOWS
    int32_t descriptor = winapi::open_exclusive_file(lock_path);
#else
    int32_t descriptor = posix::open_exclusive_file(lock_path);
#endif
    return exclusive_file_lock {descriptor};
#endif
}

auto fs::make_path(string_view path) -> std::u8string
{
    fs_validate_path(path);
    return {path.begin(), path.end()};
}

auto fs::path_to_string(const std::filesystem::path& path) -> string
{
    if (path.native().find(std::filesystem::path::value_type {}) != std::filesystem::path::string_type::npos) {
        throw std::filesystem::filesystem_error("Path contains an embedded NUL", std::make_error_code(std::errc::invalid_argument));
    }

    auto u8_str = path.u8string();
    string value {u8_str.begin(), u8_str.end()};
    fs_validate_path(value);
    return strex(std::move(value)).normalize_path_slashes();
}

auto fs::resolve_path(string_view path) -> string
{
    std::error_code ec;
    auto resolved = std::filesystem::absolute(std::filesystem::path {fs::make_path(path)}, ec);
    return !ec ? fs::path_to_string(resolved) : strex(path).normalize_path_slashes();
}

// The form for a library that opens the file itself, so its separators are left as the conversion made them:
// an extended-length Windows path is literal and does not accept forward slashes
auto fs::make_io_path(string_view path) -> string
{
    std::error_code ec;
    auto native_path = fs_make_io_path(path, ec);

    if (ec) {
        throw std::filesystem::filesystem_error("Resolve I/O path", ec);
    }

    auto u8_str = native_path.u8string();
    return string(u8_str.begin(), u8_str.end());
}

auto fs::exists(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    return std::filesystem::exists(fs_path, ec) && !ec;
}

auto fs::is_dir(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    return std::filesystem::is_directory(fs_path, ec) && !ec;
}

auto fs::is_absolute_path(string_view path) noexcept -> bool
{
    std::error_code ec;
    auto native_path = fs_make_native_path(path, ec);
    return !ec && !path.empty() && native_path.is_absolute();
}

auto fs::is_relative_path(string_view path) noexcept -> bool
{
    std::error_code ec;
    auto native_path = fs_make_native_path(path, ec);
    return !ec && native_path.is_relative();
}

auto fs::is_contained_relative_path(string_view path) noexcept -> bool
{
    // Windows calls a leading separator relative - relative to the current drive - yet it still resolves
    // from a root, so it leaves the directory behind exactly as an absolute path would
    if (path.empty() || path.front() == '/' || path.front() == '\\') {
        return false;
    }

    // Checked before the name becomes a native path: NUL would cut it short, ':' reaches a drive-relative path or
    // an alternate data stream on Windows, and malformed UTF-8 cannot be converted at all
    if (path.find('\0') != string_view::npos || path.find(':') != string_view::npos || !strvex(path).is_valid_utf8()) {
        return false;
    }

    // Refusing every '..' rather than resolving the path is deliberate: resolution depends on what exists on
    // disk, and a caller checking a name before creating it needs the answer to hold either way
    return fs::is_relative_path(path) && path.find("..") == string_view::npos;
}

auto fs::make_writable_path(string_view user_writable_path, string_view relative) -> string
{
    fs_validate_path(user_writable_path);
    fs_validate_path(relative);

    // Portable layout, or an already-absolute path: leave it as-is (written next to the exe / as given)
    if (user_writable_path.empty() || std::filesystem::path {fs::make_path(relative)}.is_absolute()) {
        return string(relative);
    }

    // Installed layout: layer the relative writable path under the writable root
    return strex(user_writable_path).combine_path(relative).str();
}

auto fs::create_directories(string_view dir) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (dir.empty()) {
        return true;
    }

    std::error_code ec;
    auto fs_dir = fs_make_io_path(dir, ec);

    if (ec) {
        return false;
    }

    // The error-code overload can still allocate while walking parent paths
    try {
        std::filesystem::create_directories(fs_dir, ec);
    }
    catch (...) {
        return false;
    }

    return std::filesystem::exists(fs_dir, ec) && !ec && std::filesystem::is_directory(fs_dir, ec) && !ec;
}

auto fs::last_write_time(string_view path) noexcept -> uint64_t
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return 0;
    }

    auto wt = std::filesystem::last_write_time(fs_path, ec);
    return !ec ? wt.time_since_epoch().count() : 0;
}

auto fs::file_size(string_view path) noexcept -> optional<uint64_t>
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return std::nullopt;
    }

    uintmax_t size = std::filesystem::file_size(fs_path, ec);
    return !ec ? optional<uint64_t> {size} : std::nullopt;
}

auto fs::available_space(string_view path) noexcept -> optional<uint64_t>
{
    std::error_code ec;
    auto native_path = fs_make_io_path(path, ec);

    if (ec) {
        return std::nullopt;
    }

    auto info = std::filesystem::space(native_path, ec);

    // The path itself need not exist yet, but its directory must, or the volume cannot be identified
    return !ec ? optional<uint64_t> {numeric_cast<uint64_t>(info.available)} : std::nullopt;
}

static auto fs_read_file_impl(string_view path, optional<size_t> max_size) noexcept -> optional<string>
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return std::nullopt;
    }

    uintmax_t file_size = std::filesystem::file_size(fs_path, ec);

    if (ec || (max_size.has_value() && file_size > max_size.value()) || std::cmp_greater(file_size, string::max_size())) {
        return std::nullopt;
    }

    auto file = fs_open_read_stream(fs_path);

    if (!file || !*file) {
        return std::nullopt;
    }

    string content;
    content.resize(numeric_cast<size_t>(file_size));
    size_t done = 0;

    while (done < content.size()) {
        size_t count = std::min<size_t>(0x10000, content.size() - done);
        auto read = fs_read_stream_chunk(*file, span<char> {content.data() + done, count});

        if (!read || *read != count) {
            return std::nullopt;
        }

        done += count;
    }

    return content;
}

auto fs::read_file(string_view path) noexcept -> optional<string>
{
    return fs_read_file_impl(path, std::nullopt);
}

auto fs::read_file_bounded(string_view path, size_t max_size) noexcept -> optional<string>
{
    return fs_read_file_impl(path, max_size);
}

auto fs::compare_file_content(string_view path, const_span<uint8_t> content) noexcept -> bool
{
    auto existing_content = fs::read_file(path);

    if (!existing_content || existing_content->size() != content.size()) {
        return false;
    }

    if (content.empty()) {
        return true;
    }

    return memory::compare((*existing_content).data(), content.data(), content.size());
}

auto fs::write_file(string_view path, string_view content) noexcept -> bool
{
    return fs::write_file(path, const_span<uint8_t> {make_nptr(content.data()).reinterpret_as<const uint8_t>().get(), content.size()});
}

auto fs::write_file(string_view path, const_span<uint8_t> content) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto native_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    auto native_parent = fs_parent_path(native_path, ec);

    if (ec) {
        return false;
    }

    string dir = fs_native_path_to_string(native_parent, ec);

    if (ec) {
        return false;
    }

    if (!dir.empty() && !fs::create_directories(dir)) {
        return false;
    }

#if FO_WINDOWS
    uint64_t process_id = winapi::get_current_process_id();
#else
    uint64_t process_id = static_cast<uint64_t>(posix::get_current_process_id());
#endif
    int64_t stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    string temporary_path;
    bool created = false;
    auto cleanup = scope_exit([&]() noexcept {
        if (created) {
            (void)fs::remove_file(temporary_path);
        }
    });

    for (uint32_t attempt = 0; attempt < 100; ++attempt) {
        string temporary_name {"._write-"};
        array<char, 32> number {};
        auto append_number = [&](auto value) noexcept {
            auto converted = std::to_chars(number.data(), number.data() + number.size(), value);

            if (converted.ec != std::errc {}) {
                return false;
            }

            temporary_name.append(number.data(), converted.ptr);
            return true;
        };

        if (!append_number(process_id)) {
            return false;
        }

        temporary_name += '-';

        if (!append_number(stamp)) {
            return false;
        }

        temporary_name += '-';

        if (!append_number(attempt)) {
            return false;
        }

        temporary_name += ".tmp";
        auto native_temporary = fs_combine_native_paths(native_parent, temporary_name, ec);

        if (ec) {
            return false;
        }

        temporary_path = fs_native_path_to_string(native_temporary, ec);

        if (ec) {
            return false;
        }

        fs::disk_write_file writer {temporary_path, fs::disk_write_mode::create_new};

        if (!writer) {
            if (fs::exists(temporary_path)) {
                continue;
            }

            return false;
        }

        created = true;

        if (!writer.write(content) || !writer.flush()) {
            return false;
        }

        writer.close();

        if (fs::replace_file(temporary_path, path)) {
            created = false;
            return true;
        }

        return false;
    }

    return false;
}

auto fs::write_file_durable_new(string_view path, const_span<uint8_t> content) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    auto native_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    auto native_parent = fs_parent_path(native_path, ec);

    if (ec) {
        return false;
    }

    string parent = fs_native_path_to_string(native_parent, ec);

    if (ec) {
        return false;
    }

    if (!parent.empty() && !fs::create_directories(parent)) {
        return false;
    }

    bool created = false;
    auto cleanup = scope_exit([&]() noexcept {
        if (created) {
            (void)fs::remove_file(path);
        }
    });
    fs::disk_write_file writer {path, fs::disk_write_mode::create_new};

    if (!writer) {
        return false;
    }

    created = true;

    if (!writer.write(content) || !writer.flush()) {
        return false;
    }

    writer.close();
    created = false;
    return true;
}

auto fs::remove_file(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    std::filesystem::remove(fs_path, ec);
    return !std::filesystem::exists(fs_path, ec) && !ec;
}

auto fs::remove_dir_tree(string_view dir) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_dir = fs_make_io_path(dir, ec, true);

    if (ec) {
        return false;
    }

    // Recursive removal owns allocating directory iterators even with an error code
    try {
        std::filesystem::remove_all(fs_dir, ec);
    }
    catch (...) {
        return false;
    }

    return !std::filesystem::exists(fs_dir, ec) && !ec;
}

auto fs::touch_file(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    bool exists = std::filesystem::exists(fs_path, ec);

    if (ec) {
        return false;
    }

    if (exists) {
        std::filesystem::last_write_time(fs_path, std::filesystem::file_time_type::clock::now(), ec);
        return !ec;
    }

    try {
        std::ofstream new_file {fs_path};
        return !!new_file;
    }
    catch (...) {
        return false;
    }
}

auto fs::rename(string_view from_path, string_view to_path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto native_from_path = fs_make_io_path(from_path, ec);

    if (ec) {
        return false;
    }

    auto native_to_path = fs_make_io_path(to_path, ec);

    if (ec) {
        return false;
    }

    std::filesystem::rename(native_from_path, native_to_path, ec);
    return !ec;
}

auto fs::sync_parent(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto native_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    auto native_parent = fs_parent_path(native_path, ec);

    if (ec) {
        return false;
    }

    string dir = fs_native_path_to_string(native_parent, ec);

    if (ec) {
        return false;
    }

#if FO_WINDOWS
    auto io_dir = fs_make_io_string(dir.empty() ? string_view {"."} : string_view {dir}, ec);
    return !ec && winapi::sync_directory(io_dir);
#else
    return posix::sync_directory(dir);
#endif
}

auto fs::rename_durable(string_view from_path, string_view to_path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

#if FO_WINDOWS
    std::error_code ec;
    auto from = fs_make_io_string(from_path, ec);

    if (ec) {
        return false;
    }

    auto to = fs_make_io_string(to_path, ec);
    return !ec && winapi::rename_file_durable(from, to);
#else
    return fs::rename(from_path, to_path) && fs::sync_parent(to_path) && fs::sync_parent(from_path);
#endif
}

auto fs::replace_file(string_view from_path, string_view to_path) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

#if FO_WINDOWS
    std::error_code ec;
    auto from = fs_make_io_string(from_path, ec);

    if (ec) {
        return false;
    }

    auto to = fs_make_io_string(to_path, ec);
    return !ec && winapi::replace_file(from, to);
#else
    return fs::rename(from_path, to_path);
#endif
}

auto fs::open_ifstream(string_view path, std::ios::openmode mode) -> std::ifstream
{
    FO_TRACE_ZONE(FileSystem);

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        std::ifstream failed;
        failed.setstate(std::ios::failbit);
        return failed;
    }

    return std::ifstream {fs_path, mode};
}

auto fs::hash_file(string_view path) noexcept -> optional<uint64_t>
{
    FO_TRACE_ZONE(FileSystem);

    // FNV-1a 64
    constexpr uint64_t offset = UINT64_C(0xcbf29ce484222325);
    constexpr uint64_t prime = UINT64_C(0x100000001b3);
    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return std::nullopt;
    }

    auto stream = fs_open_read_stream(fs_path);

    if (!stream || !*stream) {
        return std::nullopt;
    }

    array<char, 0x10000> buffer {};
    uint64_t hash = offset;

    while (true) {
        auto count = fs_read_stream_chunk(*stream, buffer);

        if (!count) {
            return std::nullopt;
        }
        if (*count == 0) {
            return hash;
        }

        auto bytes = make_ptr(buffer.data()).reinterpret_as<const uint8_t>();

        for (size_t i = 0; i < *count; ++i) {
            hash = (hash ^ bytes[i]) * prime;
        }
    }
}

auto fs::hash_data(const_span<uint8_t> data) noexcept -> uint64_t
{
    FO_TRACE_ZONE(Core);

    // FNV-1a 64
    constexpr uint64_t offset = UINT64_C(0xcbf29ce484222325);
    constexpr uint64_t prime = UINT64_C(0x100000001b3);

    auto step = [](uint64_t hash, ptr<const uint8_t> bytes, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            hash = (hash ^ bytes[i]) * prime;
        }
        return hash;
    };

    if (data.empty()) {
        return offset;
    }

    return step(offset, data.data(), data.size());
}

static void recursive_dir_look(string_view base_dir, string_view cur_dir, bool recursive, const fs::file_visitor& visitor)
{
    std::error_code ec;
    auto full_path = (std::filesystem::path {fs::make_path(base_dir)} / std::filesystem::path {fs::make_path(cur_dir)}).u8string();
    auto full_dir = fs_make_io_path(string {full_path.begin(), full_path.end()}, ec);

    if (ec) {
        throw std::filesystem::filesystem_error("Resolve directory path", full_dir, ec);
    }

    // Layered data sources may lack the requested subdirectory; other filesystem errors must propagate
    if (!std::filesystem::exists(full_dir)) {
        return;
    }

    auto dir_iterator = std::filesystem::directory_iterator(full_dir, std::filesystem::directory_options::follow_directory_symlink);

    for (const auto& dir_entry : dir_iterator) {
        auto u8_str = dir_entry.path().filename().u8string();
        string path = string(u8_str.begin(), u8_str.end());

        if (!path.empty() && path.front() != '.' && path.front() != '~') {
            if (dir_entry.is_directory()) {
                if (path.front() != '_' && recursive) {
                    recursive_dir_look(base_dir, fs::path_to_string(std::filesystem::path {fs::make_path(cur_dir)} / dir_entry.path().filename()), recursive, visitor);
                }
            }
            else {
                uintmax_t file_size = dir_entry.file_size();
                FO_VERIFY_AND_THROW(std::cmp_less_equal(file_size, std::numeric_limits<size_t>::max()), "Disk file is too large to fit into memory buffer");
                visitor(fs::path_to_string(std::filesystem::path {fs::make_path(cur_dir)} / dir_entry.path().filename()), static_cast<size_t>(file_size), dir_entry.last_write_time().time_since_epoch().count());
            }
        }
    }
}

void fs::iterate_dir(string_view dir, bool recursive, const fs::file_visitor& visitor)
{
    FO_TRACE_ZONE(FileSystem);

    recursive_dir_look(dir, "", recursive, visitor);
}

template<typename Iterator>
static auto fs_open_directory_iterator(const std::filesystem::path& root, std::error_code& ec) noexcept -> optional<Iterator>
{
    try {
        return optional<Iterator> {std::in_place, root, ec};
    }
    catch (...) {
        return std::nullopt;
    }
}

template<typename Iterator>
static auto fs_increment_directory_iterator(Iterator& iterator, std::error_code& ec) noexcept -> bool
{
    try {
        iterator.increment(ec);
    }
    catch (...) {
        return false;
    }

    return !ec;
}

static auto fs_relative_name(const std::filesystem::path& path, const std::filesystem::path& root) noexcept -> optional<string>
{
    try {
        return fs::path_to_string(path.lexically_relative(root));
    }
    catch (...) {
        return std::nullopt;
    }
}

auto fs::list_dir_file_names(string_view dir, bool recursive) noexcept -> vector<string>
{
    FO_TRACE_ZONE(FileSystem);

    vector<string> names;
    std::error_code ec;
    auto root = fs_make_io_path(dir, ec);

    if (ec) {
        return names;
    }

    auto collect = [&](auto iterator) noexcept {
        if (!iterator) {
            return;
        }

        using iterator_type = typename decltype(iterator)::value_type;
        iterator_type end;
        auto& entry = *iterator;

        while (!ec && entry != end) {
            if (entry->is_regular_file(ec) && !ec) {
                auto name = fs_relative_name(entry->path(), root);

                if (!name) {
                    return;
                }

                names.emplace_back(std::move(*name));
            }

            ec.clear();

            if (!fs_increment_directory_iterator(entry, ec)) {
                return;
            }
        }
    };

    if (recursive) {
        collect(fs_open_directory_iterator<std::filesystem::recursive_directory_iterator>(root, ec));
    }
    else {
        collect(fs_open_directory_iterator<std::filesystem::directory_iterator>(root, ec));
    }

    return names;
}

auto fs::stream_read_exact(std::istream& stream, span<uint8_t> buf) -> bool
{
    if (buf.empty()) {
        return true;
    }

    std::streamsize stream_len = numeric_cast<std::streamsize>(buf.size());
    auto target_chars = make_ptr(buf.data()).reinterpret_as<char>();
    stream.read(target_chars.get(), stream_len);
    return !!stream && stream.gcount() == stream_len;
}

auto fs::stream_get_size(std::istream& stream) -> size_t
{
    auto cur_pos = stream.tellg();

    if (cur_pos < 0) {
        return 0;
    }

    stream.clear();
    stream.seekg(0, std::ios_base::end);

    if (!stream) {
        return 0;
    }

    auto end_pos = stream.tellg();

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

auto fs::stream_get_read_pos(std::istream& stream) -> size_t
{
    auto pos = stream.tellg();
    return pos >= 0 ? static_cast<size_t>(pos) : 0;
}

auto fs::stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool
{
    stream.clear();
    stream.seekg(offset, origin);
    return !!stream;
}

fs::disk_read_file::disk_read_file(string_view path) noexcept
{
    FO_TRACE_ZONE(FileSystem);

    if (fs_path_error(path)) {
        return;
    }

#if FO_WINDOWS
    std::error_code ec;
    auto io_path = fs_make_io_string(path, ec);

    if (ec) {
        return;
    }

    _descriptor = winapi::open_shared_read_file(io_path);
#else
    _descriptor = posix::open_shared_read_file(string(path));
#endif

    if (_descriptor < 0) {
        return;
    }

#if FO_WINDOWS
    int64_t size = winapi::get_file_size(_descriptor);
#else
    int64_t size = posix::get_file_size(_descriptor);
#endif

    // A descriptor whose length cannot be read is unusable, so it closes rather than pass for an open file
    if (size < 0) {
        close();
        return;
    }

    _size = static_cast<uint64_t>(size);
}

fs::disk_read_file::disk_read_file(string_view path, uint64_t offset, uint64_t size) noexcept :
    fs::disk_read_file(path)
{
    if (_descriptor < 0 || offset > _size || size > _size - offset) {
        close();
        return;
    }

    _offset = offset;
    _size = size;
}

fs::disk_read_file::disk_read_file(fs::disk_read_file&& other) noexcept :
    _descriptor {other._descriptor},
    _size {other._size},
    _offset {other._offset}
{
    other._descriptor = -1;
    other._size = 0;
    other._offset = 0;
}

auto fs::disk_read_file::operator=(fs::disk_read_file&& other) noexcept -> fs::disk_read_file&
{
    if (this != &other) {
        close();
        _descriptor = other._descriptor;
        _size = other._size;
        _offset = other._offset;
        other._descriptor = -1;
        other._size = 0;
        other._offset = 0;
    }

    return *this;
}

fs::disk_read_file::~disk_read_file()
{
    close();
}

auto fs::disk_read_file::read_at(uint64_t offset, span<uint8_t> buf) const noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (_descriptor < 0 || offset > _size || buf.size() > _size - offset) {
        return false;
    }

    size_t done = 0;

    while (done != buf.size()) {
        auto target = make_ptr(buf.data() + done);

#if FO_WINDOWS
        int64_t read_bytes = winapi::read_file_at(_descriptor, _offset + offset + done, target, buf.size() - done);
#else
        int64_t read_bytes = posix::read_file_at(_descriptor, _offset + offset + done, target, buf.size() - done);
#endif

        // Zero means the file ended before the span did, which for a declared extent is a corrupt file
        if (read_bytes <= 0) {
            return false;
        }

        done += static_cast<size_t>(read_bytes);
    }

    return true;
}

void fs::disk_read_file::close() noexcept
{
    if (_descriptor >= 0) {
#if FO_WINDOWS
        winapi::close_file(_descriptor);
#else
        posix::close_file(_descriptor);
#endif
        _descriptor = -1;
    }

    _size = 0;
    _offset = 0;
}

fs::disk_directory_lock::disk_directory_lock(string_view path) noexcept
{
    FO_TRACE_ZONE(FileSystem);

    if (fs_path_error(path)) {
        return;
    }

#if FO_WINDOWS
    std::error_code error;
    auto native_path = fs_make_native_path(path.empty() ? "." : path, error);

    if (error) {
        return;
    }

    std::filesystem::path canonical;

    try {
        canonical = std::filesystem::weakly_canonical(native_path, error);
    }
    catch (...) {
        return;
    }

    if (error) {
        return;
    }

    auto canonical_string = fs_native_path_to_string(canonical, error);

    if (error) {
        return;
    }

    string normalized = strex(canonical_string).normalize_path_slashes().lower_utf8();
    uint64_t hash = fs::hash_data({reinterpret_cast<const uint8_t*>(normalized.data()), normalized.size()});
    string mutex_name = "Global\\FOnlineResourceWrite_";
    constexpr string_view hex_digits = "0123456789abcdef";

    for (int32_t shift = 60; shift >= 0; shift -= 4) {
        mutex_name += hex_digits[(hash >> shift) & 0xF];
    }

    _handle = winapi::lock_named_mutex(mutex_name);
#else
    _descriptor = posix::lock_directory(string(path));
#endif
}

fs::disk_directory_lock::~disk_directory_lock()
{
#if FO_WINDOWS
    winapi::unlock_named_mutex(_handle);
#else
    if (_descriptor >= 0) {
        posix::close_file(_descriptor);
    }
#endif
}

fs::disk_write_file::disk_write_file(string_view path, fs::disk_write_mode mode) noexcept
{
    FO_TRACE_ZONE(FileSystem);

    if (fs_path_error(path)) {
        return;
    }

#if FO_WINDOWS
    std::error_code ec;
    auto io_path = fs_make_io_string(path, ec);

    if (ec) {
        return;
    }

    _descriptor = winapi::open_new_write_file(io_path, mode == fs::disk_write_mode::append, mode == fs::disk_write_mode::create_new);
#else
    _descriptor = posix::open_new_write_file(string(path), mode == fs::disk_write_mode::append, mode == fs::disk_write_mode::create_new);
#endif
}

fs::disk_write_file::disk_write_file(fs::disk_write_file&& other) noexcept :
    _descriptor {other._descriptor}
{
    other._descriptor = -1;
}

auto fs::disk_write_file::operator=(fs::disk_write_file&& other) noexcept -> fs::disk_write_file&
{
    if (this != &other) {
        close();
        _descriptor = other._descriptor;
        other._descriptor = -1;
    }

    return *this;
}

fs::disk_write_file::~disk_write_file()
{
    close();
}

auto fs::disk_write_file::write(const_span<uint8_t> buf) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (_descriptor < 0) {
        return false;
    }

    size_t done = 0;

    while (done != buf.size()) {
        auto source = make_ptr(buf.data() + done).reinterpret_as<const char>();

#if FO_WINDOWS
        int64_t written = winapi::write_file_chunk(_descriptor, source, buf.size() - done);
#else
        int64_t written = posix::write_file_chunk(_descriptor, source, buf.size() - done);
#endif

        if (written <= 0) {
            return false;
        }

        done += static_cast<size_t>(written);
    }

    return true;
}

auto fs::disk_write_file::seek_to_begin() noexcept -> bool
{
    if (_descriptor < 0) {
        return false;
    }

#if FO_WINDOWS
    return winapi::seek_file_begin(_descriptor);
#else
    return posix::seek_file_begin(_descriptor);
#endif
}

auto fs::disk_write_file::truncate_to(uint64_t size) noexcept -> bool
{
    if (_descriptor < 0 || size > numeric_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return false;
    }

#if FO_WINDOWS
    return winapi::resize_file(_descriptor, size);
#else
    return posix::resize_file(_descriptor, size);
#endif
}

auto fs::disk_write_file::preallocate(uint64_t size) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (_descriptor < 0) {
        return false;
    }

#if FO_WINDOWS
    return winapi::preallocate_file(_descriptor, size);
#else
    return posix::preallocate_file(_descriptor, size);
#endif
}

auto fs::disk_write_file::flush() noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (_descriptor < 0) {
        return false;
    }

#if FO_WINDOWS
    return winapi::sync_file(_descriptor);
#else
    return posix::sync_file(_descriptor);
#endif
}

void fs::disk_write_file::close() noexcept
{
    if (_descriptor >= 0) {
#if FO_WINDOWS
        winapi::close_file(_descriptor);
#else
        posix::close_file(_descriptor);
#endif
        _descriptor = -1;
    }
}

static auto fs_path_error(string_view path) noexcept -> std::error_code
{
    std::error_code ec;

    for (size_t i = 0; i < path.length();) {
        size_t length = path.length() - i;
        uint32_t ucs = utf8::decode(make_ptr(path.data() + i), length);

        if (ucs == 0) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        if (!utf8::is_valid(ucs)) {
            // Keep scanning so an embedded NUL retains precedence over an encoding error
            ec = std::make_error_code(std::errc::illegal_byte_sequence);
        }

        i += length;
    }

    return ec;
}

static void fs_validate_path(string_view path)
{
    if (auto ec = fs_path_error(path)) {
        throw std::filesystem::filesystem_error(ec == std::errc::illegal_byte_sequence ? "Path is not valid UTF-8" : "Path contains an embedded NUL", ec);
    }
}

static auto fs_open_read_stream(const std::filesystem::path& path) noexcept -> optional<std::ifstream>
{
    // Construct in place: moving a standard file stream is not a no-throw operation
    try {
        return optional<std::ifstream> {std::in_place, path, std::ios::binary};
    }
    catch (...) {
        return std::nullopt;
    }
}

static auto fs_read_stream_chunk(std::ifstream& stream, span<char> buffer) noexcept -> optional<size_t>
{
    if (std::cmp_greater(buffer.size(), std::numeric_limits<std::streamsize>::max())) {
        return std::nullopt;
    }

    try {
        stream.read(buffer.data(), numeric_cast<std::streamsize>(buffer.size()));
    }
    catch (...) {
        return std::nullopt;
    }

    std::streamsize count = stream.gcount();

    if (stream.bad() || count < 0) {
        return std::nullopt;
    }

    return numeric_cast<size_t>(count);
}

static auto fs_combine_native_paths(const std::filesystem::path& parent, string_view name, std::error_code& ec) noexcept -> std::filesystem::path
{
    auto child = fs_make_native_path(name, ec);

    if (ec) {
        return {};
    }

    try {
        return parent / child;
    }
    catch (const std::system_error& ex) {
        ec = ex.code();
    }
    catch (const std::bad_alloc&) {
        ec = std::make_error_code(std::errc::not_enough_memory);
    }
    catch (const std::length_error&) {
        ec = std::make_error_code(std::errc::filename_too_long);
    }
    catch (...) {
        ec = std::make_error_code(std::errc::io_error);
    }

    return {};
}

static auto fs_make_native_path(string_view path, std::error_code& ec) noexcept -> std::filesystem::path
{
    ec = fs_path_error(path);

    if (ec) {
        return {};
    }

    try {
        return std::filesystem::path {std::u8string {path.begin(), path.end()}};
    }
    catch (const std::system_error& ex) {
        ec = ex.code();
    }
    catch (const std::bad_alloc&) {
        ec = std::make_error_code(std::errc::not_enough_memory);
    }
    catch (const std::length_error&) {
        ec = std::make_error_code(std::errc::filename_too_long);
    }
    catch (...) {
        ec = std::make_error_code(std::errc::io_error);
    }

    return {};
}

// Preserve native separators: a backslash is part of a POSIX filename
static auto fs_native_path_to_string(const std::filesystem::path& path, std::error_code& ec) noexcept -> string
{
    ec.clear();

    try {
        auto utf8_path = path.u8string();
        string value {utf8_path.begin(), utf8_path.end()};
        ec = fs_path_error(value);

        if (ec) {
            return {};
        }

        return value;
    }
    catch (const std::system_error& ex) {
        ec = ex.code();
    }
    catch (const std::bad_alloc&) {
        ec = std::make_error_code(std::errc::not_enough_memory);
    }
    catch (const std::length_error&) {
        ec = std::make_error_code(std::errc::filename_too_long);
    }
    catch (...) {
        ec = std::make_error_code(std::errc::io_error);
    }

    return {};
}

static auto fs_parent_path(const std::filesystem::path& path, std::error_code& ec) noexcept -> std::filesystem::path
{
    ec.clear();

    try {
        return path.parent_path();
    }
    catch (const std::bad_alloc&) {
        ec = std::make_error_code(std::errc::not_enough_memory);
    }
    catch (const std::length_error&) {
        ec = std::make_error_code(std::errc::filename_too_long);
    }
    catch (...) {
        ec = std::make_error_code(std::errc::io_error);
    }

    return {};
}

static auto fs_make_io_string(string_view path, std::error_code& ec) noexcept -> string
{
    auto native_path = fs_make_io_path(path, ec);
    return !ec ? fs_native_path_to_string(native_path, ec) : string {};
}

static auto fs_make_io_path(string_view path, std::error_code& ec, bool force_extended) noexcept -> std::filesystem::path
{
    auto fs_path = fs_make_native_path(path, ec);

    if (ec) {
        return {};
    }

#if FO_WINDOWS
    try {
        if (fs_path.empty() || fs_path.native().starts_with(LR"(\\?\)") || fs_path.native().starts_with(LR"(\\.\)")) {
            return fs_path;
        }

        // MSVC absolute calls GetFullPathNameW, preserving Win32 dot/space and separator normalization
        auto absolute_path = std::filesystem::absolute(fs_path, ec);

        if (ec) {
            return fs_path;
        }
        if (absolute_path.native().starts_with(LR"(\\.\)") || absolute_path.native().starts_with(LR"(\\?\)")) {
            return absolute_path;
        }

        // Win32 directory creation reserves room for an 8.3 name below MAX_PATH
        constexpr size_t long_path_start = 248;

        if (force_extended || fs_path.native().size() >= long_path_start || absolute_path.native().size() >= long_path_start) {
            const auto& native_path = absolute_path.native();

            if (native_path.starts_with(LR"(\\)")) {
                auto extended_path = std::filesystem::path {LR"(\\?\UNC\)"};
                extended_path.concat(native_path.begin() + 2, native_path.end());
                return extended_path;
            }

            auto extended_path = std::filesystem::path {LR"(\\?\)"};
            extended_path += absolute_path;
            return extended_path;
        }

        return fs_path;
    }
    catch (const std::system_error& ex) {
        ec = ex.code();
    }
    catch (const std::bad_alloc&) {
        ec = std::make_error_code(std::errc::not_enough_memory);
    }
    catch (const std::length_error&) {
        ec = std::make_error_code(std::errc::filename_too_long);
    }
    catch (...) {
        ec = std::make_error_code(std::errc::io_error);
    }

    return {};
#else
    ignore_unused(force_extended);
    return fs_path;
#endif
}

FO_END_NAMESPACE
