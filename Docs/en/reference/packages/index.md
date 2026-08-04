---
title: Generated Package Interface
document_id: generated-package-index
locale: en
generated: true
---

# Generated Package Interface

> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.

[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../../../generated/package.json)

This reference joins the CMake package declaration with the runtime-consumed packager contract. It describes engine capabilities, not an embedding project's release matrix.
This generated package interface is the entry point for `DefinePackage` grammar, `package.py` targets, platforms, pack tokens, and payload effects.

## Cross-contract decision

Use this cross-contract sequence:

1. State the package boundary explicitly: pin an exact Engine revision and use `BuildTools/PackageInterface.json` as the `internal` package contract; do not replace this statement with a generic revision-pinned heading.
2. Treat accepted package.py dimensions and payload effects as capability only. Embedding-project package matrices, release policy, secret provisioning, deployment topology, and installer signing credentials remain excluded and project-owned.
3. Read support separately: `build_gated`, `smoke_gated`, `source_capable`, and `not_in_public_matrix` are distinct states.
4. For each example, report source status, remote visibility/state, observed required checks, exact Engine pin, update-delivery policy, and matching Contract digest together. Only a `published` source with a `public` / `published` remote and `passing` observed checks is publication evidence; a private, reserved, source-staged, planned, or unobserved repository is not.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Not declared |
| Support policy | No versioned package support line is declared; embedding projects must pin an engine revision. |
| Manifest | [BuildTools/PackageInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/PackageInterface.json) |
| Packager | [BuildTools/package.py](https://github.com/cvet/fonline/blob/master/BuildTools/package.py) |
| Contract digest | <code>b8c9e04bf8c3d34d8992bd1176346da7d2489d2c5315ce96a82ae0f604d90a8a</code> |

## Coverage

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Declaration](declaration.md) | 2 | CMake clauses and per-binary modifiers. |
| [Targets/platforms/packs](matrix.md) | 6 / 6 / 19 | Accepted runtime dimensions and support status. |
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
