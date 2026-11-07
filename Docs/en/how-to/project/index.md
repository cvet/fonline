---
layout: default
title: Project Lifecycle
locale: en
document_id: project-lifecycle
permalink: /Docs/en/how-to/project/
---

# Project Lifecycle

This section follows a game repository from the first workstation to routine maintenance: prepare a machine, create the repository, work in an editor, keep quality gates green, run continuous integration, change saved data safely, move to a newer Engine revision, and release. Each page owns one task and links to the Engine contract it depends on instead of repeating it.

## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/fonline-tools/package.json`
- `Docs/en/how-to/build/embedding-project.md`
- `Docs/en/how-to/migration/engine-upgrade.md`
- `Examples/MinimalProject/README.md`

## Choose where to start

| Situation | Start with |
| --- | --- |
| A new machine, or a build fails before the first target | [Prepare a Workstation for an FOnline Project](workstation-setup.md), then [Troubleshoot Project Setup](../../troubleshooting/setup.md) |
| No repository yet | [Create an FOnline Game Repository](create-repository.md), after the [first project tutorial](../../tutorials/first-project.md) has succeeded once |
| Choosing between AngelScript and Managed C# | [Choose a Script Backend](../../explanation/scripting-runtime/choosing-a-backend.md) |
| Setting up the editor | [Set Up a VS Code Workspace for an FOnline Project](vscode-workspace.md) |
| Deciding where a piece of game data lives | [Choose How to Represent Game Data](../../explanation/entity-and-property-model/choosing-a-representation.md) |
| Formatting, analyzers, warnings and baselines | [Keep a Game Project Healthy](quality-gates.md) |
| Adding automated checks | [Add and Run Tests in a Game Project](project-tests.md) |
| Running checks on every change | [Set Up Continuous Integration for a Game Project](continuous-integration.md) |
| A property, prototype, enum value or text key must change | [Migrate Saved Data and Content in a Game Project](data-migrations.md) |
| Moving to a newer Engine revision | [Upgrade an Embedding Project](../migration/engine-upgrade.md) |
| Numbering and shipping builds | [Version and Release a Game Project](versioning-and-releases.md) |
| Working with AI coding agents | [Maintain a Project with AI Coding Agents](ai-assisted-maintenance.md) |

## The lifecycle at a glance

1. **Prepare.** Install the toolchain and verify it with the Engine-owned host check ([workstation setup](workstation-setup.md)).
2. **Create.** Add Engine as a pinned submodule, write the root CMake file, presets and main configuration, and reach one green configure, bake and smoke run ([create a repository](create-repository.md)).
3. **Work.** Use the editor integration and a task graph that always configures first ([VS Code workspace](vscode-workspace.md)).
4. **Guard.** Keep formatting, analysis, tests and generated-file checks in one local aggregate route and mirror it in CI ([quality gates](quality-gates.md), [project tests](project-tests.md), [continuous integration](continuous-integration.md)).
5. **Change data deliberately.** Every persisted identity change carries an explicit migration rule ([data migrations](data-migrations.md)).
6. **Update the Engine.** Record both repositories' revisions, audit the whole incoming range, apply migration notes oldest first, regenerate, and validate ([upgrade guide](../migration/engine-upgrade.md)).
7. **Release.** Produce a unique, machine-written project version and package from one commit ([versioning and releases](versioning-and-releases.md), [packaging](../release/packaging.md), [operations](../release/operations.md)).

## Routine maintenance checklist

Run these on a schedule that matches the project's pace; each item links to the page that explains it.

- Confirm the pinned Engine revision is intentional and note how far behind `master` it is; read the [changelog](../../reference/changelog.md) migration sections for the skipped range before moving ([upgrade guide](../migration/engine-upgrade.md)).
- Run the complete local aggregate route and compare it with what CI runs ([quality gates](quality-gates.md)).
- Review baselines and exemption files: every row has a written reason, and stale rows are removed ([quality gates](quality-gates.md)).
- Exercise the data migration tests against an isolated copy of production-shaped data before releasing a change that touches persisted names ([data migrations](data-migrations.md)).
- Re-run a clean-clone setup from the documented commands on one Windows and one Linux machine, and fix the documentation when it drifts ([workstation setup](workstation-setup.md), [create a repository](create-repository.md)).
- Rotate and verify backups and the recovery procedure ([backup and recovery](../release/backup-and-recovery.md)).
- Check that logs and generated output trees are ignored and not committed ([runtime log files](../../reference/runtime/log-files.md)).

## What stays in the game project

The Engine documents mechanisms and patterns. A game repository owns its concrete task names, tool choices, hosting, secrets, release channel policy, balance and content. Name those decisions in project documentation and link to the Engine page for the underlying mechanism, following [Embedding FOnline](../build/embedding-project.md#documentation-composition).
