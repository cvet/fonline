#include "StdAfx.h"
#include "DataFile.h"
#include "zlib/unzip.h"

/************************************************************************/
/* Dat/Zip loaders                                                      */
/************************************************************************/

class FalloutDatFile: public DataFile
{
private:
    typedef map< string, uchar* > IndexMap;

    IndexMap filesTree;
    string   fileName;
    uchar*   memTree;
    void*    datHandle;

    uint64   timeCreate, timeAccess, timeWrite;

    bool ReadTree();

public:
    bool Init( const char* fname );
    ~FalloutDatFile();

    const string& GetPackName() { return fileName; }
    bool          IsFilePresent( const char* fname );
    uchar*        OpenFile( const char* fname, uint& len );
    void          GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result );
    void          GetTime( uint64* create, uint64* access, uint64* write );
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

    uint64   timeCreate, timeAccess, timeWrite;

    bool ReadTree();

public:
    bool Init( const char* fname );
    ~ZipFile();

    const string& GetPackName() { return fileName; }
    bool          IsFilePresent( const char* fname );
    uchar*        OpenFile( const char* fname, uint& len );
    void          GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result );
    void          GetTime( uint64* create, uint64* access, uint64* write );
};

/************************************************************************/
/* Manage                                                               */
/************************************************************************/

DataFile* OpenDataFile( const char* fname )
{
    if( !fname || !fname[ 0 ] )
    {
        WriteLogF( _FUNC_, " - Invalid file name, empty or nullptr.\n" );
        return NULL;
    }

    const char* ext = Str::Substring( fname, "." );
    if( !ext )
    {
        WriteLogF( _FUNC_, " - File<%s> extension not found.\n", fname );
        return NULL;
    }

    const char* ext_ = ext;
    while( ( ext_ = Str::Substring( ext_ + 1, "." ) ) )
        ext = ext_;

    if( Str::CompareCase( ext, ".dat" ) ) // Try open DAT
    {
        FalloutDatFile* dat = new FalloutDatFile();
        if( !dat || !dat->Init( fname ) )
        {
            WriteLogF( _FUNC_, " - Unable to open DAT file<%s>.\n", fname );
            if( dat )
                delete dat;
            return NULL;
        }
        return dat;
    }
    else if( Str::CompareCase( ext, ".zip" ) || Str::CompareCase( ext, ".bos" ) ) // Try open ZIP, BOS
    {
        ZipFile* zip = new ZipFile();
        if( !zip || !zip->Init( fname ) )
        {
            WriteLogF( _FUNC_, " - Unable to open ZIP file<%s>.\n", fname );
            if( zip )
                delete zip;
            return NULL;
        }
        return zip;
    }
    else     // Bad file format
    {
        WriteLogF( _FUNC_, " - Invalid file format<%s>. Supported only DAT and ZIP.\n", fname );
    }

    return NULL;
}

/************************************************************************/
/* Dat file                                                             */
/************************************************************************/

bool FalloutDatFile::Init( const char* fname )
{
    datHandle = NULL;
    memTree = NULL;
    fileName = fname;

    datHandle = FileOpen( fname, false );
    if( !datHandle )
    {
        WriteLogF( _FUNC_, " - Cannot open file.\n" );
        return false;
    }

    FileGetTime( datHandle, timeCreate, timeAccess, timeWrite );

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
        datHandle = NULL;
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
        if( ( memTree = new uchar[ tree_size ] ) == NULL )
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
    if( ( memTree = new uchar[ tree_size ] ) == NULL )
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
            Str::Lower( name );
            filesTree.insert( PAIR( string( name ), ptr + 4 + fnsz ) );
        }

        if( ptr + fnsz + 17 >= end_ptr )
            break;
        ptr += fnsz + 17;
    }

    return true;
}

bool FalloutDatFile::IsFilePresent( const char* fname )
{
    if( !datHandle )
        return false;

    return filesTree.find( fname ) != filesTree.end();
}

uchar* FalloutDatFile::OpenFile( const char* fname, uint& len )
{
    if( !datHandle )
        return NULL;

    auto it = filesTree.find( fname );
    if( it == filesTree.end() )
        return NULL;

    uchar* ptr = ( *it ).second;
    uchar  type = *ptr;
    uint   real_size = *(uint*) ( ptr + 1 );
    uint   packed_size = *(uint*) ( ptr + 5 );
    uint   offset = *(uint*) ( ptr + 9 );

    if( !FileSetPointer( datHandle, offset, SEEK_SET ) )
        return NULL;

    len = real_size;
    uchar* buf = new uchar[ len ];

    if( !type )
    {
        // Plane data
        if( !FileRead( datHandle, buf, len ) )
        {
            delete[] buf;
            return NULL;
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
            return NULL;
        }

        stream.next_out = buf;
        stream.avail_out = real_size;

        int left = packed_size;
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
                    return NULL;
                }
                stream.avail_in = rb;
                left -= rb;
            }
            int r = inflate( &stream, Z_NO_FLUSH );
            if( r != Z_OK && r != Z_STREAM_END )
            {
                delete[] buf;
                return NULL;
            }
            if( r == Z_STREAM_END )
                break;
        }

        inflateEnd( &stream );
    }

    return buf;
}

void FalloutDatFile::GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    size_t path_len = Str::Length( path );
    for( auto it = filesTree.begin(), end = filesTree.end(); it != end; ++it )
    {
        const string& fname = ( *it ).first;
        if( !fname.compare( 0, path_len, path ) && ( include_subdirs || fname.find_last_of( '\\' ) < path_len ) )
        {
            if( ext && *ext )
            {
                size_t pos = fname.find_last_of( '.' );
                if( pos != string::npos && Str::CompareCase( &fname.c_str()[ pos + 1 ], ext ) )
                    result.push_back( fname );
            }
            else
            {
                result.push_back( fname );
            }
        }
    }
}

void FalloutDatFile::GetTime( uint64* create, uint64* access, uint64* write )
{
    if( create )
        *create = timeCreate;
    if( access )
        *access = timeAccess;
    if( write )
        *write = timeWrite;
}

/************************************************************************/
/* Zip file                                                             */
/************************************************************************/

bool ZipFile::Init( const char* fname )
{
    fileName = fname;
    zipHandle = NULL;

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

    FileGetTime( file, timeCreate, timeAccess, timeWrite );

    FileClose( file );

    #ifdef FO_WINDOWS
    wchar_t path_wc[ MAX_FOPATH ];
    if( MultiByteToWideChar( CP_UTF8, 0, path, -1, path_wc, MAX_FOPATH ) == 0 ||
        WideCharToMultiByte( GetACP(), 0, path_wc, -1, path, MAX_FOPATH, NULL, NULL ) == 0 )
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
        zipHandle = NULL;
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
        if( unzGetCurrentFileInfo( zipHandle, &info, name, MAX_FOPATH, NULL, 0, NULL, 0 ) != UNZ_OK )
            return false;

        if( !( info.external_fa & 0x10 ) )   // Not folder
        {
            Str::Lower( name );
            for( char* str = name; *str; str++ )
                if( *str == '/' )
                    *str = '\\';
            zip_info.Pos = pos;
            zip_info.UncompressedSize = (int) info.uncompressed_size;
            filesTree.insert( PAIR( string( name ), zip_info ) );
        }

        if( i + 1 < gi.number_entry && unzGoToNextFile( zipHandle ) != UNZ_OK )
            return false;
    }

    return true;
}

bool ZipFile::IsFilePresent( const char* fname )
{
    if( !zipHandle )
        return false;

    return filesTree.find( fname ) != filesTree.end();
}

uchar* ZipFile::OpenFile( const char* fname, uint& len )
{
    if( !zipHandle )
        return NULL;

    auto it = filesTree.find( fname );
    if( it == filesTree.end() )
        return NULL;

    ZipFileInfo& info = ( *it ).second;

    if( unzGoToFilePos( zipHandle, &info.Pos ) != UNZ_OK )
        return NULL;

    uchar* buf = new uchar[ info.UncompressedSize + 1 ];
    if( !buf )
        return NULL;

    if( unzOpenCurrentFile( zipHandle ) != UNZ_OK )
    {
        delete[] buf;
        return NULL;
    }

    int read = unzReadCurrentFile( zipHandle, buf, info.UncompressedSize );
    if( unzCloseCurrentFile( zipHandle ) != UNZ_OK || read != info.UncompressedSize )
    {
        delete[] buf;
        return NULL;
    }

    len = info.UncompressedSize;
    buf[ len ] = 0;
    return buf;
}

void ZipFile::GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result )
{
    size_t path_len = Str::Length( path );
    for( auto it = filesTree.begin(), end = filesTree.end(); it != end; ++it )
    {
        const string& fname = ( *it ).first;
        if( !fname.compare( 0, path_len, path ) && ( include_subdirs || fname.find_last_of( '\\' ) < path_len ) )
        {
            if( ext && *ext )
            {
                size_t pos = fname.find_last_of( '.' );
                if( pos != string::npos && Str::CompareCase( &fname.c_str()[ pos + 1 ], ext ) )
                    result.push_back( fname );
            }
            else
            {
                result.push_back( fname );
            }
        }
    }
}

void ZipFile::GetTime( uint64* create, uint64* access, uint64* write )
{
    if( create )
        *create = timeCreate;
    if( access )
        *access = timeAccess;
    if( write )
        *write = timeWrite;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
