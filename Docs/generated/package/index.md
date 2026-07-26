---
title: Generated Package Interface
document_id: generated-package-index
locale: en
generated: true
---

# Generated Package Interface

> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.

[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../package.json)

This reference joins the CMake package declaration with the runtime-consumed packager contract. It describes engine capabilities, not an embedding project's release matrix.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Not declared |
| Support policy | No versioned package support line is declared; embedding projects must pin an engine revision. |
| Manifest | [BuildTools/PackageInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/PackageInterface.json) |
| Packager | [BuildTools/package.py](https://github.com/cvet/fonline/blob/master/BuildTools/package.py) |
| Contract digest | <code>31774f63bbfa43d10005d90b4755ff6ae3166406807e49bafb2e7757eb40a55d</code> |

## Coverage

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Declaration](declaration.md) | 2 | CMake clauses and per-binary modifiers. |
| [Targets/platforms/packs](matrix.md) | 5 / 6 / 19 | Accepted runtime dimensions and support status. |
| [Payloads and artifacts](payloads.md) | 8 | Implemented output-producing pack tokens. |
| [Packager CLI](cli.md) | 13 | Exact internal package.py invocation contract. |

## Boundary

Included:

- DefinePackage declaration clauses and the per-binary POSTFIX modifier
- package.py targets, platforms, architecture keys, pack tokens, and payload effects
- implemented, placeholder, and unsupported package boundaries

Excluded from this slice:

- embedding-project package matrices and release policy
- project configuration key schema and secret provisioning
- resource-pack content selection and deployment topology
- installer signing credentials and external tool operation
