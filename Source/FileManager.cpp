#include "Common.h"
#include "FileManager.h"

#define OUT_BUF_START_SIZE    ( 0x100 )

const char* PathList[ PATH_LIST_COUNT ] =
{
    // Client and mapper paths
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Splash/",
    "",
    "",
    "Textures/",
    "Effects/",
    "",
    "",
    "",
    "",
    "",
    "Text/",
    "Save/",
    "Fonts/",
    "Cache/",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Save/",
    "Save/Clients/",
    "Save/Bans/",
    "Logs/",
    "Dumps/",
    "Profiler/",
    "Update/",
    "Cache/",
    "",
    "",
    "",
    "",
};

DataFileVec FileManager::dataFiles;
string      FileManager::writeDir;

FileManager::FileManager()
{
    curPos = 0;
    fileSize = 0;
    dataOutBuf = nullptr;
    fileBuf = nullptr;
    posOutBuf = 0;
    endOutBuf = 0;
    lenOutBuf = 0;
}

FileManager::~FileManager()
{
    UnloadFile();
}

void FileManager::InitDataFiles( const char* path )
{
    RUNTIME_ASSERT( path && ( !path[ 0 ] || path[ Str::Length( path ) - 1 ] == '/' || path[ Str::Length( path ) - 1 ] == '\\' ) );

    // Init internal data file
    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    if( dataFiles.empty() )
        LoadDataFile( "$Basic" );
    #endif

    // Redirect path
    void* redirection_link = FileOpen( ( string( path ) + "Redirection.link" ).c_str(), false );
    if( redirection_link )
    {
        char link[ MAX_FOPATH ];
        uint len = FileGetSize( redirection_link );
        FileRead( redirection_link, link, len );
        link[ len ] = 0;
        Str::Insert( link, path );
        FormatPath( link );
        InitDataFiles( link );
        return;
    }

    // Write path first
    if( writeDir.empty() )
        writeDir = path;

    // Process dir
    if( LoadDataFile( path ) )
    {
        StrVec files;
        dataFiles.back()->GetFileNames( "", false, "dat", files );
        dataFiles.back()->GetFileNames( "", false, "zip", files );
        dataFiles.back()->GetFileNames( "", false, "bos", files );
        dataFiles.back()->GetFileNames( "packs/", true, "dat", files );
        dataFiles.back()->GetFileNames( "packs/", true, "zip", files );
        dataFiles.back()->GetFileNames( "packs/", true, "bos", files );

        std::sort( files.begin(), files.end(), std::greater< string >() );

        for( size_t i = 0, j = files.size(); i < j; i++ )
            if( !LoadDataFile( ( string( "" ) + path + files[ i ] ).c_str() ) )
                RUNTIME_ASSERT( !"Unable to load data file." );
    }
    else
    {
        RUNTIME_ASSERT( !"Unable to load files in folder." );
    }

    // Extension of this path
    void* extension_link = FileOpen( ( string( path ) + "Extension.link" ).c_str(), false );
    if( extension_link )
    {
        char link[ MAX_FOPATH ];
        uint len = FileGetSize( extension_link );
        FileRead( extension_link, link, len );
        link[ len ] = 0;
        Str::Insert( link, path );
        FormatPath( link );
        InitDataFiles( link );
    }
}

bool FileManager::LoadDataFile( const char* path )
{
    // Find already loaded
    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
    {
        DataFile* pfile = *it;
        if( pfile->GetPackName() == path )
            return true;
    }

    // Add new
    DataFile* data_file = OpenDataFile( path );
    if( !data_file )
    {
        WriteLogF( _FUNC_, " - Load data '%s' fail.\n", path );
        return false;
    }

    dataFiles.push_back( data_file );
    return true;
}

void FileManager::ClearDataFiles()
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
    SAFEDELA( dataOutBuf );
    posOutBuf = 0;
    lenOutBuf = 0;
    endOutBuf = 0;
}

uchar* FileManager::ReleaseBuffer()
{
    uchar* tmp = fileBuf;
    fileBuf = nullptr;
    fileSize = 0;
    curPos = 0;
    return tmp;
}

bool FileManager::LoadFile( const char* path, int path_type, bool no_read_data /* = false */  )
{
    UnloadFile();

    if( !path || !path[ 0 ] )
        return false;

    if( path_type >= 0 && path_type < PATH_LIST_COUNT )
    {
        // Make data path
        char data_path[ MAX_FOPATH ];
        Str::Copy( data_path, PathList[ path_type ] );
        Str::Append( data_path, path );
        FormatPath( data_path );
        char data_path_lower[ MAX_FOPATH ];
        Str::Copy( data_path_lower, data_path );
        Str::Lower( data_path_lower );

        // Find file in every data file
        for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
        {
            DataFile* dat = *it;
            if( !no_read_data )
            {
                fileBuf = dat->OpenFile( data_path, data_path_lower, fileSize, writeTime );
                if( fileBuf )
                {
                    curPos = 0;
                    return true;
                }
            }
            else
            {
                if( dat->IsFilePresent( data_path, data_path_lower, fileSize, writeTime ) )
                    return true;
            }
        }
    }
    else if( path_type == PT_ROOT )
    {
        char folder_path[ MAX_FOPATH ] = { 0 };
        Str::Copy( folder_path, path );
        FormatPath( folder_path );

        void* file = FileOpen( folder_path, false );
        if( file )
        {
            writeTime = FileGetWriteTime( file );
            fileSize = FileGetSize( file );

            if( no_read_data )
            {
                FileClose( file );
                return true;
            }

            uchar* buf = new uchar[ fileSize + 1 ];
            if( !buf )
                return false;

            bool result = FileRead( file, buf, fileSize );
            FileClose( file );
            if( !result )
                return false;

            fileBuf = buf;
            fileBuf[ fileSize ] = 0;
            curPos = 0;
            return true;
        }
    }
    else
    {
        WriteLogF( _FUNC_, " - Invalid path %d.\n", path_type );
        return false;
    }

    return false;
}

bool FileManager::LoadStream( const uchar* stream, uint length )
{
    UnloadFile();
    if( !length )
        return false;
    fileSize = length;
    fileBuf = new uchar[ fileSize + 1 ];
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

void FileManager::GetStrNT( char* str )
{
    if( !str || curPos + 1 > fileSize )
        return;
    uint len = 1;
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

void FileManager::SwitchToRead()
{
    fileBuf = dataOutBuf;
    dataOutBuf = nullptr;
    fileSize = endOutBuf;
    curPos = 0;
    lenOutBuf = 0;
    endOutBuf = 0;
    posOutBuf = 0;
}

void FileManager::SwitchToWrite()
{
    dataOutBuf = fileBuf;
    fileBuf = nullptr;
    lenOutBuf = fileSize;
    endOutBuf = fileSize;
    posOutBuf = fileSize;
    fileSize = 0;
    curPos = 0;
}

bool FileManager::ResizeOutBuf()
{
    if( !lenOutBuf )
    {
        dataOutBuf = new uchar[ OUT_BUF_START_SIZE ];
        if( !dataOutBuf )
            return false;
        lenOutBuf = OUT_BUF_START_SIZE;
        memzero( (void*) dataOutBuf, lenOutBuf );
        return true;
    }

    uchar* old_obuf = dataOutBuf;
    dataOutBuf = new uchar[ lenOutBuf * 2 ];
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
    RUNTIME_ASSERT( dataOutBuf || !endOutBuf );

    if( !fname )
        return false;

    char fpath[ MAX_FOPATH ];
    if( path_type >= 0 && path_type < PATH_LIST_COUNT )
    {
        Str::Copy( fpath, GetWritePath( fname, path_type ) );
    }
    else if( path_type == PT_ROOT )
    {
        Str::Copy( fpath, fname );
    }
    else
    {
        WriteLogF( _FUNC_, " - Invalid path %d.\n", path_type );
        return false;
    }
    FormatPath( fpath );

    void* file = FileOpen( fpath, true );
    if( !file )
        return false;

    if( !FileWrite( file, dataOutBuf, endOutBuf ) )
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
    char    str[ MAX_FOTEXT ];

    va_list list;
    va_start( list, fmt );
    vsprintf( str, fmt, list );
    va_end( list );

    SetData( str, Str::Length( str ) );
}

void FileManager::SetStrNT( const char* fmt, ... )
{
    char    str[ MAX_FOTEXT ];

    va_list list;
    va_start( list, fmt );
    vsprintf( str, fmt, list );
    va_end( list );

    SetData( str, Str::Length( str ) + 1 );
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

void FileManager::ResetCurrentDir()
{
    static char work_dir[ MAX_FOPATH ] = { 0 };
    if( work_dir[ 0 ] == 0 )
    {
        const char* appendix = "";
        #if defined ( FONLINE_SERVER )
        appendix = MainConfig->GetStr( "", "ServerPath" );
        #elif defined ( FONLINE_CLIENT )
        appendix = MainConfig->GetStr( "", "ClientPath" );
        #elif defined ( FONLINE_MAPPER )
        appendix = MainConfig->GetStr( "", "ClientPath" );
        #endif

        if( !appendix )
        {
            #if defined ( FO_WINDOWS )
            char path[ MAX_FOPATH ];
            GetModuleFileName( GetModuleHandle( nullptr ), path, sizeof( path ) );
            char dir[ MAX_FOPATH ];
            FileManager::ExtractDir( path, dir );
            SetCurrentDirectory( dir );
            #elif defined ( FO_LINUX )
            // Read symlink to executable
            char buf[ MAX_FOPATH ];
            if( readlink( "/proc/self/exe", buf, MAX_FOPATH ) != -1 ||    // Linux
                readlink( "/proc/curproc/file", buf, MAX_FOPATH ) != -1 ) // FreeBSD
            {
                string            sbuf = buf;
                string::size_type pos = sbuf.find_last_of( '/' );
                if( pos != string::npos )
                {
                    buf[ pos ] = 0;
                    chdir( buf );
                }
            }
            #elif defined ( FO_OSX_MAC )
            chdir( "./FOnline.app/Contents/Resources/Client" );
            #elif defined ( FO_OSX_IOS )
            chdir( "./Client" );
            #endif
        }

        #ifdef FO_WINDOWS
        GetCurrentDirectory( MAX_FOPATH, work_dir );
        #else
        getcwd( work_dir, MAX_FOPATH );
        #endif

        if( appendix )
        {
            Str::Append( work_dir, appendix );
            ResolvePath( work_dir );
        }
    }

    #ifdef FO_WINDOWS
    SetCurrentDirectory( work_dir );
    #else
    chdir( work_dir );
    #endif
}

void FileManager::SetCurrentDir( const char* dir, const char* write_dir )
{
    char dir_[ MAX_FOPATH ];
    Str::Copy( dir_, dir );
    FormatPath( dir_ );
    ResolvePath( dir_ );

    #ifdef FO_WINDOWS
    SetCurrentDirectory( dir_ );
    #else
    chdir( dir_ );
    #endif

    writeDir = write_dir;
}

const char* FileManager::GetDataPath( const char* fname, int path_type )
{
    #pragma RACE_CONDITION
    static char path[ MAX_FOPATH ];
    Str::Copy( path, PathList[ path_type ] );
    Str::Append( path, fname );
    return path;
}

void FileManager::GetDataPath( const char* fname, int path_type, char* result )
{
    Str::Copy( result, MAX_FOPATH, GetDataPath( fname, path_type ) );
}

const char* FileManager::GetWritePath( const char* fname, int path_type )
{
    #pragma RACE_CONDITION
    static char path[ MAX_FOPATH ];
    Str::Copy( path, writeDir.c_str() );
    Str::Append( path, path_type >= 0 && path_type < PATH_LIST_COUNT ? PathList[ path_type ] : "" );
    Str::Append( path, fname );
    return path;
}

void FileManager::GetWritePath( const char* fname, int path_type, char* result )
{
    Str::Copy( result, MAX_FOPATH, GetWritePath( fname, path_type ) );
}

hash FileManager::GetPathHash( const char* fname, int path_type )
{
    char fpath[ MAX_FOPATH ];
    GetDataPath( fname, path_type, fpath );
    return Str::GetHash( fpath );
}

void FileManager::FormatPath( char* path )
{
    // Trim
    Str::Trim( path );

    // Change to valid slash
    NormalizePathSlashes( path );

    // Erase first './'
    while( path[ 0 ] == '.' && path[ 1 ] == '/' )
    {
        char* str = path;
        char* str_ = str + 2;
        for( ; *str_; str++, str_++ )
            *str = *str_;
        *str = 0;
    }

    // Skip first '../'
    while( path[ 0 ] == '.' && path[ 1 ] == '.' && path[ 2 ] == '/' )
        path += 3;

    // Erase './'
    char* str = Str::Substring( path, "./" );
    if( str && ( str == path || *( str - 1 ) != '.' ) )
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

    // Erase 'folder/../'
    str = Str::Substring( path, "../" );
    if( str )
    {
        // Erase interval
        char* str_ = str + 3;
        str -= 2;
        for( ; str >= path; str-- )
            if( *str == '/' )
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

    // Merge '//' to '/'
    Str::Replacement( path, '/', '/', '/' );
}

void FileManager::ExtractDir( const char* path, char* dir )
{
    char buf[ MAX_FOPATH ];
    Str::Copy( buf, path );
    FormatPath( buf );

    const char* str = Str::Substring( buf, "/" );
    if( str )
    {
        str++;
        while( true )
        {
            const char* str_ = Str::Substring( str, "/" );
            if( str_ )
                str = str_ + 1;
            else
                break;
        }
        size_t len = str - buf;
        if( len )
            memcpy( dir, buf, len );
        dir[ len ] = 0;
    }
    else
    {
        dir[ 0 ] = 0;
    }
}

void FileManager::ExtractFileName( const char* path, char* name )
{
    char buf[ MAX_FOPATH ];
    Str::Copy( buf, path );
    FormatPath( buf );

    const char* str = Str::Substring( buf, "/" );
    if( str )
    {
        str++;
        while( true )
        {
            const char* str_ = Str::Substring( str, "/" );
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
        Str::Copy( name, MAX_FOPATH, path );
    }
}

void FileManager::MakeFilePath( const char* name, const char* path, char* result )
{
    char name_[ MAX_FOPATH ];
    Str::Copy( name_, name );
    FormatPath( name_ );

    char path_[ MAX_FOPATH ];
    if( path )
    {
        Str::Copy( path_, path );
        FormatPath( path_ );
    }

    if( Str::Substring( name_, "/" ) && name_[ 0 ] != '.' )
    {
        // Direct
        Str::Copy( result, MAX_FOPATH, name_ );
    }
    else
    {
        // Relative
        if( path )
            Str::Copy( result, MAX_FOPATH, path_ );
        Str::Append( result, MAX_FOPATH, name_ );
        FormatPath( result );
    }
}

const char* FileManager::GetExtension( const char* path )
{
    if( !path )
        return nullptr;
    const char* last_dot = nullptr;
    for( ; *path; path++ )
        if( *path == '.' )
            last_dot = path;
    if( !last_dot )
        return nullptr;
    last_dot++;
    if( !*last_dot )
        return nullptr;
    return last_dot;
}

char* FileManager::EraseExtension( char* path )
{
    if( !path )
        return nullptr;
    char* ext = (char*) GetExtension( path );
    if( ext )
        *( ext - 1 ) = 0;
    return path;
}

string FileManager::CombinePath( const char* path, const char* relative_dir )
{
    char fname[ MAX_FOTEXT ];
    ExtractFileName( path, fname );
    char new_path[ MAX_FOTEXT ];
    ExtractDir( path, new_path );
    Str::Append( new_path, relative_dir );
    FormatPath( new_path );
    Str::Append( new_path, fname );
    return new_path;
}

bool FileManager::CopyFile( const char* from, const char* to )
{
    void* f_from = FileOpen( from, false );
    if( !f_from )
        return false;

    void* f_to = FileOpen( to, true );
    if( !f_to )
    {
        FileClose( f_from );
        return false;
    }

    uint  remaining_size = FileGetSize( f_from );
    uchar chunk[ 4096 ];
    bool  fail = false;
    while( !fail && remaining_size > 0 )
    {
        uint read_size = MIN( remaining_size, sizeof( chunk ) );
        fail = !( FileRead( f_from, chunk, read_size ) && FileWrite( f_to, chunk, read_size ) );
        remaining_size -= read_size;
    }

    FileClose( f_to );
    FileClose( f_from );

    if( fail )
        FileDelete( to );
    return !fail;
}

bool FileManager::DeleteFile( const char* fname )
{
    return FileDelete( fname );
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

void FileManager::RecursiveDirLook( const char* base_dir, const char* cur_dir, bool include_subdirs, const char* ext, StrVec& files_path, FindDataVec* files, StrVec* dirs_path, FindDataVec* dirs )
{
    char path[ MAX_FOPATH ];
    Str::Copy( path, base_dir );
    Str::Append( path, cur_dir );

    FindData fd;
    void*    h = FileFindFirst( path, nullptr, fd );
    while( h )
    {
        if( fd.IsDirectory )
        {
            if( fd.FileName[ 0 ] != '_' )
            {
                Str::Copy( path, cur_dir );
                Str::Append( path, fd.FileName );
                Str::Append( path, "/" );

                if( dirs )
                    dirs->push_back( fd );
                if( dirs_path )
                    dirs_path->push_back( path );

                if( include_subdirs )
                    RecursiveDirLook( base_dir, path, include_subdirs, ext, files_path, files, dirs_path, dirs );
            }
        }
        else
        {
            if( ext )
            {
                const char* ext_ = GetExtension( fd.FileName );
                if( ext_ && Str::CompareCase( ext, ext_ ) )
                {
                    Str::Copy( path, cur_dir );
                    Str::Append( path, fd.FileName );
                    files_path.push_back( path );
                    if( files )
                        files->push_back( fd );
                }
            }
            else
            {
                Str::Copy( path, cur_dir );
                Str::Append( path, fd.FileName );
                files_path.push_back( path );
                if( files )
                    files->push_back( fd );
            }
        }

        if( !FileFindNext( h, fd ) )
            break;
    }
    if( h )
        FileFindClose( h );
}

void FileManager::GetFolderFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& files_path, FindDataVec* files /* = NULL */, StrVec* dirs_path /* = NULL */, FindDataVec* dirs /* = NULL */ )
{
    // Format path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    FormatPath( path_ );

    // Find in folder files
    RecursiveDirLook( path_, "", include_subdirs, ext, files_path, files, dirs_path, dirs );
}

void FileManager::GetDataFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
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

FilesCollection::FilesCollection( const char* ext, const char* fixed_dir /* = NULL */ )
{
    curFileIndex = 0;

    StrVec find_dirs;
    if( !fixed_dir )
        find_dirs = ModuleFullDirs;
    else
        find_dirs.push_back( fixed_dir );

    for( size_t d = 0; d < find_dirs.size(); d++ )
    {
        StrVec paths;
        FileManager::GetFolderFileNames( find_dirs[ d ].c_str(), true, ext, paths );
        for( size_t i = 0; i < paths.size(); i++ )
        {
            char   fname[ MAX_FOPATH ];
            FileManager::ExtractFileName( paths[ i ].c_str(), fname );
            string path = find_dirs[ d ] + paths[ i ];
            string relative_path = paths[ i ];

            // Link to another file
            const char* link_ext = FileManager::GetExtension( path.c_str() );
            if( link_ext && Str::CompareCase( link_ext, "link" ) )
            {
                FileManager link;
                if( !link.LoadFile( path.c_str(), PT_ROOT ) )
                {
                    WriteLogF( _FUNC_, " - Can't read link file '%s'.\n", path.c_str() );
                    continue;
                }

                const char* link_str = (const char*) link.GetBuf();
                path = FileManager::CombinePath( path.c_str(), link_str );
                relative_path = FileManager::CombinePath( relative_path.c_str(), link_str );
            }

            FileManager::EraseExtension( fname );
            fileNames.push_back( fname );
            filePaths.push_back( path );
            fileRelativePaths.push_back( relative_path );
        }
    }
}

bool FilesCollection::IsNextFile()
{
    return curFileIndex < fileNames.size();
}

FileManager& FilesCollection::GetNextFile( const char** name /* = NULL */, const char** path /* = NULL */, const char** relative_path /* = NULL */, bool no_read_data /* = false */ )
{
    curFileIndex++;
    curFile.LoadFile( filePaths[ curFileIndex - 1 ].c_str(), PT_ROOT, no_read_data );
    RUNTIME_ASSERT( curFile.IsLoaded() );
    if( name )
        *name = fileNames[ curFileIndex - 1 ].c_str();
    if( path )
        *path = filePaths[ curFileIndex - 1 ].c_str();
    if( relative_path )
        *relative_path = fileRelativePaths[ curFileIndex - 1 ].c_str();
    return curFile;
}

FileManager& FilesCollection::FindFile( const char* name, const char** path /* = NULL */, const char** relative_path /* = NULL */, bool no_read_data /* = false */ )
{
    curFile.UnloadFile();
    for( size_t i = 0; i < fileNames.size(); i++ )
    {
        if( fileNames[ i ] == name )
        {
            curFile.LoadFile( filePaths[ i ].c_str(), PT_ROOT, no_read_data );
            RUNTIME_ASSERT( curFile.IsLoaded() );
            if( path )
                *path = filePaths[ i ].c_str();
            if( relative_path )
                *relative_path = fileRelativePaths[ curFileIndex - 1 ].c_str();
            break;
        }
    }
    return curFile;
}

uint FilesCollection::GetFilesCount()
{
    return (uint) fileNames.size();
}

void FilesCollection::ResetCounter()
{
    curFileIndex = 0;
}
