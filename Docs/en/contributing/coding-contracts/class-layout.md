---
layout: default
title: Class Layout and Result Use
locale: en
document_id: class-layout
permalink: /Docs/en/contributing/coding-contracts/class-layout.html
---

# Class Layout and Result Use

This contract applies to authored native Engine code and project-native extensions.
Third-party code retains its upstream conventions.

## Passive records and classes

Use `struct` only for passive records of public data. Do not declare constructors,
destructors, methods or member operators, including explicitly defaulted special
members and comparisons. They do not inherit behavior, enforce invariants or use
`final`. Default member initializers may describe the record's initial data.
Place functions that create, inspect, compare or modify records outside the
record as free functions; use nonmember operators where appropriate.

The passive-record restriction does not apply to `Source/Essentials/`.
Structs there may have constructors, methods and member operators, including
in foundational value types, internal data records and template/interface adapters.
Preserve their natural API; do not replace it with free functions or change
the keyword solely to satisfy the passive-record rule.

Settings types (including generated groups and their base/aggregate settings)
and all types marked `ExportValueType` are also exempt, in every native layer
and project extension. Retain their struct constructors, methods, member
operators and natural data API. For example, `mpos`, `msize` and `TextPackKey`
keep their member interfaces, and native settings retain `settings.Group.Name`.
An exception does not require converting an existing class to a struct.

When a type needs controlled state or a custom lifetime protocol,
use `class`. Encapsulate its representation, expose only the required public
interface and keep implementation details private. A keyword change followed by
making all fields public does not establish encapsulation. Choose the type from
its responsibilities; do not add getters merely to preserve a record's old
method syntax. Classes follow the method order and result-use rules below.

## Class method order

Place construction, assignment and destruction first. Follow them with an upper
public block containing queries about the object's state or represented data.
Then place operations, ordered from initialization to teardown. Preserve access
levels and keep related method families together within the appropriate block.
Preserve the existing relative order within each block; move only the methods
needed to establish these groups.
Nested types and aliases must remain available to declarations that use them.

An information method answers a question about the object: its properties,
contents, available entries, current status or related objects. It may return a
field, value, owned string/container, borrow, reference, span or iterator. Queries
can search, compare data, derive a value, read storage, lock or populate an internal
cache. These implementation details do not disqualify a query. Preconditions and
returning a mutable borrow or reference do not disqualify it either. A class
without such queries needs no information block.

Mutations, consuming reads/cursor advancement, resource creation, waiting for work
and substantial processing as the method's main purpose are operations. They
belong below the information block even when they are `const` or return a value.
Judge the logical contract and observable side effects, rather than treating every
lookup, allocation or I/O call as a separate operation. Read implementations and
callees when the purpose is unclear. Classify virtual declarations by their contract;
a trivial stub does not turn an operation into an information method.

For example, `CacheStorage::HasEntry()`, `GetString()`, `GetData()` and
`GetDataBounded()` request information, even when they search or read a file and
return allocated data. `ConfigFile::HasSection()` and `FileHeader::GetNameNoExt()`
also belong in the information block. `CacheStorage::SetDataChecked()` changes
stored data, `FileHeader::Copy()` explicitly creates a copy, and
`FileReader::ReadUInt8()` advances the reader: these remain operations.

## `[[nodiscard]]`

In `Source/Essentials/`, apply `[[nodiscard]]` wherever ignoring a result violates
the function's purpose or loses a required value, resource or failure status.
This includes suitable free functions, factories, conversion/arithmetic helpers
and I/O results. Fluent mutators and assignment/stream operators whose result is
optional chaining do not need the attribute merely because they return a value.

Outside Essentials, use `[[nodiscard]]` only on public instance methods in the
upper information block, and annotate all methods in that block. Preserve existing
query annotations by default; remove one only for a concrete semantic reason.
Remove it
from operations, free functions, static methods, nonpublic helpers, out-of-class
definitions and types. Do not move an operation into that block to justify an
attribute. A `Get`/`Is` name, return type, `const` or `noexcept` is not a classifier.
Mixed macro-generated method families must not acquire a blanket attribute when
some expansions mutate or perform operations.

When intentionally discarding an annotated result, use an explicit `(void)` cast
at the call site after checking that discarding it is valid. Do not suppress a
warning that exposes an unhandled failure or lost ownership.

Review declaration moves for template constraints, comments, conditional branches
and access levels. Preserve implementations, defaults and field order. Rebuild
native consumers together when virtual declarations move; the attribute and
declaration grouping do not change serialization or network formats.

Related contracts: [local variables](local-variables.md),
[exception safety](exception-safety.md) and
[Essentials](../../reference/native/essentials.md).
