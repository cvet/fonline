#include "StdAfx.h"
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
    #ifndef FO_OSX_IOS
    "save" DIR_SLASH_S,
    #else
    "../../../Documents/save/",
    #endif
    "fonts" DIR_SLASH_S,
    #ifndef FO_OSX_IOS
    "cache" DIR_SLASH_S,
    #else
    "../../../Documents/cache/",
    #endif
    "",
    "",
    "",
    "",
    "",
    "",

    // Server paths
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
    "",

    // Other
    "",
    "data" DIR_SLASH_S,
    "",
    "",
    "",
};

DataFileVec FileManager::dataFiles;
char        FileManager::dataPath[ MAX_FOPATH ] = { DIR_SLASH_SD };

void FileManager::SetDataPath( const char* path )
{
    Str::Copy( dataPath, path );
    if( dataPath[ Str::Length( dataPath ) - 1 ] != DIR_SLASH_C )
        Str::Append( dataPath, DIR_SLASH_S );
    FormatPath( dataPath );
    MakeDirectory( GetFullPath( "", PT_DATA ) );
}

void FileManager::SetCacheName( const char* name )
{
    char cache_path[ MAX_FOPATH ];
    Str::Format( cache_path, "%s%s%s", PathList[ PT_CACHE ], name && name[ 0 ] ? name : "dummy", DIR_SLASH_S );
    PathList[ PT_CACHE ] = Str::Duplicate( cache_path );
    MakeDirectory( GetFullPath( "", PT_CACHE ) );
}

void FileManager::InitDataFiles( const char* path )
{
    char list_path[ MAX_FOPATH ];
    Str::Format( list_path, "%sDataFiles.cfg", path );

    FileManager*           list = new FileManager();
    vector< FileManager* > vec;
    vec.push_back( list );
    if( list->LoadFile( list_path, -1 ) )
    {
        char line[ MAX_FOTEXT ];
        while( !vec.empty() )
        {
            if( !vec.back()->GetLine( line, 1024 ) )
            {
                delete vec.back();
                vec.pop_back();
                continue;
            }

            // Cut off comments
            char* comment1 = Str::Substring( line, "#" );
            char* comment2 = Str::Substring( line, ";" );
            if( comment1 )
                *comment1 = 0;
            if( comment2 )
                *comment2 = 0;

            // Cut off specific characters
            Str::EraseFrontBackSpecificChars( line );

            // Test for "include"
            if( Str::CompareCount( line, "include ", 8 ) )
            {
                Str::EraseInterval( line, 8 );
                Str::EraseFrontBackSpecificChars( line );

                // Make file path
                char fpath[ MAX_FOPATH ] = { 0 };
                if( line[ 1 ] != ':' )
                    Str::Copy( fpath, path );                                           // Relative path
                Str::Append( fpath, line );
                FormatPath( fpath );

                FileManager* mgr = new FileManager();
                if( !mgr->LoadFile( fpath, -1 ) )
                {
                    char errmsg[ MAX_FOTEXT ];
                    Str::Format( errmsg, "Data file '%s' not found. Run Updater.exe.", fpath );
                    ShowMessage( errmsg );
                    WriteLogF( _FUNC_, " - %s\n", errmsg );
                    delete mgr;
                    continue;
                }
                vec.push_back( mgr );
                continue;
            }

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
                ShowMessage( errmsg );
                WriteLogF( _FUNC_, " - %s\n", errmsg );
            }
        }
    }
    else
    {
        delete list;
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
    FormatPath( path_ );
    if( !ResolvePath( path_ ) )
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

bool FileManager::LoadFile( const char* fname, int path_type, bool no_read_data /* = false */  )
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
        Str::Copy( dat_path, PathList[ path_type ] );
        Str::Append( dat_path, fname );
        FormatPath( dat_path );
        #ifndef FO_WINDOWS
        if( dat_path[ 0 ] == '.' && dat_path[ 1 ] == DIR_SLASH_C )
            Str::CopyBack( dat_path ), Str::CopyBack( dat_path );
        #endif

        // Make folder path
        Str::Copy( folder_path, GetDataPath( path_type ) );
        FormatPath( folder_path );
        Str::Append( folder_path, dat_path );

        // Check for full path
        #ifdef FO_WINDOWS
        if( dat_path[ 1 ] == ':' )
            only_folder = true;                        // C:/folder/file.ext
        #else
        if( dat_path[ 0 ] == DIR_SLASH_C )
            only_folder = true;                        // /folder/file.ext
        #endif
        if( dat_path[ 0 ] == '.' && dat_path[ 1 ] == '.' && dat_path[ 2 ] == DIR_SLASH_C )
            only_folder = true;                        // ../folder/file.ext

        // Check for empty
        if( !dat_path[ 0 ] || !folder_path[ 0 ] )
            return false;
    }
    else if( path_type == -1 )
    {
        Str::Copy( folder_path, fname );
        FormatPath( folder_path );
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
        FileGetTime( file, timeCreate, timeAccess, timeWrite );

        if( no_read_data )
        {
            FileClose( file );
            return true;
        }

        uint   size = FileGetSize( file );
        uchar* buf = new uchar[ size + 1 ];
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

    // Filenames in dats in lower case
    Str::Lower( dat_path );

    // Change slashes back to slash, because dat tree use '\'
    #ifndef FO_WINDOWS
    for( char* str = dat_path; *str; str++ )
    {
        if( *str == '/' )
            *str = '\\';
    }
    #endif

    for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
    {
        DataFile* dat = *it;
        if( !no_read_data )
        {
            fileBuf = dat->OpenFile( dat_path, fileSize );
            if( fileBuf )
            {
                curPos = 0;
                dat->GetTime( &timeCreate, &timeAccess, &timeWrite );
                return true;
            }
        }
        else
        {
            if( dat->IsFilePresent( dat_path ) )
            {
                dat->GetTime( &timeCreate, &timeAccess, &timeWrite );
                return true;
            }
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
    if( !fname )
        return false;

    char fpath[ MAX_FOPATH ];
    if( path_type >= 0 && path_type < PATH_LIST_COUNT )
    {
        Str::Copy( fpath, GetDataPath( path_type ) );
        Str::Append( fpath, PathList[ path_type ] );
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
    FormatPath( fpath );

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
    vsprintf( str, fmt, list );
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
        Str::Append( buf, PathList[ path_type ] );
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
        Str::Append( get_path, MAX_FOPATH, PathList[ path_type ] );
    if( fname )
        Str::Append( get_path, MAX_FOPATH, fname );
    FormatPath( get_path );
}

const char* FileManager::GetPath( int path_type )
{
    static const char any[] = "error";
    return (uint) path_type >= PATH_LIST_COUNT ? any : PathList[ path_type ];
}

uint FileManager::GetFileHash( const char* fname, int path_type )
{
    char fpath[ MAX_FOPATH ];
    Str::Copy( fpath, GetPath( path_type ) );
    Str::Append( fpath, fname );
    return Str::GetHash( fpath );
}

const char* FileManager::GetDataPath( int path_type )
{
    static const char root_path[] = DIR_SLASH_SD;

    #if defined ( FONLINE_SERVER )
    if( path_type == PT_SERVER_ROOT )
        return root_path;
    #elif defined ( FONLINE_CLIENT )
    if( path_type == PT_ROOT )
        return root_path;
    #elif defined ( FONLINE_MAPPER )
    if( path_type == PT_MAPPER_ROOT || path_type == PT_MAPPER_DATA )
        return root_path;
    #endif

    return dataPath;
}

void FileManager::FormatPath( char* path, bool first_skipped /* = false */ )
{
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
    {
        path += 3;
        first_skipped = true;
    }

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
        FormatPath( path, first_skipped );
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
        FormatPath( path, first_skipped );
        return;
    }

    // Extra dot+slash in beginning
    #ifndef FO_WINDOWS
    if( !first_skipped && path[ 0 ] != DIR_SLASH_C )
        Str::Insert( path, DIR_SLASH_SD );
    #endif
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

    const char* str = Str::Substring( fname, DIR_SLASH_S );
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
        name[ 0 ] = 0;
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

void FileManager::CreateDirectoryTree( const char* path )
{
    char* work = Str::Duplicate( path );

    FormatPath( work );

    for( char* ptr = work; *ptr; ++ptr )
    {
        if( *ptr == DIR_SLASH_C )
        {
            *ptr = 0;
            MakeDirectory( work );
            *ptr = DIR_SLASH_C;
        }
    }

    delete[] work;
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

    CreateDirectoryTree( to );

    void* f_to = FileOpen( to, true );
    if( !f_to )
    {
        FileClose( f_from );
        return false;
    }

    uint   size = FileGetSize( f_from );
    uchar* buf = new uchar[ size ];
    bool   ok = ( FileRead( f_from, buf, size ) && FileWrite( f_to, buf, size ) );
    delete[] buf;

    FileClose( f_to );
    FileClose( f_from );

    if( !ok )
        FileDelete( to );
    return ok;
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

void FileManager::RecursiveDirLook( const char* init_dir, bool include_subdirs, const char* ext, StrVec& result )
{
    char path[ MAX_FOPATH ];
    char short_init_dir[ MAX_FOPATH ];
    Str::Format( path, "%s%s", dataPath, init_dir );

    // Short init dir name, no initial segment of the file name
    Str::Copy( short_init_dir, init_dir );
    int i = Str::Length( short_init_dir );
    while( i-- )
    {
        if( short_init_dir[ i ] == DIR_SLASH_C )
        {
            short_init_dir[ i + 1 ] = '\0';
            break;
        }
    }

    FIND_DATA fd;
    void*     h = FileFindFirst( path, NULL, fd );
    while( h )
    {
        if( fd.IsDirectory )
        {
            if( include_subdirs )
            {
                Str::Format( path, "%s%s%s", init_dir, fd.FileName, DIR_SLASH_S );
                RecursiveDirLook( path, include_subdirs, ext, result );
            }
        }
        else
        {
            if( ext )
            {
                const char* ext_ = GetExtension( fd.FileName );
                if( ext_ && Str::CompareCase( ext, ext_ ) )
                {
                    Str::Copy( path, short_init_dir );
                    Str::Append( path, fd.FileName );
                    result.push_back( path );
                }
            }
            else
            {
                Str::Copy( path, short_init_dir );
                Str::Append( path, fd.FileName );
                result.push_back( path );
            }
        }

        if( !FileFindNext( h, fd ) )
            break;
    }
    if( h )
        FileFindClose( h );
}

void FileManager::GetFolderFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    // Format path
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, path );
    FormatPath( path_ );
    #ifndef FO_WINDOWS
    // Erase './'
    Str::CopyBack( path_ );
    Str::CopyBack( path_ );
    #endif

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
