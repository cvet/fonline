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
//

#pragma once

#include "BasicCore.h"
#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

// Data serialization helpers
FO_DECLARE_EXCEPTION(DataReadingException);

class DataReader
{
public:
    explicit DataReader(const_span<uint8_t> buf) :
        _dataBuf {buf}
    {
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    auto Read() -> T
    {
        static_assert(std::is_trivially_copyable_v<T>);

        T data;
        const_span<uint8_t> bytes = TakeBytes(sizeof(data));
        auto source = ReadBytesPtr(bytes);
        ptr<T> target = &data;
        MemCopy(target.get(), source.get(), sizeof(data));
        return data;
    }

    auto ReadBytes(size_t size) -> const_span<uint8_t> { return TakeBytes(size); }

    void ReadBytes(span<uint8_t> out)
    {
        const_span<uint8_t> bytes = TakeBytes(out.size());
        CopyBytesTo(out, bytes);
    }

    void ReadStringBytes(string& out)
    {
        if (!out.empty()) {
            ReadBytes({ptr<char>(out.data()).reinterpret_as<uint8_t>().get(), out.size()});
        }
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    void ReadObjectArray(span<T> out)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (!out.empty()) {
            auto target = MutableObjectsPtr(out);
            ptr<uint8_t> bytes = target.reinterpret_as<uint8_t>();
            ReadBytes({bytes.get(), out.size() * sizeof(T)});
        }
    }

    template<typename T>
    auto ReadPtr(size_t size) -> nptr<const T>
    {
        const_span<uint8_t> bytes = TakeBytes(size);

        if (bytes.empty()) {
            return nullptr;
        }

        auto source = ReadBytesPtr(bytes);
        return source.reinterpret_as<const T>();
    }

    void ReadPtr(nptr<void> nullable_out, size_t size)
    {
        const_span<uint8_t> bytes = TakeBytes(size);

        if (!bytes.empty()) {
            FO_VERIFY_AND_THROW(nullable_out, "Output pointer is null");
            auto target = nullable_out.as_ptr();
            ptr<uint8_t> target_bytes = cast_from_void<uint8_t*>(target.get());
            CopyBytesTo({target_bytes.get(), bytes.size()}, bytes);
        }
    }

    void VerifyEnd() const
    {
        if (_readPos != _dataBuf.size()) {
            throw DataReadingException("Not all data read");
        }
    }

private:
    auto TakeBytes(size_t size) -> const_span<uint8_t>
    {
        if (_readPos > _dataBuf.size() || size > _dataBuf.size() - _readPos) {
            throw DataReadingException("Unexpected end of buffer");
        }

        const_span<uint8_t> bytes = _dataBuf.subspan(_readPos, size);
        _readPos += size;
        return bytes;
    }

    static auto ReadBytesPtr(const_span<uint8_t> bytes) -> ptr<const uint8_t>
    {
        FO_VERIFY_AND_THROW(!bytes.empty(), "Byte span is empty");

        ptr<const uint8_t> bytes_ptr = bytes.data();
        return bytes_ptr;
    }

    static void CopyBytesTo(span<uint8_t> out, const_span<uint8_t> bytes)
    {
        FO_VERIFY_AND_THROW(out.size() == bytes.size(), "Output and source sizes differ");

        if (!bytes.empty()) {
            auto target = WriteBytesPtr(out);
            auto source = ReadBytesPtr(bytes);
            MemCopy(target.get(), source.get(), bytes.size());
        }
    }

    static auto WriteBytesPtr(span<uint8_t> bytes) -> ptr<uint8_t>
    {
        FO_VERIFY_AND_THROW(!bytes.empty(), "Byte span is empty");

        ptr<uint8_t> bytes_ptr = bytes.data();
        return bytes_ptr;
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    static auto MutableObjectsPtr(span<T> data) -> ptr<T>
    {
        static_assert(std::is_trivially_copyable_v<T>);
        FO_VERIFY_AND_THROW(!data.empty(), "Object span is empty");

        ptr<T> data_ptr = data.data();
        return data_ptr;
    }

    const_span<uint8_t> _dataBuf;
    size_t _readPos {};
};

class MutableDataReader
{
public:
    explicit MutableDataReader(span<uint8_t> buf) :
        _dataBuf {buf}
    {
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    auto Read() -> T
    {
        static_assert(std::is_trivially_copyable_v<T>);

        T data;
        span<uint8_t> bytes = TakeBytes(sizeof(data));
        auto source = ReadBytesPtr(bytes);
        ptr<T> target = &data;
        MemCopy(target.get(), source.get(), sizeof(data));
        return data;
    }

    auto ReadBytes(size_t size) -> span<uint8_t> { return TakeBytes(size); }

    void ReadBytes(span<uint8_t> out)
    {
        span<uint8_t> bytes = TakeBytes(out.size());
        CopyBytesTo(out, bytes);
    }

    void ReadStringBytes(string& out)
    {
        if (!out.empty()) {
            ReadBytes({ptr<char>(out.data()).reinterpret_as<uint8_t>().get(), out.size()});
        }
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    void ReadObjectArray(span<T> out)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (!out.empty()) {
            auto target = MutableObjectsPtr(out);
            ptr<uint8_t> bytes = target.reinterpret_as<uint8_t>();
            ReadBytes({bytes.get(), out.size() * sizeof(T)});
        }
    }

    template<typename T>
    auto ReadPtr(size_t size) -> nptr<T>
    {
        span<uint8_t> bytes = TakeBytes(size);

        if (bytes.empty()) {
            return nullptr;
        }

        auto source = ReadBytesPtr(bytes);
        return source.reinterpret_as<T>();
    }

    void ReadPtr(nptr<void> nullable_out, size_t size)
    {
        span<uint8_t> bytes = TakeBytes(size);

        if (!bytes.empty()) {
            FO_VERIFY_AND_THROW(nullable_out, "Output pointer is null");
            auto target = nullable_out.as_ptr();
            ptr<uint8_t> target_bytes = cast_from_void<uint8_t*>(target.get());
            CopyBytesTo({target_bytes.get(), bytes.size()}, bytes);
        }
    }

    void VerifyEnd() const
    {
        if (_readPos != _dataBuf.size()) {
            throw DataReadingException("Not all data read");
        }
    }

private:
    auto TakeBytes(size_t size) -> span<uint8_t>
    {
        if (_readPos > _dataBuf.size() || size > _dataBuf.size() - _readPos) {
            throw DataReadingException("Unexpected end of buffer");
        }

        span<uint8_t> bytes = _dataBuf.subspan(_readPos, size);
        _readPos += size;
        return bytes;
    }

    static auto ReadBytesPtr(span<uint8_t> bytes) -> ptr<uint8_t>
    {
        FO_VERIFY_AND_THROW(!bytes.empty(), "Byte span is empty");

        ptr<uint8_t> bytes_ptr = bytes.data();
        return bytes_ptr;
    }

    static void CopyBytesTo(span<uint8_t> out, span<uint8_t> bytes)
    {
        FO_VERIFY_AND_THROW(out.size() == bytes.size(), "Output and source sizes differ");

        if (!bytes.empty()) {
            auto target = ReadBytesPtr(out);
            auto source = ReadBytesPtr(bytes);
            MemCopy(target.get(), source.get(), bytes.size());
        }
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    static auto MutableObjectsPtr(span<T> data) -> ptr<T>
    {
        static_assert(std::is_trivially_copyable_v<T>);
        FO_VERIFY_AND_THROW(!data.empty(), "Object span is empty");

        ptr<T> data_ptr = data.data();
        return data_ptr;
    }

    span<uint8_t> _dataBuf;
    size_t _readPos {};
};

class DataWriter
{
public:
    static constexpr size_t BUF_RESERVE_SIZE = 1024;

    explicit DataWriter(vector<uint8_t>& buf) :
        _dataBuf {&buf}
    {
        _dataBuf->reserve(BUF_RESERVE_SIZE);
    }
    DataWriter(const DataWriter&) = delete;
    DataWriter(DataWriter&&) noexcept = delete;
    auto operator=(const DataWriter&) = delete;
    auto operator=(DataWriter&&) noexcept = delete;
    ~DataWriter() = default;

    template<typename T, typename U>
        requires(std::is_standard_layout_v<T> && std::same_as<T, U>)
    void Write(U data)
    {
        span<uint8_t> bytes = AppendBytes(sizeof(T));
        auto target = WriteBytesPtr(bytes);
        ptr<const U> source = &data;
        MemCopy(target.get(), source.get(), bytes.size());
    }

    void WriteBytes(const_span<uint8_t> data)
    {
        if (!data.empty()) {
            auto source = SourceBytesPtr(data);
            span<uint8_t> bytes = AppendBytes(data.size());
            auto target = WriteBytesPtr(bytes);
            MemCopy(target.get(), source.get(), bytes.size());
        }
    }

    void WriteStringBytes(string_view data)
    {
        if (!data.empty()) {
            WriteBytes({ptr<const char>(data.data()).reinterpret_as<uint8_t>().get(), data.size()});
        }
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    void WriteObjectArray(const_span<T> data)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (!data.empty()) {
            auto source = SourceObjectsPtr(data);
            ptr<const uint8_t> bytes = source.reinterpret_as<const uint8_t>();
            WriteBytes({bytes.get(), data.size() * sizeof(T)});
        }
    }

    void WritePtr(nptr<const void> nullable_data, size_t size)
    {
        if (size != 0) {
            FO_VERIFY_AND_THROW(nullable_data, "Source pointer is null");
            auto source = nullable_data.as_ptr();
            ptr<const uint8_t> source_bytes = cast_from_void<const uint8_t*>(source.get());
            WriteBytes({source_bytes.get(), size});
        }
    }

private:
    auto AppendBytes(size_t size) -> span<uint8_t>
    {
        FO_VERIFY_AND_THROW(size != 0, "Append size is zero");

        const size_t offset = _dataBuf->size();
        GrowBuf(size);

        ptr<uint8_t> data = _dataBuf->data();
        ptr<uint8_t> bytes = data.get() + offset;
        return {bytes.get(), size};
    }

    static auto WriteBytesPtr(span<uint8_t> bytes) -> ptr<uint8_t>
    {
        FO_VERIFY_AND_THROW(!bytes.empty(), "Byte span is empty");

        ptr<uint8_t> bytes_ptr = bytes.data();
        return bytes_ptr;
    }

    static auto SourceBytesPtr(const_span<uint8_t> bytes) -> ptr<const uint8_t>
    {
        FO_VERIFY_AND_THROW(!bytes.empty(), "Byte span is empty");

        ptr<const uint8_t> bytes_ptr = bytes.data();
        return bytes_ptr;
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    static auto SourceObjectsPtr(const_span<T> data) -> ptr<const T>
    {
        static_assert(std::is_trivially_copyable_v<T>);
        FO_VERIFY_AND_THROW(!data.empty(), "Object span is empty");

        ptr<const T> data_ptr = data.data();
        return data_ptr;
    }

    void GrowBuf(size_t size)
    {
        while (size > _dataBuf->capacity() - _dataBuf->size()) {
            _dataBuf->reserve(_dataBuf->capacity() * 2);
        }

        _dataBuf->resize(_dataBuf->size() + size);
    }

    ptr<vector<uint8_t>> _dataBuf;
};

FO_END_NAMESPACE
