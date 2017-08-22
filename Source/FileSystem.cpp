#include "FileSystem.h"

#ifdef FO_WINDOWS
# include <io.h>
#else
# include <dirent.h>
# include <sys/stat.h>
#endif

#ifdef FO_WINDOWS
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

static std::wstring MBtoWC( const string& mb )
{
    return _str( mb ).replace( '/', '\\' ).toWideChar();
}

static string WCtoMB( const wchar_t* wc )
{
    return _str().parseWideChar( wc ).normalizePathSlashes();
}

void* FileOpen( const string& fname, bool write, bool write_through /* = false */ )
{
    HANDLE file;
    if( write )
    {
        file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
        if( file == INVALID_HANDLE_VALUE )
        {
            MakeDirectoryTree( fname );
            file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
        }
    }
    else
    {
        file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr );
    }
    if( file == INVALID_HANDLE_VALUE )
        return nullptr;
    return file;
}

void* FileOpenForAppend( const string& fname, bool write_through /* = false */ )
{
    HANDLE file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    if( file == INVALID_HANDLE_VALUE )
    {
        MakeDirectoryTree( fname );
        file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    }
    if( file == INVALID_HANDLE_VALUE )
        return nullptr;
    if( !FileSetPointer( file, 0, SEEK_END ) )
    {
        FileClose( file );
        return nullptr;
    }
    return file;
}

void* FileOpenForReadWrite( const string& fname, bool write_through /* = false */ )
{
    HANDLE file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    if( file == INVALID_HANDLE_VALUE )
    {
        MakeDirectoryTree( fname );
        file = CreateFileW( MBtoWC( fname ).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    }
    if( file == INVALID_HANDLE_VALUE )
        return nullptr;
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
    BOOL  result = ReadFile( (HANDLE) file, buf, len, &dw, nullptr );
    if( rb )
        *rb = dw;
    return result && dw == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    DWORD dw = 0;
    return WriteFile( (HANDLE) file, buf, len, &dw, nullptr ) && dw == len;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return SetFilePointer( (HANDLE) file, offset, nullptr, origin ) != INVALID_SET_FILE_POINTER;
}

uint FileGetPointer( void* file )
{
    return (uint) SetFilePointer( (HANDLE) file, 0, nullptr, FILE_CURRENT );
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

#elif defined ( FO_ANDROID )

struct FileDesc
{
    SDL_RWops* Ops;
    bool       WriteThrough;
};

void* FileOpen( const string& fname, bool write, bool write_through /* = false */ )
{
    SDL_RWops* ops = SDL_RWFromFile( fname.c_str(), write ? "wb" : "rb" );
    if( !ops && write )
    {
        MakeDirectoryTree( fname );
        ops = SDL_RWFromFile( fname.c_str(), "wb" );
    }
    if( !ops )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->Ops = ops;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void* FileOpenForAppend( const string& fname, bool write_through /* = false */ )
{
    SDL_RWops* ops = SDL_RWFromFile( fname.c_str(), "ab" );
    if( !ops )
    {
        MakeDirectoryTree( fname );
        ops = SDL_RWFromFile( fname.c_str(), "ab" );
    }
    if( !ops )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->Ops = ops;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void* FileOpenForReadWrite( const string& fname, bool write_through /* = false */ )
{
    SDL_RWops* ops = SDL_RWFromFile( fname.c_str(), "r+b" );
    if( !ops )
    {
        MakeDirectoryTree( fname );
        ops = SDL_RWFromFile( fname.c_str(), "r+b" );
    }
    if( !ops )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->Ops = ops;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void FileClose( void* file )
{
    if( file )
    {
        SDL_RWclose( ( (FileDesc*) file )->Ops );
        delete (FileDesc*) file;
    }
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    uint rb_ = (uint) SDL_RWread( ( (FileDesc*) file )->Ops, buf, sizeof( char ), len );
    if( rb )
        *rb = rb_;
    return rb_ == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    SDL_RWops* ops = ( (FileDesc*) file )->Ops;
    bool       result = ( (uint) SDL_RWwrite( ops, buf, sizeof( char ), len ) == len );
    if( result && ( (FileDesc*) file )->WriteThrough )
    {
        if( ops->type == SDL_RWOPS_WINFILE )
        {
            # ifdef __WIN32__
            FlushFileBuffers( (HANDLE) ops->hidden.windowsio.h );
            # endif
        }
        else if( ops->type == SDL_RWOPS_STDFILE )
        {
            # ifdef HAVE_STDIO_H
            fflush( ops->hidden.stdio.fp );
            # endif
        }
    }
    return result;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return SDL_RWseek( ( ( (FileDesc*) file )->Ops ), offset, origin ) != -1;
}

uint FileGetPointer( void* file )
{
    return (uint) SDL_RWtell( ( (FileDesc*) file )->Ops );
}

uint64 FileGetWriteTime( void* file )
{
    SDL_RWops* ops = ( (FileDesc*) file )->Ops;
    if( ops->type == SDL_RWOPS_WINFILE )
    {
        # ifdef __WIN32__
        FILETIME tc, ta, tw;
        GetFileTime( (HANDLE) ops->hidden.windowsio.h, &tc, &ta, &tw );
        return FileTimeToUInt64( tw );
        # endif
    }
    else if( ops->type == SDL_RWOPS_STDFILE )
    {
        # ifdef HAVE_STDIO_H
        int         fd = fileno( ops->hidden.stdio.fp );
        struct stat st;
        fstat( fd, &st );
        return (uint64) st.st_mtime;
        # endif
    }
    return (uint64) 1;
}

uint FileGetSize( void* file )
{
    Sint64 size = SDL_RWsize( ( (FileDesc*) file )->Ops );
    return (uint) ( size <= 0 ? 0 : size );
}

#else

struct FileDesc
{
    FILE* File;
    bool  Write;
    bool  WriteThrough;
};

void* FileOpen( const string& fname, bool write, bool write_through /* = false */ )
{
    FILE* f = fopen( fname.c_str(), write ? "wb" : "rb" );
    if( !f && write )
    {
        MakeDirectoryTree( fname );
        f = fopen( fname.c_str(), "wb" );
    }
    if( !f )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->File = f;
    fd->Write = write;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void* FileOpenForAppend( const string& fname, bool write_through /* = false */ )
{
    FILE* f = fopen( fname.c_str(), "ab" );
    if( !f )
    {
        MakeDirectoryTree( fname );
        f = fopen( fname.c_str(), "ab" );
    }
    if( !f )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->File = f;
    fd->Write = true;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void* FileOpenForReadWrite( const string& fname, bool write_through /* = false */ )
{
    FILE* f = fopen( fname.c_str(), "r+b" );
    if( !f )
    {
        MakeDirectoryTree( fname );
        f = fopen( fname.c_str(), "r+b" );
    }
    if( !f )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->File = f;
    fd->Write = true;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void FileClose( void* file )
{
    if( file )
    {
        fclose( ( (FileDesc*) file )->File );

        # ifdef FO_WEB
        if( ( (FileDesc*) file )->Write )
        {
            EM_ASM(
                FS.syncfs( function( err ) {} );
                );
        }
        # endif

        delete (FileDesc*) file;
    }
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    uint rb_ = (uint) fread( buf, sizeof( char ), len, ( (FileDesc*) file )->File );
    if( rb )
        *rb = rb_;
    return rb_ == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    bool result = ( (uint) fwrite( buf, sizeof( char ), len, ( (FileDesc*) file )->File ) == len );
    if( result && ( (FileDesc*) file )->WriteThrough )
        fflush( ( (FileDesc*) file )->File );
    return result;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return fseek( ( (FileDesc*) file )->File, offset, origin ) == 0;
}

uint FileGetPointer( void* file )
{
    return (uint) ftell( ( (FileDesc*) file )->File );
}

uint64 FileGetWriteTime( void* file )
{
    int         fd = fileno( ( (FileDesc*) file )->File );
    struct stat st;
    fstat( fd, &st );
    return (uint64) st.st_mtime;
}

uint FileGetSize( void* file )
{
    int         fd = fileno( ( (FileDesc*) file )->File );
    struct stat st;
    fstat( fd, &st );
    return (uint) st.st_size;
}
#endif

#ifdef FO_WINDOWS
bool FileDelete( const string& fname )
{
    return DeleteFileW( MBtoWC( fname ).c_str() ) != FALSE;
}

bool FileExist( const string& fname )
{
    return !_waccess( MBtoWC( fname ).c_str(), 0 );
}

bool FileCopy( const string& fname, const string& copy_fname )
{
    return CopyFileW( MBtoWC( fname ).c_str(), MBtoWC( copy_fname ).c_str(), FALSE ) != FALSE;
}

bool FileRename( const string& fname, const string& new_fname )
{
    return MoveFileW( MBtoWC( fname ).c_str(), MBtoWC( new_fname ).c_str() ) != FALSE;
}

void* FileFindFirst( const string& path, const string& extension, string* fname, uint* fsize, uint64* wtime, bool* is_dir )
{
    string query = path + "*";
    if( !extension.empty() )
        query = "." + extension;

    WIN32_FIND_DATAW wfd;
    HANDLE           h = FindFirstFileW( MBtoWC( query ).c_str(), &wfd );
    if( h == INVALID_HANDLE_VALUE )
        return nullptr;

    if( fname )
        *fname = WCtoMB( wfd.cFileName );
    if( is_dir )
        *is_dir = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    if( fsize )
        *fsize = wfd.nFileSizeLow;
    if( wtime )
        *wtime = FileTimeToUInt64( wfd.ftLastWriteTime );
    if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && ( !wcscmp( wfd.cFileName, L"." ) || !wcscmp( wfd.cFileName, L".." ) ) )
        if( !FileFindNext( h, fname, fsize, wtime, is_dir ) )
            return nullptr;

    return h;
}

bool FileFindNext( void* descriptor, string* fname, uint* fsize, uint64* wtime, bool* is_dir )
{
    WIN32_FIND_DATAW wfd;
    if( !FindNextFileW( (HANDLE) descriptor, &wfd ) )
        return false;

    if( fname )
        *fname = WCtoMB( wfd.cFileName );
    if( is_dir )
        *is_dir = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    if( fsize )
        *fsize = wfd.nFileSizeLow;
    if( wtime )
        *wtime = FileTimeToUInt64( wfd.ftLastWriteTime );
    if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && ( !wcscmp( wfd.cFileName, L"." ) || !wcscmp( wfd.cFileName, L".." ) ) )
        return FileFindNext( (HANDLE) descriptor, fname, fsize, wtime, is_dir );

    return true;
}

void FileFindClose( void* descriptor )
{
    if( descriptor )
        FindClose( (HANDLE) descriptor );
}

#else

bool FileDelete( const string& fname )
{
    return remove( fname.c_str() );
}

bool FileExist( const string& fname )
{
    return !access( fname.c_str(), 0 );
}

bool FileCopy( const string& fname, const string& copy_fname )
{
    bool  ok = false;
    FILE* from = fopen( fname.c_str(), "rb" );
    if( from )
    {
        FILE* to = fopen( copy_fname.c_str(), "wb" );
        if( to )
        {
            ok = true;
            char buf[ BUFSIZ ];
            while( !feof( from ) )
            {
                size_t rb = fread( buf, 1, BUFSIZ, from );
                size_t rw = fwrite( buf, 1, rb, to );
                if( !rb || rb != rw )
                {
                    ok = false;
                    break;
                }
            }
            fclose( to );
            if( !ok )
                FileDelete( copy_fname );
        }
        fclose( from );
    }
    return ok;
}

bool FileRename( const string& fname, const string& new_fname )
{
    return !rename( fname.c_str(), new_fname.c_str() );
}

struct FileFind
{
    DIR*   d = nullptr;
    string path;
    string ext;
};

void* FileFindFirst( const string& path, const string& extension, string* fname, uint* fsize, uint64* wtime, bool* is_dir )
{
    // Open dir
    DIR* h = opendir( path.c_str() );
    if( !h )
        return nullptr;

    // Create own descriptor
    FileFind* ff = new FileFind();
    ff->d = h;
    ff->path = path;
    if( !ff->path.empty() && ff->path.back() != '/' )
        ff->path += "/";
    if( !extension.empty() )
        ff->ext = extension;

    // Find first entire
    if( !FileFindNext( ff, fname, fsize, wtime, is_dir ) )
    {
        FileFindClose( ff );
        return nullptr;
    }
    return ff;
}

bool FileFindNext( void* descriptor, string* fname, uint* fsize, uint64* wtime, bool* is_dir )
{
    // Cast descriptor
    FileFind* ff = (FileFind*) descriptor;

    // Read entire
    struct dirent* ent = readdir( ff->d );
    if( !ent )
        return false;

    // Skip '.' and '..'
    if( Str::Compare( ent->d_name, "." ) || Str::Compare( ent->d_name, ".." ) )
        return FileFindNext( descriptor, fname, fsize, wtime, is_dir );

    // Read entire information
    bool        valid = false;
    bool        dir = false;
    uint        file_size;
    uint64      write_time;
    struct stat st;
    if( !stat( ( ff->path + ent->d_name ).c_str(), &st ) )
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
        return FileFindNext( descriptor, fname, fsize, wtime, is_dir );

    // Find by extensions
    if( !ff->ext.empty() )
    {
        // Skip dirs
        if( dir )
            return FileFindNext( descriptor, fname, fsize, wtime, is_dir );

        // Compare extension
        const char* ext = nullptr;
        for( const char* name = ent->d_name; *name; name++ )
            if( *name == '.' )
                ext = name;
        if( !ext || !*( ++ext ) || !Str::CompareCase( ext, ff->ext ) )
            return FileFindNext( descriptor, fname, fsize, wtime, is_dir );
    }

    // Fill find data
    if( fname )
        *fname = ent->d_name;
    if( is_dir )
        *is_dir = dir;
    if( fsize )
        *fsize = file_size;
    if( wtime )
        *wtime = write_time;
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
#endif

void NormalizePathSlashesInplace( string& path )
{
    std::replace( path.begin(), path.end(), '\\', '/' );
}

void ResolvePathInplace( string& path )
{
    #ifdef FO_WINDOWS
    DWORD    len = GetFullPathNameW( MBtoWC( path ).c_str(), 0, nullptr, nullptr );
    wchar_t* buf = (wchar_t*) alloca( len * sizeof( wchar_t ) + 1 );
    if( GetFullPathNameW( MBtoWC( path ).c_str(), len + 1, buf, nullptr ) == len )
    {
        path = WCtoMB( buf );
        NormalizePathSlashesInplace( path );
    }

    #else
    char* buf = realpath( path.c_str(), nullptr );
    if( buf )
    {
        path = buf;
        free( buf );
    }
    #endif
}

void MakeDirectory( const string& path )
{
    #ifdef FO_WINDOWS
    CreateDirectoryW( MBtoWC( path ).c_str(), nullptr );
    #else
    mkdir( path, 0777 );
    #endif
}

void MakeDirectoryTree( const string& path )
{
    uint   result = 0;
    string work = path;
    NormalizePathSlashesInplace( work );
    for( size_t i = 0; i < work.length(); i++ )
        if( work[ i ] == '/' )
            MakeDirectory( work.substr( 0, i ) );
}
