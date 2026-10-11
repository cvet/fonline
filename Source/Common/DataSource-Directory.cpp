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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "DataSource-Directory.h"

FO_BEGIN_NAMESPACE

class DummySpace final : public DataSource
{
public:
    DummySpace() = default;
    DummySpace(const DummySpace&) = delete;
    DummySpace(DummySpace&&) noexcept = delete;
    auto operator=(const DummySpace&) = delete;
    auto operator=(DummySpace&&) noexcept = delete;
    ~DummySpace() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Dummy"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override { return vector<IndexedFile> {}; }
};

class NonCachedDir final : public DataSource
{
public:
    explicit NonCachedDir(string_view fname, bool recursive);
    NonCachedDir(const NonCachedDir&) = delete;
    NonCachedDir(NonCachedDir&&) noexcept = delete;
    auto operator=(const NonCachedDir&) = delete;
    auto operator=(NonCachedDir&&) noexcept = delete;
    ~NonCachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _baseDir; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    bool _recursive;
    string _baseDir {};
};

class CachedDir final : public DataSource
{
public:
    CachedDir(string_view fname, bool recursive);
    CachedDir(const CachedDir&) = delete;
    CachedDir(CachedDir&&) noexcept = delete;
    auto operator=(const CachedDir&) = delete;
    auto operator=(CachedDir&&) noexcept = delete;
    ~CachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _baseDir; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

    auto Reindex() -> bool override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64_t WriteTime {};
    };

    unordered_map<string, FileEntry> _filesTree {};
    vector<string> _filesTreeNames {};
    string _baseDir {};
    bool _recursive {};
};

auto DataSourceDirectory::Mount(string_view dir, bool recursive, bool non_cached, bool maybe_not_available) -> unique_ptr<DataSource>
{
    FO_TRACE_ZONE(FileSystem);

    if (!fs::is_dir(dir)) {
        if (maybe_not_available) {
            return safe_alloc::make_unique<DummySpace>();
        }

        throw DataSourceException("Directory not found", dir);
    }

    if (non_cached) {
        return safe_alloc::make_unique<NonCachedDir>(dir, recursive);
    }
    else {
        return safe_alloc::make_unique<CachedDir>(dir, recursive);
    }
}

auto DummySpace::IsFileExists(string_view path) const -> bool
{
    ignore_unused(path);
    return false;
}

auto DummySpace::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    ignore_unused(path, size, write_time);
    return false;
}

auto DummySpace::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    ignore_unused(path, size, write_time);
    return nullptr;
}

auto DummySpace::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    ignore_unused(dir, recursive, ext);
    return {};
}

NonCachedDir::NonCachedDir(string_view fname, bool recursive) :
    _recursive {recursive}
{
    _baseDir = fname;
    _baseDir = fs::resolve_path(_baseDir);
    _baseDir += "/";
}

auto NonCachedDir::IsFileExists(string_view path) const -> bool
{
    if (!_recursive && !strex(path).extract_dir().empty()) {
        return false;
    }

    string full_path = strex(_baseDir).combine_path(path);

    if (!fs::exists(full_path)) {
        return false;
    }
    if (fs::is_dir(full_path)) {
        return false;
    }

    return true;
}

auto NonCachedDir::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    if (!_recursive && !strex(path).extract_dir().empty()) {
        return false;
    }

    string full_path = strex(_baseDir).combine_path(path);
    auto file = fs::open_ifstream(full_path);

    if (!file) {
        return false;
    }

    size = fs::stream_get_size(file);
    write_time = fs::last_write_time(full_path);
    return true;
}

auto NonCachedDir::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    if (!_recursive && !strex(path).extract_dir().empty()) {
        return nullptr;
    }

    string full_path = strex(_baseDir).combine_path(path);
    auto file = fs::open_ifstream(full_path);

    if (!file) {
        return nullptr;
    }

    size = fs::stream_get_size(file);
    auto buf = safe_alloc::make_unique_arr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (!fs::stream_read_exact(file, make_span(buf_data, size))) {
        throw DataSourceException("Can't read file from non cached dir", _baseDir, path);
    }

    write_time = fs::last_write_time(full_path);
    return MakeFileBufferHolder(std::move(buf));
}

auto NonCachedDir::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_TRACE_ZONE(FileSystem);

    if (!_recursive && !dir.empty()) {
        return {};
    }

    string full_dir = strex(_baseDir).combine_path(dir).str();

    if (!dir.empty() && !fs::is_dir(full_dir)) {
        return {};
    }

    vector<string> fnames;

    fs::iterate_dir(full_dir, recursive && _recursive, [&fnames](string_view path2, size_t size, uint64_t write_time) {
        ignore_unused(size, write_time);
        fnames.emplace_back(path2);
    });

    return GetFileNamesGeneric(fnames, dir, recursive, ext);
}

CachedDir::CachedDir(string_view fname, bool recursive) :
    _recursive {recursive}
{
    _baseDir = fname;
    _baseDir = fs::resolve_path(_baseDir);
    _baseDir += "/";

    Reindex();
}

auto CachedDir::Reindex() -> bool
{
    FO_TRACE_ZONE(FileSystem);

    unordered_map<string, FileEntry> files_tree;
    vector<string> files_tree_names;

    fs::iterate_dir(_baseDir, _recursive, [&](string_view path, size_t size, uint64_t write_time) {
        FileEntry fe;
        fe.FileName = strex("{}{}", _baseDir, path);
        fe.FileSize = size;
        fe.WriteTime = write_time;

        files_tree.emplace(path, std::move(fe));
        files_tree_names.emplace_back(path);
    });

    bool changed = files_tree.size() != _filesTree.size() || std::ranges::any_of(files_tree, [this](const auto& entry) {
        auto it = _filesTree.find(entry.first);
        return it == _filesTree.end() || it->second.FileSize != entry.second.FileSize || it->second.WriteTime != entry.second.WriteTime;
    });

    _filesTree = std::move(files_tree);
    _filesTreeNames = std::move(files_tree_names);
    return changed;
}

auto CachedDir::IsFileExists(string_view path) const -> bool
{
    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto CachedDir::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

auto CachedDir::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = fs::open_ifstream(fe.FileName);

    if (!file) {
        throw DataSourceException("Can't read file from cached dir", _baseDir, path);
    }

    size = fe.FileSize;
    auto buf = safe_alloc::make_unique_arr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (!fs::stream_read_exact(file, make_span(buf_data, size))) {
        return nullptr;
    }

    write_time = fe.WriteTime;
    return MakeFileBufferHolder(std::move(buf));
}

auto CachedDir::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext);
}

auto DataSourceDirectory::CreateEmpty() -> unique_ptr<DataSource>
{
    return safe_alloc::make_unique<DummySpace>();
}

FO_END_NAMESPACE
