#include "Common.h"

char* NormalizePathSlashes( char* path )
{
    for( char* s = path; *s; s++ )
        if( *s == '\\' )
            *s = '/';
    return path;
}

void CreateDirectoryTree( const char* path )
{
    char* work = Str::Duplicate( path );
    NormalizePathSlashes( work );
    for( char* ptr = work; *ptr; ++ptr )
    {
        if( *ptr == '/' )
        {
            *ptr = 0;
            MakeDirectory( work );
            *ptr = '/';
        }
    }
    delete[] work;
}

#ifdef FO_WINDOWS

# include <io.h>

static wchar_t* MBtoWC( const char* mb, wchar_t* buf )
{
    char mb_[ MAX_FOPATH ];
    Str::Copy( mb_, mb );
    for( char* s = mb_; *s; s++ )
        if( *s == '/' )
            *s = '\\';
    if( MultiByteToWideChar( CP_UTF8, 0, mb_, -1, buf, MAX_FOPATH - 1 ) == 0 )
        buf[ 0 ] = 0;
    return buf;
}

static char* WCtoMB( const wchar_t* wc, char* buf )
{
    if( WideCharToMultiByte( CP_UTF8, 0, wc, -1, buf, MAX_FOPATH - 1, NULL, NULL ) == 0 )
        buf[ 0 ] = 0;
    NormalizePathSlashes( buf );
    return buf;
}

static uint64 FileTimeToUInt64( FILETIME ft )
{
    union
    {
        FILETIME       ft;
        ULARGE_INTEGER ul;
    } t;
    t.ft = ft;
    return PACKUINT64( t.ul.HighPart, t.ul.LowPart );
}

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    wchar_t wc[ MAX_FOPATH ];
    HANDLE  file;
    if( write )
    {
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
        if( file == INVALID_HANDLE_VALUE )
        {
            CreateDirectoryTree( fname );
            file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
        }
    }
    else
    {
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    }
    if( file == INVALID_HANDLE_VALUE )
        return NULL;
    return file;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    wchar_t wc[ MAX_FOPATH ];
    HANDLE  file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
    if( file == INVALID_HANDLE_VALUE )
    {
        CreateDirectoryTree( fname );
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
    }
    if( file == INVALID_HANDLE_VALUE )
        return NULL;
    if( !FileSetPointer( file, 0, SEEK_END ) )
    {
        FileClose( file );
        return NULL;
    }
    return file;
}

void* FileOpenForReadWrite( const char* fname, bool write_through /* = false */ )
{
    wchar_t wc[ MAX_FOPATH ];
    HANDLE  file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
    if( file == INVALID_HANDLE_VALUE )
    {
        CreateDirectoryTree( fname );
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
    }
    if( file == INVALID_HANDLE_VALUE )
        return NULL;
    return file;
}

void FileClose( void* file )
{
    if( file )
        CloseHandle( (HANDLE) file );
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    DWORD dw = 0;
    BOOL  result = ReadFile( (HANDLE) file, buf, len, &dw, NULL );
    if( rb )
        *rb = dw;
    return result && dw == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    DWORD dw = 0;
    return WriteFile( (HANDLE) file, buf, len, &dw, NULL ) && dw == len;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return SetFilePointer( (HANDLE) file, offset, NULL, origin ) != INVALID_SET_FILE_POINTER;
}

uint FileGetPointer( void* file )
{
    return (uint) SetFilePointer( (HANDLE) file, 0, NULL, FILE_CURRENT );
}

uint64 FileGetWriteTime( void* file )
{
    FILETIME tc, ta, tw;
    GetFileTime( (HANDLE) file, &tc, &ta, &tw );
    return FileTimeToUInt64( tw );
}

uint FileGetSize( void* file )
{
    DWORD high;
    return GetFileSize( (HANDLE) file, &high );
}

bool FileDelete( const char* fname )
{
    wchar_t wc[ MAX_FOPATH ];
    return DeleteFileW( MBtoWC( fname, wc ) ) != FALSE;
}

bool FileExist( const char* fname )
{
    wchar_t wc[ MAX_FOPATH ];
    return !_waccess( MBtoWC( fname, wc ), 0 );
}

bool FileRename( const char* fname, const char* new_fname )
{
    wchar_t wc1[ MAX_FOPATH ];
    wchar_t wc2[ MAX_FOPATH ];
    return MoveFileW( MBtoWC( fname, wc1 ), MBtoWC( new_fname, wc2 ) ) != FALSE;
}

void* FileFindFirst( const char* path, const char* extension, FindData& fd )
{
    char query[ MAX_FOPATH ];
    if( extension )
        Str::Format( query, "%s*.%s", path, extension );
    else
        Str::Format( query, "%s*", path );

    WIN32_FIND_DATAW wfd;
    wchar_t          wc[ MAX_FOPATH ];
    HANDLE           h = FindFirstFileW( MBtoWC( query, wc ), &wfd );
    if( h == INVALID_HANDLE_VALUE )
        return NULL;

    char mb[ MAX_FOPATH ];
    Str::Copy( fd.FileName, WCtoMB( wfd.cFileName, mb ) );
    fd.IsDirectory = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    fd.FileSize = wfd.nFileSizeLow;
    fd.WriteTime = FileTimeToUInt64( wfd.ftLastWriteTime );
    if( fd.IsDirectory && ( Str::Compare( fd.FileName, "." ) || Str::Compare( fd.FileName, ".." ) ) )
        if( !FileFindNext( h, fd ) )
            return false;

    return h;
}

bool FileFindNext( void* descriptor, FindData& fd )
{
    WIN32_FIND_DATAW wfd;
    if( !FindNextFileW( (HANDLE) descriptor, &wfd ) )
        return false;

    char mb[ MAX_FOPATH ];
    Str::Copy( fd.FileName, WCtoMB( wfd.cFileName, mb ) );
    fd.IsDirectory = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    fd.FileSize = wfd.nFileSizeLow;
    fd.WriteTime = FileTimeToUInt64( wfd.ftLastWriteTime );
    if( fd.IsDirectory && ( Str::Compare( fd.FileName, "." ) || Str::Compare( fd.FileName, ".." ) ) )
        return FileFindNext( (HANDLE) descriptor, fd );

    return true;
}

void FileFindClose( void* descriptor )
{
    if( descriptor )
        FindClose( (HANDLE) descriptor );
}

bool MakeDirectory( const char* path )
{
    wchar_t wc[ MAX_FOPATH ];
    return CreateDirectoryW( MBtoWC( path, wc ), NULL ) != FALSE;
}

bool ResolvePath( char* path )
{
    wchar_t path_[ MAX_FOPATH ];
    wchar_t wc[ MAX_FOPATH ];
    if( !GetFullPathNameW( MBtoWC( path, wc ), MAX_FOPATH, path_, NULL ) )
        return false;
    char mb[ MAX_FOPATH ];
    Str::Copy( path, MAX_FOPATH, WCtoMB( path_, mb ) );
    return true;
}

#else

# include <dirent.h>
# include <sys/stat.h>

struct FileDesc
{
    FILE* f;
    bool  writeThrough;
};

static void SetRelativePath( const char* fname, char* result )
{
    result[ 0 ] = 0;
    if( fname[ 0 ] != '.' )
        Str::Copy( result, MAX_FOPATH, "./" );
    Str::Append( result, MAX_FOPATH, fname );
    NormalizePathSlashes( result );
}

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    SetRelativePath( fname, fname_ );

    FILE* f = fopen( fname_, write ? "wb" : "rb" );
    if( !f && write )
    {
        CreateDirectoryTree( fname_ );
        f = fopen( fname_, "wb" );
    }
    if( !f )
        return NULL;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    SetRelativePath( fname, fname_ );

    FILE* f = fopen( fname_, "ab" );
    if( !f )
    {
        CreateDirectoryTree( fname_ );
        f = fopen( fname_, "ab" );
    }
    if( !f )
        return NULL;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void* FileOpenForReadWrite( const char* fname, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    SetRelativePath( fname, fname_ );

    FILE* f = fopen( fname_, "r+b" );
    if( !f )
    {
        CreateDirectoryTree( fname_ );
        f = fopen( fname_, "r+b" );
    }
    if( !f )
        return NULL;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void FileClose( void* file )
{
    if( file )
    {
        fclose( ( (FileDesc*) file )->f );
        delete (FileDesc*) file;
    }
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    uint rb_ = (uint) fread( buf, sizeof( char ), len, ( (FileDesc*) file )->f );
    if( rb )
        *rb = rb_;
    return rb_ == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    bool result = ( (uint) fwrite( buf, sizeof( char ), len, ( (FileDesc*) file )->f ) == len );
    if( result && ( (FileDesc*) file )->writeThrough )
        fflush( ( (FileDesc*) file )->f );
    return result;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return fseek( ( (FileDesc*) file )->f, offset, origin ) == 0;
}

uint FileGetPointer( void* file )
{
    return (uint) ftell( ( (FileDesc*) file )->f );
}

uint64 FileGetWriteTime( void* file )
{
    int         fd = fileno( ( (FileDesc*) file )->f );
    struct stat st;
    fstat( fd, &st );
    return (uint64) st.st_mtime;
}

uint FileGetSize( void* file )
{
    int         fd = fileno( ( (FileDesc*) file )->f );
    struct stat st;
    fstat( fd, &st );
    return (uint) st.st_size;
}

bool FileDelete( const char* fname )
{
    char fname_[ MAX_FOPATH ];
    SetRelativePath( fname, fname_ );

    return std::remove( fname_ );
}

bool FileExist( const char* fname )
{
    char fname_[ MAX_FOPATH ];
    SetRelativePath( fname, fname_ );

    return !access( fname_, 0 );
}

bool FileRename( const char* fname, const char* new_fname )
{
    char fname_[ MAX_FOPATH ];
    SetRelativePath( fname, fname_ );
    char new_fname_[ MAX_FOPATH ];
    SetRelativePath( new_fname, new_fname_ );

    return !rename( fname_, new_fname_ );
}

struct FileFind
{
    DIR* d;
    char path[ MAX_FOPATH ];
    char ext[ 32 ];
};

void* FileFindFirst( const char* path, const char* extension, FindData& fd )
{
    char path_[ MAX_FOPATH ];
    SetRelativePath( path, path_ );

    // Open dir
    DIR* h = opendir( path_ );
    if( !h )
        return NULL;

    // Create own descriptor
    FileFind* ff = new FileFind;
    memzero( ff, sizeof( FileFind ) );
    ff->d = h;
    Str::Copy( ff->path, path_ );
    if( ff->path[ Str::Length( ff->path ) - 1 ] != '/' )
        Str::Append( ff->path, "/" );
    if( extension )
        Str::Copy( ff->ext, extension );

    // Find first entire
    if( !FileFindNext( ff, fd ) )
    {
        FileFindClose( ff );
        return NULL;
    }
    return ff;
}

bool FileFindNext( void* descriptor, FindData& fd )
{
    // Cast descriptor
    FileFind* ff = (FileFind*) descriptor;

    // Read entire
    struct dirent* ent = readdir( ff->d );
    if( !ent )
        return false;

    // Skip '.' and '..'
    if( Str::Compare( ent->d_name, "." ) || Str::Compare( ent->d_name, ".." ) )
        return FileFindNext( descriptor, fd );

    // Read entire information
    bool        valid = false;
    bool        dir = false;
    char        fname[ MAX_FOPATH ];
    uint        file_size;
    uint64      write_time;
    Str::Format( fname, "%s%s", ff->path, ent->d_name );
    struct stat st;
    if( !stat( fname, &st ) )
    {
        dir = S_ISDIR( st.st_mode );
        if( dir || S_ISREG( st.st_mode ) )
        {
            valid = true;
            file_size = (uint) st.st_size;
            write_time = (uint64) st.st_mtime;
        }
    }

    // Skip not dirs and regular files
    if( !valid )
        return FileFindNext( descriptor, fd );

    // Find by extensions
    if( ff->ext[ 0 ] )
    {
        // Skip dirs
        if( dir )
            return FileFindNext( descriptor, fd );

        // Compare extension
        const char* ext = NULL;
        for( const char* name = ent->d_name; *name; name++ )
            if( *name == '.' )
                ext = name;
        if( !ext || !*( ++ext ) || !Str::CompareCase( ext, ff->ext ) )
            return FileFindNext( descriptor, fd );
    }

    // Fill find data
    Str::Copy( fd.FileName, ent->d_name );
    fd.IsDirectory = dir;
    fd.FileSize = file_size;
    fd.WriteTime = write_time;
    return true;
}

void FileFindClose( void* descriptor )
{
    if( descriptor )
    {
        FileFind* ff = (FileFind*) descriptor;
        closedir( ff->d );
        delete ff;
    }
}

bool MakeDirectory( const char* path )
{
    char path_[ MAX_FOPATH ];
    SetRelativePath( path, path_ );

    return mkdir( path_, ALLPERMS ) == 0;
}

bool ResolvePath( char* path )
{
    char path_[ MAX_FOPATH ];
    SetRelativePath( path, path_ );

    char path__[ MAX_FOPATH ];
    realpath( path_, path__ );
    Str::Copy( path, MAX_FOPATH, path__ );
    return true;
}

#endif
