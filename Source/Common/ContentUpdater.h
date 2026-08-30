//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ContentUpdaterException);

constexpr uint32_t ContentUpdateManifestSignature = 0x44505546; // FUPD
constexpr uint16_t ContentUpdateManifestVersion = 4;
constexpr uint32_t ContentUpdateSignedDescriptorSignature = 0x47535546; // FUSG
constexpr uint16_t ContentUpdateSignedDescriptorVersion = 1;
constexpr size_t ContentUpdateEd25519PublicKeySize = 32;
constexpr size_t ContentUpdateEd25519PrivateKeySize = 32;
constexpr size_t ContentUpdateEd25519SignatureSize = 64;
constexpr size_t ContentUpdateMaxBinaryTargetLength = 64;
constexpr uint32_t ContentUpdateDatagramSignature = 0x44505555; // UUPD
constexpr uint16_t ContentUpdateDatagramVersion = 2;
constexpr uint32_t ContentUpdateMaxChunkPayloadSize = 60 * 1024;
constexpr size_t ContentUpdateFastCookieSize = 16;
constexpr size_t ContentUpdateSourceReportTokenSize = 16;
constexpr size_t ContentUpdateMaxConsumedSourceReportTokens = 256;
constexpr size_t ContentUpdateMaxManifestSize = 64 * 1024 * 1024;
constexpr size_t ContentUpdateMaxDescriptorSize = ContentUpdateMaxManifestSize + 256;
constexpr uint32_t ContentUpdateMaxEndpointCount = 64;
constexpr size_t ContentUpdateMaxEndpointHostLength = 255;
constexpr uint32_t ContentUpdateMaxFileCount = 64 * 1024;
constexpr size_t ContentUpdateMaxFileNameLength = 1024;
constexpr uint32_t ContentUpdateMaxChunkHashCount = 4 * 1024 * 1024;
constexpr uint32_t ContentUpdateMaxSourcesPerFile = 16;
constexpr size_t ContentUpdateMaxSourceIdentifierLength = 64;
constexpr size_t ContentUpdateMaxSourceLocatorLength = 8 * 1024;
constexpr size_t ContentUpdateMaxSourceBytesPerFile = 64 * 1024;
constexpr string_view ContentUpdateReservedProvider = "fonline";

enum class ContentUpdateDatagramType : uint8_t
{
    ChunkRequest = 1,
    ChunkData = 2,
    CookieChallenge = 3,
};

using ContentUpdateFastCookie = array<uint8_t, ContentUpdateFastCookieSize>;
using ContentUpdateSourceReportToken = array<uint8_t, ContentUpdateSourceReportTokenSize>;
using ContentUpdateEd25519PublicKey = array<uint8_t, ContentUpdateEd25519PublicKeySize>;
using ContentUpdateEd25519PrivateKey = array<uint8_t, ContentUpdateEd25519PrivateKeySize>;
using ContentUpdateEd25519Signature = array<uint8_t, ContentUpdateEd25519SignatureSize>;

struct ContentUpdateTrustedPublicKey
{
    uint32_t KeyId {};
    ContentUpdateEd25519PublicKey PublicKey {};
};

struct ContentUpdateSigningKey
{
    uint32_t KeyId {};
    ContentUpdateEd25519PublicKey PublicKey {};
    ContentUpdateEd25519PrivateKey PrivateKey {};
};

struct ContentUpdateEndpoint
{
    string Host;
    uint16_t Port {};
    int32_t Priority {};
};

enum class ContentUpdateSourceResult : uint8_t
{
    Success = 1,
    TransportFailure = 2,
    IntegrityFailure = 3,
};

struct ContentUpdateSource
{
    string Provider;
    string SourceKey;
    string Transport;
    string Locator;
    int32_t Priority {};
    int64_t ExpiresAt {}; // Game.SynchronizedTime milliseconds; zero means no declared expiry
    ContentUpdateSourceReportToken ReportToken {}; // One-shot connection-bound authenticated client-health ticket
};

struct ContentUpdateFileInfo
{
    uint32_t FileIndex {};
    string Name;
    uint64_t Size {};
    uint64_t Hash {};
    Sha256Digest Sha256 {};
    UpdateFileTarget Target {UpdateFileTarget::ClientResources};
    vector<uint64_t> ChunkHashes {};
    vector<ContentUpdateSource> Sources {};
};

struct ContentUpdateManifest
{
    uint64_t CatalogGeneration {};
    bool FastUpdateEnabled {};
    bool SelfHostedServerEnabled {};
    uint32_t SessionId {};
    uint32_t ChunkSize {};
    vector<ContentUpdateEndpoint> Endpoints {};
    vector<ContentUpdateFileInfo> Files {};
};

struct VerifiedContentUpdateManifest
{
    ContentUpdateManifest Manifest {};
    uint64_t ReleaseSequence {};
    uint32_t KeyId {};
    string BinaryTarget;
};

class ContentUpdateSourceReportReplayGuard final
{
public:
    [[nodiscard]] auto GetSessionId() const noexcept -> uint64_t { return _sessionId; }
    void Begin(uint64_t session_id);
    [[nodiscard]] auto Consume(const ContentUpdateSourceReportToken& token) -> bool;

private:
    uint64_t _sessionId {};
    set<ContentUpdateSourceReportToken> _consumedTokens {};
};

struct ContentUpdateChunkRequest
{
    uint32_t SessionId {};
    uint32_t FileIndex {};
    uint32_t ChunkIndex {};
    uint64_t ClientNonce {};
    int64_t CookieExpiresAt {};
    ContentUpdateFastCookie Cookie {};
};

struct ContentUpdateCookieChallenge
{
    uint32_t SessionId {};
    uint32_t FileIndex {};
    uint32_t ChunkIndex {};
    uint64_t ClientNonce {};
    int64_t CookieExpiresAt {};
    ContentUpdateFastCookie Cookie {};
};

struct ContentUpdateChunkDataHeader
{
    uint32_t SessionId {};
    uint32_t FileIndex {};
    uint32_t ChunkIndex {};
    uint32_t ChunkSize {};
    uint64_t ChunkHash {};
};

[[nodiscard]] auto ParseContentUpdateEndpoint(string_view value) -> optional<ContentUpdateEndpoint>;
void ValidateContentUpdateSource(const ContentUpdateSource& source);
void CanonicalizeContentUpdateSources(vector<ContentUpdateSource>& sources);
void SerializeContentUpdateManifest(const ContentUpdateManifest& manifest, vector<uint8_t>& data);
[[nodiscard]] auto DeserializeContentUpdateManifest(const_span<uint8_t> data) -> ContentUpdateManifest;
[[nodiscard]] auto TryParseContentUpdateTrustedPublicKey(string_view value, ContentUpdateTrustedPublicKey& key) noexcept -> bool;
[[nodiscard]] auto TryParseContentUpdateSigningKey(string_view value, ContentUpdateSigningKey& key) noexcept -> bool;
[[nodiscard]] auto IsContentUpdateSourceReportTokenEmpty(const ContentUpdateSourceReportToken& token) noexcept -> bool;
[[nodiscard]] auto FillContentUpdateSecureRandom(span<uint8_t> data) noexcept -> bool;
void SignContentUpdateManifestDescriptor(const_span<uint8_t> manifest_data, string_view binary_target, uint64_t release_sequence, const ContentUpdateSigningKey& key, vector<uint8_t>& data);
[[nodiscard]] auto VerifyContentUpdateManifestDescriptor(const_span<uint8_t> data, string_view expected_binary_target, uint64_t minimum_release_sequence, const vector<ContentUpdateTrustedPublicKey>& trusted_keys) -> VerifiedContentUpdateManifest;
[[nodiscard]] auto VerifyContentUpdateStagedBinaryAuthorization(const_span<uint8_t> descriptor, string_view expected_binary_target, uint64_t minimum_release_sequence, const vector<ContentUpdateTrustedPublicKey>& trusted_keys, string_view local_runtime_file_name, string_view packaged_runtime_file_name, uint64_t staged_size, const Sha256Digest& staged_sha256) -> bool;
[[nodiscard]] auto AcceptContentUpdateReleaseSequence(string_view user_writable_path, uint64_t release_sequence, uint64_t minimum_release_sequence) -> bool;
[[nodiscard]] auto MakeContentUpdateChunkRequestData(const ContentUpdateChunkRequest& request) -> vector<uint8_t>;
[[nodiscard]] auto TryReadContentUpdateChunkRequestData(const_span<uint8_t> data, ContentUpdateChunkRequest& request) noexcept -> bool;
[[nodiscard]] auto MakeContentUpdateCookieChallengeData(const ContentUpdateCookieChallenge& challenge) -> vector<uint8_t>;
[[nodiscard]] auto TryReadContentUpdateCookieChallengeData(const_span<uint8_t> data, ContentUpdateCookieChallenge& challenge) noexcept -> bool;
void MakeContentUpdateChunkData(const ContentUpdateChunkDataHeader& header, const_span<uint8_t> payload, vector<uint8_t>& data);
[[nodiscard]] auto TryReadContentUpdateChunkData(const_span<uint8_t> data, ContentUpdateChunkDataHeader& header, const_span<uint8_t>& payload) noexcept -> bool;
[[nodiscard]] auto GetContentUpdateChunkCount(uint64_t file_size, uint32_t chunk_size) noexcept -> uint32_t;
[[nodiscard]] auto GetContentUpdateChunkSize(uint64_t file_size, uint32_t chunk_size, uint32_t chunk_index) noexcept -> uint32_t;
[[nodiscard]] auto GetContentUpdateChunkOffset(uint32_t chunk_size, uint32_t chunk_index) noexcept -> uint64_t;

FO_END_NAMESPACE
