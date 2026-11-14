// FOnline Engine
// SPDX-License-Identifier: MIT

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

    class exclusive_file_lock final
    {
    public:
        exclusive_file_lock() noexcept = default;
        exclusive_file_lock(const exclusive_file_lock&) = delete;
        exclusive_file_lock(exclusive_file_lock&& other) noexcept;
        auto operator=(const exclusive_file_lock&) -> exclusive_file_lock& = delete;
        auto operator=(exclusive_file_lock&& other) noexcept -> exclusive_file_lock&;
        ~exclusive_file_lock();

        [[nodiscard]] auto owns_lock() const noexcept -> bool { return _descriptor >= 0; }

        void reset() noexcept;

    private:
        friend auto try_lock_file(string_view path) noexcept -> exclusive_file_lock;
        explicit exclusive_file_lock(int32_t descriptor) noexcept :
            _descriptor {descriptor}
        {
        }

        int32_t _descriptor {-1};
    };

    // Paths reject embedded NUL and malformed UTF-8; noexcept result-based I/O reports standard interop failures.
    // Path converters, open_ifstream and caller-owned streams/visitors may throw
    [[nodiscard]] auto make_path(string_view path) -> std::u8string;
    [[nodiscard]] auto path_to_string(const std::filesystem::path& path) -> string;
    [[nodiscard]] auto resolve_path(string_view path) -> string;
    [[nodiscard]] auto make_io_path(string_view path) -> string;
    [[nodiscard]] auto exists(string_view path) noexcept -> bool;
    [[nodiscard]] auto is_dir(string_view path) noexcept -> bool;
    [[nodiscard]] auto is_absolute_path(string_view path) noexcept -> bool;
    [[nodiscard]] auto is_relative_path(string_view path) noexcept -> bool;
    [[nodiscard]] auto is_contained_relative_path(string_view path) noexcept -> bool;
    [[nodiscard]] auto make_writable_path(string_view user_writable_path, string_view relative) -> string;
    [[nodiscard]] auto create_directories(string_view dir) noexcept -> bool;
    [[nodiscard]] auto last_write_time(string_view path) noexcept -> uint64_t;
    [[nodiscard]] auto file_size(string_view path) noexcept -> optional<uint64_t>;
    [[nodiscard]] auto available_space(string_view path) noexcept -> optional<uint64_t>;
    [[nodiscard]] auto hash_file(string_view path) noexcept -> optional<uint64_t>;
    [[nodiscard]] auto hash_data(const_span<uint8_t> data) noexcept -> uint64_t;
    [[nodiscard]] auto try_lock_file(string_view path) noexcept -> exclusive_file_lock;
    [[nodiscard]] auto read_file(string_view path) noexcept -> optional<string>;
    [[nodiscard]] auto read_file_bounded(string_view path, size_t max_size) noexcept -> optional<string>;
    [[nodiscard]] auto compare_file_content(string_view path, const_span<uint8_t> content) noexcept -> bool;
    [[nodiscard]] auto write_file(string_view path, string_view content) noexcept -> bool;
    [[nodiscard]] auto write_file(string_view path, const_span<uint8_t> content) noexcept -> bool;
    [[nodiscard]] auto write_file_durable_new(string_view path, const_span<uint8_t> content) noexcept -> bool;
    [[nodiscard]] auto remove_file(string_view path) noexcept -> bool;
    [[nodiscard]] auto remove_dir_tree(string_view dir) noexcept -> bool;
    [[nodiscard]] auto touch_file(string_view path) noexcept -> bool;
    [[nodiscard]] auto sync_parent(string_view path) noexcept -> bool;
    [[nodiscard]] auto rename_durable(string_view from_path, string_view to_path) noexcept -> bool;
    [[nodiscard]] auto rename(string_view from_path, string_view to_path) noexcept -> bool;
    [[nodiscard]] auto replace_file(string_view from_path, string_view to_path) noexcept -> bool;
    [[nodiscard]] auto open_ifstream(string_view path, std::ios::openmode mode = std::ios::binary) -> std::ifstream;
    void iterate_dir(string_view dir, bool recursive, const file_visitor& visitor);
    [[nodiscard]] auto list_dir_file_names(string_view dir, bool recursive = false) noexcept -> vector<string>;

    // Stream helpers
    [[nodiscard]] auto stream_read_exact(std::istream& stream, span<uint8_t> buf) -> bool;
    [[nodiscard]] auto stream_get_size(std::istream& stream) -> size_t;
    [[nodiscard]] auto stream_get_read_pos(std::istream& stream) -> size_t;
    [[nodiscard]] auto stream_set_read_pos(std::istream& stream, int32_t offset, std::ios_base::seekdir origin) -> bool;

    // A disk file open for reading, for the pack formats. Everything above works on whole files instead
    class disk_read_file final
    {
    public:
        disk_read_file() noexcept = default;
        // A path that cannot be opened leaves the handle closed rather than throwing
        explicit disk_read_file(string_view path) noexcept;
        disk_read_file(string_view path, uint64_t offset, uint64_t size) noexcept;
        disk_read_file(const disk_read_file&) = delete;
        disk_read_file(disk_read_file&& other) noexcept;
        auto operator=(const disk_read_file&) = delete;
        auto operator=(disk_read_file&& other) noexcept -> disk_read_file&;
        ~disk_read_file();

        [[nodiscard]] explicit operator bool() const noexcept { return _descriptor >= 0; }
        [[nodiscard]] auto get_size() const noexcept -> uint64_t { return _size; }

        // Reads exactly the requested span, or fails: a short read of a committed extent is a corrupt file. The
        // offset travels in the call, so one open file serves several threads without a cursor to share
        [[nodiscard]] auto read_at(uint64_t offset, span<uint8_t> buf) const noexcept -> bool;
        // Ends the file's life for every reader, so it belongs with destruction rather than beside a read
        void close() noexcept;

    private:
        int32_t _descriptor {-1};
        uint64_t _size {};
        uint64_t _offset {};
    };

    // Writers exclude other writers before truncation or append; readers may keep a captured committed view
    class disk_directory_lock final
    {
    public:
        explicit disk_directory_lock(string_view path) noexcept;
        disk_directory_lock(const disk_directory_lock&) = delete;
        disk_directory_lock(disk_directory_lock&&) = delete;
        auto operator=(const disk_directory_lock&) = delete;
        auto operator=(disk_directory_lock&&) = delete;
        ~disk_directory_lock();

        [[nodiscard]] explicit operator bool() const noexcept
        {
#if FO_WINDOWS
            return static_cast<bool>(_handle);
#else
            return _descriptor >= 0;
#endif
        }

    private:
#if FO_WINDOWS
        nptr<void> _handle {};
#else
        int32_t _descriptor {-1};
#endif
    };

    enum class disk_write_mode : uint8_t
    {
        create,
        append,
        create_new,
    };

    class disk_write_file final
    {
    public:
        disk_write_file() noexcept = default;
        // A path that cannot be opened leaves the handle closed
        explicit disk_write_file(string_view path, disk_write_mode mode = disk_write_mode::create) noexcept;
        disk_write_file(const disk_write_file&) = delete;
        disk_write_file(disk_write_file&& other) noexcept;
        auto operator=(const disk_write_file&) = delete;
        auto operator=(disk_write_file&& other) noexcept -> disk_write_file&;
        ~disk_write_file();

        [[nodiscard]] explicit operator bool() const noexcept { return _descriptor >= 0; }

        [[nodiscard]] auto write(const_span<uint8_t> buf) noexcept -> bool;
        // Rewinds to patch a header the writer could not fill in until everything after it was known
        [[nodiscard]] auto seek_to_begin() noexcept -> bool;
        [[nodiscard]] auto truncate_to(uint64_t size) noexcept -> bool;
        // Claims the space up front so a long write fails early instead of part way through
        [[nodiscard]] auto preallocate(uint64_t size) noexcept -> bool;
        [[nodiscard]] auto flush() noexcept -> bool;
        void close() noexcept;

    private:
        int32_t _descriptor {-1};
    };
}

FO_END_NAMESPACE
