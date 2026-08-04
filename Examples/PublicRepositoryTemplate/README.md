# Public Repository Governance Overlay

This directory is the Engine-owned publication overlay for FOnline public example repositories. It is not a standalone example and must not be published with template placeholders intact.

The publication owner combines an approved example source tree with the files mapped by `Examples/PublicRepositories.json`, renames template files to their recorded destinations, replaces every `{{PLACEHOLDER}}`, and adds `Engine/` as a git submodule at the exact revision recorded in `example-repository.json`.

Before the first push, run from the candidate repository root:

```bash
python Engine/BuildTools/docs_examples.py --verify-repository . --engine-mode pinned
```

The pinned workflow protects released behavior. The scheduled current-Engine workflow checks forward compatibility without changing the released pin. A current-Engine failure produces a reviewed update task or pull request; it never silently moves a release artifact to a floating Engine branch.

Human policy and repository-specific exit gates are documented in [Public Example Repositories](../../Docs/en/how-to/build/public-example-repositories.md). The machine source is [PublicRepositories.json](../PublicRepositories.json).
