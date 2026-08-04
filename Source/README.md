---
layout: default
title: FOnline Engine Source
permalink: /Source/README.html
locale: en
document_id: source-readme
---

# FOnline Engine source tree

- `Applications/` - executable entry points for generated build targets.
- `Client/` - client-specific runtime used by the game client and editor tools.
- `Common/` - runtime code shared by the client, server, and editor tools.
- `Tools/` - Mapper, baker, viewer, and developer-tool implementations.
- `Server/` - server-specific runtime also embedded by editor and test targets.
- `Scripting/` - script backends, code generation support, and reusable core scripts.
- `Tests/` - deterministic native engine tests and their source-level entrypoint.

## Deeper source navigation

For the maintained guide to entry points, dependency direction, and layer
ownership, see [Source Tree](../Docs/en/contributing/source-tree/),
[Architecture](../Docs/en/explanation/architecture/), and
[Applications](../Docs/en/reference/applications.md).
