#ifndef __NETWORKING__
#define __NETWORKING__

#include "Common.h"
#include "BufferManager.h"
#include "zlib/zlib.h"
#include <functional>

class NetConnection
{
public:
    uint          Ip;
    string        Host;
    ushort        Port;
    BufferManager Bin;
    BufferManager Bout;
    bool          IsDisconnected;
    uint          DisconnectTick;

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
    static NetServerBase* StartWebSocketsServer( ushort port, std::function< void(NetConnection*) > callback );
    static NetServerBase* StartSecuredWebSocketsServer( ushort port, string wss_credentials, std::function< void(NetConnection*) > callback );
};

#endif // __NETWORKING__
