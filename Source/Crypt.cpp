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

uint CryptManager::Crc32( const string& data )
{
    const uint CRC_MASK = 0xD202EF8D;
    uint       value = 0;
    for( char ch : data )
    {
        value = crcTable[ (uchar) value ^ (uchar) ch ] ^ value >> 8;
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

string CryptManager::ClientPassHash( const string& name, const string& pass )
{
    char*  bld = new char[ MAX_NAME + 1 ];
    memzero( bld, MAX_NAME + 1 );
    size_t pass_len = pass.length();
    size_t name_len = name.length();
    if( pass_len > MAX_NAME )
        pass_len = MAX_NAME;
    memcpy( bld, pass.c_str(), pass_len );
    if( pass_len < MAX_NAME )
        bld[ pass_len++ ] = '*';
    for( ; name_len && pass_len < MAX_NAME; pass_len++ )
        bld[ pass_len ] = name[ pass_len % name_len ];

    char* pass_hash = new char[ MAX_NAME + 1 ];
    memzero( pass_hash, MAX_NAME + 1 );
    sha256( (const uchar*) bld, MAX_NAME, (uchar*) pass_hash );
    string result = pass_hash;
    delete[] bld;
    delete[] pass_hash;
    return result;
}

uchar* CryptManager::Compress( const uchar* data, uint& data_len )
{
    uLongf              buf_len = data_len * 110 / 100 + 12;
    AutoPtrArr< uchar > buf( new uchar[ buf_len ] );

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
        }
        else if( result != Z_OK )
        {
            WriteLog( "Unpack error {}.\n", result );
            return nullptr;
        }
        else
        {
            break;
        }
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

static string MakeCachePath( const string& data_name )
{
    return FileManager::GetWritePath( "Cache/" + _str( data_name ).replace( '/', '_' ).replace( '\\', '_' ) );
}

bool CryptManager::IsCache( const string& data_name )
{
    string path = MakeCachePath( data_name );
    void*  f = FileOpen( path, false );
    bool   exists = ( f != nullptr );
    FileClose( f );
    return exists;
}

void CryptManager::EraseCache( const string& data_name )
{
    string path = MakeCachePath( data_name );
    FileDelete( path );
}

void CryptManager::SetCache( const string& data_name, const uchar* data, uint data_len )
{
    string path = MakeCachePath( data_name );
    void*  f = FileOpen( path, true );
    if( !f )
    {
        WriteLog( "Can't open write cache at '{}'.\n", path );
        return;
    }

    if( !FileWrite( f, data, data_len ) )
    {
        WriteLog( "Can't write cache to '{}'.\n", path );
        FileClose( f );
        FileDelete( path );
        return;
    }

    FileClose( f );
}

void CryptManager::SetCache( const string& data_name, const string& str )
{
    SetCache( data_name, !str.empty() ? (uchar*) str.c_str() : (uchar*) "", (uint) str.length() );
}

void CryptManager::SetCache( const string& data_name, UCharVec& data )
{
    SetCache( data_name, !data.empty() ? (uchar*) &data[ 0 ] : (uchar*) "", (uint) data.size() );
}

uchar* CryptManager::GetCache( const string& data_name, uint& data_len )
{
    string path = MakeCachePath( data_name );
    void*  f = FileOpen( path, false );
    if( !f )
        return nullptr;

    data_len = FileGetSize( f );
    uchar* data = new uchar[ data_len ];

    if( !FileRead( f, data, data_len ) )
    {
        WriteLog( "Can't read cache from '{}'.\n", path );
        SAFEDELA( data );
        data_len = 0;
        FileClose( f );
        return nullptr;
    }

    FileClose( f );
    return data;
}

string CryptManager::GetCache( const string& data_name )
{
    string str;
    uint   result_len;
    uchar* result = GetCache( data_name, result_len );
    if( !result )
        return str;

    if( result_len )
    {
        str.resize( result_len );
        memcpy( &str[ 0 ], result, result_len );
    }
    SAFEDELA( result );
    return str;
}

bool CryptManager::GetCache( const string& data_name, UCharVec& data )
{
    data.clear();
    uint   result_len;
    uchar* result = GetCache( data_name, result_len );
    if( !result )
        return false;

    data.resize( result_len );
    if( result_len )
        memcpy( &data[ 0 ], result, result_len );
    SAFEDELA( result );
    return true;
}
