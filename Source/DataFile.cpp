#include "Common.h"
#include "DataFile.h"
#include "minizip/unzip.h"
#include "FileManager.h"
#include "Resources/Resources.h"

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_MAPPER )
# define DISABLE_FOLDER_CACHING
#endif

/************************************************************************/
/* Folder/Dat/Zip loaders                                               */
/************************************************************************/

template< class T >
void GetFileNames_( const T& index_map, const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    char        path_[ MAX_FOPATH ];
    size_t      len = 0;
    const char* from = path;
    char*       to = path_;
    for( ; *from; from++, to++, len++ )
        *to = ( *from != '\\' ? *from : '/' );
    *to = 0;

    if( len > 0 && path_[ len - 1 ] != '/' )
    {
        path_[ len++ ] = '/';
        path_[ len ] = 0;
    }

    for( auto it = index_map.begin(), end = index_map.end(); it != end; ++it )
    {
        bool          add = false;
        const string& fname = it->first;
        if( !fname.compare( 0, len, path_ ) && ( include_subdirs ||
                                                 ( len > 0 && fname.find_last_of( '/' ) < len ) ||
                                                 ( len == 0 && fname.find_last_of( '/' ) == string::npos ) ) )
        {
            if( ext && *ext )
            {
                size_t pos = fname.find_last_of( '.' );
                if( pos != string::npos && Str::CompareCase( &fname.c_str()[ pos + 1 ], ext ) )
                    add = true;
            }
            else
            {
                add = true;
            }
        }
        if( add && std::find( result.begin(), result.end(), fname ) == result.end() )
            result.push_back( fname );
    }
}

class FolderFile: public DataFile
{
private:
    struct FileEntry
    {
        string FileName;
        string ShortFileName;
        uint   FileSize;
        uint64 WriteTime;
    };
    typedef map< string, FileEntry > IndexMap;

    #ifndef DISABLE_FOLDER_CACHING
    IndexMap filesTree;
    #endif
    string   basePath;

    void CollectFilesTree( IndexMap& files_tree );

public:
    bool Init( const char* fname );

    const string& GetPackName() { return basePath; }
    bool          IsFilePresent( const char* path, uint& size, uint64& write_time );
    uchar*        OpenFile( const char* path, uint& size, uint64& write_time );
    void          GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result );
};

class FalloutDatFile: public DataFile
{
private:
    typedef map< string, uchar* > IndexMap;

    IndexMap filesTree;
    string   fileName;
    uchar*   memTree;
    void*    datHandle;
    uint64   writeTime;
    uint     fileSize;

    bool ReadTree();

public:
    bool Init( const char* fname );
    ~FalloutDatFile();

    const string& GetPackName() { return fileName; }
    bool          IsFilePresent( const char* path, uint& size, uint64& write_time );
    uchar*        OpenFile( const char* path, uint& size, uint64& write_time );
    void          GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result ) { GetFileNames_( filesTree, path, include_subdirs, ext, result ); }
};

class ZipFile: public DataFile
{
private:
    struct ZipFileInfo
    {
        unz_file_pos Pos;
        int          UncompressedSize;
    };
    typedef map< string, ZipFileInfo > IndexMap;

    IndexMap filesTree;
    string   fileName;
    unzFile  zipHandle;
    uint64   writeTime;
    uint     fileSize;

    bool ReadTree();

public:
    bool Init( const char* fname );
    ~ZipFile();

    const string& GetPackName() { return fileName; }
    bool          IsFilePresent( const char* path, uint& size, uint64& write_time );
    uchar*        OpenFile( const char* path, uint& size, uint64& write_time );
    void          GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result ) { GetFileNames_( filesTree, path, include_subdirs, ext, result ); }
};

/************************************************************************/
/* Manage                                                               */
/************************************************************************/

DataFile* OpenDataFile( const char* path )
{
    if( !path || !path[ 0 ] )
    {
        WriteLogF( _FUNC_, " - Invalid file name, empty or nullptr.\n" );
        return nullptr;
    }

    const char* ext = FileManager::GetExtension( path );
    if( ext && Str::CompareCase( ext, "dat" ) )
    {
        FalloutDatFile* dat = new FalloutDatFile();
        if( !dat->Init( path ) )
        {
            WriteLogF( _FUNC_, " - Unable to open DAT file '%s'.\n", path );
            delete dat;
            return nullptr;
        }
        return dat;
    }
    else if( ( ext && ( Str::CompareCase( ext, "zip" ) || Str::CompareCase( ext, "bos" ) ) ) || path[ 0 ] == '$' )
    {
        ZipFile* zip = new ZipFile();
        if( !zip->Init( path ) )
        {
            WriteLogF( _FUNC_, " - Unable to open ZIP file '%s'.\n", path );
            delete zip;
            return nullptr;
        }
        return zip;
    }
    else
    {
        FolderFile* folder = new FolderFile();
        if( !folder->Init( path ) )
        {
            WriteLogF( _FUNC_, " - Unable to open folder '%s'.\n", path );
            delete folder;
            return nullptr;
        }
        return folder;
    }

    return nullptr;
}

/************************************************************************/
/* Folder file                                                          */
/************************************************************************/

bool FolderFile::Init( const char* fname )
{
    basePath = fname;
    #ifndef DISABLE_FOLDER_CACHING
    CollectFilesTree( filesTree );
    #endif

    return true;
}

void FolderFile::CollectFilesTree( IndexMap& files_tree )
{
    files_tree.clear();

    StrVec      files;
    FindDataVec find_data;
    FileManager::GetFolderFileNames( basePath.c_str(), true, nullptr, files, &find_data );

    for( size_t i = 0, j = files.size(); i < j; i++ )
    {
        FileEntry fe;
        fe.FileName = basePath + files[ i ];
        fe.ShortFileName = files[ i ];
        fe.FileSize = find_data[ i ].FileSize;
        fe.WriteTime = find_data[ i ].WriteTime;

        files_tree.insert( PAIR( files[ i ], fe ) );
    }
}

bool FolderFile::IsFilePresent( const char* path, uint& size, uint64& write_time )
{
    #ifndef DISABLE_FOLDER_CACHING
    auto it = filesTree.find( path );
    if( it == filesTree.end() )
        return nullptr;

    FileEntry& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;

    #else
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, basePath.c_str() );
    Str::Append( path_, path );
    void* f = FileOpen( path_, false );
    if( !f )
        return false;
    size = FileGetSize( f );
    write_time = FileGetWriteTime( f );
    FileClose( f );
    return true;
    #endif
}

uchar* FolderFile::OpenFile( const char* path, uint& size, uint64& write_time )
{
    #ifndef DISABLE_FOLDER_CACHING
    auto it = filesTree.find( path );
    if( it == filesTree.end() )
        return nullptr;

    FileEntry& fe = it->second;
    void*      f = FileOpen( fe.FileName.c_str(), false );
    if( !f )
        return nullptr;

    size = fe.FileSize;
    uchar* buf = new uchar[ size + 1 ];
    if( !FileRead( f, buf, size ) )
    {
        FileClose( f );
        delete[] buf;
        return nullptr;
    }
    FileClose( f );
    buf[ size ] = 0;
    write_time = fe.WriteTime;
    return buf;

    #else
    char path_[ MAX_FOPATH ];
    Str::Copy( path_, basePath.c_str() );
    Str::Append( path_, path );
    void* f = FileOpen( path_, false );
    if( !f )
        return nullptr;

    size = FileGetSize( f );
    uchar* buf = new uchar[ size + 1 ];
    if( !FileRead( f, buf, size ) )
    {
        FileClose( f );
        delete[] buf;
        return nullptr;
    }
    write_time = FileGetWriteTime( f );
    FileClose( f );
    buf[ size ] = 0;
    return buf;
    #endif
}

void FolderFile::GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    #ifndef DISABLE_FOLDER_CACHING
    IndexMap& files_tree = filesTree;
    #else
    IndexMap  files_tree;
    CollectFilesTree( files_tree );
    #endif

    StrVec result_;
    GetFileNames_( files_tree, path, include_subdirs, ext, result_ );
    for( size_t i = 0, j = result_.size(); i < j; i++ )
        result_[ i ] = files_tree[ result_[ i ] ].ShortFileName;
    if( !result_.empty() )
        result.insert( result.begin(), result_.begin(), result_.end() );
}

/************************************************************************/
/* Dat file                                                             */
/************************************************************************/

bool FalloutDatFile::Init( const char* fname )
{
    datHandle = nullptr;
    memTree = nullptr;
    fileName = fname;

    datHandle = FileOpen( fname, false );
    if( !datHandle )
    {
        WriteLogF( _FUNC_, " - Cannot open file.\n" );
        return false;
    }

    fileSize = FileGetSize( datHandle );
    writeTime = FileGetWriteTime( datHandle );

    if( !ReadTree() )
    {
        WriteLogF( _FUNC_, " - Read file tree fail.\n" );
        return false;
    }

    return true;
}

FalloutDatFile::~FalloutDatFile()
{
    if( datHandle )
    {
        FileClose( datHandle );
        datHandle = nullptr;
    }
    SAFEDELA( memTree );
}

bool FalloutDatFile::ReadTree()
{
    uint version;
    if( !FileSetPointer( datHandle, -12, SEEK_END ) )
        return false;
    if( !FileRead( datHandle, &version, 4 ) )
        return false;

    // DAT 2.1 Arcanum
    if( version == 0x44415431 ) // 1TAD
    {
        // Readed data
        uint files_total, tree_size;

        // Read info
        if( !FileSetPointer( datHandle, -4, SEEK_END ) )
            return false;
        if( !FileRead( datHandle, &tree_size, 4 ) )
            return false;

        // Read tree
        if( !FileSetPointer( datHandle, -(int) tree_size, SEEK_END ) )
            return false;
        if( !FileRead( datHandle, &files_total, 4 ) )
            return false;
        tree_size -= 28 + 4;     // Subtract information block and files total
        if( ( memTree = new uchar[ tree_size ] ) == nullptr )
            return false;
        memzero( memTree, tree_size );
        if( !FileRead( datHandle, memTree, tree_size ) )
            return false;

        // Indexing tree
        char   name[ MAX_FOPATH ];
        uchar* ptr = memTree;
        uchar* end_ptr = memTree + tree_size;
        while( true )
        {
            uint fnsz = *(uint*) ptr;                            // Include zero
            uint type = *(uint*) ( ptr + 4 + fnsz + 4 );

            if( fnsz > 1 && fnsz < MAX_FOPATH && type != 0x400 ) // Not folder
            {
                memcpy( name, ptr + 4, fnsz );
                name[ fnsz ] = 0;
                NormalizePathSlashes( name );
                Str::Lower( name );
                if( type == 2 )
                    *( ptr + 4 + fnsz + 7 ) = 1;               // Compressed
                filesTree.insert( PAIR( string( name ), ptr + 4 + fnsz + 7 ) );
            }

            if( ptr + fnsz + 24 >= end_ptr )
                break;
            ptr += fnsz + 24;
        }

        return true;
    }

    // DAT 2.0 Fallout2
    // Readed data
    uint dir_count, dat_size, files_total, tree_size;

    // Read info
    if( !FileSetPointer( datHandle, -8, SEEK_END ) )
        return false;
    if( !FileRead( datHandle, &tree_size, 4 ) )
        return false;
    if( !FileRead( datHandle, &dat_size, 4 ) )
        return false;

    // Check for DAT1.0 Fallout1 dat file
    if( !FileSetPointer( datHandle, 0, SEEK_SET ) )
        return false;
    if( !FileRead( datHandle, &dir_count, 4 ) )
        return false;
    dir_count >>= 24;
    if( dir_count == 0x01 || dir_count == 0x33 )
        return false;

    // Check for truncated
    if( FileGetSize( datHandle ) != dat_size )
        return false;

    // Read tree
    if( !FileSetPointer( datHandle, -( (int) tree_size + 8 ), SEEK_END ) )
        return false;
    if( !FileRead( datHandle, &files_total, 4 ) )
        return false;
    tree_size -= 4;
    if( ( memTree = new uchar[ tree_size ] ) == nullptr )
        return false;
    memzero( memTree, tree_size );
    if( !FileRead( datHandle, memTree, tree_size ) )
        return false;

    // Indexing tree
    char   name[ MAX_FOPATH ];
    uchar* ptr = memTree;
    uchar* end_ptr = memTree + tree_size;
    while( true )
    {
        uint fnsz = *(uint*) ptr;

        if( fnsz && fnsz + 1 < MAX_FOPATH )
        {
            memcpy( name, ptr + 4, fnsz );
            name[ fnsz ] = 0;
            NormalizePathSlashes( name );
            Str::Lower( name );
            filesTree.insert( PAIR( string( name ), ptr + 4 + fnsz ) );
        }

        if( ptr + fnsz + 17 >= end_ptr )
            break;
        ptr += fnsz + 17;
    }

    return true;
}

bool FalloutDatFile::IsFilePresent( const char* path, uint& size, uint64& write_time )
{
    if( !datHandle )
        return false;

    if( filesTree.find( path ) != filesTree.end() )
    {
        size = fileSize;
        write_time = writeTime;
        return true;
    }
    return false;
}

uchar* FalloutDatFile::OpenFile( const char* path, uint& size, uint64& write_time )
{
    if( !datHandle )
        return nullptr;

    auto it = filesTree.find( path );
    if( it == filesTree.end() )
        return nullptr;

    uchar* ptr = it->second;
    uchar  type = *ptr;
    uint   real_size = *(uint*) ( ptr + 1 );
    uint   packed_size = *(uint*) ( ptr + 5 );
    uint   offset = *(uint*) ( ptr + 9 );

    if( !FileSetPointer( datHandle, offset, SEEK_SET ) )
        return nullptr;

    size = real_size;
    uchar* buf = new uchar[ size + 1 ];

    if( !type )
    {
        // Plane data
        if( !FileRead( datHandle, buf, size ) )
        {
            delete[] buf;
            return nullptr;
        }
    }
    else
    {
        // Packed data
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.next_in = Z_NULL;
        stream.avail_in = 0;
        if( inflateInit( &stream ) != Z_OK )
        {
            delete[] buf;
            return nullptr;
        }

        stream.next_out = buf;
        stream.avail_out = real_size;

        uint left = packed_size;
        while( stream.avail_out )
        {
            if( !stream.avail_in && left > 0 )
            {
                #pragma RACE_CONDITION
                static uchar read_buf[ 0x40000 ];

                stream.next_in = read_buf;
                uint rb;
                if( !FileRead( datHandle, read_buf, left > sizeof( read_buf ) ? sizeof( read_buf ) : left, &rb ) )
                {
                    delete[] buf;
                    return nullptr;
                }
                stream.avail_in = rb;
                left -= rb;
            }
            int r = inflate( &stream, Z_NO_FLUSH );
            if( r != Z_OK && r != Z_STREAM_END )
            {
                delete[] buf;
                return nullptr;
            }
            if( r == Z_STREAM_END )
                break;
        }

        inflateEnd( &stream );
    }

    write_time = writeTime;
    buf[ size ] = 0;
    return buf;
}

/************************************************************************/
/* Zip file                                                             */
/************************************************************************/

bool ZipFile::Init( const char* fname )
{
    fileName = fname;
    zipHandle = nullptr;

    if( fname[ 0 ] != '$' )
    {
        char path[ MAX_FOPATH ];
        Str::Copy( path, fname );
        if( !ResolvePath( path ) )
        {
            WriteLogF( _FUNC_, " - Can't retrieve file full path.\n" );
            return false;
        }

        void* file = FileOpen( fname, false );
        if( !file )
        {
            WriteLogF( _FUNC_, " - Cannot open file.\n" );
            return false;
        }

        fileSize = FileGetSize( file );
        writeTime = FileGetWriteTime( file );

        FileClose( file );

        #ifdef FO_WINDOWS
        wchar_t path_wc[ MAX_FOPATH ];
        if( MultiByteToWideChar( CP_UTF8, 0, path, -1, path_wc, MAX_FOPATH ) == 0 ||
            WideCharToMultiByte( GetACP(), 0, path_wc, -1, path, MAX_FOPATH, nullptr, nullptr ) == 0 )
        {
            WriteLogF( _FUNC_, " - Code page conversion fail.\n" );
            return false;
        }
        #endif

        zipHandle = unzOpen( path );
        if( !zipHandle )
        {
            WriteLogF( _FUNC_, " - Cannot open ZIP file.\n" );
            return false;
        }
    }
    else
    {
        fileSize = 0;
        writeTime = 0;

        struct MemStream
        {
            const uchar* Buf;
            uint         Length;
            uint         Pos;
        };

        zlib_filefunc_def ffunc;
        ffunc.zopen_file = [] ( voidpf opaque, const char* filename, int mode )->voidpf
        {
            #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
            if( Str::Compare( filename, "$Basic" ) )
            {
                MemStream* mem_stream = new MemStream();
                mem_stream->Buf = Resource_Basic_zip;
                mem_stream->Length = sizeof( Resource_Basic_zip );
                mem_stream->Pos = 0;
                return mem_stream;
            }
            #endif
            return 0;
        };
        ffunc.zread_file = [] ( voidpf opaque, voidpf stream, void* buf, uLong size )->uLong
        {
            MemStream* mem_stream = (MemStream*) stream;
            memcpy( buf, mem_stream->Buf + mem_stream->Pos, size );
            mem_stream->Pos += (uint) size;
            return size;
        };
        ffunc.zwrite_file = [] ( voidpf opaque, voidpf stream, const void* buf, uLong size )->uLong
        {
            return 0;
        };
        ffunc.ztell_file = [] ( voidpf opaque, voidpf stream )->long
        {
            MemStream* mem_stream = (MemStream*) stream;
            return (long) mem_stream->Pos;
        };
        ffunc.zseek_file = [] ( voidpf opaque, voidpf stream, uLong offset, int origin )->long
        {
            MemStream* mem_stream = (MemStream*) stream;
            switch( origin )
            {
            case ZLIB_FILEFUNC_SEEK_SET:
                mem_stream->Pos = (uint) offset;
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                mem_stream->Pos += (uint) offset;
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                mem_stream->Pos = mem_stream->Length + offset;
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [] ( voidpf opaque, voidpf stream )->int
        {
            MemStream* mem_stream = new MemStream();
            delete mem_stream;
            return 0;
        };
        ffunc.zerror_file = [] ( voidpf opaque, voidpf stream )->int
        {
            if( stream == nullptr )
                return 1;
            return 0;
        };
        ffunc.opaque = nullptr;

        zipHandle = unzOpen2( fname, &ffunc );
        if( !zipHandle )
        {
            WriteLogF( _FUNC_, " - Cannot open ZIP file in memory.\n" );
            return false;
        }
    }

    if( !ReadTree() )
    {
        WriteLogF( _FUNC_, " - Read file tree fail.\n" );
        return false;
    }

    return true;
}

ZipFile::~ZipFile()
{
    if( zipHandle )
    {
        unzClose( zipHandle );
        zipHandle = nullptr;
    }
}

bool ZipFile::ReadTree()
{
    unz_global_info gi;
    if( unzGetGlobalInfo( zipHandle, &gi ) != UNZ_OK || gi.number_entry == 0 )
        return false;

    ZipFileInfo   zip_info;
    unz_file_pos  pos;
    unz_file_info info;
    char          name[ MAX_FOPATH ] = { 0 };
    for( uLong i = 0; i < gi.number_entry; i++ )
    {
        if( unzGetFilePos( zipHandle, &pos ) != UNZ_OK )
            return false;
        if( unzGetCurrentFileInfo( zipHandle, &info, name, MAX_FOPATH, nullptr, 0, nullptr, 0 ) != UNZ_OK )
            return false;

        if( !( info.external_fa & 0x10 ) )   // Not folder
        {
            NormalizePathSlashes( name );
            zip_info.Pos = pos;
            zip_info.UncompressedSize = (int) info.uncompressed_size;
            filesTree.insert( PAIR( string( name ), zip_info ) );
        }

        if( i + 1 < gi.number_entry && unzGoToNextFile( zipHandle ) != UNZ_OK )
            return false;
    }

    return true;
}

bool ZipFile::IsFilePresent( const char* path, uint& size, uint64& write_time )
{
    if( !zipHandle )
        return false;

    if( filesTree.find( path ) != filesTree.end() )
    {
        size = fileSize;
        write_time = writeTime;
        return true;
    }
    return false;
}

uchar* ZipFile::OpenFile( const char* path, uint& size, uint64& write_time )
{
    if( !zipHandle )
        return nullptr;

    auto it = filesTree.find( path );
    if( it == filesTree.end() )
        return nullptr;

    ZipFileInfo& info = it->second;

    if( unzGoToFilePos( zipHandle, &info.Pos ) != UNZ_OK )
        return nullptr;

    uchar* buf = new uchar[ info.UncompressedSize + 1 ];
    if( !buf )
        return nullptr;

    if( unzOpenCurrentFile( zipHandle ) != UNZ_OK )
    {
        delete[] buf;
        return nullptr;
    }

    int read = unzReadCurrentFile( zipHandle, buf, info.UncompressedSize );
    if( unzCloseCurrentFile( zipHandle ) != UNZ_OK || read != info.UncompressedSize )
    {
        delete[] buf;
        return nullptr;
    }

    write_time = writeTime;
    size = info.UncompressedSize;
    buf[ size ] = 0;
    return buf;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
