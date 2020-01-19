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
    void AddDataSource(const string& path);
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
