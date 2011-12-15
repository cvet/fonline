#include "StdAfx.h"
#include "Crypt.h"
#include "Zlib/zlib.h"
#include "SHA2/sha2.h"

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

uint CryptManager::Crc32( uchar* data, uint len )
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

void CryptManager::Crc32( uchar* data, uint len, uint& crc )
{
    const uint CRC_MASK = 0xD202EF8D;
    while( len-- )
    {
        crc = crcTable[ (uchar) crc ^ *data++ ] ^ crc >> 8;
        crc ^= CRC_MASK;
    }
}

uint CryptManager::CheckSum( uchar* data, uint len )
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

        //	WriteLog("%c",cur_char);

        if( TRUECHAR( cur_char ) )
            data[ i ] = cur_char;
        //	if(cur_char) data[i]=cur_char;
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

void CryptManager::ClientPassHash( const char* name, const char* pass, char* pass_hash )
{
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
            bld[ pass_len ] = tolower( name[ pass_len % name_len ] );
    }
    sha256( (const uchar*) bld, MAX_NAME, (uchar*) pass_hash );
    delete[] bld;
}

uchar* CryptManager::Compress( const uchar* data, uint& data_len )
{
    uLongf              buf_len = data_len * 110 / 100 + 12;
    AutoPtrArr< uchar > buf( new uchar[ buf_len ] );
    if( !buf.IsValid() )
        return NULL;

    if( compress( buf.Get(), &buf_len, data, data_len ) != Z_OK )
        return NULL;
    XOR( (char*) buf.Get(), buf_len, (char*) &crcTable[ 1 ], sizeof( crcTable ) - 4 );
    XOR( (char*) buf.Get(), 4, (char*) buf.Get() + 4, 4 );

    data_len = buf_len;
    return buf.Release();
}

uchar* CryptManager::Uncompress( const uchar* data, uint& data_len, uint mul_approx )
{
    uLongf buf_len = data_len * mul_approx;
    if( buf_len > 100000000 ) // 100mb
    {
        WriteLogF( _FUNC_, "Unpack - Buffer length is too large, data length<%u>, multiplier<%u>.\n", data_len, mul_approx );
        return NULL;
    }

    AutoPtrArr< uchar > buf( new uchar[ buf_len ] );
    if( !buf.IsValid() )
    {
        WriteLog( "Unpack - Bad alloc, size<%u>.\n", buf_len );
        return NULL;
    }

    AutoPtrArr< uchar > data_( new uchar[ data_len ] );
    if( !data_.IsValid() )
    {
        WriteLog( "Unpack - Bad alloc, size<%u>.\n", data_len );
        return NULL;
    }

    memcpy( data_.Get(), data, data_len );
    if( *(ushort*) data_.Get() != 0x9C78 )
    {
        XOR( (char*) data_.Get(), 4, (char*) data_.Get() + 4, 4 );
        XOR( (char*) data_.Get(), data_len, (char*) &crcTable[ 1 ], sizeof( crcTable ) - 4 );
    }

    if( *(ushort*) data_.Get() != 0x9C78 )
    {
        WriteLog( "Unpack - Signature not found.\n" );
        return NULL;
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
                WriteLog( "Unpack - Bad alloc, size<%u>.\n", buf_len );
                return NULL;
            }
        }
        else if( result != Z_OK )
        {
            WriteLog( "Unpack error<%d>.\n", result );
            return NULL;
        }
        else
            break;
    }

    data_len = buf_len;
    return buf.Release();
}

/*void CryptManager::CryptText(char* text)
   {
        uint len=Str::Length(text);
        char* buf=(char*)Compress((uchar*)text,len);
        if(!buf) len=0;
        / *for(int i=0;i<len;i++)
        {
                char& c=buf[i];
                if(c==) c=;
                text[i]=c;
        }* /
        text[len]=0;
        if(buf) delete[] buf;
   }

   void CryptManager::UncryptText(char* text)
   {
        uint len=Str::Length(text);
        char* buf=(char*)Uncompress((uchar*)text,len,2);
        if(buf) memcpy(text,buf,len);
        else len=0;
        text[len]=0;
        if(buf) delete[] buf;
   }*/

#define MAX_CACHE_DESCRIPTORS    ( 10000 )
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
string CacheTableName;

bool CryptManager::IsCacheTable( const char* cache_fname )
{
    if( !cache_fname || !cache_fname[ 0 ] )
        return false;
    FILE* f = fopen( cache_fname, "rb" );
    if( !f )
        return false;
    fclose( f );
    return true;
}

bool CryptManager::CreateCacheTable( const char* cache_fname )
{
    FILE* f = fopen( cache_fname, "wb" );
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
        fwrite( (void*) &desc_, sizeof( uchar ), sizeof( CacheDescriptor ), f );
    }

    fclose( f );
    CacheTableName = cache_fname;
    return true;
}

bool CryptManager::SetCacheTable( const char* cache_fname )
{
    if( !cache_fname || !cache_fname[ 0 ] )
        return false;

    FILE* f = fopen( cache_fname, "rb" );
    if( !f )
    {
        if( CacheTableName.length() )       // default.cache
        {
            FILE* fr = fopen( CacheTableName.c_str(), "rb" );
            if( !fr )
                return CreateCacheTable( cache_fname );

            FILE* fw = fopen( cache_fname, "wb" );
            if( !fw )
            {
                fclose( fr );
                return CreateCacheTable( cache_fname );
            }

            CacheTableName = cache_fname;
            fseek( fr, 0, SEEK_END );
            uint   len = ftell( fr );
            fseek( fr, 0, SEEK_SET );
            uchar* buf = new uchar[ len ];
            if( !buf )
                return false;
            fread( buf, sizeof( uchar ), len, fr );
            fwrite( buf, sizeof( uchar ), len, fw );
            delete[] buf;
            fclose( fr );
            fclose( fw );
            return true;
        }
        else
        {
            return CreateCacheTable( cache_fname );
        }
    }

    fseek( f, 0, SEEK_END );
    uint len = ftell( f );
    fseek( f, 0, SEEK_SET );

    if( len < sizeof( CacheTable ) )
    {
        fclose( f );
        return CreateCacheTable( cache_fname );
    }

    fread( (void*) &CacheTable[ 0 ], sizeof( uchar ), sizeof( CacheTable ), f );
    fclose( f );
    CacheTableName = cache_fname;
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        XOR( (char*) &desc, sizeof( CacheDescriptor ) - 24, (char*) &desc.XorKey[ 0 ], 20 );
        if( desc.TableSize != MAX_CACHE_DESCRIPTORS )
            return CreateCacheTable( cache_fname );
    }
    return true;
}

void CryptManager::SetCache( const char* data_name, const uchar* data, uint data_len )
{
    // Load table
    FILE* f = fopen( CacheTableName.c_str(), "r+b" );
    if( !f )
        return;

    // Fix path
    char data_name_[ MAX_FOPATH ];
    Str::Copy( data_name_, data_name );
    for( uint i = 0; data_name_[ i ]; i++ )
        if( data_name_[ i ] == '\\' )
            data_name_[ i ] = '/';

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
            fseek( f, i * sizeof( CacheDescriptor ), SEEK_SET );
            fwrite( (void*) &desc__, sizeof( uchar ), sizeof( CacheDescriptor ), f );
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

    // Grow
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( FLAG( desc.Flags, CACHE_SIZE_VALID ) )
            continue;

        fseek( f, 0, SEEK_END );
        int offset = ftell( f ) - sizeof( CacheTable );
        if( offset < 0 )
        {
            fclose( f );
            return;
        }

        uint max_len = data_len * 2;
        if( max_len < 128 )
        {
            max_len = 128;
            fwrite( (void*) &crcTable[ 1 ], sizeof( uchar ), max_len, f );
        }
        else
        {
            fwrite( data, sizeof( uchar ), data_len, f );
            fwrite( data, sizeof( uchar ), data_len, f );
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

    fclose( f );
    return;
label_PlaceFound:

    CacheDescriptor desc__ = desc_;
    XOR( (char*) &desc__, sizeof( CacheDescriptor ) - 24, (char*) &desc__.XorKey[ 0 ], 20 );
    fseek( f, desc_place * sizeof( CacheDescriptor ), SEEK_SET );
    fwrite( (void*) &desc__, sizeof( uchar ), sizeof( CacheDescriptor ), f );
    fseek( f, sizeof( CacheTable ) + desc_.DataOffset, SEEK_SET );
    fwrite( data, sizeof( uchar ), data_len, f );
    fclose( f );
}

uchar* CryptManager::GetCache( const char* data_name, uint& data_len )
{
    // Fix path
    char data_name_[ MAX_FOPATH ];
    Str::Copy( data_name_, data_name );
    for( uint i = 0; data_name_[ i ]; i++ )
        if( data_name_[ i ] == '\\' )
            data_name_[ i ] = '/';

    // Foeach descriptors
    for( int i = 0; i < MAX_CACHE_DESCRIPTORS; i++ )
    {
        CacheDescriptor& desc = CacheTable[ i ];
        if( !FLAG( desc.Flags, CACHE_DATA_VALID ) )
            continue;
        if( !Str::Compare( data_name_, desc.DataName ) )
            continue;

        FILE* f = fopen( CacheTableName.c_str(), "rb" );
        if( !f )
            return NULL;

        if( desc.DataCurLen > 0xFFFFFF )
        {
            fclose( f );
            UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
            UNSETFLAG( desc.Flags, CACHE_SIZE_VALID );
            return NULL;
        }

        fseek( f, 0, SEEK_END );
        uint file_len = ftell( f ) + 1;
        fseek( f, 0, SEEK_SET );

        if( file_len < sizeof( CacheTable ) + desc.DataOffset + desc.DataCurLen )
        {
            fclose( f );
            UNSETFLAG( desc.Flags, CACHE_DATA_VALID );
            UNSETFLAG( desc.Flags, CACHE_SIZE_VALID );
            return NULL;
        }

        uchar* data = new uchar[ desc.DataCurLen ];
        if( !data )
        {
            fclose( f );
            return NULL;
        }

        data_len = desc.DataCurLen;
        fseek( f, sizeof( CacheTable ) + desc.DataOffset, SEEK_SET );
        fread( data, sizeof( uchar ), desc.DataCurLen, f );
        fclose( f );
        return data;
    }
    return NULL;
}
