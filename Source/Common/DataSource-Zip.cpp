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

#include "DataSource-Zip.h"

#if FO_DATA_SOURCE_ZIP

#include "minizip/unzip.h"

FO_BEGIN_NAMESPACE

class ZipFile final : public DataSource
{
public:
    explicit ZipFile(string_view fname);
    ZipFile(const ZipFile&) = delete;
    ZipFile(ZipFile&&) noexcept = delete;
    auto operator=(const ZipFile&) = delete;
    auto operator=(ZipFile&&) noexcept = delete;
    ~ZipFile() override;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;

private:
    struct ZipFileInfo
    {
        unz_file_pos Pos {};
        int32_t UncompressedSize {};
    };

    unordered_map<string, ZipFileInfo> _filesTree {};
    vector<string> _filesTreeNames {};
    string _fileName {};
    mutable mutex _zipHandleLocker {};
    mutable nptr<void> _zipHandle FO_TSA_GUARDED_BY(_zipHandleLocker) {};
    unique_ptr<std::ifstream> _fileStream;
    uint64_t _writeTime {};
};

ZipFile::ZipFile(string_view fname) :
    _fileName {fname},
    _fileStream {safe_alloc::make_unique<std::ifstream>(fs::open_ifstream(_fileName))}
{
    FO_TRACE_ZONE(FileSystem);

    scoped_lock locker {_zipHandleLocker};

    zlib_filefunc_def ffunc;

    if (!*_fileStream) {
        throw DataSourceException("Can't open zip file", _fileName);
    }

    _writeTime = fs::last_write_time(_fileName);

    ffunc.zopen_file = [](voidpf opaque, const char*, int32_t) -> voidpf {
        nptr<void> stream = opaque;
        FO_VERIFY_AND_THROW(stream, "Zip open callback received a null stream handle");
        return opaque;
    };
    ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
        auto file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(file, "Zip read callback received a null file stream");
        if (size == 0) {
            return 0;
        }
        FO_VERIFY_AND_THROW(buf != nullptr, "Zip read callback received a null output buffer");
        return fs::stream_read_exact(*file, make_span(buf, size)) ? size : 0;
    };
    ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
    ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
        auto file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(file, "Zip tell callback received a null file stream");
        return numeric_cast<long>(fs::stream_get_read_pos(*file));
    };
    ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int32_t origin) -> long {
        auto file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(file, "Zip seek callback received a null file stream");
        switch (origin) {
        case ZLIB_FILEFUNC_SEEK_SET:
            return fs::stream_set_read_pos(*file, numeric_cast<int32_t>(offset), std::ios_base::beg) ? 0 : -1;
        case ZLIB_FILEFUNC_SEEK_CUR:
            return fs::stream_set_read_pos(*file, numeric_cast<int32_t>(offset), std::ios_base::cur) ? 0 : -1;
        case ZLIB_FILEFUNC_SEEK_END:
            return fs::stream_set_read_pos(*file, numeric_cast<int32_t>(offset), std::ios_base::end) ? 0 : -1;
        default:
            return -1;
        }
    };
    ffunc.zclose_file = [](voidpf, voidpf) -> int32_t {
        // The ifstream is owned by the ZipFile object (_fileStream), not by zlib, so closing the zip must
        // not delete it. This keeps ownership RAII-safe when the constructor throws on a malformed zip
        return 0;
    };
    ffunc.zerror_file = [](voidpf, voidpf stream) -> int32_t {
        if (!stream) {
            return 1;
        }
        return 0;
    };

    ffunc.opaque = _fileStream.void_cast();

    auto zip_handle = make_nptr(unzOpen2(string(_fileName).c_str(), &ffunc));
    auto close_on_fail = scope_fail([&zip_handle]() noexcept {
        if (zip_handle) {
            unzClose(zip_handle.get());
            zip_handle = nullptr;
        }
    });

    if (!zip_handle) {
        throw DataSourceException("Can't read zip file", _fileName);
    }

    unz_global_info gi;

    int32_t global_info_result = unzGetGlobalInfo(zip_handle.get(), &gi);

    if (global_info_result != UNZ_OK) {
        throw DataSourceException("Read zip file tree failed (unzGetGlobalInfo)", _fileName, global_info_result);
    }
    if (gi.number_entry == 0) {
        throw DataSourceException("ZIP archive has no entries", _fileName);
    }

    ZipFileInfo zip_info;
    unz_file_pos pos;
    unz_file_info info;
    char name_buf[4096];

    for (uLong i = 0; i < gi.number_entry; i++) {
        int32_t file_pos_result = unzGetFilePos(zip_handle.get(), &pos);

        if (file_pos_result != UNZ_OK) {
            throw DataSourceException("Read zip file tree failed (unzGetFilePos)", _fileName, i, file_pos_result);
        }

        int32_t file_info_result = unzGetCurrentFileInfo(zip_handle.get(), &info, name_buf, sizeof(name_buf), nullptr, 0, nullptr, 0);

        if (file_info_result != UNZ_OK) {
            throw DataSourceException("Read zip file tree failed (unzGetCurrentFileInfo)", _fileName, i, file_info_result);
        }
        if (info.size_filename >= sizeof(name_buf)) {
            throw DataSourceException("ZIP file name exceeds the supported length", _fileName, i, info.size_filename, sizeof(name_buf));
        }

        if ((info.external_fa & 0x10) == 0) { // Not folder
            string name = strex(name_buf).normalize_path_slashes();

            if (info.uncompressed_size > numeric_cast<uLong>(std::numeric_limits<int32_t>::max())) {
                throw DataSourceException("ZIP entry exceeds the supported decoded size", _fileName, name, info.uncompressed_size);
            }

            zip_info.Pos = pos;
            zip_info.UncompressedSize = numeric_cast<int32_t>(info.uncompressed_size);
            _filesTree.emplace(name, zip_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        if (i + 1 < gi.number_entry) {
            int32_t next_result = unzGoToNextFile(zip_handle.get());

            if (next_result != UNZ_OK) {
                throw DataSourceException("Read zip file tree failed (unzGoToNextFile)", _fileName, i, next_result);
            }
        }
    }

    _zipHandle = std::move(zip_handle);
}

ZipFile::~ZipFile()
{
    scoped_lock locker {_zipHandleLocker};

    if (_zipHandle) {
        unzClose(_zipHandle.get());
    }
}

auto ZipFile::IsFileExists(string_view path) const -> bool
{
    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto ZipFile::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& info = it->second;
    write_time = _writeTime;
    size = info.UncompressedSize;
    return true;
}

auto ZipFile::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
{
    FO_TRACE_ZONE(FileSystem);

    vector<IndexedFile> files;
    files.reserve(_filesTree.size());

    for (const auto& [path, info] : _filesTree) {
        files.emplace_back(IndexedFile {path, numeric_cast<size_t>(info.UncompressedSize), _writeTime});
    }

    return files;
}

auto ZipFile::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    scoped_lock locker {_zipHandleLocker};

    const auto& info = it->second;
    unz_file_pos pos = info.Pos;

    int32_t file_pos_result = unzGoToFilePos(_zipHandle.get(), &pos);

    if (file_pos_result != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (unzGoToFilePos)", _fileName, path, file_pos_result);
    }

    int32_t open_result = unzOpenCurrentFile(_zipHandle.get());

    if (open_result != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (unzOpenCurrentFile)", _fileName, path, open_result);
    }

    auto buf = safe_alloc::make_unique_arr<uint8_t>(numeric_cast<size_t>(info.UncompressedSize));
    int32_t read = unzReadCurrentFile(_zipHandle.get(), buf.get(), info.UncompressedSize);
    int32_t close_result = unzCloseCurrentFile(_zipHandle.get());

    if (read != info.UncompressedSize) {
        throw DataSourceException("Can't read file from zip (unzReadCurrentFile)", _fileName, path, info.UncompressedSize, read, close_result);
    }
    if (close_result != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (unzCloseCurrentFile)", _fileName, path, close_result);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    return MakeFileBufferHolder(std::move(buf));
}

auto DataSourceZip::Mount(string_view fname) -> unique_ptr<DataSource>
{
    FO_TRACE_ZONE(FileSystem);

    return safe_alloc::make_unique<ZipFile>(fname);
}

FO_END_NAMESPACE

#endif
