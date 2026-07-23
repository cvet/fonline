#nullable enable

namespace FOnline
{
    // Managed remote-call sending surface. Until the baker emits a typed per-call surface (the analogue of
    // AngelScript's player.ServerCall.X(...)), this is the entry point: name the outbound "cs" remote call and pass
    // its args. The engine validates the name against the outbound remote-call metadata for this side and
    // serializes the args via the shared RemoteCallWire byte format that the receiving side deserializes.
    public static class RemoteCall
    {
        // Send an outbound "cs" remote call to the remote peer (client->server or server->client). `caller` is the
        // entity the call is bound to (e.g. the Player whose connection carries it).
        public static void Send(Entity? caller, string name, params object[] args)
        {
            Native.SendRemoteCall(caller, name, args);
        }

        // Diagnostic/test: dispatch a "cs" remote call through the engine's real inbound path in-process (no network
        // peer), invoking the registered handler. Exercises the managed serialize -> deserialize -> dispatch glue.
        public static void Loopback(Entity? caller, string name, params object[] args)
        {
            Native.LoopbackRemoteCall(caller, name, args);
        }
    }
}
