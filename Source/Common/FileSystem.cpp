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

#include "FileSystem.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"

FileHeader::FileHeader(const string& name, const string& path, uint size, uint64 write_time, DataSource* ds) :
    isLoaded {true}, fileName {name}, filePath {path}, fileSize {size}, writeTime {write_time}, dataSource {ds}
{
}

FileHeader::operator bool() const
{
    return isLoaded;
}

const string& FileHeader::GetName()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(!fileName.empty());

    return fileName;
}

const string& FileHeader::GetPath()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(!filePath.empty());

    return filePath;
}

uint FileHeader::GetFsize()
{
    RUNTIME_ASSERT(isLoaded);

    return fileSize;
}

uint64 FileHeader::GetWriteTime()
{
    RUNTIME_ASSERT(isLoaded);

    return writeTime;
}

File::File(const string& name, const string& path, uint size, uint64 write_time, DataSource* ds, uchar* buf) :
    FileHeader(name, path, size, write_time, ds), fileBuf {buf}
{
    RUNTIME_ASSERT(fileBuf[fileSize] == 0);
}

File::File(uchar* buf, uint size) : FileHeader("", "", size, 0, nullptr), fileBuf {buf}
{
}

const char* File::GetCStr()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    return (const char*)fileBuf.get();
}

uchar* File::GetBuf()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    return fileBuf.get();
}

uchar* File::GetCurBuf()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    return fileBuf.get() + curPos;
}

uint File::GetCurPos()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    return curPos;
}

uchar* File::ReleaseBuffer()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    return fileBuf.release();
}

void File::SetCurPos(uint pos)
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);
    RUNTIME_ASSERT(pos <= fileSize);

    curPos = pos;
}

void File::GoForward(uint offs)
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);
    RUNTIME_ASSERT(curPos + offs <= fileSize);

    curPos += offs;
}

void File::GoBack(uint offs)
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);
    RUNTIME_ASSERT(offs <= curPos);

    curPos -= offs;
}

bool File::FindFragment(const uchar* fragment, uint fragment_len, uint begin_offs)
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    for (uint i = begin_offs; i < fileSize - fragment_len; i++)
    {
        if (fileBuf[i] == fragment[0])
        {
            bool not_match = false;
            for (uint j = 1; j < fragment_len; j++)
            {
                if (fileBuf[i + j] != fragment[j])
                {
                    not_match = true;
                    break;
                }
            }

            if (!not_match)
            {
                curPos = i;
                return true;
            }
        }
    }
    return false;
}

string File::GetNonEmptyLine()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    while (curPos < fileSize)
    {
        uint start = curPos;
        uint len = 0;
        while (curPos < fileSize)
        {
            if (fileBuf[curPos] == '\r' || fileBuf[curPos] == '\n' || fileBuf[curPos] == '#' || fileBuf[curPos] == ';')
            {
                for (; curPos < fileSize; curPos++)
                    if (fileBuf[curPos] == '\n')
                        break;
                curPos++;
                break;
            }

            curPos++;
            len++;
        }

        if (len)
        {
            string line = _str(string((const char*)&fileBuf[start], len)).trim();
            if (!line.empty())
                return line;
        }
    }

    return "";
}

void File::CopyMem(void* ptr, uint size)
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);
    RUNTIME_ASSERT(size);

    if (curPos + size > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    memcpy(ptr, fileBuf.get() + curPos, size);
    curPos += size;
}

string File::GetStrNT()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + 1 > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    uint len = 0;
    while (*(fileBuf.get() + curPos + len))
        len++;

    string str((const char*)&fileBuf[curPos], len);
    curPos += len + 1;
    return str;
}

uchar File::GetUChar()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(uchar) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    uchar res = 0;
    res = fileBuf[curPos++];
    return res;
}

ushort File::GetBEUShort()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(ushort) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    ushort res = 0;
    uchar* cres = (uchar*)&res;
    cres[1] = fileBuf[curPos++];
    cres[0] = fileBuf[curPos++];
    return res;
}

ushort File::GetLEUShort()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(ushort) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    ushort res = 0;
    uchar* cres = (uchar*)&res;
    cres[0] = fileBuf[curPos++];
    cres[1] = fileBuf[curPos++];
    return res;
}

uint File::GetBEUInt()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(uint) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    uint res = 0;
    uchar* cres = (uchar*)&res;
    for (int i = 3; i >= 0; i--)
        cres[i] = fileBuf[curPos++];
    return res;
}

uint File::GetLEUInt()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(uint) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    uint res = 0;
    uchar* cres = (uchar*)&res;
    for (int i = 0; i <= 3; i++)
        cres[i] = fileBuf[curPos++];
    return res;
}

uint File::GetLE3UChar()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(uchar) * 3 > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    uint res = 0;
    uchar* cres = (uchar*)&res;
    for (int i = 0; i <= 2; i++)
        cres[i] = fileBuf[curPos++];
    return res;
}

float File::GetBEFloat()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(float) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    float res;
    uchar* cres = (uchar*)&res;
    for (int i = 3; i >= 0; i--)
        cres[i] = fileBuf[curPos++];
    return res;
}

float File::GetLEFloat()
{
    RUNTIME_ASSERT(isLoaded);
    RUNTIME_ASSERT(fileBuf);

    if (curPos + sizeof(float) > fileSize)
        throw FileSystemExeption("Read file error", fileName);

    float res;
    uchar* cres = (uchar*)&res;
    for (int i = 0; i <= 3; i++)
        cres[i] = fileBuf[curPos++];
    return res;
}

OutputFile::OutputFile(DiskFile file) : diskFile {std::move(file)}
{
    RUNTIME_ASSERT(diskFile);
}

void OutputFile::Save()
{
    if (!dataBuf.empty())
    {
        bool save_ok = diskFile.Write(&dataBuf[0], (uint)dataBuf.size());
        RUNTIME_ASSERT(save_ok);
        dataBuf.clear();
    }
}

uchar* OutputFile::GetOutBuf()
{
    RUNTIME_ASSERT(!dataBuf.empty());

    return &dataBuf[0];
}

uint OutputFile::GetOutBufLen()
{
    RUNTIME_ASSERT(!dataBuf.empty());

    return (uint)dataBuf.size();
}

void OutputFile::SetData(const void* data, uint len)
{
    if (!len)
        return;

    dataWriter.WritePtr(data, len);
}

void OutputFile::SetStr(const string& str)
{
    SetData(str.c_str(), (uint)str.length());
}

void OutputFile::SetStrNT(const string& str)
{
    SetData(str.c_str(), (uint)str.length() + 1);
}

void OutputFile::SetUChar(uchar data)
{
    dataWriter.Write(data);
}

void OutputFile::SetBEUShort(ushort data)
{
    uchar* pdata = (uchar*)&data;
    dataWriter.Write(pdata[1]);
    dataWriter.Write(pdata[0]);
}

void OutputFile::SetLEUShort(ushort data)
{
    uchar* pdata = (uchar*)&data;
    dataWriter.Write(pdata[0]);
    dataWriter.Write(pdata[1]);
}

void OutputFile::SetBEUInt(uint data)
{
    uchar* pdata = (uchar*)&data;
    dataWriter.Write(pdata[3]);
    dataWriter.Write(pdata[2]);
    dataWriter.Write(pdata[1]);
    dataWriter.Write(pdata[0]);
}

void OutputFile::SetLEUInt(uint data)
{
    uchar* pdata = (uchar*)&data;
    dataWriter.Write(pdata[0]);
    dataWriter.Write(pdata[1]);
    dataWriter.Write(pdata[2]);
    dataWriter.Write(pdata[3]);
}

FileCollection::FileCollection(const string& path, vector<FileHeader> files) :
    filterPath {path}, allFiles {std::move(files)}
{
}

const string& FileCollection::GetPath()
{
    return filterPath;
}

bool FileCollection::MoveNext()
{
    RUNTIME_ASSERT(curFileIndex < (int)allFiles.size());

    return ++curFileIndex < (int)allFiles.size();
}

File FileCollection::GetCurFile()
{
    RUNTIME_ASSERT(curFileIndex >= 0);
    RUNTIME_ASSERT(curFileIndex < allFiles.size());

    FileHeader& fh = allFiles[curFileIndex];
    uchar* buf = fh.dataSource->OpenFile(fh.filePath, _str(fh.filePath).lower(), fh.fileSize, fh.writeTime);
    RUNTIME_ASSERT(buf);
    return File(fh.fileName, fh.filePath, fh.fileSize, fh.writeTime, fh.dataSource, buf);
}

FileHeader FileCollection::GetCurFileHeader()
{
    RUNTIME_ASSERT(curFileIndex >= 0);
    RUNTIME_ASSERT(curFileIndex < allFiles.size());

    FileHeader& fh = allFiles[curFileIndex];
    return FileHeader(fh.fileName, fh.filePath, fh.fileSize, fh.writeTime, fh.dataSource);
}

File FileCollection::FindFile(const string& name)
{
    for (FileHeader& fh : allFiles)
    {
        if (fh.fileName == name)
        {
            uchar* buf = fh.dataSource->OpenFile(fh.filePath, _str(fh.filePath).lower(), fh.fileSize, fh.writeTime);
            RUNTIME_ASSERT(buf);
            return File(fh.fileName, fh.filePath, fh.fileSize, fh.writeTime, fh.dataSource, buf);
        }
    }
    return File {};
}

FileHeader FileCollection::FindFileHeader(const string& name)
{
    for (FileHeader& fh : allFiles)
        if (fh.fileName == name)
            return FileHeader(fh.fileName, fh.filePath, fh.fileSize, fh.writeTime, fh.dataSource);
    return FileHeader {};
}

uint FileCollection::GetFilesCount()
{
    return (uint)allFiles.size();
}

void FileCollection::ResetCounter()
{
    curFileIndex = -1;
}

void FileManager::AddDataSource(const string& path, bool cache_dirs)
{
    dataSources.push_back(DataSource(path, cache_dirs));
}

FileCollection FileManager::FilterFiles(const string& ext, const string& dir, bool include_subdirs)
{
    vector<FileHeader> files;
    for (DataSource& ds : dataSources)
    {
        StrVec file_names;
        ds.GetFileNames(dir, include_subdirs, ext, file_names);

        for (string& fname : file_names)
        {
            uint size;
            uint64 write_time;
            bool ok = ds.IsFilePresent(fname, _str(fname).lower(), size, write_time);
            RUNTIME_ASSERT(ok);
            string name = _str(fname).extractFileName().eraseFileExtension();
            files.push_back(FileHeader(name, fname, size, write_time, &ds));
        }
    }

    return FileCollection(dir, std::move(files));
}

File FileManager::ReadFile(const string& path)
{
    RUNTIME_ASSERT(!path.empty());
    RUNTIME_ASSERT((path[0] != '.' && path[0] != '/'));

    string path_lower = _str(path).lower();
    string name = _str(path).extractFileName().eraseFileExtension();

    for (auto& ds : dataSources)
    {
        uint file_size;
        uint64 write_time;
        uchar* buf = ds.OpenFile(path, path_lower, file_size, write_time);
        if (buf)
            return File(name, path, file_size, write_time, &ds, buf);
    }
    return File(name, path, 0, 0, nullptr, nullptr);
}

FileHeader FileManager::ReadFileHeader(const string& path)
{
    RUNTIME_ASSERT(!path.empty());
    RUNTIME_ASSERT((path[0] != '.' && path[0] != '/'));

    string path_lower = _str(path).lower();
    string name = _str(path).extractFileName().eraseFileExtension();

    for (auto& ds : dataSources)
    {
        uint file_size;
        uint64 write_time;
        if (ds.IsFilePresent(path, path_lower, file_size, write_time))
            return FileHeader(name, path, file_size, write_time, &ds);
    }
    return FileHeader(name, path, 0, 0, nullptr);
}

ConfigFile FileManager::ReadConfigFile(const string& path)
{
    File file = ReadFile(path);
    if (file)
        return ConfigFile(file.GetCStr());
    return ConfigFile("");
}

OutputFile FileManager::WriteFile(const string& path, bool apply)
{
    DiskFileSystem::SetCurrentDir(rootPath);
    // Todo: handle apply file writing
    DiskFile file = DiskFileSystem::OpenFile(path, true);
    if (!file)
        throw FileSystemExeption("Can't open file for writing", path, apply);
    return OutputFile(std::move(file));
}

void FileManager::DeleteFile(const string& path)
{
    DiskFileSystem::SetCurrentDir(rootPath);
    DiskFileSystem::DeleteFile(path);
}

void FileManager::DeleteDir(const string& path)
{
    DiskFileSystem::SetCurrentDir(rootPath);
    DiskFileSystem::DeleteDir(path);
}

void FileManager::RenameFile(const string& from_path, const string& to_path)
{
    DiskFileSystem::SetCurrentDir(rootPath);
    DiskFileSystem::RenameFile(from_path, to_path);
}
