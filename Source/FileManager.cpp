#include "StdAfx.h"
#include "FileManager.h"

#define OUT_BUF_START_SIZE    ( 0x100 )

const char* PathLst[ PATH_LIST_COUNT ] =
{
    // Client and mapper paths
    "",
    "",
    "art\\",
    "art\\critters\\",
    "art\\intrface\\",
    "art\\inven\\",
    "art\\items\\",
    "art\\misc\\",
    "art\\scenery\\",
    "art\\skilldex\\",
    "art\\splash\\",
    "art\\tiles\\",
    "art\\walls\\",
    "textures\\",
    "effects\\",
    "",
    "sound\\music\\",
    "sound\\sfx\\",
    "scripts\\",
    "video\\",
    "text\\",
    "save\\",
    "fonts\\",
    "cache\\",
    "",
    "",
    "",
    "",
    "",
    "",

    // Server paths
    "",
    "data\\",
    "text\\",
    "dialogs\\",
    "maps\\",
    "proto\\items\\",
    "proto\\critters\\",
    "scripts\\",
    "save\\",
    "save\\clients\\",
    "save\\bans\\",
    "",
    "",
    "",
    "",

    // Other
    "data\\",     // Mapper data
    "",
    "",
    "",
    "",
};

DataFileVec FileManager::dataFiles;
char        FileManager::dataPath[ MAX_FOPATH ] = { ".\\" };

void FileManager::SetDataPath( const char* path )
{
    Str::Copy( dataPath, path );
    if( dataPath[ Str::Length( path ) - 1 ] != '\\' )
        Str::Append( dataPath, "\\" );
    CreateDirectory( GetFullPath( "", PT_DATA ), NULL );
}

void FileManager::SetCacheName( const char* name )
{
    char cache_path[ MAX_FOPATH ];
    Str::Format( cache_path, "cache\\%s\\", name && name[ 0 ] ? name : "dummy" );
    PathLst[ PT_CACHE ] = Str::Duplicate( cache_path );
    CreateDirectory( GetFullPath( "", PT_CACHE ), NULL );
}

void FileManager::InitDataFiles( const char* path )
{
    char list_path[ MAX_FOPATH ];
    Str::Format( list_path, "%sDataFiles.cfg", path );

    FileManager list;
    if( list.LoadFile( list_path, -1 ) )
    {
        char line[ MAX_FOTEXT ];
        while( list.GetLine( line, 1024 ) )
        {
            // Cut off comments
            char* comment1 = strstr( line, "#" );
            char* comment2 = strstr( line, ";" );
            if( comment1 )
                *comment1 = 0;
            if( comment2 )
                *comment2 = 0;

            // Cut off specific characters
            Str::EraseFrontBackSpecificChars( line );

            // Make file path
            char fpath[ MAX_FOPATH ] = { 0 };
            if( line[ 1 ] != ':' )
                Str::Copy( fpath, path );                       // Relative path
            Str::Append( fpath, line );
            FormatPath( fpath );

            // Try load
            if( !LoadDataFile( fpath ) )
            {
                char errmsg[ MAX_FOTEXT ];
                Str::Format( errmsg, "Data file '%s' not found. Run Updater.exe.", fpath );
                HWND wnd = GetActiveWindow();
                if( wnd )
                    MessageBox( wnd, errmsg, "FOnline", MB_OK );
                WriteLogF( _FUNC_, " - %s\n", errmsg );
            }
        }
    }
}

bool FileManager::LoadDataFile( const char* path )
{
    if( !path )
    {
        WriteLogF( _FUNC_, " - Path empty or nullptr.\n" );
        return false;
    }

    // Extract full path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    if( !GetFullPathName( path, MAX_FOPATH, path_, NULL ) )
    {
        WriteLogF( _FUNC_, " - Extract full path file<%s> fail.\n", path );
        return false;
    }

    // Find already loaded
    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
    {
        DataFile* pfile = *it;
        if( pfile->GetPackName() == path_ )
            return true;
    }

    // Add new
    DataFile* pfile = OpenDataFile( path_ );
    if( !pfile )
    {
        WriteLogF( _FUNC_, " - Load packed file<%s> fail.\n", path_ );
        return false;
    }

    dataFiles.insert( dataFiles.begin(), pfile );
    return true;
}

void FileManager::EndOfWork()
{
    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
        delete *it;
    dataFiles.clear();
}

void FileManager::UnloadFile()
{
    SAFEDELA( fileBuf );
    fileSize = 0;
    curPos = 0;
}

uchar* FileManager::ReleaseBuffer()
{
    uchar* tmp = fileBuf;
    fileBuf = NULL;
    fileSize = 0;
    curPos = 0;
    return tmp;
}

bool FileManager::LoadFile( const char* fname, int path_type )
{
    UnloadFile();

    if( !fname || !fname[ 0 ] )
        return false;

    char folder_path[ MAX_FOPATH ] = { 0 };
    char dat_path[ MAX_FOPATH ] = { 0 };
    bool only_folder = false;

    if( path_type >= 0 && path_type < PATH_LIST_COUNT )
    {
        // Make data path
        Str::Copy( dat_path, PathLst[ path_type ] );
        Str::Append( dat_path, fname );
        FormatPath( dat_path );

        // Make folder path
        Str::Copy( folder_path, GetDataPath( path_type ) );
        FormatPath( folder_path );
        Str::Append( folder_path, dat_path );

        // Check for full path
        if( dat_path[ 1 ] == ':' )
            only_folder = true;                        // C:/folder/file.ext
        if( dat_path[ 0 ] == '.' && dat_path[ 1 ] == '.' && dat_path[ 2 ] == '\\' )
            only_folder = true;                        // ../folder/file.ext

        // Check for empty
        if( !dat_path[ 0 ] || !folder_path[ 0 ] )
            return false;
    }
    else if( path_type == -1 )
    {
        Str::Copy( folder_path, fname );
        only_folder = true;
    }
    else
    {
        WriteLogF( _FUNC_, " - Invalid path<%d>.\n", path_type );
        return false;
    }

    void* file = FileOpen( folder_path, false );
    if( file )
    {
        union
        {
            FILETIME       ft;
            ULARGE_INTEGER ul;
        } tc, ta, tw;
        GetFileTime( (HANDLE) file, &tc.ft, &ta.ft, &tw.ft );
        timeCreate = PACKUINT64( tc.ul.HighPart, tc.ul.LowPart );
        timeAccess = PACKUINT64( ta.ul.HighPart, ta.ul.LowPart );
        timeWrite = PACKUINT64( tw.ul.HighPart, tw.ul.LowPart );

        uint   size = GetFileSize( (HANDLE) file, NULL );
        uchar* buf = new (nothrow) uchar[ size + 1 ];
        if( !buf )
            return false;

        bool result = FileRead( file, buf, size );
        FileClose( file );
        if( !result )
            return false;

        fileSize = size;
        fileBuf = buf;
        fileBuf[ fileSize ] = 0;
        curPos = 0;
        return true;
    }

    if( only_folder )
        return false;

    _strlwr( dat_path );
    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
    {
        DataFile* dat = *it;
        fileBuf = dat->OpenFile( dat_path, fileSize );
        if( fileBuf )
        {
            curPos = 0;
            dat->GetTime( &timeCreate, &timeAccess, &timeWrite );
            return true;
        }
    }

    return false;
}

bool FileManager::LoadStream( const uchar* stream, uint length )
{
    UnloadFile();
    if( !length )
        return false;
    fileSize = length;
    fileBuf = new (nothrow) uchar[ fileSize + 1 ];
    if( !fileBuf )
        return false;
    memcpy( fileBuf, stream, fileSize );
    fileBuf[ fileSize ] = 0;
    curPos = 0;
    return true;
}

void FileManager::SetCurPos( uint pos )
{
    curPos = pos;
}

void FileManager::GoForward( uint offs )
{
    curPos += offs;
}

void FileManager::GoBack( uint offs )
{
    curPos -= offs;
}

bool FileManager::FindFragment( const uchar* fragment, uint fragment_len, uint begin_offs )
{
    if( !fileBuf || fragment_len > fileSize )
        return false;

    for( uint i = begin_offs; i < fileSize - fragment_len; i++ )
    {
        if( fileBuf[ i ] == fragment[ 0 ] )
        {
            bool not_match = false;
            for( uint j = 1; j < fragment_len; j++ )
            {
                if( fileBuf[ i + j ] != fragment[ j ] )
                {
                    not_match = true;
                    break;
                }
            }

            if( !not_match )
            {
                curPos = i;
                return true;
            }
        }
    }
    return false;
}

bool FileManager::GetLine( char* str, uint len )
{
    if( curPos >= fileSize )
        return false;

    uint wpos = 0;
    for( ; wpos < len && curPos < fileSize; curPos++, wpos++ )
    {
        if( fileBuf[ curPos ] == '\r' || fileBuf[ curPos ] == '\n' || fileBuf[ curPos ] == '#' || fileBuf[ curPos ] == ';' )
        {
            for( ; curPos < fileSize; curPos++ )
                if( fileBuf[ curPos ] == '\n' )
                    break;
            curPos++;
            break;
        }

        str[ wpos ] = fileBuf[ curPos ];
    }

    str[ wpos ] = 0;
    if( !str[ 0 ] )
        return GetLine( str, len );          // Skip empty line
    return true;
}

bool FileManager::CopyMem( void* ptr, uint size )
{
    if( !size )
        return false;
    if( curPos + size > fileSize )
        return false;
    memcpy( ptr, fileBuf + curPos, size );
    curPos += size;
    return true;
}

void FileManager::GetStr( char* str )
{
    if( !str || curPos + 1 > fileSize )
        return;
    uint len = 1;   // Zero terminated
    while( *( fileBuf + curPos + len - 1 ) )
        len++;
    memcpy( str, fileBuf + curPos, len );
    curPos += len;
}

uchar FileManager::GetUChar()
{
    if( curPos + sizeof( uchar ) > fileSize )
        return 0;
    uchar res = 0;
    res = fileBuf[ curPos++ ];
    return res;
}

ushort FileManager::GetBEUShort()
{
    if( curPos + sizeof( ushort ) > fileSize )
        return 0;
    ushort res = 0;
    uchar* cres = (uchar*) &res;
    cres[ 1 ] = fileBuf[ curPos++ ];
    cres[ 0 ] = fileBuf[ curPos++ ];
    return res;
}

ushort FileManager::GetLEUShort()
{
    if( curPos + sizeof( ushort ) > fileSize )
        return 0;
    ushort res = 0;
    uchar* cres = (uchar*) &res;
    cres[ 0 ] = fileBuf[ curPos++ ];
    cres[ 1 ] = fileBuf[ curPos++ ];
    return res;
}

uint FileManager::GetBEUInt()
{
    if( curPos + sizeof( uint ) > fileSize )
        return 0;
    uint   res = 0;
    uchar* cres = (uchar*) &res;
    for( int i = 3; i >= 0; i-- )
        cres[ i ] = fileBuf[ curPos++ ];
    return res;
}

uint FileManager::GetLEUInt()
{
    if( curPos + sizeof( uint ) > fileSize )
        return 0;
    uint   res = 0;
    uchar* cres = (uchar*) &res;
    for( int i = 0; i <= 3; i++ )
        cres[ i ] = fileBuf[ curPos++ ];
    return res;
}

uint FileManager::GetLE3UChar()
{
    if( curPos + sizeof( uchar ) * 3 > fileSize )
        return 0;
    uint   res = 0;
    uchar* cres = (uchar*) &res;
    for( int i = 0; i <= 2; i++ )
        cres[ i ] = fileBuf[ curPos++ ];
    return res;
}

float FileManager::GetBEFloat()
{
    if( curPos + sizeof( float ) > fileSize )
        return 0.0f;
    float  res;
    uchar* cres = (uchar*) &res;
    for( int i = 3; i >= 0; i-- )
        cres[ i ] = fileBuf[ curPos++ ];
    return res;
}

float FileManager::GetLEFloat()
{
    if( curPos + sizeof( float ) > fileSize )
        return 0.0f;
    float  res;
    uchar* cres = (uchar*) &res;
    for( int i = 0; i <= 3; i++ )
        cres[ i ] = fileBuf[ curPos++ ];
    return res;
}

int FileManager::GetNum()
{
    // Todo: rework
    while( curPos < fileSize )
    {
        if( fileBuf[ curPos ] >= '0' && fileBuf[ curPos ] <= '9' )
            break;
        else
            curPos++;
    }

    if( curPos >= fileSize )
        return 0;
    int res = atoi( (const char*) &fileBuf[ curPos ] );
    if( res / 1000000000 )
        curPos += 10;
    else if( res / 100000000 )
        curPos += 9;
    else if( res / 10000000 )
        curPos += 8;
    else if( res / 1000000 )
        curPos += 7;
    else if( res / 100000 )
        curPos += 6;
    else if( res / 10000 )
        curPos += 5;
    else if( res / 1000 )
        curPos += 4;
    else if( res / 100 )
        curPos += 3;
    else if( res / 10 )
        curPos += 2;
    else
        curPos++;
    return res;
}

void FileManager::SwitchToRead()
{
    fileBuf = dataOutBuf;
    dataOutBuf = NULL;
    fileSize = endOutBuf;
    curPos = 0;
    lenOutBuf = 0;
    endOutBuf = 0;
    posOutBuf = 0;
}

void FileManager::SwitchToWrite()
{
    dataOutBuf = fileBuf;
    fileBuf = NULL;
    lenOutBuf = fileSize;
    endOutBuf = fileSize;
    posOutBuf = fileSize;
    fileSize = 0;
    curPos = 0;
}

void FileManager::ClearOutBuf()
{
    SAFEDELA( dataOutBuf );
    posOutBuf = 0;
    lenOutBuf = 0;
    endOutBuf = 0;
}

bool FileManager::ResizeOutBuf()
{
    if( !lenOutBuf )
    {
        dataOutBuf = new (nothrow) uchar[ OUT_BUF_START_SIZE ];
        if( !dataOutBuf )
            return false;
        lenOutBuf = OUT_BUF_START_SIZE;
        memzero( (void*) dataOutBuf, lenOutBuf );
        return true;
    }

    uchar* old_obuf = dataOutBuf;
    dataOutBuf = new (nothrow) uchar[ lenOutBuf * 2 ];
    if( !dataOutBuf )
        return false;
    memzero( (void*) dataOutBuf, lenOutBuf * 2 );
    memcpy( (void*) dataOutBuf, (void*) old_obuf, lenOutBuf );
    SAFEDELA( old_obuf );
    lenOutBuf *= 2;
    return true;
}

void FileManager::SetPosOutBuf( uint pos )
{
    if( pos > lenOutBuf )
    {
        if( ResizeOutBuf() )
            SetPosOutBuf( pos );
        return;
    }
    posOutBuf = pos;
}

bool FileManager::SaveOutBufToFile( const char* fname, int path_type )
{
    if( !fname )
        return false;

    char fpath[ MAX_FOPATH ];
    if( path_type >= 0 && path_type < PATH_LIST_COUNT )
    {
        Str::Copy( fpath, GetDataPath( path_type ) );
        Str::Append( fpath, PathLst[ path_type ] );
        Str::Append( fpath, fname );
    }
    else if( path_type == -1 )
    {
        Str::Copy( fpath, fname );
    }
    else
    {
        WriteLogF( _FUNC_, " - Invalid path<%d>.\n", path_type );
        return false;
    }

    void* file = FileOpen( fpath, true );
    if( !file )
        return false;

    if( dataOutBuf && !FileWrite( file, dataOutBuf, endOutBuf ) )
    {
        FileClose( file );
        return false;
    }

    FileClose( file );
    return true;
}

void FileManager::SetData( void* data, uint len )
{
    if( !len )
        return;
    if( posOutBuf + len > lenOutBuf )
    {
        if( ResizeOutBuf() )
            SetData( data, len );
        return;
    }

    memcpy( &dataOutBuf[ posOutBuf ], data, len );
    posOutBuf += len;
    if( posOutBuf > endOutBuf )
        endOutBuf = posOutBuf;
}

void FileManager::SetStr( const char* fmt, ... )
{
    char    str[ 2048 ];

    va_list list;
    va_start( list, fmt );
    #ifdef FO_MSVC
    vsprintf_s( str, fmt, list );
    #else
    vsprintf( str, fmt, list );
    #endif
    va_end( list );

    SetData( str, Str::Length( str ) );
}

void FileManager::SetUChar( uchar data )
{
    if( posOutBuf + sizeof( uchar ) > lenOutBuf && !ResizeOutBuf() )
        return;
    dataOutBuf[ posOutBuf++ ] = data;
    if( posOutBuf > endOutBuf )
        endOutBuf = posOutBuf;
}

void FileManager::SetBEUShort( ushort data )
{
    if( posOutBuf + sizeof( ushort ) > lenOutBuf && !ResizeOutBuf() )
        return;
    uchar* pdata = (uchar*) &data;
    dataOutBuf[ posOutBuf++ ] = pdata[ 1 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 0 ];
    if( posOutBuf > endOutBuf )
        endOutBuf = posOutBuf;
}

void FileManager::SetLEUShort( ushort data )
{
    if( posOutBuf + sizeof( ushort ) > lenOutBuf && !ResizeOutBuf() )
        return;
    uchar* pdata = (uchar*) &data;
    dataOutBuf[ posOutBuf++ ] = pdata[ 0 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 1 ];
    if( posOutBuf > endOutBuf )
        endOutBuf = posOutBuf;
}

void FileManager::SetBEUInt( uint data )
{
    if( posOutBuf + sizeof( uint ) > lenOutBuf && !ResizeOutBuf() )
        return;
    uchar* pdata = (uchar*) &data;
    dataOutBuf[ posOutBuf++ ] = pdata[ 3 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 2 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 1 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 0 ];
    if( posOutBuf > endOutBuf )
        endOutBuf = posOutBuf;
}

void FileManager::SetLEUInt( uint data )
{
    if( posOutBuf + sizeof( uint ) > lenOutBuf && !ResizeOutBuf() )
        return;
    uchar* pdata = (uchar*) &data;
    dataOutBuf[ posOutBuf++ ] = pdata[ 0 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 1 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 2 ];
    dataOutBuf[ posOutBuf++ ] = pdata[ 3 ];
    if( posOutBuf > endOutBuf )
        endOutBuf = posOutBuf;
}

const char* FileManager::GetFullPath( const char* fname, int path_type )
{
    static THREAD char buf[ MAX_FOPATH ];
    buf[ 0 ] = 0;
    if( path_type >= 0 )
        Str::Append( buf, GetDataPath( path_type ) );
    if( path_type >= 0 )
        Str::Append( buf, PathLst[ path_type ] );
    if( fname )
        Str::Append( buf, fname );
    FormatPath( buf );
    return buf;
}

void FileManager::GetFullPath( const char* fname, int path_type, char* get_path )
{
    if( !get_path )
        return;
    get_path[ 0 ] = 0;
    if( path_type >= 0 )
        Str::Append( get_path, MAX_FOPATH, GetDataPath( path_type ) );
    if( path_type >= 0 )
        Str::Append( get_path, MAX_FOPATH, PathLst[ path_type ] );
    if( fname )
        Str::Append( get_path, MAX_FOPATH, fname );
    FormatPath( get_path );
}

const char* FileManager::GetPath( int path_type )
{
    static const char any[] = "error";
    return (uint) path_type >= PATH_LIST_COUNT ? any : PathLst[ path_type ];
}

const char* FileManager::GetDataPath( int path_type )
{
    static const char root_path[] = ".\\";

    #if defined ( FONLINE_SERVER )
    if( path_type == PT_SERVER_ROOT )
        return root_path;
    #elif defined ( FONLINE_CLIENT )
    if( path_type == PT_ROOT )
        return root_path;
    #elif defined ( FONLINE_MAPPER )
    if( path_type == PT_MAPPER_DATA )
        return root_path;
    #endif

    return dataPath;
}

void FileManager::FormatPath( char* path )
{
    // Change '/' to '\'
    for( char* str = path; *str; str++ )
        if( *str == '/' )
            *str = '\\';

    // Skip first '..\'
    while( path[ 0 ] == '.' && path[ 1 ] == '.' && path[ 2 ] == '\\' )
        path += 3;

    // Erase 'folder\..\'
    char* str = strstr( path, "..\\" );
    if( str )
    {
        // Erase interval
        char* str_ = str + 3;
        str -= 2;
        for( ; str >= path; str-- )
            if( *str == '\\' )
                break;
        if( str < path )
            str = path;
        else
            str++;
        for( ; *str_; str++, str_++ )
            *str = *str_;
        *str = 0;

        // Recursive look
        FormatPath( path );
        return;
    }

    // Erase '.\'
    str = strstr( path, ".\\" );
    if( str )
    {
        // Erase interval
        char* str_ = str + 2;
        for( ; *str_; str++, str_++ )
            *str = *str_;
        *str = 0;

        // Recursive look
        FormatPath( path );
        return;
    }
}

void FileManager::ExtractPath( const char* fname, char* path )
{
    bool dup = false;
    if( fname == path )
    {
        fname = Str::Duplicate( fname );
        dup = true;
    }

    const char* str = strstr( fname, "\\" );
    if( str )
    {
        str++;
        while( true )
        {
            const char* str_ = strstr( str, "\\" );
            if( str_ )
                str = str_ + 1;
            else
                break;
        }
        size_t len = str - fname;
        if( len )
            memcpy( path, fname, len );
        path[ len ] = 0;
    }
    else
    {
        path[ 0 ] = 0;
    }

    if( dup )
        delete[] fname;
}

void FileManager::ExtractFileName( const char* fname, char* name )
{
    bool dup = false;
    if( fname == name )
    {
        fname = Str::Duplicate( fname );
        dup = true;
    }

    const char* str = strstr( fname, "\\" );
    if( str )
    {
        str++;
        while( true )
        {
            const char* str_ = strstr( str, "\\" );
            if( str_ )
                str = str_ + 1;
            else
                break;
        }
        int len = 0;
        for( ; *str; len++, str++ )
            name[ len ] = *str;
        name[ len ] = 0;
    }
    else
    {
        name[ 0 ] = 0;
    }

    if( dup )
        delete[] fname;
}

void FileManager::MakeFilePath( const char* name, const char* path, char* result )
{
    if( ( strstr( name, "\\" ) || strstr( name, "/" ) ) && name[ 0 ] != '.' )
    {
        // Direct
        Str::Copy( result, MAX_FOPATH, name );
    }
    else
    {
        // Relative
        if( path )
            Str::Copy( result, MAX_FOPATH, path );
        Str::Append( result, MAX_FOPATH, name );
        FormatPath( result );
    }
}

const char* FileManager::GetExtension( const char* fname )
{
    if( !fname )
        return NULL;
    const char* last_dot = NULL;
    for( ; *fname; fname++ )
        if( *fname == '.' )
            last_dot = fname;
    if( !last_dot )
        return NULL;
    last_dot++;
    if( !*last_dot )
        return NULL;
    return last_dot;
}

char* FileManager::EraseExtension( char* fname )
{
    if( !fname )
        return NULL;
    char* ext = (char*) GetExtension( fname );
    if( ext )
        *( ext - 1 ) = 0;
    return fname;
}

int FileManager::ParseLinesInt( const char* fname, int path_type, IntVec& lines )
{
    lines.clear();
    if( !LoadFile( fname, path_type ) )
        return -1;

    char cur_line[ 129 ];
    while( GetLine( cur_line, 128 ) )
        lines.push_back( atoi( cur_line ) );
    UnloadFile();
    return (int) lines.size();
}

void FileManager::GetTime( uint64* create, uint64* access, uint64* write )
{
    if( create )
        *create = timeCreate;
    if( access )
        *access = timeAccess;
    if( write )
        *write = timeWrite;
}

void FileManager::RecursiveDirLook( const char* init_dir, bool include_subdirs, const char* ext, StrVec& result )
{
    WIN32_FIND_DATA fd;
    char            buf[ MAX_FOPATH ];
    Str::Format( buf, "%s%s*", dataPath, init_dir );
    HANDLE          h = FindFirstFile( buf, &fd );
    while( h != INVALID_HANDLE_VALUE )
    {
        if( fd.cFileName[ 0 ] != '.' )
        {
            if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                if( include_subdirs )
                {
                    Str::Format( buf, "%s%s\\", init_dir, fd.cFileName );
                    RecursiveDirLook( buf, include_subdirs, ext, result );
                }
            }
            else
            {
                if( ext )
                {
                    const char* ext_ = GetExtension( fd.cFileName );
                    if( ext_ && Str::CompareCase( ext, ext_ ) )
                    {
                        Str::Copy( buf, init_dir );
                        Str::Append( buf, fd.cFileName );
                        result.push_back( buf );
                    }
                }
                else
                {
                    Str::Copy( buf, init_dir );
                    Str::Append( buf, fd.cFileName );
                    result.push_back( buf );
                }
            }
        }

        if( !FindNextFile( h, &fd ) )
            break;
    }
    if( h != INVALID_HANDLE_VALUE )
        FindClose( h );
}

void FileManager::GetFolderFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    // Format path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    FormatPath( path_ );

    // Find in folder files
    RecursiveDirLook( path_, include_subdirs, ext, result );
}

void FileManager::GetDatsFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    // Format path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    FormatPath( path_ );

    // Find in dat files
    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
    {
        DataFile* dat = *it;
        dat->GetFileNames( path_, include_subdirs, ext, result );
    }
}
