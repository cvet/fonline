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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "DataSource.h"
#include "DiskFileSystem.h"
#include "EmbeddedResources_Include.h"
#include "FileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"

#include "minizip/unzip.h"

class DataSource::Impl : public NonCopyable
{
public:
    virtual ~Impl() = default;
    virtual bool IsDiskDir() = 0;
    virtual const string& GetPackName() = 0;
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) = 0;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) = 0;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) = 0;
};

using FileNameVec = vector<pair<string, string>>;

static void GetFileNamesGeneric(
    const FileNameVec& fnames, const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
    string path_fixed = _str(path).lower().normalizePathSlashes();
    if (!path_fixed.empty() && path_fixed.back() != '/')
        path_fixed += "/";
    size_t len = path_fixed.length();

    for (auto& fname : fnames)
    {
        bool add = false;
        if (!fname.first.compare(0, len, path_fixed) &&
            (include_subdirs || (len > 0 && fname.first.find_last_of('/') < len) ||
                (len == 0 && fname.first.find_last_of('/') == string::npos)))
        {
            if (ext.empty() || _str(fname.first).getFileExtension() == ext)
                add = true;
        }
        if (add && std::find(result.begin(), result.end(), fname.second) == result.end())
            result.push_back(fname.second);
    }
}

class NonCachedDir : public DataSource::Impl
{
public:
    NonCachedDir(const string& fname);
    virtual bool IsDiskDir() override { return true; }
    virtual const string& GetPackName() override { return basePath; }
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) override;

private:
    string basePath {};
};

class CachedDir : public DataSource::Impl
{
public:
    CachedDir(const string& fname);
    virtual bool IsDiskDir() override { return true; }
    virtual const string& GetPackName() override { return basePath; }
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) override;

private:
    struct FileEntry
    {
        string FileName {};
        uint FileSize {};
        uint64 WriteTime {};
    };
    using IndexMap = unordered_map<string, FileEntry>;

    IndexMap filesTree {};
    FileNameVec filesTreeNames {};
    string basePath {};
};

class FalloutDat : public DataSource::Impl
{
public:
    FalloutDat(const string& fname);
    virtual ~FalloutDat() override;
    virtual bool IsDiskDir() override { return false; }
    virtual const string& GetPackName() override { return fileName; }
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) override
    {
        GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result);
    }

private:
    using IndexMap = unordered_map<string, uchar*>;

    bool ReadTree();

    IndexMap filesTree;
    FileNameVec filesTreeNames;
    string fileName;
    uchar* memTree;
    DiskFile datFile;
    uint64 writeTime;
    UCharVec readBuf;
};

class ZipFile : public DataSource::Impl
{
public:
    ZipFile(const string& fname);
    virtual ~ZipFile() override;
    virtual bool IsDiskDir() override { return false; }
    virtual const string& GetPackName() override { return fileName; }
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) override
    {
        GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result);
    }

private:
    struct ZipFileInfo
    {
        unz_file_pos Pos {};
        int UncompressedSize {};
    };
    using IndexMap = unordered_map<string, ZipFileInfo>;

    bool ReadTree();

    IndexMap filesTree {};
    FileNameVec filesTreeNames {};
    string fileName {};
    unzFile zipHandle {};
    uint64 writeTime {};
};

class AndroidAssets : public DataSource::Impl
{
public:
    AndroidAssets();
    virtual bool IsDiskDir() override { return false; }
    virtual const string& GetPackName() override { return packName; }
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) override;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) override;

private:
    struct FileEntry
    {
        string FileName {};
        uint FileSize {};
        uint64 WriteTime {};
    };
    using IndexMap = unordered_map<string, FileEntry>;

    string packName {"$AndroidAssets"};
    IndexMap filesTree {};
    FileNameVec filesTreeNames {};
};

DataSource::DataSource(const string& path, bool cache_dirs)
{
    string ext = _str(path).getFileExtension();
    if (path == "$AndroidAssets")
        pImpl = std::make_unique<AndroidAssets>();
    else if (ext == "dat")
        pImpl = std::make_unique<FalloutDat>(path);
    else if (ext == "zip" || ext == "bos" || path[0] == '$')
        pImpl = std::make_unique<ZipFile>(path);
    else if (!cache_dirs)
        pImpl = std::make_unique<NonCachedDir>(path);
    else
        pImpl = std::make_unique<CachedDir>(path);
}

DataSource::~DataSource() = default;
DataSource::DataSource(DataSource&&) noexcept = default;

bool DataSource::IsDiskDir()
{
    return pImpl->IsDiskDir();
}

const string& DataSource::GetPackName()
{
    return pImpl->GetPackName();
}

bool DataSource::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    return pImpl->IsFilePresent(path, path_lower, size, write_time);
}

uchar* DataSource::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    return pImpl->OpenFile(path, path_lower, size, write_time);
}

void DataSource::GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
    return pImpl->GetFileNames(path, include_subdirs, ext, result);
}

NonCachedDir::NonCachedDir(const string& fname)
{
    basePath = fname;
    DiskFileSystem::ResolvePath(basePath);
    basePath += "/";
}

bool NonCachedDir::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    DiskFile file = DiskFileSystem::OpenFile(basePath + path, false);
    if (!file)
        return false;

    size = file.GetSize();
    write_time = file.GetWriteTime();
    return true;
}

uchar* NonCachedDir::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    DiskFile file = DiskFileSystem::OpenFile(basePath + path, false);
    if (!file)
        return nullptr;

    size = file.GetSize();
    uchar* buf = new uchar[size + 1];
    if (!file.Read(buf, size))
    {
        delete[] buf;
        throw DataSourceException("Can't read file from non cached dir", basePath + path);
    }
    write_time = file.GetWriteTime();
    buf[size] = 0;
    return buf;
}

void NonCachedDir::GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
    FileNameVec fnames;
    DiskFileSystem::IterateDir(
        basePath + path, "", include_subdirs, [&fnames](const string& fname, uint fsize, uint64 write_time) {
            string name_lower = _str(fname).lower();
            fnames.push_back(std::make_pair(name_lower, fname));
        });

    StrVec filtered;
    GetFileNamesGeneric(fnames, path, include_subdirs, ext, filtered);
    if (!filtered.empty())
        result.insert(result.begin(), filtered.begin(), filtered.end());
}

CachedDir::CachedDir(const string& fname)
{
    basePath = fname;
    DiskFileSystem::ResolvePath(basePath);
    basePath += "/";

    DiskFileSystem::IterateDir(basePath, "", true, [this](const string& fname, uint fsize, uint64 write_time) {
        FileEntry fe;
        fe.FileName = basePath + fname;
        fe.FileSize = fsize;
        fe.WriteTime = write_time;

        string name_lower = _str(fname).lower();
        filesTree.insert(std::make_pair(name_lower, fe));
        filesTreeNames.push_back(std::make_pair(name_lower, fname));
    });
}

bool CachedDir::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    FileEntry& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

uchar* CachedDir::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    FileEntry& fe = it->second;
    DiskFile file = DiskFileSystem::OpenFile(fe.FileName, false);
    if (!file)
        throw DataSourceException("Can't read file from cached dir", basePath + path);

    size = fe.FileSize;
    uchar* buf = new uchar[size + 1];
    if (!file.Read(buf, size))
    {
        delete[] buf;
        return nullptr;
    }
    buf[size] = 0;
    write_time = fe.WriteTime;
    return buf;
}

void CachedDir::GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
    StrVec result_;
    GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result_);
    if (!result_.empty())
        result.insert(result.begin(), result_.begin(), result_.end());
}

FalloutDat::FalloutDat(const string& fname) : datFile {DiskFileSystem::OpenFile(fname, false)}
{
    fileName = fname;
    readBuf.resize(0x40000);

    if (!datFile)
        throw DataSourceException("Cannot open fallout dat file", fname);

    writeTime = datFile.GetWriteTime();

    if (!ReadTree())
        throw DataSourceException("Read fallout dat file tree fail");
}

FalloutDat::~FalloutDat()
{
    SAFEDELA(memTree);
}

bool FalloutDat::ReadTree()
{
    uint version;
    if (!datFile.SetPos(-12, DiskFileSeek::End))
        return false;
    if (!datFile.Read(&version, 4))
        return false;

    // DAT 2.1 Arcanum
    if (version == 0x44415431) // 1TAD
    {
        // Readed data
        uint files_total;
        uint tree_size;

        // Read info
        if (!datFile.SetPos(-4, DiskFileSeek::End))
            return false;
        if (!datFile.Read(&tree_size, 4))
            return false;

        // Read tree
        if (!datFile.SetPos(-(int)tree_size, DiskFileSeek::End))
            return false;
        if (!datFile.Read(&files_total, 4))
            return false;
        tree_size -= 28 + 4; // Subtract information block and files total
        memTree = new uchar[tree_size];
        memzero(memTree, tree_size);
        if (!datFile.Read(memTree, tree_size))
            return false;

        // Indexing tree
        uchar* ptr = memTree;
        uchar* end_ptr = memTree + tree_size;
        while (true)
        {
            uint fnsz; // Include zero
            memcpy(&fnsz, ptr, sizeof(fnsz));
            uint type;
            memcpy(&type, ptr + 4 + fnsz + 4, sizeof(type));

            if (fnsz > 1 && type != 0x400) // Not folder
            {
                string name = _str(string((const char*)ptr + 4, fnsz)).normalizePathSlashes();
                string name_lower = _str(name).lower();

                if (type == 2)
                    *(ptr + 4 + fnsz + 7) = 1; // Compressed
                filesTree.insert(std::make_pair(name_lower, ptr + 4 + fnsz + 7));
                filesTreeNames.push_back(std::make_pair(name_lower, name));
            }

            if (ptr + fnsz + 24 >= end_ptr)
                break;
            ptr += fnsz + 24;
        }

        return true;
    }

    // DAT 2.0 Fallout2
    // Readed data
    uint dir_count, dat_size, files_total, tree_size;

    // Read info
    if (!datFile.SetPos(-8, DiskFileSeek::End))
        return false;
    if (!datFile.Read(&tree_size, 4))
        return false;
    if (!datFile.Read(&dat_size, 4))
        return false;

    // Check for DAT1.0 Fallout1 dat file
    if (!datFile.SetPos(0, DiskFileSeek::Set))
        return false;
    if (!datFile.Read(&dir_count, 4))
        return false;
    dir_count >>= 24;
    if (dir_count == 0x01 || dir_count == 0x33)
        return false;

    // Check for truncated
    if (datFile.GetSize() != dat_size)
        return false;

    // Read tree
    if (!datFile.SetPos(-((int)tree_size + 8), DiskFileSeek::End))
        return false;
    if (!datFile.Read(&files_total, 4))
        return false;
    tree_size -= 4;
    memTree = new uchar[tree_size];
    memzero(memTree, tree_size);
    if (!datFile.Read(memTree, tree_size))
        return false;

    // Indexing tree
    uchar* ptr = memTree;
    uchar* end_ptr = memTree + tree_size;
    while (true)
    {
        uint fnsz;
        memcpy(&fnsz, ptr, sizeof(fnsz));

        if (fnsz)
        {
            string name = _str(string((const char*)ptr + 4, fnsz)).normalizePathSlashes();
            string name_lower = _str(name).lower();

            filesTree.insert(std::make_pair(name_lower, ptr + 4 + fnsz));
            filesTreeNames.push_back(std::make_pair(name_lower, name));
        }

        if (ptr + fnsz + 17 >= end_ptr)
            break;
        ptr += fnsz + 17;
    }

    return true;
}

bool FalloutDat::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    if (!datFile)
        return false;

    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    memcpy(&size, it->second + 1, sizeof(size));
    write_time = writeTime;
    return true;
}

uchar* FalloutDat::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    uchar* ptr = it->second;
    uchar type;
    memcpy(&type, ptr, sizeof(type));
    uint real_size;
    memcpy(&real_size, ptr + 1, sizeof(real_size));
    uint packed_size;
    memcpy(&packed_size, ptr + 5, sizeof(packed_size));
    uint offset;
    memcpy(&offset, ptr + 9, sizeof(offset));

    if (!datFile.SetPos(offset, DiskFileSeek::Set))
        throw DataSourceException("Can't read file from fallout dat (1)", path);

    size = real_size;
    uchar* buf = new uchar[size + 1];

    if (!type)
    {
        // Plane data
        if (!datFile.Read(buf, size))
        {
            delete[] buf;
            throw DataSourceException("Can't read file from fallout dat (2)", path);
        }
    }
    else
    {
        // Packed data
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.next_in = Z_NULL;
        stream.avail_in = 0;
        if (inflateInit(&stream) != Z_OK)
        {
            delete[] buf;
            throw DataSourceException("Can't read file from fallout dat (3)", path);
        }

        stream.next_out = buf;
        stream.avail_out = real_size;

        uint left = packed_size;
        while (stream.avail_out)
        {
            if (!stream.avail_in && left > 0)
            {
                stream.next_in = &readBuf[0];
                uint len = MIN(left, (uint)readBuf.size());
                if (!datFile.Read(&readBuf[0], len))
                {
                    delete[] buf;
                    throw DataSourceException("Can't read file from fallout dat (4)", path);
                }
                stream.avail_in = len;
                left -= len;
            }

            int r = inflate(&stream, Z_NO_FLUSH);
            if (r != Z_OK && r != Z_STREAM_END)
            {
                delete[] buf;
                throw DataSourceException("Can't read file from fallout dat (5)", path);
            }
            if (r == Z_STREAM_END)
                break;
        }

        inflateEnd(&stream);
    }

    write_time = writeTime;
    buf[size] = 0;
    return buf;
}

ZipFile::ZipFile(const string& fname)
{
    fileName = fname;
    zipHandle = nullptr;

    zlib_filefunc_def ffunc;
    if (fname[0] != '$')
    {
        DiskFile* file = new DiskFile {DiskFileSystem::OpenFile(fname, false)};
        if (!*file)
        {
            delete file;
            throw DataSourceException("Can't open zip file", fname);
        }

        writeTime = file->GetWriteTime();

        ffunc.zopen_file = [](voidpf opaque, const char* filename, int mode) -> voidpf { return opaque; };
        ffunc.zread_file = [](voidpf opaque, voidpf stream, void* buf, uLong size) -> uLong {
            DiskFile* file = (DiskFile*)stream;
            return file->Read(buf, size) ? size : 0;
        };
        ffunc.zwrite_file = [](voidpf opaque, voidpf stream, const void* buf, uLong size) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf opaque, voidpf stream) -> long {
            DiskFile* file = (DiskFile*)stream;
            return (long)file->GetPos();
        };
        ffunc.zseek_file = [](voidpf opaque, voidpf stream, uLong offset, int origin) -> long {
            DiskFile* file = (DiskFile*)stream;
            switch (origin)
            {
            case ZLIB_FILEFUNC_SEEK_SET:
                file->SetPos(offset, DiskFileSeek::Set);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                file->SetPos(offset, DiskFileSeek::Cur);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                file->SetPos(offset, DiskFileSeek::End);
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf opaque, voidpf stream) -> int {
            DiskFile* file = (DiskFile*)stream;
            delete file;
            return 0;
        };
        ffunc.zerror_file = [](voidpf opaque, voidpf stream) -> int {
            if (stream == nullptr)
                return 1;
            return 0;
        };
        ffunc.opaque = file;
    }
    else
    {
        writeTime = 0;

        struct MemStream
        {
            const uchar* Buf;
            uint Length;
            uint Pos;
        };

        ffunc.zopen_file = [](voidpf opaque, const char* filename, int mode) -> voidpf {
            if (Str::Compare(filename, "$Basic"))
            {
                MemStream* mem_stream = new MemStream();
                mem_stream->Buf = Resource_Basic_zipped;
                mem_stream->Length = sizeof(Resource_Basic_zipped);
                mem_stream->Pos = 0;
                return mem_stream;
            }
            return 0;
        };
        ffunc.zread_file = [](voidpf opaque, voidpf stream, void* buf, uLong size) -> uLong {
            MemStream* mem_stream = (MemStream*)stream;
            memcpy(buf, mem_stream->Buf + mem_stream->Pos, size);
            mem_stream->Pos += (uint)size;
            return size;
        };
        ffunc.zwrite_file = [](voidpf opaque, voidpf stream, const void* buf, uLong size) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf opaque, voidpf stream) -> long {
            MemStream* mem_stream = (MemStream*)stream;
            return (long)mem_stream->Pos;
        };
        ffunc.zseek_file = [](voidpf opaque, voidpf stream, uLong offset, int origin) -> long {
            MemStream* mem_stream = (MemStream*)stream;
            switch (origin)
            {
            case ZLIB_FILEFUNC_SEEK_SET:
                mem_stream->Pos = (uint)offset;
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                mem_stream->Pos += (uint)offset;
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                mem_stream->Pos = mem_stream->Length + offset;
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf opaque, voidpf stream) -> int {
            MemStream* mem_stream = (MemStream*)stream;
            delete mem_stream;
            return 0;
        };
        ffunc.zerror_file = [](voidpf opaque, voidpf stream) -> int {
            if (stream == nullptr)
                return 1;
            return 0;
        };
        ffunc.opaque = nullptr;
    }

    zipHandle = unzOpen2(fname.c_str(), &ffunc);
    if (!zipHandle)
        throw DataSourceException("Can't read zip file", fname);

    if (!ReadTree())
        throw DataSourceException("Read zip file tree fail", fname);
}

ZipFile::~ZipFile()
{
    if (zipHandle)
        unzClose(zipHandle);
}

bool ZipFile::ReadTree()
{
    unz_global_info gi;
    if (unzGetGlobalInfo(zipHandle, &gi) != UNZ_OK || gi.number_entry == 0)
        return false;

    ZipFileInfo zip_info;
    unz_file_pos pos;
    unz_file_info info;
    for (uLong i = 0; i < gi.number_entry; i++)
    {
        if (unzGetFilePos(zipHandle, &pos) != UNZ_OK)
            return false;

        char buf[MAX_FOPATH];
        if (unzGetCurrentFileInfo(zipHandle, &info, buf, sizeof(buf), nullptr, 0, nullptr, 0) != UNZ_OK)
            return false;

        if (!(info.external_fa & 0x10)) // Not folder
        {
            string name = _str(buf).normalizePathSlashes();
            string name_lower = _str(name).lower();

            zip_info.Pos = pos;
            zip_info.UncompressedSize = (int)info.uncompressed_size;
            filesTree.insert(std::make_pair(name_lower, zip_info));
            filesTreeNames.push_back(std::make_pair(name_lower, name));
        }

        if (i + 1 < gi.number_entry && unzGoToNextFile(zipHandle) != UNZ_OK)
            return false;
    }

    return true;
}

bool ZipFile::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    ZipFileInfo& info = it->second;
    write_time = writeTime;
    size = info.UncompressedSize;
    return true;
}

uchar* ZipFile::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    ZipFileInfo& info = it->second;

    if (unzGoToFilePos(zipHandle, &info.Pos) != UNZ_OK)
        throw DataSourceException("Can't read file from zip (1)", path);

    if (unzOpenCurrentFile(zipHandle) != UNZ_OK)
        throw DataSourceException("Can't read file from zip (2)", path);

    uchar* buf = new uchar[info.UncompressedSize + 1];
    int read = unzReadCurrentFile(zipHandle, buf, info.UncompressedSize);
    if (unzCloseCurrentFile(zipHandle) != UNZ_OK || read != info.UncompressedSize)
    {
        delete[] buf;
        throw DataSourceException("Can't read file from zip (3)", path);
    }

    write_time = writeTime;
    size = info.UncompressedSize;
    buf[size] = 0;
    return buf;
}

AndroidAssets::AndroidAssets()
{
    filesTree.clear();
    filesTreeNames.clear();

    // Read tree
    DiskFile file_tree = DiskFileSystem::OpenFile("FilesTree.txt", false);
    if (!file_tree)
        throw DataSourceException("Can't open 'FilesTree.txt' in android assets");

    uint len = file_tree.GetSize() + 1;
    char* buf = new char[len];
    buf[len - 1] = 0;

    if (!file_tree.Read(buf, len))
    {
        delete[] buf;
        throw DataSourceException("Can't read 'FilesTree.txt' in android assets");
    }

    StrVec names = _str(buf).normalizeLineEndings().split('\n');
    delete[] buf;

    // Parse
    for (const string& name : names)
    {
        DiskFile file = DiskFileSystem::OpenFile(name, false);
        if (!file)
            throw DataSourceException("Can't open file in android assets", name);

        FileEntry fe;
        fe.FileName = name;
        fe.FileSize = file.GetSize();
        fe.WriteTime = file.GetWriteTime();

        string name_lower = _str(name).lower();
        filesTree.insert(std::make_pair(name_lower, fe));
        filesTreeNames.push_back(std::make_pair(name_lower, name));
    }
}

bool AndroidAssets::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    FileEntry& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

uchar* AndroidAssets::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    FileEntry& fe = it->second;
    DiskFile file = DiskFileSystem::OpenFile(fe.FileName, false);
    if (!file)
        throw DataSourceException("Can't open file in android assets", path);

    size = fe.FileSize;
    uchar* buf = new uchar[size + 1];
    if (!file.Read(buf, size))
    {
        delete[] buf;
        throw DataSourceException("Can't read file in android assets", path);
    }
    buf[size] = 0;
    write_time = fe.WriteTime;
    return buf;
}

void AndroidAssets::GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
    StrVec result_;
    GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result_);
    if (!result_.empty())
        result.insert(result.begin(), result_.begin(), result_.end());
}
