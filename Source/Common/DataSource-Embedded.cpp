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

#include "DataSource-Embedded.h"

#include "EmbeddedResources.gen.inc"

#include "minizip/unzip.h"

FO_BEGIN_NAMESPACE

class EmbeddedFile final : public DataSource
{
public:
    explicit EmbeddedFile();
    EmbeddedFile(const EmbeddedFile&) = delete;
    EmbeddedFile(EmbeddedFile&&) noexcept = delete;
    auto operator=(const EmbeddedFile&) = delete;
    auto operator=(EmbeddedFile&&) noexcept = delete;
    ~EmbeddedFile() override;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return EMBEDDED_PACK_NAME; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;

private:
    struct EmbeddedFileInfo
    {
        unz_file_pos Pos {};
        int32_t UncompressedSize {};
    };

    unordered_map<string, EmbeddedFileInfo> _filesTree {};
    vector<string> _filesTreeNames {};
    mutable mutex _zipHandleLocker {};
    mutable nptr<void> _zipHandle FO_TSA_GUARDED_BY(_zipHandleLocker) {};
    uint64_t _writeTime {};
};

struct EmbeddedZipMemStream
{
    span<const volatile uint8_t> Buf;
    uint32_t Pos;
};

EmbeddedFile::EmbeddedFile()
{
    FO_TRACE_ZONE(FileSystem);

    scoped_lock locker {_zipHandleLocker};

    zlib_filefunc_def ffunc;

    static_assert(sizeof(EMBEDDED_RESOURCES) > 100);
    bool default_array = true;

    for (size_t i = 0; i < sizeof(EMBEDDED_RESOURCES) && default_array; i++) {
        if (EMBEDDED_RESOURCES[i] != numeric_cast<uint8_t>((i + 42) % 200)) {
            default_array = false;
        }
    }

    if (default_array) {
        logging::write("Embedded resources not really embed");
        return;
    }

    ffunc.zopen_file = [](voidpf, const char*, int32_t) -> voidpf {
        array<uint8_t, sizeof(uint32_t)> embedded_size_bytes {};

        for (size_t i = 0; i < embedded_size_bytes.size(); i++) {
            embedded_size_bytes[i] = EMBEDDED_RESOURCES[i];
        }

        uint32_t embedded_size = 0;

        auto embedded_size_target = make_ptr(&embedded_size).reinterpret_as<uint8_t>();
        memory::copy(embedded_size_target, embedded_size_bytes.data(), embedded_size_bytes.size());

        auto mem_stream = safe_alloc::make_unique<EmbeddedZipMemStream>(span<const volatile uint8_t> {EMBEDDED_RESOURCES + sizeof(uint32_t), numeric_cast<size_t>(embedded_size)}, 0);

        return mem_stream.release().void_cast();
    };
    ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
        auto mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(mem_stream, "Embedded zip read callback received a null memory stream");
        auto out_buf = cast_from_void<uint8_t*>(buf);

        if (size == 0) {
            return 0;
        }

        FO_VERIFY_AND_THROW(out_buf, "Embedded zip read callback received a null output buffer");
        for (size_t i = 0; i < size; i++) {
            out_buf[i] = mem_stream->Buf[mem_stream->Pos + i];
        }
        mem_stream->Pos += numeric_cast<uint32_t>(size);
        return size;
    };
    ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
    ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
        auto mem_stream = cast_from_void<const EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(mem_stream, "Embedded zip tell callback received a null memory stream");
        return numeric_cast<long>(mem_stream->Pos);
    };
    ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int32_t origin) -> long {
        auto mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(mem_stream, "Embedded zip seek callback received a null memory stream");
        switch (origin) {
        case ZLIB_FILEFUNC_SEEK_SET:
            mem_stream->Pos = numeric_cast<uint32_t>(offset);
            break;
        case ZLIB_FILEFUNC_SEEK_CUR:
            mem_stream->Pos += numeric_cast<uint32_t>(offset);
            break;
        case ZLIB_FILEFUNC_SEEK_END:
            mem_stream->Pos = numeric_cast<uint32_t>(mem_stream->Buf.size()) + numeric_cast<uint32_t>(offset);
            break;
        default:
            return -1;
        }
        return 0;
    };
    ffunc.zclose_file = [](voidpf, voidpf stream) -> int32_t {
        auto mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(mem_stream, "Embedded zip close callback received a null memory stream");

        auto owned_mem_stream = adopt_unique_ptr(mem_stream);
        ignore_unused(owned_mem_stream);
        return 0;
    };
    ffunc.zerror_file = [](voidpf, voidpf stream) -> int32_t {
        if (!stream) {
            return 1;
        }
        return 0;
    };
    ffunc.opaque = nullptr;

    auto zip_handle = make_nptr(unzOpen2("", &ffunc));
    auto close_on_fail = scope_fail([&zip_handle]() noexcept {
        if (zip_handle) {
            unzClose(zip_handle.get());
            zip_handle = nullptr;
        }
    });

    if (!zip_handle) {
        throw DataSourceException("Can't read embedded ZIP archive", EMBEDDED_PACK_NAME);
    }

    unz_global_info gi;

    int32_t global_info_result = unzGetGlobalInfo(zip_handle.get(), &gi);

    if (global_info_result != UNZ_OK) {
        throw DataSourceException("Read embedded file tree failed (unzGetGlobalInfo)", EMBEDDED_PACK_NAME, global_info_result);
    }
    if (gi.number_entry == 0) {
        throw DataSourceException("Embedded ZIP archive has no entries", EMBEDDED_PACK_NAME);
    }

    EmbeddedFileInfo zip_info;
    unz_file_pos pos;
    unz_file_info info;
    char name_buf[4096];

    for (uLong i = 0; i < gi.number_entry; i++) {
        int32_t file_pos_result = unzGetFilePos(zip_handle.get(), &pos);

        if (file_pos_result != UNZ_OK) {
            throw DataSourceException("Read embedded file tree failed (unzGetFilePos)", EMBEDDED_PACK_NAME, i, file_pos_result);
        }

        int32_t file_info_result = unzGetCurrentFileInfo(zip_handle.get(), &info, name_buf, sizeof(name_buf), nullptr, 0, nullptr, 0);

        if (file_info_result != UNZ_OK) {
            throw DataSourceException("Read embedded file tree failed (unzGetCurrentFileInfo)", EMBEDDED_PACK_NAME, i, file_info_result);
        }
        if (info.size_filename >= sizeof(name_buf)) {
            throw DataSourceException("Embedded ZIP file name exceeds the supported length", EMBEDDED_PACK_NAME, i, info.size_filename, sizeof(name_buf));
        }

        if ((info.external_fa & 0x10) == 0) { // Not folder
            string name = strex(name_buf).normalize_path_slashes();

            if (info.uncompressed_size > numeric_cast<uLong>(std::numeric_limits<int32_t>::max())) {
                throw DataSourceException("Embedded ZIP entry exceeds the supported decoded size", EMBEDDED_PACK_NAME, name, info.uncompressed_size);
            }

            zip_info.Pos = pos;
            zip_info.UncompressedSize = numeric_cast<int32_t>(info.uncompressed_size);
            _filesTree.emplace(name, zip_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        if (i + 1 < gi.number_entry) {
            int32_t next_result = unzGoToNextFile(zip_handle.get());

            if (next_result != UNZ_OK) {
                throw DataSourceException("Read embedded file tree failed (unzGoToNextFile)", EMBEDDED_PACK_NAME, i, next_result);
            }
        }
    }

    _zipHandle = std::move(zip_handle);
}

EmbeddedFile::~EmbeddedFile()
{
    scoped_lock locker {_zipHandleLocker};

    if (_zipHandle) {
        unzClose(_zipHandle.get());
    }
}

auto EmbeddedFile::IsFileExists(string_view path) const -> bool
{
    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return false;
    }

    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto EmbeddedFile::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return false;
    }

    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& info = it->second;
    write_time = _writeTime;
    size = info.UncompressedSize;
    return true;
}

auto EmbeddedFile::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
{
    FO_TRACE_ZONE(FileSystem);

    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return std::nullopt;
    }

    vector<IndexedFile> files;
    files.reserve(_filesTree.size());

    for (const auto& [path, info] : _filesTree) {
        files.emplace_back(IndexedFile {path, numeric_cast<size_t>(info.UncompressedSize), _writeTime});
    }

    return files;
}

auto EmbeddedFile::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return nullptr;
    }

    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& info = it->second;
    unz_file_pos pos = info.Pos;

    int32_t file_pos_result = unzGoToFilePos(_zipHandle.get(), &pos);

    if (file_pos_result != UNZ_OK) {
        throw DataSourceException("Can't read embedded file (unzGoToFilePos)", EMBEDDED_PACK_NAME, path, file_pos_result);
    }

    int32_t open_result = unzOpenCurrentFile(_zipHandle.get());

    if (open_result != UNZ_OK) {
        throw DataSourceException("Can't read embedded file (unzOpenCurrentFile)", EMBEDDED_PACK_NAME, path, open_result);
    }

    auto buf = safe_alloc::make_unique_arr<uint8_t>(numeric_cast<size_t>(info.UncompressedSize));
    int32_t read = unzReadCurrentFile(_zipHandle.get(), buf.get(), info.UncompressedSize);
    int32_t close_result = unzCloseCurrentFile(_zipHandle.get());

    if (read != info.UncompressedSize) {
        throw DataSourceException("Can't read embedded file (unzReadCurrentFile)", EMBEDDED_PACK_NAME, path, info.UncompressedSize, read, close_result);
    }
    if (close_result != UNZ_OK) {
        throw DataSourceException("Can't read embedded file (unzCloseCurrentFile)", EMBEDDED_PACK_NAME, path, close_result);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    return MakeFileBufferHolder(std::move(buf));
}

auto DataSourceEmbedded::Mount() -> unique_ptr<DataSource>
{
    FO_TRACE_ZONE(FileSystem);

    return safe_alloc::make_unique<EmbeddedFile>();
}

FO_END_NAMESPACE
