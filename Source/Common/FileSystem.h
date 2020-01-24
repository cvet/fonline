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

#pragma once

#include "Common.h"

#include "ConfigFile.h"
#include "DataSource.h"
#include "DiskFileSystem.h"

DECLARE_EXCEPTION(FileSystemExeption);

class FileHeader : public NonCopyable
{
    friend class FileManager;
    friend class FileCollection;
    friend class File;

public:
    operator bool() const;
    const string& GetName();
    const string& GetPath();
    uint GetFsize();
    uint64 GetWriteTime();

protected:
    FileHeader() = default;
    FileHeader(const string& name, const string& path, uint size, uint64 write_time, DataSource* ds);

    bool isLoaded {};
    string fileName {};
    string filePath {};
    uint fileSize {};
    uint64 writeTime {};
    DataSource* dataSource {};
};

class File : public FileHeader
{
    friend class FileManager;
    friend class FileCollection;

public:
    File() = default;
    File(uchar* buf, uint size);
    const char* GetCStr();
    uchar* GetBuf();
    uchar* GetCurBuf();
    uint GetCurPos();
    uchar* ReleaseBuffer();
    void SetCurPos(uint pos);
    void GoForward(uint offs);
    void GoBack(uint offs);
    bool FindFragment(const uchar* fragment, uint fragment_len, uint begin_offs);
    string GetNonEmptyLine();
    void CopyMem(void* ptr, uint size);
    string GetStrNT(); // Null terminated
    uchar GetUChar();
    ushort GetBEUShort();
    short GetBEShort() { return (short)GetBEUShort(); }
    ushort GetLEUShort();
    short GetLEShort() { return (short)GetLEUShort(); }
    uint GetBEUInt();
    uint GetLEUInt();
    uint GetLE3UChar();
    float GetBEFloat();
    float GetLEFloat();

private:
    File(const string& name, const string& path, uint size, uint64 write_time, DataSource* ds, uchar* buf);

    unique_ptr<uchar[]> fileBuf {};
    uint curPos {};
};

class OutputFile : public NonCopyable
{
    friend class FileManager;

public:
    void Save();
    uchar* GetOutBuf();
    uint GetOutBufLen();
    void SetData(const void* data, uint len);
    void SetStr(const string& str);
    void SetStrNT(const string& str);
    void SetUChar(uchar data);
    void SetBEUShort(ushort data);
    void SetBEShort(short data) { SetBEUShort((ushort)data); }
    void SetLEUShort(ushort data);
    void SetBEUInt(uint data);
    void SetLEUInt(uint data);

private:
    OutputFile(DiskFile file);

    DiskFile diskFile;
    vector<uchar> dataBuf {};
    DataWriter dataWriter {dataBuf};
};

class FileCollection : public NonCopyable
{
    friend class FileManager;

public:
    const string& GetPath();
    bool MoveNext();
    File GetCurFile();
    FileHeader GetCurFileHeader();
    File FindFile(const string& name);
    FileHeader FindFileHeader(const string& name);
    uint GetFilesCount();
    void ResetCounter();

private:
    FileCollection(const string& path, vector<FileHeader> files);

    string filterPath {};
    vector<FileHeader> allFiles {};
    int curFileIndex {-1};
};

class FileManager : public NonCopyable
{
public:
    FileManager() = default;
    void AddDataSource(const string& path, bool cache_dirs);
    FileCollection FilterFiles(const string& ext, const string& dir = "", bool include_subdirs = true);
    File ReadFile(const string& path);
    FileHeader ReadFileHeader(const string& path);
    ConfigFile ReadConfigFile(const string& path);
    OutputFile WriteFile(const string& path, bool apply = false);
    void DeleteFile(const string& path);
    void DeleteDir(const string& path);
    void RenameFile(const string& from_path, const string& to_path);

    EventObserver<DataSource*> OnDataSourceAdded {};

private:
    string rootPath {};
    vector<DataSource> dataSources {};
    EventDispatcher<DataSource*> dataSourceAddedDispatcher {OnDataSourceAdded};
};
