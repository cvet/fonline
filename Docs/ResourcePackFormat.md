# Resource Pack Format

A logical resource pack selects one full `Pack.fores` and, optionally, one writable `Pack.patch.fores`.
The patch carries new payloads and a complete current catalog. Each entry selects bytes from the base or
patch; deleted paths disappear from that catalog. There are no patch chains, resource-set files or parts.
`Resources.foindex` is a disposable merged lookup cache over the selected pairs.

See [ClientUpdater.md](ClientUpdater.md) for synchronization,
[ConfigurationAndDataSources.md](ConfigurationAndDataSources.md) for mounting, and
[Essentials.md](Essentials.md) for filesystem primitives.

## Encoding and identities

All integer fields are explicitly sized little-endian values. Formats require exactly version **2.0**;
there are no readers for previous versions. Offsets and lengths are 64-bit. Catalog string-pool offsets,
path lengths and entry counts are 32-bit; decoded catalogs cannot exceed `UINT32_MAX` bytes.

Paths are unique, sorted by UTF-8 bytes and relative. Components cannot be empty, `.` or `..`; backslashes,
colons and NUL are rejected. Writers normalize input backslashes to `/` before validation. Payloads may
share an extent, including when a rename reuses existing content.

All hashes use FNV-1a 64, seed `0xcbf29ce484222325`, prime `0x100000001b3`:

- `PackHash`: physical bytes after the full base's header, including its encoded catalog.
- `FileContentHash`: decoded bytes of one resource.
- `ContentHash`: `uint32 entry_count`, then, for each sorted entry,
  `uint32 path_byte_length`, `uint64 decoded_size`, `uint64 FileContentHash`, and the UTF-8 path bytes.
- Header, catalog and footer checksums cover the byte ranges described below.

`ContentHash` excludes codec, source and offsets. Recompression changes physical identity without changing
logical identity. A patch binds to **physical** `BasePackHash`, because its base offsets must match that
exact artifact. These are integrity/change-detection hashes, not authentication.

## Full base

```text
[80-byte header][payload blobs][complete encoded catalog]
```

The header is uncompressed:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | Magic `0x53524F46` (`FORS`) |
| 4 | 2 | Major = 2 |
| 6 | 2 | Minor = 0 |
| 8 | 8 | `PackHash`, FNV over `[80, EOF)` |
| 16 | 8 | `IndexOffset` |
| 24 | 8 | `IndexStoredSize` |
| 32 | 8 | `IndexDecodedSize` |
| 40 | 4 | `IndexCodec` |
| 44 | 4 | `EntryCount` |
| 48 | 8 | `DataOffset` = 80 |
| 56 | 8 | `DataSize` |
| 64 | 8 | `ContentHash` |
| 72 | 8 | Header checksum, FNV over `[0, 72)` |

`IndexOffset = DataOffset + DataSize`, and the index ends exactly at EOF. There is no padding or reserved
space. Packaging streams encoded blobs and the physical hash, appends the catalog, then writes the header.
Canonical packaging also adds payloads in sorted path order. No timestamps are serialized.

## Catalog entries

A decoded catalog is `Entry[EntryCount]`, 48 bytes each, followed by its UTF-8 string pool without terminators.

| Entry offset | Bytes | Field |
|---|---|---|
| 0 | 4 | Path offset into the decoded catalog |
| 4 | 4 | Path byte length |
| 8 | 8 | Encoded payload offset in the selected file |
| 16 | 8 | Stored size |
| 24 | 8 | Decoded size |
| 32 | 4 | Codec: 0 = Stored, 1 = Deflate |
| 36 | 4 | Source: 0 = Base, 1 = Patch; other values rejected |
| 40 | 8 | `FileContentHash` |

Full-base entries require Source 0. A patch catalog can use either source. Base references fit the base
payload region; patch references start after its 32-byte header and end before the selected catalog.
References cannot reach the current catalog/footer or an uncommitted suffix. Stored resources must declare
equal stored/decoded sizes. Reads verify the exact decoded size and `FileContentHash`.

`ResourcePackSource` owns the current entry list and path strings and builds a lookup map whose keys reference
those strings. Decoded catalog bytes are released after parsing. File enumeration follows sorted paths.
All entries report the effective patch's modification time when a committed patch is selected, including
entries backed by base bytes. Without a patch they report the base time.

## Append-only patch

```text
[32-byte patch header]
[payloads 1][complete catalog 1][80-byte footer 1]
[payloads 2][complete catalog 2][80-byte footer 2]
...
```

The fixed header is never rewritten to publish an update:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | Magic `0x50524F46` (`FORP`) |
| 4 | 2 | Major = 2 |
| 6 | 2 | Minor = 0 |
| 8 | 8 | `BasePackHash` |
| 16 | 8 | Reserved = 0 |
| 24 | 8 | Header checksum, FNV over `[0, 24)` |

Every commit ends with this uncompressed footer:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | Magic `0x54524F46` (`FORT`) |
| 4 | 2 | Major = 2 |
| 6 | 2 | Minor = 0 |
| 8 | 8 | `BasePackHash` |
| 16 | 8 | Current `ContentHash` |
| 24 | 8 | Catalog offset |
| 32 | 8 | Catalog stored size |
| 40 | 8 | Catalog decoded size |
| 48 | 8 | Committed file size, including this footer |
| 56 | 4 | Catalog codec |
| 60 | 4 | Entry count |
| 64 | 8 | FNV of the encoded catalog |
| 72 | 8 | Footer checksum, FNV over `[0, 72)` |

The catalog must immediately precede its footer. Old payloads, catalogs and footers remain unused bytes
unless the current catalog references an old payload. Only the newest complete catalog is applied; earlier
catalogs are never layered beneath it.

`ResourcePatchWriter` plans offsets, reuses matching `(FileContentHash, DecodedSize)` from the base/current
patch, and encodes the next full catalog before writing. It validates each received resource, appends the
catalog, flushes, appends the footer, and flushes again. It persists the directory entry on POSIX platforms.
The writable directory is locked during mutation, through an OS lock rather than a stored selector or
journal. File writers also exclude other writers before truncation or append.

Readers retain their catalog and captured file bounds while a writer appends. With a valid previous commit,
`ResourcePatchWriter::Begin` truncates only the unfinished suffix after that committed end before appending.
A patch with no valid commit is removed and recreated under the directory lock. The updater removes a stale
patch whose base binding differs; the writer also recreates such a patch if a new append is needed.
A patch is never truncated underneath readers of a different base generation. On POSIX, readers can retain
an unlinked inode; Windows rejects deletion/replacement while incompatible readers still hold the file.

## Recovery and validation

Healthy reads inspect the header, footer at EOF, and that footer's catalog. An invalid EOF triggers a
backward scan in 64 KiB windows with footer-sized overlap. Candidate magic alone is insufficient: validate
version, base binding, absolute committed length, footer checksum, catalog extent/checksum and computed
logical hash. The latest valid candidate wins. No valid commit means the full base remains the local view.
A patch header that fails its own checks commits nothing either: a power loss can leave a created patch with
its length but not its header bytes, and refusing the file would keep the client from starting while a
reinstall never reaches the writable root. The base stays the view and the next update recreates the patch;
`Begin` flushes a new header before any payload so the window stays small. A valid header bound to a
different base is stale and excluded.

All extent checks subtract only after checking the minuend and widen table multiplication before use.
Unknown codecs/sources, noncanonical or duplicate paths and invalid pool references are rejected. Mounting
does not hash all payloads. Payload corruption is reported when read; received patch blobs are verified
before publication. Full downloads also verify the physical body hash and complete catalog before promotion.
The Python packager validates every decoded payload before accepting an archive, including exact stored/
decoded lengths, complete Deflate streams without trailing bytes, and each `FileContentHash`. It hashes
and decodes payloads in bounded chunks rather than allocating their declared decoded sizes.

An interrupted append may retransmit its uncommitted addition. Previously committed bytes remain reusable.
There is no persistent per-resource resume journal or configured patch-size limit. Dead blobs and old
catalogs can accumulate without triggering a full-base download. See the updater doc for full-base
installation/repair ordering when local data is missing or unusable.

## Codecs

Codec 0 stores bytes unchanged. Codec 1 is a zlib Deflate stream. Packaging takes the level (0–9) from
`Baking.CompressLevel` and the gain from `Baking.ResourcePackMinCompressGain` (default 5): a blob is deflated
only when the bytes it saves exceed `size * gain / 100` in integer division. Blobs smaller than 64 bytes
remain stored. Catalogs use the same encoding rule. Catalogs and caches the client writes itself (patch
catalogs, `.foindex`) use the `ResourcePackWriteSettings` defaults, level 6 and gain 5. Matching decoded
content can reuse an existing extent even when the server encoded it differently.

## Merged cache: `.foindex`

The cache contains references only. It records effective pairs from the configured suffix after the last
Embedded entry. Earlier packs and Embedded stay at their configured positions. It never mounts a patch as
an independent overriding pack.

Cache mounting verifies base headers and patch commit identities through the retained read handles. A
replacement between path resolution and opening invalidates the cache rather than mixing old offsets with
new backing bytes. Mounted readers retain their captured view across later appends and POSIX replacements.

The 72-byte header contains magic `0x58494F46` (`FOIX`), version 2.0, `PackListHash` at 8, index offset/stored/
decoded sizes at 16/24/32, codec at 40, entry count at 44, pack count at 48, and checksum over `[0,64)` at 64.
Decoded contents are 32-byte pack records, 56-byte entry records, then the string pool.

A pack record stores name offset/length (4 bytes each), base `PackHash` (8), committed patch catalog checksum
(8), and patch committed size (8). The last two fields are zero without a committed patch. `PackListHash`
folds each name's bytes followed by these three 64-bit values, in configured order.

An entry stores path offset/length, pack index and codec (four 4-byte fields), data offset, stored size,
decoded size and content hash (four 8-byte fields), source (4), and reserved zero (4).

Later configured packs win duplicate paths. The disk table is path-sorted; runtime enumeration groups winners
by descending pack precedence, then path, matching direct mounts. Entry times come from the effective pair.
Freshness includes base identity and patch commit identity, so a full refresh cannot keep stale offsets merely
because logical content is unchanged. Invalid cache data is discarded at the client boundary and authoritative
pairs are mounted. Rebuild writes a temporary cache and replaces the old one.

## Platforms and API

Android packages `.fores` without outer ZIP compression. The activity passes
`<installed APK>!/assets/<configured client resource directory>` as `Baking.ClientResources`, and its private files directory as
`Common.UserWritablePath`. `OpenResourcePackFile` locates the stored APK ZIP entry and returns a bounded,
64-bit positional `fs::disk_read_file` region over the APK. Compressed or encrypted outer entries are rejected.
`!/` separates an archive only where the text before it names an existing file, so an ordinary directory whose
name ends in `!` (a profile named `Bob!`) is never mistaken for one.
No complete pack or resource tree is copied into memory or staged into app storage for mounting.
Writable replacement bases and patches live under the private `Resources` directory.

Web retains the preloaded in-memory filesystem and can update pairs within that session. It does not persist
updates across page reloads and does not build a merged cache.

The main API is in `Source/Common/ResourcePack.h`: `ResourcePackWriter`, `ResourcePatchWriter`,
`ResourcePackSource`, `ReadResourcePackHeader`, `DecodeResourcePackIndex`, `ReadResourcePatchInfo`, and
`VerifyResourcePackFile`. `GetClientPackDirs`, `GetClientResourcePackPath` and `AddClientPackSource` in
`FileSystem.h` give bootstrap, updater and runtime the same base/patch selection.

`Test_ResourcePack.cpp` pins the shared Python/C++ golden bytes, payload validation, repeated append and
interrupted-footer recovery. `Test_ResourceIndex.cpp` covers pair-backed cached reads and APK region bounds;
`Test_ClientServerIntegration.cpp` exercises the real updater/backend lifecycle.
