#include "Common.h"
#include "FileManager.h"
#include "FileSystem.h"

#define OUT_BUF_START_SIZE    ( 0x100 )

DataFileVec FileManager::dataFiles;
string      FileManager::writeDir;

FileManager::FileManager()
{
    fileLoaded = false;
    curPos = 0;
    fileSize = 0;
    writeTime = 0;
    dataOutBuf = nullptr;
    fileBuf = nullptr;
    posOutBuf = 0;
    endOutBuf = 0;
    lenOutBuf = 0;
}

FileManager::FileManager( const string& path, bool no_read /* = false */ ): FileManager()
{
    LoadFile( path, no_read );
}

FileManager::FileManager( const uchar* stream, uint length ): FileManager()
{
    LoadStream( stream, length );
}

FileManager::~FileManager()
{
    UnloadFile();
}

void FileManager::InitDataFiles( const string& path, bool set_write_dir /* = true */ )
{
    // Format path
    string fixed_path = _str( path ).formatPath();
    if( !fixed_path.empty() && fixed_path.front() != '$' && fixed_path.back() != '/' )
        fixed_path += "/";

    // Check special files
    if( fixed_path.empty() || fixed_path.front() != '$' )
    {
        // Redirect path
        void* redirection_link = FileOpen( fixed_path + "Redirection.link", false );
        if( redirection_link )
        {
            uint  len = FileGetSize( redirection_link );
            char* link = (char*) alloca( len + 1 );
            FileRead( redirection_link, link, len );
            FileClose( redirection_link );
            link[ len ] = 0;
            InitDataFiles( _str( fixed_path + link ).formatPath(), set_write_dir );
            return;
        }

        // Additional path
        void* extension_link = FileOpen( fixed_path + "Extension.link", false );
        if( extension_link )
        {
            uint  len = FileGetSize( extension_link );
            char* link = (char*) alloca( len + 1 );
            FileRead( extension_link, link, len );
            FileClose( extension_link );
            link[ len ] = 0;
            InitDataFiles( _str( fixed_path + link ).formatPath(), false );
        }
    }

    // Write path first
    if( set_write_dir && ( fixed_path.empty() || fixed_path[ 0 ] != '$' ) )
        writeDir = fixed_path;

    // Process dir
    if( !LoadDataFile( fixed_path ) )
        RUNTIME_ASSERT( !"Unable to load files in folder." );
}

bool FileManager::LoadDataFile( const string& path, bool skip_inner /* = false */ )
{
    DataFile* data_file = nullptr;

    // Find already loaded
    for( DataFile* df : dataFiles )
    {
        if( df->GetPackName() == path )
        {
            data_file = df;
            break;
        }
    }

    // Add new
    if( !data_file )
    {
        data_file = OpenDataFile( path.c_str() );
        if( !data_file )
        {
            WriteLog( "Load data '{}' fail.\n", path );
            return false;
        }
    }

    // Inner data files
    if( !skip_inner )
    {
        StrVec files;
        data_file->GetFileNames( "", false, "dat", files );
        data_file->GetFileNames( "", false, "zip", files );
        data_file->GetFileNames( "", false, "bos", files );

        std::sort( files.begin(), files.end(), std::less< string >() );

        for( size_t i = 0; i < files.size(); i++ )
        {
            if( !LoadDataFile( ( path[ 0 ] != '$' ? path : "" ) + files[ i ], true ) )
            {
                WriteLog( "Unable to load inner data file." );
                return false;
            }
        }
    }

    // Put to begin of list
    dataFiles.insert( dataFiles.begin(), data_file );
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
    fileLoaded = false;
    SAFEDELA( fileBuf );
    fileSize = 0;
    writeTime = 0;
    curPos = 0;
    SAFEDELA( dataOutBuf );
    posOutBuf = 0;
    lenOutBuf = 0;
    endOutBuf = 0;
}

uchar* FileManager::ReleaseBuffer()
{
    fileLoaded = false;
    uchar* tmp = fileBuf;
    fileBuf = nullptr;
    fileSize = 0;
    curPos = 0;
    return tmp;
}

bool FileManager::LoadFile( const string& path, bool no_read /* = false */ )
{
    UnloadFile();

    if( !path.empty() && path[ 0 ] != '.' && path[ 0 ] != '/' && ( path.length() < 2 || path[ 1 ] != ':' ) )
    {
        // Make data path
        string data_path = _str( path ).formatPath();
        string data_path_lower = _str( data_path ).lower();

        // Find file in every data file
        for( auto it = dataFiles.begin(), end = dataFiles.end(); it != end; ++it )
        {
            DataFile* dat = *it;
            if( !no_read )
            {
                uint   file_size;
                uint64 write_time;
                fileBuf = dat->OpenFile( data_path, data_path_lower, file_size, write_time );
                if( fileBuf )
                {
                    curPos = 0;
                    fileSize = file_size;
                    writeTime = write_time;
                    fileLoaded = true;
                    return true;
                }
            }
            else
            {
                uint   file_size;
                uint64 write_time;
                if( dat->IsFilePresent( data_path, data_path_lower, file_size, write_time ) )
                {
                    curPos = 0;
                    fileSize = file_size;
                    writeTime = write_time;
                    fileLoaded = true;
                    return true;
                }
            }
        }
    }
    else
    {
        void* file = FileOpen( _str( path ).formatPath(), false );
        if( file )
        {
            uint   file_size = FileGetSize( file );
            uint64 write_time = FileGetWriteTime( file );

            if( !no_read )
            {
                uchar* buf = new uchar[ file_size + 1 ];
                bool   result = FileRead( file, buf, file_size );
                FileClose( file );
                if( !result )
                {
                    delete[] buf;
                    return false;
                }

                fileBuf = buf;
                fileBuf[ file_size ] = 0;
            }
            else
            {
                FileClose( file );
            }

            curPos = 0;
            fileSize = file_size;
            writeTime = write_time;
            fileLoaded = true;
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
    fileBuf = new uchar[ fileSize + 1 ];
    memcpy( fileBuf, stream, fileSize );
    fileBuf[ fileSize ] = 0;
    curPos = 0;
    fileLoaded = true;
    return true;
}

void FileManager::SetCurPos( uint pos )
{
    RUNTIME_ASSERT( fileBuf );
    curPos = pos;
}

void FileManager::GoForward( uint offs )
{
    RUNTIME_ASSERT( fileBuf );
    curPos += offs;
}

void FileManager::GoBack( uint offs )
{
    RUNTIME_ASSERT( fileBuf );
    curPos -= offs;
}

bool FileManager::FindFragment( const uchar* fragment, uint fragment_len, uint begin_offs )
{
    RUNTIME_ASSERT( fileBuf );
    if( fragment_len > fileSize )
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

string FileManager::GetNonEmptyLine()
{
    RUNTIME_ASSERT( fileBuf );
    if( curPos >= fileSize )
        return "";

    while( curPos < fileSize )
    {
        uint start = curPos;
        uint len = 0;
        while( curPos < fileSize )
        {
            if( fileBuf[ curPos ] == '\r' || fileBuf[ curPos ] == '\n' || fileBuf[ curPos ] == '#' || fileBuf[ curPos ] == ';' )
            {
                for( ; curPos < fileSize; curPos++ )
                    if( fileBuf[ curPos ] == '\n' )
                        break;
                curPos++;
                break;
            }

            curPos++;
            len++;
        }

        if( len )
        {
            string line = _str( string( (const char*) &fileBuf[ start ], len ) ).trim();
            if( !line.empty() )
                return line;
        }
    }

    return "";
}

bool FileManager::CopyMem( void* ptr, uint size )
{
    RUNTIME_ASSERT( fileBuf );
    if( !size )
        return false;
    if( curPos + size > fileSize )
        return false;

    memcpy( ptr, fileBuf + curPos, size );
    curPos += size;
    return true;
}

string FileManager::GetStrNT()
{
    RUNTIME_ASSERT( fileBuf );
    if( curPos + 1 > fileSize )
        return "";

    uint len = 0;
    while( *( fileBuf + curPos + len ) )
        len++;

    string str( (const char*) &fileBuf[ curPos ], len );
    curPos += len + 1;
    return str;
}

uchar FileManager::GetUChar()
{
    RUNTIME_ASSERT( fileBuf );
    if( curPos + sizeof( uchar ) > fileSize )
        return 0;

    uchar res = 0;
    res = fileBuf[ curPos++ ];
    return res;
}

ushort FileManager::GetBEUShort()
{
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( fileBuf );
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
    RUNTIME_ASSERT( dataOutBuf );
    fileLoaded = true;
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
    RUNTIME_ASSERT( fileBuf );
    fileLoaded = false;
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
        lenOutBuf = OUT_BUF_START_SIZE;
        memzero( (void*) dataOutBuf, lenOutBuf );
        return true;
    }

    uchar* old_obuf = dataOutBuf;
    dataOutBuf = new uchar[ lenOutBuf * 2 ];
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

bool FileManager::SaveFile( const string& fname )
{
    RUNTIME_ASSERT( dataOutBuf || !endOutBuf );

    string fpath = GetWritePath( fname );
    void*  file = FileOpen( fpath, true );
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

void FileManager::SetData( const void* data, uint len )
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

void FileManager::SetStr( const string& str )
{
    SetData( str.c_str(), (uint) str.length() );
}

void FileManager::SetStrNT( const string& str )
{
    SetData( str.c_str(), (uint) str.length() + 1 );
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
    #ifdef FO_WINDOWS
    SetCurrentDirectoryW( _str( GameOpt.WorkDir ).toWideChar().c_str() );
    #else
    chdir( GameOpt.WorkDir.c_str() );
    #endif
}

void FileManager::SetCurrentDir( const string& dir, const string& write_dir )
{
    string resolved_dir = _str( dir ).formatPath().resolvePath();
    #ifdef FO_WINDOWS
    SetCurrentDirectoryW( _str( resolved_dir ).toWideChar().c_str() );
    #else
    chdir( resolved_dir.c_str() );
    #endif

    writeDir = _str( write_dir ).formatPath();
    if( !writeDir.empty() && writeDir.back() != '/' )
        writeDir += "/";
}

string FileManager::GetWritePath( const string& fname )
{
    return _str( writeDir + fname ).formatPath();
}

bool FileManager::IsFileExists( const string& fname )
{
    return FileExist( fname );
}

bool FileManager::CopyFile( const string& from, const string& to )
{
    string dir = _str( to ).extractDir();
    if( !dir.empty() )
        MakeDirectoryTree( dir );

    return FileCopy( from, to );
}

bool FileManager::RenameFile( const string& from, const string& to )
{
    string dir = _str( to ).extractDir();
    if( !dir.empty() )
        MakeDirectoryTree( dir );

    return FileRename( from, to );
}

bool FileManager::DeleteFile( const string& fname )
{
    return FileDelete( fname );
}

void FileManager::DeleteDir( const string& dir )
{
    FilesCollection files( "", dir.c_str() );
    while( files.IsNextFile() )
    {
        string path;
        files.GetNextFile( nullptr, &path, nullptr, true );
        DeleteFile( path );
    }

    #ifdef FO_WINDOWS
    RemoveDirectoryW( _str( dir ).toWideChar().c_str() );
    #else
    rmdir( dir.c_str() );
    #endif
}

void FileManager::CreateDirectoryTree( const string& path )
{
    MakeDirectoryTree( path );
}

string FileManager::GetExePath()
{
    #ifdef FO_WINDOWS
    wchar_t exe_path[ MAX_FOPATH ];
    DWORD   r = GetModuleFileNameW( GetModuleHandle( nullptr ), exe_path, sizeof( exe_path ) );
    RUNTIME_ASSERT_STR( r, "Get executable path failed" );
    return _str().parseWideChar( exe_path );
    #else
    RUNTIME_ASSERT( !"Invalid platform for executable path" );
    return "";
    #endif
}

void FileManager::RecursiveDirLook( const string& base_dir, const string& cur_dir, bool include_subdirs, const string& ext, StrVec& files_path, FindDataVec* files, StrVec* dirs_path, FindDataVec* dirs )
{
    FindData fd;
    void*    h = FileFindFirst( base_dir + cur_dir, "", &fd.FileName, &fd.FileSize, &fd.WriteTime, &fd.IsDirectory );
    while( h )
    {
        if( fd.IsDirectory )
        {
            if( fd.FileName[ 0 ] != '_' )
            {
                string path = cur_dir + fd.FileName + "/";

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
            if( ext.empty() || _str( fd.FileName ).getFileExtension() == ext )
            {
                files_path.push_back( cur_dir + fd.FileName );
                if( files )
                    files->push_back( fd );
            }
        }

        if( !FileFindNext( h, &fd.FileName, &fd.FileSize, &fd.WriteTime, &fd.IsDirectory ) )
            break;
    }
    if( h )
        FileFindClose( h );
}

void FileManager::GetFolderFileNames( const string& path, bool include_subdirs, const string& ext, StrVec& files_path, FindDataVec* files /* = NULL */, StrVec* dirs_path /* = NULL */, FindDataVec* dirs /* = NULL */ )
{
    RecursiveDirLook( _str( path ).formatPath(), "", include_subdirs, ext, files_path, files, dirs_path, dirs );
}

void FileManager::GetDataFileNames( const string& path, bool include_subdirs, const string& ext, StrVec& result )
{
    for( DataFile* dataFile : dataFiles )
        dataFile->GetFileNames( _str( path ).formatPath(), include_subdirs, ext, result );
}

FilesCollection::FilesCollection( const string& ext, const string& fixed_dir /* = "" */ )
{
    curFileIndex = 0;

    StrVec find_dirs;
    if( fixed_dir.empty() )
        find_dirs = ProjectFiles;
    else
        find_dirs.push_back( fixed_dir );

    for( const string& find_dir : find_dirs )
    {
        StrVec paths;
        FileManager::GetFolderFileNames( find_dir, true, ext, paths );
        for( size_t i = 0; i < paths.size(); i++ )
        {
            string path = find_dir + paths[ i ];
            string relative_path = paths[ i ];

            // Link to another file
            if( _str( path ).getFileExtension() == "link" )
            {
                FileManager link;
                if( !link.LoadFile( path ) )
                {
                    WriteLog( "Can't read link file '{}'.\n", path );
                    continue;
                }

                const char* link_str = (const char*) link.GetBuf();
                path = _str( path ).forwardPath( link_str );
                relative_path = _str( relative_path ).forwardPath( link_str );
            }

            fileNames.push_back( _str( paths[ i ] ).extractFileName().eraseFileExtension() );
            filePaths.push_back( path );
            fileRelativePaths.push_back( relative_path );
        }
    }
}

bool FilesCollection::IsNextFile()
{
    return curFileIndex < fileNames.size();
}

FileManager& FilesCollection::GetNextFile( string* name /* = nullptr */, string* path /* = nullptr */, string* relative_path /* = nullptr */, bool no_read_data /* = false */ )
{
    curFileIndex++;
    curFile.LoadFile( filePaths[ curFileIndex - 1 ], no_read_data );
    RUNTIME_ASSERT_STR( curFile.IsLoaded(), filePaths[ curFileIndex - 1 ] );
    if( name )
        *name = fileNames[ curFileIndex - 1 ];
    if( path )
        *path = filePaths[ curFileIndex - 1 ];
    if( relative_path )
        *relative_path = fileRelativePaths[ curFileIndex - 1 ];
    return curFile;
}

FileManager& FilesCollection::FindFile( const string& name, string* path /* = nullptr */, string* relative_path /* = nullptr */, bool no_read_data /* = false */ )
{
    curFile.UnloadFile();
    for( size_t i = 0; i < fileNames.size(); i++ )
    {
        if( fileNames[ i ] == name )
        {
            curFile.LoadFile( filePaths[ i ], no_read_data );
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
