#ifndef __FILE_MANAGER__
#define __FILE_MANAGER__

#include "Defines.h"
#include "FileSystem.h"
#include "Log.h"
#include "DataFile.h"

// Paths
#define PT_ROOT               ( -1 )
#define PT_DATA               ( 0 )
#define PT_ART                ( 2 )
#define PT_ART_CRITTERS       ( 3 )
#define PT_ART_INTRFACE       ( 4 )
#define PT_ART_INVEN          ( 5 )
#define PT_ART_ITEMS          ( 6 )
#define PT_ART_MISC           ( 7 )
#define PT_ART_SCENERY        ( 8 )
#define PT_ART_SKILLDEX       ( 9 )
#define PT_ART_SPLASH         ( 10 )
#define PT_ART_TILES          ( 11 )
#define PT_ART_WALLS          ( 12 )
#define PT_TEXTURES           ( 13 )
#define PT_EFFECTS            ( 14 )
#define PT_SND_MUSIC          ( 16 )
#define PT_SND_SFX            ( 17 )
#define PT_SCRIPTS            ( 18 )
#define PT_VIDEO              ( 19 )
#define PT_TEXTS              ( 20 )
#define PT_SAVE               ( 21 )
#define PT_FONTS              ( 22 )
#define PT_CACHE              ( 23 )
#define PT_SERVER_CONFIGS     ( 31 )
#define PT_SERVER_SAVE        ( 38 )
#define PT_SERVER_CLIENTS     ( 39 )
#define PT_SERVER_BANS        ( 40 )
#define PT_SERVER_LOGS        ( 41 )
#define PT_SERVER_DUMPS       ( 42 )
#define PT_SERVER_PROFILER    ( 43 )
#define PT_SERVER_UPDATE      ( 44 )
#define PT_SERVER_CACHE       ( 45 )
#define PT_SERVER_CONTENT     ( 49 )
#define PATH_LIST_COUNT       ( 50 )
extern const char* PathList[ PATH_LIST_COUNT ];

class FileManager
{
public:
    static void InitDataFiles( const char* path );
    static bool LoadDataFile( const char* path );
    static void ClearDataFiles();

    bool   LoadFile( const char* fname, int path_type, bool no_read_data = false );
    bool   LoadStream( const uchar* stream, uint length );
    void   UnloadFile();
    uchar* ReleaseBuffer();

    void SetCurPos( uint pos );
    void GoForward( uint offs );
    void GoBack( uint offs );
    bool FindFragment( const uchar* fragment, uint fragment_len, uint begin_offs );

    bool   GetLine( char* str, uint len );
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
    void   ClearOutBuf();
    bool   ResizeOutBuf();
    void   SetPosOutBuf( uint pos );
    bool   SaveOutBufToFile( const char* fname, int path_type );
    uchar* GetOutBuf()    { return dataOutBuf; }
    uint   GetOutBufLen() { return endOutBuf; }

    void SetData( void* data, uint len );
    void SetStr( const char* fmt, ... );
    void SetStrNT( const char* fmt, ... );
    void SetUChar( uchar data );
    void SetBEUShort( ushort data );
    void SetBEShort( short data ) { SetBEUShort( (ushort) data ); }
    void SetLEUShort( ushort data );
    void SetBEUInt( uint data );
    void SetLEUInt( uint data );

    static const char* GetDataPath( const char* fname, int path_type );
    static void        GetDataPath( const char* fname, int path_type, char* result );
    static const char* GetReadPath( const char* fname, int path_type );
    static void        GetReadPath( const char* fname, int path_type, char* result );
    static const char* GetWritePath( const char* fname, int path_type );
    static void        GetWritePath( const char* fname, int path_type, char* result );
    static void        SetWritePath( const char* path );
    static hash        GetPathHash( const char* fname, int path_type );
    static void        FormatPath( char* path );
    static void        ExtractDir( const char* path, char* dir );
    static void        ExtractFileName( const char* path, char* name );
    static void        MakeFilePath( const char* name, const char* path, char* result );
    static const char* GetExtension( const char* fname ); // EXT without dot
    static char*       EraseExtension( char* fname );     // Erase EXT with dot
    static bool        CopyFile( const char* from, const char* to );
    static bool        DeleteFile( const char* fname );

    bool   IsLoaded()     { return fileSize != 0; }
    uchar* GetBuf()       { return fileBuf; }
    uchar* GetCurBuf()    { return fileBuf + curPos; }
    uint   GetCurPos()    { return curPos; }
    uint   GetFsize()     { return fileSize; }
    bool   IsEOF()        { return curPos >= fileSize; }
    uint64 GetWriteTime() { return writeTime; }
    int    ParseLinesInt( const char* fname, int path_type, IntVec& lines );

    static DataFileVec& GetDataFiles() { return dataFiles; }
    static void         GetFolderFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& files_path, FindDataVec* files = NULL, StrVec* dirs_path = NULL, FindDataVec* dirs = NULL );
    static void         GetDataFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result );

    FileManager();
    ~FileManager();

private:
    static DataFileVec dataFiles;
    static string      basePathes[ 2 ];     // Write and read

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
    FilesCollection( const char* ext, int path_type = PT_SERVER_CONTENT, const char* dir = NULL );
    bool         IsNextFile();
    FileManager& GetNextFile( const char** name = NULL, const char** path = NULL, bool no_read_data = false );
    FileManager& FindFile( const char* name, const char** path = NULL );
    uint         GetFilesCount();
    void         ResetCounter();

private:
    string      searchPath;
    StrVec      fileNames;
    StrVec      filePaths;
    uint        curFileIndex;
    FileManager curFile;
};

#endif // __FILE_MANAGER__
