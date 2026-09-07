# Resource Pack Format

Two formats. `.fores` is the engine resource pack: one file holding a fixed header, the payload blobs and the
index over them, and it replaces the ZIP artifact behind a resource pack. `.foindex` is the merged tree over
several packs - references only, built locally, disposable.

Two properties drive the layout:

- **The pack identity is one small read away.** The header carries `PackHash` at a fixed offset, so an updater
  builds its work list from header reads alone and never hashes a body it is not about to replace.
- **The index arrives in one contiguous read.** Mounting is a header read plus an index read; no directory walk
  and no per-entry seeking.

Related: `ConfigurationAndDataSources.md` for mounting, `ClientUpdater.md` for the sync, `Essentials.md` for the
`disk_read_file` / `disk_write_file` primitives this format is built on.

## Conventions

- All integers are little endian with explicit widths. There are no C++ structs or pointers on disk.
- All offsets and sizes are 64-bit and are validated against the file length before use.
- Paths are relative UTF-8 with `/` separators, normalized by the writer. A path appears at most once.
- Field widths are the limits: at most 2^32-1 entries, a path at most 2^32-1 bytes at an offset within a
  decoded index of at most 2^32-1 bytes, and blob offsets and sizes bounded only by the 64-bit file.
- Hashes are FNV-1a 64, seeded `0xcbf29ce484222325` with prime `0x100000001b3` — the digest the updater already
  computes over whole files. Nothing in this format is a security boundary; the delivery layer signs the pack
  list, and after that the local file is trusted (see the plan's decision on the trust boundary).

## File layout

```text
[0, 72)                       header
[DataOffset, +DataSize)       payload blobs, in index order
[IndexOffset, +IndexStoredSize)  index
```

`DataOffset` is 72 and the index follows the data, so the writer streams every blob out as it arrives and
appends the index once it knows the offsets.

Every extent in v1 is **committed**: there is no reserved space, no padding and no alignment requirement, and
the file ends where the index ends. The writer may preallocate the target (`disk_write_file::preallocate`) so a
long write fails early, but that is a transfer property and never leaves unused bytes in a finished pack. A
reader can therefore treat the file length as the outer bound of every extent, which is what the validation
below does. Reserved extents belong to the in-place diff work under Reserved for later.

## Header

72 bytes, always uncompressed.

| Offset | Size | Field | Meaning |
|--------|------|-------|---------|
| 0 | 4 | `Magic` | `0x53524F46`, the ASCII `FORS` |
| 4 | 2 | `VersionMajor` | 1. A different major means the fields mean something else; refuse the file |
| 6 | 2 | `VersionMinor` | 0. A higher minor stays readable |
| 8 | 8 | `PackHash` | FNV-1a 64 over `[72, end of file)`. The sync and divergence key |
| 16 | 8 | `IndexOffset` | Absolute offset of the stored index |
| 24 | 8 | `IndexStoredSize` | Index bytes on disk |
| 32 | 8 | `IndexDecodedSize` | Index bytes after decoding |
| 40 | 4 | `IndexCodec` | `0` stored, `1` deflate |
| 44 | 4 | `EntryCount` | Number of catalog entries |
| 48 | 8 | `DataOffset` | Absolute offset of the payload region |
| 56 | 8 | `DataSize` | Payload region length |
| 64 | 8 | `HeaderChecksum` | FNV-1a 64 over `[0, 64)` |

`HeaderChecksum` covers `PackHash` as well, so a header that survived a truncated or torn write is rejected
before any offset it carries is believed. `PackHash` deliberately excludes the header: the header is derived
from the body layout, and excluding it lets the writer hash the body as it streams instead of buffering it.

## Index

The index is one buffer, stored raw or deflated as `IndexCodec` says. Decoded, it is:

```text
Entry[EntryCount]     40 bytes each
StringPool            IndexDecodedSize - EntryCount * 40 bytes, UTF-8, not null terminated
```

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `PathOffset` into the decoded index buffer |
| 4 | 4 | `PathLength` in bytes |
| 8 | 8 | `DataOffset`, absolute file offset of the blob |
| 16 | 8 | `StoredSize`, blob bytes on disk |
| 24 | 8 | `DecodedSize`, blob bytes after decoding |
| 32 | 4 | `Codec`, `0` stored or `1` deflate |
| 36 | 4 | `Flags`, reserved, written as zero |

An entry carries no timestamp. `GetFileInfo` reports the pack file's own modification time for every entry,
so one pack is one epoch: consumers that cache by `(size, write_time)` re-read everything in a pack the updater
replaced and nothing in one it did not. Per-entry times would be a per-file cache key the format cannot honour
anyway, since replacing any blob rewrites the whole pack.

Entries are sorted by path, byte-wise, which makes the file canonical and enumeration an ordered walk. Lookup
is not a search over that order: `ResourcePackSource` builds an `unordered_map` at mount whose keys are
`string_view`s into the resident index buffer, so a path is hashed once and the sorted order costs the reader
nothing at lookup time. The payload region follows the order the writer was given, so a canonical file - one
whose bytes depend only on its contents - needs its paths added in sorted order; the packager does that, and
the golden vector in `Test_ResourcePack.cpp` pins both writers to the same layout.

Mounting therefore holds three things per pack, which is what a memory budget has to count: the decoded index
buffer, one `FileEntry` per entry, and the lookup map. The paths themselves exist once, in the buffer - the
entries and the map both point into it.

## Codecs

| Id | Name | Meaning |
|----|------|---------|
| 0 | `Stored` | The bytes as they are |
| 1 | `Deflate` | zlib stream, as produced by `compress2` |

A blob is deflated only when it gives back at least a configured minimum (default 5 %); otherwise it is stored
raw. The level is `Baking.CompressLevel`, passed to `compress2` as the zlib level 0-9, and the gain threshold
is `Baking.ResourcePackMinCompressGain`; both reach the engine writer as `ResourcePackWriteSettings`, so the
two writers take them from one place. The 64-byte floor is fixed rather than configured - below it the header
of a deflate stream costs more than the stream can save. So already-compressed data — audio, compressed textures, well-packed images — is never re-deflated and
costs nothing to read back. Blobs under 64 bytes are always stored. The same rule governs the index itself.

Codec choice is a writer input, not part of what the pack *contains*: re-encoding a blob differently changes
`PackHash` but not the file tree the pack presents.

## What a reader must validate

Structural validation is always on, because corruption, truncation and a wrong pairing are ordinary states.
Content hashes are **not** re-verified at mount or at read.

Before trusting anything: `Magic`, `HeaderChecksum`, `VersionMajor`, and that the file is at least 72 bytes.

Before reading the index: `DataOffset` and `IndexOffset` lie at or after the header and their extents fit the
file; `IndexDecodedSize >= EntryCount * 40`.

Per entry: the path lies inside the string pool and is not empty; the extent lies inside the payload region,
checked in an order that cannot overflow; a `Stored` entry has `StoredSize == DecodedSize`; the codec is known;
the path has not already been seen.

Per read: the decoded size matches what the entry declared. A mismatch is a corrupt payload, not a surprise.

Every failure throws. A file that claims the `FORS` magic and fails validation is never downgraded to an empty
source and never falls back to another artifact of the same pack.

## The merged tree: `.foindex`

`.fores` answers "what is in this pack". A client needs "where is this path", once, over every pack it has.
`.foindex` is that answer written down: one file naming every path the packs present and where its bytes live.
It holds no payload of its own - every entry points into a `.fores`.

It is built locally and is disposable. Nothing ships it, nothing downloads it, and deleting it costs one
rebuild. A client keeps it as `Resources.foindex` beside the packs under its writable root, and
`GetClientResources()` mounts it only when `IsResourceIndexCurrent()` says it still describes what is on disk,
falling back to mounting each pack otherwise.

### Header

72 bytes, always uncompressed, the same shape as a pack header so one reader habit covers both.

| Offset | Size | Field | Meaning |
|--------|------|-------|---------|
| 0 | 4 | `Magic` | `0x58494F46`, the ASCII `FOIX` |
| 4 | 2 | `VersionMajor` | 1 |
| 6 | 2 | `VersionMinor` | 0 |
| 8 | 8 | `PackListHash` | The divergence key, below |
| 16 | 8 | `IndexOffset` | Absolute offset of the stored index |
| 24 | 8 | `IndexStoredSize` | Index bytes on disk |
| 32 | 8 | `IndexDecodedSize` | Index bytes after decoding |
| 40 | 4 | `IndexCodec` | `0` stored, `1` deflate |
| 44 | 4 | `EntryCount` | Number of merged entries |
| 48 | 4 | `PackCount` | Number of packs the tree draws from |
| 64 | 8 | `HeaderChecksum` | FNV-1a 64 over `[0, 64)` |

### Index

One buffer, stored or deflated by the same rule a pack's index follows. Decoded, it is:

```text
Pack[PackCount]       16 bytes each
Entry[EntryCount]     40 bytes each
StringPool            UTF-8, not null terminated
```

A pack record is `NameOffset` (4), `NameLength` (4) and `PackHash` (8). It records the pack's **name**, never a
path, so an installed client that moved on disk still resolves; the hash is what proves the file found under
that name is the one that was merged.

An entry is `PathOffset` (4), `PathLength` (4), `PackIndex` (4), `Codec` (4), `DataOffset` (8), `StoredSize`
(8) and `DecodedSize` (8). The offset and sizes are the blob's inside the pack the index names, copied from
that pack's own index at build time - so a read is one hash probe here and one positional read there, with no
per-pack index consulted at runtime.

### The merge rule

Packs are folded in the configured order and the last one to declare a path wins, which is exactly the
precedence the per-pack mounts already have. Entries are sorted by path, as in a pack.

Neither format stores a per-file timestamp - one would cost writer determinism for nothing - so a read reports
the mtime of the `.fores` the bytes live in. The merged tree reports the owning pack's mtime rather than its
own, so the answer for one file does not change with which of the two views is mounted.

### Divergence and rebuild

`PackListHash` folds each pack's name and hash, in order. That one field answers the whole question: an edited
pack changes its `PackHash`, and an added, removed or reordered pack changes the sequence. Checking it costs
one 72-byte read per pack, not a mount.

The index is rebuilt whenever it does not describe what is on disk: it is missing, it fails header validation,
its `PackListHash` differs from the current pack list, or any pack it names is absent or carries a different
hash. There is no partial update - the rebuild reads the packs and replaces the file, writing through a
neighbouring temporary and renaming over the target, so an interrupted rebuild leaves the previous index
intact. A rebuild never writes into a `.fores`.

A reader that meets a stale index throws rather than falling back, because a merged tree that half-describes
the packs is worse than no tree: the caller's answer is to rebuild.

## Platforms

Two targets do not keep their packs in an ordinary directory, and both resolve it before the reader ever sees
them.

**Android** ships the packs inside the APK, where they are assets rather than files - and a pack is read by
positional file reads, which an asset does not answer. The activity therefore stages the whole resource tree
into the application's files directory on launch and points `Baking.ClientResources` at it. The staging is
keyed by the package's last-update time and repeats when the staged directory is empty or gone; it probes the
directory rather than one artifact inside it, because naming an artifact ties the check to a pack list and a
format, and a check that names the wrong file re-copies the tree on every start. The merged tree is written
into that staged directory and is discarded with it.

**Web** has the client resource directory preloaded into the Emscripten in-memory filesystem when the package
is built, and that filesystem does not survive a page reload. **The merged tree is therefore not built there at
all.** Its whole value is that one fold outlives the launch that paid for it, and on web nothing outlives the
launch - while building it means parsing every pack's index, which is precisely the work that mounting the
packs separately already does. Lookup does not suffer either way: `FileSystem` folds the snapshot of every
mounted source into one cross-source index, so a path resolves in one probe with or without the tree. Only the
build is skipped and not the mount, so a `.foindex` that ever arrives inside a package is still used.

Neither platform ever receives a `.foindex` over the wire; nothing ships or transfers one.

Preallocation is available (`disk_write_file::preallocate`, and see the ladder in
[Essentials.md](Essentials.md)) but the download target is deliberately not preallocated - sizing the file up
front would make every partial download look complete to the resume check. The transfer checks free space
instead; see [ClientUpdater.md](ClientUpdater.md).

## Reserved for later

The format is shaped so these are additive, not a version break:

- **Block-structured payloads.** A `Flags` bit plus a block offset table would let a reader seek into a
  compressed blob and decode only the blocks it needs. It pays on large blobs, and the measured corpus has
  more of those than a file count suggests: 70 % of files are under 64 KB but they hold 5 % of the bytes, so
  95 % of what a reader decodes lives in blobs a block table could seek into.
- **In-place diff updates.** Reserved extents, a per-blob content hash and a per-file content id would let an
  updater replace one blob instead of the pack. v1 replaces the whole pack on a `PackHash` mismatch.
- **Further codecs.** The codec id space is open.

## Engine API

`Engine/Source/Common/ResourcePack.h`:

- `ReadResourcePackHeader(path, header)` — header only, no index, no payloads.
- `VerifyResourcePackFile(path, expected_pack_hash)` — the one place a pack body is hashed: after a download,
  to prove the file carries the hash it was fetched for. The header hash is trusted from then on.
- `ResourcePackWriter` — streams blobs out as they arrive, appends the index, patches the header last. An
  abandoned writer removes its own half-written file.
- `ResourcePackSource` — a `DataSource`; the index is resident and every payload read is positional.

`DataSource::MountPack` probes `.fores` before `.zip`, so a pack that exists in both formats mounts as a resource pack.
