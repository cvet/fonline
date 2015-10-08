#ifndef __CRYPT__
#define __CRYPT__

#include "Log.h"

class CryptManager
{
private:
    // Crc32 table
    uint crcTable[ 0x100 ];

public:
    // Init Crypt Manager
    CryptManager();

    // Returns Crc32 of data
    uint Crc32( const char* data );
    uint Crc32( const uchar* data, uint len );
    void Crc32( const uchar* data, uint len, uint& crc );

    // Returns hash
    uint MurmurHash2( const uchar* data, uint len );

    // Returns CheckSum of data
    uint CheckSum( const uchar* data, uint len );

    // Xor the data
    void XOR( char* data, uint len, char* xor_key, uint xor_len );

    // Password encrypt
    void DecryptPassword( char* data, uint len, uint key );

    // Client credentials SHA-2 hash
    void ClientPassHash( const char* name, const char* pass, char* pass_hash );

    // Xor the text
    void TextXOR( char* data, uint len, char* xor_key, uint xor_len );

    // Compress zlib
    uchar* Compress( const uchar* data, uint& data_len );
    bool   Compress( UCharVec& data );

    // Uncompress zlib
    uchar* Uncompress( const uchar* data, uint& data_len, uint mul_approx );
    bool   Uncompress( UCharVec& data, uint mul_approx );

    // Cache stuff
    bool   IsCacheTable( const char* cache_fname );
    bool   CreateCacheTable( const char* cache_fname );
    bool   SetCacheTable( const char* cache_fname );
    void   GetCacheNames( const char* start_with, StrVec& names );
    bool   IsCache( const char* name );
    void   EraseCache( const char* name );
    void   SetCache( const char* data_name, const uchar* data, uint data_len );
    void   SetCache( const char* data_name, const string& str );
    void   SetCache( const char* data_name, UCharVec& data );
    uchar* GetCache( const char* data_name, uint& data_len );
    string GetCache( const char* data_name );
    bool   GetCache( const char* data_name, UCharVec& data );
};

extern CryptManager Crypt;

#endif // __CRYPT__
