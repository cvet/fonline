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

#include <atomic>
#include <sstream>

#include "ConfigFile.h"
#include "ContentUpdater.h"
#include "NetSockets.h"
#include "UpdaterBackend.h"
#include "UpdaterFastClient.h"
#include "UpdaterFastServer.h"

FO_BEGIN_NAMESPACE

namespace
{
    constexpr string_view TestUpdateSigningPublicKey = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
    constexpr string_view TestUpdateSigningPrivateKey = "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
    constexpr uint32_t TestUpdateSigningKeyId = 4294967294U;
    constexpr uint64_t TestUpdateReleaseSequence = 7;

    auto MakeTempContentUpdaterDir(string_view name) -> string
    {
        FO_STACK_TRACE_ENTRY();

        const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs_path_to_string(base);
    }

    auto AcquireContentUpdaterTestPort() -> uint16_t
    {
        FO_STACK_TRACE_ENTRY();

        static std::atomic<uint16_t> next_port {43100};
        return next_port.fetch_add(1, std::memory_order_relaxed);
    }

    auto MakeUpdaterSettings(string_view temp_dir, int32_t bind_port, bool fast_update_enabled = true, optional<uint16_t> endpoint_port = std::nullopt, bool advertise_endpoint = true, int32_t fast_update_max_retries = 4, int32_t fast_update_chunk_size = 257, bool update_files_in_memory = false) -> GlobalSettings
    {
        FO_STACK_TRACE_ENTRY();

        string client_resources = strex(temp_dir).combine_path("server_resources").str();
        string platform_binaries = strex(temp_dir).combine_path("platform_binaries").str();
        std::ranges::replace(client_resources, '\\', '/');
        std::ranges::replace(platform_binaries, '\\', '/');
        const int32_t advertised_port = endpoint_port.has_value() ? numeric_cast<int32_t>(*endpoint_port) : bind_port;
        const string endpoint_setting = advertise_endpoint ? strex("127.0.0.1:{}:10", advertised_port).str() : string {};

        ConfigFile config {strex("Baking.ClientResources = {}\n"
                                 "Baking.PlatformBinaries = {}\n"
                                 "ServerNetwork.UpdateFilesInMemory = {}\n"
                                 "Network.FastUpdateEnabled = {}\n"
                                 "Network.FastUpdateChunkSize = {}\n"
                                 "Network.FastUpdateRequestTimeout = 50\n"
                                 "Network.FastUpdateMaxRetries = {}\n"
                                 "Network.FastUpdateMaxSockets = 2\n"
                                 "Network.UpdateManifestSignatureRequired = True\n"
                                 "Network.UpdateManifestTrustedPublicKeys = {}:{}\n"
                                 "Network.UpdateManifestMinimumReleaseSequence = {}\n"
                                 "ServerNetwork.FastUpdateServerEnabled = True\n"
                                 "ServerNetwork.FastUpdateBindHost = 127.0.0.1\n"
                                 "ServerNetwork.FastUpdateBindPort = {}\n"
                                 "ServerNetwork.FastUpdateEndpoints = {}\n"
                                 "ServerNetwork.UpdateManifestSigningKey = {}:{}:{}\n"
                                 "ServerNetwork.UpdateManifestReleaseSequence = {}\n"
                                 "[ResourcePack]\n"
                                 "Name = FastPack\n"
                                 "ClientOnly = True\n",
            client_resources, platform_binaries, update_files_in_memory ? "True" : "False", fast_update_enabled ? "True" : "False", fast_update_chunk_size, fast_update_max_retries, TestUpdateSigningKeyId, TestUpdateSigningPublicKey, TestUpdateReleaseSequence, bind_port, endpoint_setting, TestUpdateSigningKeyId, TestUpdateSigningPublicKey, TestUpdateSigningPrivateKey, TestUpdateReleaseSequence)
                .str()};

        GlobalSettings settings {false};
        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();
        settings.ApplyConfigFile(config, "");
        return settings;
    }

    auto VerifyTestDescriptor(const_span<uint8_t> descriptor, string_view binary_target) -> ContentUpdateManifest
    {
        FO_STACK_TRACE_ENTRY();

        ContentUpdateTrustedPublicKey key;
        FO_VERIFY_AND_THROW(TryParseContentUpdateTrustedPublicKey(strex("{}:{}", TestUpdateSigningKeyId, TestUpdateSigningPublicKey), key), "Invalid test updater public key");
        const VerifiedContentUpdateManifest verified = VerifyContentUpdateManifestDescriptor(descriptor, binary_target, TestUpdateReleaseSequence, {key});
        FO_VERIFY_AND_THROW(verified.KeyId == TestUpdateSigningKeyId, "Unexpected test updater key id");
        FO_VERIFY_AND_THROW(verified.ReleaseSequence == TestUpdateReleaseSequence, "Unexpected test updater release sequence");
        FO_VERIFY_AND_THROW(verified.BinaryTarget.empty() || verified.BinaryTarget == binary_target, "Unexpected test updater binary target");
        FO_VERIFY_AND_THROW(!verified.BinaryTarget.empty() || std::ranges::none_of(verified.Manifest.Files, [](const ContentUpdateFileInfo& file) { return file.Target == UpdateFileTarget::ClientBinaries; }), "Wildcard test updater descriptor contains client binaries");
        return verified.Manifest;
    }

    auto MakePayload(size_t size) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        vector<uint8_t> payload(size);

        for (size_t index = 0; index != payload.size(); ++index) {
            payload[index] = numeric_cast<uint8_t>((index * 31u + 17u) & 0xFFu);
        }

        return payload;
    }

    template<typename T>
    void WritePodAt(vector<uint8_t>& data, size_t offset, T value)
    {
        FO_STACK_TRACE_ENTRY();

        static_assert(std::is_standard_layout_v<T>);
        REQUIRE(offset <= data.size());
        REQUIRE(sizeof(T) <= data.size() - offset);
        MemCopy(data.data() + offset, &value, sizeof(value));
    }

    auto GetContentUpdateSnapshotBasePathForTest() -> std::filesystem::path
    {
        FO_STACK_TRACE_ENTRY();

        return std::filesystem::temp_directory_path() / "fonline-content-update-snapshots";
    }

    auto MakeContentUpdateSnapshotRootPathForTest(uint64_t snapshot_id, uint32_t attempt = 0) -> string
    {
        FO_STACK_TRACE_ENTRY();

        return fs_path_to_string(GetContentUpdateSnapshotBasePathForTest() / strex("catalog-{}-{}", snapshot_id, attempt).str());
    }

    auto FindContentUpdateSnapshotRootsForTest(uint64_t snapshot_id) -> vector<string>
    {
        FO_STACK_TRACE_ENTRY();

        vector<string> roots;
        const auto base_path = GetContentUpdateSnapshotBasePathForTest();
        std::error_code error;

        if (!std::filesystem::is_directory(base_path, error) || error) {
            return roots;
        }

        const string prefix = strex("catalog-{}-", snapshot_id).str();

        for (const auto& entry : std::filesystem::directory_iterator {base_path}) {
            const string name = fs_path_to_string(entry.path().filename());

            if (entry.is_directory() && name.starts_with(prefix)) {
                roots.emplace_back(fs_path_to_string(entry.path()));
            }
        }

        return roots;
    }

    auto MakeContentUpdateSnapshotIdForTest(const UpdaterBackend& backend, uint64_t generation) -> uint64_t
    {
        FO_STACK_TRACE_ENTRY();

        return (numeric_cast<uint64_t>(backend.GetFastUpdateSessionId()) << 32U) | (generation & UINT64_C(0xFFFF'FFFF));
    }
}

TEST_CASE("ContentUpdater")
{
    SECTION("Sha256KnownVectorsAndStreaming")
    {
        const Sha256Digest empty_expected = {
            0xe3,
            0xb0,
            0xc4,
            0x42,
            0x98,
            0xfc,
            0x1c,
            0x14,
            0x9a,
            0xfb,
            0xf4,
            0xc8,
            0x99,
            0x6f,
            0xb9,
            0x24,
            0x27,
            0xae,
            0x41,
            0xe4,
            0x64,
            0x9b,
            0x93,
            0x4c,
            0xa4,
            0x95,
            0x99,
            0x1b,
            0x78,
            0x52,
            0xb8,
            0x55,
        };
        const Sha256Digest abc_expected = {
            0xba,
            0x78,
            0x16,
            0xbf,
            0x8f,
            0x01,
            0xcf,
            0xea,
            0x41,
            0x41,
            0x40,
            0xde,
            0x5d,
            0xae,
            0x22,
            0x23,
            0xb0,
            0x03,
            0x61,
            0xa3,
            0x96,
            0x17,
            0x7a,
            0x9c,
            0xb4,
            0x10,
            0xff,
            0x61,
            0xf2,
            0x00,
            0x15,
            0xad,
        };
        const vector<uint8_t> empty;
        const vector<uint8_t> abc = {'a', 'b', 'c'};
        const string multi_block_text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        const vector<uint8_t> multi_block(multi_block_text.begin(), multi_block_text.end());
        const Sha256Digest multi_block_expected = {
            0x24,
            0x8d,
            0x6a,
            0x61,
            0xd2,
            0x06,
            0x38,
            0xb8,
            0xe5,
            0xc0,
            0x26,
            0x93,
            0x0c,
            0x3e,
            0x60,
            0x39,
            0xa3,
            0x3c,
            0xe4,
            0x59,
            0x64,
            0xff,
            0x21,
            0x67,
            0xf6,
            0xec,
            0xed,
            0xd4,
            0x19,
            0xdb,
            0x06,
            0xc1,
        };

        CHECK(ComputeSha256(empty) == empty_expected);
        CHECK(ComputeSha256(abc) == abc_expected);
        CHECK(ComputeSha256(multi_block) == multi_block_expected);

        Sha256Hasher streaming_hasher;
        streaming_hasher.Update({abc.data(), 1});
        streaming_hasher.Update({});
        const Sha256Digest a_digest = streaming_hasher.Finalize();
        CHECK(a_digest != abc_expected);
        streaming_hasher.Update({abc.data() + 1, abc.size() - 1});
        CHECK(streaming_hasher.Finalize() == abc_expected);
        CHECK(streaming_hasher.Finalize() == abc_expected);

        streaming_hasher.Reset();
        streaming_hasher.Update({multi_block.data(), 17});
        streaming_hasher.Update({multi_block.data() + 17, 32});
        streaming_hasher.Update({multi_block.data() + 49, multi_block.size() - 49});
        CHECK(streaming_hasher.Finalize() == multi_block_expected);

        const string abc_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        CHECK(Sha256DigestToHex(abc_expected) == abc_hex);

        Sha256Digest parsed {};
        REQUIRE(TryParseSha256Digest(abc_hex, parsed));
        CHECK(parsed == abc_expected);
        REQUIRE(TryParseSha256Digest("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", parsed));
        CHECK(parsed == abc_expected);
        CHECK_FALSE(TryParseSha256Digest("ba78", parsed));
        CHECK_FALSE(TryParseSha256Digest("za7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", parsed));

        const vector<uint8_t> hmac_key(20, UINT8_C(0x0b));
        const string hmac_text = "Hi There";
        const vector<uint8_t> hmac_data(hmac_text.begin(), hmac_text.end());
        Sha256Digest hmac_expected {};
        REQUIRE(TryParseSha256Digest("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", hmac_expected));
        CHECK(ComputeHmacSha256(hmac_key, hmac_data) == hmac_expected);

        const string temp_path = MakeTempContentUpdaterDir("sha256_file") + ".bin";
        const auto cleanup = scope_exit([&temp_path]() noexcept { safe_call([&] { fs_remove_file(temp_path); }); });
        REQUIRE(fs_write_file(temp_path, abc));
        const auto file_digest = fs_sha256_file(temp_path);
        REQUIRE(file_digest.has_value());
        CHECK(*file_digest == abc_expected);
        CHECK_FALSE(fs_sha256_file(temp_path + ".missing").has_value());
    }

    SECTION("EndpointParsing")
    {
        const auto endpoint = ParseContentUpdateEndpoint("127.0.0.1:43010:5");

        REQUIRE(endpoint.has_value());
        CHECK(endpoint->Host == "127.0.0.1");
        CHECK(endpoint->Port == 43010);
        CHECK(endpoint->Priority == 5);

        CHECK_FALSE(ParseContentUpdateEndpoint("").has_value());
        CHECK_FALSE(ParseContentUpdateEndpoint("127.0.0.1").has_value());
        CHECK_FALSE(ParseContentUpdateEndpoint("127.0.0.1:0").has_value());
        CHECK_FALSE(ParseContentUpdateEndpoint("127.0.0.1:70000").has_value());
    }

    SECTION("ManifestRoundtrip")
    {
        ContentUpdateSourceReportToken report_token_111 {};
        ContentUpdateSourceReportToken report_token_222 {};
        report_token_111[0] = 111;
        report_token_222[0] = 222;
        ContentUpdateManifest manifest;
        manifest.CatalogGeneration = 987654321;
        manifest.FastUpdateEnabled = true;
        manifest.SelfHostedServerEnabled = true;
        manifest.SessionId = 123456;
        manifest.ChunkSize = 1024;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "mirror.example",
            .Port = 43010,
            .Priority = 7,
        });
        manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 42,
            .Name = "Data/client.zip",
            .Size = 2049,
            .Hash = 0x1122334455667788ULL,
            .Target = UpdateFileTarget::ClientResources,
            .ChunkHashes = {1, 2, 3},
            .Sources =
                {
                    ContentUpdateSource {
                        .Provider = "mirror-b",
                        .SourceKey = "primary",
                        .Transport = "https",
                        .Locator = "https://b.example/client.zip?token=opaque",
                        .Priority = 5,
                        .ExpiresAt = 123456789,
                        .ReportToken = report_token_222,
                    },
                    ContentUpdateSource {
                        .Provider = "mirror-a",
                        .SourceKey = "primary",
                        .Transport = "future-transport",
                        .Locator = "opaque:source-payload",
                        .Priority = 10,
                        .ReportToken = report_token_111,
                    },
                },
        });
        manifest.Files[0].Sha256 = ComputeSha256(vector<uint8_t> {'a', 'b', 'c'});

        vector<uint8_t> data;
        SerializeContentUpdateManifest(manifest, data);

        const auto restored = DeserializeContentUpdateManifest(data);

        CHECK(restored.CatalogGeneration == manifest.CatalogGeneration);
        REQUIRE(restored.FastUpdateEnabled);
        REQUIRE(restored.SelfHostedServerEnabled);
        CHECK(restored.SessionId == manifest.SessionId);
        CHECK(restored.ChunkSize == manifest.ChunkSize);
        REQUIRE(restored.Endpoints.size() == 1);
        CHECK(restored.Endpoints[0].Host == "mirror.example");
        CHECK(restored.Endpoints[0].Port == 43010);
        CHECK(restored.Endpoints[0].Priority == 7);
        REQUIRE(restored.Files.size() == 1);
        CHECK(restored.Files[0].FileIndex == 42);
        CHECK(restored.Files[0].Name == "Data/client.zip");
        CHECK(restored.Files[0].Size == 2049);
        CHECK(restored.Files[0].Hash == 0x1122334455667788ULL);
        CHECK(restored.Files[0].Sha256 == manifest.Files[0].Sha256);
        CHECK(restored.Files[0].Target == UpdateFileTarget::ClientResources);
        const vector<uint64_t> expected_chunk_hashes {1, 2, 3};
        CHECK(restored.Files[0].ChunkHashes == expected_chunk_hashes);
        REQUIRE(restored.Files[0].Sources.size() == 2);
        CHECK(restored.Files[0].Sources[0].Provider == "mirror-a");
        CHECK(restored.Files[0].Sources[0].SourceKey == "primary");
        CHECK(restored.Files[0].Sources[0].Transport == "future-transport");
        CHECK(restored.Files[0].Sources[0].Locator == "opaque:source-payload");
        CHECK(restored.Files[0].Sources[0].Priority == 10);
        CHECK(restored.Files[0].Sources[0].ExpiresAt == 0);
        CHECK(restored.Files[0].Sources[0].ReportToken == report_token_111);
        CHECK(restored.Files[0].Sources[1].Provider == "mirror-b");
        CHECK(restored.Files[0].Sources[1].ExpiresAt == 123456789);
        CHECK(restored.Files[0].Sources[1].ReportToken == report_token_222);

        vector<uint8_t> canonical_data;
        SerializeContentUpdateManifest(restored, canonical_data);
        CHECK(canonical_data == data);
    }

    SECTION("SignedDescriptorAuthenticationAndRollbackProtection")
    {
        ContentUpdateSigningKey signing_key;
        ContentUpdateTrustedPublicKey trusted_key;
        REQUIRE(TryParseContentUpdateSigningKey(strex("{}:{}:{}", TestUpdateSigningKeyId, TestUpdateSigningPublicKey, TestUpdateSigningPrivateKey), signing_key));
        REQUIRE(TryParseContentUpdateTrustedPublicKey(strex("{}:{}", TestUpdateSigningKeyId, TestUpdateSigningPublicKey), trusted_key));
        CHECK_FALSE(TryParseContentUpdateSigningKey("1:bad:key", signing_key));
        CHECK_FALSE(TryParseContentUpdateTrustedPublicKey("0:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", trusted_key));

        ContentUpdateManifest manifest;
        manifest.CatalogGeneration = 42;
        manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 1,
            .Name = "ClientRuntime.dll",
            .Size = 3,
            .Hash = 7,
            .Sha256 = ComputeSha256(vector<uint8_t> {'a', 'b', 'c'}),
            .Target = UpdateFileTarget::ClientBinaries,
        });

        vector<uint8_t> manifest_data;
        SerializeContentUpdateManifest(manifest, manifest_data);
        vector<uint8_t> descriptor;
        SignContentUpdateManifestDescriptor(manifest_data, "Windows-win64", TestUpdateReleaseSequence, signing_key, descriptor);

        ContentUpdateTrustedPublicKey rotation_key = trusted_key;
        rotation_key.KeyId = 123;
        const VerifiedContentUpdateManifest verified = VerifyContentUpdateManifestDescriptor(descriptor, "Windows-win64", TestUpdateReleaseSequence, {rotation_key, trusted_key});
        CHECK(verified.KeyId == TestUpdateSigningKeyId);
        CHECK(verified.ReleaseSequence == TestUpdateReleaseSequence);
        CHECK(verified.BinaryTarget == "Windows-win64");
        REQUIRE(verified.Manifest.Files.size() == 1);
        CHECK(verified.Manifest.Files[0].Sha256 == manifest.Files[0].Sha256);

        CHECK_THROWS_AS(VerifyContentUpdateManifestDescriptor(descriptor, "Linux-x64", TestUpdateReleaseSequence, {trusted_key}), ContentUpdaterException);
        CHECK_THROWS_AS(VerifyContentUpdateManifestDescriptor(descriptor, "Windows-win64", TestUpdateReleaseSequence + 1, {trusted_key}), ContentUpdaterException);
        CHECK_THROWS_AS(VerifyContentUpdateManifestDescriptor(descriptor, "Windows-win64", TestUpdateReleaseSequence, {rotation_key}), ContentUpdaterException);

        ContentUpdateManifest resource_manifest;
        resource_manifest.CatalogGeneration = 43;
        resource_manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 2,
            .Name = "Resources.zip",
            .Size = 3,
            .Hash = 8,
            .Sha256 = ComputeSha256(vector<uint8_t> {'d', 'e', 'f'}),
            .Target = UpdateFileTarget::ClientResources,
        });
        vector<uint8_t> resource_manifest_data;
        SerializeContentUpdateManifest(resource_manifest, resource_manifest_data);
        vector<uint8_t> wildcard_descriptor;
        SignContentUpdateManifestDescriptor(resource_manifest_data, "", TestUpdateReleaseSequence, signing_key, wildcard_descriptor);
        CHECK(VerifyContentUpdateManifestDescriptor(wildcard_descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}).BinaryTarget.empty());
        CHECK(VerifyContentUpdateManifestDescriptor(wildcard_descriptor, "Linux-x64", TestUpdateReleaseSequence, {trusted_key}).BinaryTarget.empty());

        vector<uint8_t> wildcard_binary_descriptor;
        SignContentUpdateManifestDescriptor(manifest_data, "", TestUpdateReleaseSequence, signing_key, wildcard_binary_descriptor);
        CHECK_THROWS_AS(VerifyContentUpdateManifestDescriptor(wildcard_binary_descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}), ContentUpdaterException);

        vector<uint8_t> tampered = descriptor;
        REQUIRE(tampered.size() > 32);
        tampered[32] ^= UINT8_C(0x01);
        CHECK_THROWS_AS(VerifyContentUpdateManifestDescriptor(tampered, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}), ContentUpdaterException);

        const string trust_root = MakeTempContentUpdaterDir("content_updater_release_trust");
        const bool removed_before = fs_remove_dir_tree(trust_root);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&trust_root]() noexcept { safe_call([&] { fs_remove_dir_tree(trust_root); }); });
        CHECK(AcceptContentUpdateReleaseSequence(trust_root, 7, 7));
        CHECK(AcceptContentUpdateReleaseSequence(trust_root, 9, 7));
        CHECK_FALSE(AcceptContentUpdateReleaseSequence(trust_root, 8, 7));
        CHECK(AcceptContentUpdateReleaseSequence(trust_root, 10, 10));

        const string corrupt_marker = strex(trust_root).combine_path("UpdaterTrust/release-11.accepted").str();
        REQUIRE(fs_write_file(corrupt_marker, "corrupt"));
        CHECK(AcceptContentUpdateReleaseSequence(trust_root, 12, 7));
        CHECK_FALSE(fs_exists(corrupt_marker));

        const string stale_pending = strex(trust_root).combine_path("UpdaterTrust/.release-13.accepted.pending-crashed-process").str();
        const string malformed_marker = strex(trust_root).combine_path("UpdaterTrust/release-invalid.accepted").str();
        REQUIRE(fs_write_file(stale_pending, "partial"));
        REQUIRE(fs_write_file(malformed_marker, "partial"));
        CHECK(AcceptContentUpdateReleaseSequence(trust_root, 13, 7));
        CHECK_FALSE(AcceptContentUpdateReleaseSequence(trust_root, 11, 7));
        CHECK(fs_read_file(strex(trust_root).combine_path("UpdaterTrust/release-13.accepted").str()) == optional<string> {"FONLINE_CONTENT_UPDATE_TRUST_V1\n13\n"});

        vector<std::future<bool>> concurrent_accepts;

        for (size_t index = 0; index != 8; ++index) {
            concurrent_accepts.emplace_back(std::async(std::launch::async, [&trust_root] { return AcceptContentUpdateReleaseSequence(trust_root, 14, 7); }));
        }

        for (auto& accepted : concurrent_accepts) {
            CHECK(accepted.get());
        }

        CHECK(fs_read_file(strex(trust_root).combine_path("UpdaterTrust/release-14.accepted").str()) == optional<string> {"FONLINE_CONTENT_UPDATE_TRUST_V1\n14\n"});
    }

    SECTION("StagedBinaryAuthorizationBindsDescriptorTargetAndFileIdentity")
    {
        ContentUpdateSigningKey signing_key;
        ContentUpdateTrustedPublicKey trusted_key;
        REQUIRE(TryParseContentUpdateSigningKey(strex("{}:{}:{}", TestUpdateSigningKeyId, TestUpdateSigningPublicKey, TestUpdateSigningPrivateKey), signing_key));
        REQUIRE(TryParseContentUpdateTrustedPublicKey(strex("{}:{}", TestUpdateSigningKeyId, TestUpdateSigningPublicKey), trusted_key));

        const vector<uint8_t> staged_binary {'r', 'u', 'n'};
        const Sha256Digest staged_sha256 = ComputeSha256(staged_binary);
        ContentUpdateManifest manifest;
        manifest.CatalogGeneration = 42;
        manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 1,
            .Name = "PlatformBinaries/Windows-win64/LastFrontier.dll",
            .Size = staged_binary.size(),
            .Hash = 7,
            .Sha256 = staged_sha256,
            .Target = UpdateFileTarget::ClientBinaries,
        });

        vector<uint8_t> manifest_data;
        SerializeContentUpdateManifest(manifest, manifest_data);
        vector<uint8_t> descriptor;
        SignContentUpdateManifestDescriptor(manifest_data, "Windows-win64", TestUpdateReleaseSequence, signing_key, descriptor);

        CHECK(VerifyContentUpdateStagedBinaryAuthorization(descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size(), staged_sha256));
        CHECK(VerifyContentUpdateStagedBinaryAuthorization(descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "InstalledRuntime.dll", "LastFrontier.dll", staged_binary.size(), staged_sha256));

        const vector<uint8_t> missing_descriptor;
        CHECK_THROWS_AS(VerifyContentUpdateStagedBinaryAuthorization(missing_descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size(), staged_sha256), ContentUpdaterException);

        vector<uint8_t> tampered_descriptor = descriptor;
        REQUIRE_FALSE(tampered_descriptor.empty());
        tampered_descriptor.back() ^= UINT8_C(0x01);
        CHECK_THROWS_AS(VerifyContentUpdateStagedBinaryAuthorization(tampered_descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size(), staged_sha256), ContentUpdaterException);

        vector<uint8_t> empty_target_descriptor;
        SignContentUpdateManifestDescriptor(manifest_data, "", TestUpdateReleaseSequence, signing_key, empty_target_descriptor);
        CHECK_THROWS_AS(VerifyContentUpdateStagedBinaryAuthorization(empty_target_descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size(), staged_sha256), ContentUpdaterException);
        CHECK_THROWS_AS(VerifyContentUpdateStagedBinaryAuthorization(descriptor, "Linux-x64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size(), staged_sha256), ContentUpdaterException);

        CHECK_FALSE(VerifyContentUpdateStagedBinaryAuthorization(descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "OtherRuntime.dll", "", staged_binary.size(), staged_sha256));
        CHECK_FALSE(VerifyContentUpdateStagedBinaryAuthorization(descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size() + 1, staged_sha256));
        CHECK_FALSE(VerifyContentUpdateStagedBinaryAuthorization(descriptor, "Windows-win64", TestUpdateReleaseSequence, {trusted_key}, "LastFrontier.dll", "", staged_binary.size(), ComputeSha256(vector<uint8_t> {'b', 'a', 'd'})));
    }

    SECTION("SourceReportTicketsAreOneShotPerConnection")
    {
        ContentUpdateSourceReportReplayGuard replay_guard;
        ContentUpdateSourceReportToken empty_token {};
        ContentUpdateSourceReportToken first_token {};
        first_token[0] = 1;

        CHECK_FALSE(replay_guard.Consume(first_token));
        CHECK_THROWS(replay_guard.Begin(0));
        replay_guard.Begin(42);
        CHECK(replay_guard.GetSessionId() == 42);
        CHECK_FALSE(replay_guard.Consume(empty_token));
        CHECK(replay_guard.Consume(first_token));
        CHECK_FALSE(replay_guard.Consume(first_token));

        for (size_t token_index = 2; token_index <= ContentUpdateMaxConsumedSourceReportTokens; ++token_index) {
            ContentUpdateSourceReportToken token {};
            token[0] = numeric_cast<uint8_t>(token_index & 0xFFU);
            token[1] = numeric_cast<uint8_t>(token_index >> 8U);
            REQUIRE(replay_guard.Consume(token));
        }

        ContentUpdateSourceReportToken over_limit_token {};
        over_limit_token[2] = 1;
        CHECK_FALSE(replay_guard.Consume(over_limit_token));
        CHECK_THROWS(replay_guard.Begin(43));
    }

    SECTION("ManifestRejectsMalformedPayload")
    {
        const vector<uint8_t> empty_data;
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(empty_data), ContentUpdaterException);

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 123456;
        manifest.ChunkSize = 1024;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "mirror.example",
            .Port = 43010,
            .Priority = 7,
        });
        manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 42,
            .Name = "Data/client.zip",
            .Size = 2049,
            .Hash = 0x1122334455667788ULL,
            .Target = UpdateFileTarget::ClientResources,
            .ChunkHashes = {1, 2, 3},
        });

        vector<uint8_t> data;
        SerializeContentUpdateManifest(manifest, data);

        REQUIRE_FALSE(data.empty());
        data.pop_back();
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        constexpr size_t manifest_fixed_header_size = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint64_t);
        constexpr size_t fast_update_enabled_offset = manifest_fixed_header_size;
        constexpr size_t self_hosted_server_enabled_offset = fast_update_enabled_offset + sizeof(uint8_t);

        SerializeContentUpdateManifest(manifest, data);
        WritePodAt<uint8_t>(data, fast_update_enabled_offset, 2);
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        SerializeContentUpdateManifest(manifest, data);
        WritePodAt<uint8_t>(data, self_hosted_server_enabled_offset, 2);
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        ContentUpdateManifest empty_manifest;
        SerializeContentUpdateManifest(empty_manifest, data);
        constexpr size_t endpoints_count_offset = manifest_fixed_header_size + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
        WritePodAt<uint32_t>(data, endpoints_count_offset, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        SerializeContentUpdateManifest(empty_manifest, data);
        constexpr size_t files_count_offset = endpoints_count_offset + sizeof(uint32_t);
        WritePodAt<uint32_t>(data, files_count_offset, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        ContentUpdateManifest single_file_manifest;
        single_file_manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 42,
            .Name = "x",
            .Size = 2049,
            .Hash = 0x1122334455667788ULL,
            .Target = UpdateFileTarget::ClientResources,
        });

        SerializeContentUpdateManifest(single_file_manifest, data);

        const size_t chunk_count_offset = files_count_offset + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t) + single_file_manifest.Files[0].Name.size() + sizeof(uint64_t) + sizeof(uint64_t) + Sha256DigestSize + sizeof(UpdateFileTarget);
        WritePodAt<uint32_t>(data, chunk_count_offset, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        const size_t sources_count_offset = chunk_count_offset + sizeof(uint32_t);
        SerializeContentUpdateManifest(single_file_manifest, data);
        WritePodAt<uint32_t>(data, sources_count_offset, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        const size_t file_target_offset = chunk_count_offset - sizeof(UpdateFileTarget);
        SerializeContentUpdateManifest(single_file_manifest, data);
        WritePodAt<uint8_t>(data, file_target_offset, 255);
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(data), ContentUpdaterException);

        manifest.Endpoints[0].Host.clear();
        CHECK_THROWS_AS(SerializeContentUpdateManifest(manifest, data), ContentUpdaterException);

        manifest.Endpoints[0].Host = "mirror.example";
        manifest.Endpoints[0].Port = 0;
        CHECK_THROWS_AS(SerializeContentUpdateManifest(manifest, data), ContentUpdaterException);

        manifest.Endpoints[0].Port = 43010;
        manifest.Files[0].Name.clear();
        CHECK_THROWS_AS(SerializeContentUpdateManifest(manifest, data), ContentUpdaterException);

        for (const string invalid_name : {string {"../evil.zip"}, string {"Data/../evil.zip"}, string {"/evil.zip"}, string {"C:/evil.zip"}, string {"Data\\evil.zip"}, string {"Data//evil.zip"}}) {
            manifest.Files[0].Name = invalid_name;
            CHECK_THROWS_AS(SerializeContentUpdateManifest(manifest, data), ContentUpdaterException);
        }
    }

    SECTION("ManifestRejectsPayloadAboveNetworkAllocationLimit")
    {
        const vector<uint8_t> oversized_descriptor(ContentUpdateMaxDescriptorSize + 1);
        CHECK_THROWS_AS(VerifyContentUpdateManifestDescriptor(oversized_descriptor, "Windows-win64", TestUpdateReleaseSequence, {}), ContentUpdaterException);
        CHECK_THROWS_AS(DeserializeContentUpdateManifest(const_span<uint8_t> {oversized_descriptor}.first(ContentUpdateMaxManifestSize + 1)), ContentUpdaterException);
    }

    SECTION("ManifestRejectsOversizedSerializedStrings")
    {
        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 123456;
        manifest.ChunkSize = 1024;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "mirror.example",
            .Port = 43010,
            .Priority = 7,
        });
        manifest.Files.emplace_back(ContentUpdateFileInfo {
            .FileIndex = 42,
            .Name = "Data/client.zip",
            .Size = 2049,
            .Hash = 0x1122334455667788ULL,
            .Target = UpdateFileTarget::ClientResources,
            .ChunkHashes = {1, 2, 3},
        });

        const size_t oversized_string_length = numeric_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1;
        const string oversized_string(oversized_string_length, 'x');
        vector<uint8_t> data;

        manifest.Endpoints[0].Host = oversized_string;
        CHECK_THROWS_AS(SerializeContentUpdateManifest(manifest, data), ContentUpdaterException);

        manifest.Endpoints[0].Host = "mirror.example";
        manifest.Files[0].Name = oversized_string;
        CHECK_THROWS_AS(SerializeContentUpdateManifest(manifest, data), ContentUpdaterException);
    }

    SECTION("ExternalSourceValidationAndCaps")
    {
        ContentUpdateSource source {
            .Provider = "project-cloud",
            .SourceKey = "content.0123",
            .Transport = "future_transport",
            .Locator = "opaque://payload?value=1",
            .Priority = 10,
            .ExpiresAt = 123456789,
        };

        CHECK_NOTHROW(ValidateContentUpdateSource(source));

        source.Provider = "fonline";
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);
        source.Provider = "FONLINE.internal";
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);
        source.Provider = ".project";
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);
        source.Provider = "project-cloud";
        source.SourceKey = "invalid/key";
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);
        source.SourceKey = "content.0123";
        source.Locator = "opaque\npayload";
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);
        source.Locator = "opaque://payload";
        source.ExpiresAt = -1;
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);

        source.ExpiresAt = 0;
        vector<ContentUpdateSource> duplicate_sources = {source, source};
        CHECK_THROWS_AS(CanonicalizeContentUpdateSources(duplicate_sources), ContentUpdaterException);

        vector<ContentUpdateSource> too_many_sources(ContentUpdateMaxSourcesPerFile + 1, source);
        CHECK_THROWS_AS(CanonicalizeContentUpdateSources(too_many_sources), ContentUpdaterException);

        source.Locator = string(ContentUpdateMaxSourceLocatorLength + 1, 'x');
        CHECK_THROWS_AS(ValidateContentUpdateSource(source), ContentUpdaterException);

        vector<ContentUpdateSource> oversized_sources;

        for (uint32_t index = 0; index != 9; ++index) {
            oversized_sources.emplace_back(ContentUpdateSource {
                .Provider = "project-cloud",
                .SourceKey = strex("content{}", index).str(),
                .Transport = "https",
                .Locator = string(ContentUpdateMaxSourceLocatorLength, 'x'),
            });
        }

        CHECK_THROWS_AS(CanonicalizeContentUpdateSources(oversized_sources), ContentUpdaterException);
    }

    SECTION("DatagramRoundtrip")
    {
        const auto request_data = MakeContentUpdateChunkRequestData(ContentUpdateChunkRequest {
            .SessionId = 77,
            .FileIndex = 3,
            .ChunkIndex = 9,
            .ClientNonce = 0x0102030405060708ULL,
            .CookieExpiresAt = 123456,
            .Cookie = {1, 2, 3, 4},
        });

        ContentUpdateChunkRequest request;
        REQUIRE(TryReadContentUpdateChunkRequestData(request_data, request));
        CHECK(request.SessionId == 77);
        CHECK(request.FileIndex == 3);
        CHECK(request.ChunkIndex == 9);
        CHECK(request.ClientNonce == 0x0102030405060708ULL);
        CHECK(request.CookieExpiresAt == 123456);
        CHECK(request.Cookie[0] == 1);
        CHECK(request.Cookie[3] == 4);

        const auto challenge_data = MakeContentUpdateCookieChallengeData(ContentUpdateCookieChallenge {
            .SessionId = request.SessionId,
            .FileIndex = request.FileIndex,
            .ChunkIndex = request.ChunkIndex,
            .ClientNonce = request.ClientNonce,
            .CookieExpiresAt = 654321,
            .Cookie = {9, 8, 7, 6},
        });
        CHECK(challenge_data.size() == request_data.size());

        ContentUpdateCookieChallenge challenge;
        REQUIRE(TryReadContentUpdateCookieChallengeData(challenge_data, challenge));
        CHECK(challenge.SessionId == request.SessionId);
        CHECK(challenge.FileIndex == request.FileIndex);
        CHECK(challenge.ChunkIndex == request.ChunkIndex);
        CHECK(challenge.ClientNonce == request.ClientNonce);
        CHECK(challenge.CookieExpiresAt == 654321);
        CHECK(challenge.Cookie[0] == 9);
        CHECK(challenge.Cookie[3] == 6);

        vector<uint8_t> payload = {10, 20, 30};
        vector<uint8_t> chunk_data;
        MakeContentUpdateChunkData(
            ContentUpdateChunkDataHeader {
                .SessionId = 77,
                .FileIndex = 3,
                .ChunkIndex = 9,
                .ChunkSize = numeric_cast<uint32_t>(payload.size()),
                .ChunkHash = 0xAABBCCDD00112233ULL,
            },
            payload, chunk_data);

        ContentUpdateChunkDataHeader header;
        const_span<uint8_t> read_payload {};

        REQUIRE(TryReadContentUpdateChunkData(chunk_data, header, read_payload));
        CHECK(header.SessionId == 77);
        CHECK(header.FileIndex == 3);
        CHECK(header.ChunkIndex == 9);
        CHECK(numeric_cast<size_t>(header.ChunkSize) == payload.size());
        CHECK(header.ChunkHash == 0xAABBCCDD00112233ULL);
        REQUIRE(read_payload.size() == payload.size());
        CHECK(vector<uint8_t>(read_payload.begin(), read_payload.end()) == payload);

        chunk_data[0] = 0;
        CHECK_FALSE(TryReadContentUpdateChunkData(chunk_data, header, read_payload));

        vector<uint8_t> oversized_chunk_data(31);
        WritePodAt<uint32_t>(oversized_chunk_data, 0, ContentUpdateDatagramSignature);
        WritePodAt<uint16_t>(oversized_chunk_data, 4, ContentUpdateDatagramVersion);
        WritePodAt<uint8_t>(oversized_chunk_data, 6, static_cast<uint8_t>(ContentUpdateDatagramType::ChunkData));
        WritePodAt<uint32_t>(oversized_chunk_data, 7, 77);
        WritePodAt<uint32_t>(oversized_chunk_data, 11, 3);
        WritePodAt<uint32_t>(oversized_chunk_data, 15, 9);
        WritePodAt<uint32_t>(oversized_chunk_data, 19, ContentUpdateMaxChunkPayloadSize + 1);
        WritePodAt<uint64_t>(oversized_chunk_data, 23, 0xAABBCCDD00112233ULL);

        CHECK_FALSE(TryReadContentUpdateChunkData(oversized_chunk_data, header, read_payload));
    }

    SECTION("ChunkMath")
    {
        CHECK(GetContentUpdateChunkCount(0, 1024) == 0);
        CHECK(GetContentUpdateChunkCount(2048, 1024) == 2);
        CHECK(GetContentUpdateChunkCount(2049, 1024) == 3);
        CHECK(GetContentUpdateChunkCount(numeric_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 2, 2) == std::numeric_limits<uint32_t>::max());
        CHECK(GetContentUpdateChunkCount(numeric_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1, 1) == 0);
        CHECK(GetContentUpdateChunkCount(std::numeric_limits<uint64_t>::max(), 1) == 0);
        CHECK(GetContentUpdateChunkOffset(1024, 2) == 2048);
        CHECK(GetContentUpdateChunkSize(2049, 1024, 0) == 1024);
        CHECK(GetContentUpdateChunkSize(2049, 1024, 1) == 1024);
        CHECK(GetContentUpdateChunkSize(2049, 1024, 2) == 1);
        CHECK(GetContentUpdateChunkSize(2049, 1024, 3) == 0);
    }

    SECTION("FastClientRejectsInconsistentChunkTable")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_bad_chunks");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port);

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 1024;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "127.0.0.1",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = 2049;
        file_info.Hash = 123;
        file_info.ChunkHashes = {11, 22};

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        CHECK(fast_client.IsFailed());
    }

    SECTION("FastClientRejectsOversizedChunkSize")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_oversized_chunk_client");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port);

        const vector<uint8_t> payload = MakePayload(1);

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = ContentUpdateMaxChunkPayloadSize + 1;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "127.0.0.1",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload);
        file_info.ChunkHashes = {fs_hash_data(payload)};

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        CHECK(fast_client.IsFailed());
        CHECK(fast_client.GetError() == "Fast updater manifest chunk size is too large");
    }

    SECTION("FastClientUsesInitialAttemptPlusRetries")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_retry_count");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, true, 1);
        const vector<uint8_t> payload = MakePayload(257);

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 257;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "not-an-ip",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload);
        file_info.ChunkHashes = {fs_hash_data(payload)};

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        REQUIRE_FALSE(fast_client.IsFailed());

        fast_client.Process();
        CHECK_FALSE(fast_client.IsFailed());

        fast_client.Process();
        CHECK(fast_client.IsFailed());
        CHECK(fast_client.GetError() == "Fast updater send failed");
    }

    SECTION("FastClientSaturatesHugeRetryBudget")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_retry_saturation");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, true, std::numeric_limits<int32_t>::max());
        const vector<uint8_t> payload = MakePayload(257);

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 257;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "not-an-ip",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload);
        file_info.ChunkHashes = {fs_hash_data(payload)};

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        REQUIRE_FALSE(fast_client.IsFailed());

        fast_client.Process();
        CHECK_FALSE(fast_client.IsFailed());
    }

    SECTION("FastClientDropsCorruptedExistingChunk")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_corrupted_chunk");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port);
        const vector<uint8_t> payload = MakePayload(514);

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 257;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "127.0.0.1",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload);
        file_info.ChunkHashes = {
            fs_hash_data({payload.data(), 257}),
            fs_hash_data({payload.data() + 257, 257}),
        };

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        const string chunk_path = strex("{}.__fastupd.{}", client_file_path, 0).str();
        const vector<uint8_t> corrupted_chunk = {1, 2, 3};

        REQUIRE(fs_write_file(chunk_path, corrupted_chunk));
        REQUIRE(fs_exists(chunk_path));

        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        REQUIRE_FALSE(fast_client.IsFailed());
        CHECK(fast_client.GetContiguousChunkCount() == 0);
        CHECK(fast_client.GetVerifiedBytes() == 0);
        CHECK_FALSE(fs_exists(chunk_path));
    }

    SECTION("FastClientRechecksChunksBeforeAssembly")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_recheck_chunk");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port);
        const vector<uint8_t> payload = MakePayload(514);
        const vector<uint8_t> first_chunk(payload.begin(), payload.begin() + 257);
        const vector<uint8_t> second_chunk(payload.begin() + 257, payload.end());

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 257;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "127.0.0.1",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload);
        file_info.ChunkHashes = {
            fs_hash_data(first_chunk),
            fs_hash_data(second_chunk),
        };

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        const string first_chunk_path = strex("{}.__fastupd.{}", client_file_path, 0).str();
        const string second_chunk_path = strex("{}.__fastupd.{}", client_file_path, 1).str();

        REQUIRE(fs_write_file(first_chunk_path, first_chunk));
        REQUIRE(fs_write_file(second_chunk_path, second_chunk));

        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        REQUIRE_FALSE(fast_client.IsFailed());
        REQUIRE(fast_client.IsFinished());
        CHECK(fast_client.GetVerifiedBytes() == payload.size());

        const vector<uint8_t> corrupted_chunk(second_chunk.size(), 0xEE);
        REQUIRE(fs_write_file(second_chunk_path, corrupted_chunk));

        CHECK_FALSE(fast_client.AssembleFile(client_file_path));
    }

    SECTION("FastClientWritesOnlyRecheckedContiguousFallbackData")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_fallback_prefix");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port);
        const vector<uint8_t> payload = MakePayload(514);
        const vector<uint8_t> first_chunk(payload.begin(), payload.begin() + 257);
        const vector<uint8_t> second_chunk(payload.begin() + 257, payload.end());

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 257;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "127.0.0.1",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload);
        file_info.ChunkHashes = {
            fs_hash_data(first_chunk),
            fs_hash_data(second_chunk),
        };

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        const string first_chunk_path = strex("{}.__fastupd.{}", client_file_path, 0).str();
        const string second_chunk_path = strex("{}.__fastupd.{}", client_file_path, 1).str();

        REQUIRE(fs_write_file(first_chunk_path, first_chunk));
        REQUIRE(fs_write_file(second_chunk_path, second_chunk));

        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        REQUIRE_FALSE(fast_client.IsFailed());
        REQUIRE(fast_client.IsFinished());

        const vector<uint8_t> corrupted_chunk(second_chunk.size(), 0xEE);
        REQUIRE(fs_write_file(second_chunk_path, corrupted_chunk));

        std::ostringstream temp_file {std::ios::binary};
        uint64_t written_bytes = 0;

        REQUIRE(fast_client.WriteContiguousData(temp_file, written_bytes));

        const auto written_data = temp_file.str();
        const vector<uint8_t> written_payload(written_data.begin(), written_data.end());

        CHECK(written_bytes == first_chunk.size());
        CHECK(written_payload == first_chunk);
        CHECK_FALSE(fs_exists(first_chunk_path));
        CHECK_FALSE(fs_exists(second_chunk_path));
    }

    SECTION("FastClientChecksAssembledFileHash")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_assembled_hash");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        const auto settings = MakeUpdaterSettings(temp_dir, port);
        const vector<uint8_t> payload = MakePayload(514);
        const vector<uint8_t> first_chunk(payload.begin(), payload.begin() + 257);
        const vector<uint8_t> second_chunk(payload.begin() + 257, payload.end());

        ContentUpdateManifest manifest;
        manifest.FastUpdateEnabled = true;
        manifest.SessionId = 1;
        manifest.ChunkSize = 257;
        manifest.Endpoints.emplace_back(ContentUpdateEndpoint {
            .Host = "127.0.0.1",
            .Port = port,
            .Priority = 1,
        });

        ContentUpdateFileInfo file_info;
        file_info.FileIndex = 1;
        file_info.Name = "FastPack.zip";
        file_info.Size = payload.size();
        file_info.Hash = fs_hash_data(payload) ^ 1ULL;
        file_info.ChunkHashes = {
            fs_hash_data(first_chunk),
            fs_hash_data(second_chunk),
        };

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        const string first_chunk_path = strex("{}.__fastupd.{}", client_file_path, 0).str();
        const string second_chunk_path = strex("{}.__fastupd.{}", client_file_path, 1).str();

        REQUIRE(fs_write_file(first_chunk_path, first_chunk));
        REQUIRE(fs_write_file(second_chunk_path, second_chunk));

        UpdaterFastClient fast_client {settings, manifest, file_info, client_file_path};

        REQUIRE_FALSE(fast_client.IsFailed());
        REQUIRE(fast_client.IsFinished());

        CHECK_FALSE(fast_client.AssembleFile(client_file_path));

        std::ostringstream temp_file {std::ios::binary};
        uint64_t written_bytes = 0;

        REQUIRE(fast_client.WriteContiguousData(temp_file, written_bytes));

        const auto written_data = temp_file.str();
        const vector<uint8_t> written_payload(written_data.begin(), written_data.end());

        CHECK(written_bytes == payload.size());
        CHECK(written_payload == payload);
        CHECK_FALSE(fs_exists(first_chunk_path));
        CHECK_FALSE(fs_exists(second_chunk_path));
    }

    SECTION("BackendStorageModesProduceMatchingFastUpdateData")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_storage_mode_parity");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto disk_settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, true, 4, 257, false);
        auto memory_settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, true, 4, 257, true);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(disk_settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend disk_backend;
        disk_backend.LoadFromClientResources(disk_settings);

        UpdaterBackend memory_backend;
        memory_backend.LoadFromClientResources(memory_settings);

        const auto disk_descriptor = disk_backend.GetUpdateDescriptor("Windows-win64", 0);
        const auto memory_descriptor = memory_backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(disk_descriptor);
        REQUIRE(memory_descriptor);
        const auto disk_manifest = VerifyTestDescriptor(*disk_descriptor, "Windows-win64");
        const auto memory_manifest = VerifyTestDescriptor(*memory_descriptor, "Windows-win64");

        REQUIRE(disk_manifest.Files.size() == 1);
        REQUIRE(memory_manifest.Files.size() == 1);

        const auto& disk_file = disk_manifest.Files[0];
        const auto& memory_file = memory_manifest.Files[0];

        CHECK(disk_file.Name == memory_file.Name);
        CHECK(disk_file.Size == memory_file.Size);
        CHECK(disk_file.Hash == memory_file.Hash);
        CHECK(disk_file.Target == memory_file.Target);
        CHECK(disk_file.ChunkHashes == memory_file.ChunkHashes);

        const uint32_t chunk_count = numeric_cast<uint32_t>(disk_file.ChunkHashes.size());
        const_span<uint8_t> payload_view = payload;

        for (uint32_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
            vector<uint8_t> disk_chunk;
            vector<uint8_t> memory_chunk;
            uint64_t disk_chunk_hash = 0;
            uint64_t memory_chunk_hash = 0;

            REQUIRE(disk_backend.ReadFastUpdateChunk(disk_file.FileIndex, chunk_index, disk_chunk, disk_chunk_hash));
            REQUIRE(memory_backend.ReadFastUpdateChunk(memory_file.FileIndex, chunk_index, memory_chunk, memory_chunk_hash));

            const size_t chunk_offset = numeric_cast<size_t>(GetContentUpdateChunkOffset(disk_manifest.ChunkSize, chunk_index));
            const size_t chunk_size = numeric_cast<size_t>(GetContentUpdateChunkSize(payload.size(), disk_manifest.ChunkSize, chunk_index));
            const auto expected_chunk = payload_view.subspan(chunk_offset, chunk_size);

            CHECK(disk_chunk == memory_chunk);
            CHECK(std::ranges::equal(disk_chunk, expected_chunk));
            CHECK(disk_chunk_hash == memory_chunk_hash);
            CHECK(disk_chunk_hash == fs_hash_data(expected_chunk));
        }
    }

    SECTION("BackendArtifactLeasesMatchMemoryAndDiskStorage")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_artifact_leases");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto disk_settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, false);
        auto memory_settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, true);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(disk_settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend disk_backend;
        disk_backend.LoadFromClientResources(disk_settings);
        UpdaterBackend memory_backend;
        memory_backend.LoadFromClientResources(memory_settings);

        const auto disk_catalog = disk_backend.GetContentUpdateCatalog();
        const auto memory_catalog = memory_backend.GetContentUpdateCatalog();
        REQUIRE(disk_catalog.size() == 1);
        REQUIRE(memory_catalog.size() == 1);
        REQUIRE(disk_catalog[0]->GetGeneration() != 0);
        REQUIRE(memory_catalog[0]->GetGeneration() != 0);
        CHECK(disk_catalog[0]->GetFileId() == memory_catalog[0]->GetFileId());
        CHECK(disk_catalog[0]->GetName() == memory_catalog[0]->GetName());
        CHECK(disk_catalog[0]->GetSize() == payload.size());
        CHECK(disk_catalog[0]->GetSha256Native() == ComputeSha256(payload));
        CHECK(disk_catalog[0]->GetSha256Native() == memory_catalog[0]->GetSha256Native());

        const auto disk_lease = disk_backend.AcquireContentUpdateArtifact(disk_catalog[0]->GetGeneration(), disk_catalog[0]->GetFileId(), disk_catalog[0]->GetSha256Native());
        const auto memory_lease = memory_backend.AcquireContentUpdateArtifact(memory_catalog[0]->GetGeneration(), memory_catalog[0]->GetFileId(), memory_catalog[0]->GetSha256Native());
        REQUIRE(disk_lease);
        REQUIRE(memory_lease);

        vector<uint8_t> disk_data(payload.size());
        vector<uint8_t> memory_data(payload.size());
        REQUIRE(disk_lease->Read(0, disk_data));
        REQUIRE(memory_lease->Read(0, memory_data));
        CHECK(disk_data == payload);
        CHECK(memory_data == payload);

        vector<uint8_t> tail(17);
        REQUIRE(disk_lease->Read(numeric_cast<uint64_t>(payload.size() - tail.size()), tail));
        CHECK(std::ranges::equal(tail, const_span<uint8_t> {payload}.last(tail.size())));
        CHECK_FALSE(disk_lease->Read(numeric_cast<uint64_t>(payload.size() + 1), span<uint8_t> {}));
        CHECK_FALSE(memory_lease->Read(numeric_cast<uint64_t>(payload.size()), span<uint8_t> {memory_data}.first(1)));

        Sha256Digest wrong_sha256 = disk_catalog[0]->GetSha256Native();
        wrong_sha256[0] ^= 0xFF;
        CHECK_FALSE(disk_backend.AcquireContentUpdateArtifact(disk_catalog[0]->GetGeneration(), disk_catalog[0]->GetFileId(), wrong_sha256));
        CHECK_FALSE(disk_backend.AcquireContentUpdateArtifact(disk_catalog[0]->GetGeneration() + 1, disk_catalog[0]->GetFileId(), disk_catalog[0]->GetSha256Native()));
    }

    SECTION("BackendDiskSnapshotScavengerRemovesOrphansAndPreservesActiveOwners")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_snapshot_scavenger");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });
        const vector<uint8_t> payload = MakePayload(2049);
        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, false);
        REQUIRE(fs_write_file(strex(settings.ClientResources).combine_path("FastPack.zip").str(), payload));
        REQUIRE(fs_write_file(strex(settings.PlatformBinaries).combine_path("Windows-win64/ClientRuntime.dll").str(), payload));

        const uint64_t orphan_id_base = numeric_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        vector<string> orphan_roots;

        for (uint32_t index = 0; index != 12; ++index) {
            const string orphan_root = MakeContentUpdateSnapshotRootPathForTest(orphan_id_base + index);
            const bool removed_orphan_before = fs_remove_dir_tree(orphan_root);
            ignore_unused(removed_orphan_before);
            REQUIRE(fs_create_directories(orphan_root));
            REQUIRE(fs_write_file(strex(orphan_root).combine_path("owner.lock").str(), "FONLINE_CONTENT_UPDATE_SNAPSHOT_V1\n999999\n"));
            REQUIRE(fs_write_file(strex(orphan_root).combine_path("content-0.bin").str(), "orphan"));
            orphan_roots.emplace_back(orphan_root);
        }

        const string unknown_root = MakeContentUpdateSnapshotRootPathForTest(orphan_id_base + 100);
        const bool removed_unknown_before = fs_remove_dir_tree(unknown_root);
        ignore_unused(removed_unknown_before);
        REQUIRE(fs_create_directories(unknown_root));
        REQUIRE(fs_write_file(strex(unknown_root).combine_path("keep.txt").str(), "unknown-owner"));

        const string unknown_contents_root = MakeContentUpdateSnapshotRootPathForTest(orphan_id_base + 101);
        const bool removed_unknown_contents_before = fs_remove_dir_tree(unknown_contents_root);
        ignore_unused(removed_unknown_contents_before);
        REQUIRE(fs_create_directories(unknown_contents_root));
        REQUIRE(fs_write_file(strex(unknown_contents_root).combine_path("owner.lock").str(), "FONLINE_CONTENT_UPDATE_SNAPSHOT_V1\n999999\n"));
        REQUIRE(fs_write_file(strex(unknown_contents_root).combine_path("keep.txt").str(), "unknown-content"));

        const auto snapshot_cleanup = scope_exit([&]() noexcept {
            safe_call([&] {
                for (const auto& orphan_root : orphan_roots) {
                    fs_remove_dir_tree(orphan_root);
                }
                fs_remove_dir_tree(unknown_root);
                fs_remove_dir_tree(unknown_contents_root);
            });
        });

        shared_ptr<ContentUpdateArtifactLease> retained_lease;
        string active_root;

        {
            UpdaterBackend backend;
            backend.LoadFromClientResources(settings);
            const auto catalog = backend.GetContentUpdateCatalog();
            REQUIRE(catalog.size() == 2);

            const uint64_t snapshot_id = MakeContentUpdateSnapshotIdForTest(backend, catalog[0]->GetGeneration());
            const auto active_roots = FindContentUpdateSnapshotRootsForTest(snapshot_id);
            REQUIRE(active_roots.size() == 1);
            active_root = active_roots.front();
            REQUIRE(fs_exists(active_root));
            CHECK(fs_exists(strex(active_root).combine_path("content-0.bin").str()));
            CHECK(fs_exists(strex(active_root).combine_path("content-1.bin").str()));

            const size_t remaining_after_bounded_pass = numeric_cast<size_t>(std::ranges::count_if(orphan_roots, [](const string& root) { return fs_exists(root); }));
            CHECK(remaining_after_bounded_pass >= 4);
            CHECK(remaining_after_bounded_pass < orphan_roots.size());

            retained_lease = backend.AcquireContentUpdateArtifact(catalog[0]->GetGeneration(), catalog[0]->GetFileId(), catalog[0]->GetSha256Native());
            REQUIRE(retained_lease);

            for (uint32_t pass = 0; pass != 3 && std::ranges::any_of(orphan_roots, [](const string& root) { return fs_exists(root); }); ++pass) {
                UpdaterBackend scavenger_backend;
                scavenger_backend.LoadFromClientResources(settings);
                CHECK(fs_exists(active_root));
                CHECK(fs_exists(unknown_root));
                CHECK(fs_exists(unknown_contents_root));
            }

            CHECK_FALSE(std::ranges::any_of(orphan_roots, [](const string& root) { return fs_exists(root); }));
            CHECK(fs_exists(active_root));
            CHECK(fs_exists(unknown_root));
            CHECK(fs_exists(unknown_contents_root));
        }

        REQUIRE(retained_lease);
        CHECK(fs_exists(active_root));
        vector<uint8_t> active_data(payload.size());
        REQUIRE(retained_lease->Read(0, active_data));
        CHECK(active_data == payload);
        retained_lease.reset();
        CHECK_FALSE(fs_exists(active_root));
        CHECK(fs_exists(unknown_root));
        CHECK(fs_exists(unknown_contents_root));
    }

    SECTION("BackendDiskCatalogKeepsSnapshotAcrossSourceMutationAndReplacement")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_disk_snapshot");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, true, 4, 257, false);
        const vector<uint8_t> payload = MakePayload(4097);
        vector<uint8_t> replacement = MakePayload(payload.size());
        std::ranges::reverse(replacement);
        REQUIRE(replacement != payload);
        vector<uint8_t> atomic_replacement = replacement;
        std::ranges::rotate(atomic_replacement, atomic_replacement.begin() + 1);
        REQUIRE(atomic_replacement != payload);
        REQUIRE(atomic_replacement != replacement);

        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();
        const string replacement_path = strex(settings.ClientResources).combine_path("FastPack.replacement").str();
        const string retired_path = strex(settings.ClientResources).combine_path("FastPack.retired").str();
        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        auto catalog = backend.GetContentUpdateCatalog();
        REQUIRE(catalog.size() == 1);
        const auto descriptor_before = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor_before);
        const auto manifest_before = VerifyTestDescriptor(*descriptor_before, "Windows-win64");
        REQUIRE(manifest_before.Files.size() == 1);

        REQUIRE(fs_write_file(pack_path, replacement));
        REQUIRE(fs_write_file(replacement_path, atomic_replacement));
        REQUIRE(fs_rename(pack_path, retired_path));
        REQUIRE(fs_rename(replacement_path, pack_path));

        const auto lease = backend.AcquireContentUpdateArtifact(catalog[0]->GetGeneration(), catalog[0]->GetFileId(), catalog[0]->GetSha256Native());
        REQUIRE(lease);
        vector<uint8_t> lease_data(payload.size());
        REQUIRE(lease->Read(0, lease_data));
        CHECK(lease_data == payload);

        vector<uint8_t> first_chunk;
        uint64_t first_chunk_hash = 0;
        REQUIRE(backend.ReadFastUpdateChunk(catalog[0]->GetFileId(), 0, first_chunk, first_chunk_hash));
        const auto expected_first_chunk = const_span<uint8_t> {payload}.first(first_chunk.size());
        CHECK(std::ranges::equal(first_chunk, expected_first_chunk));
        CHECK(first_chunk_hash == fs_hash_data(expected_first_chunk));
        CHECK(manifest_before.Files[0].Hash == fs_hash_data(payload));
        CHECK(manifest_before.Files[0].Sha256 == ComputeSha256(payload));

        const auto visible_path_data = fs_read_file(pack_path);
        REQUIRE(visible_path_data);
        const vector<uint8_t> visible_path_bytes(visible_path_data->begin(), visible_path_data->end());
        CHECK(visible_path_bytes == atomic_replacement);
    }

    SECTION("BackendExternalSourcesUseGenerationBoundImmutableSnapshots")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_external_sources");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, true);
        const vector<uint8_t> payload = MakePayload(1025);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto catalog = backend.GetContentUpdateCatalog();
        REQUIRE(catalog.size() == 1);
        const uint64_t generation = catalog[0]->GetGeneration();
        const uint32_t file_id = catalog[0]->GetFileId();
        const Sha256Digest sha256 = catalog[0]->GetSha256Native();
        const auto original_descriptor = backend.GetUpdateDescriptor("Windows-win64", 1000);
        REQUIRE(original_descriptor);
        REQUIRE(VerifyTestDescriptor(*original_descriptor, "Windows-win64").Files[0].Sources.empty());

        ContentUpdateSource primary_source {
            .Provider = "mirror-a",
            .SourceKey = "primary",
            .Transport = "https",
            .Locator = "https://mirror.invalid/first",
            .Priority = 100,
            .ExpiresAt = 2000,
        };

        Sha256Digest wrong_sha256 = sha256;
        wrong_sha256[0] ^= 0xFF;
        CHECK_FALSE(backend.UpsertContentUpdateSource(generation + 1, file_id, sha256, primary_source));
        CHECK_FALSE(backend.UpsertContentUpdateSource(generation, file_id, wrong_sha256, primary_source));
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, primary_source));

        const auto first_descriptor = backend.GetUpdateDescriptor("Windows-win64", 1000);
        REQUIRE(first_descriptor);
        const auto first_manifest = VerifyTestDescriptor(*first_descriptor, "Windows-win64");
        REQUIRE(first_manifest.Files[0].Sources.size() == 1);
        CHECK(first_manifest.Files[0].Sources[0].Locator == primary_source.Locator);
        CHECK(VerifyTestDescriptor(*original_descriptor, "Windows-win64").Files[0].Sources.empty());

        primary_source.Locator = "https://mirror.invalid/refreshed";
        primary_source.ExpiresAt = 3000;
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, primary_source));

        ContentUpdateSource secondary_source {
            .Provider = "mirror-b",
            .SourceKey = "secondary",
            .Transport = "https",
            .Locator = "https://mirror.invalid/secondary",
            .Priority = 50,
        };
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, secondary_source));

        const auto refreshed_descriptor = backend.GetUpdateDescriptor("Windows-win64", 1000);
        REQUIRE(refreshed_descriptor);
        const auto refreshed_manifest = VerifyTestDescriptor(*refreshed_descriptor, "Windows-win64");
        REQUIRE(refreshed_manifest.Files[0].Sources.size() == 2);
        CHECK(refreshed_manifest.Files[0].Sources[0].Locator == primary_source.Locator);
        CHECK_FALSE(backend.RemoveContentUpdateSource(generation, file_id, wrong_sha256, primary_source.Provider, primary_source.SourceKey));
        REQUIRE(backend.RemoveContentUpdateSource(generation, file_id, sha256, primary_source.Provider, primary_source.SourceKey));

        const auto removed_descriptor = backend.GetUpdateDescriptor("Windows-win64", 1000);
        REQUIRE(removed_descriptor);
        const auto removed_manifest = VerifyTestDescriptor(*removed_descriptor, "Windows-win64");
        REQUIRE(removed_manifest.Files[0].Sources.size() == 1);
        CHECK(removed_manifest.Files[0].Sources[0].Provider == secondary_source.Provider);
        REQUIRE(backend.ClearContentUpdateSources(generation, secondary_source.Provider));
        REQUIRE(VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 1000), "Windows-win64").Files[0].Sources.empty());

        primary_source.ExpiresAt = 1500;
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, primary_source));
        REQUIRE(VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 1499), "Windows-win64").Files[0].Sources.size() == 1);
        CHECK(VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 1500), "Windows-win64").Files[0].Sources.empty());

        backend.LoadFromClientResources(settings);
        CHECK(backend.GetContentUpdateCatalogGeneration() > generation);
        CHECK_FALSE(backend.UpsertContentUpdateSource(generation, file_id, sha256, primary_source));
    }

    SECTION("BackendClientFeedbackIsConnectionBoundAdvisoryAndNeverSuppressesSources")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_source_feedback");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, true);
        const string resources_dir = strex(temp_dir).combine_path("server_resources").str();
        REQUIRE(fs_create_directories(resources_dir));
        REQUIRE(fs_write_file(strex(resources_dir).combine_path("FastPack.zip").str(), MakePayload(1024)));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);
        const auto catalog = backend.GetContentUpdateCatalog();
        REQUIRE(catalog.size() == 1);
        const uint64_t generation = catalog[0]->GetGeneration();
        const uint32_t file_id = catalog[0]->GetFileId();
        const Sha256Digest sha256 = catalog[0]->GetSha256Native();
        const uint64_t feedback_session = backend.BeginContentUpdateFeedbackSession();
        const uint64_t other_feedback_session = backend.BeginContentUpdateFeedbackSession();
        const auto cached_descriptor = backend.GetUpdateDescriptor("Windows-win64", 1000, feedback_session);
        const auto other_cached_descriptor = backend.GetUpdateDescriptor("Windows-win64", 1000, other_feedback_session);
        REQUIRE(cached_descriptor);
        REQUIRE(other_cached_descriptor);
        CHECK(cached_descriptor == other_cached_descriptor);
        CHECK(VerifyTestDescriptor(*cached_descriptor, "Windows-win64").Files[0].Sources.empty());
        CHECK(VerifyTestDescriptor(*other_cached_descriptor, "Windows-win64").Files[0].Sources.empty());
        ContentUpdateSource source {
            .Provider = "mirror-feedback",
            .SourceKey = "primary",
            .Transport = "https",
            .Locator = "https://mirror.invalid/content",
            .Priority = 100,
        };
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, source));
        ContentUpdateSource secondary_source {
            .Provider = "mirror-feedback",
            .SourceKey = "secondary",
            .Transport = "https",
            .Locator = "https://mirror.invalid/secondary-content",
            .Priority = 50,
        };
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, secondary_source));

        const auto initial_manifest = VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 1000, feedback_session), "Windows-win64");
        const auto other_session_manifest = VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 1000, other_feedback_session), "Windows-win64");
        CHECK(initial_manifest.CatalogGeneration == generation);
        REQUIRE(initial_manifest.Files[0].Sources.size() == 2);
        REQUIRE(other_session_manifest.Files[0].Sources.size() == 2);
        const ContentUpdateSourceReportToken report_token = initial_manifest.Files[0].Sources[0].ReportToken;
        REQUIRE_FALSE(IsContentUpdateSourceReportTokenEmpty(report_token));
        CHECK(other_session_manifest.Files[0].Sources[0].ReportToken != report_token);

        const ContentUpdateSourceFeedbackPolicy policy {
            .Enabled = true,
            .MinReports = 5,
            .FailurePercent = 60,
            .WindowMilliseconds = 10000,
        };

        ContentUpdateSourceReportToken invalid_token {};
        invalid_token.fill(UINT8_C(0xff));
        CHECK(backend.ReportContentUpdateSourceResult(generation + 1, file_id, report_token, feedback_session, ContentUpdateSourceResult::TransportFailure, 1, 1000, policy) == ContentUpdateSourceFeedbackDecision::Ignored);
        CHECK(backend.ReportContentUpdateSourceResult(generation, file_id, invalid_token, feedback_session, ContentUpdateSourceResult::TransportFailure, 1, 1000, policy) == ContentUpdateSourceFeedbackDecision::Ignored);
        CHECK(backend.ReportContentUpdateSourceResult(generation, file_id, report_token, other_feedback_session, ContentUpdateSourceResult::TransportFailure, 1, 1000, policy) == ContentUpdateSourceFeedbackDecision::Ignored);

        for (uint64_t reporter_id = 1; reporter_id <= 5; ++reporter_id) {
            CHECK(backend.ReportContentUpdateSourceResult(generation, file_id, report_token, feedback_session, ContentUpdateSourceResult::TransportFailure, reporter_id, 1000, policy) == ContentUpdateSourceFeedbackDecision::Recorded);
        }

        const auto advisory_manifest = VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 1000, feedback_session), "Windows-win64");
        REQUIRE(advisory_manifest.Files[0].Sources.size() == 2);
        CHECK(advisory_manifest.Files[0].Sources[0].ReportToken == report_token);

        source.Locator = "https://mirror.invalid/refreshed-content";
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, source));
        const auto refreshed_manifest = VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 7000, feedback_session), "Windows-win64");
        REQUIRE(refreshed_manifest.Files[0].Sources.size() == 2);
        CHECK(refreshed_manifest.Files[0].Sources[0].SourceKey == source.SourceKey);
        CHECK(refreshed_manifest.Files[0].Sources[0].ReportToken != report_token);
        CHECK(backend.ReportContentUpdateSourceResult(generation, file_id, report_token, feedback_session, ContentUpdateSourceResult::TransportFailure, 21, 7000, policy) == ContentUpdateSourceFeedbackDecision::Ignored);

        source.Locator = "https://mirror.invalid/expiring-content";
        source.ExpiresAt = 8000;
        REQUIRE(backend.UpsertContentUpdateSource(generation, file_id, sha256, source));
        const auto expiring_manifest = VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 7999, feedback_session), "Windows-win64");
        REQUIRE(expiring_manifest.Files[0].Sources.size() == 2);
        const ContentUpdateSourceReportToken expiring_report_token = expiring_manifest.Files[0].Sources[0].ReportToken;
        CHECK(backend.ReportContentUpdateSourceResult(generation, file_id, expiring_report_token, feedback_session, ContentUpdateSourceResult::TransportFailure, 22, 8000, policy) == ContentUpdateSourceFeedbackDecision::Ignored);
        CHECK(VerifyTestDescriptor(*backend.GetUpdateDescriptor("Windows-win64", 8000, feedback_session), "Windows-win64").Files[0].Sources.size() == 1);
    }

    SECTION("BackendDescriptorSnapshotsRemainConsistentDuringConcurrentPublication")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_concurrent_sources");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, true);
        const vector<uint8_t> payload = MakePayload(1025);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();
        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);
        const auto catalog = backend.GetContentUpdateCatalog();
        REQUIRE(catalog.size() == 1);
        const uint64_t generation = catalog[0]->GetGeneration();
        const uint32_t file_id = catalog[0]->GetFileId();
        const Sha256Digest sha256 = catalog[0]->GetSha256Native();
        std::atomic<bool> writer_finished {false};
        std::atomic<bool> snapshots_valid {true};
        std::atomic<uint32_t> readers_ready {};

        std::thread writer {[&] {
            while (readers_ready.load(std::memory_order_acquire) != 3) {
                std::this_thread::yield();
            }

            try {
                for (uint32_t revision = 1; revision <= 200; ++revision) {
                    ContentUpdateSource source {
                        .Provider = "mirror-a",
                        .SourceKey = "primary",
                        .Transport = "https",
                        .Locator = strex("https://mirror.invalid/{}", revision).str(),
                        .Priority = numeric_cast<int32_t>(revision),
                    };

                    if (!backend.UpsertContentUpdateSource(generation, file_id, sha256, std::move(source))) {
                        snapshots_valid.store(false, std::memory_order_relaxed);
                        break;
                    }
                }
            }
            catch (...) {
                snapshots_valid.store(false, std::memory_order_relaxed);
            }

            writer_finished.store(true, std::memory_order_release);
        }};

        vector<std::thread> readers;

        for (size_t reader_index = 0; reader_index < 3; ++reader_index) {
            readers.emplace_back([&] {
                readers_ready.fetch_add(1, std::memory_order_release);

                while (!writer_finished.load(std::memory_order_acquire)) {
                    try {
                        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);

                        if (!descriptor) {
                            snapshots_valid.store(false, std::memory_order_relaxed);
                            break;
                        }

                        const auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

                        if (manifest.Files.size() != 1 || manifest.Files[0].Sha256 != sha256 || manifest.Files[0].Sources.size() > 1) {
                            snapshots_valid.store(false, std::memory_order_relaxed);
                            break;
                        }

                        if (!manifest.Files[0].Sources.empty()) {
                            const auto& source = manifest.Files[0].Sources[0];

                            if (source.Provider != "mirror-a" || source.SourceKey != "primary" || source.Transport != "https") {
                                snapshots_valid.store(false, std::memory_order_relaxed);
                                break;
                            }
                        }
                    }
                    catch (...) {
                        snapshots_valid.store(false, std::memory_order_relaxed);
                        break;
                    }
                }
            });
        }

        writer.join();

        for (auto& reader : readers) {
            reader.join();
        }

        CHECK(snapshots_valid.load(std::memory_order_relaxed));
    }

    SECTION("BackendExternalSourcesStayWithinTheirBinaryTarget")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_source_target_isolation");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, false, std::nullopt, false, 0, 257, true);
        const vector<uint8_t> payload = MakePayload(1025);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();
        const string windows_dir = strex(settings.PlatformBinaries).combine_path("Windows-win64").str();
        const string linux_dir = strex(settings.PlatformBinaries).combine_path("Linux-x64").str();

        REQUIRE(fs_write_file(pack_path, payload));
        REQUIRE(fs_create_directories(windows_dir));
        REQUIRE(fs_create_directories(linux_dir));
        REQUIRE(fs_write_file(strex(windows_dir).combine_path("Runtime.dll").str(), MakePayload(511)));
        REQUIRE(fs_write_file(strex(linux_dir).combine_path("Runtime.so").str(), MakePayload(513)));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        auto catalog = backend.GetContentUpdateCatalog();
        REQUIRE(catalog.size() == 3);
        nptr<ContentUpdateArtifact> windows_artifact;

        for (auto& artifact : catalog) {
            if (artifact->GetBinaryTargets() == vector<string> {"Windows-win64"}) {
                windows_artifact = artifact;
                break;
            }
        }

        REQUIRE(windows_artifact);
        const auto original_windows_descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        const auto original_linux_descriptor = backend.GetUpdateDescriptor("Linux-x64", 0);
        const auto original_common_descriptor = backend.GetUpdateDescriptor("Unknown-target", 0);
        REQUIRE(original_windows_descriptor);
        REQUIRE(original_linux_descriptor);
        REQUIRE(original_common_descriptor);
        ContentUpdateSource source {
            .Provider = "mirror-a",
            .SourceKey = "windows-runtime",
            .Transport = "https",
            .Locator = "https://mirror.invalid/runtime",
            .Priority = 100,
        };
        REQUIRE(backend.UpsertContentUpdateSource(windows_artifact->GetGeneration(), windows_artifact->GetFileId(), windows_artifact->GetSha256Native(), source));

        const auto windows_descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        const auto linux_descriptor = backend.GetUpdateDescriptor("Linux-x64", 0);
        const auto common_descriptor = backend.GetUpdateDescriptor("Unknown-target", 0);
        REQUIRE(windows_descriptor);
        REQUIRE(linux_descriptor);
        REQUIRE(common_descriptor);
        CHECK(*windows_descriptor != *original_windows_descriptor);
        CHECK(*linux_descriptor == *original_linux_descriptor);
        CHECK(*common_descriptor == *original_common_descriptor);
        const auto windows_manifest = VerifyTestDescriptor(*windows_descriptor, "Windows-win64");
        const auto linux_manifest = VerifyTestDescriptor(*linux_descriptor, "Linux-x64");
        const auto common_manifest = VerifyTestDescriptor(*common_descriptor, "Unknown-target");
        const auto windows_file = std::ranges::find_if(windows_manifest.Files, [windows_artifact](const ContentUpdateFileInfo& file) { return file.FileIndex == windows_artifact->GetFileId(); });

        REQUIRE(windows_file != windows_manifest.Files.end());
        REQUIRE(windows_file->Sources.size() == 1);
        CHECK(std::ranges::none_of(linux_manifest.Files, [windows_artifact](const ContentUpdateFileInfo& file) { return file.FileIndex == windows_artifact->GetFileId(); }));
        CHECK(std::ranges::none_of(common_manifest.Files, [windows_artifact](const ContentUpdateFileInfo& file) { return file.FileIndex == windows_artifact->GetFileId(); }));
    }

    SECTION("BackendOmitsChunkHashesWhenFastUpdateDisabled")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_disabled_fast");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, false);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        const auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

        CHECK_FALSE(backend.IsFastUpdateEnabled());
        CHECK_FALSE(manifest.FastUpdateEnabled);
        CHECK_FALSE(manifest.SelfHostedServerEnabled);
        CHECK(manifest.Endpoints.empty());
        REQUIRE(manifest.Files.size() == 1);
        CHECK(manifest.Files[0].ChunkHashes.empty());
    }

    SECTION("BackendDisablesFastUpdateWhenChunkSizeUnsupported")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_oversized_chunk_backend");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, true, 4, numeric_cast<int32_t>(ContentUpdateMaxChunkPayloadSize) + 1);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        const auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

        CHECK_FALSE(backend.IsFastUpdateEnabled());
        CHECK_FALSE(manifest.FastUpdateEnabled);
        CHECK_FALSE(manifest.SelfHostedServerEnabled);
        CHECK(manifest.Endpoints.empty());
        REQUIRE(manifest.Files.size() == 1);
        CHECK(manifest.Files[0].ChunkHashes.empty());
    }

    SECTION("BackendDoesNotAdvertiseSelfHostedWithoutBindPort")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_self_host_disabled");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t advertised_port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, 0, true, advertised_port);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        const auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

        CHECK(backend.IsFastUpdateEnabled());
        CHECK(manifest.FastUpdateEnabled);
        CHECK_FALSE(manifest.SelfHostedServerEnabled);
        REQUIRE(manifest.Endpoints.size() == 1);
        CHECK(manifest.Endpoints[0].Port == advertised_port);
        REQUIRE(manifest.Files.size() == 1);
        CHECK_FALSE(manifest.Files[0].ChunkHashes.empty());
    }

    SECTION("BackendAndFastServerRejectOutOfRangeBindPort")
    {
        const string temp_dir = MakeTempContentUpdaterDir("content_updater_invalid_bind_port");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t advertised_port = AcquireContentUpdaterTestPort();
        const int32_t invalid_bind_port = numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max()) + 1;
        auto settings = MakeUpdaterSettings(temp_dir, invalid_bind_port, true, advertised_port);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        const auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

        REQUIRE(backend.IsFastUpdateEnabled());
        REQUIRE(manifest.FastUpdateEnabled);
        CHECK_FALSE(manifest.SelfHostedServerEnabled);
        REQUIRE(manifest.Endpoints.size() == 1);
        CHECK(manifest.Endpoints[0].Port == advertised_port);

        UpdaterFastServer fast_server {settings, backend};
        CHECK_FALSE(fast_server.Start());
        CHECK_FALSE(fast_server.IsStarted());
    }

    SECTION("FastServerDoesNotStartWithoutBackendFastData")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_no_fast_backend");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port, true, std::nullopt, false);

        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        CHECK_FALSE(backend.IsFastUpdateEnabled());

        vector<uint8_t> chunk_data;
        uint64_t chunk_hash = 0;
        CHECK_FALSE(backend.ReadFastUpdateChunk(0, 0, chunk_data, chunk_hash));

        UpdaterFastServer fast_server {settings, backend};
        CHECK_FALSE(fast_server.Start());
        CHECK_FALSE(fast_server.IsStarted());
    }

    SECTION("FastClientDownloadsFromLoopbackServer")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_fast_loopback");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port);
        const vector<uint8_t> payload = MakePayload(4097);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        const auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

        REQUIRE(backend.IsFastUpdateEnabled());
        REQUIRE(manifest.FastUpdateEnabled);
        REQUIRE(manifest.SelfHostedServerEnabled);
        REQUIRE(manifest.ChunkSize == numeric_cast<uint32_t>(settings.FastUpdateChunkSize));
        REQUIRE(manifest.Endpoints.size() == 1);
        REQUIRE(manifest.Files.size() == 1);
        CHECK(manifest.Files[0].Size == payload.size());
        CHECK(manifest.Files[0].Hash == fs_hash_data(payload));
        REQUIRE(manifest.Files[0].ChunkHashes.size() == GetContentUpdateChunkCount(payload.size(), manifest.ChunkSize));

        UpdaterFastServer fast_server {settings, backend};
        REQUIRE(fast_server.Start());
        const auto stop_server = scope_exit([&fast_server]() noexcept { fast_server.Stop(); });

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        UpdaterFastClient fast_client {settings, manifest, manifest.Files[0], client_file_path};
        REQUIRE_FALSE(fast_client.IsFailed());

        // One advertised endpoint must still use the configured parallel socket count. Complete
        // the same-size anti-amplification cookie round trip for the initial socket window, then
        // consume authenticated chunk replies without polling the server again. Receiving at least
        // two chunks proves both authenticated requests were already in flight.
        fast_client.Process();

        for (int32_t poll_index = 0; poll_index < 20; ++poll_index) {
            fast_server.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        fast_client.Process();

        for (int32_t poll_index = 0; poll_index < 20; ++poll_index) {
            fast_server.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        const uint64_t parallel_window_bytes = numeric_cast<uint64_t>(manifest.ChunkSize) * 2;
        const auto parallel_deadline = nanotime::now() + std::chrono::seconds {1};

        while (fast_client.GetVerifiedBytes() < parallel_window_bytes && nanotime::now() < parallel_deadline) {
            fast_client.Process();
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        CHECK(fast_client.GetVerifiedBytes() >= parallel_window_bytes);

        const auto deadline = nanotime::now() + std::chrono::seconds {5};

        while (!fast_client.IsFinished() && !fast_client.IsFailed() && nanotime::now() < deadline) {
            fast_client.Process();
            fast_server.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        REQUIRE_FALSE(fast_client.IsFailed());
        REQUIRE(fast_client.IsFinished());
        REQUIRE(fast_client.AssembleFile(client_file_path));

        CHECK(fs_compare_file_content(client_file_path, payload));
    }

    SECTION("FastServerRequiresAddressBoundCookieWithoutAmplification")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_fast_cookie");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, port);
        const vector<uint8_t> expected_payload = MakePayload(257);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();
        REQUIRE(fs_write_file(pack_path, expected_payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);
        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        const ContentUpdateManifest manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");
        REQUIRE(manifest.Files.size() == 1);

        UpdaterFastServer fast_server {settings, backend};
        REQUIRE(fast_server.Start());
        const auto stop_server = scope_exit([&fast_server]() noexcept { fast_server.Stop(); });

        udp_socket original_socket;
        udp_socket replay_socket;
        REQUIRE(original_socket.bind("127.0.0.1", 0, false));
        REQUIRE(replay_socket.bind("127.0.0.1", 0, false));

        ContentUpdateChunkRequest request {
            .SessionId = manifest.SessionId,
            .FileIndex = manifest.Files[0].FileIndex,
            .ChunkIndex = 0,
            .ClientNonce = UINT64_C(0x1020304050607080),
        };
        vector<uint8_t> request_data = MakeContentUpdateChunkRequestData(request);
        REQUIRE(original_socket.send_to("127.0.0.1", port, request_data) == numeric_cast<int32_t>(request_data.size()));

        vector<uint8_t> receive_buffer(ContentUpdateMaxChunkPayloadSize + 128);
        const auto receive_response = [&fast_server, &receive_buffer](udp_socket& socket) -> int32_t {
            for (int32_t attempt = 0; attempt != 100; ++attempt) {
                fast_server.Poll();

                if (socket.can_read()) {
                    string host;
                    uint16_t response_port = 0;
                    return socket.receive_from(receive_buffer, host, response_port);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }

            return -1;
        };

        const int32_t challenge_size = receive_response(original_socket);
        REQUIRE(challenge_size > 0);
        CHECK(challenge_size <= numeric_cast<int32_t>(request_data.size()));

        ContentUpdateCookieChallenge challenge;
        REQUIRE(TryReadContentUpdateCookieChallengeData({receive_buffer.data(), numeric_cast<size_t>(challenge_size)}, challenge));
        request.CookieExpiresAt = challenge.CookieExpiresAt;
        request.Cookie = challenge.Cookie;
        request_data = MakeContentUpdateChunkRequestData(request);

        REQUIRE(replay_socket.send_to("127.0.0.1", port, request_data) == numeric_cast<int32_t>(request_data.size()));
        const int32_t replay_response_size = receive_response(replay_socket);
        REQUIRE(replay_response_size > 0);
        ContentUpdateCookieChallenge replay_challenge;
        CHECK(TryReadContentUpdateCookieChallengeData({receive_buffer.data(), numeric_cast<size_t>(replay_response_size)}, replay_challenge));

        REQUIRE(original_socket.send_to("127.0.0.1", port, request_data) == numeric_cast<int32_t>(request_data.size()));
        const int32_t chunk_response_size = receive_response(original_socket);
        REQUIRE(chunk_response_size > 0);

        ContentUpdateChunkDataHeader chunk_header;
        const_span<uint8_t> chunk_payload {};
        REQUIRE(TryReadContentUpdateChunkData({receive_buffer.data(), numeric_cast<size_t>(chunk_response_size)}, chunk_header, chunk_payload));
        CHECK(chunk_header.SessionId == manifest.SessionId);
        CHECK(chunk_header.FileIndex == manifest.Files[0].FileIndex);
        CHECK(chunk_header.ChunkIndex == 0);
        CHECK(vector<uint8_t>(chunk_payload.begin(), chunk_payload.end()) == expected_payload);
    }

    SECTION("FastClientFailsOverToNextEndpoint")
    {
        REQUIRE(net_sockets::startup());

        const string temp_dir = MakeTempContentUpdaterDir("content_updater_endpoint_failover");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);
        const auto cleanup = scope_exit([&temp_dir]() noexcept { safe_call([&] { fs_remove_dir_tree(temp_dir); }); });

        const uint16_t dead_port = AcquireContentUpdaterTestPort();
        const uint16_t live_port = AcquireContentUpdaterTestPort();
        auto settings = MakeUpdaterSettings(temp_dir, live_port, true, std::nullopt, true, 0);
        const vector<uint8_t> payload = MakePayload(514);
        const string pack_path = strex(settings.ClientResources).combine_path("FastPack.zip").str();

        REQUIRE(fs_write_file(pack_path, payload));

        UpdaterBackend backend;
        backend.LoadFromClientResources(settings);

        const auto descriptor = backend.GetUpdateDescriptor("Windows-win64", 0);
        REQUIRE(descriptor);
        auto manifest = VerifyTestDescriptor(*descriptor, "Windows-win64");

        REQUIRE(backend.IsFastUpdateEnabled());
        REQUIRE(manifest.FastUpdateEnabled);
        REQUIRE(manifest.Endpoints.size() == 1);
        manifest.Endpoints.insert(manifest.Endpoints.begin(),
            ContentUpdateEndpoint {
                .Host = "127.0.0.1",
                .Port = dead_port,
                .Priority = manifest.Endpoints[0].Priority + 1,
            });

        UpdaterFastServer fast_server {settings, backend};
        REQUIRE(fast_server.Start());
        const auto stop_server = scope_exit([&fast_server]() noexcept { fast_server.Stop(); });

        const string client_file_path = strex(temp_dir).combine_path("client_resources/FastPack.zip").str();
        UpdaterFastClient fast_client {settings, manifest, manifest.Files[0], client_file_path};
        REQUIRE_FALSE(fast_client.IsFailed());

        const auto deadline = nanotime::now() + std::chrono::seconds {5};

        while (!fast_client.IsFinished() && !fast_client.IsFailed() && nanotime::now() < deadline) {
            fast_client.Process();
            fast_server.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        REQUIRE_FALSE(fast_client.IsFailed());
        REQUIRE(fast_client.IsFinished());
        REQUIRE(fast_client.AssembleFile(client_file_path));

        CHECK(fs_compare_file_content(client_file_path, payload));
    }
}

FO_END_NAMESPACE
