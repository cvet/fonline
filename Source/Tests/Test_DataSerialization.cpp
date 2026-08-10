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

template<typename T>
consteval auto IsMutableZeroCopyReadPtrAvailable() -> bool
{
    return requires(MutableDataReader& reader) { reader.template ReadPtr<T>(0); };
}

static_assert(IsZeroCopyReadPtrAvailable<byte>());
static_assert(IsZeroCopyReadPtrAvailable<char>());
static_assert(IsZeroCopyReadPtrAvailable<void>());
static_assert(!IsZeroCopyReadPtrAvailable<uint8_t>());
static_assert(!IsZeroCopyReadPtrAvailable<uint32_t>());
static_assert(!IsZeroCopyReadPtrAvailable<float32_t>());
static_assert(IsMutableZeroCopyReadPtrAvailable<byte>());
static_assert(IsMutableZeroCopyReadPtrAvailable<char>());
static_assert(IsMutableZeroCopyReadPtrAvailable<void>());
static_assert(!IsMutableZeroCopyReadPtrAvailable<uint8_t>());
static_assert(!IsMutableZeroCopyReadPtrAvailable<uint32_t>());
static_assert(std::constructible_from<DataReader, const_span<byte>>);
static_assert(!std::constructible_from<DataReader, const_span<uint8_t>>);
static_assert(std::constructible_from<DataWriter, vector<byte>&>);
static_assert(!std::constructible_from<DataWriter, vector<uint8_t>&>);

TEST_CASE("DataSerialization")
{
    SECTION("SpanHelpers")
    {
        array<byte, 16> buf {};
        auto writable = make_byte_span(buf);

        size_t write_pos = 1;
        span_write_object<uint16_t>(writable, write_pos, static_cast<uint16_t>(0xABCD));

        array<byte, 3> raw = {byte {1}, byte {2}, byte {3}};
        span_write_bytes(writable, write_pos, {raw.data(), raw.size()});

        size_t zero_size_pos = write_pos;
        span_write_bytes(writable, write_pos, const_span<byte> {});
        CHECK(write_pos == zero_size_pos);

        size_t read_pos = 1;
        CHECK(span_read_object<uint16_t>(const_span<byte> {buf}, read_pos) == static_cast<uint16_t>(0xABCD));
        CHECK(read_pos == 1 + sizeof(uint16_t));

        const_span<byte> raw_read = span_read_bytes(const_span<byte> {buf}, read_pos, raw.size());
        CHECK(raw_read[0] == raw[0]);
        CHECK(raw_read[1] == raw[1]);
        CHECK(raw_read[2] == raw[2]);

        size_t after_raw_pos = read_pos;
        CHECK(span_read_bytes(const_span<byte> {buf}, read_pos, 0).empty());
        CHECK(read_pos == after_raw_pos);

        size_t mutable_read_pos = 1 + sizeof(uint16_t);
        span<byte> mutable_raw = span_read_bytes(writable, mutable_read_pos, raw.size());
        mutable_raw[0] = byte {9};
        CHECK(buf[1 + sizeof(uint16_t)] == byte {9});

        size_t overflow_read_pos = buf.size();
        CHECK_THROWS_AS(span_read_bytes(const_span<byte> {buf}, overflow_read_pos, 1), DataReadingException);

        size_t overflow_write_pos = buf.size();
        CHECK_THROWS_AS(span_write_bytes(writable, overflow_write_pos, static_cast<size_t>(1)), VerificationException);

        array<byte, 32> aligned_buf {};
        auto aligned_writable = make_byte_span(aligned_buf);

        CHECK(alignment_for_size(0) == 1);
        CHECK(alignment_for_size(12) == 4);
        CHECK(alignment_for_size(MAX_SERIALIZED_ALIGNMENT * 2) == MAX_SERIALIZED_ALIGNMENT);

        size_t aligned_write_pos = 3;
        span_write_aligned_object<uint32_t>(aligned_writable, aligned_write_pos, 0x11223344u);
        CHECK(aligned_write_pos == 8);

        array<byte, 3> aligned_raw = {byte {4}, byte {5}, byte {6}};
        aligned_write_pos = 5;
        span_write_aligned_bytes(aligned_writable, aligned_write_pos, {aligned_raw.data(), aligned_raw.size()}, 8);
        CHECK(aligned_write_pos == 11);

        size_t zero_aligned_write_pos = aligned_write_pos;
        span_write_aligned_bytes(aligned_writable, aligned_write_pos, const_span<byte> {}, 8);
        CHECK(aligned_write_pos == zero_aligned_write_pos);

        size_t aligned_read_pos = 3;
        CHECK(span_read_aligned_object<uint32_t>(const_span<byte> {aligned_buf}, aligned_read_pos) == 0x11223344u);
        CHECK(aligned_read_pos == 8);

        aligned_read_pos = 5;
        const_span<byte> aligned_raw_read = span_read_aligned_bytes(const_span<byte> {aligned_buf}, aligned_read_pos, aligned_raw.size(), 8);
        CHECK(aligned_read_pos == 11);
        CHECK(aligned_raw_read[0] == aligned_raw[0]);
        CHECK(aligned_raw_read[1] == aligned_raw[1]);
        CHECK(aligned_raw_read[2] == aligned_raw[2]);

        size_t zero_aligned_read_pos = aligned_read_pos;
        CHECK(span_read_aligned_bytes(const_span<byte> {aligned_buf}, aligned_read_pos, 0, 8).empty());
        CHECK(aligned_read_pos == zero_aligned_read_pos);
    }

    SECTION("ReadWriteRoundtrip")
    {
        vector<byte> buf;
        DataWriter writer {buf};

        writer.Write<uint32_t>(0xAABBCCDDu);
        writer.Write<int16_t>(static_cast<int16_t>(-1234));

        array<byte, 3> raw = {byte {1}, byte {2}, byte {3}};
        writer.WriteBytes({raw.data(), raw.size()});

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint32_t>() == 0xAABBCCDDu);
        CHECK(reader.Read<int16_t>() == static_cast<int16_t>(-1234));

        auto raw_read = reader.ReadPtr<byte>(raw.size());
        CHECK(static_cast<bool>(raw_read));
        CHECK(raw_read[0] == byte {1});
        CHECK(raw_read[1] == byte {2});
        CHECK(raw_read[2] == byte {3});

        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("NumericUint8AndRawBytesHaveStableLayout")
    {
        uint8_t numeric_value = uint8_t {0x5A};
        array<byte, 3> raw = {byte {0x00}, byte {0x80}, byte {0xFF}};
        array<byte, 4> golden = {byte {0x5A}, byte {0x00}, byte {0x80}, byte {0xFF}};

        vector<byte> written;
        DataWriter writer {written};
        writer.Write<uint8_t>(numeric_value);
        writer.WriteBytes(make_byte_span(raw));

        REQUIRE(written.size() == golden.size());
        for (size_t index = 0; index < golden.size(); index++) {
            CHECK(written[index] == golden[index]);
        }

        DataReader reader {make_byte_span(golden)};
        CHECK(reader.Read<uint8_t>() == numeric_value);

        const_span<byte> read_raw = reader.ReadBytes(raw.size());
        REQUIRE(read_raw.size() == raw.size());
        for (size_t index = 0; index < raw.size(); index++) {
            CHECK(read_raw[index] == raw[index]);
        }
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("DataReaderBounds")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint8_t>(static_cast<uint8_t>(10));

        DataReader reader {span {buf}};
        CHECK(reader.GetUnreadSize() == sizeof(uint8_t));
        CHECK(reader.Read<uint8_t>() == 10);
        CHECK(reader.GetUnreadSize() == 0);
        CHECK_THROWS_AS(reader.Read<uint8_t>(), DataReadingException);
    }

    SECTION("Utf8StringBytesRoundtrip")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.WriteStringBytes(u8"Привет 🌍");

        DataReader reader {buf};
        CHECK(reader.ReadUtf8StringView(buf.size()) == u8"Привет 🌍");
        CHECK_NOTHROW(reader.VerifyEnd());

        vector<byte> invalid = buf;
        invalid.back() = byte {0xFF};
        DataReader invalid_reader {invalid};
        CHECK_THROWS_AS(invalid_reader.ReadUtf8StringView(invalid.size()), TextValidationException);
    }

    SECTION("ReaderRejectsLengthAndCountBombsBeforeAllocation")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());

        DataReader string_view_reader {span {buf}};
        uint32_t hostile_string_size = string_view_reader.Read<uint32_t>();
        CHECK_THROWS_AS(string_view_reader.ReadStringView(hostile_string_size), DataReadingException);

        DataReader payload_count_reader {span {buf}};
        uint32_t hostile_count = payload_count_reader.Read<uint32_t>();
        CHECK_THROWS_AS(payload_count_reader.VerifyPayloadCount(numeric_cast<size_t>(hostile_count), sizeof(uint32_t)), DataReadingException);

        DataReader string_reader {span {buf}};
        CHECK_THROWS_AS(string_reader.ReadString(), DataReadingException);

        DataReader string_vector_reader {span {buf}};
        CHECK_THROWS_AS(string_vector_reader.ReadStringVector(), DataReadingException);

        DataReader object_vector_reader {span {buf}};
        CHECK_THROWS_AS(object_vector_reader.ReadSizedObjectVector<uint32_t>(), DataReadingException);

        DataReader empty_reader {const_span<byte> {}};
        CHECK_THROWS_AS(empty_reader.VerifyPayloadCount(0, 0), VerificationException);
    }

    SECTION("VerifyEnd")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(55));
        writer.Write<uint16_t>(static_cast<uint16_t>(66));

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint16_t>() == 55);
        CHECK_THROWS_AS(reader.VerifyEnd(), DataReadingException);
    }

    SECTION("EmptyReadersAcceptVerifyEndAndZeroSizePointers")
    {
        vector<byte> buf;

        DataReader reader {span {buf}};
        CHECK_FALSE(static_cast<bool>(reader.ReadPtr<byte>(0)));
        CHECK_NOTHROW(reader.VerifyEnd());

        MutableDataReader mutable_reader {span {buf}};
        CHECK_FALSE(static_cast<bool>(mutable_reader.ReadPtr<byte>(0)));
        CHECK_NOTHROW(mutable_reader.VerifyEnd());
    }

    SECTION("MutableDataReader")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint32_t>(static_cast<uint32_t>(123));
        writer.Write<uint8_t>(static_cast<uint8_t>(9));

        MutableDataReader reader {span {buf}};
        CHECK(reader.Read<uint32_t>() == 123);

        auto value = reader.ReadPtr<byte>(1);
        REQUIRE(static_cast<bool>(value));
        CHECK(*value == byte {9});
        *value = byte {11};

        CHECK(buf.back() == byte {11});
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ZeroSizePointers")
    {
        vector<byte> buf;
        DataWriter writer {buf};

        writer.Write<uint8_t>(static_cast<uint8_t>(77));
        writer.WritePtr(nullptr, 0);

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint8_t>() == 77);
        CHECK_FALSE(static_cast<bool>(reader.ReadPtr<byte>(0)));
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ReadPtrToBuffer")
    {
        vector<byte> buf;
        DataWriter writer {buf};

        array<byte, 4> source = {byte {9}, byte {8}, byte {7}, byte {6}};
        writer.WriteBytes({source.data(), source.size()});

        DataReader reader {span {buf}};
        array<byte, 4> target = {byte {0}, byte {0}, byte {0}, byte {0}};
        reader.ReadPtr(target.data(), target.size());

        CHECK(target == source);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ZeroSizeBufferedReadDoesNotModifyTargetOrAdvance")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(0xABCD));

        DataReader reader {span {buf}};
        array<byte, 3> target = {byte {7}, byte {8}, byte {9}};

        reader.ReadPtr(target.data(), 0);
        CHECK(target == array<byte, 3> {byte {7}, byte {8}, byte {9}});
        CHECK(reader.Read<uint16_t>() == static_cast<uint16_t>(0xABCD));
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("ReadPtrToBufferThrowsWithoutModifyingTargetWhenOutOfBounds")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(0xCAFE));

        DataReader reader {span {buf}};
        array<byte, 4> target = {byte {1}, byte {2}, byte {3}, byte {4}};

        CHECK_THROWS_AS(reader.ReadPtr(target.data(), target.size()), DataReadingException);
        CHECK(target == array<byte, 4> {byte {1}, byte {2}, byte {3}, byte {4}});
    }

    SECTION("MutableDataReaderBounds")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint8_t>(static_cast<uint8_t>(5));

        MutableDataReader reader {span {buf}};
        CHECK(reader.Read<uint8_t>() == 5);
        CHECK_THROWS_AS(reader.Read<uint8_t>(), DataReadingException);
    }

    SECTION("MutableZeroSizePointers")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint16_t>(static_cast<uint16_t>(0x1234));

        MutableDataReader reader {span {buf}};
        CHECK(reader.Read<uint16_t>() == static_cast<uint16_t>(0x1234));
        CHECK_FALSE(static_cast<bool>(reader.ReadPtr<byte>(0)));

        array<byte, 2> temp = {byte {1}, byte {2}};
        reader.ReadPtr(temp.data(), 0);
        CHECK(temp == array<byte, 2> {byte {1}, byte {2}});
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("MutableReadPtrToBufferThrowsWithoutModifyingTargetWhenOutOfBounds")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint8_t>(static_cast<uint8_t>(42));

        MutableDataReader reader {span {buf}};
        array<byte, 3> target = {byte {9}, byte {8}, byte {7}};

        CHECK_THROWS_AS(reader.ReadPtr(target.data(), target.size()), DataReadingException);
        CHECK(target == array<byte, 3> {byte {9}, byte {8}, byte {7}});
    }

    SECTION("ZeroSizeWritePtrDoesNotGrowBuffer")
    {
        vector<byte> buf;
        DataWriter writer {buf};
        writer.Write<uint32_t>(0x11223344u);

        size_t initial_size = buf.size();
        array<byte, 3> source = {byte {5}, byte {6}, byte {7}};

        writer.WritePtr(source.data(), 0);

        CHECK(buf.size() == initial_size);

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint32_t>() == 0x11223344u);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("LargeWriteBytesGrowsBufferAndPreservesData")
    {
        vector<byte> buf;
        DataWriter writer {buf};

        vector<byte> source(DataWriter::BUF_RESERVE_SIZE + 17, byte {0x5A});
        source.front() = byte {1};
        source.back() = byte {2};

        writer.Write<uint8_t>(static_cast<uint8_t>(0x11));
        writer.WriteBytes({source.data(), source.size()});

        CHECK(buf.size() == source.size() + sizeof(uint8_t));

        DataReader reader {span {buf}};
        CHECK(reader.Read<uint8_t>() == static_cast<uint8_t>(0x11));

        auto raw = reader.ReadPtr<byte>(source.size());
        REQUIRE(static_cast<bool>(raw));
        CHECK(raw[0] == byte {1});
        CHECK(raw[source.size() - 1] == byte {2});
        CHECK(raw[source.size() / 2] == byte {0x5A});
        CHECK_NOTHROW(reader.VerifyEnd());
    }
}

FO_END_NAMESPACE
