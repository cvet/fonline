#include "Common.h"
#include "FileManager.h"

#define OUT_BUF_START_SIZE    ( 0x100 )

const char* PathList[ PATH_LIST_COUNT ] =
{
    // Client and mapper paths
    "",
    "",
    "art" DIR_SLASH_S,
    "art" DIR_SLASH_S "critters" DIR_SLASH_S,
    "art" DIR_SLASH_S "intrface" DIR_SLASH_S,
    "art" DIR_SLASH_S "inven" DIR_SLASH_S,
    "art" DIR_SLASH_S "items" DIR_SLASH_S,
    "art" DIR_SLASH_S "misc" DIR_SLASH_S,
    "art" DIR_SLASH_S "scenery" DIR_SLASH_S,
    "art" DIR_SLASH_S "skilldex" DIR_SLASH_S,
    "art" DIR_SLASH_S "splash" DIR_SLASH_S,
    "art" DIR_SLASH_S "tiles" DIR_SLASH_S,
    "art" DIR_SLASH_S "walls" DIR_SLASH_S,
    "textures" DIR_SLASH_S,
    "effects" DIR_SLASH_S,
    "",
    "sound" DIR_SLASH_S "music" DIR_SLASH_S,
    "sound" DIR_SLASH_S "sfx" DIR_SLASH_S,
    "scripts" DIR_SLASH_S,
    "video" DIR_SLASH_S,
    "text" DIR_SLASH_S,
    "save" DIR_SLASH_S,
    "fonts" DIR_SLASH_S,
    "cache" DIR_SLASH_S,
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "data" DIR_SLASH_S,
    "text" DIR_SLASH_S,
    "dialogs" DIR_SLASH_S,
    "maps" DIR_SLASH_S,
    "proto" DIR_SLASH_S "items" DIR_SLASH_S,
    "proto" DIR_SLASH_S "critters" DIR_SLASH_S,
    "scripts" DIR_SLASH_S,
    "save" DIR_SLASH_S,
    "save" DIR_SLASH_S "clients" DIR_SLASH_S,
    "save" DIR_SLASH_S "bans" DIR_SLASH_S,
    "logs" DIR_SLASH_S,
    "dumps" DIR_SLASH_S,
    "profiler" DIR_SLASH_S,
    "update" DIR_SLASH_S,
    "cache" DIR_SLASH_S,
    "cache" DIR_SLASH_S "scripts" DIR_SLASH_S,
    "cache" DIR_SLASH_S "maps" DIR_SLASH_S,
    "resources" DIR_SLASH_S,
    "",
};

DataFileVec FileManager::dataFiles;
string      FileManager::basePathes[ 2 ];

FileManager::FileManager()
{
    curPos = 0;
    fileSize = 0;
    dataOutBuf = NULL;
    fileBuf = NULL;
    posOutBuf = 0;
    endOutBuf = 0;
    lenOutBuf = 0;
}

FileManager::~FileManager()
{
    UnloadFile();
    ClearOutBuf();
}

void FileManager::InitDataFiles( const char* path )
{
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
        FormatPath( link );
        InitDataFiles( link );
        return;
    }

    // Main read and write paths
    if( basePathes[ 0 ].empty() )
        basePathes[ 0 ] = basePathes[ 1 ] = path;
    else if( basePathes[ 0 ] == basePathes[ 1 ] )
        basePathes[ 1 ] = path;

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
            LoadDataFile( ( string( "" ) + path + files[ i ] ).c_str() );
    }

    // Extension of this path
    void* extension_link = FileOpen( ( string( path ) + "Extension.link" ).c_str(), false );
    if( extension_link )
    {
        char link[ MAX_FOPATH ];
        uint len = FileGetSize( extension_link );
        FileRead( extension_link, link, len );
        link[ len ] = 0;
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
        WriteLogF( _FUNC_, " - Load data<%s> fail.\n", path );
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
}

uchar* FileManager::ReleaseBuffer()
{
    uchar* tmp = fileBuf;
    fileBuf = NULL;
    fileSize = 0;
    curPos = 0;
    return tmp;
}

bool FileManager::LoadFile( const char* fname, int path_type, bool no_read_data /* = false */  )
{
    UnloadFile();

    if( !fname || !fname[ 0 ] )
        return false;

    if( path_type >= 0 && path_type < PATH_LIST_COUNT )
    {
        // Make data path
        char data_path[ MAX_FOPATH ];
        Str::Copy( data_path, PathList[ path_type ] );
        Str::Append( data_path, fname );
        FormatPath( data_path );

        // Store original data path
        char original_data_path[ MAX_FOPATH ];
        Str::Copy( original_data_path, data_path );

        // File names in data files in lower case
        Str::Lower( data_path );

        // Change slashes back to slash, because dat tree use '\'
        for( char* str = data_path; *str; str++ )
            if( *str == '/' )
                *str = '\\';

        // Find file in every data file
        for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
        {
            DataFile* dat = *it;
            if( !no_read_data )
            {
                fileBuf = dat->OpenFile( data_path, original_data_path, fileSize, writeTime );
                if( fileBuf )
                {
                    curPos = 0;
                    return true;
                }
            }
            else
            {
                if( dat->IsFilePresent( data_path, original_data_path, fileSize, writeTime ) )
                    return true;
            }
        }
    }
    else if( path_type == PT_ROOT )
    {
        char folder_path[ MAX_FOPATH ] = { 0 };
        Str::Copy( folder_path, fname );
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
        WriteLogF( _FUNC_, " - Invalid path<%d>.\n", path_type );
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
        WriteLogF( _FUNC_, " - Invalid path<%d>.\n", path_type );
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

const char* FileManager::GetReadPath( const char* fname, int path_type )
{
    #pragma RACE_CONDITION
    static char path[ MAX_FOPATH ];
    Str::Copy( path, basePathes[ 1 ].c_str() );
    Str::Append( path, path_type >= 0 && path_type < PATH_LIST_COUNT ? PathList[ path_type ] : "" );
    Str::Append( path, fname );
    return path;
}

void FileManager::GetReadPath( const char* fname, int path_type, char* result )
{
    Str::Copy( result, MAX_FOPATH, GetReadPath( fname, path_type ) );
}

const char* FileManager::GetWritePath( const char* fname, int path_type )
{
    #pragma RACE_CONDITION
    static char path[ MAX_FOPATH ];
    Str::Copy( path, basePathes[ 0 ].c_str() );
    Str::Append( path, path_type >= 0 && path_type < PATH_LIST_COUNT ? PathList[ path_type ] : "" );
    Str::Append( path, fname );
    return path;
}

void FileManager::GetWritePath( const char* fname, int path_type, char* result )
{
    Str::Copy( result, MAX_FOPATH, GetWritePath( fname, path_type ) );
}

void FileManager::SetWritePath( const char* path )
{
    basePathes[ 0 ] = path;
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
    for( char* str = path; *str; str++ )
    {
        #ifdef FO_WINDOWS
        if( *str == '/' )
            *str = '\\';
        #else
        if( *str == '\\' )
            *str = '/';
        #endif
    }

    // Erase first './'
    while( path[ 0 ] == '.' && path[ 1 ] == DIR_SLASH_C )
    {
        char* str = path;
        char* str_ = str + 2;
        for( ; *str_; str++, str_++ )
            *str = *str_;
        *str = 0;
    }

    // Skip first '../'
    while( path[ 0 ] == '.' && path[ 1 ] == '.' && path[ 2 ] == DIR_SLASH_C )
        path += 3;

    // Erase 'folder/../'
    char* str = Str::Substring( path, DIR_SLASH_SDD );
    if( str )
    {
        // Erase interval
        char* str_ = str + 3;
        str -= 2;
        for( ; str >= path; str-- )
            if( *str == DIR_SLASH_C )
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

    // Erase './'
    str = Str::Substring( path, DIR_SLASH_SD );
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
    char buf[ MAX_FOPATH ];
    Str::Copy( buf, fname );
    FormatPath( buf );

    const char* str = Str::Substring( buf, DIR_SLASH_S );
    if( str )
    {
        str++;
        while( true )
        {
            const char* str_ = Str::Substring( str, DIR_SLASH_S );
            if( str_ )
                str = str_ + 1;
            else
                break;
        }
        size_t len = str - buf;
        if( len )
            memcpy( path, buf, len );
        path[ len ] = 0;
    }
    else
    {
        path[ 0 ] = 0;
    }
}

void FileManager::ExtractFileName( const char* fname, char* name )
{
    char buf[ MAX_FOPATH ];
    Str::Copy( buf, fname );
    FormatPath( buf );

    const char* str = Str::Substring( buf, DIR_SLASH_S );
    if( str )
    {
        str++;
        while( true )
        {
            const char* str_ = Str::Substring( str, DIR_SLASH_S );
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
        Str::Copy( name, MAX_FOPATH, fname );
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

    if( Str::Substring( name_, DIR_SLASH_S ) && name_[ 0 ] != '.' )
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

void FileManager::RecursiveDirLook( const char* base_dir, const char* cur_dir, bool include_subdirs, const char* ext, StrVec& result, vector< FIND_DATA >* files, vector< FIND_DATA >* dirs )
{
    char path[ MAX_FOPATH ];
    Str::Copy( path, base_dir );
    Str::Append( path, cur_dir );

    FIND_DATA fd;
    void*     h = FileFindFirst( path, NULL, fd );
    while( h )
    {
        if( fd.IsDirectory )
        {
            if( dirs )
                dirs->push_back( fd );

            if( include_subdirs )
            {
                Str::Format( path, "%s%s%s", cur_dir, fd.FileName, DIR_SLASH_S );
                RecursiveDirLook( base_dir, path, include_subdirs, ext, result, files, dirs );
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
                    result.push_back( path );
                    if( files )
                        files->push_back( fd );
                }
            }
            else
            {
                Str::Copy( path, cur_dir );
                Str::Append( path, fd.FileName );
                result.push_back( path );
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

void FileManager::GetFolderFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result, vector< FIND_DATA >* files /* = NULL */, vector< FIND_DATA >* dirs /* = NULL */ )
{
    // Format path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    FormatPath( path_ );

    // Find in folder files
    RecursiveDirLook( path_, "", include_subdirs, ext, result, files, dirs );
}

void FileManager::GetDataFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    // Format path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    FormatPath( path_ );

    // Change slashes back to slash, because dat tree use '\'
    for( char* str = path_; *str; str++ )
        if( *str == '/' )
            *str = '\\';

    // Find in dat files
    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
    {
        DataFile* dat = *it;
        dat->GetFileNames( path_, include_subdirs, ext, result );
    }
}

FilesCollection::FilesCollection( int path_type, const char* ext, bool include_subdirs )
{
    curFileIndex = 0;
    searchPath = FileManager::GetDataPath( "", path_type );
    FileManager::GetDataFileNames( searchPath.c_str(), include_subdirs, ext, fileNames );
}

FilesCollection::FilesCollection( int path_type, const char* dir, const char* ext, bool include_subdirs )
{
    curFileIndex = 0;
    searchPath = FileManager::GetDataPath( dir, path_type );
    if( searchPath.length() > 0 && searchPath[ searchPath.length() - 1 ] != DIR_SLASH_C )
        searchPath.append( DIR_SLASH_S );
    FileManager::GetDataFileNames( searchPath.c_str(), include_subdirs, ext, fileNames );
}

bool FilesCollection::IsNextFile()
{
    return curFileIndex < fileNames.size();
}

FileManager& FilesCollection::GetNextFile( const char** name /* = NULL */, bool name_with_path /* = false */, bool no_read_data /* = false */ )
{
    curFileIndex++;
    curFile.LoadFile( fileNames[ curFileIndex - 1 ].c_str(), PT_DATA, no_read_data );
    if( name )
    {
        char fname[ MAX_FOPATH ];
        if( !name_with_path )
        {
            FileManager::ExtractFileName( fileNames[ curFileIndex - 1 ].c_str(), fname );
            FileManager::EraseExtension( fname );
            curFileName = fname;
        }
        else
        {
            Str::Copy( fname, fileNames[ curFileIndex - 1 ].c_str() );
            char* s = Str::Substring( fname, searchPath.c_str() );
            RUNTIME_ASSERT( s );
            curFileName = s + searchPath.length();
        }
        *name = curFileName.c_str();
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
