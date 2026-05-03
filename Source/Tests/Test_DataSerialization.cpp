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

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

TEST_CASE("DataSerialization")
{
    SECTION("ReadWriteRoundtrip")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        writer.Write<uint32_t>(0xAABBCCDDu);
        writer.Write<int16_t>(static_cast<int16_t>(-1234));

        const array<uint8_t, 3> raw = {1, 2, 3};
        writer.WritePtr(raw.data(), raw.size());

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint32_t>() == 0xAABBCCDDu);
        CHECK(reader.Read<int16_t>() == static_cast<int16_t>(-1234));

        const auto* raw_read = reader.ReadPtr<uint8_t>(raw.size());
        CHECK(raw_read != nullptr);
        CHECK(raw_read[0] == 1);
        CHECK(raw_read[1] == 2);
        CHECK(raw_read[2] == 3);

        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("DataReaderBounds")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint8_t>(static_cast<uint8_t>(10));

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8_t>() == 10);
        CHECK_THROWS_AS(reader.Read<uint8_t>(), DataReadingException);
    }

    SECTION("VerifyEnd")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(55));
        writer.Write<uint16_t>(static_cast<uint16_t>(66));

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint16_t>() == 55);
        CHECK_THROWS_AS(reader.VerifyEnd(), DataReadingException);
    }

    SECTION("EmptyReadersAcceptVerifyEndAndZeroSizePointers")
    {
        vector<uint8_t> buf;

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.ReadPtr<uint8_t>(0) == nullptr);
        CHECK_NOTHROW(reader.VerifyEnd());

        MutableDataReader mutable_reader {span<uint8_t> {buf.data(), buf.size()}};
        CHECK(mutable_reader.ReadPtr<uint8_t>(0) == nullptr);
        CHECK_NOTHROW(mutable_reader.VerifyEnd());
    }

    SECTION("MutableDataReader")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint32_t>(static_cast<uint32_t>(123));
        writer.Write<uint8_t>(static_cast<uint8_t>(9));

        MutableDataReader reader {span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint32_t>() == 123);

        auto* value = reader.ReadPtr<uint8_t>(1);
        REQUIRE(value != nullptr);
        CHECK(*value == 9);
        *value = 11;

        CHECK(buf.back() == 11);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ZeroSizePointers")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        writer.Write<uint8_t>(static_cast<uint8_t>(77));
        writer.WritePtr<uint8_t>(nullptr, 0);

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8_t>() == 77);
        CHECK(reader.ReadPtr<uint8_t>(0) == nullptr);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ReadPtrToBuffer")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        const array<uint8_t, 4> source = {9, 8, 7, 6};
        writer.WritePtr(source.data(), source.size());

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        array<uint8_t, 4> target = {0, 0, 0, 0};
        reader.ReadPtr(target.data(), target.size());

        CHECK(target == source);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ZeroSizeBufferedReadDoesNotModifyTargetOrAdvance")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(0xABCD));

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        array<uint8_t, 3> target = {7, 8, 9};

        reader.ReadPtr(target.data(), 0);
        CHECK(target == array<uint8_t, 3> {7, 8, 9});
        CHECK(reader.Read<uint16_t>() == static_cast<uint16_t>(0xABCD));
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ReadPtrToBufferThrowsWithoutModifyingTargetWhenOutOfBounds")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(0xCAFE));

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        array<uint8_t, 4> target = {1, 2, 3, 4};

        CHECK_THROWS_AS(reader.ReadPtr(target.data(), target.size()), DataReadingException);
        CHECK(target == array<uint8_t, 4> {1, 2, 3, 4});
    }

    SECTION("MutableDataReaderBounds")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint8_t>(static_cast<uint8_t>(5));

        MutableDataReader reader {span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8_t>() == 5);
        CHECK_THROWS_AS(reader.Read<uint8_t>(), DataReadingException);
    }

    SECTION("MutableZeroSizePointers")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(0x1234));

        MutableDataReader reader {span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint16_t>() == static_cast<uint16_t>(0x1234));
        CHECK(reader.ReadPtr<uint8_t>(0) == nullptr);

        array<uint8_t, 2> temp = {1, 2};
        reader.ReadPtr(temp.data(), 0);
        CHECK(temp == array<uint8_t, 2> {1, 2});
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("MutableReadPtrToBufferThrowsWithoutModifyingTargetWhenOutOfBounds")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint8_t>(static_cast<uint8_t>(42));

        MutableDataReader reader {span<uint8_t> {buf.data(), buf.size()}};
        array<uint8_t, 3> target = {9, 8, 7};

        CHECK_THROWS_AS(reader.ReadPtr(target.data(), target.size()), DataReadingException);
        CHECK(target == array<uint8_t, 3> {9, 8, 7});
    }

    SECTION("ZeroSizeWritePtrDoesNotGrowBuffer")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint32_t>(0x11223344u);

        const size_t initial_size = buf.size();
        const array<uint8_t, 3> source = {5, 6, 7};

        writer.WritePtr(source.data(), 0);

        CHECK(buf.size() == initial_size);

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint32_t>() == 0x11223344u);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("LargeWritePtrGrowsBufferAndPreservesData")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        vector<uint8_t> source(DataWriter::BUF_RESERVE_SIZE + 17, static_cast<uint8_t>(0x5A));
        source.front() = 1;
        source.back() = 2;

        writer.Write<uint8_t>(static_cast<uint8_t>(0x11));
        writer.WritePtr(source.data(), source.size());

        CHECK(buf.size() == source.size() + sizeof(uint8_t));

        DataReader reader {const_span<uint8_t> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8_t>() == static_cast<uint8_t>(0x11));

        const auto* raw = reader.ReadPtr<uint8_t>(source.size());
        REQUIRE(raw != nullptr);
        CHECK(raw[0] == 1);
        CHECK(raw[source.size() - 1] == 2);
        CHECK(raw[source.size() / 2] == static_cast<uint8_t>(0x5A));
        CHECK_NOTHROW(reader.VerifyEnd());
    }
}

FO_END_NAMESPACE
