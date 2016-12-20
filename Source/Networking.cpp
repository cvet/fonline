#include "Networking.h"

#define ASIO_STANDALONE
#include "asio.hpp"

#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_
#define _WEBSOCKETPP_CPP11_STL_
#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

static void* ZlibAlloc( void* opaque, unsigned int items, unsigned int size ) { return calloc( items, size ); }
static void  ZlibFree( void* opaque, void* address )                          { free( address ); }

NetConnection::~NetConnection() {}

class NetConnectionImpl: public NetConnection
{
    SOCKET    Sock;
    UCharVec  NetIOBuffer;
    bool      DisableZlib;
    z_stream* Zstrm;

    NetConnectionImpl( bool use_compression )
    {
        if( use_compression )
        {
            Zstrm = new z_stream();
            Zstrm->zalloc = ZlibAlloc;
            Zstrm->zfree = ZlibFree;
            Zstrm->opaque = nullptr;
            int result = deflateInit( Zstrm, Z_BEST_SPEED );
            RUNTIME_ASSERT( result == Z_OK );
        }
        else
        {
            Zstrm = nullptr;
        }
    }

    virtual ~NetConnectionImpl() override
    {
        Disconnect();
        if( Zstrm )
            deflateEnd( Zstrm );
        SAFEDEL( Zstrm );
    }

    virtual void DisableCompression() override
    {
        if( Zstrm )
            deflateEnd( Zstrm );
        SAFEDEL( Zstrm );
    }

    virtual void Dispatch() override
    {}

    virtual void Disconnect() override
    {
        // Connection->IsDisconnected = true;
        // if( !Connection->DisconnectTick )
        //    Connection->DisconnectTick = Timer::FastTick();
        // Bin.LockReset();
        // Bout.LockReset();
    }

    virtual void Shutdown() override
    {}
};

class NetTcpServer: public NetServerBase
{
    virtual void SetConnectionCallback( std::function< void(NetConnection*) > callback ) override
    {}

    virtual bool Listen( ushort port ) override
    {
        return false;
    }

    virtual void Shutdown() override
    {}
};

class NetWebSocketsServer: public NetServerBase
{
    virtual void SetConnectionCallback( std::function< void(NetConnection*) > callback ) override
    {}

    virtual bool Listen( ushort port ) override
    {
        return false;
    }

    virtual void Shutdown() override
    {}
};

/*void FOServer::NetIO_Input( Client::NetIOArg* io )
   {
    Client* cl = (Client*) io->PClient;

    if( cl->IsOffline() )
    {
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        return;
    }

    cl->Connection->Bin.Lock();
    if( cl->Connection->Bin.GetCurPos() + io->Bytes >= GameOpt.FloodSize && !Singleplayer )
    {
        WriteLog( "Flood.\n" );
        cl->Connection->Bin.Reset();
        cl->Connection->Bin.Unlock();
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        cl->Disconnect();
        return;
    }
    cl->Connection->Bin.Push( io->Buffer.buf, io->Bytes, true );
    cl->Connection->Bin.Unlock();

    io->Flags = 0;
    DWORD bytes;
    if( WSARecv( cl->Sock, &io->Buffer, 1, &bytes, &io->Flags, &io->OV, nullptr ) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
    {
        WriteLog( "Recv fail, error '{}'.\n", GetLastSocketError() );
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        cl->Disconnect();
    }
   }

   void FOServer::NetIO_Output( Client::NetIOArg* io )
   {
    Client* cl = (Client*) io->PClient;

    // Nothing to send
    cl->Connection->Bout.Lock();
    if( cl->Connection->Bout.IsEmpty() )
    {
        cl->Connection->Bout.Unlock();
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        return;
    }

    // Compress
    if( !GameOpt.DisableZlibCompression && !cl->DisableZlib )
    {
        uint to_compr = cl->Connection->Bout.GetEndPos();
        if( to_compr > WSA_BUF_SIZE )
            to_compr = WSA_BUF_SIZE;

        cl->Zstrm.next_in = (Bytef*) cl->Connection->Bout.GetCurData();
        cl->Zstrm.avail_in = to_compr;
        cl->Zstrm.next_out = (Bytef*) io->Buffer.buf;
        cl->Zstrm.avail_out = WSA_BUF_SIZE;

        if( deflate( &cl->Zstrm, Z_SYNC_FLUSH ) != Z_OK )
        {
            WriteLog( "Deflate fail.\n" );
            cl->Connection->Bout.Reset();
            cl->Connection->Bout.Unlock();
            InterlockedExchange( &io->Operation, WSAOP_FREE );
            cl->Disconnect();
            return;
        }

        uint compr = (uint) ( (size_t) cl->Zstrm.next_out - (size_t) io->Buffer.buf );
        uint real = (uint) ( (size_t) cl->Zstrm.next_in - (size_t) cl->Connection->Bout.GetCurData() );
        io->Buffer.len = compr;
        cl->Connection->Bout.Cut( real );
        Statistics.DataReal += real;
        Statistics.DataCompressed += compr;
    }
    // Without compressing
    else
    {
        uint len = cl->Connection->Bout.GetEndPos();
        if( len > WSA_BUF_SIZE )
            len = WSA_BUF_SIZE;
        memcpy( io->Buffer.buf, cl->Connection->Bout.GetCurData(), len );
        io->Buffer.len = len;
        cl->Connection->Bout.Cut( len );
        Statistics.DataReal += len;
        Statistics.DataCompressed += len;
    }
    if( cl->Connection->Bout.IsEmpty() )
        cl->Connection->Bout.Reset();
    cl->Connection->Bout.Unlock();

    // Send
    DWORD bytes;
    if( WSASend( cl->Sock, &io->Buffer, 1, &bytes, 0, (LPOVERLAPPED) io, nullptr ) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
    {
        WriteLog( "Send fail, error '{}'.\n", GetLastSocketError() );
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        cl->Disconnect();
    }
   }*/

NetServerBase* NetServerBase::CreateTcpServer()
{
    return new NetTcpServer();
}

NetServerBase* NetServerBase::CreateWebSocketsServer()
{
    return new NetWebSocketsServer();
}
