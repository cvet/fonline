#ifndef __BUFFER_MANAGER__
#define __BUFFER_MANAGER__

#include "Common.h"

#define CRYPT_KEYS_COUNT    ( 50 )

class BufferManager
{
private:
    bool isError;
    Mutex bufLocker;
    char* bufData;
    uint bufLen;
    uint bufEndPos;
    uint bufReadPos;
    bool encryptActive;
    int encryptKeyPos;
    uint encryptKeys[ CRYPT_KEYS_COUNT ];

    void CopyBuf( const char* from, char* to, const char* mask, uint crypt_key, uint len );

public:
    BufferManager();
    BufferManager( uint alen );
    BufferManager& operator=( const BufferManager& r );
    ~BufferManager();

    void  SetEncryptKey( uint seed );
    void  Lock();
    void  Unlock();
    void  Refresh();
    void  Reset();
    void  LockReset();
    void  Push( const char* buf, uint len, bool no_crypt = false );
    void  Push( const char* buf, const char* mask, uint len );
    void  Pop( char* buf, uint len );
    void  Cut( uint len );
    void  GrowBuf( uint len );
    char* GetData()             { return bufData; }
    char* GetCurData()          { return bufData + bufReadPos; }
    uint  GetLen()              { return bufLen; }
    uint  GetCurPos()           { return bufReadPos; }
    void  SetEndPos( uint pos ) { bufEndPos = pos; }
    uint  GetEndPos()           { return bufEndPos; }
    void  MoveReadPos( int val );
    bool  IsError()               { return isError; }
    void  SetError()              { isError = true; }
    bool  IsEmpty()               { return bufReadPos >= bufEndPos; }
    bool  IsHaveSize( uint size ) { return bufReadPos + size <= bufEndPos; }
    bool  NeedProcess();
    void  SkipMsg( uint msg );

    template< typename T >
    BufferManager& operator<<( const T& i )
    {
        if( isError )
            return *this;
        if( bufEndPos + sizeof( T ) >= bufLen )
            GrowBuf( sizeof( T ) );
        *(T*) ( bufData + bufEndPos ) = i ^ EncryptKey( sizeof( T ) );
        bufEndPos += sizeof( T );
        return *this;
    }

    template< typename T >
    BufferManager& operator>>( T& i )
    {
        if( isError )
            return *this;
        if( bufReadPos + sizeof( T ) > bufEndPos )
        {
            isError = true;
            WriteLog( "Buffer Manager Error!\n" );
            return *this;
        }
        i = *(T*) ( bufData + bufReadPos ) ^ EncryptKey( sizeof( T ) );
        bufReadPos += sizeof( T );
        return *this;
    }

    BufferManager& operator<<( const bool& b )
    {
        *this << (uchar) ( b ? 1 : 0 );
        return *this;
    }

    BufferManager& operator>>( bool& b )
    {
        uchar i = 0;
        *this >> i;
        b = ( i != 0 );
        return *this;
    }

    // Disable transferring some types
private:
    BufferManager& operator<<( const uint64& i );
    BufferManager& operator>>( uint64& i );
    BufferManager& operator<<( const float& i );
    BufferManager& operator>>( float& i );
    BufferManager& operator<<( const double& i );
    BufferManager& operator>>( double& i );

private:
    inline uint EncryptKey( int move )
    {
        if( !encryptActive )
            return 0;
        uint key = encryptKeys[ encryptKeyPos ];
        encryptKeyPos += move;
        if( encryptKeyPos < 0 || encryptKeyPos >= CRYPT_KEYS_COUNT )
        {
            encryptKeyPos %= CRYPT_KEYS_COUNT;
            if( encryptKeyPos < 0 )
                encryptKeyPos += CRYPT_KEYS_COUNT;
        }
        return key;
    }
};

#endif // __BUFFER_MANAGER__
