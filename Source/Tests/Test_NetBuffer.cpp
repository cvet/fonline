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

#include "catch_amalgamated.hpp"

#include "NetBuffer.h"

FO_BEGIN_NAMESPACE

TEST_CASE("NetBuffer")
{
    SECTION("PrimitiveAndStringRoundtrip")
    {
        NetOutBuffer out_buf {8, false};
        out_buf.Write<int32>(42);
        out_buf.Write<float32>(12.5f);
        out_buf.Write<string>(string {"hello"});

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        CHECK(in_buf.Read<int32>() == 42);
        CHECK(in_buf.Read<float32>() == 12.5f);
        CHECK(in_buf.Read<string>() == "hello");
        CHECK(in_buf.GetReadPos() == out_buf.GetDataSize());
    }

    SECTION("DiscardWriteBufDropsPrefix")
    {
        NetOutBuffer out_buf {8, false};
        out_buf.Write<uint32>(111);
        out_buf.Write<uint32>(222);
        out_buf.DiscardWriteBuf(sizeof(uint32));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        CHECK(in_buf.Read<uint32>() == 222);
        CHECK(out_buf.GetDataSize() == sizeof(uint32));
    }

    SECTION("DiscardWriteBufTooMuchResetsAndThrows")
    {
        NetOutBuffer out_buf {8, false};
        out_buf.Write<uint32>(111);

        CHECK_THROWS_AS(out_buf.DiscardWriteBuf(sizeof(uint32) + 1), NetBufferException);
        CHECK(out_buf.GetDataSize() == 0);
    }

    SECTION("MessageFramingHandlesPartialInput")
    {
        NetOutBuffer out_buf {8, false};
        out_buf.StartMsg(NetMessage::Ping);
        out_buf.Write<uint16>(321);
        out_buf.EndMsg();

        const auto data = out_buf.GetData();
        const auto partial_size = data.size() - 1;

        NetInBuffer in_buf {8};
        in_buf.AddData({data.data(), partial_size});
        CHECK_FALSE(in_buf.NeedProcess());

        in_buf.AddData({data.data() + partial_size, 1});
        CHECK(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::Ping);
        CHECK(in_buf.Read<uint16>() == 321);
        in_buf.ShrinkReadBuf();
        CHECK(in_buf.GetDataSize() == 0);
        CHECK(in_buf.GetReadPos() == 0);
    }

    SECTION("EncryptionRoundtripPreservesPayload")
    {
        NetOutBuffer out_buf {8, false};
        out_buf.SetEncryptKey(123456);
        out_buf.StartMsg(NetMessage::RemoteCall);
        out_buf.Write<uint32>(0xABCD1234);
        out_buf.Write<string_view>("secret");
        out_buf.EndMsg();

        const auto data = out_buf.GetData();
        CHECK(data.size() > sizeof(uint32));

        uint32 stored_signature = 0;
        std::memcpy(&stored_signature, data.data(), sizeof(stored_signature));
        CHECK(stored_signature != NetBuffer::NETMSG_SIGNATURE);

        NetInBuffer in_buf {8};
        in_buf.SetEncryptKey(123456);
        in_buf.AddData(data);

        CHECK(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::RemoteCall);
        CHECK(in_buf.Read<uint32>() == 0xABCD1234);
        CHECK(in_buf.Read<string>() == "secret");
    }

    SECTION("HashedStringRoundtripSupportsDebugMode")
    {
        HashStorage hashes;
        const auto value = hashes.ToHashedString("net_hash_value");

        NetOutBuffer out_buf {8, true};
        out_buf.Write<hstring>(value);

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        const auto read_value = in_buf.Read<hstring>(hashes);
        CHECK(read_value == value);
        CHECK(read_value.as_str() == "net_hash_value");
    }

    SECTION("InvalidSignatureThrows")
    {
        NetInBuffer in_buf {8};
        const uint32 invalid_signature = 0xDEADBEEF;
        in_buf.AddData({reinterpret_cast<const uint8*>(&invalid_signature), sizeof(invalid_signature)});

        CHECK_THROWS_AS(in_buf.NeedProcess(), UnknownMessageException);
    }

    SECTION("InvalidMessageLengthThrowsAndResetsBuffer")
    {
        NetInBuffer in_buf {8};
        const uint32 signature = NetBuffer::NETMSG_SIGNATURE;
        const uint32 invalid_len = sizeof(uint32) + sizeof(uint32) + sizeof(NetMessage) - 1;

        in_buf.AddData({reinterpret_cast<const uint8*>(&signature), sizeof(signature)});
        in_buf.AddData({reinterpret_cast<const uint8*>(&invalid_len), sizeof(invalid_len)});

        CHECK_THROWS_AS(in_buf.NeedProcess(), UnknownMessageException);
        CHECK(in_buf.GetDataSize() == 0);
        CHECK(in_buf.GetReadPos() == 0);
    }
}

FO_END_NAMESPACE
