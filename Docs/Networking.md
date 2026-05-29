# Networking

This document explains the reusable engine networking layers: message buffers, debug/hash handling, legacy net commands, client/server connection abstractions, and the ordered UDP transport.

Use it when changing `Source/Common/NetBuffer.*`, `NetCommand.*`, `NetworkUdp.*`, `Source/Client/NetworkClient*`, `Source/Server/NetworkServer*`, or network tests.

## Ownership model

The engine owns transport abstractions, message framing, ordered UDP behavior, and client/server connection interfaces. An embedding project owns deployment topology, public server addresses, operational policy, and game-specific command usage.

Do not document project-specific hosts, ports, or release infrastructure here.

## Source paths inspected

- `Source/Common/NetBuffer.h`
- `Source/Common/NetBuffer.cpp`
- `Source/Common/NetCommand.h`
- `Source/Common/NetCommand.cpp`
- `Source/Common/NetworkUdp.h`
- `Source/Common/NetworkUdp.cpp`
- `Source/Client/NetworkClient.h`
- `Source/Client/NetworkClient-Interthread.cpp`
- `Source/Client/NetworkClient-Sockets.cpp`
- `Source/Client/NetworkClient-UdpSockets.cpp`
- `Source/Server/NetworkServer.h`
- `Source/Server/NetworkServer-Interthread.cpp`
- `Source/Server/NetworkServer-UdpSockets.cpp`
- `Source/Server/NetworkServer-Asio.cpp`
- `Source/Server/NetworkServer-WebSockets.cpp`
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`

## Message buffers

`Source/Common/NetBuffer.h` defines the shared binary message layer:

- `NetBuffer` — common storage, growth, encryption-key state, and raw copy support.
- `NetOutBuffer` — write/framing helper for outgoing messages.
- `NetInBuffer` — read/framing helper for incoming messages.

Important constants:

- `CRYPT_KEYS_COUNT = 50`
- `NETMSG_SIGNATURE = 0x011E9422`

`NetOutBuffer` responsibilities:

- append raw bytes via `Push()`;
- write arithmetic/enums/plain property types through typed `Write()`;
- write strings and hashed strings;
- write property data blocks with `WritePropsData()`;
- frame messages with `StartMsg()` and `EndMsg()`.

`NetInBuffer` responsibilities:

- accumulate incoming bytes with `AddData()`;
- determine when a full message needs processing with `NeedProcess()`;
- read typed values, strings, and hashed strings;
- read property data with `ReadPropsData()`;
- parse message IDs with `ReadMsg()`;
- shrink/reset read buffers after processing.

Property synchronization and entity state transfer should go through these helpers instead of hand-rolled byte layouts.

## Hashes

Network buffers can serialize `hstring` values: `NetOutBuffer` writes the 64-bit hash, and `NetInBuffer` resolves it back to a string through a `HashResolver`.

When changing hash serialization, inspect both generated metadata/hash registration and runtime network consumers.

### Unresolved hash recovery

Client and server build their hash storages independently from local resources, so the server can transmit an `hstring` that was created at runtime (or that lives in content the client lacks) and which the client cannot resolve. When `NetInBuffer::ReadHashedString` fails to resolve an incoming hash it throws `UnresolvedHashException` (declared in `NetBuffer.h`), a `NetBufferException` subclass that carries the raw `hstring::hash_t`. Catch the derived type before the base where the failed hash must be recovered.

The engine recovers from this instead of looping on the disconnect:

1. `ClientConnection::Process` catches the exception and immediately sends the failing hash to the server in a `NetMessage::UnresolvedHash` message with a single non-blocking `SendData()` flush. `Process` runs on the main loop, so it must not stall: the report is tiny and the connection was just live, so it lands in the kernel send buffer (delivered by the graceful close) without a sleep/retry busy-wait. If a wedged socket drops it, the client re-reports the same hash the next time it hits it, so no bounded-wait loop is needed. The client then drops the connection, rethrows so the host's recreate/reconnect flow re-establishes it, and keeps no state and writes nothing to disk.
2. The server (`Process_UnresolvedHash`) resolves the reported hash against its own storage, logs it, and — when it can resolve the string — stores it in the persistent `HashReports` database collection (keyed by the string) and remembers it in memory. Hashes the server cannot resolve either are logged once per session and not stored. The server then drops the connection (`HardDisconnect`), since a client that reported a bad hash has already stopped parsing the stream and is reconnecting — this also covers a client that reports without disconnecting itself.
3. The server broadcasts a newly learned string to all already-connected clients (`NetMessage::HashList`) and, on every handshake, sends the full known set to the connecting client right after `InitData` (`SendAllReportedHashes`). `HashList` is a count followed by length-prefixed strings.
4. Clients feed each received string through `HashResolver::ToHashedString`, which registers the same hash locally, so subsequent resolves of that hash succeed. Because the server resends the full set on every connect, a client that reported a hash and dropped resolves it after reconnecting.

The reported strings are stored raw (not registered into the server hash storage) so the server can keep and rebroadcast them without recreating dead entries. On startup the server loads the persisted `HashReports` collection after static content is loaded but before runtime/world strings are created, and checks each stored string with `HashStorage::CheckHashedString` (a non-inserting existence check). A reported gap is treated as fixed once its string resolves — i.e. the missing data was added to content — so it is deleted from storage and no longer broadcast. A string that is still unresolvable is logged with a warning, kept, and rebroadcast, since the underlying content is still missing.

This is a serialized contract change: `NetMessage::HashList` (server→client) and `NetMessage::UnresolvedHash` (client→server) were added, so the central compatibility marker in `Source/Common/Common.h` is bumped accordingly.

## Net commands

`Source/Common/NetCommand.h` contains legacy/admin-style command IDs and `PackNetCommand()`:

- `CMD_EXIT`
- `CMD_MYINFO`
- `CMD_GAMEINFO`
- `CMD_MOVECRIT`
- `CMD_DISCONCRIT`
- `CMD_TOGLOBAL`
- `CMD_PROPERTY`
- `CMD_ADDITEM`
- `CMD_ADDITEM_SELF`
- `CMD_ADDNPC`
- `CMD_ADDLOCATION`
- `CMD_RUNSCRIPT`
- `CMD_REGENMAP`
- `CMD_LOG`

`PackNetCommand()` converts command text into an outgoing network buffer using a log callback and hash resolver.

Treat command IDs as wire-level compatibility points. If they change, update server/client handling and tests together.

## Client connection abstraction

`Source/Client/NetworkClient.h` defines `NetworkClientConnection`.

The public surface is transport-neutral:

- `IsConnecting()` / `IsConnected()`;
- byte counters: `GetBytesSend()`, `GetBytesReceived()`;
- `CheckStatus(for_write)`;
- `SendData()`;
- `ReceiveData()`;
- `Disconnect()`.

Factory methods choose transport implementation:

- `CreateInterthreadConnection()`
- `CreateSocketsConnection()`
- `CreateUdpSocketsConnection()`

Concrete files include:

- `NetworkClient-Interthread.cpp`
- `NetworkClient-Sockets.cpp`
- `NetworkClient-UdpSockets.cpp`

The client runtime should depend on the abstract connection interface where possible; transport-specific behavior belongs in the implementation files.

## Server connection abstraction

`Source/Server/NetworkServer.h` defines two server-side abstractions:

- `NetworkServerConnection` — one accepted/active connection.
- `NetworkServer` — listening server lifecycle.

`NetworkServerConnection` owns callback registration and dispatch:

- `SetAsyncCallbacks(send, receive, disconnect)`;
- `Dispatch()`;
- `Disconnect()`;
- `GetHost()` / `GetPort()`;
- `IsDisconnected()`.

`NetworkServer` starts transport-specific servers through factories:

- `StartInterthreadServer()`;
- `StartUdpSocketsServer()`;
- `StartAsioServer()` when `FO_HAVE_ASIO` is enabled;
- `StartWebSocketsServer()` when `FO_HAVE_WEB_SOCKETS` is enabled;
- `CreateDummyConnection()` for tests/special paths.

Concrete files include:

- `NetworkServer-Interthread.cpp`
- `NetworkServer-UdpSockets.cpp`
- `NetworkServer-Asio.cpp`
- `NetworkServer-WebSockets.cpp`

## Ordered UDP transport

`Source/Common/NetworkUdp.h` implements an ordered/reliable payload layer over UDP.

Packet types:

- `Connect`
- `Accept`
- `Payload`
- `KeepAlive`
- `Disconnect`

`UdpTransportOptions` controls:

- `MaxPayload`
- `MaxPendingBytes`
- `ResendTimeoutMs`
- `ConnectRetryMs`
- `Redundancy`

`UdpPacketInfo` carries parsed packet data:

- packet type;
- session ID;
- sequence number;
- acknowledgement sequence;
- acknowledgement bitmask;
- extra value;
- payload bytes.

`UdpOrderedChannel` owns session state and reliable ordering:

- session state: `GetSessionId()`, `HasSession()`, `SetSessionId()`, `Reset()`;
- output readiness: `NeedSend()`, `CanAcceptPayload()`;
- packet creation/resend: `PrepareOutput()`;
- incoming sequence handling: `HandleIncomingPayload()`;
- ordered delivery: `HasReadyData()`, `ExtractReadyData()`;
- disconnect: `MakeDisconnectPacket()`.

Standalone helpers:

- `MakeUdpConnectPacket()`
- `MakeUdpAcceptPacket()`
- `TryParseUdpPacket()`

When changing UDP behavior, validate acknowledgement handling, pending-byte limits, resend timing, packet parsing, disconnect handling, and redundant tail packets.

## Relationship to entity and property state

Entity/property synchronization uses property metadata to decide what can be sent and network buffers to serialize the data.

Relevant property flags from [EntityModel.md](EntityModel.md):

- `Synced`
- `OwnerSync`
- `PublicSync`
- `NoSync`
- `ModifiableByClient`
- `ModifiableByAnyClient`

A network change that affects property replication should be reviewed together with entity/property docs and tests.

## Transport selection

The source tree supports several connection families:

- interthread transports for in-process/test-style communication;
- socket transports;
- UDP socket transports backed by `UdpOrderedChannel`;
- ASIO server support when built with `FO_HAVE_ASIO`;
- WebSocket server support when built with `FO_HAVE_WEB_SOCKETS`.

Build availability is controlled by compile-time feature toggles and platform dependencies. For build toggles and package workflow, see [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp` for the in-process client/server handshake and connection-event path.

## Change routing

- Binary framing/encryption/hash serialization: `Source/Common/NetBuffer.*`.
- Text/admin command packing: `Source/Common/NetCommand.*`.
- Ordered UDP behavior: `Source/Common/NetworkUdp.*`.
- Client transport abstraction and implementations: `Source/Client/NetworkClient*`.
- Server transport abstraction and implementations: `Source/Server/NetworkServer*`.
- Entity/property replication semantics: [EntityModel.md](EntityModel.md) and generated metadata/property code.
- Build feature toggles: [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).

## Validation checklist

1. Run UDP, client, and server network tests relevant to the changed transport.
2. Validate both connect/accept and disconnect paths.
3. Validate partial receives and message framing when changing `NetInBuffer` / `NetOutBuffer`.
4. Validate hash resolution/debug-hash behavior across client and server builds.
5. Validate property synchronization when message layout or property-data serialization changes.
6. Validate platform-specific transport availability when touching ASIO/WebSocket/socket code.
