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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ExceptionHadling.h"

FO_BEGIN_NAMESPACE();

// Data serialization helpers
FO_DECLARE_EXCEPTION(DataReadingException);

class DataReader
{
public:
    explicit DataReader(span<const uint8> buf) :
        _dataBuf {buf}
    {
    }

    template<typename T>
        requires(std::is_standard_layout_v<T>)
    auto Read() -> T
    {
        if (_readPos + sizeof(T) > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        T data = *reinterpret_cast<const T*>(_dataBuf.data() + _readPos);
        _readPos += sizeof(T);
        return data;
    }

    template<typename T>
    auto ReadPtr(size_t size) -> const T*
    {
        if (_readPos + size > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        if (size != 0) {
            const T* ptr = reinterpret_cast<const T*>(_dataBuf.data() + _readPos);
            _readPos += size;
            return ptr;
        }

        return nullptr;
    }

    template<typename T>
    void ReadPtr(T* ptr, size_t size)
    {
        if (_readPos + size > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        MemCopy(ptr, _dataBuf.data() + _readPos, size);
        _readPos += size;
    }

    void VerifyEnd() const
    {
        if (_readPos != _dataBuf.size()) {
            throw DataReadingException("Not all data read");
        }
    }

private:
    span<const uint8> _dataBuf;
    size_t _readPos {};
};

class DataWriter
{
public:
    static constexpr size_t BUF_RESERVE_SIZE = 1024;

    explicit DataWriter(vector<uint8>& buf) noexcept :
        _dataBuf {&buf}
    {
        _dataBuf->reserve(BUF_RESERVE_SIZE);
    }

    template<typename T, typename U>
        requires(std::is_standard_layout_v<T> && std::same_as<T, U>)
    void Write(U data) noexcept
    {
        GrowBuf(sizeof(T));
        *reinterpret_cast<T*>(_dataBuf->data() + _dataBuf->size() - sizeof(T)) = data;
    }

    template<typename T>
    void WritePtr(const T* data, size_t size) noexcept
    {
        if (size != 0) {
            GrowBuf(size);
            MemCopy(_dataBuf->data() + _dataBuf->size() - size, data, size);
        }
    }

private:
    void GrowBuf(size_t size) noexcept
    {
        while (size > _dataBuf->capacity() - _dataBuf->size()) {
            _dataBuf->reserve(_dataBuf->capacity() * 2);
        }

        _dataBuf->resize(_dataBuf->size() + size);
    }

    raw_ptr<vector<uint8>> _dataBuf;
};

FO_END_NAMESPACE();
