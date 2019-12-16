#pragma once

#include "Common.h"
#include "NetBuffer.h"
#include "zlib.h"

class NetConnection
{
public:
    uint      Ip;
    string    Host;
    ushort    Port;
    NetBuffer Bin;
    NetBuffer Bout;
    bool      IsDisconnected;
    uint      DisconnectTick;

    virtual ~NetConnection() = 0;
    virtual void DisableCompression() = 0;
    virtual void Dispatch() = 0;
    virtual void Disconnect() = 0;
};

class NetServerBase
{
public:
    virtual ~NetServerBase() = 0;

    static NetServerBase* StartTcpServer( ushort port, std::function< void(NetConnection*) > callback );
    static NetServerBase* StartWebSocketsServer( ushort port, string wss_credentials, std::function< void(NetConnection*) > callback );
};
