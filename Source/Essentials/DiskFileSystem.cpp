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

static auto fs_make_io_path(string_view path, std::error_code& ec, bool force_extended = false) -> std::filesystem::path;

auto fs::make_path(string_view path) -> std::u8string
{
    FO_NO_STACK_TRACE_ENTRY();

    return {path.begin(), path.end()};
}

auto fs::path_to_string(const std::filesystem::path& path) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    auto u8_str = path.u8string();
    return strex(string(u8_str.begin(), u8_str.end())).normalize_path_slashes();
}

auto fs::resolve_path(string_view path) -> string
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto resolved = std::filesystem::absolute(std::filesystem::path {fs::make_path(path)}, ec);
    return !ec ? fs::path_to_string(resolved) : strex(path).normalize_path_slashes();
}

// The form for a library that opens the file itself, so its separators are left as the conversion made them:
// an extended-length Windows path is literal and does not accept forward slashes
auto fs::make_io_path(string_view path) -> string
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto u8_str = fs_make_io_path(path, ec).u8string();
    return string(u8_str.begin(), u8_str.end());
}

auto fs::exists(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    return std::filesystem::exists(fs_path, ec) && !ec;
}

auto fs::is_dir(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    return std::filesystem::is_directory(fs_path, ec) && !ec;
}

auto fs::is_absolute_path(string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !path.empty() && std::filesystem::path {fs::make_path(path)}.is_absolute();
}

auto fs::is_relative_path(string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return path.empty() || std::filesystem::path {fs::make_path(path)}.is_relative();
}

auto fs::is_contained_relative_path(string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    // Portable layout, or an already-absolute path: leave it as-is (written next to the exe / as given)
    if (user_writable_path.empty() || fs::is_absolute_path(relative)) {
        return string(relative);
    }

    // Installed layout: layer the relative writable path under the writable root
    return strex(user_writable_path).combine_path(relative).str();
}

auto fs::create_directories(string_view dir) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (dir.empty()) {
        return true;
    }

    std::error_code ec;
    auto fs_dir = fs_make_io_path(dir, ec);

    if (ec) {
        return false;
    }

    std::filesystem::create_directories(fs_dir, ec);
    return std::filesystem::exists(fs_dir, ec) && !ec && std::filesystem::is_directory(fs_dir, ec) && !ec;
}

auto fs::last_write_time(string_view path) noexcept -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto info = std::filesystem::space(fs_make_io_path(path, ec), ec);

    // The path itself need not exist yet, but its directory must, or the volume cannot be identified
    return !ec ? optional<uint64_t> {numeric_cast<uint64_t>(info.available)} : std::nullopt;
}

static auto fs_read_file_impl(string_view path, optional<size_t> max_size) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return std::nullopt;
    }

    uintmax_t file_size = std::filesystem::file_size(fs_path, ec);

    if (ec) {
        return std::nullopt;
    }

    // Checked ahead of the memory-buffer guard: a bounded read answers "too large" with an empty result,
    // and must not raise on a file that only an unbounded read could have tried to hold
    if (max_size.has_value() && file_size > max_size.value()) {
        return std::nullopt;
    }

    FO_VERIFY_AND_THROW(std::cmp_less_equal(file_size, std::numeric_limits<size_t>::max()), "Disk file is too large to fit into memory buffer");

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

auto fs::read_file(string_view path) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    return fs_read_file_impl(path, std::nullopt);
}

auto fs::read_file_bounded(string_view path, size_t max_size) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    return fs_read_file_impl(path, max_size);
}

auto fs::compare_file_content(string_view path, const_span<uint8_t> content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto existing_content = fs::read_file(path);

    if (!existing_content || existing_content->size() != content.size()) {
        return false;
    }

    if (content.empty()) {
        return true;
    }

    return memory::compare((*existing_content).data(), content.data(), content.size());
}

auto fs::write_file(string_view path, string_view content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    size_t separator = path.find_last_of("/\\");
    string_view dir = separator != string_view::npos ? path.substr(0, separator) : string_view {};

    if (!dir.empty() && !fs::create_directories(dir)) {
        return false;
    }

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    std::ofstream file {fs_path, std::ios::binary | std::ios::trunc};

    if (!file) {
        return false;
    }

    if (!content.empty()) {
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    file.flush();
    return !!file;
}

auto fs::write_file(string_view path, const_span<uint8_t> content) -> bool
{
    FO_STACK_TRACE_ENTRY();

    size_t separator = path.find_last_of("/\\");
    string_view dir = separator != string_view::npos ? path.substr(0, separator) : string_view {};

    if (!dir.empty() && !fs::create_directories(dir)) {
        return false;
    }

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return false;
    }

    std::ofstream file {fs_path, std::ios::binary | std::ios::trunc};

    if (!file) {
        return false;
    }

    if (!content.empty()) {
        file.write(make_ptr(content.data()).reinterpret_as<char>().get(), static_cast<std::streamsize>(content.size()));
    }

    file.flush();
    return !!file;
}

auto fs::remove_file(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto fs_dir = fs_make_io_path(dir, ec, true);

    if (ec) {
        return false;
    }

    std::filesystem::remove_all(fs_dir, ec);
    return !std::filesystem::exists(fs_dir, ec) && !ec;
}

auto fs::touch_file(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

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

    std::ofstream new_file {fs_path};
    return !!new_file;
}

auto fs::rename(string_view from_path, string_view to_path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    string dir = strex(path).extract_dir().str();

#if FO_WINDOWS
    return winapi::sync_directory(fs::make_io_path(dir.empty() ? string_view {"."} : string_view {dir}));
#else
    return posix::sync_directory(dir);
#endif
}

auto fs::rename_durable(string_view from_path, string_view to_path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return winapi::rename_file_durable(fs::make_io_path(from_path), fs::make_io_path(to_path));
#else
    return fs::rename(from_path, to_path) && fs::sync_parent(to_path) && fs::sync_parent(from_path);
#endif
}

auto fs::open_ifstream(string_view path, std::ios::openmode mode) -> std::ifstream
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    auto fs_path = fs_make_io_path(path, ec);

    if (ec) {
        return {};
    }

    return std::ifstream {fs_path, mode};
}

auto fs::hash_file(string_view path) -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    // FNV-1a 64
    constexpr uint64_t offset = UINT64_C(0xcbf29ce484222325);
    constexpr uint64_t prime = UINT64_C(0x100000001b3);

    auto step = [](uint64_t hash, ptr<const uint8_t> bytes, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            hash = (hash ^ bytes[i]) * prime;
        }
        return hash;
    };

    auto stream = fs::open_ifstream(path);

    if (!stream) {
        return std::nullopt;
    }

    array<char, 0x10000> buf {};
    uint64_t hash = offset;

    while (stream) {
        auto read_buf = make_nptr(buf.data());
        stream.read(read_buf.get(), numeric_cast<std::streamsize>(buf.size()));

        auto read_size = numeric_cast<size_t>(stream.gcount());

        if (read_size != 0) {
            auto hash_bytes = read_buf.reinterpret_as<const uint8_t>();
            hash = step(hash, hash_bytes, read_size);
        }

        if (stream.bad()) {
            return std::nullopt;
        }
    }

    return hash;
}

auto fs::hash_data(const_span<uint8_t> data) noexcept -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    recursive_dir_look(dir, "", recursive, visitor);
}

auto fs::list_dir_file_names(string_view dir, bool recursive) noexcept -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> names;
    std::error_code ec;
    std::filesystem::path root {fs::make_path(dir)};
    auto collect = [&](auto it) {
        decltype(it) end;

        // Every step takes the error code, because the throwing increment would reach a noexcept frame
        while (!ec && it != end) {
            if (it->is_regular_file(ec) && !ec) {
                names.emplace_back(fs::path_to_string(it->path().lexically_relative(root)));
            }

            ec.clear();
            it.increment(ec);
        }
    };

    if (recursive) {
        collect(std::filesystem::recursive_directory_iterator {root, ec});
    }
    else {
        collect(std::filesystem::directory_iterator {root, ec});
    }

    return names;
}

auto fs::stream_read_exact(std::istream& stream, span<uint8_t> buf) -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    auto pos = stream.tellg();
    return pos >= 0 ? static_cast<size_t>(pos) : 0;
}

auto fs::stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool
{
    FO_STACK_TRACE_ENTRY();

    stream.clear();
    stream.seekg(offset, origin);
    return !!stream;
}

fs::disk_read_file::disk_read_file(string_view path) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    _descriptor = winapi::open_shared_read_file(fs::make_io_path(path));
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
    FO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    other._descriptor = -1;
    other._size = 0;
    other._offset = 0;
}

auto fs::disk_read_file::operator=(fs::disk_read_file&& other) noexcept -> fs::disk_read_file&
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    close();
}

auto fs::disk_read_file::read_at(uint64_t offset, span<uint8_t> buf) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(std::filesystem::path {fs::make_path(path.empty() ? "." : path)}, error);

    if (error) {
        return;
    }

    string normalized = strex(fs::path_to_string(canonical)).lower_utf8();
    uint64_t hash = fs::hash_data({reinterpret_cast<const uint8_t*>(normalized.data()), normalized.size()});
    _handle = winapi::lock_named_mutex(strex("Global\\FOnlineResourceWrite_{:016x}", hash).str());
#else
    _descriptor = posix::lock_directory(string(path));
#endif
}

fs::disk_directory_lock::~disk_directory_lock()
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    _descriptor = winapi::open_new_write_file(fs::make_io_path(path), mode == fs::disk_write_mode::append);
#else
    _descriptor = posix::open_new_write_file(string(path), mode == fs::disk_write_mode::append);
#endif
}

fs::disk_write_file::disk_write_file(fs::disk_write_file&& other) noexcept :
    _descriptor {other._descriptor}
{
    FO_NO_STACK_TRACE_ENTRY();

    other._descriptor = -1;
}

auto fs::disk_write_file::operator=(fs::disk_write_file&& other) noexcept -> fs::disk_write_file&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (this != &other) {
        close();
        _descriptor = other._descriptor;
        other._descriptor = -1;
    }

    return *this;
}

fs::disk_write_file::~disk_write_file()
{
    FO_NO_STACK_TRACE_ENTRY();

    close();
}

auto fs::disk_write_file::write(const_span<uint8_t> buf) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    if (_descriptor >= 0) {
#if FO_WINDOWS
        winapi::close_file(_descriptor);
#else
        posix::close_file(_descriptor);
#endif
        _descriptor = -1;
    }
}

static auto fs_make_io_path(string_view path, std::error_code& ec, bool force_extended) -> std::filesystem::path
{
    FO_NO_STACK_TRACE_ENTRY();

    auto fs_path = std::filesystem::path {fs::make_path(path)};
    ignore_unused(ec);
    ignore_unused(force_extended);

#if FO_WINDOWS
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
#endif

    return fs_path;
}

FO_END_NAMESPACE
