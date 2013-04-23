#include "StdAfx.h"

#ifdef FO_WINDOWS

# include <io.h>

wchar_t* MBtoWC( const char* mb )
{
    static THREAD wchar_t wc[ MAX_FOTEXT ];
    if( MultiByteToWideChar( CP_UTF8, 0, mb, -1, wc, MAX_FOTEXT - 1 ) == 0 )
        wc[ 0 ] = 0;
    return wc;
}

char* WCtoMB( const wchar_t* wc )
{
    static THREAD char mb[ MAX_FOTEXT ];
    if( WideCharToMultiByte( CP_UTF8, 0, wc, -1, mb, MAX_FOTEXT - 1, NULL, NULL ) == 0 )
        mb[ 0 ] = 0;
    return mb;
}

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    HANDLE file;
    if( write )
        file = CreateFileW( MBtoWC( fname ), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
    else
        file = CreateFileW( MBtoWC( fname ), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( file == INVALID_HANDLE_VALUE )
        return NULL;
    return file;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    HANDLE file = CreateFileW( MBtoWC( fname ), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, NULL );
    if( file == INVALID_HANDLE_VALUE )
        return NULL;
    if( !FileSetPointer( file, 0, SEEK_END ) )
    {
        FileClose( file );
        return NULL;
    }
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

void FileGetTime( void* file, uint64& tc, uint64& ta, uint64& tw )
{
    union
    {
        FILETIME       ft;
        ULARGE_INTEGER ul;
    } tc_, ta_, tw_;
    GetFileTime( (HANDLE) file, &tc_.ft, &ta_.ft, &tw_.ft );
    tc = PACKUINT64( tc_.ul.HighPart, tc_.ul.LowPart );
    ta = PACKUINT64( ta_.ul.HighPart, ta_.ul.LowPart );
    tw = PACKUINT64( tw_.ul.HighPart, tw_.ul.LowPart );
}

uint FileGetSize( void* file )
{
    DWORD high;
    return GetFileSize( (HANDLE) file, &high );
}

bool FileDelete( const char* fname )
{
    return DeleteFileW( MBtoWC( fname ) ) != FALSE;
}

bool FileExist( const char* fname )
{
    return !_waccess( MBtoWC( fname ), 0 );
}

bool FileRename( const char* fname, const char* new_fname )
{
    return MoveFileW( MBtoWC( fname ), MBtoWC( new_fname ) ) != FALSE;
}

void* FileFindFirst( const char* path, const char* extension, FIND_DATA& fd )
{
    char query[ MAX_FOPATH ];
    if( extension )
        Str::Format( query, "%s*.%s", path, extension );
    else
        Str::Format( query, "%s*", path );

    WIN32_FIND_DATAW wfd;
    HANDLE           h = FindFirstFileW( MBtoWC( query ), &wfd );
    if( h == INVALID_HANDLE_VALUE )
        return NULL;

    Str::Copy( fd.FileName, WCtoMB( wfd.cFileName ) );
    fd.IsDirectory = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    if( fd.IsDirectory && ( Str::Compare( fd.FileName, "." ) || Str::Compare( fd.FileName, ".." ) ) )
        if( !FileFindNext( h, fd ) )
            return false;

    return h;
}

bool FileFindNext( void* descriptor, FIND_DATA& fd )
{
    WIN32_FIND_DATAW wfd;
    if( !FindNextFileW( (HANDLE) descriptor, &wfd ) )
        return false;

    Str::Copy( fd.FileName, WCtoMB( wfd.cFileName ) );
    fd.IsDirectory = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
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
    return CreateDirectoryW( MBtoWC( path ), NULL ) != FALSE;
}

char* FixPathSlashes( char* path )
{
    char* path_ = path;
    while( *path )
    {
        if( *path == '/' )
            *path = '\\';
        ++path;
    }
    return path_;
}

bool ResolvePath( char* path )
{
    wchar_t path_[ MAX_FOPATH ];
    if( !GetFullPathNameW( MBtoWC( path ), MAX_FOPATH, path_, NULL ) )
        return false;
    Str::Copy( path, MAX_FOPATH, WCtoMB( path_ ) );
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

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    FILE* f = fopen( fname, write ? "wb" : "rb" );
    if( !f )
        return NULL;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    if( access( fname, 0 ) )
        return NULL;
    FILE* f = fopen( fname, "ab" );
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
    uint rb_ = fread( buf, sizeof( char ), len, ( (FileDesc*) file )->f );
    if( rb )
        *rb = rb_;
    return rb_ == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    bool result = ( fwrite( buf, sizeof( char ), len, ( (FileDesc*) file )->f ) == len );
    if( result && ( (FileDesc*) file )->writeThrough )
        fflush( ( (FileDesc*) file )->f );
    return result;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return fseek( ( (FileDesc*) file )->f, offset, origin ) == 0;
}

void FileGetTime( void* file, uint64& tc, uint64& ta, uint64& tw )
{
    int         fd = fileno( ( (FileDesc*) file )->f );
    struct stat st;
    fstat( fd, &st );
    tc = (uint64) st.st_mtime;
    ta = (uint64) st.st_atime;
    tw = (uint64) st.st_mtime;
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
    return std::remove( fname );
}

bool FileExist( const char* fname )
{
    return !access( fname, 0 );
}

bool FileRename( const char* fname, const char* new_fname )
{
    return !rename( fname, new_fname );
}

struct FileFind
{
    DIR* d;
    char path[ MAX_FOPATH ];
    char ext[ 32 ];
};

void* FileFindFirst( const char* path, const char* extension, FIND_DATA& fd )
{
    // Open dir
    DIR* h = opendir( path );
    if( !h )
        return NULL;

    // Create own descriptor
    FileFind* ff = new FileFind;
    memzero( ff, sizeof( FileFind ) );
    ff->d = h;
    Str::Copy( ff->path, path );
    if( ff->path[ Str::Length( ff->path ) - 1 ] != DIR_SLASH_C )
        Str::Append( ff->path, DIR_SLASH_S );
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

bool FileFindNext( void* descriptor, FIND_DATA& fd )
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
    Str::Format( fname, "%s%s", ff->path, ent->d_name );
    struct stat st;
    if( !stat( fname, &st ) )
    {
        dir = S_ISDIR( st.st_mode );
        if( dir || S_ISREG( st.st_mode ) )
            valid = true;
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
    mkdir( path, ALLPERMS ) == 0;
}

char* FixPathSlashes( char* path )
{
    char* path_ = path;
    while( *path )
    {
        if( *path == '\\' )
            *path = '/';
        ++path;
    }
    return path_;
}

bool ResolvePath( char* path )
{
    char path_[ 4096 ];
    realpath( path, path_ );
    Str::Copy( path, MAX_FOPATH, path_ );
    return true;
}

#endif
