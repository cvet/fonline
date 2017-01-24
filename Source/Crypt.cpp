#include "Crypt.h"
#include "zlib.h"
#include "SHA/sha2.h"
#include "FileSystem.h"

CryptManager Crypt;

CryptManager::CryptManager()
{
    // Create crc32 mass
    const uint CRC_POLY = 0xEDB88320;
    uint       i, j, r;
    for( i = 0; i < 0x100; i++ )
    {
        for( r = i, j = 0x8; j; j-- )
            r = ( ( r & 1 ) ? ( r >> 1 ) ^ CRC_POLY : r >> 1 );
        crcTable[ i ] = r;
    }
}

uint CryptManager::Crc32( const char* data )
{
    const uint CRC_MASK = 0xD202EF8D;
    uint       value = 0;
    while( *data )
    {
        value = crcTable[ (uchar) value ^ ( uchar ) * data++ ] ^ value >> 8;
        value ^= CRC_MASK;
    }
    return value;
}

uint CryptManager::Crc32( const uchar* data, uint len )
{
    const uint CRC_MASK = 0xD202EF8D;
    uint       value = 0;
    while( len-- )
    {
        value = crcTable[ (uchar) value ^ *data++ ] ^ value >> 8;
        value ^= CRC_MASK;
    }
    return value;
}

void CryptManager::Crc32( const uchar* data, uint len, uint& crc )
{
    const uint CRC_MASK = 0xD202EF8D;
    while( len-- )
    {
        crc = crcTable[ (uchar) crc ^ *data++ ] ^ crc >> 8;
        crc ^= CRC_MASK;
    }
}

uint CryptManager::MurmurHash2( const uchar* data, uint len )
{
    const uint m = 0x5BD1E995;
    const uint seed = 0;
    const int  r = 24;
    uint       h = seed ^ len;

    while( len >= 4 )
    {
        uint k;

        k  = data[ 0 ];
        k |= data[ 1 ] << 8;
        k |= data[ 2 ] << 16;
        k |= data[ 3 ] << 24;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch( len )
    {
    case 3:
        h ^= data[ 2 ] << 16;
    case 2:
        h ^= data[ 1 ] << 8;
    case 1:
        h ^= data[ 0 ];
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

uint CryptManager::CheckSum( const uchar* data, uint len )
{
    uint value = 0;
    while( len-- )
        value += *data++;
    return value;
}

void CryptManager::XOR( char* data, uint len, char* xor_key, uint xor_len )
{
    for( uint i = 0, k = 0; i < len; ++i, ++k )
    {
        if( k >= xor_len )
            k = 0;
        data[ i ] ^= xor_key[ k ];
    }
}

void CryptManager::TextXOR( char* data, uint len, char* xor_key, uint xor_len )
{
    char cur_char;
    for( uint i = 0, k = 0; i < len; ++i, ++k )
    {
        if( k >= xor_len )
            k = 0;

        cur_char = ( data[ i ] - ( i + 5 ) * ( i * 5 ) ) ^ xor_key[ k ];

        #define TRUECHAR( a )                    \
            ( ( ( a ) >= 32 && ( a ) <= 126 ) || \
              ( ( a ) >= -64 && ( a ) <= -1 ) )

        if( TRUECHAR( cur_char ) )
            data[ i ] = cur_char;
    }
}

void CryptManager::DecryptPassword( char* data, uint len, uint key )
{
    for( uint i = 10; i < len + 10; i++ )
        Crypt.XOR( &data[ i - 10 ], 1, (char*) &i, 1 );
    XOR( data, len, (char*) &key, sizeof( key ) );
    for( uint i = 0; i < ( len - 1 ) / 2; i++ )
        std::swap( data[ i ], data[ len - 1 - i ] );
    uint slen = data[ len - 1 ];
    for( uint i = slen; i < len; i++ )
        data[ i ] = 0;
    data[ len - 1 ] = 0;
}

char* ConvertUTF8ToCP1251( const char* str, uchar* buf, uint buf_len, bool to_lower )
{
    memzero( buf, buf_len );
    for( uint i = 0; *str; i++ )
    {
        uint length;
        uint ch = Str::DecodeUTF8( str, &length );
        if( ch >= 1072 && 1072 <= 1103 )
            buf[ i ] = 224 + ( ch - 1072 );
        else if( ch >= 1040 && 1072 <= 1071 )
            buf[ i ] = ( to_lower ? 224 : 192 ) + ( ch - 1040 );
        else if( ch == 1105 )
            buf[ i ] = 184;
        else if( ch == 1025 )
            buf[ i ] = ( to_lower ? 184 : 168 );
        else if( length >= 4 )
            buf[ i ] = *str ^ *( str + 1 ) ^ *( str + 2 ) ^ *( str + 3 );
        else if( length == 3 )
            buf[ i ] = *str ^ *( str + 1 ) ^ *( str + 2 );
        else if( length == 2 )
            buf[ i ] = *str ^ *( str + 1 );
        else
            buf[ i ] = ( to_lower ? tolower( *str ) : *str );
        if( !buf[ i ] )
            buf[ i ] = 0xAA;
        str += length;
    }
    return (char*) buf;
}

void CryptManager::ClientPassHash( const char* name, const char* pass, char* pass_hash )
{
    // Convert utf8 to cp1251, for backward compatability with older client saves
    uchar name_[ MAX_NAME + 1 ];
    name = ConvertUTF8ToCP1251( name, name_, sizeof( name_ ), true );
    uchar pass_[ MAX_NAME + 1 ];
    pass = ConvertUTF8ToCP1251( pass, pass_, sizeof( pass_ ), false );

    // Calculate hash
    char* bld = new char[ MAX_NAME + 1 ];
    memzero( bld, MAX_NAME + 1 );
    uint  pass_len = Str::Length( pass );
    uint  name_len = Str::Length( name );
    if( pass_len > MAX_NAME )
        pass_len = MAX_NAME;
    Str::Copy( bld, MAX_NAME + 1, pass );
    if( pass_len < MAX_NAME )
        bld[ pass_len++ ] = '*';
    if( name_len )
    {
        for( ; pass_len < MAX_NAME; pass_len++ )
            bld[ pass_len ] = name[ pass_len % name_len ];
    }
    sha256( (const uchar*) bld, MAX_NAME, (uchar*) pass_hash );
    delete[] bld;
}

uchar* CryptManager::Compress( const uchar* data, uint& data_len )
{
    uLongf              buf_len = data_len * 110 / 100 + 12;
    AutoPtrArr< uchar > buf( new uchar[ buf_len ] );
    if( !buf.IsValid() )
        return nullptr;

    if( compress( buf.Get(), &buf_len, data, data_len ) != Z_OK )
        return nullptr;
    XOR( (char*) buf.Get(), (uint) buf_len, (char*) &crcTable[ 1 ], sizeof( crcTable ) - 4 );
    XOR( (char*) buf.Get(), 4, (char*) buf.Get() + 4, 4 );

    data_len = (uint) buf_len;
    return buf.Release();
}

bool CryptManager::Compress( UCharVec& data )
{
    uint   result_len = (uint) data.size();
    uchar* result = Compress( &data[ 0 ], result_len );
    if( !result )
        return false;
    data.resize( result_len );
    memcpy( &data[ 0 ], result, result_len );
    SAFEDELA( result );
    return true;
}

uchar* CryptManager::Uncompress( const uchar* data, uint& data_len, uint mul_approx )
{
    uLongf buf_len = data_len * mul_approx;
    if( buf_len > 100000000 ) // 100mb
    {
        WriteLog( "Unpack buffer length is too large, data length {}, multiplier {}.\n", data_len, mul_approx );
        return nullptr;
    }

    AutoPtrArr< uchar > buf( new uchar[ buf_len ] );
    AutoPtrArr< uchar > data_( new uchar[ data_len ] );

    memcpy( data_.Get(), data, data_len );
    if( *(ushort*) data_.Get() != 0x9C78 )
    {
        XOR( (char*) data_.Get(), 4, (char*) data_.Get() + 4, 4 );
        XOR( (char*) data_.Get(), data_len, (char*) &crcTable[ 1 ], sizeof( crcTable ) - 4 );
    }

    if( *(ushort*) data_.Get() != 0x9C78 )
    {
        WriteLog( "Unpack signature not found.\n" );
        return nullptr;
    }

    while( true )
    {
        int result = uncompress( buf.Get(), &buf_len, data_.Get(), data_len );
        if( result == Z_BUF_ERROR )
        {
            buf_len *= 2;
            buf.Reset( new uchar[ buf_len ] );
            if( !buf.IsValid() )
            {
                WriteLog( "Unpack bad alloc, size {}.\n", buf_len );
                return nullptr;
            }
        }
        else if( result != Z_OK )
        {
            WriteLog( "Unpack error {}.\n", result );
            return nullptr;
        }
        else
            break;
    }

    data_len = (uint) buf_len;
    return buf.Release();
}

bool CryptManager::Uncompress( UCharVec& data, uint mul_approx )
{
    uint   result_len = (uint) data.size();
    uchar* result = Uncompress( &data[ 0 ], result_len, mul_approx );
    if( !result )
        return false;
    data.resize( result_len );
    memcpy( &data[ 0 ], result, result_len );
    SAFEDELA( result );
    return true;
}

#define MAX_CACHE_DESCRIPTORS    ( 10001 )
#define CACHE_DATA_VALID         ( 0x08 )
#define CACHE_SIZE_VALID         ( 0x10 )

struct CacheDescriptor
{
    uint   Rnd2[ 2 ];
    uchar  Flags;
    char   Rnd0;
    ushort Rnd1;
    uint   DataOffset;
    uint   DataCurLen;
    uint   DataMaxLen;
    uint   Rnd3;
    uint   DataCrc;
    uint   Rnd4;
    char   DataName[ 32 ];
    uint   TableSize;
    uint   XorKey[ 5 ];
    uint   Crc;
} CacheTable[ MAX_CACHE_DESCRIPTORS ];

void* CacheTableFile = nullptr;

bool CryptManager::IsCacheTable( const char* cache_fname )
{
    if( !cache_fname || !cache_fname[ 0 ] )
        return false;
    void* f = FileOpen( cache_fname, false );
    if( !f )
        return false;
    FileClose( f );
    return true;
}

bool CryptManager::CreateCacheTable( const char* cache_fname )
{
    if( CacheTableFile )
    {
        FileClose( CacheTableFile );
        CacheTableFile = nullptr;
    }

    void* f = FileOpen( cache_fname, true );
    if( !f )
        return false;

    for( uint i = 0; i < sizeof( CacheTable ); i++ )
        ( (uchar*) &CacheTable[ 0 ] )[ i ] = Random( 0, 255 );
    for( uint i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
        UNSETFLAG( desc.Flags, CACHE_SIZE_VALID );
        desc.TableSize = MAX_CACHE_DESCRIPTORS;
        CacheDescriptor desc_ = desc;
        XOR( (char*) &desc_, sizeof( CacheDescriptor ) - 24, (char*) &desc_.XorKey[ 0 ], 20 );
        FileWrite( f, (void*) &desc_, sizeof( CacheDescriptor ) );
    }

    FileClose( f );
    CacheTableFile = FileOpenForReadWrite( cache_fname, true );
    return CacheTableFile != nullptr;
}

bool CryptManager::SetCacheTable( const char* cache_fname )
{
    if( !cache_fname || !cache_fname[ 0 ] )
        return false;

    if( CacheTableFile )
    {
        FileClose( CacheTableFile );
        CacheTableFile = nullptr;
    }

    void* f = FileOpenForReadWrite( cache_fname, true );
    if( !f )
        return CreateCacheTable( cache_fname );

    uint len = FileGetSize( f );
    if( len < sizeof( CacheTable ) )
    {
        FileClose( f );
        return CreateCacheTable( cache_fname );
    }

    FileRead( f, (void*) &CacheTable[ 0 ], sizeof( CacheTable ) );

    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        XOR( (char*) &desc, sizeof( CacheDescriptor ) - 24, (char*) &desc.XorKey[ 0 ], 20 );
        if( desc.TableSize != MAX_CACHE_DESCRIPTORS )
        {
            FileClose( f );
            return CreateCacheTable( cache_fname );
        }
    }

    CacheTableFile = f;
    return true;
}

void CryptManager::GetCacheNames( const char* start_with, StrVec& names )
{
    uint start_with_len = ( start_with ? Str::Length( start_with ) : 0 );
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( !FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( start_with && !Str::CompareCount( desc.DataName, start_with, start_with_len ) )
            continue;
        names.push_back( desc.DataName );
    }
}

bool CryptManager::IsCache( const char* name )
{
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( !FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( Str::Compare( desc.DataName, name ) )
            return true;
    }
    return false;
}

void CryptManager::EraseCache( const char* name )
{
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( !FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( Str::Compare( desc.DataName, name ) )
        {
            UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
            CacheDescriptor desc_ = desc;
            XOR( (char*) &desc_, sizeof( CacheDescriptor ) - 24, (char*) &desc_.XorKey[ 0 ], 20 );
            FileSetPointer( CacheTableFile, i * sizeof( CacheDescriptor ), SEEK_SET );
            FileWrite( CacheTableFile, (void*) &desc_, sizeof( CacheDescriptor ) );
            break;
        }
    }
}

void CryptManager::SetCache( const char* data_name, const uchar* data, uint data_len )
{
    // Fix path
    char data_name_[ MAX_FOPATH ];
    Str::Copy( data_name_, data_name );
    NormalizePathSlashes( data_name_ );

    // Find data
    CacheDescriptor desc_;
    int             desc_place = -1;

    // In prev place
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( !FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( !Str::Compare( data_name_, desc.DataName ) )
            continue;

        if( data_len > desc.DataMaxLen )
        {
            UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
            CacheDescriptor desc__ = desc;
            XOR( (char*) &desc__, sizeof( CacheDescriptor ) - 24, (char*) &desc__.XorKey[ 0 ], 20 );
            FileSetPointer( CacheTableFile, i * sizeof( CacheDescriptor ), SEEK_SET );
            FileWrite( CacheTableFile, (void*) &desc__, sizeof( CacheDescriptor ) );
            break;
        }

        desc.DataCurLen = data_len;
        desc_ = desc;
        desc_place = i;
        goto label_PlaceFound;
    }

    // Valid size
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( !FLAG( desc.Flags, CACHE_SIZE_VALID ) )
            continue;
        if( data_len > desc.DataMaxLen )
            continue;

        SETFLAG( desc.Flags, CACHE_DATA_VALID );
        desc.DataCurLen = data_len;
        Str::Copy( desc.DataName, data_name_ );
        desc_ = desc;
        desc_place = i;
        goto label_PlaceFound;
    }

    // Add new descriptor
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( FLAG( desc.Flags, CACHE_SIZE_VALID ) )
            continue;

        FileSetPointer( CacheTableFile, 0, SEEK_END );
        int offset = (int) ( FileGetPointer( CacheTableFile ) - sizeof( CacheTable ) );
        if( offset < 0 )
            return;

        uint max_len = data_len * 2;
        if( max_len < 128 )
        {
            max_len = 1024;
            FileWrite( CacheTableFile, (void*) &crcTable[ 1 ], 512 );
            FileWrite( CacheTableFile, (void*) &crcTable[ 1 ], 512 );
        }
        else
        {
            FileWrite( CacheTableFile, data, data_len );
            FileWrite( CacheTableFile, data, data_len );
        }

        SETFLAG( desc.Flags, CACHE_DATA_VALID );
        SETFLAG( desc.Flags, CACHE_SIZE_VALID );
        desc.DataOffset = offset;
        desc.DataCurLen = data_len;
        desc.DataMaxLen = max_len;
        Str::Copy( desc.DataName, data_name_ );
        desc_ = desc;
        desc_place = i;
        goto label_PlaceFound;
    }

    // Grow table
    #pragma MESSAGE("Grow cache descriptors table.")
    WriteLog( "Cache table descriptors ended! Delete 'default.cache' file." );
    return;

    // Place founded
label_PlaceFound:
    CacheDescriptor desc__ = desc_;
    XOR( (char*) &desc__, sizeof( CacheDescriptor ) - 24, (char*) &desc__.XorKey[ 0 ], 20 );
    FileSetPointer( CacheTableFile, sizeof( CacheTable ) + desc_.DataOffset, SEEK_SET );
    XOR( (char*) data, data_len, (char*) &desc__.XorKey[ 0 ], 20 );
    FileWrite( CacheTableFile, data, data_len );
    XOR( (char*) data, data_len, (char*) &desc__.XorKey[ 0 ], 20 );
    FileSetPointer( CacheTableFile, desc_place * sizeof( CacheDescriptor ), SEEK_SET );
    FileWrite( CacheTableFile, (void*) &desc__, sizeof( CacheDescriptor ) );
}

void CryptManager::SetCache( const char* data_name, const string& str )
{
    SetCache( data_name, (uchar*) str.c_str(), (uint) str.length() );
}

void CryptManager::SetCache( const char* data_name, UCharVec& data )
{
    SetCache( data_name, (uchar*) &data[ 0 ], (uint) data.size() );
}

uchar* CryptManager::GetCache( const char* data_name, uint& data_len )
{
    // Fix path
    char data_name_[ MAX_FOPATH ];
    Str::Copy( data_name_, data_name );
    NormalizePathSlashes( data_name_ );

    // For each descriptors
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( !FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( !Str::Compare( data_name_, desc.DataName ) )
            continue;

        if( desc.DataCurLen > 0xFFFFFF )
        {
            UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
            UNSETFLAG( desc.Flags, CACHE_SIZE_VALID );
            return nullptr;
        }

        uint file_len = FileGetSize( CacheTableFile );
        if( file_len < sizeof( CacheTable ) + desc.DataOffset + desc.DataCurLen )
        {
            UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
            UNSETFLAG( desc.Flags, CACHE_SIZE_VALID );
            return nullptr;
        }

        uchar* data = new uchar[ desc.DataCurLen ];
        if( !data )
            return nullptr;

        data_len = desc.DataCurLen;
        FileSetPointer( CacheTableFile, sizeof( CacheTable ) + desc.DataOffset, SEEK_SET );
        FileRead( CacheTableFile, data, desc.DataCurLen );
        XOR( (char*) data, data_len, (char*) &desc.XorKey[ 0 ], 20 );
        return data;
    }
    return nullptr;
}

string CryptManager::GetCache( const char* data_name )
{
    string str;
    uint   result_len;
    uchar* result = GetCache( data_name, result_len );
    if( result && result_len )
    {
        str.resize( result_len );
        memcpy( &str[ 0 ], result, result_len );
    }
    SAFEDELA( result );
    return str;
}

bool CryptManager::GetCache( const char* data_name, UCharVec& data )
{
    data.clear();
    uint   result_len;
    uchar* result = GetCache( data_name, result_len );
    if( !result )
        return false;
    data.resize( result_len );
    memcpy( &data[ 0 ], result, result_len );
    SAFEDELA( result );
    return true;
}
