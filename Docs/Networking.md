# Networking

This document explains the reusable engine networking layers: the secure channel, message buffers, debug/hash handling, client/server connection abstractions, and the ordered UDP transport.

Use it when changing `Source/Common/NetBuffer.*`, `NetworkUdp.*`, `NoiseProtocol.*`, `SecureChannel.*`, `Source/Essentials/Cryptography.*`, `Source/Client/NetworkClient*`, `Source/Client/ClientConnection.*`, `Source/Server/NetworkServer*`, `Source/Server/ServerConnection.*`, or network tests.

## Ownership model

The engine owns transport abstractions, message framing, ordered UDP behavior, and client/server connection interfaces. An embedding project owns deployment topology, public server addresses, operational policy, and game-specific command usage.

Do not document project-specific hosts, ports, or release infrastructure here.

## Source paths inspected

- `Source/Common/NetBuffer.h`
- `Source/Common/NetBuffer.cpp`
- `Source/Common/NetworkUdp.h`
- `Source/Common/NetworkUdp.cpp`
- `Source/Common/NoiseProtocol.h`
- `Source/Common/NoiseProtocol.cpp`
- `Source/Common/SecureChannel.h`
- `Source/Common/SecureChannel.cpp`
- `Source/Essentials/Cryptography.h`
- `Source/Essentials/Cryptography.cpp`
- `Source/Client/ClientConnection.h`
- `Source/Client/ClientConnection.cpp`
- `Source/Client/NetworkClient.h`
- `Source/Client/NetworkClient-Interthread.cpp`
- `Source/Client/NetworkClient-Sockets.cpp`
- `Source/Client/NetworkClient-UdpSockets.cpp`
- `Source/Server/NetworkServer.h`
- `Source/Server/NetworkServer-Interthread.cpp`
- `Source/Server/NetworkServer-UdpSockets.cpp`
- `Source/Server/NetworkServer-Asio.cpp`
- `Source/Server/NetworkServer-WebSockets.cpp`
- `Source/Server/ServerConnection.h`
- `Source/Server/ServerConnection.cpp`
- `BuildTools/secure_channel_key.py`
- `Source/Tests/Test_Cryptography.cpp`
- `Source/Tests/Test_NoiseProtocol.cpp`
- `Source/Tests/Test_SecureChannel.cpp`
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`

## Secure channel

Every connection that crosses a network carries its bytes inside a Noise channel: the protocol
`Noise_NK_25519_ChaChaPoly_BLAKE2b` (Noise Protocol Framework, revision 34). NK fits the game's shape: a client has no
identity of its own before it logs in, while the server's static key is known in advance and pinned in the client.
One round trip therefore authenticates the server, derives fresh session keys (so a later theft of the server key
does not open recorded sessions), and from then on every byte is encrypted and authenticated in each direction, with
per-direction nonce counters that reject a replayed, reordered, dropped or reflected frame.

What it protects against: an observer or a man in the middle on the path (public Wi-Fi, a provider, forged DNS), and a
forged server — a client never speaks to a server that cannot prove it holds a pinned key, so such a server can
neither read the login nor call the client's remote calls. What it cannot protect against: the owner of the client
machine. Whoever controls the process can patch it, read the session keys from memory and send forged messages
through a genuine session; server authority and the inbound hardening below remain the defence there.

### Where it sits

The channel wraps the ordered byte stream every transport already provides (TCP, ordered UDP, WebSocket and the
in-process interthread one), so the engine has one path for all of them, and WebSocket clients are pinned to the
server key independently of the Web PKI that WSS uses underneath. The channel handshake runs before `NetMessage::Handshake`, so nothing travels in the
clear — updater traffic included. Order in the stack:

- server to client: `NetOutBuffer` messages → zlib stream → sealed frames → transport;
- client to server: `NetOutBuffer` messages (not compressed) → sealed frames → transport.

Sealing traffic that never leaves the process protects nothing, since whoever can read it can read the session keys
too. The interthread transport runs the channel anyway, so every embedded client — in tests, tools and development
launches — takes the path a remote player does. There is no exception at all: every `ServerConnection` owns a channel,
and a stand-in with no peer behind it (`NetworkServer::CreateDummyConnection()`) simply never receives an offer, so
whatever is written to it stays sealed away in its output buffer.

### Wire format

Every frame is a big-endian 16-bit length followed by that many bytes. The prologue of every handshake is the fixed
string `SecureChannel::PROLOGUE`, and both handshake messages carry empty payloads.

| Frame | Sender | Body |
|---|---|---|
| Offer | client, first | a count `n` (1 to `SecureChannel::MAX_OFFERED_KEYS`), then `n` first NK messages (`e`, `es`: 32-byte key + 16-byte tag), one per pinned server key |
| Answer | server | the index of the offer its key opened, then the second NK message (`e`, `ee`: 48 bytes) |
| Transport | both, afterwards | one Noise transport message: at most 65519 bytes of plaintext and its 16-byte tag |

The client offers every pinned key at once, each with its own ephemeral key, and the server answers the one it can
open. A server that opens none sends nothing and drops the connection. Frame sizes are checked at the header, before
the body is buffered: a handshake frame of a size no valid offer or answer has, or a transport frame shorter than a
tag, fails the channel at once.

### Keys

| Setting | Side | Meaning |
|---|---|---|
| `ServerNetwork.ChannelSecretKey` | server | the static X25519 secret key, 64 hex digits; every server needs it, one that listens on nothing included, since every connection it creates owns a channel; the server logs its public half at networking start |
| `ClientNetwork.ChannelServerKeys` | client | the pinned server public keys, 64 hex digits each, at most `SecureChannel::MAX_OFFERED_KEYS` |

A server refuses to start without a valid key, and a client without a valid pin fails the connect: there is no
unprotected fallback. Engine tests share `BakerTests::TEST_CHANNEL_SECRET_KEY` and its public half,
which `ApplySelfContainedServerSettings()` and `ApplySelfContainedClientSettings()` apply; a `ServerConnection` a test
builds by hand takes `BakerTests::MakeTestChannelIdentity()`. The secret key is a credential — keep it out of the repository and
supply it at run time, for example through `$TARGET_FILE{...}` (see
[ConfigurationAndDataSources.md](ConfigurationAndDataSources.md#runtime-settings)), which reads it on the target host
and never at bake time. `BuildTools/secure_channel_key.py generate <file>` writes a new secret key readable by its owner only (it never
overwrites an existing file) and prints the public key; `public <file>` prints the public key of an existing one.

Rotation needs no downtime because the client offers all its pins: ship clients that pin both the current and the next
key, switch the server to the next key once those clients are out, and drop the old pin later.

### Connection integration

`ClientConnection` starts the channel when the transport connects and writes `NetMessage::Handshake` the moment the
channel is established; its ping waits for the same point, so the server never reads anything before the handshake
message. A UDP connect that falls back to TCP starts a new channel and a new compressed stream on the new connection.
Any channel failure is a `NoiseException` (`SecureChannelException` derives from it) and ends in `Disconnect()`.

`ServerConnection` keeps the channel under its own mutex, taken inside either buffer lock and never around another:
the network thread decrypts under the input lock, the transport's send path seals under the output lock. The answer to
an offer is dispatched from the receive path at once. A message written before the channel stands stays in the output
buffer and leaves sealed afterwards. A channel failure is latched the same way as an input overflow
(`IsInputRejected()`), because it is detected inside the transport receive lock a disconnect would take again; the
owning worker pass then disconnects with `DisconnectReason::ProtocolError`. A failed channel is logged as a warning, not
reported as an exception: a stranger, a scanner or a client pinned to another key produces exactly that.

The ordered UDP transport's own header (sequence, acknowledgement, session) stays outside the channel. Forging it can
disturb delivery, which an on-path attacker can do anyway, but cannot inject content: every payload byte still has to
pass the channel. The same holds for TCP resets.

Compression runs before encryption only on the server-to-client stream; the client's own stream — logins and tokens —
is never compressed, so its length reveals nothing about repeated secrets.

## Message buffers

`Source/Common/NetBuffer.h` defines the shared binary message layer:

- `NetBuffer` — common storage, growth, and raw copy support.
- `NetOutBuffer` — write/framing helper for outgoing messages.
- `NetInBuffer` — read/framing helper for incoming messages.

Important constant: `NETMSG_SIGNATURE = 0x011E9422`, the marker every framed message starts with. The buffers hold
plaintext; confidentiality and integrity belong to the secure channel above.

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
- expose `GetUnreadSize()` for only the current framed message and `GetBufferedUnreadSize()` for all retained input;
- require exact consumption of the current frame before `ShrinkReadBuf()` or the next `NeedProcess()` advances past it;
- shrink/reset read buffers after processing.

Property synchronization and entity state transfer should go through these helpers instead of hand-rolled byte layouts.

## Inbound hardening (untrusted client → server)

The server treats all inbound bytes as hostile. Two layers guard against resource-exhaustion and malformed input:

- **Length-before-allocation rule.** Any peer-declared length/count must be validated against the bytes actually remaining in the *current frame* before allocation or iteration. `NetInBuffer::Read<string>()` and `NetInBuffer::ReadPropsData()` reject (`NetBufferException`) when the declared length exceeds `GetUnreadSize()`, so a tiny message cannot borrow bytes from the next coalesced frame or amplify into a multi-GB allocation. Inbound remote-call decoding uses a read-only `data_reader`: string bytes are bounds-checked as a borrowed view before constructing the owned string, while array, dict, and dict-of-array counts are charged against the remaining payload using each value type's minimum wire size before `Reserve()`, container creation, or a count-driven loop. The server-side content validator performs the same count preflight.
- **Maximum message size.** `NetInBuffer::SetMaxMsgLen(len)` sets an upper bound on a single framed message; `NeedProcess()` throws `UnknownMessageException` (→ hard disconnect) at the header when `msg_len` exceeds it, before the receive buffer accumulates the payload. The server sets this from `ServerNetwork.MaxMessageSize` (0 = unlimited); the client leaves it unset so large server→client sync still works. All server-inbound messages are small control messages, so the default cap is well above any legitimate value.
- **Maximum retained input.** `NetInBuffer::SetMaxBufLen(len)` caps total unread bytes retained across coalesced/partial frames, so a peer cannot grow the receive buffer by never completing a message. The server applies `ServerNetwork.MaxBufferedInputSize` to the TCP/UDP/interthread buffers and to the WebSocket endpoint's per-message cap; the limit must be zero (unlimited) or at least `MaxMessageSize`, and `InitNetworkingJob()` fails server startup otherwise. `AddData()` throws `NetBufferException` over the cap, but it runs on the network thread inside the same transport receive lock a disconnect would take again, so `ServerConnection` only latches the rejection (reporting an overflow once) and `ServerEngine::ProcessConnection()` performs the hard disconnect on the owning worker pass. A connection whose input was rejected takes no further input. The secure channel's own buffer holds at most one partial frame, so it adds no unbounded retention of its own.
- **Remote-call structural limits.** A `///@ RemoteCall` may declare `MaxBytes N` and `MaxCollectionSize N`. Metadata carries both limits for every call - a call that declares neither is baked with an explicit `Limits 0 0` trailer, and a record without it is rejected at registration - and the server resolves the call name and rejects its payload before construction, while both the content validator and the AngelScript decoder enforce the collection limit before reserve/container creation, including nested dict arrays. Unknown calls are rejected before allocating their body. Adding these fields changed compatibility metadata and therefore requires the corresponding compatibility-version bump.
- **Remote-call runtime ceiling.** `ServerNetwork.MaxRemoteCallPayloadSize` is the server-wide decoded RPC ceiling. The effective payload limit is the smaller non-zero structural/runtime limit; a call-specific `MaxBytes` is the semantic protocol boundary, not a substitute for the global hostile-input ceiling.
- **Per-pass message budget.** The server drains at most `ServerNetwork.MaxMessagesPerProcessPass` messages per connection per worker-job pass, then yields; the periodic player job reschedules, so leftover buffered messages drain on the next pass and one flooding connection cannot monopolize a worker thread shared with world jobs.
- **UDP reorder window.** `UdpTransportOptions.MaxReorderAhead` (server: `ServerNetwork.MaxUdpReorderAhead`) bounds how far ahead of the next expected sequence the out-of-order reassembly map (`_receivedPackets`) buffers; payloads beyond the window are dropped (the sender retransmits), so a peer that never sends the in-order packet cannot grow the map without limit.

The per-type *content* validator (`ClientDataValidation.*`, invoked for client property writes and inbound remote-call payloads) is the complementary layer: it enforces finite floats, valid UTF-8, rejection of embedded NUL bytes in strings (a NUL is valid UTF-8 but never legitimate client text and is dangerous for C-string/log/DB consumers), non-negative sizes, count-to-payload consistency, and enum/hash/proto resolution. It does not impose a fixed maximum string length or element count, so absolute length/flood ceilings still live in the buffer/transport layer above.

## Hashes

Network buffers can serialize `hstring` values: `NetOutBuffer` writes the 64-bit hash, and `NetInBuffer` resolves it back to a string through a `hash_resolver`.

Each engine fills its hash storage in advance, from its own resources, and a hash that arrives over the wire is resolved against that storage: the message carries the 64-bit hash only, so the receiver never learns the text from it. A client holds exactly these strings before the first message:

- metadata names registered at startup: entity, fixed and base type names, holder entries, remote-call names, migration-rule parts;
- `fopro-bin-client`: the id of every prototype and fixed-type row the client registers, and every string a client-visible (`Common` or `Client`) hashed property holds in those prototypes: `hstring` values, prototype references, `hstring` dictionary keys and values. A `Server` property is disabled on the client, so it contributes nothing;
- the animation-info index of the loaded packs: every image path in `SpriteInfo/<Pack>.foinfo` and every model name in `ModelAnimationInfo.foinfo`. Other resources are not interned by indexing: the sound index keeps plain file names;
- every key part of the text packs of the loaded language;
- the static `hstring` values of the client script assembly, interned when the assembly initializes;
- a map's `fomap-bin-client` hash table, the client-visible hashed values of that map's static items with hidden ones included, only when that map loads (`MapView::LoadStaticData`).

Nothing else is there. A map-instance override on a critter or a dynamic item lives only in the server map-bin, a string the server holds only in server code or composes at runtime is in no client source, and a sound or other non-image resource path is known only when a client-visible prototype property names it. Any of them fails to resolve when it arrives in a synced property, a remote-call argument or a synced ref-type value. The server storage is filled the same way from the server's resources, and `ClientDataValidation` rejects a hash a client sends that the server does not hold.

When changing hash serialization, inspect both generated metadata/hash registration and runtime network consumers.

### Unresolved hash recovery

Client and server build their hash storages independently from local resources, so the server can transmit an `hstring` that was created at runtime (or that lives in content the client lacks) and which the client cannot resolve. `NetInBuffer::ReadHashedString` resolves the raw hash through the supplied `hash_resolver`; when that lookup fails, the resolver's failure handler sees the raw `hstring::hash_t`, the input buffer is reset, and `ReadHashedString` throws a regular `NetBufferException`. The same handler also covers non-buffer lazy resolves, such as converting raw replicated property data into AngelScript `hstring`, arrays, dictionaries, or proto-reference objects.

The engine recovers from this instead of looping on the disconnect:

1. `ClientEngine` registers a `hash_storage` resolve-failure handler. When the client hits an unknown hash on an established connection, the handler writes `NetMessage::UnresolvedHash` and performs one immediate pending-output flush. `ClientConnection::Process` still turns the following `NetBufferException` into a normal disconnect for direct buffer reads; lazy script/property exceptions may be contained by the script event system, so the server also hard-disconnects the reporter after receiving the hash. The report is tiny and the connection was just live, so it lands in the kernel send buffer without a sleep/retry busy-wait. If a wedged socket drops it, the client re-reports the same hash the next time it hits it, so no bounded-wait loop is needed. The client keeps no state, writes nothing to disk, and learns the string on the next normal reconnect.
2. The server (`Process_UnresolvedHash`) resolves the reported hash against its own storage, logs it, and — when it can resolve the string — stores it in the persistent `HashReports` database collection (keyed by the string) and remembers it in memory. Hashes the server cannot resolve either are logged once per session and not stored. If a transport reports the close before the server worker reaches already-delivered input, the server checks a hard-disconnected connection for a pending `UnresolvedHash` before cleanup. The server then drops the connection (`HardDisconnect`), since a client that reported a bad hash has already stopped parsing the stream and is reconnecting — this also covers a client that reports without disconnecting itself.
3. The server broadcasts a newly learned string to all already-connected clients (`NetMessage::HashList`) and, on every handshake, sends the full known set to the connecting client right after `InitData` (`SendAllReportedHashes`). `HashList` is a count followed by length-prefixed strings.
4. Clients feed each received string through `hash_resolver::to_hashed_string`, which registers the same hash locally, so subsequent resolves of that hash succeed. Because the server resends the full set on every connect, a client that reported a hash and dropped resolves it after reconnecting.

Recovery repairs the next connection, not the read that failed: the first read has already thrown, and on the managed bridge that is a script exception at the property read. It is a diagnostic for a missing string, never a way to deliver one; a value that must resolve on arrival belongs in one of the sources listed above.

The reported strings are stored raw (not registered into the server hash storage) so the server can keep and rebroadcast them without recreating dead entries. On startup the server loads the persisted `HashReports` collection after static content is loaded but before runtime/world strings are created, and checks each stored string with `hash_storage::CheckHashedString` (a non-inserting existence check). A reported gap is treated as fixed once its string resolves — i.e. the missing data was added to content — so it is deleted from storage and no longer broadcast. A string that is still unresolvable is logged with a warning, kept, and rebroadcast, since the underlying content is still missing.

This is a serialized contract change: `NetMessage::HashList` (server→client) and `NetMessage::UnresolvedHash` (client→server) were added, so the central compatibility marker in `Source/Common/Common.h` is bumped accordingly.

## Client connection abstraction

`Source/Client/NetworkClient.h` defines `NetworkClientConnection`.

Compressed client/server traffic is one continuous zlib stream flushed with `Z_SYNC_FLUSH`; transport reads may split or coalesce its bytes and are not independent compressed packets. Malformed input cannot be skipped or resynchronized inside the same connection. `stream_decompressor` reports peer-stream failures as `DecompressException`, and `ClientConnection` treats that as a protocol failure: it logs the error, disconnects, and resets its buffers and decompressor so a later reconnect starts from a clean stream. It does not retry the same bytes, continue on the poisoned stream, or reinterpret decompression failure as a UDP-to-TCP fallback condition.

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

`ClientConnection` watches the server from its side too. It pings every `ClientNetwork.PingPeriod` milliseconds,
and a ping - or, before that, the secure channel handshake - that stays unanswered while **nothing at all**
arrives for `ClientNetwork.PingTimeout` milliseconds (default 30000, `0` waits for ever) ends the connection with
`Connection lost: the server has sent nothing for ... ms` - through the ordinary disconnect path, so the updater
reports `ConnectionFailed` and the game client its usual connection loss. The handshake half matters because the
first ping goes out only once the channel stands, so a server that accepts the transport and then answers nothing
would otherwise never be pinged at all. Without it a peer that vanished without closing the connection was waited on for ever:
the ordered UDP channel resends to a silent address indefinitely, and a half-open TCP link or a stopped server
process keeps its socket established while nothing is served. Any received byte counts as an answer, so a large
update portion queued ahead of the ping reply on a slow link is not mistaken for silence; the check is skipped
under a debugger, like the server's own ping watchdog. The pending ping is cleared on disconnect, so a reconnect
starts pinging afresh. Pinned by `ClientUpdaterGivesUpOnAServerThatStopsAnswering`
(`Source/Tests/Test_ClientUpdater.cpp`).

## Server connection abstraction

`Source/Server/NetworkServer.h` defines two server-side abstractions:

- `NetworkServerConnection` — one accepted/active connection.
- `NetworkServer` — listening server lifecycle.

`NetworkServerConnection` owns callback registration and dispatch:

- `SetAsyncCallbacks(send, receive, disconnect)`;
- `Dispatch()`;
- `Disconnect()`;
- `DropAsyncCallbacks()`;
- `GetHost()` / `GetPort()`;
- `IsDisconnected()`.

The send callback returns the outgoing bytes **by value**, and every transport owns the buffer it hands to
its socket. That is a correctness requirement, not a style choice: the sender behind the callback is a
`ServerConnection` owned by one `Player`, while the connection object lives in a transport's `shared_ptr`,
and `Dispatch()` reaches the send path from any pool thread. A buffer borrowed from the sender would be
refilled by a second dispatch, or freed when its owner disconnects, while a transport was still reading it -
which is how a partly compressed packet turned into a SIGSEGV inside zlib on the UDP send thread.
`Disconnect()` clears the send-callback flag before disconnecting, so a transport that ticks afterwards
stops pulling from a sender that is going away. Each callback is also invoked under the lock that guards
it, and `Disconnect()` drops the send and receive callbacks under those same locks on every call, so a
destructor that disconnects waits for a call already running on a transport thread instead of racing it.
The disconnect callback is the exception: only the call that wins the disconnect flag reports it, and the
winner may be a transport thread still inside the transport teardown when the owner's own `Disconnect()`
returns - the server sees `IsDisconnected()` as soon as the flag is won and may destroy the owner at once.
An owner therefore finishes with `DropAsyncCallbacks()`, which clears every callback under its lock: a
report already running is waited out, and one not yet started finds nothing to call
(`ServerConnection::~ServerConnection`).

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

A logged-in connection is additionally dropped when it stops answering pings: `ServerNetwork.ClientPingTime`
sets the interval, and a connection that has not answered the previous ping when the next one is due is hard
disconnected. The in-process interthread transport opts out of this watchdog: its peer lifetime is explicit
through the callback channel, while a busy shared process can delay both ends of the ping exchange together.
Closing either interthread endpoint still disconnects the other immediately. It is still pinged — the answer is
the round trip below, and an embedded client is subject to the same movement allowances as a remote one — but a
late answer only postpones the next ping instead of dropping the peer, so every sample stays paired with its own
request. `ServerConnection::NeedPing()` schedules the exchange for every transport and
`ServerConnection::NeedsPingWatchdog()` says whether a missed answer disconnects.

The same exchange also yields the only transport-delay figure the **server** owns. `RegisterPingRequest`
stamps the request, `RegisterPingAnswer` turns a paired answer into a round-trip sample and folds it into a
smoothed `GetRoundTrip()`; an unpaired answer is discarded. It is deliberately an upper bound rather than a
measurement of the wire: the client answers from its frame loop, so the sample carries that frame time too.
That is the property that makes it usable — every allowance the server grants a client for transport delay is
clamped by this number, so the figure may be generous but is never a value the client chose. It is zero until
the first answer, and each consumer states what it does with an unknown round trip.

### The client reports a movement it finished predicting

A client predicts its own critter's movement locally and sends `SendCritterMove` only afterwards, so the
server starts the same movement one uplink transit later and stays that far behind for its whole duration.
An **interrupted** movement already closes that gap: the client sends `SendStopCritterMove` with its final
position and the server reconciles along the movement path. A movement that simply **completed** used to
report nothing, and the server learned of the arrival only by finishing the movement itself — which is
exactly when a reach-sensitive action request tends to arrive.

`SendCritterMoveFinished` closes that path. The client sends it when a predicted movement plays out to its
end (`CritterHexView::ProcessMoving` → `ClientEngine::CritterMovingFinished`, chosen critter only), carrying
map id, critter id, the hex the finished plan ended on, and the final position/dir. `Process_MoveFinished`
reconciles through the same `ReconcileCritterStopPosition` the stop path uses, so every hex still passes the
blocking check, triggers and visibility.

Two rules keep it honest:

- **The report names its movement by that movement's end hex.** It cannot name a server-side movement id,
  because the initiator is excluded from the `CritterMove` broadcast (`SendAndBroadcast(initiator, …)`) and
  therefore never receives one for its own movement. A report that does not match the plan the critter is
  walking is dropped, and that movement plays out as before.
- **A client may fast-forward only what the transport plausibly cost it.** The allowance is
  `min(round trip / 2, Server.MoveFinishCatchUpMaxMs) + Server.CritterMovingPeriodMs` — the measured delay,
  capped, plus how late this server's own arrival can be. A larger remainder is refused and logged as
  `Process_MoveFinished: arrival reported too early`. With no round trip yet the allowance is the movement
  period alone, which still covers the scheduling quantum.

Because one connection preserves order and the handler is fully synchronous inside `ProcessPlayer`'s message
loop, the arrival is reconciled **before** any action request behind it is read. That ordering is the point:
it is a guarantee by construction rather than a race the server usually wins.

Ordinary completion sends no position broadcast — observers walk the same plan and reach its end themselves,
so a broadcast would only snap clients that are already right. One is sent only when the reconciled hex is
not the hex the plan ends on.

### Movement synchronization trace

`Network.MoveSyncTrace` (off by default, too verbose for production) makes the server and every client write
one `MOVESYNC` log line per movement synchronization event, so the three views of one critter — where the
server holds it, where its own client draws it, and where another client sees it — can be laid side by side
after a run. The line is a format rather than prose, written by `WriteMoveSyncTrace` (`Movement.h`):

```
MOVESYNC side=<srv|cl> ev=<event> t=<monotonic µs> st=<synchronized ms> [viewer=<chosen id>] key=value…
```

`t` is the local monotonic clock, which every process on one machine shares, so a single-machine run aligns
all three views without estimating any clock offset. Across machines only `st` is common, and a client's
`st` trails the server's by the delivery of its last time sync, so cross-machine latencies read from it are
estimates. Every client line carries `viewer=<its chosen critter id>`, which keeps several clients apart
when they write into one log (embedded clients do).

| Side | `ev` | Written by | Fields |
|------|------|------------|--------|
| srv | `move_req` | `Process_Move`, before the plan starts | `cr player client_start server_hex steps bridge truncated speed rtt_ms` |
| srv | `move_start` | `StartCritterMoving` (player or script) | `cr uid start end whole_ms offset_ms speed was_moving initiator` |
| srv | `step` | `ProcessCritterMovingBySteps`, per hex entered | `cr uid hex elapsed_ms runtime_ms` |
| srv | `stop` | `StopCritterMoving` | `cr uid reason hex broadcast` (`reason` is the `MovingState` value) |
| srv | `stopmove_req` | `Process_StopMove`, after reconciliation | `cr client_hex server_hex reconciled final_hex rtt_ms` |
| srv | `finish_req` | `Process_MoveFinished`, at every exit | `cr reported_end client_hex server_hex remaining_ms allowed_ms rtt_ms outcome` |
| srv | `speed_change` | `ChangeCritterMovingSpeed` | `cr uid old_speed speed hex elapsed_ms runtime_ms rebased_ms whole_ms`; `runtime_ms - elapsed_ms` is progress the rebase discards |
| srv | `send` | `Player::Send_Moving` / `Send_Teleport` | `cr to own kind(move\|pos\|teleport) hex [end offset_ms]`; `own=1` is a correction of the recipient's own critter |
| cl | `move_send` / `stop_send` / `finish_send` | `Net_SendMove` / `Net_SendStopMove` / `Net_SendMoveFinished` | what the acting client told the server |
| cl | `move_recv` / `pos_recv` / `teleport_recv` | the three inbound position messages | `cr own …`; `own=1` is a correction; `pos_recv` carries `jump` (hexes moved) and `err_px` (pixels between where the critter was drawn and the received position — a sub-hex re-split moves the hex but not the picture) |
| cl | `speed_recv` | `Net_OnCritterMoveSpeed` | `cr own old_speed speed hex elapsed_ms rebased_ms whole_ms` — the client rebases one delivery after the server |
| cl | `step` / `arrive` | `CritterHexView::ProcessMoving` | where the client draws a critter, hex by hex, and where its plan ended |
| cl | `in` / `out` | `Net_OnAddCritter` / `Net_OnRemoveCritter` | a critter entered or left this client's view: `cr own hex` (and `moving` on `in`). Between an `out` and the next `in` the client knows nothing about the critter, so a stale last hex is not a disagreement; a client re-entering its own critter on login traces `in` with itself as the viewer |
| cl | `mark` | project scripts | a scenario boundary (`label=begin:<name>` / `end:<name>`) written through the AI-control bridge |
| cl | `input` | project scripts | a change of scripted direct input (`state=<keys or stick> pattern_ms`), written by the AI-control input driver |

`finish_req` outcomes are `accepted`, `not_moving` (the server had already finished — the normal case),
`attached`, `stale_plan`, `too_early`, `invalidated`, `superseded` and `reconcile_failed`. Reading the lines
is the job of an embedding project's tooling; the engine only guarantees that the field names above stay
stable.

### Disconnect reasons

Every close records **why** it happened, because after the fact a connection that went away tells nothing
about whether the player quit, the network died, or the server dropped them. `ServerConnection` stores a
`DisconnectReason` (`ServerConnection.h`), and `HardDisconnect(reason)` takes it as a mandatory argument so
a new call site cannot forget one:

| Reason | Cause |
|--------|-------|
| `None` | still connected; no close has happened |
| `ClientClosed` | the transport reported the peer went away — a voluntary quit and a lost network are indistinguishable here, because the client closes its socket without announcing either |
| `InactivityTimeout` | `ServerNetwork.InactivityDisconnectTime` elapsed with no inbound message |
| `PingTimeout` | the previous ping was never answered |
| `LoginTimeout` | `ServerNetwork.LoginTimeout` elapsed without pre-login progress |
| `ProtocolError` | unreadable or unexpected network data, or a failed connection publication |
| `UpdaterError` | a bad update-file request |
| `ServerShutdown` | the server is stopping |
| `ScriptRequest` | `Player.HardDisconnect()` from a script |
| `LoginFailed` | login rolled back after a server-side failure |
| `ReplacedByReconnect` | the same account logged in again and took the session over |

The **first** recorded reason wins. The transport close callback records `ClientClosed` of its own accord,
and it runs after the path that decided to disconnect — without first-wins, that generic cause would bury
every specific one. The reason is part of the `Closed connection from` log line and is readable from scripts
through `Player.GetDisconnectReason()` while `OnPlayerLogout` handlers run, which is how the game reports a
truthful session-end cause instead of assuming every disconnect was a logout. Pinned by
`ServerConnectionRecordsWhyItWasDisconnected` in `Source/Tests/Test_NetworkServer.cpp`.

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

Socket diagnostics must not use `std::error_code::message()`, `std::system_error::what()`, `strerror()`,
or `FormatMessage()` directly because those APIs return text in the host OS locale. Route socket failures
through `net_sockets::error_text()` instead. It maps common network conditions to stable English names and
always retains the native category and numeric code; unknown conditions use the English fallback
`Network error (<category>:<code>)`. Listener startup exceptions must also add the transport and port so
an occupied TCP or WebSocket endpoint is actionable without relying on localized system text.

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

A client in the server's own process takes the interthread transport whenever the server registered its listener
on `Network.ServerPort`. `Network.DisableInterthreadCommunication` keeps `ServerEngine::InitNetworkingJob()` from
registering it, so such a client connects over UDP or TCP and goes through a real socket.

Build availability is controlled by compile-time feature toggles and platform dependencies. For build toggles and package workflow, see [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).

### A listener that cannot bind is retried before the startup gives up

A restart races the process it replaces for its ports, and the loser used to take the whole startup
down on its first attempt: the world loaded, a socket that frees itself within seconds was still
held, and every bit of that work was thrown away. Each remote listener is therefore started through
`ServerEngine::StartConnectionServer`, which retries until `ServerNetwork.ListenRetryTime` runs out,
waiting `ServerNetwork.ListenRetryDelay` between attempts.

Past the deadline the original exception is rethrown and the startup fails, because a server nobody
can reach is not a started server — the retries buy the losing side of the race some time, they do
not turn a dead port into an acceptable state. The interthread transport is not part of this: it
binds nothing another process could hold, so a failure there is a defect rather than a race.

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_Cryptography.cpp` for the primitives against RFC 7748, RFC 8439 and reference BLAKE2b/HMAC values;
- `Source/Tests/Test_NoiseProtocol.cpp` for `Noise_NK_25519_ChaChaPoly_BLAKE2b` against the published cacophony, snow and noise-c vectors, and refusal of a foreign key, a changed prologue, tampering, truncation and nonce reuse;
- `Source/Tests/Test_SecureChannel.cpp` for framing in any chunking, offering several pins, refusal of an unpinned server, malformed frames, tampering, replay and loss, a handshake cut off halfway, and real client/server connections over TCP, UDP and WebSocket;
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp` for the in-process client/server handshake and connection-event path.

## Change routing

- Secure channel framing and handshake: `Source/Common/SecureChannel.*`; the Noise state machine: `Source/Common/NoiseProtocol.*`; the primitives and OS randomness: `Source/Essentials/Cryptography.*`.
- Binary message framing/hash serialization: `Source/Common/NetBuffer.*`.
- Ordered UDP behavior: `Source/Common/NetworkUdp.*`.
- Client transport abstraction and implementations: `Source/Client/NetworkClient*`.
- Server transport abstraction and implementations: `Source/Server/NetworkServer*`.
- Entity/property replication semantics: [EntityModel.md](EntityModel.md) and generated metadata/property code.
- Build feature toggles: [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).

## Validation checklist

1. Run UDP, client, and server network tests relevant to the changed transport, and `Test_SecureChannel` for any change that touches the byte stream a transport carries.
2. Validate both connect/accept and disconnect paths.
3. Validate partial receives and message framing when changing `NetInBuffer` / `NetOutBuffer`.
4. Validate hash resolution/debug-hash behavior across client and server builds.
5. Validate property synchronization when message layout or property-data serialization changes.
6. Validate platform-specific transport availability when touching ASIO/WebSocket/socket code.
