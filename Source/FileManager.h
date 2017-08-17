#ifndef __FILE_MANAGER__
#define __FILE_MANAGER__

#include "Defines.h"
#include "Log.h"
#include "DataFile.h"

#ifdef FO_MSVC
# undef CopyFile
# undef DeleteFile
#endif

struct FindData
{
    string FileName;
    uint   FileSize;
    uint64 WriteTime;
    bool   IsDirectory;
};
typedef vector< FindData > FindDataVec;

class FileManager
{
public:
    static void InitDataFiles( const char* path, bool set_write_dir = true );
    static bool LoadDataFile( const char* path, bool skip_inner = false );
    static void ClearDataFiles();

    bool   LoadFile( const char* path, bool no_read = false );
    bool   LoadStream( const uchar* stream, uint length );
    void   UnloadFile();
    uchar* ReleaseBuffer();

    void SetCurPos( uint pos );
    void GoForward( uint offs );
    void GoBack( uint offs );
    bool FindFragment( const uchar* fragment, uint fragment_len, uint begin_offs );

    string GetNonEmptyLine();
    bool   CopyMem( void* ptr, uint size );
    void   GetStrNT( char* str ); // Null terminated
    uchar  GetUChar();
    ushort GetBEUShort();
    short  GetBEShort() { return (short) GetBEUShort(); }
    ushort GetLEUShort();
    short  GetLEShort() { return (short) GetLEUShort(); }
    uint   GetBEUInt();
    uint   GetLEUInt();
    uint   GetLE3UChar();
    float  GetBEFloat();
    float  GetLEFloat();

    void   SwitchToRead();
    void   SwitchToWrite();
    bool   ResizeOutBuf();
    void   SetPosOutBuf( uint pos );
    uchar* GetOutBuf()    { return dataOutBuf; }
    uint   GetOutBufLen() { return endOutBuf; }
    bool   SaveFile( const char* fname );

    void SetData( void* data, uint len );
    void SetStr( const char* fmt, ... );
    void SetStrNT( const char* fmt, ... );
    void SetUChar( uchar data );
    void SetBEUShort( ushort data );
    void SetBEShort( short data ) { SetBEUShort( (ushort) data ); }
    void SetLEUShort( ushort data );
    void SetBEUInt( uint data );
    void SetLEUInt( uint data );

    static void        ResetCurrentDir();
    static void        SetCurrentDir( const char* dir, const char* write_dir );
    static const char* GetWritePath( const char* fname );
    static void        GetWritePath( const char* fname, char* result );
    static hash        GetPathHash( const char* fname );
    static void        FormatPath( char* path );
    static void        ExtractDir( const char* path, char* dir );
    static void        ExtractFileName( const char* path, char* name );
    static void        CombinePath( const char* base_path, const char* path, char* result );
    static const char* GetExtension( const char* path ); // EXT without dot
    static char*       EraseExtension( char* path );     // Erase EXT with dot
    static string      ForwardPath( const char* path, const char* relative_dir );
    static bool        IsFileExists( const char* fname );
    static bool        CopyFile( const char* from, const char* to );
    static bool        RenameFile( const char* from, const char* to );
    static bool        DeleteFile( const char* fname );
    static void        DeleteDir( const char* dir );
    static void        CreateDirectoryTree( const char* path );
    static bool        ResolvePathInplace( char* path );
    static void        NormalizePathSlashesInplace( char* path );

    bool        IsLoaded()     { return fileSize != 0; }
    uchar*      GetBuf()       { return fileBuf; }
    const char* GetCStr()      { return (const char*) fileBuf; }
    uchar*      GetCurBuf()    { return fileBuf + curPos; }
    uint        GetCurPos()    { return curPos; }
    uint        GetFsize()     { return fileSize; }
    bool        IsEOF()        { return curPos >= fileSize; }
    uint64      GetWriteTime() { return writeTime; }

    static DataFileVec& GetDataFiles() { return dataFiles; }
    static void         GetFolderFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& files_path, FindDataVec* files = nullptr, StrVec* dirs_path = nullptr, FindDataVec* dirs = nullptr );
    static void         GetDataFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result );

    FileManager();
    ~FileManager();

private:
    static DataFileVec dataFiles;
    static string      writeDir;

    uint               fileSize;
    uchar*             fileBuf;
    uint               curPos;

    uchar*             dataOutBuf;
    uint               posOutBuf;
    uint               endOutBuf;
    uint               lenOutBuf;

    uint64             writeTime;

    static void RecursiveDirLook( const char* base_dir, const char* cur_dir, bool include_subdirs, const char* ext, StrVec& files_path, FindDataVec* files, StrVec* dirs_path, FindDataVec* dirs );
};

class FilesCollection
{
public:
    FilesCollection( const char* ext, const char* fixed_dir = nullptr );
    bool         IsNextFile();
    FileManager& GetNextFile( const char** name = nullptr, const char** path = nullptr, const char** relative_path = nullptr, bool no_read_data = false );
    FileManager& FindFile( const char* name, const char** path = nullptr, const char** relative_path = nullptr, bool no_read_data = false );
    uint         GetFilesCount();
    void         ResetCounter();

private:
    StrVec      fileNames;
    StrVec      filePaths;
    StrVec      fileRelativePaths;
    uint        curFileIndex;
    FileManager curFile;
};

#endif // __FILE_MANAGER__
