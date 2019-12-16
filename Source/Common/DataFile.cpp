#include "DataFile.h"
#include "EmbeddedResources_Include.h"
#include "FileSystem.h"
#include "FileUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"
#include "minizip/unzip.h"

#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
#define DISABLE_FOLDER_CACHING
#endif

/************************************************************************/
/* Folder/Dat/Zip loaders                                               */
/************************************************************************/

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

class FolderFile : public IDataFile
{
private:
    struct FileEntry
    {
        string FileName;
        uint FileSize;
        uint64 WriteTime;
    };
    typedef map<string, FileEntry> IndexMap;

#ifndef DISABLE_FOLDER_CACHING
    IndexMap filesTree;
    FileNameVec filesTreeNames;
#endif
    string basePath;

    void CollectFilesTree(IndexMap& files_tree, FileNameVec& files_tree_names);

public:
    FolderFile(const string& fname);

    const string& GetPackName() { return basePath; }
    bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time);
    uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time);
    void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result);
};

class FalloutDatFile : public IDataFile
{
private:
    typedef map<string, uchar*> IndexMap;

    IndexMap filesTree;
    FileNameVec filesTreeNames;
    string fileName;
    uchar* memTree;
    void* datHandle;
    uint64 writeTime;
    UCharVec readBuf;

    bool ReadTree();

public:
    FalloutDatFile(const string& fname);
    ~FalloutDatFile();

    const string& GetPackName() { return fileName; }
    bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time);
    uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time);
    void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
    {
        GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result);
    }
};

class ZipFile : public IDataFile
{
private:
    struct ZipFileInfo
    {
        unz_file_pos Pos;
        int UncompressedSize;
    };
    typedef map<string, ZipFileInfo> IndexMap;

    IndexMap filesTree;
    FileNameVec filesTreeNames;
    string fileName;
    unzFile zipHandle;
    uint64 writeTime;

    bool ReadTree();

public:
    ZipFile(const string& fname);
    ~ZipFile();

    const string& GetPackName() { return fileName; }
    bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time);
    uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time);
    void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
    {
        GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result);
    }
};

class BundleFile : public IDataFile
{
private:
    struct FileEntry
    {
        string FileName;
        uint FileSize;
        uint64 WriteTime;
    };
    typedef map<string, FileEntry> IndexMap;

    static string packName;
    IndexMap filesTree;
    FileNameVec filesTreeNames;

public:
    BundleFile(const string& fname);

    const string& GetPackName() { return packName; }
    bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time);
    uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time);
    void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result);
};
string BundleFile::packName = "$Bundle";

/************************************************************************/
/* Manage                                                               */
/************************************************************************/

DataFile Fabric::OpenDataFile(const string& path)
{
    string ext = _str(path).getFileExtension();
    if (path == "$Bundle")
        return std::make_shared<BundleFile>(path);
    else if (ext == "dat")
        return std::make_shared<FalloutDatFile>(path);
    else if (ext == "zip" || ext == "bos" || path[0] == '$')
        return std::make_shared<ZipFile>(path);
    else
        return std::make_shared<FolderFile>(path);
}

/************************************************************************/
/* Folder file                                                          */
/************************************************************************/

FolderFile::FolderFile(const string& fname)
{
    basePath = fname;
#ifndef DISABLE_FOLDER_CACHING
    CollectFilesTree(filesTree, filesTreeNames);
#endif
}

void FolderFile::CollectFilesTree(IndexMap& files_tree, FileNameVec& files_tree_names)
{
    files_tree.clear();
    files_tree_names.clear();

    StrVec files;
    FindDataVec find_data;
    File::GetFolderFileNames(basePath, true, "", files, &find_data);

    for (size_t i = 0, j = files.size(); i < j; i++)
    {
        FileEntry fe;
        fe.FileName = basePath + files[i];
        fe.FileSize = find_data[i].FileSize;
        fe.WriteTime = find_data[i].WriteTime;

        string name_lower = _str(files[i]).lower();
        files_tree.insert(std::make_pair(name_lower, fe));
        files_tree_names.push_back(std::make_pair(name_lower, files[i]));
    }
}

bool FolderFile::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
#ifndef DISABLE_FOLDER_CACHING
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    FileEntry& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;

#else
    void* f = FileOpen(basePath + path, false);
    if (!f)
        return false;
    size = FileGetSize(f);
    write_time = FileGetWriteTime(f);
    FileClose(f);
    return true;
#endif
}

uchar* FolderFile::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
#ifndef DISABLE_FOLDER_CACHING
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    FileEntry& fe = it->second;
    void* f = FileOpen(fe.FileName, false);
    if (!f)
        return nullptr;

    size = fe.FileSize;
    uchar* buf = new uchar[size + 1];
    if (!FileRead(f, buf, size))
    {
        FileClose(f);
        delete[] buf;
        return nullptr;
    }
    FileClose(f);
    buf[size] = 0;
    write_time = fe.WriteTime;
    return buf;

#else
    void* f = FileOpen(basePath + path, false);
    if (!f)
        return nullptr;

    size = FileGetSize(f);
    uchar* buf = new uchar[size + 1];
    if (!FileRead(f, buf, size))
    {
        FileClose(f);
        delete[] buf;
        return nullptr;
    }
    write_time = FileGetWriteTime(f);
    FileClose(f);
    buf[size] = 0;
    return buf;
#endif
}

void FolderFile::GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
#ifndef DISABLE_FOLDER_CACHING
    FileNameVec& files_tree_names = filesTreeNames;
#else
    IndexMap files_tree;
    FileNameVec files_tree_names;
    CollectFilesTree(files_tree, files_tree_names);
#endif

    StrVec result_;
    GetFileNamesGeneric(files_tree_names, path, include_subdirs, ext, result_);
    if (!result_.empty())
        result.insert(result.begin(), result_.begin(), result_.end());
}

/************************************************************************/
/* Dat file                                                             */
/************************************************************************/

FalloutDatFile::FalloutDatFile(const string& fname)
{
    datHandle = nullptr;
    memTree = nullptr;
    fileName = fname;
    readBuf.resize(0x40000);

    datHandle = FileOpen(fname, false);
    if (!datHandle)
        throw fo_exception("Cannot open file '{}'", fname);

    writeTime = FileGetWriteTime(datHandle);

    if (!ReadTree())
        throw fo_exception("Read file tree fail");
}

FalloutDatFile::~FalloutDatFile()
{
    if (datHandle)
    {
        FileClose(datHandle);
        datHandle = nullptr;
    }
    SAFEDELA(memTree);
}

bool FalloutDatFile::ReadTree()
{
    uint version;
    if (!FileSetPointer(datHandle, -12, SEEK_END))
        return false;
    if (!FileRead(datHandle, &version, 4))
        return false;

    // DAT 2.1 Arcanum
    if (version == 0x44415431) // 1TAD
    {
        // Readed data
        uint files_total, tree_size;

        // Read info
        if (!FileSetPointer(datHandle, -4, SEEK_END))
            return false;
        if (!FileRead(datHandle, &tree_size, 4))
            return false;

        // Read tree
        if (!FileSetPointer(datHandle, -(int)tree_size, SEEK_END))
            return false;
        if (!FileRead(datHandle, &files_total, 4))
            return false;
        tree_size -= 28 + 4; // Subtract information block and files total
        memTree = new uchar[tree_size];
        memzero(memTree, tree_size);
        if (!FileRead(datHandle, memTree, tree_size))
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
    if (!FileSetPointer(datHandle, -8, SEEK_END))
        return false;
    if (!FileRead(datHandle, &tree_size, 4))
        return false;
    if (!FileRead(datHandle, &dat_size, 4))
        return false;

    // Check for DAT1.0 Fallout1 dat file
    if (!FileSetPointer(datHandle, 0, SEEK_SET))
        return false;
    if (!FileRead(datHandle, &dir_count, 4))
        return false;
    dir_count >>= 24;
    if (dir_count == 0x01 || dir_count == 0x33)
        return false;

    // Check for truncated
    if (FileGetSize(datHandle) != dat_size)
        return false;

    // Read tree
    if (!FileSetPointer(datHandle, -((int)tree_size + 8), SEEK_END))
        return false;
    if (!FileRead(datHandle, &files_total, 4))
        return false;
    tree_size -= 4;
    memTree = new uchar[tree_size];
    memzero(memTree, tree_size);
    if (!FileRead(datHandle, memTree, tree_size))
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

bool FalloutDatFile::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    if (!datHandle)
        return false;

    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    memcpy(&size, it->second + 1, sizeof(size));
    write_time = writeTime;
    return true;
}

uchar* FalloutDatFile::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    if (!datHandle)
        return nullptr;

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

    if (!FileSetPointer(datHandle, offset, SEEK_SET))
        return nullptr;

    size = real_size;
    uchar* buf = new uchar[size + 1];

    if (!type)
    {
        // Plane data
        if (!FileRead(datHandle, buf, size))
        {
            delete[] buf;
            return nullptr;
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
            return nullptr;
        }

        stream.next_out = buf;
        stream.avail_out = real_size;

        uint left = packed_size;
        while (stream.avail_out)
        {
            if (!stream.avail_in && left > 0)
            {
                stream.next_in = &readBuf[0];
                uint rb;
                if (!FileRead(datHandle, &readBuf[0], MIN(left, (uint)readBuf.size()), &rb))
                {
                    delete[] buf;
                    return nullptr;
                }
                stream.avail_in = rb;
                left -= rb;
            }

            int r = inflate(&stream, Z_NO_FLUSH);
            if (r != Z_OK && r != Z_STREAM_END)
            {
                delete[] buf;
                return nullptr;
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

/************************************************************************/
/* Zip file                                                             */
/************************************************************************/

ZipFile::ZipFile(const string& fname)
{
    fileName = fname;
    zipHandle = nullptr;

    zlib_filefunc_def ffunc;
    if (fname[0] != '$')
    {
        void* file = FileOpen(fname, false);
        if (!file)
            throw fo_exception("Can't open ZIP file '{}'", fname);

        writeTime = FileGetWriteTime(file);

        ffunc.zopen_file = [](voidpf opaque, const char* filename, int mode) -> voidpf { return opaque; };
        ffunc.zread_file = [](voidpf opaque, voidpf stream, void* buf, uLong size) -> uLong {
            uint rb = 0;
            FileRead(stream, buf, size, &rb);
            return rb;
        };
        ffunc.zwrite_file = [](voidpf opaque, voidpf stream, const void* buf, uLong size) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf opaque, voidpf stream) -> long { return (long)FileGetPointer(stream); };
        ffunc.zseek_file = [](voidpf opaque, voidpf stream, uLong offset, int origin) -> long {
            switch (origin)
            {
            case ZLIB_FILEFUNC_SEEK_SET:
                FileSetPointer(stream, (uint)offset, SEEK_SET);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                FileSetPointer(stream, (uint)offset, SEEK_CUR);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                FileSetPointer(stream, (uint)offset, SEEK_END);
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf opaque, voidpf stream) -> int {
            FileClose(stream);
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
                mem_stream->Buf = Resource_Basic_zip;
                mem_stream->Length = sizeof(Resource_Basic_zip);
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
            MemStream* mem_stream = new MemStream();
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
        throw fo_exception("Can't read ZIP file '{}'", fname);

    if (!ReadTree())
        throw fo_exception("Read file tree fail");
}

ZipFile::~ZipFile()
{
    if (zipHandle)
    {
        unzClose(zipHandle);
        zipHandle = nullptr;
    }
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
    if (!zipHandle)
        return false;

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
    if (!zipHandle)
        return nullptr;

    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    ZipFileInfo& info = it->second;

    if (unzGoToFilePos(zipHandle, &info.Pos) != UNZ_OK)
        return nullptr;

    uchar* buf = new uchar[info.UncompressedSize + 1];

    if (unzOpenCurrentFile(zipHandle) != UNZ_OK)
    {
        delete[] buf;
        return nullptr;
    }

    int read = unzReadCurrentFile(zipHandle, buf, info.UncompressedSize);
    if (unzCloseCurrentFile(zipHandle) != UNZ_OK || read != info.UncompressedSize)
    {
        delete[] buf;
        return nullptr;
    }

    write_time = writeTime;
    size = info.UncompressedSize;
    buf[size] = 0;
    return buf;
}

/************************************************************************/
/* Bundle file                                                          */
/************************************************************************/

BundleFile::BundleFile(const string& fname)
{
    filesTree.clear();
    filesTreeNames.clear();

    // Read tree
    void* f_tree = FileOpen("FilesTree.txt", false);
    if (!f_tree)
        throw fo_exception("Can't open 'FilesTree.txt'");

    uint len = FileGetSize(f_tree) + 1;
    char* buf = new char[len];
    buf[len - 1] = 0;

    if (!FileRead(f_tree, buf, len))
    {
        delete[] buf;
        FileClose(f_tree);
        throw fo_exception("Can't read 'FilesTree.txt'");
    }

    StrVec names = _str(buf).normalizeLineEndings().split('\n');
    delete[] buf;
    FileClose(f_tree);

    // Parse
    for (const string& name : names)
    {
        void* f = FileOpen(name, false);
        if (!f)
            throw fo_exception("Can't open file '{}' in bundle", name);

        FileEntry fe;
        fe.FileName = name;
        fe.FileSize = FileGetSize(f);
        fe.WriteTime = FileGetWriteTime(f);

        string name_lower = _str(name).lower();
        filesTree.insert(std::make_pair(name_lower, fe));
        filesTreeNames.push_back(std::make_pair(name_lower, name));

        FileClose(f);
    }
}

bool BundleFile::IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return false;

    FileEntry& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

uchar* BundleFile::OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time)
{
    auto it = filesTree.find(path_lower);
    if (it == filesTree.end())
        return nullptr;

    FileEntry& fe = it->second;
    void* f = FileOpen(fe.FileName, false);
    if (!f)
        return nullptr;

    size = fe.FileSize;
    uchar* buf = new uchar[size + 1];
    if (!FileRead(f, buf, size))
    {
        FileClose(f);
        delete[] buf;
        return nullptr;
    }
    FileClose(f);
    buf[size] = 0;
    write_time = fe.WriteTime;
    return buf;
}

void BundleFile::GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result)
{
    StrVec result_;
    GetFileNamesGeneric(filesTreeNames, path, include_subdirs, ext, result_);
    if (!result_.empty())
        result.insert(result.begin(), result_.begin(), result_.end());
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
