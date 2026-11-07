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
| Auditing native code for allocation, pointer and local-variable rules | [Audit Native Source Rules in a Game Project](native-source-audits.md) |
| Auditing native entity cover and exception-safety levels | [Audit Entity Cover and Exception Safety in a Game Project](native-safety-audits.md) |
| Checking script bindings, handler nullability and AngelScript module hygiene | [Check Script Bindings and AngelScript Hygiene](script-and-binding-checks.md) |
| Enforcing Managed C# entry-point, invariant and script-static rules | [Engine-Contract Analyzers for Managed C# Projects](engine-contract-analyzers.md) |
| Checking Markdown links and anchors | [Check Documentation Links in a Game Project](documentation-checks.md) |
| Adding automated checks | [Add and Run Tests in a Game Project](project-tests.md) |
| Writing script-level gameplay tests | [Script Test Harness Pattern for AngelScript and Managed C#](script-test-harness.md) |
| Running checks on every change | [Set Up Continuous Integration for a Game Project](continuous-integration.md) |
| A property, prototype, enum value or text key must change | [Migrate Saved Data and Content in a Game Project](data-migrations.md) |
| Proving that every renamed or removed identity has a rule | [Guard Identity Migrations](migration-guard.md) |
| Formatting `.fomain`, prototype and `.fos` files | [Format Game Content Files](format-content.md) |
| Linting text packs, analyzing movement traces, locating the generated C# project | [Text Pack, Move Sync and Managed Project Tools](content-and-diagnostics-tools.md) |
| Reading configuration, prototypes and text packs from a project tool | [Read Engine Content Formats from Project Tools](reading-engine-formats.md) |
| Moving to a newer Engine revision | [Upgrade an Embedding Project](../migration/engine-upgrade.md) |
| Numbering and shipping builds | [Version and Release a Game Project](versioning-and-releases.md) |
| Generating `VERSION`, preflighting a release, writing package checksums, recording an Engine update | [Release Tooling for a Game Project](release-tooling.md) |
| Working with AI coding agents | [Maintain a Project with AI Coding Agents](ai-assisted-maintenance.md) |
| Letting an AI agent observe and play a development build | [Drive a Game with the AiControl MCP Adapter](ai-control-mcp.md) |

## The lifecycle at a glance

1. **Prepare.** Install the toolchain and verify it with the Engine-owned host check ([workstation setup](workstation-setup.md)).
2. **Create.** Add Engine as a pinned submodule, write the root CMake file, presets and main configuration, and reach one green configure, bake and smoke run ([create a repository](create-repository.md)).
3. **Work.** Use the editor integration and a task graph that always configures first ([VS Code workspace](vscode-workspace.md)). An AI agent can observe and play a development build through the Engine's [AiControl MCP adapter](ai-control-mcp.md).
4. **Guard.** Keep formatting, analysis, tests and generated-file checks in one local aggregate route and mirror it in CI ([quality gates](quality-gates.md), [project tests](project-tests.md), [continuous integration](continuous-integration.md)). The Engine ships the checks; the project passes its own scopes, configuration and baselines:
   - quality tools: [native source audits](native-source-audits.md), [entity cover and exception-safety audits](native-safety-audits.md), [script binding and AngelScript hygiene checks](script-and-binding-checks.md), [documentation link checks](documentation-checks.md) and the [script test harness](script-test-harness.md);
   - analyzers: the [engine-contract analyzers](engine-contract-analyzers.md) for Managed C#;
   - content tools: [content formatting](format-content.md), the [text-pack lint, movement-trace analyzer and managed project locator](content-and-diagnostics-tools.md), and the [format reader](reading-engine-formats.md) for project tools.
5. **Change data deliberately.** Every persisted identity change carries an explicit migration rule ([data migrations](data-migrations.md)), and the [migration guard](migration-guard.md) fails a change that drops an identity without one.
6. **Update the Engine.** Record both repositories' revisions, audit the whole incoming range, apply migration notes oldest first, regenerate, and validate ([upgrade guide](../migration/engine-upgrade.md); `engine_upgrade_report.py` drafts the record, see [release tooling](release-tooling.md#prepare-an-engine-update-record)).
7. **Release.** Produce a unique, machine-written project version and package from one commit ([versioning and releases](versioning-and-releases.md), [release tooling](release-tooling.md), [packaging](../release/packaging.md), [operations](../release/operations.md)).

## Routine maintenance checklist

Run these on a schedule that matches the project's pace; each item links to the page that explains it.

- Confirm the pinned Engine revision is intentional and note how far behind `master` it is; read the [changelog](../../reference/changelog.md) migration sections for the skipped range before moving ([upgrade guide](../migration/engine-upgrade.md)).
- Run the complete local aggregate route and compare it with what CI runs ([quality gates](quality-gates.md)).
- Review baselines and exemption files: every row has a written reason, and stale rows are removed ([quality gates](quality-gates.md)).
- Run the native source audits, the entity cover and exception-safety audits, and the script binding and AngelScript hygiene checks on the project scopes; each fails on a stale baseline row, so delete rows whose findings were fixed ([native source audits](native-source-audits.md), [native safety audits](native-safety-audits.md), [script checks](script-and-binding-checks.md)).
- Run `format-content --check` and the text-pack lint so formatting drift and translation gaps do not accumulate ([content formatting](format-content.md), [text packs](content-and-diagnostics-tools.md#lint-text-packs-before-baking)).
- Run the migration guard with `--check` against the committed identity manifest ([migration guard](migration-guard.md)).
- Run the documentation link check over the project's Markdown, including links into the Engine after an update ([documentation checks](documentation-checks.md)).
- Exercise the data migration tests against an isolated copy of production-shaped data before releasing a change that touches persisted names ([data migrations](data-migrations.md)).
- Re-run a clean-clone setup from the documented commands on one Windows and one Linux machine, and fix the documentation when it drifts ([workstation setup](workstation-setup.md), [create a repository](create-repository.md)).
- Rotate and verify backups and the recovery procedure ([backup and recovery](../release/backup-and-recovery.md)).
- Check that logs and generated output trees are ignored and not committed ([runtime log files](../../reference/runtime/log-files.md)).

## What stays in the game project

The Engine documents mechanisms and patterns. A game repository owns its concrete task names, tool choices, hosting, secrets, release channel policy, balance and content. Name those decisions in project documentation and link to the Engine page for the underlying mechanism, following [Embedding FOnline](../build/embedding-project.md#documentation-composition).
