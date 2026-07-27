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

#include "UpdaterBackend.h"
#include "DataSerialization.h"
#include "DiskFileSystem.h"
#include "Logging.h"
#include "Player.h"
#include "SafeArithmetics.h"
#include "ServerConnection.h"
#include "StringUtils.h"

#if FO_WINDOWS
#include <fcntl.h>
#include <io.h>
#include <share.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

FO_BEGIN_NAMESPACE

static constexpr size_t MaxContentUpdateSourceFeedbackReporters = 256;
static constexpr string_view ContentUpdateSnapshotBaseDir = "fonline-content-update-snapshots";
static constexpr string_view ContentUpdateSnapshotRootPrefix = "catalog-";
static constexpr string_view ContentUpdateSnapshotOwnerFile = "owner.lock";
static constexpr string_view ContentUpdateSnapshotOwnerHeader = "FONLINE_CONTENT_UPDATE_SNAPSHOT_V1\n";
static constexpr string_view ContentUpdateSnapshotFilePrefix = "content-";
static constexpr string_view ContentUpdateSnapshotFileSuffix = ".bin";
static constexpr size_t MaxContentUpdateSnapshotRootEntriesScanned = 128;
static constexpr size_t MaxContentUpdateSnapshotFileEntriesScanned = 256;
static constexpr size_t MaxContentUpdateSnapshotFilesDeleted = 64;
static constexpr size_t MaxContentUpdateSnapshotRootsDeleted = 8;

struct ContentUpdateSnapshotScavengeBudget final
{
    size_t RootEntriesRemaining {MaxContentUpdateSnapshotRootEntriesScanned};
    size_t FileEntriesRemaining {MaxContentUpdateSnapshotFileEntriesScanned};
    size_t FilesRemaining {MaxContentUpdateSnapshotFilesDeleted};
    size_t RootsRemaining {MaxContentUpdateSnapshotRootsDeleted};
};

UpdaterBackend::UpdaterBackend()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(FillContentUpdateSecureRandom(_sourceFeedbackSecret), "Can't initialize content update feedback secret");
}

static auto IsSameContentUpdateSource(const ContentUpdateSource& left, const ContentUpdateSource& right) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return left.Provider == right.Provider && left.SourceKey == right.SourceKey && left.Transport == right.Transport && left.Locator == right.Locator && left.Priority == right.Priority && left.ExpiresAt == right.ExpiresAt;
}

static auto IsValidContentUpdateSourceResult(ContentUpdateSourceResult result) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return result == ContentUpdateSourceResult::Success || result == ContentUpdateSourceResult::TransportFailure || result == ContentUpdateSourceResult::IntegrityFailure;
}

static auto IsContentUpdateSourceFailure(ContentUpdateSourceResult result) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return result == ContentUpdateSourceResult::TransportFailure || result == ContentUpdateSourceResult::IntegrityFailure;
}

static auto TryAcquireContentUpdateSnapshotLock(const string& lock_path, bool create) noexcept -> int
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto path_cstr = make_ptr(lock_path.c_str());
    int fd = -1;

#if FO_WINDOWS
    const int open_flags = _O_BINARY | _O_NOINHERIT | _O_RDWR | (create ? _O_CREAT : 0);

    if (_sopen_s(&fd, path_cstr.get(), open_flags, _SH_DENYRW, _S_IREAD | _S_IWRITE) != 0) {
        return -1;
    }
#else
    const int open_flags = O_CLOEXEC | O_RDWR | (create ? O_CREAT : 0);
    fd = open(path_cstr.get(), open_flags, 0666);

    if (fd < 0) {
        return -1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        (void)close(fd);
        return -1;
    }
#endif

    return fd;
}

static void ReleaseContentUpdateSnapshotLock(int& fd) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (fd < 0) {
        return;
    }

#if FO_WINDOWS
    (void)_close(fd);
#else
    (void)flock(fd, LOCK_UN);
    (void)close(fd);
#endif

    fd = -1;
}

static auto WriteContentUpdateSnapshotOwner(int fd) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const string owner = strex("{}{}\n", ContentUpdateSnapshotOwnerHeader, Platform::GetCurrentProcessIdStr()).str();

#if FO_WINDOWS
    if (_chsize_s(fd, 0) != 0 || _lseeki64(fd, 0, SEEK_SET) < 0) {
        return false;
    }
#else
    if (ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) < 0) {
        return false;
    }
#endif

    size_t offset = 0;

    while (offset != owner.size()) {
        const size_t remaining = owner.size() - offset;

#if FO_WINDOWS
        const int write_size = numeric_cast<int>(std::min<size_t>(remaining, INT_MAX));
        const int written = _write(fd, owner.data() + offset, write_size);
#else
        const ssize_t written = write(fd, owner.data() + offset, remaining);
#endif

        if (written <= 0) {
            return false;
        }

        offset += numeric_cast<size_t>(written);
    }

#if FO_WINDOWS
    return _commit(fd) == 0;
#else
    return fsync(fd) == 0;
#endif
}

static auto IsDecimalContentUpdateSnapshotPart(string_view value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !value.empty() && std::ranges::all_of(value, [](char ch) noexcept { return ch >= '0' && ch <= '9'; });
}

static auto IsContentUpdateSnapshotRootName(string_view name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!name.starts_with(ContentUpdateSnapshotRootPrefix)) {
        return false;
    }

    const string_view suffix = name.substr(ContentUpdateSnapshotRootPrefix.size());
    const size_t separator = suffix.find('-');
    return separator != string_view::npos && IsDecimalContentUpdateSnapshotPart(suffix.substr(0, separator)) && IsDecimalContentUpdateSnapshotPart(suffix.substr(separator + 1));
}

static auto IsContentUpdateSnapshotFileName(string_view name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!name.starts_with(ContentUpdateSnapshotFilePrefix) || !name.ends_with(ContentUpdateSnapshotFileSuffix)) {
        return false;
    }

    const size_t number_size = name.size() - ContentUpdateSnapshotFilePrefix.size() - ContentUpdateSnapshotFileSuffix.size();
    return number_size != 0 && number_size <= 10 && IsDecimalContentUpdateSnapshotPart(name.substr(ContentUpdateSnapshotFilePrefix.size(), number_size));
}

static auto GetContentUpdateSnapshotBasePath() -> std::filesystem::path
{
    FO_STACK_TRACE_ENTRY();

    return std::filesystem::temp_directory_path() / string(ContentUpdateSnapshotBaseDir);
}

static auto IsContentUpdateSnapshotOwnerMarker(string_view marker_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code error;
    const auto status = std::filesystem::symlink_status(std::filesystem::path {fs_make_path(marker_path)}, error);

    if (error || !std::filesystem::is_regular_file(status)) {
        return false;
    }

    const auto owner = fs_read_file(marker_path);

    if (!owner.has_value() || owner->size() <= ContentUpdateSnapshotOwnerHeader.size() + 1 || owner->size() > 128 || !string_view(*owner).starts_with(ContentUpdateSnapshotOwnerHeader) || !owner->ends_with('\n')) {
        return false;
    }

    const string_view owner_id = string_view(*owner).substr(ContentUpdateSnapshotOwnerHeader.size(), owner->size() - ContentUpdateSnapshotOwnerHeader.size() - 1);
    return IsDecimalContentUpdateSnapshotPart(owner_id);
}

static auto TryScavengeContentUpdateSnapshotRoot(const std::filesystem::path& snapshot_root, ContentUpdateSnapshotScavengeBudget& budget) -> bool
{
    FO_STACK_TRACE_ENTRY();

    vector<std::filesystem::path> content_files;
    bool complete_scan = true;
    bool unknown_entry = false;
    std::error_code error;
    std::filesystem::directory_iterator entry {snapshot_root, error};
    const std::filesystem::directory_iterator end;

    if (error) {
        return false;
    }

    while (entry != end) {
        if (budget.FileEntriesRemaining == 0 || content_files.size() >= budget.FilesRemaining) {
            complete_scan = false;
            break;
        }

        --budget.FileEntriesRemaining;
        const string entry_name = fs_path_to_string(entry->path().filename());

        if (entry_name != ContentUpdateSnapshotOwnerFile) {
            const auto status = entry->symlink_status(error);

            if (!error && std::filesystem::is_regular_file(status) && IsContentUpdateSnapshotFileName(entry_name)) {
                content_files.emplace_back(entry->path());
            }
            else {
                error.clear();
                unknown_entry = true;
            }
        }

        entry.increment(error);

        if (error) {
            complete_scan = false;
            break;
        }
    }

    bool all_files_removed = true;

    for (const auto& content_file : content_files) {
        error.clear();

        if (std::filesystem::remove(content_file, error) && !error) {
            --budget.FilesRemaining;
        }
        else {
            all_files_removed = false;
        }
    }

    return complete_scan && !unknown_entry && all_files_removed;
}

static void ScavengeContentUpdateSnapshotDirs() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        const auto base_dir = GetContentUpdateSnapshotBasePath();
        std::error_code error;
        const auto base_status = std::filesystem::symlink_status(base_dir, error);

        if (error || !std::filesystem::is_directory(base_status)) {
            return;
        }

        ContentUpdateSnapshotScavengeBudget budget;
        std::filesystem::directory_iterator entry {base_dir, error};
        const std::filesystem::directory_iterator end;

        while (!error && entry != end && budget.RootEntriesRemaining != 0 && budget.FileEntriesRemaining != 0 && budget.FilesRemaining != 0 && budget.RootsRemaining != 0) {
            --budget.RootEntriesRemaining;
            const auto snapshot_root = entry->path();
            entry.increment(error);

            std::error_code entry_error;
            const auto root_status = std::filesystem::symlink_status(snapshot_root, entry_error);
            const string root_name = fs_path_to_string(snapshot_root.filename());

            if (entry_error || !std::filesystem::is_directory(root_status) || !IsContentUpdateSnapshotRootName(root_name)) {
                continue;
            }

            const string owner_path = fs_path_to_string(snapshot_root / string(ContentUpdateSnapshotOwnerFile));

            // A recognizable owner marker plus an obtainable exclusive lock proves that this is
            // an updater snapshot left by a process that no longer owns it. Locked and unknown
            // roots are deliberately preserved.
            if (!IsContentUpdateSnapshotOwnerMarker(owner_path)) {
                continue;
            }

            int owner_fd = TryAcquireContentUpdateSnapshotLock(owner_path, false);

            if (owner_fd < 0) {
                continue;
            }

            const bool root_ready_to_remove = TryScavengeContentUpdateSnapshotRoot(snapshot_root, budget);
            ReleaseContentUpdateSnapshotLock(owner_fd);

            if (root_ready_to_remove) {
                entry_error.clear();
                const bool owner_removed = std::filesystem::remove(std::filesystem::path {fs_make_path(owner_path)}, entry_error);

                if (owner_removed && !entry_error) {
                    entry_error.clear();

                    if (std::filesystem::remove(snapshot_root, entry_error) && !entry_error) {
                        --budget.RootsRemaining;
                        WriteLog("Removed orphaned content update snapshot {}", fs_path_to_string(snapshot_root));
                    }
                }
            }
        }
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Can't scavenge content update disk snapshots: {}", ex.what());
    }
    catch (...) {
        WriteLog(LogType::Warning, "Can't scavenge content update disk snapshots: unknown error");
    }
}

static auto CreateContentUpdateSnapshotDir(uint64_t snapshot_id) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto base_dir = GetContentUpdateSnapshotBasePath();
    std::error_code error;
    std::filesystem::create_directories(base_dir, error);
    const auto base_status = std::filesystem::symlink_status(base_dir, error);
    FO_VERIFY_AND_THROW(!error && std::filesystem::is_directory(base_status), "Can't create content update snapshot base directory", fs_path_to_string(base_dir), error.message());

    for (uint32_t attempt = 0; attempt != 1024; ++attempt) {
        const auto candidate = base_dir / strex("{}{}-{}", ContentUpdateSnapshotRootPrefix, snapshot_id, attempt).str();
        error.clear();

        if (std::filesystem::create_directory(candidate, error)) {
            return fs_path_to_string(candidate);
        }
        if (error && error != std::errc::file_exists) {
            throw UpdaterException("Can't create content update disk snapshot directory", fs_path_to_string(candidate), error.message());
        }
    }

    throw UpdaterException("Can't allocate unique content update disk snapshot directory", snapshot_id);
}

class ContentUpdateSnapshotOwner final
{
public:
    explicit ContentUpdateSnapshotOwner(uint64_t snapshot_id) :
        _snapshotDir {CreateContentUpdateSnapshotDir(snapshot_id)}
    {
        FO_STACK_TRACE_ENTRY();

        auto cleanup_snapshot = scope_exit([this]() noexcept {
            ReleaseContentUpdateSnapshotLock(_snapshotLockFd);
            const bool snapshot_removed = fs_remove_dir_tree(_snapshotDir);
            ignore_unused(snapshot_removed);
        });
        const string owner_path = strex(_snapshotDir).combine_path(ContentUpdateSnapshotOwnerFile).str();
        _snapshotLockFd = TryAcquireContentUpdateSnapshotLock(owner_path, true);
        FO_VERIFY_AND_THROW(_snapshotLockFd >= 0, "Can't acquire content update disk snapshot owner lock", owner_path);
        FO_VERIFY_AND_THROW(WriteContentUpdateSnapshotOwner(_snapshotLockFd), "Can't write content update disk snapshot owner marker", owner_path);
        cleanup_snapshot.release();
    }

    ContentUpdateSnapshotOwner(const ContentUpdateSnapshotOwner&) = delete;
    ContentUpdateSnapshotOwner(ContentUpdateSnapshotOwner&&) noexcept = delete;
    auto operator=(const ContentUpdateSnapshotOwner&) -> ContentUpdateSnapshotOwner& = delete;
    auto operator=(ContentUpdateSnapshotOwner&&) noexcept -> ContentUpdateSnapshotOwner& = delete;

    ~ContentUpdateSnapshotOwner()
    {
        FO_NO_STACK_TRACE_ENTRY();

        ReleaseContentUpdateSnapshotLock(_snapshotLockFd);
        const bool snapshot_removed = fs_remove_dir_tree(_snapshotDir);
        ignore_unused(snapshot_removed);
    }

    [[nodiscard]] auto MakeFilePath(uint32_t file_id) const -> string
    {
        FO_STACK_TRACE_ENTRY();

        return strex(_snapshotDir).combine_path(strex("{}{}{}", ContentUpdateSnapshotFilePrefix, file_id, ContentUpdateSnapshotFileSuffix)).str();
    }

private:
    string _snapshotDir;
    int _snapshotLockFd {-1};
};

static void CopyContentUpdateSnapshot(string_view source_path, string_view snapshot_path)
{
    FO_STACK_TRACE_ENTRY();

    auto source = fs_open_ifstream(source_path);

    if (!source) {
        throw UpdaterException("Resource pack for client not found", source_path);
    }

    std::ofstream snapshot {std::filesystem::path {fs_make_path(snapshot_path)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(snapshot, "Can't create content update disk snapshot", source_path);
    array<char, 0x10000> buffer {};

    while (source) {
        source.read(buffer.data(), numeric_cast<std::streamsize>(buffer.size()));
        const std::streamsize read_size = source.gcount();

        if (read_size > 0) {
            snapshot.write(buffer.data(), read_size);
            FO_VERIFY_AND_THROW(snapshot, "Can't write content update disk snapshot", source_path);
        }
    }

    FO_VERIFY_AND_THROW(!source.bad(), "Can't read resource pack into content update disk snapshot", source_path);
    snapshot.flush();
    FO_VERIFY_AND_THROW(snapshot, "Can't flush content update disk snapshot", source_path);
}

ContentUpdateFileStorage::ContentUpdateFileStorage(string disk_path, shared_ptr<const ContentUpdateSnapshotOwner> snapshot_owner, uint32_t file_id) :
    _diskPath {std::move(disk_path)},
    _snapshotOwner {std::move(snapshot_owner)}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_snapshotOwner, "Missing content update disk snapshot owner", _diskPath, file_id);
    const string snapshot_path = _snapshotOwner->MakeFilePath(file_id);
    auto cleanup_snapshot = scope_exit([this, &snapshot_path]() noexcept {
        _file.close();
        const bool snapshot_removed = fs_remove_file(snapshot_path);
        ignore_unused(snapshot_removed);
    });
    CopyContentUpdateSnapshot(_diskPath, snapshot_path);
    _file = fs_open_ifstream(snapshot_path);
    FO_VERIFY_AND_THROW(_file, "Can't open content update disk snapshot", _diskPath);
    _size = numeric_cast<uint64_t>(stream_get_size(_file));
    cleanup_snapshot.release();
}

ContentUpdateFileStorage::~ContentUpdateFileStorage()
{
    FO_NO_STACK_TRACE_ENTRY();

    _file.close();
}

auto ContentUpdateFileStorage::Read(uint64_t offset, span<uint8_t> data) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (offset > _size || numeric_cast<uint64_t>(data.size()) > _size - offset) {
        return false;
    }
    if (data.empty()) {
        return true;
    }

    scoped_lock locker {_fileLocker};
    _file.clear();
    _file.seekg(numeric_cast<std::streamoff>(offset), std::ios::beg);

    return _file && stream_read_exact(_file, data);
}

ContentUpdateArtifact::ContentUpdateArtifact(uint64_t generation, uint32_t file_id, string name, UpdateFileTarget target, uint64_t size, uint64_t hash, Sha256Digest sha256, vector<string> binary_targets) :
    _generation {generation},
    _fileId {file_id},
    _name {std::move(name)},
    _target {target},
    _size {size},
    _hash {hash},
    _sha256 {sha256},
    _binaryTargets {std::move(binary_targets)}
{
    FO_STACK_TRACE_ENTRY();
}

ContentUpdateArtifactLease::ContentUpdateArtifactLease(uint64_t generation, uint32_t file_id, string name, UpdateFileTarget target, uint64_t size, uint64_t hash, Sha256Digest sha256, vector<string> binary_targets, shared_ptr<const vector<uint8_t>> memory_data, shared_ptr<const ContentUpdateFileStorage> disk_storage) :
    _generation {generation},
    _fileId {file_id},
    _name {std::move(name)},
    _target {target},
    _size {size},
    _hash {hash},
    _sha256 {sha256},
    _binaryTargets {std::move(binary_targets)},
    _memoryData {std::move(memory_data)},
    _diskStorage {std::move(disk_storage)}
{
    FO_STACK_TRACE_ENTRY();

    if (_memoryData) {
        FO_VERIFY_AND_THROW(_memoryData->size() == numeric_cast<size_t>(_size), "Content update artifact memory size differs from catalog size", _name, _memoryData->size(), _size);
        FO_VERIFY_AND_THROW(!_diskStorage, "Content update artifact has both memory and disk storage", _name);
    }
    else {
        FO_VERIFY_AND_THROW(_diskStorage, "Content update artifact has no storage", _name);
        FO_VERIFY_AND_THROW(_diskStorage->GetSize() == _size, "Content update artifact disk size differs from catalog size", _name, _diskStorage->GetSize(), _size);
    }
}

auto ContentUpdateArtifactLease::Read(uint64_t offset, span<uint8_t> data) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (offset > _size || numeric_cast<uint64_t>(data.size()) > _size - offset) {
        return false;
    }
    if (data.empty()) {
        return true;
    }

    if (_memoryData) {
        const size_t memory_offset = numeric_cast<size_t>(offset);
        MemCopy(data.data(), _memoryData->data() + memory_offset, data.size());
        return true;
    }

    return _diskStorage->Read(offset, data);
}

static auto MakeFastUpdateSessionId(const GlobalSettings& settings) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    const uint64_t time_part = numeric_cast<uint64_t>(nanotime::now().nanoseconds());
    const string& compatibility_version = settings.CompatibilityVersion;
    const uint64_t settings_part = compatibility_version.empty() ? 0 : fs_hash_data({make_ptr(compatibility_version.data()).reinterpret_as<const uint8_t>().get(), compatibility_version.size()});
    const uint64_t mixed = (time_part >> 32U) ^ time_part ^ settings_part;
    uint32_t session_id = numeric_cast<uint32_t>((mixed >> 32U) ^ (mixed & 0xFFFF'FFFFULL));

    if (session_id == 0) {
        session_id = 1;
    }

    return session_id;
}

static auto MakeOrderedFastUpdateEndpoints(const ServerNetworkSettings& settings) -> vector<ContentUpdateEndpoint>
{
    FO_STACK_TRACE_ENTRY();

    vector<ContentUpdateEndpoint> endpoints;

    for (const auto& endpoint_entry : settings.FastUpdateEndpoints) {
        if (const auto endpoint = ParseContentUpdateEndpoint(endpoint_entry); endpoint.has_value()) {
            endpoints.emplace_back(*endpoint);
        }
        else {
            WriteLog(LogType::Warning, "Skip invalid fast updater endpoint '{}'", endpoint_entry);
        }
    }

    std::sort(endpoints.begin(), endpoints.end(), [](const ContentUpdateEndpoint& left, const ContentUpdateEndpoint& right) { return left.Priority > right.Priority; });

    return endpoints;
}

struct UpdateFileIdentity final
{
    uint64_t Hash {};
    Sha256Digest Sha256 {};
};

static auto CalculateUpdateFileIdentity(const ContentUpdateFileStorage& storage) -> UpdateFileIdentity
{
    FO_STACK_TRACE_ENTRY();

    constexpr uint64_t fnv_offset = UINT64_C(0xcbf29ce484222325);
    constexpr uint64_t fnv_prime = UINT64_C(0x100000001b3);

    uint64_t hash = fnv_offset;
    Sha256Hasher sha256_hasher;
    array<uint8_t, 0x10000> buffer {};
    uint64_t offset = 0;

    while (offset < storage.GetSize()) {
        const size_t portion_size = numeric_cast<size_t>(std::min<uint64_t>(buffer.size(), storage.GetSize() - offset));
        span<uint8_t> portion {buffer.data(), portion_size};
        FO_VERIFY_AND_THROW(storage.Read(offset, portion), "Can't read content update file storage while hashing", storage.GetDiskPath(), offset, portion_size);

        for (const uint8_t value : portion) {
            hash = (hash ^ value) * fnv_prime;
        }

        sha256_hasher.Update(portion);
        offset += portion_size;
    }

    return UpdateFileIdentity {.Hash = hash, .Sha256 = sha256_hasher.Finalize()};
}

template<typename T>
static auto ReadUpdateFileData(const T& update_file, uint64_t start_offset, vector<uint8_t>& data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (data.empty()) {
        return true;
    }

    if (update_file.MemoryData) {
        const size_t offset = numeric_cast<size_t>(start_offset);

        if (offset > update_file.MemoryData->size() || data.size() > update_file.MemoryData->size() - offset) {
            return false;
        }

        MemCopy(data.data(), update_file.MemoryData->data() + offset, data.size());
        return true;
    }

    return update_file.DiskStorage && update_file.DiskStorage->Read(start_offset, data);
}

void UpdaterBackend::LoadFromClientResources(const GlobalSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Load client data packs for synchronization");

    if (!settings.UpdateFilesInMemory) {
        ScavengeContentUpdateSnapshotDirs();
    }

    // Build the complete immutable catalog off to the side. Any failure while reading or hashing
    // resources leaves the currently published catalog and its descriptors untouched.
    auto catalog = SafeAlloc::MakeShared<CatalogState>();

    catalog->ManifestSignatureRequired = settings.UpdateManifestSignatureRequired;

    if (catalog->ManifestSignatureRequired) {
        ContentUpdateSigningKey signing_key;

        if (!TryParseContentUpdateSigningKey(settings.UpdateManifestSigningKey, signing_key)) {
            throw UpdaterException("Content update manifest signing key is missing or invalid");
        }
        if (settings.UpdateManifestReleaseSequence <= 0) {
            throw UpdaterException("Content update manifest release sequence must be positive", settings.UpdateManifestReleaseSequence);
        }

        bool signing_key_trusted = false;
        set<uint32_t> trusted_key_ids;

        for (const auto& trusted_key_entry : settings.UpdateManifestTrustedPublicKeys) {
            ContentUpdateTrustedPublicKey trusted_key;

            if (!TryParseContentUpdateTrustedPublicKey(trusted_key_entry, trusted_key)) {
                throw UpdaterException("Invalid trusted content update manifest public key entry");
            }
            if (!trusted_key_ids.emplace(trusted_key.KeyId).second) {
                throw UpdaterException("Duplicate trusted content update manifest public key id", trusted_key.KeyId);
            }

            if (trusted_key.KeyId == signing_key.KeyId && trusted_key.PublicKey == signing_key.PublicKey) {
                signing_key_trusted = true;
            }
        }

        if (!signing_key_trusted) {
            throw UpdaterException("Content update manifest signing key is not present in the trusted public-key set", signing_key.KeyId);
        }

        catalog->ManifestReleaseSequence = numeric_cast<uint64_t>(settings.UpdateManifestReleaseSequence);
        catalog->ManifestSigningKey = signing_key;
    }

    {
        scoped_lock locker {_catalogLocker};
        FO_VERIFY_AND_THROW(_catalogGenerationCounter != std::numeric_limits<uint64_t>::max(), "Content update catalog generation overflow");
        catalog->Generation = ++_catalogGenerationCounter;
    }

    catalog->FastUpdateSessionId = MakeFastUpdateSessionId(settings);
    catalog->FastUpdateChunkSize = settings.FastUpdateChunkSize > 0 && settings.FastUpdateChunkSize <= numeric_cast<int32_t>(ContentUpdateMaxChunkPayloadSize) ? numeric_cast<uint32_t>(settings.FastUpdateChunkSize) : 0;

    if (!settings.UpdateFilesInMemory) {
        const uint64_t snapshot_id = (numeric_cast<uint64_t>(catalog->FastUpdateSessionId) << 32U) | (catalog->Generation & UINT64_C(0xFFFF'FFFF));
        catalog->SnapshotOwner = SafeAlloc::MakeShared<ContentUpdateSnapshotOwner>(snapshot_id);
    }

    const auto fast_update_endpoints = MakeOrderedFastUpdateEndpoints(settings);
    const bool fast_update_enabled = settings.FastUpdateEnabled && catalog->FastUpdateChunkSize != 0 && !fast_update_endpoints.empty();
    const bool fast_update_bind_port_valid = settings.FastUpdateBindPort > 0 && settings.FastUpdateBindPort <= numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max());
    const bool self_hosted_fast_update_enabled = fast_update_enabled && settings.FastUpdateServerEnabled && fast_update_bind_port_valid;
    catalog->FastUpdateEnabled = fast_update_enabled;
    catalog->SelfHostedFastUpdateEnabled = self_hosted_fast_update_enabled;
    catalog->FastUpdateEndpoints = fast_update_endpoints;

    if (settings.FastUpdateEnabled && catalog->FastUpdateChunkSize == 0) {
        WriteLog(LogType::Warning, "Fast updater is disabled because chunk size {} is outside the supported range 1..{}", settings.FastUpdateChunkSize, ContentUpdateMaxChunkPayloadSize);
    }

    const auto add_sync_file = [&settings, &catalog, fast_update_enabled](string_view disk_path, string_view client_path, UpdateFileTarget target, vector<string> binary_targets) -> UpdateFileInfo {
        UpdateFileData data {};

        if (settings.UpdateFilesInMemory) {
            auto file = fs_open_ifstream(disk_path);

            if (!file) {
                throw UpdaterException("Resource pack for client not found", disk_path);
            }

            const size_t file_size = stream_get_size(file);
            auto memory_data = SafeAlloc::MakeShared<vector<uint8_t>>();
            memory_data->resize(file_size);

            if (!stream_read_exact(file, *memory_data)) {
                throw UpdaterException("Can't read resource pack for client", disk_path);
            }

            data.Size = numeric_cast<uint64_t>(file_size);
            data.Hash = fs_hash_data(*memory_data);
            data.Sha256 = ComputeSha256(*memory_data);
            data.MemoryData = std::move(memory_data);
        }
        else {
            FO_STRONG_ASSERT(catalog->SnapshotOwner, "Missing disk-backed content update catalog snapshot owner");
            const uint32_t file_id = numeric_cast<uint32_t>(catalog->UpdateFiles.size());
            auto disk_storage = SafeAlloc::MakeShared<ContentUpdateFileStorage>(string(disk_path), catalog->SnapshotOwner, file_id);
            const UpdateFileIdentity identity = CalculateUpdateFileIdentity(*disk_storage);
            data.Size = disk_storage->GetSize();
            data.Hash = identity.Hash;
            data.Sha256 = identity.Sha256;
            data.DiskStorage = std::move(disk_storage);
        }

        if (fast_update_enabled && catalog->FastUpdateChunkSize != 0) {
            const uint32_t chunk_count = GetContentUpdateChunkCount(data.Size, catalog->FastUpdateChunkSize);
            data.ChunkHashes.reserve(chunk_count);

            vector<uint8_t> chunk_data;

            for (uint32_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
                const uint64_t chunk_offset = GetContentUpdateChunkOffset(catalog->FastUpdateChunkSize, chunk_index);
                const uint32_t chunk_size = GetContentUpdateChunkSize(data.Size, catalog->FastUpdateChunkSize, chunk_index);

                chunk_data.resize(chunk_size);

                const bool chunk_read = ReadUpdateFileData(data, chunk_offset, chunk_data);

                if (!chunk_read) {
                    throw UpdaterException("Can't read update file chunk for fast updater", disk_path);
                }

                data.ChunkHashes.emplace_back(fs_hash_data(chunk_data));
            }
        }

        auto data_owner = SafeAlloc::MakeShared<UpdateFileData>(std::move(data));
        shared_ptr<const UpdateFileData> immutable_data = std::move(data_owner);
        catalog->UpdateFiles.emplace_back(std::move(immutable_data));

        UpdateFileInfo info {};
        info.FileIndex = numeric_cast<uint32_t>(catalog->UpdateFiles.size() - 1);
        info.ClientPath = string(client_path);
        info.Target = target;
        info.BinaryTargets = std::move(binary_targets);

        const auto& stored_data = catalog->UpdateFiles.back();
        catalog->Artifacts.emplace_back(SafeAlloc::MakeRefCounted<ContentUpdateArtifact>(catalog->Generation, info.FileIndex, info.ClientPath, info.Target, stored_data->Size, stored_data->Hash, stored_data->Sha256, info.BinaryTargets));

        return info;
    };

    auto client_resources_dir = std::filesystem::path {fs_make_path(settings.ClientResources)};

    for (const auto& resource_entry : settings.ClientResourceEntries) {
        if (resource_entry != "Embedded") {
            const auto pack_name = strex("{}.zip", resource_entry).str();
            const auto pack_disk_path = fs_path_to_string(client_resources_dir / pack_name);
            auto info = add_sync_file(pack_disk_path, pack_name, UpdateFileTarget::ClientResources, {});
            catalog->CommonUpdateFiles.emplace_back(std::move(info));
        }
    }

    auto platform_binaries_dir = std::filesystem::path {fs_make_path(settings.PlatformBinaries)};
    string platform_binaries_path = fs_path_to_string(platform_binaries_dir);

    if (std::filesystem::exists(platform_binaries_dir)) {
        FO_VERIFY_AND_THROW(std::filesystem::is_directory(platform_binaries_dir), "Platform binaries path exists but is not a directory", platform_binaries_path);

        for (const auto& platform_entry : std::filesystem::directory_iterator {platform_binaries_dir}) {
            if (!platform_entry.is_directory()) {
                continue;
            }

            string binary_target_name = fs_path_to_string(platform_entry.path().filename());
            FO_VERIFY_AND_THROW(!binary_target_name.empty(), "Updater backend found a platform binaries directory entry with an empty target name", platform_binaries_path, fs_path_to_string(platform_entry.path()));

            for (const auto& binary_entry : std::filesystem::recursive_directory_iterator {platform_entry.path()}) {
                FO_VERIFY_AND_THROW(binary_entry.is_regular_file(), "Updater backend binary target contains a non-file entry", binary_target_name, fs_path_to_string(binary_entry.path()));
                const auto disk_path = fs_path_to_string(binary_entry.path());
                const auto client_file_name = fs_path_to_string(binary_entry.path().filename());
                auto info = add_sync_file(disk_path, client_file_name, UpdateFileTarget::ClientBinaries, {binary_target_name});
                catalog->BinaryTargetUpdateFiles[string(binary_target_name)].emplace_back(std::move(info));
            }
        }
    }

    catalog->UpdateFileSources.reserve(catalog->UpdateFiles.size());

    for (size_t index = 0; index < catalog->UpdateFiles.size(); ++index) {
        auto empty_sources = SafeAlloc::MakeShared<vector<ContentUpdateSource>>();
        shared_ptr<const vector<ContentUpdateSource>> immutable_sources = std::move(empty_sources);
        catalog->UpdateFileSources.emplace_back(std::move(immutable_sources));
    }
    BuildDescriptorSnapshots(*catalog);

    // No-throw commit tail: clearing feedback destroys only owned values and moving the shared
    // catalog pointer cannot throw, so readers never observe a partially rebuilt catalog.
    {
        scoped_lock locker {_catalogLocker};
        FO_VERIFY_AND_THROW(!_catalog || _catalog->Generation < catalog->Generation, "Stale content update catalog load completion", catalog->Generation, _catalog ? _catalog->Generation : 0);
        _sourceFeedback.clear();
        _catalog = std::move(catalog);
    }
}

auto UpdaterBackend::GetUpdateDescriptor(string_view binary_target_name, int64_t current_synchronized_time_ms, uint64_t feedback_session_id) -> shared_ptr<const vector<uint8_t>>
{
    FO_STACK_TRACE_ENTRY();

    shared_ptr<const CatalogState> catalog;
    shared_ptr<const vector<uint8_t>> raw_descriptor;
    shared_ptr<const vector<uint8_t>> signed_descriptor;

    {
        scoped_lock locker {_catalogLocker};

        if (!_catalog) {
            return {};
        }

        if (_catalog->NextSourceExpiry != 0 && _catalog->NextSourceExpiry <= current_synchronized_time_ms) {
            PruneExpiredSourcesLocked(current_synchronized_time_ms);
        }

        catalog = _catalog;
        const auto desc_it = catalog->BinaryTargetUpdateFilesDesc.find(string(binary_target_name));

        if (desc_it != catalog->BinaryTargetUpdateFilesDesc.end()) {
            raw_descriptor = desc_it->second;
            const auto signed_desc_it = catalog->BinaryTargetSignedUpdateFilesDesc.find(string(binary_target_name));
            signed_descriptor = signed_desc_it != catalog->BinaryTargetSignedUpdateFilesDesc.end() ? signed_desc_it->second : shared_ptr<const vector<uint8_t>> {};
        }
        else {
            raw_descriptor = catalog->CommonUpdateFilesDesc;
            signed_descriptor = catalog->CommonSignedUpdateFilesDesc;
        }
    }

    if (!raw_descriptor) {
        return raw_descriptor;
    }

    if (feedback_session_id == 0 || !catalog->HasSources) {
        FO_STRONG_ASSERT(!catalog->ManifestSignatureRequired || signed_descriptor, "Missing cached signed content update descriptor", binary_target_name);
        return catalog->ManifestSignatureRequired ? signed_descriptor : raw_descriptor;
    }

    ContentUpdateManifest manifest = DeserializeContentUpdateManifest(*raw_descriptor);

    for (auto& file : manifest.Files) {
        for (auto& source : file.Sources) {
            source.ReportToken = MakeSourceReportToken(feedback_session_id, *catalog, file.FileIndex, source);
        }
    }

    auto personalized_descriptor = SafeAlloc::MakeShared<vector<uint8_t>>();
    SerializeContentUpdateManifest(manifest, *personalized_descriptor);

    if (!catalog->ManifestSignatureRequired) {
        return personalized_descriptor;
    }

    FO_VERIFY_AND_THROW(catalog->ManifestSigningKey.has_value(), "Signed content update catalog has no signing key");
    auto personalized_signed_descriptor = SafeAlloc::MakeShared<vector<uint8_t>>();
    SignContentUpdateManifestDescriptor(*personalized_descriptor, binary_target_name, catalog->ManifestReleaseSequence, *catalog->ManifestSigningKey, *personalized_signed_descriptor);
    shared_ptr<const vector<uint8_t>> immutable_descriptor = std::move(personalized_signed_descriptor);
    return immutable_descriptor;
}

auto UpdaterBackend::BeginContentUpdateFeedbackSession() -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_catalogLocker};
    ++_sourceFeedbackSessionCounter;

    if (_sourceFeedbackSessionCounter == 0) {
        ++_sourceFeedbackSessionCounter;
    }

    return _sourceFeedbackSessionCounter;
}

auto UpdaterBackend::GetFastUpdateSessionId() const noexcept -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto catalog = GetCatalogSnapshot();
    return catalog ? catalog->FastUpdateSessionId : 0;
}

auto UpdaterBackend::IsFastUpdateEnabled() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto catalog = GetCatalogSnapshot();
    return catalog && catalog->FastUpdateEnabled;
}

auto UpdaterBackend::GetContentUpdateCatalogGeneration() const noexcept -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    const auto catalog = GetCatalogSnapshot();
    return catalog ? catalog->Generation : 0;
}

auto UpdaterBackend::GetContentUpdateCatalog() const -> vector<refcount_ptr<ContentUpdateArtifact>>
{
    FO_STACK_TRACE_ENTRY();

    const auto catalog = GetCatalogSnapshot();
    return catalog ? catalog->Artifacts : vector<refcount_ptr<ContentUpdateArtifact>> {};
}

auto UpdaterBackend::AcquireContentUpdateArtifact(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256) const -> shared_ptr<ContentUpdateArtifactLease>
{
    FO_STACK_TRACE_ENTRY();

    const auto catalog = GetCatalogSnapshot();

    if (!catalog || catalog->Generation != generation || file_id >= catalog->UpdateFiles.size()) {
        return {};
    }

    const auto& data = catalog->UpdateFiles[file_id];

    if (data->Sha256 != expected_sha256) {
        return {};
    }

    const auto& artifact = catalog->Artifacts[file_id];
    return SafeAlloc::MakeShared<ContentUpdateArtifactLease>(catalog->Generation, file_id, artifact->GetName(), artifact->GetTargetNative(), data->Size, data->Hash, data->Sha256, artifact->GetBinaryTargets(), data->MemoryData, data->DiskStorage);
}

auto UpdaterBackend::UpsertContentUpdateSource(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256, ContentUpdateSource source) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ValidateContentUpdateSource(source);

    scoped_lock locker {_catalogLocker};

    if (!_catalog || _catalog->Generation != generation || file_id >= _catalog->UpdateFiles.size() || _catalog->UpdateFiles[file_id]->Sha256 != expected_sha256) {
        return false;
    }

    const auto& current_sources = *_catalog->UpdateFileSources[file_id];
    const auto current_source_it = std::ranges::find_if(current_sources, [&source](const ContentUpdateSource& existing) { return existing.Provider == source.Provider && existing.SourceKey == source.SourceKey; });

    if (current_source_it != current_sources.end() && IsSameContentUpdateSource(*current_source_it, source)) {
        return true;
    }

    source.ReportToken = {};
    const SourceIdentity source_identity {.FileId = file_id, .Provider = source.Provider, .SourceKey = source.SourceKey};

    auto next_catalog = SafeAlloc::MakeShared<CatalogState>(*_catalog);
    auto next_sources = SafeAlloc::MakeShared<vector<ContentUpdateSource>>(current_sources);
    const auto source_it = std::ranges::find_if(*next_sources, [&source](const ContentUpdateSource& existing) { return existing.Provider == source.Provider && existing.SourceKey == source.SourceKey; });

    if (source_it != next_sources->end()) {
        if (IsSameContentUpdateSource(*source_it, source)) {
            return true;
        }

        *source_it = std::move(source);
    }
    else {
        next_sources->emplace_back(std::move(source));
    }

    CanonicalizeContentUpdateSources(*next_sources);
    next_catalog->UpdateFileSources[file_id] = std::move(next_sources);
    BuildAffectedDescriptorSnapshots(*next_catalog, {file_id});

    // No-throw commit tail: a failed descriptor rebuild must preserve both the published catalog
    // and its accumulated feedback/token state.
    _catalog = std::move(next_catalog);
    _sourceFeedback.erase(source_identity);
    return true;
}

auto UpdaterBackend::RemoveContentUpdateSource(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256, string_view provider, string_view source_key) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ValidateContentUpdateSource(ContentUpdateSource {.Provider = string(provider), .SourceKey = string(source_key), .Transport = "validation", .Locator = "validation"});

    scoped_lock locker {_catalogLocker};

    if (!_catalog || _catalog->Generation != generation || file_id >= _catalog->UpdateFiles.size() || _catalog->UpdateFiles[file_id]->Sha256 != expected_sha256) {
        return false;
    }

    const auto& current_sources = *_catalog->UpdateFileSources[file_id];
    const auto source_it = std::ranges::find_if(current_sources, [provider, source_key](const ContentUpdateSource& existing) { return existing.Provider == provider && existing.SourceKey == source_key; });

    if (source_it == current_sources.end()) {
        return true;
    }

    auto next_catalog = SafeAlloc::MakeShared<CatalogState>(*_catalog);
    auto next_sources = SafeAlloc::MakeShared<vector<ContentUpdateSource>>(current_sources);
    next_sources->erase(std::ranges::find_if(*next_sources, [provider, source_key](const ContentUpdateSource& existing) { return existing.Provider == provider && existing.SourceKey == source_key; }));
    next_catalog->UpdateFileSources[file_id] = std::move(next_sources);
    const SourceIdentity source_identity {.FileId = file_id, .Provider = string(provider), .SourceKey = string(source_key)};

    BuildAffectedDescriptorSnapshots(*next_catalog, {file_id});
    _catalog = std::move(next_catalog);
    _sourceFeedback.erase(source_identity);
    return true;
}

auto UpdaterBackend::ClearContentUpdateSources(uint64_t generation, string_view provider) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ValidateContentUpdateSource(ContentUpdateSource {.Provider = string(provider), .SourceKey = "validation", .Transport = "validation", .Locator = "validation"});

    scoped_lock locker {_catalogLocker};

    if (!_catalog || _catalog->Generation != generation) {
        return false;
    }

    bool changed = false;
    auto next_catalog = SafeAlloc::MakeShared<CatalogState>(*_catalog);
    vector<uint32_t> affected_file_ids;
    vector<SourceIdentity> feedback_to_erase;

    for (size_t file_id = 0; file_id < next_catalog->UpdateFileSources.size(); file_id++) {
        const auto& current_sources = *next_catalog->UpdateFileSources[file_id];
        auto next_sources = SafeAlloc::MakeShared<vector<ContentUpdateSource>>(current_sources);

        for (const auto& source : current_sources) {
            if (source.Provider == provider) {
                const SourceIdentity source_identity {.FileId = numeric_cast<uint32_t>(file_id), .Provider = source.Provider, .SourceKey = source.SourceKey};
                feedback_to_erase.emplace_back(source_identity);
            }
        }

        std::erase_if(*next_sources, [provider](const ContentUpdateSource& source) { return source.Provider == provider; });

        if (next_sources->size() != current_sources.size()) {
            next_catalog->UpdateFileSources[file_id] = std::move(next_sources);
            affected_file_ids.emplace_back(numeric_cast<uint32_t>(file_id));
            changed = true;
        }
    }

    if (!changed) {
        return true;
    }

    BuildAffectedDescriptorSnapshots(*next_catalog, affected_file_ids);
    _catalog = std::move(next_catalog);

    for (const auto& source_identity : feedback_to_erase) {
        _sourceFeedback.erase(source_identity);
    }

    return true;
}

auto UpdaterBackend::MakeSourceReportToken(uint64_t feedback_session_id, const CatalogState& catalog, uint32_t file_id, const ContentUpdateSource& source) const -> ContentUpdateSourceReportToken
{
    FO_STACK_TRACE_ENTRY();

    if (feedback_session_id == 0) {
        return {};
    }

    vector<uint8_t> token_data;
    DataWriter writer {token_data};
    writer.WriteString("FOnline.ContentUpdate.SourceReport.v1");
    writer.Write<uint64_t>(feedback_session_id);
    writer.Write<uint64_t>(catalog.Generation);
    writer.Write<uint32_t>(file_id);
    writer.WriteString(source.Provider);
    writer.WriteString(source.SourceKey);
    writer.WriteString(source.Transport);
    writer.WriteString(source.Locator);
    writer.Write<int32_t>(source.Priority);
    writer.Write<int64_t>(source.ExpiresAt);

    const Sha256Digest digest = ComputeHmacSha256(_sourceFeedbackSecret, token_data);
    ContentUpdateSourceReportToken token {};
    MemCopy(token.data(), digest.data(), token.size());
    return token;
}

auto UpdaterBackend::ReportContentUpdateSourceResult(uint64_t generation, uint32_t file_id, const ContentUpdateSourceReportToken& report_token, uint64_t feedback_session_id, ContentUpdateSourceResult result, uint64_t reporter_id, int64_t current_synchronized_time_ms, const ContentUpdateSourceFeedbackPolicy& policy) -> ContentUpdateSourceFeedbackDecision
{
    FO_STACK_TRACE_ENTRY();

    if (!policy.Enabled || policy.MinReports < 2 || policy.FailurePercent == 0 || policy.FailurePercent > 100 || policy.WindowMilliseconds <= 0 || feedback_session_id == 0 || IsContentUpdateSourceReportTokenEmpty(report_token) || current_synchronized_time_ms < 0 || !IsValidContentUpdateSourceResult(result)) {
        return ContentUpdateSourceFeedbackDecision::Ignored;
    }

    scoped_lock locker {_catalogLocker};

    if (!_catalog || _catalog->Generation != generation || file_id >= _catalog->UpdateFileSources.size()) {
        return ContentUpdateSourceFeedbackDecision::Ignored;
    }

    const auto& sources = *_catalog->UpdateFileSources[file_id];
    const auto source_it = std::ranges::find_if(sources, [this, feedback_session_id, file_id, &report_token](const ContentUpdateSource& source) { return MakeSourceReportToken(feedback_session_id, *_catalog, file_id, source) == report_token; });

    if (source_it == sources.end()) {
        return ContentUpdateSourceFeedbackDecision::Ignored;
    }

    if (source_it->ExpiresAt != 0 && source_it->ExpiresAt <= current_synchronized_time_ms) {
        return ContentUpdateSourceFeedbackDecision::Ignored;
    }

    const SourceIdentity source_identity {.FileId = file_id, .Provider = source_it->Provider, .SourceKey = source_it->SourceKey};

    auto& feedback = _sourceFeedback[source_identity];

    if (feedback.ReporterFailures.empty() || current_synchronized_time_ms < feedback.WindowStart || current_synchronized_time_ms - feedback.WindowStart >= policy.WindowMilliseconds) {
        feedback.WindowStart = current_synchronized_time_ms;
        feedback.ReporterFailures.clear();
        feedback.ThresholdLogged = false;
    }

    const bool failure = IsContentUpdateSourceFailure(result);
    const auto reporter_it = feedback.ReporterFailures.find(reporter_id);

    if (reporter_it == feedback.ReporterFailures.end()) {
        if (feedback.ReporterFailures.size() >= MaxContentUpdateSourceFeedbackReporters) {
            return ContentUpdateSourceFeedbackDecision::Ignored;
        }

        feedback.ReporterFailures.emplace(reporter_id, failure);
    }
    else {
        reporter_it->second = failure;
    }

    const size_t total_reports = feedback.ReporterFailures.size();
    const size_t failure_reports = numeric_cast<size_t>(std::ranges::count_if(feedback.ReporterFailures, [](const auto& entry) { return entry.second; }));

    if (total_reports < policy.MinReports || numeric_cast<uint64_t>(failure_reports) * 100 < numeric_cast<uint64_t>(total_reports) * policy.FailurePercent) {
        return ContentUpdateSourceFeedbackDecision::Recorded;
    }

    if (!feedback.ThresholdLogged) {
        feedback.ThresholdLogged = true;
        WriteLog(LogType::Warning, "Content update source '{}'/'{}' file {} reached {}/{} distinct-client failures; reports are advisory and clients already fell back locally, so no provider-wide suppression was applied", source_identity.Provider, source_identity.SourceKey, source_identity.FileId, failure_reports, total_reports);
    }

    return ContentUpdateSourceFeedbackDecision::Recorded;
}

auto UpdaterBackend::GetCatalogSnapshot() const noexcept -> shared_ptr<const CatalogState>
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_catalogLocker};
    return _catalog;
}

auto UpdaterBackend::BuildDescriptorSnapshot(const CatalogState& catalog, nptr<const vector<UpdateFileInfo>> platform_files) const -> shared_ptr<const vector<uint8_t>>
{
    FO_STACK_TRACE_ENTRY();

    ContentUpdateManifest manifest {};
    manifest.CatalogGeneration = catalog.Generation;
    manifest.FastUpdateEnabled = catalog.FastUpdateEnabled;
    manifest.SelfHostedServerEnabled = catalog.SelfHostedFastUpdateEnabled;
    manifest.SessionId = catalog.FastUpdateSessionId;
    manifest.ChunkSize = catalog.FastUpdateChunkSize;

    if (catalog.FastUpdateEnabled) {
        manifest.Endpoints = catalog.FastUpdateEndpoints;
    }

    const auto add_file_info = [&catalog, &manifest](const UpdateFileInfo& info) {
        const auto& data = catalog.UpdateFiles[info.FileIndex];
        ContentUpdateFileInfo file_info {};
        file_info.FileIndex = info.FileIndex;
        file_info.Name = info.ClientPath;
        file_info.Size = data->Size;
        file_info.Hash = data->Hash;
        file_info.Sha256 = data->Sha256;
        file_info.Target = info.Target;

        for (const auto& source : *catalog.UpdateFileSources[info.FileIndex]) {
            file_info.Sources.emplace_back(source);
        }

        if (catalog.FastUpdateEnabled) {
            file_info.ChunkHashes = data->ChunkHashes;
        }

        manifest.Files.emplace_back(std::move(file_info));
    };

    for (const auto& info : catalog.CommonUpdateFiles) {
        add_file_info(info);
    }

    if (platform_files) {
        for (const auto& info : *platform_files) {
            add_file_info(info);
        }
    }

    auto desc = SafeAlloc::MakeShared<vector<uint8_t>>();
    SerializeContentUpdateManifest(manifest, *desc);
    shared_ptr<const vector<uint8_t>> immutable_desc = std::move(desc);
    return immutable_desc;
}

auto UpdaterBackend::BuildSignedDescriptorSnapshot(const CatalogState& catalog, const shared_ptr<const vector<uint8_t>>& descriptor, string_view binary_target_name) const -> shared_ptr<const vector<uint8_t>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(descriptor, "Missing raw content update descriptor while building signed snapshot", binary_target_name);

    if (!catalog.ManifestSignatureRequired) {
        return descriptor;
    }

    FO_VERIFY_AND_THROW(catalog.ManifestSigningKey.has_value(), "Signed content update catalog has no signing key");
    auto signed_descriptor = SafeAlloc::MakeShared<vector<uint8_t>>();
    SignContentUpdateManifestDescriptor(*descriptor, binary_target_name, catalog.ManifestReleaseSequence, *catalog.ManifestSigningKey, *signed_descriptor);
    shared_ptr<const vector<uint8_t>> immutable_descriptor = std::move(signed_descriptor);
    return immutable_descriptor;
}

void UpdaterBackend::BuildDescriptorSnapshots(CatalogState& catalog) const
{
    FO_STACK_TRACE_ENTRY();

    catalog.CommonUpdateFilesDesc.reset();
    catalog.CommonSignedUpdateFilesDesc.reset();
    catalog.BinaryTargetUpdateFilesDesc.clear();
    catalog.BinaryTargetSignedUpdateFilesDesc.clear();

    BuildCommonDescriptorSnapshot(catalog);

    for (const auto& [binary_target_name, files] : catalog.BinaryTargetUpdateFiles) {
        ignore_unused(files);
        BuildBinaryTargetDescriptorSnapshot(catalog, binary_target_name);
    }

    RecalculateNextSourceExpiry(catalog);
}

void UpdaterBackend::BuildAffectedDescriptorSnapshots(CatalogState& catalog, const vector<uint32_t>& affected_file_ids) const
{
    FO_STACK_TRACE_ENTRY();

    bool rebuild_all_descriptors = false;
    set<string> affected_binary_targets;

    for (const uint32_t file_id : affected_file_ids) {
        FO_STRONG_ASSERT(file_id < catalog.Artifacts.size(), "Affected content update file id is outside the catalog");
        const auto binary_targets = catalog.Artifacts[file_id]->GetBinaryTargets();

        if (binary_targets.empty()) {
            rebuild_all_descriptors = true;
            break;
        }

        affected_binary_targets.insert(binary_targets.begin(), binary_targets.end());
    }

    if (rebuild_all_descriptors) {
        BuildDescriptorSnapshots(catalog);
        return;
    }

    for (const auto& binary_target_name : affected_binary_targets) {
        BuildBinaryTargetDescriptorSnapshot(catalog, binary_target_name);
    }

    RecalculateNextSourceExpiry(catalog);
}

void UpdaterBackend::BuildCommonDescriptorSnapshot(CatalogState& catalog) const
{
    FO_STACK_TRACE_ENTRY();

    const auto descriptor = BuildDescriptorSnapshot(catalog, nullptr);
    const auto signed_descriptor = BuildSignedDescriptorSnapshot(catalog, descriptor, "");
    catalog.CommonUpdateFilesDesc = descriptor;
    catalog.CommonSignedUpdateFilesDesc = signed_descriptor;
}

void UpdaterBackend::BuildBinaryTargetDescriptorSnapshot(CatalogState& catalog, string_view binary_target_name) const
{
    FO_STACK_TRACE_ENTRY();

    const auto files_it = catalog.BinaryTargetUpdateFiles.find(string(binary_target_name));
    FO_STRONG_ASSERT(files_it != catalog.BinaryTargetUpdateFiles.end(), "Missing content update binary target while rebuilding descriptor", binary_target_name);
    const auto descriptor = BuildDescriptorSnapshot(catalog, &files_it->second);
    const auto signed_descriptor = BuildSignedDescriptorSnapshot(catalog, descriptor, binary_target_name);
    catalog.BinaryTargetUpdateFilesDesc[string(binary_target_name)] = descriptor;
    catalog.BinaryTargetSignedUpdateFilesDesc[string(binary_target_name)] = signed_descriptor;
}

void UpdaterBackend::RecalculateNextSourceExpiry(CatalogState& catalog) const
{
    FO_STACK_TRACE_ENTRY();

    catalog.NextSourceExpiry = 0;
    catalog.HasSources = false;

    for (const auto& sources : catalog.UpdateFileSources) {
        catalog.HasSources = catalog.HasSources || !sources->empty();

        for (const auto& source : *sources) {
            if (source.ExpiresAt != 0 && (catalog.NextSourceExpiry == 0 || source.ExpiresAt < catalog.NextSourceExpiry)) {
                catalog.NextSourceExpiry = source.ExpiresAt;
            }
        }
    }
}

void UpdaterBackend::PruneExpiredSourcesLocked(int64_t current_synchronized_time_ms)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_catalog, "Missing content update catalog while pruning sources");

    auto next_catalog = SafeAlloc::MakeShared<CatalogState>(*_catalog);
    bool changed = false;
    vector<uint32_t> affected_file_ids;
    vector<SourceIdentity> feedback_to_erase;

    for (size_t file_id = 0; file_id < next_catalog->UpdateFileSources.size(); file_id++) {
        const auto& current_sources = *next_catalog->UpdateFileSources[file_id];
        auto next_sources = SafeAlloc::MakeShared<vector<ContentUpdateSource>>(current_sources);

        for (const auto& source : current_sources) {
            if (source.ExpiresAt != 0 && source.ExpiresAt <= current_synchronized_time_ms) {
                const SourceIdentity source_identity {.FileId = numeric_cast<uint32_t>(file_id), .Provider = source.Provider, .SourceKey = source.SourceKey};
                feedback_to_erase.emplace_back(source_identity);
            }
        }

        std::erase_if(*next_sources, [current_synchronized_time_ms](const ContentUpdateSource& source) { return source.ExpiresAt != 0 && source.ExpiresAt <= current_synchronized_time_ms; });

        if (next_sources->size() != current_sources.size()) {
            next_catalog->UpdateFileSources[file_id] = std::move(next_sources);
            affected_file_ids.emplace_back(numeric_cast<uint32_t>(file_id));
            changed = true;
        }
    }

    if (changed) {
        BuildAffectedDescriptorSnapshots(*next_catalog, affected_file_ids);
        _catalog = std::move(next_catalog);

        for (const auto& source_identity : feedback_to_erase) {
            _sourceFeedback.erase(source_identity);
        }
    }
    else {
        FO_STRONG_ASSERT(_catalog->NextSourceExpiry == 0 || _catalog->NextSourceExpiry > current_synchronized_time_ms, "Content update source expiry index is inconsistent");
    }
}

void UpdaterBackend::ProcessContentUpdateSourceReport(ptr<Player> player, int64_t current_synchronized_time_ms, const ServerSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    auto connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();
    const uint64_t generation = in_buf->Read<uint64_t>();
    const uint32_t file_id = in_buf->Read<uint32_t>();
    ContentUpdateSourceReportToken report_token {};
    in_buf->Pop(report_token.data(), report_token.size());
    const auto result = in_buf->Read<ContentUpdateSourceResult>();
    in_buf.Unlock();

    if (!IsValidContentUpdateSourceResult(result)) {
        WriteLog(LogType::Warning, "Invalid content update source result {} from client host '{}'", result, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    const uint64_t feedback_session_id = connection->GetContentUpdateFeedbackSessionId();

    if (!connection->ConsumeContentUpdateSourceReportToken(report_token)) {
        return;
    }

    const int32_t min_reports = std::clamp(settings.UpdateSourceFeedbackMinReports, 2, 64);
    const int32_t failure_percent = std::clamp(settings.UpdateSourceFeedbackFailurePercent, 1, 100);
    const int32_t window_seconds = std::clamp(settings.UpdateSourceFeedbackWindowSeconds, 1, 24 * 60 * 60);
    const ContentUpdateSourceFeedbackPolicy policy {
        .Enabled = settings.UpdateSourceFeedbackEnabled,
        .MinReports = numeric_cast<uint32_t>(min_reports),
        .FailurePercent = numeric_cast<uint32_t>(failure_percent),
        .WindowMilliseconds = numeric_cast<int64_t>(window_seconds) * 1000,
    };
    const string_view reporter_host = connection->GetHost();
    const uint64_t reporter_id = hashing_ex::hash(reporter_host.data(), reporter_host.size());
    (void)ReportContentUpdateSourceResult(generation, file_id, report_token, feedback_session_id, result, reporter_id, current_synchronized_time_ms, policy);
}

void UpdaterBackend::ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size)
{
    FO_STACK_TRACE_ENTRY();

    const auto catalog = GetCatalogSnapshot();
    auto connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    auto file_index = in_buf->Read<uint32_t>();
    auto start_offset = in_buf->Read<uint64_t>();

    in_buf.Unlock();

    if (!catalog || file_index >= catalog->UpdateFiles.size()) {
        WriteLog(LogType::Warning, "Wrong file index {}, from host '{}'", file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (update_file_max_portion_size <= 0) {
        WriteLog(LogType::Warning, "Wrong update file max portion size {}, client host '{}'", update_file_max_portion_size, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    const auto& update_file = catalog->UpdateFiles[file_index];
    const uint64_t file_size = update_file->Size;

    if (start_offset > file_size) {
        WriteLog(LogType::Warning, "Wrong update file offset {}, file index {}, client host '{}'", start_offset, file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    uint64_t update_portion_limit = numeric_cast<uint64_t>(update_file_max_portion_size);
    uint64_t remaining_size = file_size - start_offset;
    uint64_t update_portion = std::min(update_portion_limit, remaining_size);
    size_t update_portion_size = numeric_cast<size_t>(update_portion);

    // Disk-backed files are read into a temporary buffer; in-memory packs are pushed directly to avoid a per-portion copy
    vector<uint8_t> disk_update_data;

    if (update_portion_size != 0 && !update_file->MemoryData) {
        disk_update_data.resize(update_portion_size);

        if (!ReadUpdateFileData(*update_file, start_offset, disk_update_data)) {
            WriteLog(LogType::Warning, "Can't read update file '{}', file index {}, client host '{}'", update_file->DiskStorage->GetDiskPath(), file_index, connection->GetHost());
            connection->HardDisconnect();
            return;
        }
    }

    const_span<uint8_t> update_data {};

    if (update_portion_size != 0) {
        if (update_file->MemoryData) {
            const size_t offset = numeric_cast<size_t>(start_offset);
            FO_STRONG_ASSERT(offset < update_file->MemoryData->size(), "Byte offset is past the end of the update data buffer");
            update_data = {update_file->MemoryData->data() + offset, update_portion_size};
        }
        else {
            update_data = {disk_update_data.data(), update_portion_size};
        }
    }

    player->Send_UpdateFileData(update_data);
}

auto UpdaterBackend::ReadFastUpdateChunk(uint32_t file_index, uint32_t chunk_index, vector<uint8_t>& data, uint64_t& chunk_hash) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    data.clear();
    chunk_hash = 0;

    const auto catalog = GetCatalogSnapshot();

    if (!catalog || !catalog->FastUpdateEnabled || catalog->FastUpdateChunkSize == 0 || file_index >= catalog->UpdateFiles.size()) {
        return false;
    }

    const auto& update_file = catalog->UpdateFiles[file_index];

    if (chunk_index >= update_file->ChunkHashes.size()) {
        return false;
    }

    const uint32_t chunk_size = GetContentUpdateChunkSize(update_file->Size, catalog->FastUpdateChunkSize, chunk_index);

    if (chunk_size == 0) {
        return false;
    }

    data.resize(chunk_size);

    if (!ReadUpdateFileData(*update_file, GetContentUpdateChunkOffset(catalog->FastUpdateChunkSize, chunk_index), data)) {
        data.clear();
        return false;
    }

    chunk_hash = update_file->ChunkHashes[chunk_index];
    return true;
}

FO_END_NAMESPACE
