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
        NetOutBuffer out_buf {8};
        out_buf.Write<int32_t>(42);
        out_buf.Write<float32_t>(12.5f);
        out_buf.Write<string>(string {"hello"});

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        CHECK(in_buf.Read<int32_t>() == 42);
        CHECK(in_buf.Read<float32_t>() == 12.5f);
        CHECK(in_buf.Read<string>() == "hello");
        CHECK(in_buf.GetReadPos() == out_buf.GetDataSize());
    }

    SECTION("DiscardWriteBufDropsPrefix")
    {
        NetOutBuffer out_buf {8};
        out_buf.Write<uint32_t>(111);
        out_buf.Write<uint32_t>(222);
        out_buf.DiscardWriteBuf(sizeof(uint32_t));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        CHECK(in_buf.Read<uint32_t>() == 222);
        CHECK(out_buf.GetDataSize() == sizeof(uint32_t));
    }

    SECTION("DiscardWriteBufTooMuchResetsAndThrows")
    {
        NetOutBuffer out_buf {8};
        out_buf.Write<uint32_t>(111);

        CHECK_THROWS_AS(out_buf.DiscardWriteBuf(sizeof(uint32_t) + 1), NetBufferException);
        CHECK(out_buf.GetDataSize() == 0);
    }

    SECTION("MessageFramingHandlesPartialInput")
    {
        NetOutBuffer out_buf {8};
        out_buf.StartMsg(NetMessage::Ping);
        out_buf.Write<uint16_t>(321);
        out_buf.EndMsg();

        const auto data = out_buf.GetData();
        const auto partial_size = data.size() - 1;

        NetInBuffer in_buf {8};
        in_buf.AddData({data.data(), partial_size});
        CHECK_FALSE(in_buf.NeedProcess());

        in_buf.AddData({data.data() + partial_size, 1});
        CHECK(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::Ping);
        CHECK(in_buf.Read<uint16_t>() == 321);
        in_buf.ShrinkReadBuf();
        CHECK(in_buf.GetDataSize() == 0);
        CHECK(in_buf.GetReadPos() == 0);
    }

    SECTION("EncryptionRoundtripPreservesPayload")
    {
        NetOutBuffer out_buf {8};
        out_buf.SetEncryptKey(123456);
        out_buf.StartMsg(NetMessage::RemoteCall);
        out_buf.Write<uint32_t>(0xABCD1234);
        out_buf.Write<string_view>("secret");
        out_buf.EndMsg();

        const auto data = out_buf.GetData();
        CHECK(data.size() > sizeof(uint32_t));

        uint32_t stored_signature = 0;
        std::memcpy(&stored_signature, data.data(), sizeof(stored_signature));
        CHECK(stored_signature != NetBuffer::NETMSG_SIGNATURE);

        NetInBuffer in_buf {8};
        in_buf.SetEncryptKey(123456);
        in_buf.AddData(data);

        CHECK(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::RemoteCall);
        CHECK(in_buf.Read<uint32_t>() == 0xABCD1234);
        CHECK(in_buf.Read<string>() == "secret");
    }

    SECTION("HashedStringRoundtrip")
    {
        HashStorage hashes {};
        const auto value = hashes.ToHashedString("net_hash_value");

        NetOutBuffer out_buf {8};
        out_buf.Write<hstring>(value);

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        const auto read_value = in_buf.Read<hstring>(hashes);
        CHECK(read_value == value);
        CHECK(read_value.as_str() == "net_hash_value");
    }

    SECTION("UnresolvedHashReportsWithHandlerAndCanBeLearned")
    {
        HashStorage sender {};
        const auto value = sender.ToHashedString("runtime_only_hash");

        NetOutBuffer out_buf {8};
        out_buf.Write<hstring>(value);

        // A receiver that doesn't know this hash yet fails and reports the raw hash
        HashStorage receiver {};
        hstring::hash_t reported_hash = 0;
        receiver.SetResolveHashFailureHandler([&reported_hash](hstring::hash_t hash) { reported_hash = hash; });

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        CHECK_THROWS_AS(in_buf.Read<hstring>(receiver), NetBufferException);
        CHECK(reported_hash == value.as_hash());

        // Learning the string (as the client does from the server's HashList) makes the same hash resolve
        const auto learned = receiver.ToHashedString("runtime_only_hash");
        CHECK(learned.as_hash() == value.as_hash());

        NetInBuffer in_buf_again {8};
        in_buf_again.AddData(out_buf.GetData());
        const auto read_value = in_buf_again.Read<hstring>(receiver);
        CHECK(read_value.as_hash() == value.as_hash());
        CHECK(read_value.as_str() == "runtime_only_hash");
    }

    SECTION("InvalidSignatureThrows")
    {
        NetInBuffer in_buf {8};
        const uint32_t invalid_signature = 0xDEADBEEF;
        in_buf.AddData({reinterpret_cast<const uint8_t*>(&invalid_signature), sizeof(invalid_signature)});

        CHECK_THROWS_AS(in_buf.NeedProcess(), UnknownMessageException);
    }

    SECTION("InvalidMessageLengthThrowsAndResetsBuffer")
    {
        NetInBuffer in_buf {8};
        const uint32_t signature = NetBuffer::NETMSG_SIGNATURE;
        const uint32_t invalid_len = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage) - 1;

        in_buf.AddData({reinterpret_cast<const uint8_t*>(&signature), sizeof(signature)});
        in_buf.AddData({reinterpret_cast<const uint8_t*>(&invalid_len), sizeof(invalid_len)});

        CHECK_THROWS_AS(in_buf.NeedProcess(), UnknownMessageException);
        CHECK(in_buf.GetDataSize() == 0);
        CHECK(in_buf.GetReadPos() == 0);
    }

    SECTION("StringLengthExceedingBufferThrowsBeforeAllocating")
    {
        // A client-declared string length larger than the bytes actually buffered must be rejected
        // before the resize, so a tiny message cannot amplify into a multi-GB allocation
        NetInBuffer in_buf {8};
        const uint32_t bogus_len = 0xFFFFFFFF;
        in_buf.AddData({reinterpret_cast<const uint8_t*>(&bogus_len), sizeof(bogus_len)});

        CHECK_THROWS_AS(in_buf.Read<string>(), NetBufferException);
        CHECK(in_buf.GetDataSize() == 0);
        CHECK(in_buf.GetReadPos() == 0);
    }

    SECTION("MaxMsgLenRejectsOversizedMessage")
    {
        NetOutBuffer out_buf {8};
        out_buf.StartMsg(NetMessage::Ping);
        out_buf.Write<uint32_t>(0);
        out_buf.EndMsg();
        const auto data = out_buf.GetData();

        NetInBuffer in_buf {8};
        in_buf.SetMaxMsgLen(data.size() - 1);
        in_buf.AddData(data);

        CHECK_THROWS_AS(in_buf.NeedProcess(), UnknownMessageException);
        CHECK(in_buf.GetDataSize() == 0);

        // The same message is accepted when the cap admits it
        NetInBuffer in_buf_ok {8};
        in_buf_ok.SetMaxMsgLen(data.size());
        in_buf_ok.AddData(data);
        CHECK(in_buf_ok.NeedProcess());
        CHECK(in_buf_ok.ReadMsg() == NetMessage::Ping);
    }
}

FO_END_NAMESPACE
