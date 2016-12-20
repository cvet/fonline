#ifndef __NETWORKING__
#define __NETWORKING__

#include "Common.h"
#include "BufferManager.h"
#include "zlib/zlib.h"
#include <functional>

class NetConnection
{
public:
    sockaddr_in   From;
    BufferManager Bin;
    BufferManager Bout;
    bool          IsDisconnected;
    uint          DisconnectTick;

    virtual ~NetConnection() = 0;
    virtual void DisableCompression() = 0;
    virtual void Dispatch() = 0;
    virtual void Disconnect() = 0;
    virtual void Shutdown() = 0;
};

class NetServerBase
{
public:
    virtual void SetConnectionCallback( std::function< void(NetConnection*) > callback ) = 0;
    virtual bool Listen( ushort port ) = 0;
    virtual void Shutdown() = 0;

    static NetServerBase* CreateTcpServer();
    static NetServerBase* CreateWebSocketsServer();
};

#endif // __NETWORKING__
