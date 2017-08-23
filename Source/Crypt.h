#ifndef __CRYPT__
#define __CRYPT__

#include "Common.h"

class CryptManager
{
private:
    // Crc32 table
    uint crcTable[ 0x100 ];

public:
    // Init Crypt Manager
    CryptManager();

    // Returns Crc32 of data
    uint Crc32( const string& data );
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
    string ClientPassHash( const string& name, const string& pass );

    // Xor the text
    void TextXOR( char* data, uint len, char* xor_key, uint xor_len );

    // Compress zlib
    uchar* Compress( const uchar* data, uint& data_len );
    bool   Compress( UCharVec& data );

    // Uncompress zlib
    uchar* Uncompress( const uchar* data, uint& data_len, uint mul_approx );
    bool   Uncompress( UCharVec& data, uint mul_approx );

    // Cache stuff
    bool   IsCache( const string& data_name );
    void   EraseCache( const string& data_name );
    void   SetCache( const string& data_name, const uchar* data, uint data_len );
    void   SetCache( const string& data_name, const string& str );
    void   SetCache( const string& data_name, UCharVec& data );
    uchar* GetCache( const string& data_name, uint& data_len );
    string GetCache( const string& data_name );
    bool   GetCache( const string& data_name, UCharVec& data );
};

extern CryptManager Crypt;

#endif // __CRYPT__
