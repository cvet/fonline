#pragma once

#include "Common.h"

#include "NetBuffer.h"

class NetConnection
{
public:
    uint Ip;
    string Host;
    ushort Port;
    NetBuffer Bin;
    std::mutex BinLocker;
    NetBuffer Bout;
    std::mutex BoutLocker;
    bool IsDisconnected;
    uint DisconnectTick;

    virtual ~NetConnection() = default;
    virtual void DisableCompression() = 0;
    virtual void Dispatch() = 0;
    virtual void Disconnect() = 0;
};

class NetServerBase
{
public:
    using ConnectionCallback = std::function<void(NetConnection*)>;

    static NetServerBase* StartTcpServer(ushort port, ConnectionCallback callback);
    static NetServerBase* StartWebSocketsServer(ushort port, string wss_credentials, ConnectionCallback callback);

    virtual ~NetServerBase() = default;
};
