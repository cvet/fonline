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

#include "AdminPanelCommon.h"
#include "Common.h"

FO_BEGIN_NAMESPACE

TEST_CASE("AdminChannelCipher")
{
    SECTION("DisabledWithoutPassword")
    {
        AdminChannelCipher cipher;
        CHECK_FALSE(cipher.IsEnabled());

        // An empty password keeps the channel as plain bytes (no-op).
        cipher.Init("");
        CHECK_FALSE(cipher.IsEnabled());

        array<uint8_t, 5> data = {1, 2, 3, 4, 5};
        const auto original = data;
        cipher.Apply(data.data(), data.size());
        CHECK(data == original);
    }

    SECTION("EnabledWithPassword")
    {
        AdminChannelCipher cipher;
        cipher.Init("secret");
        CHECK(cipher.IsEnabled());
    }

    SECTION("EncryptDecryptRoundtrip")
    {
        const string plaintext = "hello admin channel, restart requested";

        AdminChannelCipher encoder;
        encoder.Init("p@ssw0rd");
        string buffer = plaintext;
        encoder.Apply(buffer.data(), buffer.size());
        // The payload must actually be transformed.
        CHECK(buffer != plaintext);

        // A fresh cipher seeded with the same password reproduces the keystream.
        AdminChannelCipher decoder;
        decoder.Init("p@ssw0rd");
        decoder.Apply(buffer.data(), buffer.size());
        CHECK(buffer == plaintext);
    }

    SECTION("KeystreamIsDeterministicPerPassword")
    {
        const string plaintext = "deterministic keystream payload";

        AdminChannelCipher first;
        first.Init("shared-key");
        string a = plaintext;
        first.Apply(a.data(), a.size());

        AdminChannelCipher second;
        second.Init("shared-key");
        string b = plaintext;
        second.Apply(b.data(), b.size());

        CHECK(a == b);
    }

    SECTION("DifferentPasswordsProduceDifferentCiphertext")
    {
        const string plaintext = "a sufficiently long payload to avoid coincidental matches";

        AdminChannelCipher alpha;
        alpha.Init("alpha");
        string a = plaintext;
        alpha.Apply(a.data(), a.size());

        AdminChannelCipher beta;
        beta.Init("beta");
        string b = plaintext;
        beta.Apply(b.data(), b.size());

        CHECK(a != b);
    }

    SECTION("ResetDisablesAndStopsTransforming")
    {
        AdminChannelCipher cipher;
        cipher.Init("secret");
        CHECK(cipher.IsEnabled());

        cipher.Reset();
        CHECK_FALSE(cipher.IsEnabled());

        array<uint8_t, 3> data = {9, 8, 7};
        const auto original = data;
        cipher.Apply(data.data(), data.size());
        CHECK(data == original);
    }

    SECTION("HandlesNullAndEmptyBuffers")
    {
        AdminChannelCipher cipher;
        cipher.Init("secret");

        // Must not crash on degenerate inputs.
        cipher.Apply(nullptr, 0);

        array<uint8_t, 1> data = {42};
        cipher.Apply(data.data(), 0);
        CHECK(data[0] == 42);
    }
}

TEST_CASE("AdminDiscoveryProtocolConstants")
{
    // These constants are part of the wire contract shared between the server
    // host and the admin panel client; pin them so an accidental change breaks
    // the build rather than silently breaking discovery interop.
    CHECK(ADMIN_DISCOVERY_PROTOCOL_VERSION == 1);
    CHECK(ADMIN_DISCOVERY_PROBE == "lf-admin-discover-v1");
    CHECK(ADMIN_DISCOVERY_RESPONSE_PREFIX == "lf-admin-server-v1");
}

FO_END_NAMESPACE
