# Resource Pack Format

`.fores` is the engine resource pack format: one file holding a fixed header, the payload blobs and the index
over them. It replaces the ZIP artifact behind a resource pack.

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

Entries are sorted by path, byte-wise. That makes enumeration a walk and lookup a binary search over the
resident buffer. The payload region follows the order the writer was given, so a canonical file - one whose
bytes depend only on its contents - needs its paths added in sorted order; the packager does that, and the
golden vector in `Test_ResourcePack.cpp` pins both writers to the same layout.

## Codecs

| Id | Name | Meaning |
|----|------|---------|
| 0 | `Stored` | The bytes as they are |
| 1 | `Deflate` | zlib stream, as produced by `compress2` |

A blob is deflated only when it gives back at least a configured minimum (default 5 %); otherwise it is stored
raw. The level is `Baking.CompressLevel`, passed to `compress2` as the zlib level 0-9, and the minimum saving
and the 64-byte floor are `ResourcePackWriteSettings` inputs, so both writers take them from the same place. So already-compressed data — audio, compressed textures, well-packed images — is never re-deflated and
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

## Reserved for later

The format is shaped so these are additive, not a version break:

- **Block-structured payloads.** A `Flags` bit plus a block offset table would let a reader seek into a
  compressed blob and decode only the blocks it needs. Only worth paying for on large blobs — most of the
  corpus is under 64 KB.
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
