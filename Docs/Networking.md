# Networking

This document explains the reusable engine networking layers: message buffers, debug/hash handling, client/server connection abstractions, and the ordered UDP transport.

Use it when changing `Source/Common/NetBuffer.*`, `NetworkUdp.*`, `Source/Client/NetworkClient*`, `Source/Server/NetworkServer*`, or network tests.

## Ownership model

The engine owns transport abstractions, message framing, ordered UDP behavior, and client/server connection interfaces. An embedding project owns deployment topology, public server addresses, operational policy, and game-specific command usage.

Do not document project-specific hosts, ports, or release infrastructure here.

## Source paths inspected

- `Source/Common/NetBuffer.h`
- `Source/Common/NetBuffer.cpp`
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

## Inbound hardening (untrusted client → server)

The server treats all inbound bytes as hostile. Two layers guard against resource-exhaustion and malformed input:

- **Length-before-allocation rule.** Any peer-declared length/count must be validated against the bytes actually remaining in the buffer *before* allocation or iteration. `NetInBuffer::Read<string>()` and `NetInBuffer::ReadPropsData()` reject (`NetBufferException`) when the declared length exceeds `GetUnreadSize()`, so a tiny message can no longer amplify into a multi-GB allocation. Inbound remote-call decoding uses a read-only `DataReader`: string bytes are bounds-checked as a borrowed view before constructing the owned string, while array, dict, and dict-of-array counts are charged against the remaining payload using each value type's minimum wire size before `Reserve()`, container creation, or a count-driven loop. The server-side content validator performs the same count preflight. These checks reject prefixes that cannot fit their payload without imposing a separate fixed limit on conforming calls, so they do **not** require a compatibility-version bump.
- **Maximum message size.** `NetInBuffer::SetMaxMsgLen(len)` sets an upper bound on a single framed message; `NeedProcess()` throws `UnknownMessageException` (→ hard disconnect) at the header when `msg_len` exceeds it, before the receive buffer accumulates the payload. The server sets this from `ServerNetwork.MaxMessageSize` (0 = unlimited); the client leaves it unset so large server→client sync still works. All server-inbound messages are small control messages, so the default cap is well above any legitimate value.
- **Per-pass message budget.** The server drains at most `ServerNetwork.MaxMessagesPerProcessPass` messages per connection per worker-job pass, then yields; the periodic player job reschedules, so leftover buffered messages drain on the next pass and one flooding connection cannot monopolize a worker thread shared with world jobs.
- **UDP reorder window.** `UdpTransportOptions.MaxReorderAhead` (server: `ServerNetwork.MaxUdpReorderAhead`) bounds how far ahead of the next expected sequence the out-of-order reassembly map (`_receivedPackets`) buffers; payloads beyond the window are dropped (the sender retransmits), so a peer that never sends the in-order packet cannot grow the map without limit.

The per-type *content* validator (`ClientDataValidation.*`, invoked for client property writes and inbound remote-call payloads) is the complementary layer: it enforces finite floats, valid UTF-8, rejection of embedded NUL bytes in strings (a NUL is valid UTF-8 but never legitimate client text and is dangerous for C-string/log/DB consumers), non-negative sizes, count-to-payload consistency, and enum/hash/proto resolution. It does not impose a fixed maximum string length or element count, so absolute length/flood ceilings still live in the buffer/transport layer above.

## Hashes

Network buffers can serialize `hstring` values: `NetOutBuffer` writes the 64-bit hash, and `NetInBuffer` resolves it back to a string through a `HashResolver`.

When changing hash serialization, inspect both generated metadata/hash registration and runtime network consumers.

### Unresolved hash recovery

Client and server build their hash storages independently from local resources, so the server can transmit an `hstring` that was created at runtime (or that lives in content the client lacks) and which the client cannot resolve. `NetInBuffer::ReadHashedString` resolves the raw hash through the supplied `HashResolver`; when that lookup fails, the resolver's failure handler sees the raw `hstring::hash_t`, the input buffer is reset, and `ReadHashedString` throws a regular `NetBufferException`. The same handler also covers non-buffer lazy resolves, such as converting raw replicated property data into AngelScript `hstring`, arrays, dictionaries, or proto-reference objects.

The engine recovers from this instead of looping on the disconnect:

1. `ClientEngine` registers a `HashStorage` resolve-failure handler. When the client hits an unknown hash on an established connection, the handler writes `NetMessage::UnresolvedHash` and performs one immediate pending-output flush. `ClientConnection::Process` still turns the following `NetBufferException` into a normal disconnect for direct buffer reads; lazy script/property exceptions may be contained by the script event system, so the server also hard-disconnects the reporter after receiving the hash. The report is tiny and the connection was just live, so it lands in the kernel send buffer without a sleep/retry busy-wait. If a wedged socket drops it, the client re-reports the same hash the next time it hits it, so no bounded-wait loop is needed. The client keeps no state, writes nothing to disk, and learns the string on the next normal reconnect.
2. The server (`Process_UnresolvedHash`) resolves the reported hash against its own storage, logs it, and — when it can resolve the string — stores it in the persistent `HashReports` database collection (keyed by the string) and remembers it in memory. Hashes the server cannot resolve either are logged once per session and not stored. If a transport reports the close before the server worker reaches already-delivered input, the server checks a hard-disconnected connection for a pending `UnresolvedHash` before cleanup. The server then drops the connection (`HardDisconnect`), since a client that reported a bad hash has already stopped parsing the stream and is reconnecting — this also covers a client that reports without disconnecting itself.
3. The server broadcasts a newly learned string to all already-connected clients (`NetMessage::HashList`) and, on every handshake, sends the full known set to the connecting client right after `InitData` (`SendAllReportedHashes`). `HashList` is a count followed by length-prefixed strings.
4. Clients feed each received string through `HashResolver::ToHashedString`, which registers the same hash locally, so subsequent resolves of that hash succeed. Because the server resends the full set on every connect, a client that reported a hash and dropped resolves it after reconnecting.

The reported strings are stored raw (not registered into the server hash storage) so the server can keep and rebroadcast them without recreating dead entries. On startup the server loads the persisted `HashReports` collection after static content is loaded but before runtime/world strings are created, and checks each stored string with `HashStorage::CheckHashedString` (a non-inserting existence check). A reported gap is treated as fixed once its string resolves — i.e. the missing data was added to content — so it is deleted from storage and no longer broadcast. A string that is still unresolvable is logged with a warning, kept, and rebroadcast, since the underlying content is still missing.

This is a serialized contract change: `NetMessage::HashList` (server→client) and `NetMessage::UnresolvedHash` (client→server) were added, so the central compatibility marker in `Source/Common/Common.h` is bumped accordingly.

## Client connection abstraction

`Source/Client/NetworkClient.h` defines `NetworkClientConnection`.

Compressed client/server traffic is one continuous zlib stream flushed with `Z_SYNC_FLUSH`; transport reads may split or coalesce its bytes and are not independent compressed packets. Malformed input cannot be skipped or resynchronized inside the same connection. `StreamDecompressor` reports peer-stream failures as `DecompressException`, and `ClientConnection` treats that as a protocol failure: it logs the error, disconnects, and resets its buffers and decompressor so a later reconnect starts from a clean stream. It does not retry the same bytes, continue on the poisoned stream, or reinterpret decompression failure as a UDP-to-TCP fallback condition.

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

`NetworkServer` keeps weak references to every accepted connection. `Shutdown()` first closes registration
against concurrent accepts, snapshots and disconnects all still-live connections, and only then invokes the
transport-specific listener/io-context shutdown and thread join. A connection accepted concurrently with
shutdown is either included in that snapshot or rejected and disconnected by `TrackConnection()`; it cannot
escape between the accept callback and io-thread teardown. Repeated `Shutdown()` calls are no-ops.

The server runtime applies two independent limits to connections that have not logged in:

- `ServerNetwork.InactivityDisconnectTime` limits silence between any inbound messages;
- `ServerNetwork.LoginTimeout` limits time without meaningful pre-login progress (0 disables it). Handshake,
  authentication remote calls, and update-file requests refresh progress; transport pings do not. This lets a
  legitimate updater continue while preventing a peer from keeping an unauthenticated slot forever by only
  answering pings.

`NetworkServer` starts transport-specific servers through factories:

- `StartInterthreadServer()`;
- `StartUdpSocketsServer()`;
- `StartAsioServer()` when `FO_HAVE_ASIO` is enabled;
- `StartWebSocketsServer()` when `FO_HAVE_WEB_SOCKETS` is enabled;
- `CreateDummyConnection()` for tests/special paths.

The listen ports and the client connect endpoint are configured per transport:

- **TCP** listens on `Network.ServerPort`; **UDP** on `Network.ServerPort + Network.UdpPortOffset`.
- **WebSocket(S)** listens on `Network.WebSocketPort`.
- The client connects plain TCP/UDP to `ClientNetwork.ServerHost`:`Network.ServerPort`, and
  WebSocket(S) to `ClientNetwork.WebSocketHost`:`Network.WebSocketPort` — so the WebSocket endpoint
  can keep a hostname (for its TLS certificate) while the TCP/UDP endpoint can be a raw IP, letting a
  native client connect without DNS resolution.

Each endpoint is configured explicitly: the WebSocket host and port are independent settings, not
derived from `ServerHost` / `ServerPort`.

Concrete files include:

- `NetworkServer-Interthread.cpp`
- `NetworkServer-UdpSockets.cpp`
- `NetworkServer-Asio.cpp`
- `NetworkServer-WebSockets.cpp`

### Async transport connection lifetime & threading

The socket-based server connections (`Asio`, `WebSockets`) run their io loop on a dedicated thread while
the engine worker pool drives `Dispatch()`/`Disconnect()` on the connection from other threads, so the
connection wrapper's lifetime must be disciplined:

- **The wrapper must never be dereferenced by an io-thread callback after the engine drops it.** The Asio
  transport gets this for free by capturing `shared_from_this()` in every async read/write handler. The
  WebSockets transport wires its persistent websocketpp handlers post-construction (a `Start()` method,
  because `weak_from_this()` is unusable in the constructor) and each handler locks a `weak_from_this()`
  before touching the wrapper — a raw `this` capture is a use-after-free.
- **Cross-thread teardown must use the transport's thread-safe path.** For WebSockets that is
  `connection->close()` (it posts to the io service), never the io-thread-only `connection->terminate()`.
- **The wrapper must not extend the underlying connection's lifetime past its owning io_context.** The
  websocketpp endpoint owns each connection (with its io_context-bound asio timers) and destroys it on the
  io thread; the wrapper therefore holds the connection **weak** and locks per use. A strong ref lets a
  surviving wrapper destroy the connection after the io_context is gone — a shutdown-time use-after-free.

`Test_NetworkServer.cpp` covers each transport end-to-end (interthread, Asio accept-rearm and shutdown with an
accepted TCP connection, and a real websocketpp client that sends a frame then relies on server shutdown to
disconnect it); run it under the AddressSanitizer job to guard these lifetime rules.

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
