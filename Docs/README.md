# FOnline Engine Documentation

Engine-owned documentation moved out of the Last Frontier project docs.

## Entry points

- [../README.md](../README.md) - human front door for the engine repository.
- [../AGENTS.md](../AGENTS.md) - AI-maintainer front door and working conventions.
- [../PUBLIC_API.md](../PUBLIC_API.md) - public native API notes.
- [../TUTORIAL.md](../TUTORIAL.md) - engine tutorial.

## Maintained engine docs

- [ClientUpdater.md](ClientUpdater.md) - client host/runtime split, runtime ABI, updater protocol, and server-side updater backend.
- [Debugging.md](Debugging.md) - native debugger support, stack traces, Visual Studio helpers, and client host/runtime validation.
- [Nullability.md](Nullability.md) - `T?` / `FO_NULLABLE` script/native boundary contract and analyzers.
- [MapperTools.md](MapperTools.md) - mapper-side automation and native mapper helper integration points.
- [WebDebugging.md](WebDebugging.md) - web target preparation, package layout, and debug workflow.
- [AndroidDebugging.md](AndroidDebugging.md) - Android target preparation, package layout, ADB workflow, and remote-scene debugging.

## Embedding-project references

Some docs intentionally reference `../../...` paths. Those are not part of the standalone engine repository; they point to the game project that embeds the engine as a submodule (for example Last Frontier).
