#include "Networking.h"
#include <stdexcept>

#define ASIO_STANDALONE
#include "asio.hpp"

#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_
#define _WEBSOCKETPP_CPP11_STL_
#include "websocketpp/config/asio.hpp"
#include "websocketpp/server.hpp"
using web_sockets_tls = websocketpp::server< websocketpp::config::asio_tls >;
using web_sockets_no_tls = websocketpp::server< websocketpp::config::asio >;
using ssl_context = websocketpp::lib::asio::ssl::context;

static void* ZlibAlloc( void* opaque, unsigned int items, unsigned int size ) { return calloc( items, size ); }
static void  ZlibFree( void* opaque, void* address )                          { free( address ); }

#ifndef FO_WINDOWS
# define InterlockedExchange( val, newval )    __sync_lock_test_and_set( val, newval )
#endif

NetConnection::~NetConnection() {}

class NetConnectionImpl: public NetConnection
{
    z_stream* zStream;
    uchar     outBuf[ BufferManager::DefaultBufSize ];

public:
    NetConnectionImpl()
    {
        IsDisconnected = false;
        DisconnectTick = 0;
        zStream = nullptr;
        memzero( outBuf, sizeof( outBuf ) );

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
        Bout.Unlock();

        DispatchImpl();
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
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    const uchar* SendCallback( uint& out_len )
    {
        Bout.Lock();
        if( Bout.IsEmpty() )
        {
            Bout.Unlock();
            return nullptr;
        }

        // Compress
        if( zStream )
        {
            uint to_compr = Bout.GetEndPos();
            if( to_compr > sizeof( outBuf ) - 32 )
                to_compr = sizeof( outBuf ) - 32;

            zStream->next_in = Bout.GetCurData();
            zStream->avail_in = to_compr;
            zStream->next_out = outBuf;
            zStream->avail_out = sizeof( outBuf );

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
            if( len > sizeof( outBuf ) )
                len = sizeof( outBuf );
            memcpy( outBuf, Bout.GetCurData(), len );
            out_len = len;
            Bout.Cut( len );
        }

        // Normalize buffer size
        if( Bout.IsEmpty() )
            Bout.Reset();

        Bout.Unlock();

        RUNTIME_ASSERT( out_len > 0 );
        return outBuf;
    }

    void ReceiveCallback( const uchar* buf, uint len )
    {
        Bin.Lock();
        if( Bin.GetCurPos() + len >= GameOpt.FloodSize )
        {
            Bin.Reset();
            Bin.Unlock();
            Disconnect();
            return;
        }
        Bin.Push( buf, len, true );
        Bin.Unlock();
    }
};

class NetConnectionAsio: public NetConnectionImpl
{
    asio::ip::tcp::socket* socket;
    volatile long          writePending;
    uchar                  inBuf[ BufferManager::DefaultBufSize ];
    asio::error_code       dummyError;

    void NextAsyncRead()
    {
        asio::async_read( *socket, asio::buffer( inBuf ), asio::transfer_at_least( 1 ),
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
        InterlockedExchange( &writePending, 0 );

        if( !error )
            DispatchImpl();
        else
            Disconnect();
    }

    virtual void DispatchImpl() override
    {
        if( InterlockedExchange( &writePending, 1 ) == 0 )
        {
            uint         len = 0;
            const uchar* buf = SendCallback( len );
            if( buf )
            {
                asio::async_write( *socket, asio::buffer( buf, len ),
                                   std::bind( &NetConnectionAsio::AsyncWrite, this, std::placeholders::_1, std::placeholders::_2 ) );
            }
            else
            {
                if( IsDisconnected )
                {
                    socket->shutdown( asio::ip::tcp::socket::shutdown_both, dummyError );
                    socket->close( dummyError );
                }

                InterlockedExchange( &writePending, 0 );
            }
        }
    }

    virtual void DisconnectImpl() override
    {
        socket->shutdown( asio::ip::tcp::socket::shutdown_both, dummyError );
        socket->close( dummyError );
    }

public:
    NetConnectionAsio( asio::ip::tcp::socket* socket ): socket( socket )
    {
        const auto& address = socket->remote_endpoint().address();
        Ip = ( address.is_v4() ? address.to_v4().to_ulong() : uint( -1 ) );
        Host = address.to_string();
        Port = socket->remote_endpoint().port();

        if( GameOpt.DisableTcpNagle )
            socket->set_option( asio::ip::tcp::no_delay( true ), dummyError );
        memzero( inBuf, sizeof( inBuf ) );
        writePending = 0;
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
        asio::error_code error;
        ioService.run( error );
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
    NetTcpServer( ushort port, std::function< void(NetConnection*) > callback ): acceptor( ioService, asio::ip::tcp::endpoint( asio::ip::tcp::v6(), port ) )
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

template< typename web_sockets >
class NetConnectionWS: public NetConnectionImpl
{
    using connection_ptr = typename web_sockets::connection_ptr;
    using message_ptr = typename web_sockets::message_ptr;

    web_sockets*   server;
    connection_ptr connection;

    void OnMessage( message_ptr msg )
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
        // Prevent use this feature
        Disconnect();
    }

    virtual void DispatchImpl() override
    {
        uint         len = 0;
        const uchar* buf = SendCallback( len );
        if( buf )
        {
            std::error_code error = connection->send( buf, len, websocketpp::frame::opcode::binary );
            if( !error )
                DispatchImpl();
            else
                Disconnect();
        }
    }

    virtual void DisconnectImpl() override
    {
        std::error_code error;
        connection->terminate( error );
    }

public:
    NetConnectionWS( web_sockets* server, connection_ptr connection ): server( server ), connection( connection )
    {
        const auto& address = connection->get_raw_socket().remote_endpoint().address();
        Ip = ( address.is_v4() ? address.to_v4().to_ulong() : uint( -1 ) );
        Host = address.to_string();
        Port = connection->get_raw_socket().remote_endpoint().port();

        if( GameOpt.DisableTcpNagle )
        {
            asio::error_code error;
            connection->get_raw_socket().set_option( asio::ip::tcp::no_delay( true ), error );
        }

        connection->set_message_handler( websocketpp::lib::bind( &NetConnectionWS::OnMessage, this, websocketpp::lib::placeholders::_2 ) );
        connection->set_fail_handler( websocketpp::lib::bind( &NetConnectionWS::OnFail, this ) );
        connection->set_close_handler( websocketpp::lib::bind( &NetConnectionWS::OnClose, this ) );
        connection->set_http_handler( websocketpp::lib::bind( &NetConnectionWS::OnHttp, this ) );
    }
};

class NetNoTlsWebSocketsServer: public NetServerBase
{
    std::function< void(NetConnection*) > connectionCallback;
    web_sockets_no_tls                    server;
    std::thread                           runThread;

    void Run()
    {
        server.run();
    }

    void OnOpen( websocketpp::connection_hdl hdl )
    {
        web_sockets_no_tls::connection_ptr connection = server.get_con_from_hdl( hdl );
        connectionCallback( new NetConnectionWS< web_sockets_no_tls >( &server, connection ) );
    }

    bool OnValidate( websocketpp::connection_hdl hdl )
    {
        web_sockets_no_tls::connection_ptr connection = server.get_con_from_hdl( hdl );
        std::error_code                    error;
        connection->select_subprotocol( "binary", error );
        return !error;
    }

public:
    NetNoTlsWebSocketsServer( ushort port, std::function< void(NetConnection*) > callback )
    {
        connectionCallback = callback;

        server.init_asio();
        server.set_open_handler( websocketpp::lib::bind( &NetNoTlsWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1 ) );
        server.set_validate_handler( websocketpp::lib::bind( &NetNoTlsWebSocketsServer::OnValidate, this, websocketpp::lib::placeholders::_1 ) );
        server.listen( asio::ip::tcp::v6(), port );
        server.start_accept();

        runThread = std::thread( &NetNoTlsWebSocketsServer::Run, this );
    }

    virtual ~NetNoTlsWebSocketsServer() override
    {
        server.stop();
        runThread.join();
    }
};

class NetTlsWebSocketsServer: public NetServerBase
{
    std::function< void(NetConnection*) > connectionCallback;
    web_sockets_tls                       server;
    std::thread                           runThread;
    string                                privateKey;
    string                                certificate;

    void Run()
    {
        server.run();
    }

    void OnOpen( websocketpp::connection_hdl hdl )
    {
        web_sockets_tls::connection_ptr connection = server.get_con_from_hdl( hdl );
        connectionCallback( new NetConnectionWS< web_sockets_tls >( &server, connection ) );
    }

    bool OnValidate( websocketpp::connection_hdl hdl )
    {
        web_sockets_tls::connection_ptr connection = server.get_con_from_hdl( hdl );
        std::error_code                 error;
        connection->select_subprotocol( "binary", error );
        return !error;
    }

    websocketpp::lib::shared_ptr< ssl_context > OnTlsInit( websocketpp::connection_hdl hdl )
    {
        websocketpp::lib::shared_ptr< ssl_context > ctx( new ssl_context( ssl_context::tlsv1 ) );
        ctx->set_options( ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::single_dh_use );
        ctx->use_certificate_chain_file( certificate );
        ctx->use_private_key_file( privateKey, ssl_context::pem );
        return ctx;
    }

public:
    NetTlsWebSocketsServer( ushort port, string private_key, string cert, std::function< void(NetConnection*) > callback )
    {
        connectionCallback = callback;
        privateKey = private_key;
        certificate = cert;

        server.init_asio();
        server.set_open_handler( websocketpp::lib::bind( &NetTlsWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1 ) );
        server.set_validate_handler( websocketpp::lib::bind( &NetTlsWebSocketsServer::OnValidate, this, websocketpp::lib::placeholders::_1 ) );
        server.set_tls_init_handler( websocketpp::lib::bind( &NetTlsWebSocketsServer::OnTlsInit, this, websocketpp::lib::placeholders::_1 ) );
        server.listen( asio::ip::tcp::v6(), port );
        server.start_accept();

        runThread = std::thread( &NetTlsWebSocketsServer::Run, this );
    }

    virtual ~NetTlsWebSocketsServer() override
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

NetServerBase* NetServerBase::StartWebSocketsServer( ushort port, string wss_credentials, std::function< void(NetConnection*) > callback )
{
    try
    {
        if( wss_credentials.empty() )
        {
            return new NetNoTlsWebSocketsServer( port, callback );
        }
        else
        {
            StrVec keys = _str( wss_credentials ).split( ' ' );
            if( keys.size() != 2 ) throw std::runtime_error( "Invalid 'WssCredentials' option" );

            return new NetTlsWebSocketsServer( port, keys[ 0 ], keys[ 1 ], callback );
        }
    }
    catch( std::exception ex )
    {
        WriteLog( "Can't start Web sockets server: {}.\n", ex.what() );
        return nullptr;
    }
}
