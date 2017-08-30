#ifndef __CRYPT__
#define __CRYPT__

#include "Common.h"

class CryptManager
{
public:
    uint   MurmurHash2( const uchar* data, uint len );
    void   XOR( uchar* data, uint len, const uchar* xor_key, uint xor_len );
    string ClientPassHash( const string& name, const string& pass );

    // Compressor
    uchar* Compress( const uchar* data, uint& data_len );
    bool   Compress( UCharVec& data );
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
