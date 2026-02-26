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
        vector<uint8> buf;
        DataWriter writer {buf};

        writer.Write<uint32>(0xAABBCCDDu);
        writer.Write<int16>(static_cast<int16>(-1234));

        const array<uint8, 3> raw = {1, 2, 3};
        writer.WritePtr(raw.data(), raw.size());

        DataReader reader {const_span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint32>() == 0xAABBCCDDu);
        CHECK(reader.Read<int16>() == static_cast<int16>(-1234));

        const auto* raw_read = reader.ReadPtr<uint8>(raw.size());
        CHECK(raw_read != nullptr);
        CHECK(raw_read[0] == 1);
        CHECK(raw_read[1] == 2);
        CHECK(raw_read[2] == 3);

        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("DataReaderBounds")
    {
        vector<uint8> buf;
        DataWriter writer {buf};
        writer.Write<uint8>(static_cast<uint8>(10));

        DataReader reader {const_span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8>() == 10);
        CHECK_THROWS_AS(reader.Read<uint8>(), DataReadingException);
    }

    SECTION("VerifyEnd")
    {
        vector<uint8> buf;
        DataWriter writer {buf};
        writer.Write<uint16>(static_cast<uint16>(55));
        writer.Write<uint16>(static_cast<uint16>(66));

        DataReader reader {const_span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint16>() == 55);
        CHECK_THROWS_AS(reader.VerifyEnd(), DataReadingException);
    }

    SECTION("MutableDataReader")
    {
        vector<uint8> buf;
        DataWriter writer {buf};
        writer.Write<uint32>(static_cast<uint32>(123));
        writer.Write<uint8>(static_cast<uint8>(9));

        MutableDataReader reader {span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint32>() == 123);

        auto* value = reader.ReadPtr<uint8>(1);
        REQUIRE(value != nullptr);
        CHECK(*value == 9);
        *value = 11;

        CHECK(buf.back() == 11);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ZeroSizePointers")
    {
        vector<uint8> buf;
        DataWriter writer {buf};

        writer.Write<uint8>(static_cast<uint8>(77));
        writer.WritePtr<uint8>(nullptr, 0);

        DataReader reader {const_span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8>() == 77);
        CHECK(reader.ReadPtr<uint8>(0) == nullptr);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ReadPtrToBuffer")
    {
        vector<uint8> buf;
        DataWriter writer {buf};

        const array<uint8, 4> source = {9, 8, 7, 6};
        writer.WritePtr(source.data(), source.size());

        DataReader reader {const_span<uint8> {buf.data(), buf.size()}};
        array<uint8, 4> target = {0, 0, 0, 0};
        reader.ReadPtr(target.data(), target.size());

        CHECK(target == source);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("MutableDataReaderBounds")
    {
        vector<uint8> buf;
        DataWriter writer {buf};
        writer.Write<uint8>(static_cast<uint8>(5));

        MutableDataReader reader {span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint8>() == 5);
        CHECK_THROWS_AS(reader.Read<uint8>(), DataReadingException);
    }

    SECTION("MutableZeroSizePointers")
    {
        vector<uint8> buf;
        DataWriter writer {buf};
        writer.Write<uint16>(static_cast<uint16>(0x1234));

        MutableDataReader reader {span<uint8> {buf.data(), buf.size()}};
        CHECK(reader.Read<uint16>() == static_cast<uint16>(0x1234));
        CHECK(reader.ReadPtr<uint8>(0) == nullptr);

        array<uint8, 2> temp = {1, 2};
        reader.ReadPtr(temp.data(), 0);
        CHECK(temp == array<uint8, 2> {1, 2});
        CHECK_NOTHROW(reader.VerifyEnd());
    }
}

FO_END_NAMESPACE
