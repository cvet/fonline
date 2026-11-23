// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DataSource.h"

FO_BEGIN_NAMESPACE

struct ClientSettings;
class FileSystem;

auto GetClientPackDirs(const ClientSettings& settings) -> vector<string>;
auto GetClientWritableResourceDir(const ClientSettings& settings) -> string;
auto GetClientWritableResourceDir(string_view writable_root, string_view resources_dir) -> string;
auto GetClientResourcePackPath(const ClientSettings& settings, string_view pack_name) -> string;
auto GetClientResourcePatchPath(const ClientSettings& settings, string_view pack_name) -> string;
auto IsClientResourcePackCurrent(const ClientSettings& settings, string_view pack_name, uint64_t content_hash) -> bool;
void AddClientPackSource(FileSystem& resources, const ClientSettings& settings, string_view pack_name, bool optional = false);
void AddClientPackSource(FileSystem& resources, string_view resources_dir, string_view writable_root, string_view pack_name, bool optional = false);

FO_DECLARE_EXCEPTION(FileSystemExeption);

class FileHeader
{
public:
    FileHeader() noexcept = default;
    explicit FileHeader(string_view path, size_t size, uint64_t write_time, ptr<const DataSource> ds);
    FileHeader(const FileHeader&) = delete;
    FileHeader(FileHeader&&) noexcept = default;
    auto operator=(const FileHeader&) = delete;
    auto operator=(FileHeader&&) noexcept -> FileHeader& = default;
    ~FileHeader() = default;

    [[nodiscard]] explicit operator bool() const noexcept { return _isLoaded; }
    [[nodiscard]] auto GetNameNoExt() const -> string_view;
    [[nodiscard]] auto GetPath() const -> string_view;
    [[nodiscard]] auto GetDiskPath() const -> string;
    [[nodiscard]] auto GetSize() const -> size_t;
    [[nodiscard]] auto GetWriteTime() const -> uint64_t;
    [[nodiscard]] auto GetDataSource() const -> ptr<const DataSource>;

    auto Copy() const -> FileHeader;

protected:
    bool _isLoaded {};
    string _filePath {};
    size_t _fileSize {};
    uint64_t _writeTime {};
    nptr<const DataSource> _dataSource {};
};

class FileReader final
{
public:
    explicit FileReader(const_span<uint8_t> buf);
    FileReader(const FileReader&) = delete;
    FileReader(FileReader&&) noexcept = default;
    auto operator=(const FileReader&) = delete;
    auto operator=(FileReader&&) noexcept -> FileReader& = default;
    ~FileReader() = default;

    [[nodiscard]] auto GetStr() const -> string;
    [[nodiscard]] auto GetData() const -> vector<uint8_t>;
    [[nodiscard]] auto GetDataSpan() const -> const_span<uint8_t>;
    [[nodiscard]] auto GetSize() const -> size_t;
    [[nodiscard]] auto GetCurDataSpan(size_t size) const -> const_span<uint8_t>;
    [[nodiscard]] auto GetCurPos() const -> size_t;

    auto ReadStrNT() -> string; // Null terminated
    auto ReadUInt8() -> uint8_t;
    auto ReadInt8() -> int8_t { return std::bit_cast<int8_t>(ReadUInt8()); }
    auto ReadBEUInt16() -> uint16_t;
    auto ReadBEInt16() -> int16_t { return std::bit_cast<int16_t>(ReadBEUInt16()); }
    auto ReadLEUInt16() -> uint16_t;
    auto ReadLEInt16() -> int16_t { return std::bit_cast<int16_t>(ReadLEUInt16()); }
    auto ReadBEUInt32() -> uint32_t;
    auto ReadBEInt32() -> int32_t { return std::bit_cast<int32_t>(ReadBEUInt32()); }
    auto ReadLEUInt32() -> uint32_t;
    auto ReadLEInt32() -> int32_t { return std::bit_cast<int32_t>(ReadLEUInt32()); }

    auto SeekFragment(string_view fragment) -> bool;
    void CopyData(span<uint8_t> buf);
    void ReadBytes(span<uint8_t> out);
    void SetCurPos(size_t pos);
    void GoForward(size_t offs);
    void GoBack(size_t offs);

    template<typename T>
        requires(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>)
    void ReadObject(T& out)
    {
        CopyData(make_span(&out, sizeof(T)));
    }

    template<typename T>
        requires(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>)
    void ReadObjectArray(span<T> out)
    {
        if (!out.empty()) {
            CopyData(make_span(out.data(), out.size() * sizeof(T)));
        }
    }

private:
    const_span<uint8_t> _buf;
    size_t _curPos {};
};

class File final : public FileHeader
{
public:
    File() noexcept = default;
    explicit File(string_view path, size_t size, uint64_t write_time, ptr<const DataSource> ds, unique_del_ptr<const uint8_t>&& buf);
    File(const File&) = delete;
    File(File&&) noexcept = default;
    auto operator=(const File&) = delete;
    auto operator=(File&&) noexcept -> File& = default;
    ~File() = default;

    [[nodiscard]] auto GetStr() const -> string;
    [[nodiscard]] auto GetData() const -> vector<uint8_t>;
    [[nodiscard]] auto GetDataSpan() const -> const_span<uint8_t>;
    [[nodiscard]] auto GetReader() const -> FileReader;

    static auto Load(const FileHeader& fh) -> File;

protected:
    unique_del_nptr<const uint8_t> _fileBuf {};
};

class FileCollection final
{
public:
    explicit FileCollection(initializer_list<FileHeader> files);
    explicit FileCollection(vector<FileHeader> files);
    FileCollection(const FileCollection&) = delete;
    FileCollection(FileCollection&&) noexcept = default;
    auto operator=(const FileCollection&) = delete;
    auto operator=(FileCollection&&) noexcept -> FileCollection& = default;
    ~FileCollection() = default;

    [[nodiscard]] auto GetFilesCount() const -> size_t;
    [[nodiscard]] auto GetFileByIndex(size_t index) const -> const FileHeader&;
    [[nodiscard]] auto FindFileByName(string_view name_no_ext) const -> File;
    [[nodiscard]] auto FindFileByPath(string_view path) const -> File;

    [[nodiscard]] auto begin() { return _files.begin(); }
    [[nodiscard]] auto end() { return _files.end(); }
    [[nodiscard]] auto begin() const { return _files.begin(); }
    [[nodiscard]] auto end() const { return _files.end(); }

private:
    vector<FileHeader> _files {};
    unordered_map<string, size_t> _nameToIndex {};
    unordered_map<string, size_t> _pathToIndex {};
};

class FileSystem final
{
public:
    FileSystem() noexcept = default;
    FileSystem(const FileSystem&) = delete;
    FileSystem(FileSystem&&) noexcept = default;
    auto operator=(const FileSystem&) = delete;
    auto operator=(FileSystem&&) noexcept = delete;
    ~FileSystem() = default;

    [[nodiscard]] auto GetAllFiles() const -> FileCollection;
    [[nodiscard]] auto FilterFiles(string_view ext, string_view dir = "", bool recursive = true) const -> FileCollection;
    [[nodiscard]] auto FilterFiles(const_span<string> include_patterns, const_span<string> exclude_patterns) const -> FileCollection;
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool;
    [[nodiscard]] auto ReadFile(string_view path) const -> File;
    [[nodiscard]] auto ReadFileText(string_view path) const -> string;
    [[nodiscard]] auto ReadFileHeader(string_view path) const -> FileHeader;

    void AddDirSource(string_view dir, bool recursive = false, bool non_cached = false, bool maybe_not_available = false);
    void AddPackSource(bool packaged, string_view dir, string_view pack, bool maybe_not_available = false);
    void AddPacksSource(bool packaged, string_view dir, const vector<string>& packs);
    void AddCustomSource(unique_ptr<DataSource> data_source);
    auto ReindexDataSources() -> bool;
    void CleanDataSources();

private:
    struct ResourceIndexEntry
    {
        nptr<const DataSource> Source {};
        size_t Size {};
        uint64_t WriteTime {};
    };

    void IndexMountedSource(ptr<const DataSource> ds);
    void RebuildIndex();

    vector<unique_ptr<DataSource>> _dataSources {};
    unordered_map<string, ResourceIndexEntry> _index {};
    bool _indexed {true};
};

FO_END_NAMESPACE
