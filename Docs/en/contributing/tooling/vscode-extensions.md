---
layout: default
title: Maintain the FOnline VS Code Extensions
locale: en
document_id: contributing-vscode-extensions
permalink: /Docs/en/contributing/tooling/vscode-extensions.html
---

# Maintain the FOnline VS Code Extensions

This guide is for Engine contributors who change `BuildTools/fonline-tools` (FOnline Tools) or `BuildTools/angelscript-debugger` (the FOnline AngelScript debugger). Both are working Engine-owned tools: projects load them as source folders from `BuildTools` through tracked workspace symlinks. The guide describes the source layout, the build and test commands, which committed output is the runtime entry, how to change the label grammar without breaking project workspaces, and the checks a change needs, including what the automated tests do not cover.

## Source paths inspected

- `.gitignore`
- `.gitattributes`
- `.github/workflows/validate.yml`
- `BuildTools/fonline-tools/package.json`
- `BuildTools/fonline-tools/tsconfig.json`
- `BuildTools/fonline-tools/.gitignore`
- `BuildTools/fonline-tools/src/extension.ts`
- `BuildTools/fonline-tools/src/jsonc.ts`
- `BuildTools/fonline-tools/src/tests/jsonc.test.ts`
- `BuildTools/fonline-tools/dist/extension.js`
- `BuildTools/angelscript-debugger/package.json`
- `BuildTools/angelscript-debugger/tsconfig.json`
- `BuildTools/angelscript-debugger/.gitignore`
- `BuildTools/angelscript-debugger/src/extension.ts`
- `BuildTools/angelscript-debugger/src/activateFosDebug.ts`
- `BuildTools/angelscript-debugger/src/attachDiscovery.ts`
- `BuildTools/angelscript-debugger/src/debugAdapter.ts`
- `BuildTools/angelscript-debugger/src/fosDebug.ts`
- `BuildTools/angelscript-debugger/src/fosRuntime.ts`
- `BuildTools/angelscript-debugger/src/web-extension.ts`
- `BuildTools/angelscript-debugger/src/tests/adapter.test.ts`
- `BuildTools/angelscript-debugger/src/tests/attachDiscovery.test.ts`
- `BuildTools/angelscript-debugger/dist/extension.js`
- `BuildTools/angelscript-debugger/dist/debugAdapter.js`
- `BuildTools/tests/test_docs_debugging.py`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/how-to/project/vscode-workspace.md`
- `Docs/en/contributing/documentation/index.md`
- `Docs/en/how-to/release/versioning.md`

## Layout

Extension maintenance lives under `contributing/tooling` because the audience is Engine contributors, not game teams. Game teams use [Set Up a VS Code Workspace for an FOnline Project](../../how-to/project/vscode-workspace.md).

### FOnline Tools

| Path | Role |
| --- | --- |
| `package.json` | Extension manifest: command and keybinding contributions, activation, scripts. `main` is `./dist/extension.js`. |
| `src/extension.ts` | Menu building, platform filtering and hotkeys, one file of about 480 lines. |
| `src/jsonc.ts` | The parser for `.vscode/tasks.json`: strict JSON plus comments, trailing commas and a leading byte-order mark, the relaxed syntax VS Code accepts. |
| `src/tests/jsonc.test.ts` | `node:test` unit tests of the parser. |
| `tsconfig.json` | CommonJS, ES2020, `strict`, `noUnusedLocals`, `noEmit`: `tsc` only typechecks, esbuild writes the bundle. |
| `dist/extension.js`, `dist/extension.js.map` | Committed esbuild bundle. This is what VS Code runs. |
| `.gitignore` | Ignores `test-out/`, where `npm test` writes the bundled tests. |

There is no README, icon or `.vscodeignore`, and no `package` script: the Engine delivers the extension as a source folder, and projects install it through the workspace symlink described in the workspace guide.

### AngelScript debugger

| Path | Role |
| --- | --- |
| `package.json` | Manifest: `fos` language, grammar, `fos` debugger type, attach properties, scripts. `main` is `./dist/extension.js`. |
| `src/extension.ts`, `src/activateFosDebug.ts`, `src/web-extension.ts` | Extension host entry points; `activateFosDebug.ts` holds the configuration provider that runs attach discovery and applies the `processId` filter. |
| `src/debugAdapter.ts`, `src/fosDebug.ts`, `src/fosRuntime.ts`, `src/attachDiscovery.ts` | The debug adapter, its sample runtime and UDP/TCP attach transport. `attachDiscovery.ts` also holds the target selection shared by the configuration provider and the adapter. |
| `src/tests/` | `adapter.test.ts` with its data files, and `attachDiscovery.test.ts`. |
| `dist/extension.js`, `dist/web-extension.js`, `dist/debugAdapter.js` (+ maps) | Committed bundles. `contributes.debuggers[].program` is `./dist/debugAdapter.js`. |
| `sublime-angelscript/`, `language-configuration.json`, `icon.png` | Grammar and editor assets contributed to VS Code. |
| `.gitignore` | Ignores `test-out/`, where `npm test` writes the bundled tests. |

The adapter's run mode is a compile-time constant in `src/extension.ts` (`runMode`), currently `inline`. Discovery needs Node's UDP module and is unavailable in the browser bundle; `src/attachDiscovery.ts` reports that, and the configuration provider then asks for an explicit `tcp://` endpoint.

## Which output is the runtime entry

Both extensions load `dist/extension.js` (`main` in each `package.json`), and `dist/` is the only build output either package tracks. Neither `tsconfig.json` emits: `tsc` runs only as `npm run typecheck`, and esbuild produces every bundle. A change to `src/` has no effect for users until `dist/` is rebuilt from it and committed together with the source.

## Prerequisites and commands

Both packages are standalone npm projects; neither is built by CMake or by any Engine workflow. Use Node.js 18.18 or newer with npm: the test scripts rely on `node:test` and on mocha 11. The results below come from Node `24.16.0` and npm `11.13.0` on Windows, in scratch copies of each package outside the repository, with TypeScript 4.9.5, esbuild 0.28.2 and `@types/vscode` 1.89.0.

No `package-lock.json` is committed: the repository `.gitignore` ignores `package-lock.json` except for two named test and docs packages. Dependencies therefore float within their declared ranges. Two ranges are deliberately narrow: `typescript` is `^4.9.5` because the sources use the `satisfies` operator, and `@types/vscode` is `~1.89.0`, the minimum editor version in `engines.vscode`, so that `typecheck` rejects newer VS Code API.

### FOnline Tools

Run from `BuildTools/fonline-tools`:

```bash
npm install
npm test
npm run build
```

| Script | Command it runs |
| --- | --- |
| `typecheck` | `tsc -p tsconfig.json --noEmit` |
| `build` | `esbuild ./src/extension.ts --bundle --tsconfig=./tsconfig.json --external:vscode --format=cjs --platform=node --outfile=dist/extension.js --sourcemap --sources-content=false` |
| `watch` | The `build` command with `--watch` |
| `test` | `typecheck`, then esbuild bundles `src/tests/jsonc.test.ts` into `test-out/` and `node --test` runs it |
| `vscode:prepublish` | The esbuild command with `--minify` |

Observed behavior in the scratch copy: `npm run typecheck` passes, `npm test` passes its five parser tests, and `npm run build` reproduces the committed `dist/` files apart from line endings. The tests cover only the parser; menu building, platform filtering and hotkeys need the VS Code API. They were checked by running the bundle against an ad-hoc stub of that API, which is not part of the repository.

### AngelScript debugger

Run from `BuildTools/angelscript-debugger`:

```bash
npm install
npm test
npm run build
```

| Script | Command it runs |
| --- | --- |
| `typecheck` | `tsc -p tsconfig.json --noEmit` |
| `build` | The `esbuild-base`, `esbuild-web` and `esbuild-adapter` bundles with source maps |
| `test` | `typecheck`, `build`, then esbuild bundles `src/tests/*.test.ts` into `test-out/tests/` and `mocha --ui tdd` runs them |
| `watch`, `watch-web` | Watch builds of the extension and web bundles |
| `vscode:prepublish` | `rimraf dist` plus minified builds of all three bundles |
| `package`, `publish`, `publish-pre-release` | `vsce` commands; the Engine does not package or publish a VSIX, projects use the symlinked folder |

Observed behavior in the scratch copy: `npm run typecheck` passes, and `npm test` passes 13 tests. Eight are the adapter tests in `adapter.test.ts`: they start the rebuilt `dist/debugAdapter.js` over the Debug Adapter Protocol and run the sample runtime on the Markdown data files, so they check the adapter's protocol handling, not the live Engine endpoint. Five are unit tests of attach target selection: an unset filter, an exact `processId` match, a bare process id that matches nothing, duplicate ids from several hosts, and an explicit `endpoint`. Because `npm test` rebuilds `dist/` before the adapter tests, run it before you commit so that the committed bundles match the source. The package has no lint script; `typecheck` under `strict` and `noUnusedLocals` is its static check.

The adapter's delivery status (private package, source folder installed through a symlink, no lock file, no VSIX, adapter tests against the sample runtime only) is recorded in [Adapter delivery status](../../troubleshooting/debugging.md#adapter-delivery-status). Keep that page and its test, `BuildTools/tests/test_docs_debugging.py`, consistent when you change the package.

## Extend the label grammar safely

The grammar is implemented only in `BuildTools/fonline-tools/src/extension.ts`. Projects depend on every detail described in the [workspace guide](../../how-to/project/vscode-workspace.md#how-fonline-tools-builds-its-menu). Changing a tag, a precedence rule or a hotkey rule changes the menu of every project that uses the extension, and the Engine has no versioned contract for it.

Procedure for a grammar change:

1. Read the current behavior in the workspace guide and the functions it describes. Platform filtering is one function, `matchesCurrentPlatform`, shared by tasks, launch configurations and compounds. Label stripping is `stripPlatformTags`, and grouping is `parseGroupedTaskName`.
2. Change the source. The task path (`getTaskPicks`) and the launch path (`getLaunchPicks`) duplicate the grouping code; update both. The menu and the hotkeys share one list, `getTopLevelPicks`, which every hotkey press rebuilds, so there is no separate hotkey cache to keep in step. Keep `src/jsonc.ts` accepting what VS Code accepts in `tasks.json`, and extend `src/tests/jsonc.test.ts` when you change it.
3. Keep tags inside the label or name. Removing a tag from the stored string would break `dependsOn`, `preLaunchTask` and compound references in existing projects.
4. When a hotkey or command changes, update `contributes.commands` and `contributes.keybindings` in `package.json`; `MAX_SORT_HOTKEYS` in the source and the nine contributed commands must agree.
5. Run `npm test` and `npm run build`, then commit `src/` and the rebuilt `dist/` together. A change without a rebuilt `dist/` has no effect for users, because `dist/` is the entry point.
6. Update the [workspace guide](../../how-to/project/vscode-workspace.md) and its Russian mirror, including the label grammar table, the platform rules and the validation checklist. Update the [debugging guide](../../troubleshooting/debugging.md) only if attach behavior changed.
7. Add a changelog entry under the rules in [versioning](../../how-to/release/versioning.md#every-master-update) and describe the behavior change for embedding projects, because an existing `tasks.json` may stop matching.

The automated tests of FOnline Tools cover only the `tasks.json` parser. The menu itself is checked by hand: open a scratch workspace with a `tasks.json` and a `launch.json`, run `FOnline: Show Tools` and a few hotkeys, edit a `[sort:N]` label and press its hotkey again, and walk the checklist in the workspace guide on each host platform you claim.

## Change checklist

1. The change is in `src/`, and `dist/` was rebuilt with `npm run build` from the same source.
2. `npm test` passes in each changed package; it includes `typecheck`, which accepts no errors.
3. `dist/` was not hand-edited.
4. Label grammar, `tasks.json` parsing and hotkey behavior were re-read against the source and are reflected in the workspace guide, English and Russian.
5. The labels in the workspace guide's `tasks.json` and `launch.json` still produce the same menu.
6. `package.json` contributions agree with the source.
7. For the debugger: attach properties in `package.json`, the configuration provider and the debugging guide agree, and the delivery-status wording in the debugging guide and `test_docs_debugging.py` still holds.
8. The documentation gates in [documentation maintenance](../documentation/index.md#validation-checklist) pass, and the changelog entry exists.

## Coverage summary

| Area | Automated coverage |
| --- | --- |
| Typecheck of both packages | `npm run typecheck`, part of each `npm test` |
| FOnline Tools `tasks.json` parser | `npm test`: `src/tests/jsonc.test.ts` |
| FOnline Tools menu, platform filtering and hotkeys | None; manual check above |
| Debugger adapter protocol handling | `npm test`: `src/tests/adapter.test.ts` against the sample runtime, through the rebuilt `dist/debugAdapter.js` |
| Debugger attach target selection | `npm test`: `src/tests/attachDiscovery.test.ts` |
| Debugger package metadata | Documentation test `BuildTools/tests/test_docs_debugging.py` |
| UDP discovery and live attach to an Engine process | None |

The package tests run on a developer machine. `.github/workflows/validate.yml` runs none of them: its only Node.js setup belongs to the documentation browser job, which installs from a committed lock file, and the extension packages have no lock file.
