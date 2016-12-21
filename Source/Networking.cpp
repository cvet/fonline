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
typedef websocketpp::server< websocketpp::config::asio > web_sockets;

static void* ZlibAlloc( void* opaque, unsigned int items, unsigned int size ) { return calloc( items, size ); }
static void  ZlibFree( void* opaque, void* address )                          { free( address ); }

NetConnection::~NetConnection() {}

class NetConnectionImpl: public NetConnection
{
protected:
    static const uint BufSize = 4096;

    z_stream*         zStream;
    uchar             outBuf[ BufSize ];

public:
    NetConnectionImpl()
    {
        IsDisconnected = false;
        DisconnectTick = 0;

        if( !GameOpt.DisableZlibCompression )
        {
            zStream = new z_stream();
            zStream->zalloc = ZlibAlloc;
            zStream->zfree = ZlibFree;
            zStream->opaque = nullptr;
            int result = deflateInit( zStream, Z_BEST_SPEED );
            RUNTIME_ASSERT( result == Z_OK );
        }
    }

    virtual ~NetConnectionImpl() override
    {
        Dispatch();
        Disconnect();

        if( zStream )
            deflateEnd( zStream );
        SAFEDEL( zStream );
    }

    virtual void DisableCompression() override
    {
        if( zStream )
            deflateEnd( zStream );
        SAFEDEL( zStream );
    }

    virtual void Dispatch() override
    {
        if( IsDisconnected )
            return;

        // Nothing to send
        Bout.Lock();
        if( Bout.IsEmpty() )
        {
            Bout.Unlock();
            return;
        }

        // Compress
        uint out_len = 0;
        if( zStream )
        {
            uint to_compr = Bout.GetEndPos();
            if( to_compr > BufSize )
                to_compr = BufSize;

            zStream->next_in = (Bytef*) Bout.GetCurData();
            zStream->avail_in = to_compr;
            zStream->next_out = (Bytef*) outBuf;
            zStream->avail_out = BufSize;

            int result = deflate( zStream, Z_SYNC_FLUSH );
            RUNTIME_ASSERT( result == Z_OK );

            uint compr = (uint) ( (size_t) zStream->next_out - (size_t) outBuf );
            uint real = (uint) ( (size_t) zStream->next_in - (size_t) Bout.GetCurData() );
            out_len = compr;
            Bout.Cut( real );
        }
        // Without compressing
        else
        {
            uint len = Bout.GetEndPos();
            if( len > BufSize )
                len = BufSize;
            memcpy( outBuf, Bout.GetCurData(), len );
            out_len = len;
            Bout.Cut( len );
        }
        if( Bout.IsEmpty() )
            Bout.Reset();
        Bout.Unlock();

        RUNTIME_ASSERT( out_len > 0 );
        DispatchImpl( outBuf, out_len );
    }

    virtual void Disconnect() override
    {
        if( IsDisconnected )
            return;

        IsDisconnected = true;
        if( !DisconnectTick )
            DisconnectTick = Timer::FastTick();

        DisconnectImpl();
    }

protected:
    virtual void DispatchImpl( const uchar* buf, uint len ) = 0;
    virtual void DisconnectImpl() = 0;

    void ReceiveCallback( const uchar* buf, uint len )
    {
        Bin.Lock();
        if( Bin.GetCurPos() + len >= GameOpt.FloodSize && !Singleplayer )
        {
            Bin.Reset();
            Bin.Unlock();
            Disconnect();
            return;
        }
        Bin.Push( (const char*) buf, len, true );
        Bin.Unlock();
    }
};

class NetConnectionAsio: public NetConnectionImpl
{
    asio::ip::tcp::socket* socket;
    uchar                  inBuf[ BufSize ];

    void NextAsyncRead()
    {
        asio::async_read( *socket, asio::buffer( inBuf, BufSize ), asio::transfer_at_least( 1 ),
                          std::bind( &NetConnectionAsio::AsyncRead, this, std::placeholders::_1, std::placeholders::_2 ) );
    }

    void AsyncRead( std::error_code error, size_t bytes )
    {
        if( !error )
        {
            ReceiveCallback( inBuf, (uint) bytes );
            NextAsyncRead();
        }
        else
        {
            Disconnect();
        }
    }

    void AsyncWrite( std::error_code error, size_t bytes )
    {
        if( !error )
            Dispatch();
        else
            Disconnect();
    }

    virtual void DispatchImpl( const uchar* buf, uint len ) override
    {
        asio::async_write( *socket, asio::buffer( buf, len ),
                           std::bind( &NetConnectionAsio::AsyncWrite, this, std::placeholders::_1, std::placeholders::_2 ) );
    }

    virtual void DisconnectImpl() override
    {
        socket->shutdown( asio::ip::tcp::socket::shutdown_receive );
        socket->close();
    }

public:
    NetConnectionAsio( asio::ip::tcp::socket* socket ): socket( socket )
    {
        NextAsyncRead();
    }

    virtual ~NetConnectionAsio() override
    {
        delete socket;
    }
};

NetServerBase::~NetServerBase() {}

class NetTcpServer: public NetServerBase
{
    std::function< void(NetConnection*) > connectionCallback;
    asio::io_service                      ioService;
    asio::ip::tcp::acceptor               acceptor;
    std::thread                           runThread;

    void Run()
    {
        ioService.run();
    }

    void AcceptNext()
    {
        asio::ip::tcp::socket* socket = new asio::ip::tcp::socket( ioService );
        acceptor.async_accept( *socket,
                               std::bind( &NetTcpServer::AcceptConnection, this, std::placeholders::_1, socket ) );
    }

    void AcceptConnection( std::error_code error, asio::ip::tcp::socket* socket )
    {
        if( !error )
        {
            connectionCallback( new NetConnectionAsio( socket ) );
        }
        else
        {
            WriteLog( "Accept error: {}.\n", error.message() );
            delete socket;
        }

        AcceptNext();
    }

public:
    NetTcpServer( ushort port, std::function< void(NetConnection*) > callback ): acceptor( ioService, asio::ip::tcp::endpoint( asio::ip::tcp::v4(), port ) )
    {
        connectionCallback = callback;
        AcceptNext();
        runThread = std::thread( &NetTcpServer::Run, this );
    }

    virtual ~NetTcpServer() override
    {
        ioService.stop();
        runThread.join();
    }
};

class NetConnectionWS: public NetConnectionImpl
{
    web_sockets*                server;
    web_sockets::connection_ptr connection;

    void OnMessage( web_sockets::message_ptr msg )
    {
        const string& payload = msg->get_payload();
        RUNTIME_ASSERT( !payload.empty() );
        ReceiveCallback( (const uchar*) payload.c_str(), (uint) payload.length() );
    }

    void OnFail()
    {
        WriteLog( "Fail: {}.\n", connection->get_ec().message() );
        Disconnect();
    }

    void OnClose()
    {
        Disconnect();
    }

    void OnHttp()
    {
        RUNTIME_ASSERT( "!OnHttp" );
    }

    virtual void DispatchImpl( const uchar* buf, uint len ) override
    {
        std::error_code error = connection->send( buf, len, websocketpp::frame::opcode::binary );
        if( !error )
            Dispatch();
        else
            Disconnect();
    }

    virtual void DisconnectImpl() override
    {
        std::error_code error;
        connection->terminate( error );
    }

public:
    NetConnectionWS( web_sockets* server, web_sockets::connection_ptr connection ): server( server ), connection( connection )
    {
        connection->set_message_handler( websocketpp::lib::bind( &NetConnectionWS::OnMessage, this, websocketpp::lib::placeholders::_2 ) );
        connection->set_fail_handler( websocketpp::lib::bind( &NetConnectionWS::OnFail, this ) );
        connection->set_close_handler( websocketpp::lib::bind( &NetConnectionWS::OnClose, this ) );
        connection->set_http_handler( websocketpp::lib::bind( &NetConnectionWS::OnHttp, this ) );
    }
};

class NetWebSocketsServer: public NetServerBase
{
    std::function< void(NetConnection*) > connectionCallback;
    web_sockets                           server;
    std::thread                           runThread;

    void Run()
    {
        server.run();
    }

    void OnOpen( websocketpp::connection_hdl hdl )
    {
        web_sockets::connection_ptr connection = server.get_con_from_hdl( hdl );
        connectionCallback( new NetConnectionWS( &server, connection ) );
    }

public:
    NetWebSocketsServer( ushort port, std::function< void(NetConnection*) > callback )
    {
        connectionCallback = callback;

        server.init_asio();
        server.set_open_handler( websocketpp::lib::bind( &NetWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1 ) );
        server.listen( port );
        server.start_accept();

        runThread = std::thread( &NetWebSocketsServer::Run, this );
    }

    virtual ~NetWebSocketsServer() override
    {
        server.stop();
        runThread.join();
    }
};

NetServerBase* NetServerBase::StartTcpServer( ushort port, std::function< void(NetConnection*) > callback )
{
    try
    {
        return new NetTcpServer( port, callback );
    }
    catch( std::exception ex )
    {
        WriteLog( "Can't start Tcp server: {}.\n", ex.what() );
        return nullptr;
    }
}

NetServerBase* NetServerBase::StartWebSocketsServer( ushort port, std::function< void(NetConnection*) > callback )
{
    try
    {
        return new NetWebSocketsServer( port, callback );
    }
    catch( std::exception ex )
    {
        WriteLog( "Can't start Web sockets server: {}.\n", ex.what() );
        return nullptr;
    }
}
