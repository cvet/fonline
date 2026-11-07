---
title: Generated Helper CLI Reference
document_id: generated-helper-cli-index
locale: en
generated: true
---

# Generated Helper CLI Reference

> Generated reference. Do not edit this page directly. Update `BuildTools/HelperCliInterface.json` or the owning executable parser, then run `python BuildTools/docs_helper_cli.py --write`.

[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../../../generated/helper-cli.json) | [Generation contract](../metadata/)

This reference is generated from the `argparse.ArgumentParser` objects used by executable engine helper scripts. The manifest owns purpose and audience; source parsers own executable syntax.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Not declared |
| Support policy | Helper command lines are revision-pinned implementation interfaces; automation must pin an engine revision. |
| Source manifest | [BuildTools/HelperCliInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/HelperCliInterface.json) |
| Contract digest | <code>b5d6391cbb8dc43b7a7d75ae83e85381cb8af54396fb805b33ace1359248e950</code> |

## Inventory

| Stable ID | Helper | Owner | Invocation owner | Parser source | Commands / global args |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codegen-60abdf415d"></a><code>helper-cli.codegen</code> | [Code generation](commands.md#entry-helper-cli-codegen-60abdf415d) | <code>build-release</code> | BuildTools/cmake/stages/Codegen.cmake | [BuildTools/codegen.py](https://github.com/cvet/fonline/blob/master/BuildTools/codegen.py) | 0 / 10 |
| <a id="entry-helper-cli-codecoverage-b014400e5e"></a><code>helper-cli.codecoverage</code> | [Code coverage](commands.md#entry-helper-cli-codecoverage-b014400e5e) | <code>quality</code> | BuildTools/cmake/stages/Applications.cmake | [BuildTools/codecoverage.py](https://github.com/cvet/fonline/blob/master/BuildTools/codecoverage.py) | 4 / 0 |
| <a id="entry-helper-cli-gameplay-test-runner-b34ed8deb4"></a><code>helper-cli.gameplay-test-runner</code> | [Gameplay test runner](commands.md#entry-helper-cli-gameplay-test-runner-b34ed8deb4) | <code>quality</code> | embedding-project CMake targets and CI gameplay smoke jobs | [BuildTools/gameplay_test_runner.py](https://github.com/cvet/fonline/blob/master/BuildTools/gameplay_test_runner.py) | 0 / 3 |
| <a id="entry-helper-cli-ai-control-client-35184e9731"></a><code>helper-cli.ai-control-client</code> | [AiControl protocol client](commands.md#entry-helper-cli-ai-control-client-35184e9731) | <code>tooling</code> | embedding-project AI adapters, protocol smoke tests, and direct developer diagnostics | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) | 5 / 5 |
| <a id="entry-helper-cli-format-content-55dd5b4af8"></a><code>helper-cli.format-content</code> | [Content formatter](commands.md#entry-helper-cli-format-content-55dd5b4af8) | <code>tooling</code> | BuildTools/buildtools.py format-content, embedding-project format tasks and CI drift checks | [BuildTools/format_content.py](https://github.com/cvet/fonline/blob/master/BuildTools/format_content.py) | 0 / 8 |
| <a id="entry-helper-cli-fo-formats-9aa60fdd1b"></a><code>helper-cli.fo-formats</code> | [Engine content format reader](commands.md#entry-helper-cli-fo-formats-9aa60fdd1b) | <code>tooling</code> | embedding-project content tools and direct developer diagnostics | [BuildTools/fo_formats.py](https://github.com/cvet/fonline/blob/master/BuildTools/fo_formats.py) | 3 / 0 |
| <a id="entry-helper-cli-migration-guard-3e5c9b3e72"></a><code>helper-cli.migration-guard</code> | [Identity migration guard](commands.md#entry-helper-cli-migration-guard-3e5c9b3e72) | <code>quality</code> | embedding-project local validation tasks and CI content gates | [BuildTools/migration_guard.py](https://github.com/cvet/fonline/blob/master/BuildTools/migration_guard.py) | 0 / 11 |
| <a id="entry-helper-cli-ai-control-mcp-f5daba9e92"></a><code>helper-cli.ai-control-mcp</code> | [AiControl MCP adapter](commands.md#entry-helper-cli-ai-control-mcp-f5daba9e92) | <code>tooling</code> | embedding-project MCP client configurations, project tool modules, and AI playtest runners | [BuildTools/ai_control_mcp/cli.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_mcp/cli.py) | 0 / 14 |
| <a id="entry-helper-cli-windows7-import-check-a0c7e4cb59"></a><code>helper-cli.windows7-import-check</code> | [Windows 7 import validation](commands.md#entry-helper-cli-windows7-import-check-a0c7e4cb59) | <code>quality</code> | embedding-project Windows 7 CI and release validation | [BuildTools/check_windows7_imports.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_windows7_imports.py) | 0 / 2 |
| <a id="entry-helper-cli-markdown-links-dd3ef679c3"></a><code>helper-cli.markdown-links</code> | [Markdown link check](commands.md#entry-helper-cli-markdown-links-dd3ef679c3) | <code>quality</code> | embedding-project documentation wrappers, local tasks, and CI documentation jobs | [BuildTools/markdown_links.py](https://github.com/cvet/fonline/blob/master/BuildTools/markdown_links.py) | 0 / 10 |
| <a id="entry-helper-cli-native-source-audit-b6c759ee81"></a><code>helper-cli.native-source-audit</code> | [Native source audits](commands.md#entry-helper-cli-native-source-audit-b6c759ee81) | <code>quality</code> | Engine validation CI, embedding-project native audit wrappers and direct developer use | [BuildTools/native_source_audit.py](https://github.com/cvet/fonline/blob/master/BuildTools/native_source_audit.py) | 5 / 0 |
| <a id="entry-helper-cli-native-safety-audit-2f4598f1d2"></a><code>helper-cli.native-safety-audit</code> | [Native safety audits](commands.md#entry-helper-cli-native-safety-audit-2f4598f1d2) | <code>quality</code> | Engine validate workflow, embedding-project native validation tasks and CI | [BuildTools/native_safety_audit.py](https://github.com/cvet/fonline/blob/master/BuildTools/native_safety_audit.py) | 2 / 0 |
| <a id="entry-helper-cli-android-device-ab99179ae9"></a><code>helper-cli.android-device</code> | [Android device control](commands.md#entry-helper-cli-android-device-ab99179ae9) | <code>platform</code> | embedding-project Android tasks and direct developer use | [BuildTools/android_device.py](https://github.com/cvet/fonline/blob/master/BuildTools/android_device.py) | 7 / 1 |
| <a id="entry-helper-cli-simple-web-server-58fbf70798"></a><code>helper-cli.simple-web-server</code> | [Local web server](commands.md#entry-helper-cli-simple-web-server-58fbf70798) | <code>platform</code> | BuildTools/package.py WebServer payload | [BuildTools/web/simple-web-server.py](https://github.com/cvet/fonline/blob/master/BuildTools/web/simple-web-server.py) | 0 / 2 |
| <a id="entry-helper-cli-artifact-checksums-efe1b1aaa6"></a><code>helper-cli.artifact-checksums</code> | [Artifact checksums](commands.md#entry-helper-cli-artifact-checksums-efe1b1aaa6) | <code>build-release</code> | embedding-project release jobs after the last package writer stops | [BuildTools/artifact_checksums.py](https://github.com/cvet/fonline/blob/master/BuildTools/artifact_checksums.py) | 0 / 6 |
| <a id="entry-helper-cli-project-version-52ce8849a0"></a><code>helper-cli.project-version</code> | [Project version generator](commands.md#entry-helper-cli-project-version-52ce8849a0) | <code>build-release</code> | embedding-project version tasks, hooks and CI checks | [BuildTools/project_version.py](https://github.com/cvet/fonline/blob/master/BuildTools/project_version.py) | 0 / 8 |
| <a id="entry-helper-cli-release-preflight-f5192a7f8b"></a><code>helper-cli.release-preflight</code> | [Release preflight](commands.md#entry-helper-cli-release-preflight-f5192a7f8b) | <code>build-release</code> | embedding-project manual release jobs before tagging and publication | [BuildTools/release_preflight.py](https://github.com/cvet/fonline/blob/master/BuildTools/release_preflight.py) | 0 / 17 |
| <a id="entry-helper-cli-engine-upgrade-report-a1902fe984"></a><code>helper-cli.engine-upgrade-report</code> | [Engine upgrade report](commands.md#entry-helper-cli-engine-upgrade-report-a1902fe984) | <code>documentation</code> | embedding-project Engine update work, run from the project root | [BuildTools/engine_upgrade_report.py](https://github.com/cvet/fonline/blob/master/BuildTools/engine_upgrade_report.py) | 0 / 10 |
| <a id="entry-helper-cli-script-binding-check-56e942e304"></a><code>helper-cli.script-binding-check</code> | [Script binding check](commands.md#entry-helper-cli-script-binding-check-56e942e304) | <code>quality</code> | Engine CI and embedding-project quality gates | [BuildTools/check_script_bindings.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_script_bindings.py) | 0 / 7 |
| <a id="entry-helper-cli-angelscript-hygiene-check-b5053583a9"></a><code>helper-cli.angelscript-hygiene-check</code> | [AngelScript hygiene check](commands.md#entry-helper-cli-angelscript-hygiene-check-b5053583a9) | <code>quality</code> | Engine CI and embedding-project quality gates | [BuildTools/check_angelscript_hygiene.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_angelscript_hygiene.py) | 0 / 7 |
| <a id="entry-helper-cli-createmsi-18899fd2a5"></a><code>helper-cli.createmsi</code> | [MSI creation](commands.md#entry-helper-cli-createmsi-18899fd2a5) | <code>build-release</code> | BuildTools/package.py Wix pack | [BuildTools/msicreator/createmsi.py](https://github.com/cvet/fonline/blob/master/BuildTools/msicreator/createmsi.py) | 0 / 2 |
| <a id="entry-helper-cli-move-sync-analysis-d49c9c73db"></a><code>helper-cli.move-sync-analysis</code> | [Movement synchronization trace analysis](commands.md#entry-helper-cli-move-sync-analysis-d49c9c73db) | <code>networking</code> | embedding-project movement playtests and direct developer diagnostics | [BuildTools/move_sync_analysis.py](https://github.com/cvet/fonline/blob/master/BuildTools/move_sync_analysis.py) | 0 / 10 |
| <a id="entry-helper-cli-check-text-packs-fedc98c863"></a><code>helper-cli.check-text-packs</code> | [Text pack lint](commands.md#entry-helper-cli-check-text-packs-fedc98c863) | <code>localization</code> | embedding-project localization tasks and CI content gates | [BuildTools/check_text_packs.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_text_packs.py) | 0 / 11 |

## Coverage

The model contains 23 helpers, 26 subcommands, 144 global arguments, and 174 subcommand arguments.

Included:

- engine-owned Python helper scripts that expose a top-level create_parser() factory
- exact argparse usage, help output, arguments, subcommands, ownership, audience, and invocation context

Excluded:

- the separately modeled BuildTools/buildtools.py command line
- the separately modeled BuildTools/package.py command line and package declaration contract
- documentation generators, tests, library-only modules, shell scripts, and embedding-project tools
