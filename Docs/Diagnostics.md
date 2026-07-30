# Diagnostics

## Ownership and availability

The reusable native instrumentation module lives in [../Source/Essentials/Diagnostics.h](../Source/Essentials/Diagnostics.h) and [../Source/Essentials/Diagnostics.cpp](../Source/Essentials/Diagnostics.cpp). Both files belong to `EssentialsLib`, and `Essentials.h` always includes `Diagnostics.h` as its last include. Client, server, mapper, engine tools, tests, and embedding extensions can therefore use the API without adding an include, while the module may build probes on every Essentials facility, including sockets.

The module itself is permanent engine code, but it owns no ambient recorder, settings, history, trigger poller, or `BaseEngine` state. Runtime cost appears only when a caller explicitly creates an instrument or calls a function. Investigation-specific call sites remain temporary and must be removed before integration.

All public names are `snake_case` under `diagnostics`.

## Instruments

All APIs are in `diagnostics` inside the engine namespace:

| Instrument | Use |
|---|---|
| `here(name)` | Log the checkpoint name, source file, line, and function using `std::source_location`. |
| `checkpoint(name, format, args...)` | Write a formatted state checkpoint through the normal engine log. |
| `stack_trace()` / `stack_checkpoint(name)` | Return or print resolved native frames plus active script stack layers. |
| `thread_checkpoint(name)` | Print the current engine thread name. |
| `debugger_attached()` / `debug_break()` / `debug_break_if(condition)` | Query the native debugger and break only when one is attached. |
| `capture_process()` / `process_checkpoint(name)` | Return or print the process id and resident/private memory usage. |
| `capture_memory_delta(before, after)` / `scoped_memory_delta` | Calculate or automatically log resident/private process-memory changes around a scope. |
| `hex_dump(data, max_bytes, bytes_per_line)` / `hex_checkpoint(...)` | Return or print a bounded hexadecimal and ASCII packet/buffer view. `max_bytes == 0` selects the complete input. |
| `write_text_dump` / `write_binary_dump` / `write_hex_dump` | Write an investigation artifact and create missing parent directories. |
| `scoped_timer` | RAII scope timing with `elapsed`, named `lap`, a reporting threshold, `cancel`, and optional safe stack output during exception unwinding. |
| `one_shot_trigger` | Consume a non-empty environment variable once across threads and return a copied payload. |
| `hit_counter` | Count a hot boundary while reporting only the first N and then every Mth hit. |
| `rate_counter` | Count weighted events in a time window, periodically log events per second, and expose a current non-resetting snapshot. |
| `duration_statistics` | Aggregate count, total, minimum, maximum, latest, and average durations across threads. |
| `scoped_duration` | Automatically add one scope duration to a `duration_statistics` instance unless cancelled. |
| `change_detector<T>` | Report whether a watched value changed since the previous observation. |
| `fault_injector` / `inject_delay` / `corrupt_bytes` | Select deterministic hits, introduce latency, or XOR a bounded byte range. The caller decides whether an injected hit means return-error, drop, disconnect, or another domain action. |
| `udp_sender` | Send binary buffers or text messages to an explicit UDP endpoint for inspection by Wireshark, `nc`, or a purpose-built listener. |
| `pcap_writer` | Write explicitly supplied Ethernet, raw-IP, or user-link packets to a flushed classic-PCAP file readable by Wireshark. |
| `timeout_watchdog` | Run an explicit callback, optional debugger break, and log message when a scope-owned deadline expires. |
| `event_history` | Keep a thread-safe bounded history with channels and elapsed timestamps; capture stack entries, take a snapshot, dump it, or clear it. |

Example:

```cpp
// TEMP_DIAGNOSTIC: remove after investigation.
diagnostics::scoped_timer load_map_timer {"load-map"};
diagnostics::checkpoint("load-map", "map={}, entities={}", map_id, entity_count);

if (invalid_state) {
    diagnostics::stack_checkpoint("invalid-map-state");
}
```

Packet and external-listener example:

```cpp
// TEMP_DIAGNOSTIC: remove after investigation.
diagnostics::hex_checkpoint("incoming-login", packet);

static diagnostics::udp_sender diagnostic_output {"127.0.0.1", 43020};
(void)diagnostic_output.send_bytes(packet);
```

`udp_sender` starts the platform socket layer and binds an ephemeral local UDP socket when constructed. It never opens a listener or retries; `is_ready()` and the returned byte count make setup and send failures visible. A send is one datagram, so keep each payload below the practical path MTU when packet boundaries matter.

For a boundary called frequently, use `hit_counter`, `rate_counter`, or `change_detector<T>` to reduce log volume. Feed existing elapsed values into `duration_statistics`; use `scoped_timer` for one scope. Use `event_history` when the useful evidence is the sequence before a failure. Use Tracy instead of log timers or aggregators for sustained profiling or frame-by-frame measurements.

Aggregated timing and memory example:

```cpp
// TEMP_DIAGNOSTIC: remove after investigation.
static diagnostics::duration_statistics decode_times {"packet-decode"};
diagnostics::scoped_duration decode_sample {decode_times};
diagnostics::scoped_memory_delta decode_memory {"packet-decode", 64 * 1024};
```

`scoped_duration` records on destruction; both scope tools support `cancel()` for abandoned paths. Memory readings are process-wide and may move in either direction because other threads and allocator housekeeping remain visible.

Fault and capture example:

```cpp
// TEMP_DIAGNOSTIC: remove after investigation.
static diagnostics::fault_injector packet_fault {"login-packet", 5};
static diagnostics::pcap_writer packet_capture {"Workspace/login.pcap", diagnostics::pcap_link_user0};

(void)packet_capture.write_packet(packet);

if (packet_fault.hit()) {
    diagnostics::inject_delay(std::chrono::milliseconds {250});
    (void)diagnostics::corrupt_bytes(packet, 0, 1);
}
```

`fault_injector` is a deterministic hit selector, not a hidden policy engine: an injected hit becomes a simulated error or packet drop only when the instrumented caller explicitly returns the error or skips the send. `one_shot_trigger` remains the right companion when activation must be controlled by an environment variable during reproduction.

`pcap_writer` flushes the global header and every record so a crash does not discard the useful prefix. `pcap_link_user0` is the safe default for arbitrary engine payloads; select `pcap_link_ethernet` or `pcap_link_raw_ip` only when the supplied bytes really contain that complete link-layer format.

Watchdog example:

```cpp
// TEMP_DIAGNOSTIC: remove after investigation.
diagnostics::timeout_watchdog watchdog {
    "server-start",
    std::chrono::seconds {5},
    [] { diagnostics::process_checkpoint("server-start-timeout"); },
};
```

The callback runs on the engine thread pool, not on the stalled thread. It can dump process-wide or externally synchronized state, but it cannot safely inspect unsynchronized local variables or manufacture the stalled thread's native stack. The watchdog destructor cancels and joins its task; normal scope exit therefore leaves no background callback behind.

## Cleanup contract

Temporary call sites must carry `TEMP_DIAGNOSTIC`. Environment triggers should use an embedding-specific temporary prefix. Before integration, remove:

- all `diagnostics::` objects and calls;
- temporary environment variables and launch arguments;
- context-specific helpers added only for the probe.

Embedding projects should enforce this with a static source check. The maintained module implementation and its focused unit test are exact-path exceptions because they define and verify the toolbox rather than activate a product call site.

If a discovered signal deserves to remain, replace the temporary probe with a deliberately designed low-noise log, assertion, test, metric, or Tracy zone owned by the affected subsystem.

## Relationship to permanent facilities

- [Debugging.md](Debugging.md) owns native/script debugger setup and the underlying stack-trace API.
- Permanent engine logging remains in `Source/Essentials/Logging.*`.
- Ordinary filesystem and debugger primitives remain in `DiskFileSystem.*` and `BasicCore.*`; Diagnostics only packages temporary investigation patterns.
- Socket transport remains in `Source/Essentials/NetSockets.*`; `udp_sender` is only a small explicit diagnostic adapter.
- Tracy remains the sustained profiler.
- Crash self-tests and fatal reporting remain separate because they validate failure handling rather than investigate ordinary state.
