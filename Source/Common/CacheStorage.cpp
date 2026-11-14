// FOnline Engine
// SPDX-License-Identifier: MIT

#include "CacheStorage.h"

FO_BEGIN_NAMESPACE

class CacheStorageImpl
{
public:
    CacheStorageImpl() = default;
    CacheStorageImpl(const CacheStorageImpl&) = delete;
    CacheStorageImpl(CacheStorageImpl&&) noexcept = delete;
    auto operator=(const CacheStorageImpl&) = delete;
    auto operator=(CacheStorageImpl&&) noexcept = delete;
    virtual ~CacheStorageImpl() = default;

    [[nodiscard]] virtual auto HasEntry(string_view entry_name) const -> bool = 0;
    [[nodiscard]] virtual auto GetString(string_view entry_name) const -> string = 0;
    [[nodiscard]] virtual auto GetData(string_view entry_name) const -> vector<uint8_t> = 0;
    [[nodiscard]] virtual auto GetDataBounded(string_view entry_name, size_t max_size) const -> CacheStorageReadResult = 0;

    virtual void SetString(string_view entry_name, string_view str) = 0;
    virtual void SetData(string_view entry_name, const_span<uint8_t> data) = 0;
    virtual auto SetDataChecked(string_view entry_name, const_span<uint8_t> data) -> bool = 0;
    virtual void RemoveEntry(string_view entry_name) = 0;
};

class FileCacheStorage final : public CacheStorageImpl
{
public:
    explicit FileCacheStorage(string_view real_path);
    FileCacheStorage(const FileCacheStorage&) = delete;
    FileCacheStorage(FileCacheStorage&&) noexcept = delete;
    auto operator=(const FileCacheStorage&) = delete;
    auto operator=(FileCacheStorage&&) noexcept = delete;
    ~FileCacheStorage() override = default;

    [[nodiscard]] auto HasEntry(string_view entry_name) const -> bool override;
    [[nodiscard]] auto GetString(string_view entry_name) const -> string override;
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8_t> override;
    [[nodiscard]] auto GetDataBounded(string_view entry_name, size_t max_size) const -> CacheStorageReadResult override;

    auto CreateCacheStorage() const -> bool;
    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, const_span<uint8_t> data) override;
    auto SetDataChecked(string_view entry_name, const_span<uint8_t> data) -> bool override;
    void RemoveEntry(string_view entry_name) override;

private:
    auto MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string;

    string _workPath {};
};

CacheStorage::CacheStorage(string_view path) :
    _impl {safe_alloc::make_unique<FileCacheStorage>(path)}
{
}

CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;
CacheStorage::~CacheStorage() = default;

auto CacheStorage::HasEntry(string_view entry_name) const -> bool
{
    return _impl->HasEntry(entry_name);
}

auto CacheStorage::GetString(string_view entry_name) const -> string
{
    return _impl->GetString(entry_name);
}

auto CacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    return _impl->GetData(entry_name);
}

auto CacheStorage::GetDataBounded(string_view entry_name, size_t max_size) const -> CacheStorageReadResult
{
    return _impl->GetDataBounded(entry_name, max_size);
}

void CacheStorage::SetString(string_view entry_name, string_view str)
{
    _impl->SetString(entry_name, str);
}

void CacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
{
    _impl->SetData(entry_name, data);
}

auto CacheStorage::SetDataChecked(string_view entry_name, const_span<uint8_t> data) -> bool
{
    return _impl->SetDataChecked(entry_name, data);
}

void CacheStorage::RemoveEntry(string_view entry_name)
{
    _impl->RemoveEntry(entry_name);
}

auto FileCacheStorage::MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string
{
    return strex(work_path).combine_path(strex(data_name).replace('/', '_').replace('\\', '_'));
}

FileCacheStorage::FileCacheStorage(string_view real_path)
{
    _workPath = fs::resolve_path(real_path);
}

auto FileCacheStorage::CreateCacheStorage() const -> bool
{
    if (!fs::is_dir(_workPath)) {
        (void)fs::create_directories(_workPath);

        if (!fs::is_dir(_workPath)) {
            logging::write(logging::type::warning, "Can't create dir for cache '{}'", _workPath);
            return false;
        }
    }

    return true;
}

auto FileCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    string path {MakeCacheEntryPath(_workPath, entry_name)};
    return fs::exists(path);
}

auto FileCacheStorage::GetString(string_view entry_name) const -> string
{
    FO_TRACE_ZONE(FileSystem);

    string path {MakeCacheEntryPath(_workPath, entry_name)};
    auto str {fs::read_file(path)};

    if (!str) {
        return {};
    }

    return *str;
}

auto FileCacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    string path {MakeCacheEntryPath(_workPath, entry_name)};
    auto data {fs::read_file(path)};

    if (!data) {
        return {};
    }

    return vector<uint8_t>(data->begin(), data->end());
}

auto FileCacheStorage::GetDataBounded(string_view entry_name, size_t max_size) const -> CacheStorageReadResult
{
    FO_TRACE_ZONE(FileSystem);

    string path {MakeCacheEntryPath(_workPath, entry_name)};
    auto file_size {fs::file_size(path)};

    if (!file_size.has_value()) {
        return {.Status = fs::exists(path) ? CacheStorageReadStatus::Failed : CacheStorageReadStatus::Missing};
    }
    if (file_size.value() > max_size) {
        return {.Status = CacheStorageReadStatus::TooLarge};
    }

    auto data {fs::read_file_bounded(path, max_size)};

    if (!data.has_value()) {
        return {.Status = CacheStorageReadStatus::Failed};
    }

    return {.Status = CacheStorageReadStatus::Success, .Data = vector<uint8_t>(data->begin(), data->end())};
}

void FileCacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_TRACE_ZONE(FileSystem);

    if (!CreateCacheStorage()) {
        return;
    }

    string path {MakeCacheEntryPath(_workPath, entry_name)};

    if (!fs::write_file(path, str)) {
        logging::write(logging::type::warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
{
    (void)SetDataChecked(entry_name, data);
}

auto FileCacheStorage::SetDataChecked(string_view entry_name, const_span<uint8_t> data) -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (!CreateCacheStorage()) {
        return false;
    }

    string path {MakeCacheEntryPath(_workPath, entry_name)};

    if (!fs::write_file(path, data)) {
        logging::write(logging::type::warning, "Can't write cache at '{}'", path);
        return false;
    }

    return true;
}

void FileCacheStorage::RemoveEntry(string_view entry_name)
{
    FO_TRACE_ZONE(FileSystem);

    string path {MakeCacheEntryPath(_workPath, entry_name)};
    (void)fs::remove_file(path);
}

FO_END_NAMESPACE
