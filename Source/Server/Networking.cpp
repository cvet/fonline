//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Todo: catch exceptions in network servers

#include "Networking.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "WinApi_Include.h"

#define ASIO_STANDALONE
#include "asio.hpp"
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_
#define _WEBSOCKETPP_CPP11_STL_
#pragma warning(push)
#pragma warning(disable : 4267)
#include "websocketpp/config/asio.hpp"
#include "websocketpp/server.hpp"
#pragma warning(pop)
using web_sockets_tls = websocketpp::server<websocketpp::config::asio_tls>;
using web_sockets_no_tls = websocketpp::server<websocketpp::config::asio>;
using ssl_context = websocketpp::lib::asio::ssl::context;
#include "zlib.h"

class NetTcpServer : public NetServerBase
{
public:
    NetTcpServer(ServerNetworkSettings& sett, ConnectionCallback callback);
    virtual ~NetTcpServer() override;

private:
    void Run();
    void AcceptNext();
    void AcceptConnection(std::error_code error, asio::ip::tcp::socket* socket);

    ServerNetworkSettings& settings;
    asio::ip::tcp::acceptor acceptor;
    ConnectionCallback connectionCallback {};
    asio::io_service ioService {};
    std::thread runThread {};
};

class NetNoTlsWebSocketsServer : public NetServerBase
{
public:
    NetNoTlsWebSocketsServer(ServerNetworkSettings& sett, ConnectionCallback callback);
    virtual ~NetNoTlsWebSocketsServer() override;

private:
    void Run();
    void OnOpen(websocketpp::connection_hdl hdl);
    bool OnValidate(websocketpp::connection_hdl hdl);

    ServerNetworkSettings& settings;
    ConnectionCallback connectionCallback {};
    web_sockets_no_tls server {};
    std::thread runThread {};
};

class NetTlsWebSocketsServer : public NetServerBase
{
public:
    NetTlsWebSocketsServer(ServerNetworkSettings& sett, ConnectionCallback callback);
    virtual ~NetTlsWebSocketsServer() override;

private:
    void Run();
    void OnOpen(websocketpp::connection_hdl hdl);
    bool OnValidate(websocketpp::connection_hdl hdl);
    websocketpp::lib::shared_ptr<ssl_context> OnTlsInit(websocketpp::connection_hdl hdl);

    ServerNetworkSettings& settings;
    ConnectionCallback connectionCallback {};
    web_sockets_tls server {};
    std::thread runThread {};
};

class NetConnectionImpl : public NetConnection
{
public:
    NetConnectionImpl(ServerNetworkSettings& sett) : settings {sett}
    {
        IsDisconnected = false;
        DisconnectTick = 0;
        zStream = nullptr;
        memzero(outBuf, sizeof(outBuf));

        if (!settings.DisableZlibCompression)
        {
            zStream = new z_stream();
            zStream->zalloc = [](void* opaque, unsigned int items, unsigned int size) { return calloc(items, size); };
            zStream->zfree = [](void* opaque, void* address) { free(address); };
            zStream->opaque = nullptr;
            int result = deflateInit(zStream, Z_BEST_SPEED);
            RUNTIME_ASSERT(result == Z_OK);
        }
    }

    virtual ~NetConnectionImpl() override
    {
        Dispatch();
        Disconnect();

        if (zStream)
            deflateEnd(zStream);
        SAFEDEL(zStream);
    }

    virtual void DisableCompression() override
    {
        if (zStream)
            deflateEnd(zStream);
        SAFEDEL(zStream);
    }

    virtual void Dispatch() override
    {
        if (IsDisconnected)
            return;

        // Nothing to send
        {
            SCOPE_LOCK(BoutLocker);
            if (Bout.IsEmpty())
                return;
        }

        DispatchImpl();
    }

    virtual void Disconnect() override
    {
        if (IsDisconnected)
            return;

        IsDisconnected = true;
        if (!DisconnectTick)
            DisconnectTick = (uint)Timer::RealtimeTick();

        DisconnectImpl();
    }

protected:
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    const uchar* SendCallback(uint& out_len)
    {
        SCOPE_LOCK(BoutLocker);

        if (Bout.IsEmpty())
            return nullptr;

        // Compress
        if (zStream)
        {
            uint to_compr = Bout.GetEndPos();
            if (to_compr > sizeof(outBuf) - 32)
                to_compr = sizeof(outBuf) - 32;

            zStream->next_in = Bout.GetCurData();
            zStream->avail_in = to_compr;
            zStream->next_out = outBuf;
            zStream->avail_out = sizeof(outBuf);

            int result = deflate(zStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(result == Z_OK);

            uint compr = (uint)((size_t)zStream->next_out - (size_t)outBuf);
            uint real = (uint)((size_t)zStream->next_in - (size_t)Bout.GetCurData());
            out_len = compr;
            Bout.Cut(real);
        }
        // Without compressing
        else
        {
            uint len = Bout.GetEndPos();
            if (len > sizeof(outBuf))
                len = sizeof(outBuf);
            memcpy(outBuf, Bout.GetCurData(), len);
            out_len = len;
            Bout.Cut(len);
        }

        // Normalize buffer size
        if (Bout.IsEmpty())
            Bout.Reset();

        RUNTIME_ASSERT(out_len > 0);
        return outBuf;
    }

    void ReceiveCallback(const uchar* buf, uint len)
    {
        SCOPE_LOCK(BinLocker);

        if (Bin.GetCurPos() + len < settings.FloodSize)
        {
            Bin.Push(buf, len, true);
        }
        else
        {
            Bin.Reset();
            Disconnect();
        }
    }

    ServerNetworkSettings& settings;

private:
    z_stream* zStream {};
    uchar outBuf[NetBuffer::DefaultBufSize] {};
};

class NetConnectionAsio : public NetConnectionImpl
{
public:
    NetConnectionAsio(ServerNetworkSettings& sett, asio::ip::tcp::socket* socket) :
        NetConnectionImpl(sett), socket {socket}
    {
        const auto& address = socket->remote_endpoint().address();
        Ip = (address.is_v4() ? address.to_v4().to_ulong() : uint(-1));
        Host = address.to_string();
        Port = socket->remote_endpoint().port();

        if (settings.DisableTcpNagle)
            socket->set_option(asio::ip::tcp::no_delay(true), dummyError);
        memzero(inBuf, sizeof(inBuf));
        writePending = 0;
        NextAsyncRead();
    }

    virtual ~NetConnectionAsio() override { delete socket; }

private:
    void NextAsyncRead()
    {
        asio::async_read(*socket, asio::buffer(inBuf), asio::transfer_at_least(1),
            std::bind(&NetConnectionAsio::AsyncRead, this, std::placeholders::_1, std::placeholders::_2));
    }

    void AsyncRead(std::error_code error, size_t bytes)
    {
        if (!error)
        {
            ReceiveCallback(inBuf, (uint)bytes);
            NextAsyncRead();
        }
        else
        {
            Disconnect();
        }
    }

    void AsyncWrite(std::error_code error, size_t bytes)
    {
        writePending = false;

        if (!error)
            DispatchImpl();
        else
            Disconnect();
    }

    virtual void DispatchImpl() override
    {
        bool expected = false;
        if (writePending.compare_exchange_strong(expected, true))
        {
            uint len = 0;
            const uchar* buf = SendCallback(len);
            if (buf)
            {
                asio::async_write(*socket, asio::buffer(buf, len),
                    std::bind(&NetConnectionAsio::AsyncWrite, this, std::placeholders::_1, std::placeholders::_2));
            }
            else
            {
                if (IsDisconnected)
                {
                    socket->shutdown(asio::ip::tcp::socket::shutdown_both, dummyError);
                    socket->close(dummyError);
                }

                writePending = false;
            }
        }
    }

    virtual void DisconnectImpl() override
    {
        socket->shutdown(asio::ip::tcp::socket::shutdown_both, dummyError);
        socket->close(dummyError);
    }

    asio::ip::tcp::socket* socket {};
    std::atomic_bool writePending {};
    uchar inBuf[NetBuffer::DefaultBufSize] {};
    asio::error_code dummyError {};
};

template<typename web_sockets>
class NetConnectionWS : public NetConnectionImpl
{
    using connection_ptr = typename web_sockets::connection_ptr;
    using message_ptr = typename web_sockets::message_ptr;

public:
    NetConnectionWS(ServerNetworkSettings& sett, web_sockets* server, connection_ptr connection) :
        NetConnectionImpl(sett), server {server}, connection {connection}
    {
        const auto& address = connection->get_raw_socket().remote_endpoint().address();
        Ip = (address.is_v4() ? address.to_v4().to_ulong() : uint(-1));
        Host = address.to_string();
        Port = connection->get_raw_socket().remote_endpoint().port();

        if (settings.DisableTcpNagle)
        {
            asio::error_code error;
            connection->get_raw_socket().set_option(asio::ip::tcp::no_delay(true), error);
        }

        connection->set_message_handler(
            websocketpp::lib::bind(&NetConnectionWS::OnMessage, this, websocketpp::lib::placeholders::_2));
        connection->set_fail_handler(websocketpp::lib::bind(&NetConnectionWS::OnFail, this));
        connection->set_close_handler(websocketpp::lib::bind(&NetConnectionWS::OnClose, this));
        connection->set_http_handler(websocketpp::lib::bind(&NetConnectionWS::OnHttp, this));
    }

private:
    void OnMessage(message_ptr msg)
    {
        const string& payload = msg->get_payload();
        RUNTIME_ASSERT(!payload.empty());
        ReceiveCallback((const uchar*)payload.c_str(), (uint)payload.length());
    }

    void OnFail()
    {
        WriteLog("Fail: {}.\n", connection->get_ec().message());
        Disconnect();
    }

    void OnClose() { Disconnect(); }

    void OnHttp()
    {
        // Prevent use this feature
        Disconnect();
    }

    virtual void DispatchImpl() override
    {
        uint len = 0;
        const uchar* buf = SendCallback(len);
        if (buf)
        {
            std::error_code error = connection->send(buf, len, websocketpp::frame::opcode::binary);
            if (!error)
                DispatchImpl();
            else
                Disconnect();
        }
    }

    virtual void DisconnectImpl() override
    {
        std::error_code error;
        connection->terminate(error);
    }

    web_sockets* server {};
    connection_ptr connection {};
};

NetTcpServer::NetTcpServer(ServerNetworkSettings& sett, ConnectionCallback callback) :
    settings {sett}, acceptor(ioService, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), settings.Port))
{
    connectionCallback = callback;
    AcceptNext();
    runThread = std::thread(&NetTcpServer::Run, this);
}

NetTcpServer::~NetTcpServer()
{
    ioService.stop();
    runThread.join();
}

void NetTcpServer::Run()
{
    asio::error_code error;
    ioService.run(error);
}

void NetTcpServer::AcceptNext()
{
    asio::ip::tcp::socket* socket = new asio::ip::tcp::socket(ioService);
    acceptor.async_accept(*socket, std::bind(&NetTcpServer::AcceptConnection, this, std::placeholders::_1, socket));
}

void NetTcpServer::AcceptConnection(std::error_code error, asio::ip::tcp::socket* socket)
{
    if (!error)
    {
        connectionCallback(new NetConnectionAsio(settings, socket));
    }
    else
    {
        WriteLog("Accept error: {}.\n", error.message());
        delete socket;
    }

    AcceptNext();
}

NetNoTlsWebSocketsServer::NetNoTlsWebSocketsServer(ServerNetworkSettings& sett, ConnectionCallback callback) :
    settings {sett}
{
    connectionCallback = callback;

    server.init_asio();
    server.set_open_handler(
        websocketpp::lib::bind(&NetNoTlsWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1));
    server.set_validate_handler(
        websocketpp::lib::bind(&NetNoTlsWebSocketsServer::OnValidate, this, websocketpp::lib::placeholders::_1));
    server.listen(asio::ip::tcp::v6(), settings.Port + 1);
    server.start_accept();

    runThread = std::thread(&NetNoTlsWebSocketsServer::Run, this);
}

NetNoTlsWebSocketsServer::~NetNoTlsWebSocketsServer()
{
    server.stop();
    runThread.join();
}

void NetNoTlsWebSocketsServer::Run()
{
    server.run();
}

void NetNoTlsWebSocketsServer::OnOpen(websocketpp::connection_hdl hdl)
{
    web_sockets_no_tls::connection_ptr connection = server.get_con_from_hdl(hdl);
    connectionCallback(new NetConnectionWS<web_sockets_no_tls>(settings, &server, connection));
}

bool NetNoTlsWebSocketsServer::OnValidate(websocketpp::connection_hdl hdl)
{
    web_sockets_no_tls::connection_ptr connection = server.get_con_from_hdl(hdl);
    std::error_code error;
    connection->select_subprotocol("binary", error);
    return !error;
}

NetTlsWebSocketsServer::NetTlsWebSocketsServer(ServerNetworkSettings& sett, ConnectionCallback callback) :
    settings {sett}
{
    if (settings.WssPrivateKey.empty())
        throw GenericException("'WssPrivateKey' not provided");
    if (settings.WssCertificate.empty())
        throw GenericException("'WssCertificate' not provided");

    connectionCallback = callback;

    server.init_asio();
    server.set_open_handler(
        websocketpp::lib::bind(&NetTlsWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1));
    server.set_validate_handler(
        websocketpp::lib::bind(&NetTlsWebSocketsServer::OnValidate, this, websocketpp::lib::placeholders::_1));
    server.set_tls_init_handler(
        websocketpp::lib::bind(&NetTlsWebSocketsServer::OnTlsInit, this, websocketpp::lib::placeholders::_1));
    server.listen(asio::ip::tcp::v6(), settings.Port + 1);
    server.start_accept();

    runThread = std::thread(&NetTlsWebSocketsServer::Run, this);
}

NetTlsWebSocketsServer::~NetTlsWebSocketsServer()
{
    server.stop();
    runThread.join();
}

void NetTlsWebSocketsServer::Run()
{
    server.run();
}

void NetTlsWebSocketsServer::OnOpen(websocketpp::connection_hdl hdl)
{
    web_sockets_tls::connection_ptr connection = server.get_con_from_hdl(hdl);
    connectionCallback(new NetConnectionWS<web_sockets_tls>(settings, &server, connection));
}

bool NetTlsWebSocketsServer::OnValidate(websocketpp::connection_hdl hdl)
{
    web_sockets_tls::connection_ptr connection = server.get_con_from_hdl(hdl);
    std::error_code error;
    connection->select_subprotocol("binary", error);
    return !error;
}

websocketpp::lib::shared_ptr<ssl_context> NetTlsWebSocketsServer::OnTlsInit(websocketpp::connection_hdl hdl)
{
    websocketpp::lib::shared_ptr<ssl_context> ctx(new ssl_context(ssl_context::tlsv1));
    ctx->set_options(
        ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::single_dh_use);
    ctx->use_certificate_chain_file(settings.WssCertificate);
    ctx->use_private_key_file(settings.WssPrivateKey, ssl_context::pem);
    return ctx;
}

NetServerBase* NetServerBase::StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback)
{
    try
    {
        return new NetTcpServer(settings, callback);
    }
    catch (std::exception ex)
    {
        WriteLog("Can't start Tcp server: {}.\n", ex.what());
        return nullptr;
    }
}

NetServerBase* NetServerBase::StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback)
{
    try
    {
        if (!settings.SecuredWebSockets)
            return new NetNoTlsWebSocketsServer(settings, callback);
        else
            return new NetTlsWebSocketsServer(settings, callback);
    }
    catch (std::exception ex)
    {
        WriteLog("Can't start Web sockets server: {}.\n", ex.what());
        return nullptr;
    }
}
