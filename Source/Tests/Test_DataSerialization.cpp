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

template<typename T>
consteval auto IsZeroCopyReadPtrAvailable() -> bool
{
    return requires(DataReader& reader) { reader.template ReadPtr<T>(0); };
}

static_assert(IsZeroCopyReadPtrAvailable<uint8_t>());
static_assert(IsZeroCopyReadPtrAvailable<char>());
static_assert(IsZeroCopyReadPtrAvailable<void>());
static_assert(!IsZeroCopyReadPtrAvailable<uint32_t>());
static_assert(!IsZeroCopyReadPtrAvailable<float32_t>());

TEST_CASE("DataSerialization")
{
    SECTION("SpanHelpers")
    {
        array<uint8_t, 16> buf {};
        auto writable = make_span(buf);

        size_t write_pos = 1;
        span_write_object<uint16_t>(writable, write_pos, static_cast<uint16_t>(0xABCD));

        const array<uint8_t, 3> raw = {1, 2, 3};
        span_write_bytes(writable, write_pos, {raw.data(), raw.size()});

        const size_t zero_size_pos = write_pos;
        span_write_bytes(writable, write_pos, const_span<uint8_t> {});
        CHECK(write_pos == zero_size_pos);

        size_t read_pos = 1;
        CHECK(span_read_object<uint16_t>(const_span<uint8_t> {buf}, read_pos) == static_cast<uint16_t>(0xABCD));
        CHECK(read_pos == 1 + sizeof(uint16_t));

        const_span<uint8_t> raw_read = span_read_bytes(const_span<uint8_t> {buf}, read_pos, raw.size());
        CHECK(raw_read[0] == raw[0]);
        CHECK(raw_read[1] == raw[1]);
        CHECK(raw_read[2] == raw[2]);

        const size_t after_raw_pos = read_pos;
        CHECK(span_read_bytes(const_span<uint8_t> {buf}, read_pos, 0).empty());
        CHECK(read_pos == after_raw_pos);

        size_t mutable_read_pos = 1 + sizeof(uint16_t);
        span<uint8_t> mutable_raw = span_read_bytes(writable, mutable_read_pos, raw.size());
        mutable_raw[0] = 9;
        CHECK(buf[1 + sizeof(uint16_t)] == 9);

        size_t overflow_read_pos = buf.size();
        CHECK_THROWS_AS(span_read_bytes(const_span<uint8_t> {buf}, overflow_read_pos, 1), DataReadingException);

        size_t overflow_write_pos = buf.size();
        CHECK_THROWS_AS(span_write_bytes(writable, overflow_write_pos, static_cast<size_t>(1)), VerificationException);

        array<uint8_t, 32> aligned_buf {};
        auto aligned_writable = make_span(aligned_buf);

        CHECK(alignment_for_size(0) == 1);
        CHECK(alignment_for_size(12) == 4);
        CHECK(alignment_for_size(MAX_SERIALIZED_ALIGNMENT * 2) == MAX_SERIALIZED_ALIGNMENT);

        size_t aligned_write_pos = 3;
        span_write_aligned_object<uint32_t>(aligned_writable, aligned_write_pos, 0x11223344u);
        CHECK(aligned_write_pos == 8);

        const array<uint8_t, 3> aligned_raw = {4, 5, 6};
        aligned_write_pos = 5;
        span_write_aligned_bytes(aligned_writable, aligned_write_pos, {aligned_raw.data(), aligned_raw.size()}, 8);
        CHECK(aligned_write_pos == 11);

        const size_t zero_aligned_write_pos = aligned_write_pos;
        span_write_aligned_bytes(aligned_writable, aligned_write_pos, const_span<uint8_t> {}, 8);
        CHECK(aligned_write_pos == zero_aligned_write_pos);

        size_t aligned_read_pos = 3;
        CHECK(span_read_aligned_object<uint32_t>(const_span<uint8_t> {aligned_buf}, aligned_read_pos) == 0x11223344u);
        CHECK(aligned_read_pos == 8);

        aligned_read_pos = 5;
        const_span<uint8_t> aligned_raw_read = span_read_aligned_bytes(const_span<uint8_t> {aligned_buf}, aligned_read_pos, aligned_raw.size(), 8);
        CHECK(aligned_read_pos == 11);
        CHECK(aligned_raw_read[0] == aligned_raw[0]);
        CHECK(aligned_raw_read[1] == aligned_raw[1]);
        CHECK(aligned_raw_read[2] == aligned_raw[2]);

        const size_t zero_aligned_read_pos = aligned_read_pos;
        CHECK(span_read_aligned_bytes(const_span<uint8_t> {aligned_buf}, aligned_read_pos, 0, 8).empty());
        CHECK(aligned_read_pos == zero_aligned_read_pos);
    }

    SECTION("ReadWriteRoundtrip")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        writer.Write<uint32_t>(0xAABBCCDDu);
        writer.Write<int16_t>(static_cast<int16_t>(-1234));

        const array<uint8_t, 3> raw = {1, 2, 3};
        writer.WriteBytes({raw.data(), raw.size()});

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint32_t>() == 0xAABBCCDDu);
        CHECK(reader.Read<int16_t>() == static_cast<int16_t>(-1234));

        const auto raw_read = reader.ReadPtr<uint8_t>(raw.size());
        CHECK(static_cast<bool>(raw_read));
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

        DataReader reader {span {buf}};
        CHECK(reader.GetUnreadSize() == sizeof(uint8_t));
        CHECK(reader.Read<uint8_t>() == 10);
        CHECK(reader.GetUnreadSize() == 0);
        CHECK_THROWS_AS(reader.Read<uint8_t>(), DataReadingException);
    }

    SECTION("ReaderRejectsLengthAndCountBombsBeforeAllocation")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());

        DataReader string_view_reader {span {buf}};
        const uint32_t hostile_string_size = string_view_reader.Read<uint32_t>();
        CHECK_THROWS_AS(string_view_reader.ReadStringView(hostile_string_size), DataReadingException);

        DataReader payload_count_reader {span {buf}};
        const uint32_t hostile_count = payload_count_reader.Read<uint32_t>();
        CHECK_THROWS_AS(payload_count_reader.VerifyPayloadCount(numeric_cast<size_t>(hostile_count), sizeof(uint32_t)), DataReadingException);

        DataReader string_reader {span {buf}};
        CHECK_THROWS_AS(string_reader.ReadString(), DataReadingException);

        DataReader string_vector_reader {span {buf}};
        CHECK_THROWS_AS(string_vector_reader.ReadStringVector(), DataReadingException);

        DataReader object_vector_reader {span {buf}};
        CHECK_THROWS_AS(object_vector_reader.ReadSizedObjectVector<uint32_t>(), DataReadingException);

        DataReader empty_reader {const_span<uint8_t> {}};
        CHECK_NOTHROW(empty_reader.VerifyPayloadCount(0, 0));
        CHECK_THROWS_AS(empty_reader.VerifyPayloadCount(1, 0), DataReadingException);
    }

    SECTION("VerifyEnd")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(55));
        writer.Write<uint16_t>(static_cast<uint16_t>(66));

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint16_t>() == 55);
        CHECK_THROWS_AS(reader.VerifyEnd(), DataReadingException);
    }

    SECTION("EmptyReadersAcceptVerifyEndAndZeroSizePointers")
    {
        vector<uint8_t> buf;

        DataReader reader {span {buf}};
        CHECK_FALSE(static_cast<bool>(reader.ReadPtr<uint8_t>(0)));
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ZeroSizePointers")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        writer.Write<uint8_t>(static_cast<uint8_t>(77));
        writer.WritePtr(nullptr, 0);

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint8_t>() == 77);
        CHECK_FALSE(static_cast<bool>(reader.ReadPtr<uint8_t>(0)));
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ReadPtrToBuffer")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        const array<uint8_t, 4> source = {9, 8, 7, 6};
        writer.WriteBytes({source.data(), source.size()});

        DataReader reader {span {buf}};
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

        DataReader reader {span {buf}};
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

        DataReader reader {span {buf}};
        array<uint8_t, 4> target = {1, 2, 3, 4};

        CHECK_THROWS_AS(reader.ReadPtr(target.data(), target.size()), DataReadingException);
        CHECK(target == array<uint8_t, 4> {1, 2, 3, 4});
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

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint32_t>() == 0x11223344u);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("LargeWriteBytesGrowsBufferAndPreservesData")
    {
        vector<uint8_t> buf;
        DataWriter writer {buf};

        vector<uint8_t> source(DataWriter::BUF_RESERVE_SIZE + 17, static_cast<uint8_t>(0x5A));
        source.front() = 1;
        source.back() = 2;

        writer.Write<uint8_t>(static_cast<uint8_t>(0x11));
        writer.WriteBytes({source.data(), source.size()});

        CHECK(buf.size() == source.size() + sizeof(uint8_t));

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint8_t>() == static_cast<uint8_t>(0x11));

        const auto raw = reader.ReadPtr<uint8_t>(source.size());
        REQUIRE(static_cast<bool>(raw));
        CHECK(raw[0] == 1);
        CHECK(raw[source.size() - 1] == 2);
        CHECK(raw[source.size() / 2] == static_cast<uint8_t>(0x5A));
        CHECK_NOTHROW(reader.VerifyEnd());
    }
}

FO_END_NAMESPACE
