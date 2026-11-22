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

#include "ContentUpdater.h"

#include "Platform.h"

#include <cerrno>
#include <openssl/curve25519.h>
#include <openssl/rand.h>

#if FO_WINDOWS
#include <fcntl.h>
#include <io.h>
#include <share.h>
#elif !FO_WEB
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

FO_BEGIN_NAMESPACE

static constexpr size_t MinManifestEndpointSize = sizeof(uint16_t) + 1 + sizeof(uint16_t) + sizeof(int32_t);
static constexpr size_t MinManifestFileSize = sizeof(uint32_t) + sizeof(uint16_t) + 1 + sizeof(uint64_t) + sizeof(uint64_t) + Sha256DigestSize + sizeof(UpdateFileTarget) + sizeof(uint64_t) * 7 + sizeof(uint32_t) * 2 + sizeof(uint16_t) * 2 + sizeof(uint32_t) + sizeof(uint32_t);
static constexpr size_t MinManifestChunkHashSize = sizeof(uint64_t);
static constexpr size_t MinManifestSourceSize = (sizeof(uint16_t) + 1) * 4 + sizeof(int32_t) + sizeof(int64_t) + ContentUpdateSourceReportTokenSize;

static void WriteDatagramHeader(data_writer& writer, ContentUpdateDatagramType type)
{
    FO_TRACE_ZONE(Core);

    writer.write<uint32_t>(ContentUpdateDatagramSignature);
    writer.write<uint16_t>(ContentUpdateDatagramVersion);
    writer.write<uint8_t>(static_cast<uint8_t>(type));
}

static auto ReadAndCheckDatagramHeader(data_reader& reader, ContentUpdateDatagramType expected_type) -> bool
{
    FO_TRACE_ZONE(Core);

    const uint32_t signature = reader.read<uint32_t>();
    const uint16_t version = reader.read<uint16_t>();
    const auto type = static_cast<ContentUpdateDatagramType>(reader.read<uint8_t>());
    return signature == ContentUpdateDatagramSignature && version == ContentUpdateDatagramVersion && type == expected_type;
}

static void CheckManifestStringLength(uint16_t length, size_t max_length, string_view field_name)
{
    FO_TRACE_ZONE(Core);

    if (length == 0 || length > max_length) {
        throw ContentUpdaterException("Invalid content update manifest string length", field_name);
    }
}

static void WriteManifestString(data_writer& writer, string_view value, size_t max_length, string_view field_name)
{
    FO_TRACE_ZONE(Core);

    if (value.empty() || value.length() > max_length || value.length() > std::numeric_limits<uint16_t>::max()) {
        throw ContentUpdaterException("Invalid content update manifest string length", field_name);
    }

    writer.write<uint16_t>(numeric_cast<uint16_t>(value.length()));
    writer.write_string_bytes(value);
}

static auto ReadManifestString(data_reader& reader, size_t max_length, string_view field_name) -> string
{
    FO_TRACE_ZONE(Core);

    const uint16_t length = reader.read<uint16_t>();
    CheckManifestStringLength(length, max_length, field_name);
    return string(reader.read_string_view(length));
}

static auto ReadManifestBool(data_reader& reader, string_view field_name) -> bool
{
    FO_TRACE_ZONE(Core);

    const uint8_t value = reader.read<uint8_t>();

    if (value > 1) {
        throw ContentUpdaterException("Invalid content update manifest bool", field_name);
    }

    return value != 0;
}

static void CheckManifestCount(uint32_t count, uint32_t max_count, size_t min_entry_size, string_view field_name, size_t remaining_size)
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(min_entry_size != 0, "Manifest count entry size must be positive");

    if (count > max_count || numeric_cast<uint64_t>(count) > numeric_cast<uint64_t>(remaining_size / min_entry_size)) {
        throw ContentUpdaterException("Invalid content update manifest count", field_name);
    }
}

static void CheckManifestCollectionSize(size_t size, uint32_t max_count, string_view field_name)
{
    FO_TRACE_ZONE(Core);

    if (size > max_count || size > std::numeric_limits<uint32_t>::max()) {
        throw ContentUpdaterException("Invalid content update manifest count", field_name);
    }
}

static void CheckManifestFileName(string_view file_name)
{
    FO_TRACE_ZONE(Core);

    if (file_name.empty() || file_name.size() > ContentUpdateMaxFileNameLength || file_name.front() == '/' || file_name.front() == '\\' || file_name.find('\\') != string_view::npos || file_name.find(':') != string_view::npos) {
        throw ContentUpdaterException("Invalid content update manifest file name", file_name);
    }

    size_t component_pos = 0;

    while (component_pos <= file_name.size()) {
        const size_t next_separator = file_name.find('/', component_pos);
        const string_view component = next_separator != string_view::npos ? file_name.substr(component_pos, next_separator - component_pos) : file_name.substr(component_pos);

        if (component.empty() || component == "." || component == "..") {
            throw ContentUpdaterException("Invalid content update manifest file name", file_name);
        }

        if (next_separator == string_view::npos) {
            break;
        }

        component_pos = next_separator + 1;
    }
}

static void CheckManifestEndpoint(const ContentUpdateEndpoint& endpoint)
{
    FO_TRACE_ZONE(Core);

    if (endpoint.Host.empty() || endpoint.Host.size() > ContentUpdateMaxEndpointHostLength || endpoint.Port == 0) {
        throw ContentUpdaterException("Invalid content update manifest endpoint", endpoint.Host);
    }
}

static void CheckManifestFileTarget(UpdateFileTarget target)
{
    FO_TRACE_ZONE(Core);

    if (target != UpdateFileTarget::ClientResources && target != UpdateFileTarget::ClientBinaries) {
        throw ContentUpdaterException("Invalid content update manifest file target");
    }
}

static auto IsContentUpdateIdentifierChar(char ch) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
}

static auto IsContentUpdateIdentifierStart(char ch) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

static auto IsEqualAsciiNoCase(char left, char right) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    const char normalized_left = left >= 'A' && left <= 'Z' ? numeric_cast<char>(left - 'A' + 'a') : left;
    const char normalized_right = right >= 'A' && right <= 'Z' ? numeric_cast<char>(right - 'A' + 'a') : right;
    return normalized_left == normalized_right;
}

static auto StartsWithAsciiNoCase(string_view value, string_view prefix) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    if (value.size() < prefix.size()) {
        return false;
    }

    for (size_t index = 0; index != prefix.size(); ++index) {
        if (!IsEqualAsciiNoCase(value[index], prefix[index])) {
            return false;
        }
    }

    return true;
}

static void CheckContentUpdateIdentifier(string_view value, string_view field_name)
{
    FO_TRACE_ZONE(Core);

    if (value.empty() || value.size() > ContentUpdateMaxSourceIdentifierLength || !IsContentUpdateIdentifierStart(value.front()) || !std::ranges::all_of(value, IsContentUpdateIdentifierChar)) {
        throw ContentUpdaterException("Invalid content update source identifier", field_name);
    }
}

static auto ContentUpdateSourceLess(const ContentUpdateSource& left, const ContentUpdateSource& right) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    if (left.Priority != right.Priority) {
        return left.Priority > right.Priority;
    }
    if (left.Provider != right.Provider) {
        return left.Provider < right.Provider;
    }
    if (left.SourceKey != right.SourceKey) {
        return left.SourceKey < right.SourceKey;
    }
    if (left.Transport != right.Transport) {
        return left.Transport < right.Transport;
    }
    if (left.Locator != right.Locator) {
        return left.Locator < right.Locator;
    }
    return left.ExpiresAt < right.ExpiresAt;
}

static void CheckManifestFileChunks(const ContentUpdateManifest& manifest, const ContentUpdateFileInfo& file)
{
    FO_TRACE_ZONE(Core);

    CheckManifestCollectionSize(file.ChunkHashes.size(), ContentUpdateMaxChunkHashCount, "chunk hashes");

    const uint32_t expected_count = manifest.FastUpdateEnabled ? GetContentUpdateChunkCount(file.Size, manifest.ChunkSize) : 0;

    if (numeric_cast<size_t>(expected_count) != file.ChunkHashes.size()) {
        throw ContentUpdaterException("Invalid content update manifest chunk hashes", file.Name);
    }
}

auto ParseContentUpdateEndpoint(string_view value) -> optional<ContentUpdateEndpoint>
{
    FO_TRACE_ZONE(Core);

    const auto parts = strex(value).split(':');

    if (parts.size() < 2 || parts.size() > 3) {
        return std::nullopt;
    }

    ContentUpdateEndpoint endpoint;
    endpoint.Host = parts[0];

    if (endpoint.Host.empty()) {
        return std::nullopt;
    }

    const int32_t port_value = strex(parts[1]).to_int32();

    if (port_value <= 0 || port_value > std::numeric_limits<uint16_t>::max()) {
        return std::nullopt;
    }

    endpoint.Port = numeric_cast<uint16_t>(port_value);
    endpoint.Priority = parts.size() == 3 ? strex(parts[2]).to_int32() : 0;
    return endpoint;
}

void ValidateContentUpdateSource(const ContentUpdateSource& source)
{
    FO_TRACE_ZONE(Core);

    CheckContentUpdateIdentifier(source.Provider, "provider");
    CheckContentUpdateIdentifier(source.SourceKey, "source key");
    CheckContentUpdateIdentifier(source.Transport, "transport");

    const bool reserved_provider = StartsWithAsciiNoCase(source.Provider, ContentUpdateReservedProvider) && (source.Provider.size() == ContentUpdateReservedProvider.size() || source.Provider[ContentUpdateReservedProvider.size()] == '.');

    if (reserved_provider) {
        throw ContentUpdaterException("Reserved content update source provider", source.Provider);
    }

    if (source.Locator.empty() || source.Locator.size() > ContentUpdateMaxSourceLocatorLength || std::ranges::any_of(source.Locator, [](char ch) noexcept { return (ch >= '\0' && ch < ' ') || ch == '\x7f'; })) {
        throw ContentUpdaterException("Invalid content update source locator", source.Provider, source.SourceKey);
    }

    if (source.ExpiresAt < 0) {
        throw ContentUpdaterException("Invalid content update source expiry", source.Provider, source.SourceKey);
    }
}

void CanonicalizeContentUpdateSources(vector<ContentUpdateSource>& sources)
{
    FO_TRACE_ZONE(Core);

    CheckManifestCollectionSize(sources.size(), ContentUpdateMaxSourcesPerFile, "sources");

    size_t total_bytes = 0;

    for (const auto& source : sources) {
        ValidateContentUpdateSource(source);

        const size_t source_bytes = MinManifestSourceSize + source.Provider.size() + source.SourceKey.size() + source.Transport.size() + source.Locator.size();

        if (source_bytes > ContentUpdateMaxSourceBytesPerFile || total_bytes > ContentUpdateMaxSourceBytesPerFile - source_bytes) {
            throw ContentUpdaterException("Content update sources exceed per-file size limit");
        }

        total_bytes += source_bytes;
    }

    for (size_t left_index = 0; left_index != sources.size(); ++left_index) {
        for (size_t right_index = left_index + 1; right_index != sources.size(); ++right_index) {
            if (sources[left_index].Provider == sources[right_index].Provider && sources[left_index].SourceKey == sources[right_index].SourceKey) {
                throw ContentUpdaterException("Duplicate content update source key", sources[left_index].Provider, sources[left_index].SourceKey);
            }
        }
    }

    std::sort(sources.begin(), sources.end(), ContentUpdateSourceLess);
}

void SerializeContentUpdateManifest(const ContentUpdateManifest& manifest, vector<uint8_t>& data)
{
    FO_TRACE_ZONE(Core);

    if (manifest.FastUpdateEnabled && (manifest.ChunkSize == 0 || manifest.ChunkSize > ContentUpdateMaxChunkPayloadSize)) {
        throw ContentUpdaterException("Invalid content update manifest chunk size", manifest.ChunkSize);
    }

    CheckManifestCollectionSize(manifest.Endpoints.size(), ContentUpdateMaxEndpointCount, "endpoints");
    CheckManifestCollectionSize(manifest.Files.size(), ContentUpdateMaxFileCount, "files");

    vector<uint8_t> serialized;
    data_writer writer {serialized};

    writer.write<uint32_t>(ContentUpdateManifestSignature);
    writer.write<uint16_t>(ContentUpdateManifestVersion);
    writer.write<uint64_t>(manifest.CatalogGeneration);
    writer.write<uint8_t>(manifest.FastUpdateEnabled ? uint8_t {1} : uint8_t {0});
    writer.write<uint8_t>(manifest.SelfHostedServerEnabled ? uint8_t {1} : uint8_t {0});
    writer.write<uint32_t>(manifest.SessionId);
    writer.write<uint32_t>(manifest.ChunkSize);
    writer.write<uint32_t>(numeric_cast<uint32_t>(manifest.Endpoints.size()));

    for (const auto& endpoint : manifest.Endpoints) {
        CheckManifestEndpoint(endpoint);
        WriteManifestString(writer, endpoint.Host, ContentUpdateMaxEndpointHostLength, "endpoint host");
        writer.write<uint16_t>(endpoint.Port);
        writer.write<int32_t>(endpoint.Priority);
    }

    writer.write<uint32_t>(numeric_cast<uint32_t>(manifest.Files.size()));

    unordered_set<uint32_t> file_indexes;
    file_indexes.reserve(manifest.Files.size());

    for (const auto& file : manifest.Files) {
        if (!file_indexes.emplace(file.FileIndex).second) {
            throw ContentUpdaterException("Duplicate content update manifest file index", file.FileIndex);
        }

        CheckManifestFileName(file.Name);
        CheckManifestFileTarget(file.Target);
        CheckManifestFileChunks(manifest, file);

        auto sources = file.Sources;
        CanonicalizeContentUpdateSources(sources);

        writer.write<uint32_t>(file.FileIndex);
        WriteManifestString(writer, file.Name, ContentUpdateMaxFileNameLength, "file name");
        writer.write<uint64_t>(file.Size);
        writer.write<uint64_t>(file.Hash);
        writer.write_bytes(file.Sha256);
        writer.write<UpdateFileTarget>(file.Target);
        writer.write<uint64_t>(file.PackHeader.PackHash);
        writer.write<uint64_t>(file.PackHeader.ContentHash);
        writer.write<uint64_t>(file.PackHeader.IndexOffset);
        writer.write<uint64_t>(file.PackHeader.IndexStoredSize);
        writer.write<uint64_t>(file.PackHeader.IndexDecodedSize);
        writer.write<uint64_t>(file.PackHeader.DataOffset);
        writer.write<uint64_t>(file.PackHeader.DataSize);
        writer.write<uint32_t>(file.PackHeader.IndexCodec);
        writer.write<uint32_t>(file.PackHeader.EntryCount);
        writer.write<uint16_t>(file.PackHeader.VersionMajor);
        writer.write<uint16_t>(file.PackHeader.VersionMinor);
        writer.write<uint32_t>(numeric_cast<uint32_t>(file.ChunkHashes.size()));

        for (const uint64_t chunk_hash : file.ChunkHashes) {
            writer.write<uint64_t>(chunk_hash);
        }

        writer.write<uint32_t>(numeric_cast<uint32_t>(sources.size()));

        for (const auto& source : sources) {
            WriteManifestString(writer, source.Provider, ContentUpdateMaxSourceIdentifierLength, "provider");
            WriteManifestString(writer, source.SourceKey, ContentUpdateMaxSourceIdentifierLength, "source key");
            WriteManifestString(writer, source.Transport, ContentUpdateMaxSourceIdentifierLength, "transport");
            WriteManifestString(writer, source.Locator, ContentUpdateMaxSourceLocatorLength, "locator");
            writer.write<int32_t>(source.Priority);
            writer.write<int64_t>(source.ExpiresAt);
            writer.write_bytes(source.ReportToken);
        }
    }

    if (serialized.size() > ContentUpdateMaxManifestSize) {
        throw ContentUpdaterException("Content update manifest exceeds size limit", serialized.size());
    }

    data = std::move(serialized);
}

auto DeserializeContentUpdateManifest(const_span<uint8_t> data) -> ContentUpdateManifest
{
    FO_TRACE_ZONE(Core);

    try {
        if (data.size() > ContentUpdateMaxManifestSize) {
            throw ContentUpdaterException("Content update manifest exceeds size limit", data.size());
        }

        data_reader reader {data};

        const uint32_t signature = reader.read<uint32_t>();
        const uint16_t version = reader.read<uint16_t>();

        if (signature != ContentUpdateManifestSignature || version != ContentUpdateManifestVersion) {
            throw ContentUpdaterException("Invalid content update manifest header");
        }

        ContentUpdateManifest manifest;
        manifest.CatalogGeneration = reader.read<uint64_t>();
        manifest.FastUpdateEnabled = ReadManifestBool(reader, "fast update enabled");
        manifest.SelfHostedServerEnabled = ReadManifestBool(reader, "self-hosted server enabled");
        manifest.SessionId = reader.read<uint32_t>();
        manifest.ChunkSize = reader.read<uint32_t>();

        if (manifest.FastUpdateEnabled && (manifest.ChunkSize == 0 || manifest.ChunkSize > ContentUpdateMaxChunkPayloadSize)) {
            throw ContentUpdaterException("Invalid content update manifest chunk size", manifest.ChunkSize);
        }

        const uint32_t endpoints_count = reader.read<uint32_t>();
        CheckManifestCount(endpoints_count, ContentUpdateMaxEndpointCount, MinManifestEndpointSize, "endpoints", reader.get_unread_size());
        manifest.Endpoints.reserve(endpoints_count);

        for (uint32_t i = 0; i < endpoints_count; i++) {
            ContentUpdateEndpoint endpoint;
            endpoint.Host = ReadManifestString(reader, ContentUpdateMaxEndpointHostLength, "endpoint host");
            endpoint.Port = reader.read<uint16_t>();
            endpoint.Priority = reader.read<int32_t>();
            CheckManifestEndpoint(endpoint);
            manifest.Endpoints.emplace_back(std::move(endpoint));
        }

        const uint32_t files_count = reader.read<uint32_t>();
        CheckManifestCount(files_count, ContentUpdateMaxFileCount, MinManifestFileSize, "files", reader.get_unread_size());
        manifest.Files.reserve(files_count);

        unordered_set<uint32_t> file_indexes;
        file_indexes.reserve(files_count);

        for (uint32_t i = 0; i < files_count; i++) {
            ContentUpdateFileInfo file;
            file.FileIndex = reader.read<uint32_t>();

            if (!file_indexes.emplace(file.FileIndex).second) {
                throw ContentUpdaterException("Duplicate content update manifest file index", file.FileIndex);
            }

            file.Name = ReadManifestString(reader, ContentUpdateMaxFileNameLength, "file name");
            CheckManifestFileName(file.Name);
            file.Size = reader.read<uint64_t>();
            file.Hash = reader.read<uint64_t>();
            reader.read_bytes(file.Sha256);
            file.Target = reader.read<UpdateFileTarget>();
            CheckManifestFileTarget(file.Target);
            file.PackHeader.PackHash = reader.read<uint64_t>();
            file.PackHeader.ContentHash = reader.read<uint64_t>();
            file.PackHeader.IndexOffset = reader.read<uint64_t>();
            file.PackHeader.IndexStoredSize = reader.read<uint64_t>();
            file.PackHeader.IndexDecodedSize = reader.read<uint64_t>();
            file.PackHeader.DataOffset = reader.read<uint64_t>();
            file.PackHeader.DataSize = reader.read<uint64_t>();
            file.PackHeader.IndexCodec = reader.read<uint32_t>();
            file.PackHeader.EntryCount = reader.read<uint32_t>();
            file.PackHeader.VersionMajor = reader.read<uint16_t>();
            file.PackHeader.VersionMinor = reader.read<uint16_t>();

            const uint32_t chunks_count = reader.read<uint32_t>();
            CheckManifestCount(chunks_count, ContentUpdateMaxChunkHashCount, MinManifestChunkHashSize, "chunk hashes", reader.get_unread_size());
            file.ChunkHashes.reserve(chunks_count);

            for (uint32_t chunk_index = 0; chunk_index < chunks_count; chunk_index++) {
                file.ChunkHashes.emplace_back(reader.read<uint64_t>());
            }

            CheckManifestFileChunks(manifest, file);

            const uint32_t sources_count = reader.read<uint32_t>();
            CheckManifestCount(sources_count, ContentUpdateMaxSourcesPerFile, MinManifestSourceSize, "sources", reader.get_unread_size());
            file.Sources.reserve(sources_count);

            for (uint32_t source_index = 0; source_index < sources_count; source_index++) {
                ContentUpdateSource source;
                source.Provider = ReadManifestString(reader, ContentUpdateMaxSourceIdentifierLength, "provider");
                source.SourceKey = ReadManifestString(reader, ContentUpdateMaxSourceIdentifierLength, "source key");
                source.Transport = ReadManifestString(reader, ContentUpdateMaxSourceIdentifierLength, "transport");
                source.Locator = ReadManifestString(reader, ContentUpdateMaxSourceLocatorLength, "locator");
                source.Priority = reader.read<int32_t>();
                source.ExpiresAt = reader.read<int64_t>();
                reader.read_bytes(source.ReportToken);
                file.Sources.emplace_back(std::move(source));
            }

            CanonicalizeContentUpdateSources(file.Sources);

            manifest.Files.emplace_back(std::move(file));
        }

        reader.verify_end();
        return manifest;
    }
    catch (const ContentUpdaterException&) {
        throw;
    }
    catch (const DataReadingException& ex) {
        throw ContentUpdaterException("Invalid content update manifest payload", ex.what());
    }
}

static auto TryParseContentUpdateKeyId(string_view value, uint32_t& key_id) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    if (value.empty()) {
        return false;
    }

    uint32_t parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);

    if (error != std::errc {} || end != value.data() + value.size() || parsed == 0) {
        return false;
    }

    key_id = parsed;
    return true;
}

auto TryParseContentUpdateTrustedPublicKey(string_view value, ContentUpdateTrustedPublicKey& key) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    const size_t separator = value.find(':');

    if (separator == string_view::npos || value.find(':', separator + 1) != string_view::npos) {
        return false;
    }

    ContentUpdateTrustedPublicKey parsed {};
    Sha256Digest public_key {};

    if (!TryParseContentUpdateKeyId(value.substr(0, separator), parsed.KeyId) || !TryParseSha256Digest(value.substr(separator + 1), public_key)) {
        return false;
    }

    parsed.PublicKey = public_key;
    key = parsed;
    return true;
}

auto TryParseContentUpdateSigningKey(string_view value, ContentUpdateSigningKey& key) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    const size_t first_separator = value.find(':');
    const size_t second_separator = first_separator != string_view::npos ? value.find(':', first_separator + 1) : string_view::npos;

    if (first_separator == string_view::npos || second_separator == string_view::npos || value.find(':', second_separator + 1) != string_view::npos) {
        return false;
    }

    ContentUpdateSigningKey parsed {};
    Sha256Digest public_key {};
    Sha256Digest private_key {};

    if (!TryParseContentUpdateKeyId(value.substr(0, first_separator), parsed.KeyId) || !TryParseSha256Digest(value.substr(first_separator + 1, second_separator - first_separator - 1), public_key) || !TryParseSha256Digest(value.substr(second_separator + 1), private_key)) {
        return false;
    }

    parsed.PublicKey = public_key;
    parsed.PrivateKey = private_key;
    key = parsed;
    return true;
}

auto IsContentUpdateSourceReportTokenEmpty(const ContentUpdateSourceReportToken& token) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    return std::ranges::all_of(token, [](uint8_t value) noexcept { return value == 0; });
}

auto FillContentUpdateSecureRandom(span<uint8_t> data) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    if (data.empty()) {
        return true;
    }
    if (data.size() > numeric_cast<size_t>(INT_MAX)) {
        return false;
    }

    return RAND_bytes(data.data(), static_cast<int>(data.size())) == 1;
}

void ContentUpdateSourceReportReplayGuard::Begin(uint64_t session_id)
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(session_id != 0, "Content update feedback session id must be non-zero");
    FO_VERIFY_AND_THROW(_sessionId == 0, "Content update feedback session was already initialized");
    _sessionId = session_id;
    _consumedTokens.clear();
}

auto ContentUpdateSourceReportReplayGuard::Consume(const ContentUpdateSourceReportToken& token) -> bool
{
    FO_TRACE_ZONE(Core);

    if (_sessionId == 0 || IsContentUpdateSourceReportTokenEmpty(token) || _consumedTokens.size() >= ContentUpdateMaxConsumedSourceReportTokens) {
        return false;
    }

    return _consumedTokens.emplace(token).second;
}

void SignContentUpdateManifestDescriptor(const_span<uint8_t> manifest_data, string_view binary_target, uint64_t release_sequence, const ContentUpdateSigningKey& key, vector<uint8_t>& data)
{
    FO_TRACE_ZONE(Core);

    if (manifest_data.empty() || manifest_data.size() > ContentUpdateMaxManifestSize) {
        throw ContentUpdaterException("Invalid content update manifest payload size", manifest_data.size());
    }
    if (binary_target.size() > ContentUpdateMaxBinaryTargetLength || binary_target.size() > std::numeric_limits<uint16_t>::max()) {
        throw ContentUpdaterException("Invalid content update binary target length", binary_target.size());
    }
    if (release_sequence == 0 || key.KeyId == 0) {
        throw ContentUpdaterException("Invalid content update descriptor signing metadata", release_sequence, key.KeyId);
    }

    vector<uint8_t> signed_data;
    data_writer writer {signed_data};
    writer.write<uint32_t>(ContentUpdateSignedDescriptorSignature);
    writer.write<uint16_t>(ContentUpdateSignedDescriptorVersion);
    writer.write<uint32_t>(key.KeyId);
    writer.write<uint64_t>(release_sequence);
    writer.write<uint16_t>(numeric_cast<uint16_t>(binary_target.size()));
    writer.write_string_bytes(binary_target);
    writer.write<uint32_t>(numeric_cast<uint32_t>(manifest_data.size()));
    writer.write_bytes(manifest_data);

    ContentUpdateEd25519Signature signature {};

    if (ED25519_sign(signature.data(), signed_data.data(), signed_data.size(), key.PublicKey.data(), key.PrivateKey.data()) != 1 || ED25519_verify(signed_data.data(), signed_data.size(), signature.data(), key.PublicKey.data()) != 1) {
        throw ContentUpdaterException("Content update descriptor signing key pair is invalid", key.KeyId);
    }

    writer.write_bytes(signature);

    if (signed_data.size() > ContentUpdateMaxDescriptorSize) {
        throw ContentUpdaterException("Signed content update descriptor exceeds size limit", signed_data.size());
    }

    data = std::move(signed_data);
}

auto VerifyContentUpdateManifestDescriptor(const_span<uint8_t> data, string_view expected_binary_target, uint64_t minimum_release_sequence, const vector<ContentUpdateTrustedPublicKey>& trusted_keys) -> VerifiedContentUpdateManifest
{
    FO_TRACE_ZONE(Core);

    try {
        if (data.size() > ContentUpdateMaxDescriptorSize || data.size() < sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint32_t) + ContentUpdateEd25519SignatureSize) {
            throw ContentUpdaterException("Invalid signed content update descriptor size", data.size());
        }

        data_reader reader {data};
        const uint32_t descriptor_signature = reader.read<uint32_t>();
        const uint16_t descriptor_version = reader.read<uint16_t>();

        if (descriptor_signature != ContentUpdateSignedDescriptorSignature || descriptor_version != ContentUpdateSignedDescriptorVersion) {
            throw ContentUpdaterException("Invalid signed content update descriptor header");
        }

        VerifiedContentUpdateManifest verified {};
        verified.KeyId = reader.read<uint32_t>();
        verified.ReleaseSequence = reader.read<uint64_t>();
        const uint16_t binary_target_length = reader.read<uint16_t>();

        if (binary_target_length > ContentUpdateMaxBinaryTargetLength) {
            throw ContentUpdaterException("Invalid signed content update binary target length", binary_target_length);
        }

        verified.BinaryTarget = string(reader.read_string_view(binary_target_length));
        const uint32_t manifest_size = reader.read<uint32_t>();

        if (manifest_size == 0 || manifest_size > ContentUpdateMaxManifestSize || numeric_cast<size_t>(manifest_size) > data.size() - ContentUpdateEd25519SignatureSize) {
            throw ContentUpdaterException("Invalid signed content update manifest payload size", manifest_size);
        }

        const const_span<uint8_t> manifest_data = reader.read_bytes(manifest_size);
        ContentUpdateEd25519Signature signature {};
        reader.read_bytes(signature);
        reader.verify_end();

        if (verified.ReleaseSequence == 0 || verified.ReleaseSequence < minimum_release_sequence) {
            throw ContentUpdaterException("Content update descriptor release sequence is below the trusted minimum", verified.ReleaseSequence, minimum_release_sequence);
        }
        const auto trusted_key_it = std::ranges::find_if(trusted_keys, [&verified](const ContentUpdateTrustedPublicKey& trusted_key) { return trusted_key.KeyId == verified.KeyId; });

        if (trusted_key_it == trusted_keys.end()) {
            throw ContentUpdaterException("Unknown content update descriptor signing key", verified.KeyId);
        }

        const size_t signed_size = data.size() - signature.size();

        if (ED25519_verify(data.data(), signed_size, signature.data(), trusted_key_it->PublicKey.data()) != 1) {
            throw ContentUpdaterException("Invalid content update descriptor signature", verified.KeyId);
        }

        verified.Manifest = DeserializeContentUpdateManifest(manifest_data);

        if (verified.BinaryTarget != expected_binary_target) {
            const bool resource_only_wildcard = verified.BinaryTarget.empty() && std::ranges::none_of(verified.Manifest.Files, [](const ContentUpdateFileInfo& file) { return file.Target == UpdateFileTarget::ClientBinaries; });

            if (!resource_only_wildcard) {
                throw ContentUpdaterException("Content update descriptor binary target mismatch", verified.BinaryTarget, expected_binary_target);
            }
        }

        return verified;
    }
    catch (const ContentUpdaterException&) {
        throw;
    }
    catch (const DataReadingException& ex) {
        throw ContentUpdaterException("Invalid signed content update descriptor payload", ex.what());
    }
}

auto VerifyContentUpdateStagedBinaryAuthorization(const_span<uint8_t> descriptor, string_view expected_binary_target, uint64_t minimum_release_sequence, const vector<ContentUpdateTrustedPublicKey>& trusted_keys, string_view local_runtime_file_name, string_view packaged_runtime_file_name, uint64_t staged_size, const Sha256Digest& staged_sha256) -> bool
{
    FO_TRACE_ZONE(Core);

    const VerifiedContentUpdateManifest verified = VerifyContentUpdateManifestDescriptor(descriptor, expected_binary_target, minimum_release_sequence, trusted_keys);

    for (const auto& file : verified.Manifest.Files) {
        if (file.Target != UpdateFileTarget::ClientBinaries || file.Size != staged_size || file.Sha256 != staged_sha256) {
            continue;
        }

        const string signed_runtime_file_name = strex(file.Name).extract_file_name().str();

        if (signed_runtime_file_name == local_runtime_file_name || (!packaged_runtime_file_name.empty() && signed_runtime_file_name == packaged_runtime_file_name)) {
            return true;
        }
    }

    return false;
}

static std::atomic<uint64_t> ContentUpdateTrustTempCounter {};

class ContentUpdateTrustProcessLock final
{
public:
    explicit ContentUpdateTrustProcessLock(string_view path) noexcept;
    ContentUpdateTrustProcessLock(const ContentUpdateTrustProcessLock&) = delete;
    ContentUpdateTrustProcessLock(ContentUpdateTrustProcessLock&&) noexcept = delete;
    auto operator=(const ContentUpdateTrustProcessLock&) -> ContentUpdateTrustProcessLock& = delete;
    auto operator=(ContentUpdateTrustProcessLock&&) noexcept -> ContentUpdateTrustProcessLock& = delete;
    ~ContentUpdateTrustProcessLock();

    [[nodiscard]] auto IsLocked() const noexcept -> bool;

private:
    int _fd {-1};
};

ContentUpdateTrustProcessLock::ContentUpdateTrustProcessLock(string_view path) noexcept
{
    FO_TRACE_ZONE(Core);

#if FO_WINDOWS
    constexpr int32_t max_attempts = 1000;
    const string lock_path {path};

    for (int32_t attempt = 0; attempt != max_attempts; ++attempt) {
        const errno_t error = _sopen_s(&_fd, lock_path.c_str(), _O_BINARY | _O_NOINHERIT | _O_RDWR | _O_CREAT, _SH_DENYRW, _S_IREAD | _S_IWRITE);

        if (error == 0) {
            return;
        }
        if (error != EACCES && error != EAGAIN) {
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {10});
    }
#elif FO_WEB
    ignore_unused(path);
    _fd = 0;
#else
    const string lock_path {path};
    _fd = open(lock_path.c_str(), O_CLOEXEC | O_RDWR | O_CREAT, 0666);

    if (_fd < 0) {
        return;
    }

    int lock_result = 0;

    do {
        lock_result = flock(_fd, LOCK_EX);
    } while (lock_result != 0 && errno == EINTR);

    if (lock_result != 0) {
        (void)close(_fd);
        _fd = -1;
    }
#endif
}

ContentUpdateTrustProcessLock::~ContentUpdateTrustProcessLock()
{
    FO_TRACE_ZONE(Core);

#if FO_WINDOWS
    if (_fd >= 0) {
        (void)_close(_fd);
    }
#elif FO_WEB
#else
    if (_fd >= 0) {
        (void)flock(_fd, LOCK_UN);
        (void)close(_fd);
    }
#endif
}

auto ContentUpdateTrustProcessLock::IsLocked() const noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    return _fd >= 0;
}

static auto SyncContentUpdateTrustFile(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

#if FO_WINDOWS
    const string file_path {path};
    int fd = -1;

    if (_sopen_s(&fd, file_path.c_str(), _O_BINARY | _O_NOINHERIT | _O_RDWR, _SH_DENYRW, _S_IREAD | _S_IWRITE) != 0) {
        return false;
    }

    const bool synced = _commit(fd) == 0;
    (void)_close(fd);
    return synced;
#elif FO_WEB
    ignore_unused(path);
    return true;
#else
    const string file_path {path};
    const int fd = open(file_path.c_str(), O_CLOEXEC | O_RDWR);

    if (fd < 0) {
        return false;
    }

    const bool synced = fsync(fd) == 0;
    (void)close(fd);
    return synced;
#endif
}

static auto SyncContentUpdateTrustDir(string_view path) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

#if FO_WINDOWS || FO_WEB
    ignore_unused(path);
    return true;
#else
    const string dir_path {path};
    const int fd = open(dir_path.c_str(), O_CLOEXEC | O_RDONLY | O_DIRECTORY);

    if (fd < 0) {
        return false;
    }

    const bool synced = fsync(fd) == 0;
    (void)close(fd);
    return synced;
#endif
}

static auto MakeContentUpdateTrustTempPath(string_view trust_dir, string_view marker_name, string_view suffix) -> string
{
    FO_TRACE_ZONE(Core);

    const uint64_t counter = ContentUpdateTrustTempCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    return strex(trust_dir).combine_path(strex(".{}.{}-{}-{}", marker_name, suffix, platform::get_current_process_id_str(), counter)).str();
}

static auto QuarantineContentUpdateTrustMarker(string_view trust_dir, string_view marker_path, string_view marker_name) -> bool
{
    FO_TRACE_ZONE(Core);

    for (int32_t attempt = 0; attempt != 16; ++attempt) {
        const string quarantine_path = MakeContentUpdateTrustTempPath(trust_dir, marker_name, "corrupt");

        if (fs::exists(quarantine_path)) {
            continue;
        }
        if (fs::rename(marker_path, quarantine_path)) {
            return SyncContentUpdateTrustDir(trust_dir);
        }

        return false;
    }

    return false;
}

static auto WriteContentUpdateTrustMarkerAtomically(string_view trust_dir, string_view marker_path, string_view marker_name, string_view marker) -> bool
{
    FO_TRACE_ZONE(Core);

    const string pending_path = MakeContentUpdateTrustTempPath(trust_dir, marker_name, "pending");
    const auto cleanup_pending = scope_exit([&pending_path]() noexcept { (void)fs::remove_file(pending_path); });

    if (!fs::write_file(pending_path, marker) || !SyncContentUpdateTrustFile(pending_path) || !fs::rename(pending_path, marker_path)) {
        return false;
    }

    return SyncContentUpdateTrustDir(trust_dir);
}

auto AcceptContentUpdateReleaseSequence(string_view user_writable_path, uint64_t release_sequence, uint64_t minimum_release_sequence) -> bool
{
    FO_TRACE_ZONE(Core);

    if (release_sequence == 0 || release_sequence < minimum_release_sequence) {
        return false;
    }

    try {
        constexpr string_view trust_header = "FONLINE_CONTENT_UPDATE_TRUST_V1\n";
        constexpr string_view trust_dir_name = "UpdaterTrust";
        constexpr string_view marker_prefix = "release-";
        constexpr string_view marker_suffix = ".accepted";
        const string trust_dir = fs::make_writable_path(user_writable_path, trust_dir_name);

        if (!fs::create_directories(trust_dir)) {
            return false;
        }

        const string lock_path = strex(trust_dir).combine_path("acceptance.lock").str();
        const ContentUpdateTrustProcessLock process_lock {lock_path};

        if (!process_lock.IsLocked()) {
            return false;
        }

        uint64_t highest_accepted = 0;
        bool accepted_marker_found = false;
        vector<pair<string, string>> corrupt_markers;

        for (const auto& entry : std::filesystem::directory_iterator {std::filesystem::path {fs::make_path(trust_dir)}}) {
            std::error_code error;
            const auto status = entry.symlink_status(error);

            if (error || !std::filesystem::is_regular_file(status)) {
                continue;
            }

            const string file_name = fs::path_to_string(entry.path().filename());
            const string_view file_name_view = file_name;

            if (!file_name_view.starts_with(marker_prefix) || !file_name_view.ends_with(marker_suffix)) {
                continue;
            }

            const string_view sequence_text = file_name_view.substr(marker_prefix.size(), file_name_view.size() - marker_prefix.size() - marker_suffix.size());
            uint64_t stored_sequence = 0;
            const auto [sequence_end, sequence_error] = std::from_chars(sequence_text.data(), sequence_text.data() + sequence_text.size(), stored_sequence);

            if (sequence_error != std::errc {} || sequence_end != sequence_text.data() + sequence_text.size() || stored_sequence == 0) {
                continue;
            }

            const auto marker = fs::read_file(fs::path_to_string(entry.path()));
            const string expected_marker = strex("{}{}\n", trust_header, stored_sequence).str();

            if (!marker.has_value() || *marker != expected_marker) {
                corrupt_markers.emplace_back(fs::path_to_string(entry.path()), file_name);
                continue;
            }

            highest_accepted = std::max(highest_accepted, stored_sequence);
            accepted_marker_found = true;
        }

        for (const auto& [marker_path, marker_name] : corrupt_markers) {
            if (!QuarantineContentUpdateTrustMarker(trust_dir, marker_path, marker_name)) {
                return false;
            }
        }

        if (accepted_marker_found && release_sequence < highest_accepted) {
            return false;
        }
        if (accepted_marker_found && release_sequence == highest_accepted) {
            return true;
        }

        const string marker_name = strex("{}{}{}", marker_prefix, release_sequence, marker_suffix).str();
        const string marker_path = strex(trust_dir).combine_path(marker_name).str();
        const string marker = strex("{}{}\n", trust_header, release_sequence).str();
        return WriteContentUpdateTrustMarkerAtomically(trust_dir, marker_path, marker_name, marker);
    }
    catch (const std::exception&) {
        return false;
    }
}

auto MakeContentUpdateChunkRequestData(const ContentUpdateChunkRequest& request) -> vector<uint8_t>
{
    FO_TRACE_ZONE(Core);

    vector<uint8_t> data;
    data_writer writer {data};
    WriteDatagramHeader(writer, ContentUpdateDatagramType::ChunkRequest);
    writer.write<uint32_t>(request.SessionId);
    writer.write<uint32_t>(request.FileIndex);
    writer.write<uint32_t>(request.ChunkIndex);
    writer.write<uint64_t>(request.ClientNonce);
    writer.write<int64_t>(request.CookieExpiresAt);
    writer.write_bytes(request.Cookie);
    return data;
}

auto TryReadContentUpdateChunkRequestData(const_span<uint8_t> data, ContentUpdateChunkRequest& request) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    try {
        data_reader reader {data};

        if (!ReadAndCheckDatagramHeader(reader, ContentUpdateDatagramType::ChunkRequest)) {
            return false;
        }

        request.SessionId = reader.read<uint32_t>();
        request.FileIndex = reader.read<uint32_t>();
        request.ChunkIndex = reader.read<uint32_t>();
        request.ClientNonce = reader.read<uint64_t>();
        request.CookieExpiresAt = reader.read<int64_t>();
        reader.read_bytes(request.Cookie);
        reader.verify_end();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

auto MakeContentUpdateCookieChallengeData(const ContentUpdateCookieChallenge& challenge) -> vector<uint8_t>
{
    FO_TRACE_ZONE(Core);

    vector<uint8_t> data;
    data_writer writer {data};
    WriteDatagramHeader(writer, ContentUpdateDatagramType::CookieChallenge);
    writer.write<uint32_t>(challenge.SessionId);
    writer.write<uint32_t>(challenge.FileIndex);
    writer.write<uint32_t>(challenge.ChunkIndex);
    writer.write<uint64_t>(challenge.ClientNonce);
    writer.write<int64_t>(challenge.CookieExpiresAt);
    writer.write_bytes(challenge.Cookie);
    return data;
}

auto TryReadContentUpdateCookieChallengeData(const_span<uint8_t> data, ContentUpdateCookieChallenge& challenge) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    try {
        data_reader reader {data};

        if (!ReadAndCheckDatagramHeader(reader, ContentUpdateDatagramType::CookieChallenge)) {
            return false;
        }

        challenge.SessionId = reader.read<uint32_t>();
        challenge.FileIndex = reader.read<uint32_t>();
        challenge.ChunkIndex = reader.read<uint32_t>();
        challenge.ClientNonce = reader.read<uint64_t>();
        challenge.CookieExpiresAt = reader.read<int64_t>();
        reader.read_bytes(challenge.Cookie);
        reader.verify_end();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

void MakeContentUpdateChunkData(const ContentUpdateChunkDataHeader& header, const_span<uint8_t> payload, vector<uint8_t>& data)
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(header.ChunkSize == payload.size(), "Chunk data header size must match the payload size");
    FO_VERIFY_AND_THROW(header.ChunkSize <= ContentUpdateMaxChunkPayloadSize, "Chunk data payload is over the supported datagram size");

    data.clear();

    data_writer writer {data};
    WriteDatagramHeader(writer, ContentUpdateDatagramType::ChunkData);
    writer.write<uint32_t>(header.SessionId);
    writer.write<uint32_t>(header.FileIndex);
    writer.write<uint32_t>(header.ChunkIndex);
    writer.write<uint32_t>(header.ChunkSize);
    writer.write<uint64_t>(header.ChunkHash);
    writer.write_bytes(payload);
}

auto TryReadContentUpdateChunkData(const_span<uint8_t> data, ContentUpdateChunkDataHeader& header, const_span<uint8_t>& payload) noexcept -> bool
{
    FO_TRACE_ZONE(Core);

    payload = {};

    try {
        data_reader reader {data};

        if (!ReadAndCheckDatagramHeader(reader, ContentUpdateDatagramType::ChunkData)) {
            return false;
        }

        header.SessionId = reader.read<uint32_t>();
        header.FileIndex = reader.read<uint32_t>();
        header.ChunkIndex = reader.read<uint32_t>();
        header.ChunkSize = reader.read<uint32_t>();
        header.ChunkHash = reader.read<uint64_t>();

        if (header.ChunkSize > ContentUpdateMaxChunkPayloadSize) {
            return false;
        }

        payload = reader.read_bytes(header.ChunkSize);
        reader.verify_end();
        return true;
    }
    catch (const std::exception&) {
        payload = {};
        return false;
    }
}

auto GetContentUpdateChunkCount(uint64_t file_size, uint32_t chunk_size) noexcept -> uint32_t
{
    FO_TRACE_ZONE(Core);

    if (file_size == 0 || chunk_size == 0) {
        return 0;
    }

    const uint64_t chunk_count = file_size / chunk_size + (file_size % chunk_size != 0 ? 1 : 0);

    if (chunk_count > std::numeric_limits<uint32_t>::max()) {
        return 0;
    }

    return numeric_cast<uint32_t>(chunk_count);
}

auto GetContentUpdateChunkSize(uint64_t file_size, uint32_t chunk_size, uint32_t chunk_index) noexcept -> uint32_t
{
    FO_TRACE_ZONE(Core);

    if (chunk_size == 0) {
        return 0;
    }

    const uint64_t offset = GetContentUpdateChunkOffset(chunk_size, chunk_index);

    if (offset >= file_size) {
        return 0;
    }

    return numeric_cast<uint32_t>(std::min<uint64_t>(chunk_size, file_size - offset));
}

auto GetContentUpdateChunkOffset(uint32_t chunk_size, uint32_t chunk_index) noexcept -> uint64_t
{
    FO_TRACE_ZONE(Core);

    return numeric_cast<uint64_t>(chunk_size) * chunk_index;
}

FO_END_NAMESPACE
