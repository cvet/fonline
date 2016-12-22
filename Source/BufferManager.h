#ifndef __BUFFER_MANAGER__
#define __BUFFER_MANAGER__

#include "Common.h"

class BufferManager
{
public:
    static const uint DefaultBufSize = 4096;
    static const int  CryptKeysCount = 50;

private:
    bool   isError;
    #ifndef NO_THREADING
    Mutex  bufLocker;
    #endif
    uchar* bufData;
    uint   bufLen;
    uint   bufEndPos;
    uint   bufReadPos;
    bool   encryptActive;
    int    encryptKeyPos;
    uint   encryptKeys[ CryptKeysCount ];

    uint EncryptKey( int move );
    void CopyBuf( const uchar* from, uchar* to, uint crypt_key, uint len );

public:
    BufferManager();
    ~BufferManager();

    void   SetEncryptKey( uint seed );
    void   Lock();
    void   Unlock();
    void   Refresh();
    void   Reset();
    void   LockReset();
    void   Push( const void* buf, uint len, bool no_crypt = false );
    void   Pop( void* buf, uint len );
    void   Cut( uint len );
    void   GrowBuf( uint len );
    uchar* GetData()             { return bufData; }
    uchar* GetCurData()          { return bufData + bufReadPos; }
    uint   GetLen()              { return bufLen; }
    uint   GetCurPos()           { return bufReadPos; }
    void   SetEndPos( uint pos ) { bufEndPos = pos; }
    uint   GetEndPos()           { return bufEndPos; }
    void   MoveReadPos( int val );
    bool   IsError()               { return isError; }
    void   SetError()              { isError = true; }
    bool   IsEmpty()               { return bufReadPos >= bufEndPos; }
    bool   IsHaveSize( uint size ) { return bufReadPos + size <= bufEndPos; }
    bool   NeedProcess();
    void   SkipMsg( uint msg );

    template< typename T >
    BufferManager& operator<<( const T& i )
    {
        Push( &i, sizeof( T ) );
        return *this;
    }

    template< typename T >
    BufferManager& operator>>( T& i )
    {
        Pop( &i, sizeof( T ) );
        return *this;
    }

    // Disable copying
    BufferManager( const BufferManager& other ) = delete;
    BufferManager& operator=( const BufferManager& other ) = delete;

    // Disable transferring of some types
    BufferManager& operator<<( const uint64& i ) = delete;
    BufferManager& operator>>( uint64& i ) = delete;
    BufferManager& operator<<( const float& i ) = delete;
    BufferManager& operator>>( float& i ) = delete;
    BufferManager& operator<<( const double& i ) = delete;
    BufferManager& operator>>( double& i ) = delete;
};

#endif // __BUFFER_MANAGER__
