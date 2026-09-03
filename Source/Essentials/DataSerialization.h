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
#include "ExceptionHandling.h"
#include "SafeArithmetics.h"

FO_BEGIN_NAMESPACE

// Data serialization helpers
FO_DECLARE_EXCEPTION(DataReadingException);

inline void span_align_pos(size_t& pos, size_t alignment)
{
    FO_VERIFY_AND_THROW(alignment != 0, "Span alignment is zero");
    FO_VERIFY_AND_THROW((alignment & (alignment - 1)) == 0, "Span alignment is not power of two");

    pos = align_up(pos, alignment);
}

[[nodiscard]] inline auto span_read_bytes(const_span<uint8_t> buffer, size_t& pos, size_t size) -> const_span<uint8_t>
{
    if (pos > buffer.size() || size > buffer.size() - pos) {
        throw DataReadingException("Unexpected end of buffer");
    }

    const_span<uint8_t> bytes = buffer.subspan(pos, size);
    pos += size;
    return bytes;
}

[[nodiscard]] inline auto span_read_bytes(span<uint8_t> buffer, size_t& pos, size_t size) -> span<uint8_t>
{
    if (pos > buffer.size() || size > buffer.size() - pos) {
        throw DataReadingException("Unexpected end of buffer");
    }

    span<uint8_t> bytes = buffer.subspan(pos, size);
    pos += size;
    return bytes;
}

[[nodiscard]] inline auto span_read_aligned_bytes(const_span<uint8_t> buffer, size_t& pos, size_t size, size_t alignment) -> const_span<uint8_t>
{
    if (size != 0) {
        span_align_pos(pos, alignment);
    }

    return span_read_bytes(buffer, pos, size);
}

[[nodiscard]] inline auto span_read_aligned_bytes(span<uint8_t> buffer, size_t& pos, size_t size, size_t alignment) -> span<uint8_t>
{
    if (size != 0) {
        span_align_pos(pos, alignment);
    }

    return span_read_bytes(buffer, pos, size);
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
[[nodiscard]] auto span_read_object(const_span<uint8_t> buffer, size_t& pos) -> T
{
    static_assert(std::is_trivially_copyable_v<T>);

    T data;
    const_span<uint8_t> bytes = span_read_bytes(buffer, pos, sizeof(T));
    memory::copy(&data, bytes.data(), sizeof(T));
    return data;
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
[[nodiscard]] auto span_read_aligned_object(const_span<uint8_t> buffer, size_t& pos, size_t alignment) -> T
{
    static_assert(std::is_trivially_copyable_v<T>);

    span_align_pos(pos, alignment);
    return span_read_object<T>(buffer, pos);
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
[[nodiscard]] auto span_read_aligned_object(const_span<uint8_t> buffer, size_t& pos) -> T
{
    return span_read_aligned_object<T>(buffer, pos, alignment_for_size(sizeof(T)));
}

[[nodiscard]] inline auto span_read_string(const_span<uint8_t> buffer, size_t& pos, size_t size) -> string
{
    if (size == 0) {
        return {};
    }

    const_span<uint8_t> bytes = span_read_bytes(buffer, pos, size);
    auto bytes_data = make_ptr(bytes.data());
    auto chars = bytes_data.reinterpret_as<const char>();
    return {chars.get(), bytes.size()};
}

[[nodiscard]] inline auto span_write_bytes(span<uint8_t> buffer, size_t& pos, size_t size) -> span<uint8_t>
{
    FO_VERIFY_AND_THROW(pos <= buffer.size(), "Write position past buffer end");
    FO_VERIFY_AND_THROW(size <= buffer.size() - pos, "Write size exceeds remaining buffer");

    span<uint8_t> bytes = buffer.subspan(pos, size);
    pos += size;
    return bytes;
}

inline void span_write_bytes(span<uint8_t> buffer, size_t& pos, const_span<uint8_t> data)
{
    span<uint8_t> bytes = span_write_bytes(buffer, pos, data.size());
    memory::copy(bytes.data(), data.data(), data.size());
}

inline void span_write_bytes(span<uint8_t> buffer, size_t& pos, nptr<const void> source, size_t size)
{
    span<uint8_t> bytes = span_write_bytes(buffer, pos, size);
    memory::copy(bytes.data(), source, size);
}

[[nodiscard]] inline auto span_write_aligned_bytes(span<uint8_t> buffer, size_t& pos, size_t size, size_t alignment) -> span<uint8_t>
{
    if (size != 0) {
        span_align_pos(pos, alignment);
    }

    return span_write_bytes(buffer, pos, size);
}

inline void span_write_aligned_bytes(span<uint8_t> buffer, size_t& pos, const_span<uint8_t> data, size_t alignment)
{
    span<uint8_t> bytes = span_write_aligned_bytes(buffer, pos, data.size(), alignment);
    memory::copy(bytes.data(), data.data(), data.size());
}

inline void span_write_aligned_bytes(span<uint8_t> buffer, size_t& pos, nptr<const void> source, size_t size, size_t alignment)
{
    span<uint8_t> bytes = span_write_aligned_bytes(buffer, pos, size, alignment);
    memory::copy(bytes.data(), source, size);
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
void span_write_object(span<uint8_t> buffer, size_t& pos, const T& data)
{
    span_write_bytes(buffer, pos, make_nptr(&data).void_cast(), sizeof(T));
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
void span_write_object_bytes(span<uint8_t> buffer, size_t& pos, const T& data, size_t size)
{
    static_assert(std::is_trivially_copyable_v<T>);
    FO_VERIFY_AND_THROW(size <= sizeof(T), "Write size exceeds value type size");

    span_write_bytes(buffer, pos, make_nptr(&data).void_cast(), size);
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
void span_write_aligned_object(span<uint8_t> buffer, size_t& pos, const T& data, size_t alignment)
{
    span_align_pos(pos, alignment);
    span_write_object(buffer, pos, data);
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
void span_write_aligned_object(span<uint8_t> buffer, size_t& pos, const T& data)
{
    span_write_aligned_object(buffer, pos, data, alignment_for_size(sizeof(T)));
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
void span_write_aligned_object_bytes(span<uint8_t> buffer, size_t& pos, const T& data, size_t size, size_t alignment)
{
    static_assert(std::is_trivially_copyable_v<T>);
    FO_VERIFY_AND_THROW(size <= sizeof(T), "Write size exceeds value type size");

    if (size != 0) {
        span_align_pos(pos, alignment);
    }

    span_write_object_bytes(buffer, pos, data, size);
}

template<typename T>
    requires(std::is_standard_layout_v<T>)
void span_write_aligned_object_bytes(span<uint8_t> buffer, size_t& pos, const T& data, size_t size)
{
    span_write_aligned_object_bytes(buffer, pos, data, size, alignment_for_size(size));
}

inline void span_write_string(span<uint8_t> buffer, size_t& pos, string_view value)
{
    span_write_bytes(buffer, pos, make_nptr(value.data()), value.length());
}

class data_reader
{
public:
    explicit data_reader(const_span<uint8_t> buf) :
        _data_buf {buf}
    {
    }

    [[nodiscard]] auto get_unread_size() const noexcept -> size_t { return _data_buf.size() - _read_pos; }

    // Preflights an untrusted element count before a caller allocates or loops. Variable-size items pass their
    // minimum wire size
    void verify_payload_count(size_t count, size_t minimum_item_size) const
    {
        FO_VERIFY_AND_THROW(minimum_item_size != 0, "Payload minimum item size is zero");

        if (count > get_unread_size() / minimum_item_size) {
            throw DataReadingException("Payload element count exceeds remaining buffer");
        }
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    auto read() -> T
    {
        static_assert(std::is_trivially_copyable_v<T>);

        return span_read_object<T>(_data_buf, _read_pos);
    }

    auto read_bytes(size_t size) -> const_span<uint8_t> { return span_read_bytes(_data_buf, _read_pos, size); }

    void read_bytes(span<uint8_t> out)
    {
        const_span<uint8_t> bytes = read_bytes(out.size());
        copy_bytes_to(out, bytes);
    }

    void read_string_bytes(string& out)
    {
        if (!out.empty()) {
            read_bytes({make_ptr(out.data()).reinterpret_as<uint8_t>().get(), out.size()});
        }
    }

    // Reads a zero-copy view of the next `size` bytes as text (no length prefix); the view borrows the underlying buffer
    auto read_string_view(size_t size) -> string_view
    {
        const_span<uint8_t> bytes = read_bytes(size);

        if (bytes.empty()) {
            return {};
        }

        auto bytes_data = make_ptr(bytes.data());
        auto chars = bytes_data.reinterpret_as<const char>();
        return {chars.get(), bytes.size()};
    }

    // Reads a self-describing string written with write_string (uint32 length prefix + bytes)
    auto read_string() -> string
    {
        uint32_t len = read<uint32_t>();

        if (len > get_unread_size()) {
            throw DataReadingException("String length exceeds remaining buffer");
        }

        string value;
        value.resize(len);
        read_string_bytes(value);
        return value;
    }

    // Reads a self-describing vector of strings written with write_string_vector (uint32 count + each element via read_string)
    auto read_string_vector() -> vector<string>
    {
        uint32_t count = read<uint32_t>();

        verify_payload_count(numeric_cast<size_t>(count), sizeof(uint32_t));

        vector<string> values;
        values.reserve(count);

        for (uint32_t i = 0; i < count; i++) {
            values.emplace_back(read_string());
        }

        return values;
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    void read_object_array(span<T> out)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (!out.empty()) {
            verify_payload_count(out.size(), sizeof(T));

            auto target = mutable_objects_ptr(out);
            ptr<uint8_t> bytes = target.template reinterpret_as<uint8_t>();
            read_bytes({bytes.get(), out.size() * sizeof(T)});
        }
    }

    // Reads a self-describing vector of trivially-copyable objects written with write_sized_object_vector (uint32 count + elements)
    template<typename T>
        requires(std::is_standard_layout_v<T>)
    auto read_sized_object_vector() -> vector<T>
    {
        static_assert(std::is_trivially_copyable_v<T>);

        uint32_t count = read<uint32_t>();

        verify_payload_count(numeric_cast<size_t>(count), sizeof(T));

        vector<T> values;
        values.resize(count);
        read_object_array(span<T> {values});
        return values;
    }

    template<typename T>
        requires(std::same_as<T, uint8_t> || std::same_as<T, char> || std::is_void_v<T>)
    auto read_ptr(size_t size) -> nptr<const T>
    {
        const_span<uint8_t> bytes = read_bytes(size);

        if (bytes.empty()) {
            return nullptr;
        }

        return make_ptr(bytes.data()).reinterpret_as<const T>();
    }

    void read_ptr(nptr<void> out, size_t size)
    {
        const_span<uint8_t> bytes = read_bytes(size);

        if (!bytes.empty()) {
            FO_VERIFY_AND_THROW(out, "Output pointer is null");
            auto target = ptr<void> {out};
            auto target_bytes = target.reinterpret_as<uint8_t>();
            copy_bytes_to({target_bytes.get(), bytes.size()}, bytes);
        }
    }

    void verify_end() const
    {
        if (_read_pos != _data_buf.size()) {
            throw DataReadingException("Not all data read");
        }
    }

private:
    static void copy_bytes_to(span<uint8_t> out, const_span<uint8_t> bytes)
    {
        FO_VERIFY_AND_THROW(out.size() == bytes.size(), "Output and source sizes differ");

        size_t pos = 0;
        span_write_bytes(out, pos, bytes);
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    static auto mutable_objects_ptr(span<T> data) -> ptr<T>
    {
        static_assert(std::is_trivially_copyable_v<T>);
        FO_VERIFY_AND_THROW(!data.empty(), "Object span is empty");

        return data.data();
    }

    const_span<uint8_t> _data_buf;
    size_t _read_pos {};
};

class data_writer
{
public:
    static constexpr size_t BUF_RESERVE_SIZE = 1024;

    explicit data_writer(vector<uint8_t>& buf) :
        _data_buf {&buf}
    {
        _data_buf->reserve(BUF_RESERVE_SIZE);
    }
    data_writer(const data_writer&) = delete;
    data_writer(data_writer&&) noexcept = delete;
    auto operator=(const data_writer&) = delete;
    auto operator=(data_writer&&) noexcept = delete;
    ~data_writer() = default;

    template<typename T, typename U>
        requires(std::is_standard_layout_v<T> && std::same_as<T, U>)
    void write(U data)
    {
        span<uint8_t> bytes = append_bytes(sizeof(T));
        size_t pos = 0;
        span_write_object<T>(bytes, pos, data);
    }

    void write_bytes(const_span<uint8_t> data)
    {
        if (!data.empty()) {
            span<uint8_t> bytes = append_bytes(data.size());
            size_t pos = 0;
            span_write_bytes(bytes, pos, data);
        }
    }

    void write_string_bytes(string_view data)
    {
        if (!data.empty()) {
            write_bytes({make_ptr(data.data()).reinterpret_as<uint8_t>().get(), data.size()});
        }
    }

    // Writes a self-describing string (uint32 length prefix + bytes); read back with data_reader::read_string
    void write_string(string_view data)
    {
        write<uint32_t>(numeric_cast<uint32_t>(data.length()));
        write_string_bytes(data);
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    void write_object_array(const_span<T> data)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (!data.empty()) {
            auto source = source_objects_ptr(data);
            ptr<const uint8_t> bytes = source.template reinterpret_as<const uint8_t>();
            write_bytes({bytes.get(), data.size() * sizeof(T)});
        }
    }

    void write_ptr(nptr<const void> data, size_t size)
    {
        if (size != 0) {
            FO_VERIFY_AND_THROW(data, "Source pointer is null");
            auto source = ptr<const void> {data};
            auto source_bytes = source.reinterpret_as<uint8_t>();
            write_bytes({source_bytes.get(), size});
        }
    }

    void write_byte_vector(const vector<uint8_t>& data)
    {
        if (!data.empty()) {
            write_bytes({data.data(), data.size()});
        }
    }

    // Writes the elements of a vector without a length prefix; the reader must already know the count
    template<typename T>
    void write_object_vector(const vector<T>& values)
    {
        if (!values.empty()) {
            write_object_array(const_span<T> {values.data(), values.size()});
        }
    }

    // Writes a self-describing vector of trivially-copyable objects (uint32 count + elements); read back with data_reader::ReadObjectVector
    template<typename T>
    void write_sized_object_vector(const vector<T>& values)
    {
        write<uint32_t>(numeric_cast<uint32_t>(values.size()));
        write_object_vector(values);
    }

    // Writes a self-describing vector of strings (uint32 count + each element via write_string); read back with data_reader::read_string_vector
    void write_string_vector(const vector<string>& values)
    {
        write<uint32_t>(numeric_cast<uint32_t>(values.size()));

        for (const string& value : values) {
            write_string(value);
        }
    }

private:
    auto append_bytes(size_t size) -> span<uint8_t>
    {
        FO_VERIFY_AND_THROW(size != 0, "Append size is zero");

        size_t offset = _data_buf->size();
        grow_buf(size);

        ptr<uint8_t> data = _data_buf->data();
        ptr<uint8_t> bytes = data.offset(offset);
        return {bytes.get(), size};
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    static auto source_objects_ptr(const_span<T> data) -> ptr<const T>
    {
        static_assert(std::is_trivially_copyable_v<T>);
        FO_VERIFY_AND_THROW(!data.empty(), "Object span is empty");

        return data.data();
    }

    void grow_buf(size_t size)
    {
        while (size > _data_buf->capacity() - _data_buf->size()) {
            _data_buf->reserve(_data_buf->capacity() * 2);
        }

        _data_buf->resize(_data_buf->size() + size);
    }

    ptr<vector<uint8_t>> _data_buf;
};

FO_END_NAMESPACE
