# Retained collection-event integration, 10 October 2026

## State

Validation accepted. Merge retained `24da100693cc0240c3f8d09e58178af3bf23df43`
and published `cfa468f17bd5dff82cebc6f05b568620fc34163c` without rewriting
either history. The merge owns `2026.1.22-dev`; both parents own version 21.
The published incoming range from `8b6a5f6020` contains one commit.
The embedding project starts at `22d9c29be8ba5a24829015f065e7fb10c0aa817f`;
its incoming tip is `c4014a656babcffccef3a5d067ed5302cb670a29`.
Publication is outside this task.

## Complete incoming review

- Array proxies resolve current container storage instead of retaining conversion
  temporaries. Dictionary proxies refresh current entries on size access and
  invalidate the view after additions/clears. Const writes still throw.
- Mutable Managed event lists/dictionaries convert every source entry before
  clearing the native destination, then use its accessor. Following subscribers
  observe replacement, edits and clearing; scalar copyback is unchanged.
- Database-document contexts use non-null borrowed pointers without changing
  synchronous lifetime, reads, exception propagation or migration staging.
- Restore callers explicitly discard the migration change flag while persisting
  its updates. Remaining source edits apply the owning formatter.
- Native proxy tests cover container-owned addresses, vector growth, duplicate
  dictionary insertion, native iteration order and const rejection. The optional
  Engine-source event probe adds real boxing, GC, context, shutdown and a matching
  negative control; it requires a configured GCC/Clang Ninja Managed unit build.
- Callback probes adopt cached native-worker attachment behavior and validate
  detach at thread exit. Link input discovery excludes compiler options.
- The reviewed full AI context limit grows by 32 KiB to 2,260,992 bytes;
  complete documents, membership and migration history remain required.

## Acceptance

- [x] Read the complete changed native/runtime sources and affected tests/docs
- [x] Preserve both version 21 histories and prepare the next version
- [x] Version checks, prepared models, translation and complete documentation tests
- [x] Aggregate comparison of all 17 contract domains against the published tip
- [x] Native collection-proxy unit tests and supported Managed callback probes
- [x] Rendered site/artifacts, diagram regression and full browser audit
The final merge commit must preserve both parent histories.

Embedding bake, binaries, gameplay and ordinary acceptance remain project-owned
checks. This plan does not use project tests as proof of a reusable contract.
Safety copies are retained until both repositories qualify.

## Recorded evidence

- Prepared corpus: 413 Markdown entries; 569 documentation tests and 11 version
  tests pass. All 17 published contract domains have zero differences.
- Native Managed-enabled unit build passes; the three real collection-proxy
  cases pass all 32 assertions. The first launcher used an incorrect output
  directory after a successful build; the next launcher uses the actual CMake
  output without changing or rebuilding the qualified source inputs.
- Callback regression files: 26 pass, 13 skip. Linux embedding, Makefiles and
  GCC/Clang probes are unavailable on this Windows/MSVC host. In particular,
  skipped boxed-collection probes do not establish native writeback acceptance.
- Diagram regression and rendered artifacts pass. Pinned Chromium checks all
  592 routes in three profiles: 1776 pages, 15 interaction profiles and
  23 screenshots, with no findings. This evidence update touches only the
  internal plan and its manifest entry; the final rendered HTML, scripts,
  styles and visual assets must remain byte-identical to that browser run.
- Reusable sources, source tests, models and retained release history remain
  unchanged during this final evidence reconciliation.
