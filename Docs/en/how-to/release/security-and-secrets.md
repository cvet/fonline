---
layout: default
title: Security and Secrets
locale: en
document_id: security-and-secrets
permalink: /Docs/en/how-to/release/security-and-secrets.html
---

# Security and Secrets

This guide defines the reusable FOnline boundaries for credentials, configuration substitution, package signing, CI, diagnostics, and incident response. It does not choose a secret manager, certificate provider, production account, retention period, or incident policy for an embedding game.

Use [Project Configuration](../build/project-configuration.md) for general `.fomain` precedence, [Packaging and Release](packaging.md) for artifact production, and [Client Updater](../../explanation/runtime/client-updater.md) for the downloaded native-runtime boundary.

## Secret decision

Use `$ENV{NAME}` only when the value may resolve on the build or baking host;
its concrete value can enter baked config. Use `$TARGET_ENV{NAME}` for a value
that must remain unresolved until the target application runs.
The command-line override log masks only its narrow supported form and does not
redact target-time directives, generated files, process state, or arbitrary
logs.

Keep signing material outside staged artifacts. Hand native signing through
`Packaging.CodeSigningHook`, whose project-owned process reads credentials from
its protected environment. The current Android packager has no host-only secret
input: it reads passwords from baked target config before copying them into
`FO_ANDROID_RELEASE_STORE_PASSWORD` and `FO_ANDROID_RELEASE_KEY_PASSWORD` for
Gradle. Do not put production passwords into that config; use a project-owned
protected signing stage until a dedicated handoff exists. After exposure, contain access, revoke the affected credential,
rotate replacements, rebuild or redeploy affected artifacts, and audit use.
Rewriting git history is cleanup, not revocation.

## Threat model and ownership

Protect at least these asset classes:

| Asset | Typical exposure paths | Owner |
|---|---|---|
| Runtime credentials | tracked config, baked internal config, process arguments, logs, crash reports, settings UI, memory | embedding project and deployment operator |
| Signing credentials | CI variables, local environment, keystore/certificate files, signing-provider session, build logs | release operator |
| Release integrity | compromised runner, altered binary after signing, mutable artifact, unverified updater payload | project release pipeline |
| Player and operational data | database credentials, backups, support exports, telemetry payloads | project operations |
| Engine supply chain | source revision, dependencies, actions, SDKs, package tools | Engine maintainers plus the qualifying project |

Treat project source, pull-request code, downloaded dependencies, build runners, package staging, artifact storage, deployment hosts, and the running client/server as separate trust zones. A value becoming available in one zone is not permission to copy it into another.

The Engine provides substitution mechanics, one narrow log-masking rule, package-time handoffs, and validation fixtures. The project owns secret creation, access policy, storage backend, rotation, revocation, audit retention, environment separation, and incident severity. Never put real values in Engine examples, tests, documentation, issue text, or generated reports.

## Choose the right configuration form

`GlobalSettings::SetValue()` recognizes four substitutions. Their time of resolution is a security boundary, not just syntax.

| Form | Normal resolution | During `ConfigBaker` | Correct use |
|---|---|---|---|
| literal | config parse | copied as the value | public, non-sensitive configuration only |
| `$ENV{NAME}` | process reading the authored config | resolves on the baking host, so the concrete value can enter baked config | non-secret build input that is intended to be embedded |
| `$FILE{path}` | process reading the authored config; relative to the owning config directory | reads on the baking host, so file contents can enter baked config | non-secret generated metadata such as a version |
| `$TARGET_ENV{NAME}` | target application runtime | remains a directive while baking | runtime secret that must not be baked |
| `$TARGET_FILE{path}` | target application runtime | remains a directive while baking | protected target-host file whose contents are needed at runtime |

For runtime credentials, prefer `$TARGET_ENV{...}`. Use `$TARGET_FILE{...}` only when the deployment owns the file path and permissions; a relative target-file path in an embedded config resolves from the target process context, not from the original repository.

Do not assume the packager resolves target directives. Android packaging reads the already baked effective target config; a retained `$TARGET_ENV{...}` value is still a directive string, not a package-host secret lookup. Windows signing reads `Packaging.CodeSigningHook` as a path from the project config and likewise has no target-directive resolver. Keep package credentials out of both authored and baked config.

```ini
# Values, paths, and aliases below are examples of variable names, not credentials.
Auth.SessionSigningSecret = $TARGET_ENV{MYGAME_AUTH_SESSION_SECRET}

Android.Keystore = $TARGET_ENV{MYGAME_ANDROID_KEYSTORE_PATH}
Android.KeystorePassword = $TARGET_ENV{MYGAME_ANDROID_STORE_PASSWORD}
Android.KeyAlias = $TARGET_ENV{MYGAME_ANDROID_KEY_ALIAS}
Android.KeyPassword = $TARGET_ENV{MYGAME_ANDROID_KEY_PASSWORD}

Packaging.CodeSigningHook = Tools/SignWindowsArtifacts
```

The Android lines above illustrate runtime-directive syntax only; the current packager does not resolve them for signing. The Windows hook path is non-secret. Its executable obtains credentials from a protected environment that Engine does not inspect.

Do not pass a secret as a command-line setting. `Common.SecretSettingTokens` masks matching values only in `ApplyCommandLine()`'s `Set <name> to <value>` log line. The raw argument can still be visible in shell history, process inspection, `Common.CommandLine`, `Common.CommandLineArgs`, a debugger, or a crash dump.

## Understand redaction limits

The default `Common.SecretSettingTokens` entries are case-insensitive name substrings: `secret`, `token`, `password`, and `apikey`. Extend the list for project names such as `dsn`, but treat this as defense in depth only.

The Engine does **not** infer a credential type, encrypt settings, zero memory, rotate values, or guarantee redaction outside that one command-line override log. In particular:

- config values are ordinary strings after resolution;
- `GlobalSettings::Save()` emits applied values in baking mode;
- `ConfigBaker` writes non-empty applicable values into side-specific internal configs;
- `GlobalSettings::Draw()` renders registered setting values;
- project code, third-party libraries, crash handlers, CI shells, and signing tools can log their own inputs;
- a custom setting reported as unknown during baking is logged with its current value.

Therefore, target-time directives are the default for runtime secrets, production logs and crash attachments require an independent redaction review, and settings/debug UI must not be exposed to untrusted operators. Never test redaction with a real credential. Use a unique synthetic canary in an isolated lane, then delete the lane's logs and artifacts.

## Package and sign without copying credentials

### Windows

`Packaging.CodeSigningHook` names a project-owned executable. The packager calls it as `<hook> <absolute-binary-path>` after binary patching and before archive or MSI production. No signing credential is passed as a command-line argument by the Engine. The hook obtains its provider session or credentials from the protected packaging-host environment and must fail nonzero when signing or signature verification fails.

The hook path itself must be a directly usable non-secret path; the packager does not resolve `$TARGET_ENV{...}` there. Keep the executable outside untrusted workspace writes, pin or hash it, restrict who can replace it, and have it verify the final signature and timestamp. The current Engine hook signs staged `.exe` and `.dll` files; signing the enclosing installer or publication metadata remains project-owned.

### Android

Android release packaging reads `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, and `Android.KeyPassword` from the baked effective target config. The complete tuple is required when any member is set. `package.py` then passes the two password strings to Gradle through `FO_ANDROID_RELEASE_STORE_PASSWORD` and `FO_ANDROID_RELEASE_KEY_PASSWORD`; the generated `build.gradle` reads those environment variables instead of containing the passwords.

That Gradle handoff prevents password substitution into `build.gradle`, but it does not make the input host-only: a concrete password has already passed through baked config, while a `$TARGET_ENV{...}` directive is not resolved. The current Engine therefore does not provide a production-safe Android signing-secret boundary. Leave the tuple empty for development output or use a protected project-owned Android signing stage whose credentials never enter authored or baked Engine config.

The keystore path and key alias are patched into the generated project and are not treated as passwords. Protect the keystore itself, the generated Gradle tree, `GRADLE_USER_HOME`, process memory, and worker logs. An APK produced through the debug-key fallback is a development artifact, not a production release.

### Artifacts and updater payloads

Signing does not prove that an artifact is secret-free or that the correct bytes were published. After signing:

1. verify every required signature and timestamp;
2. inventory raw payloads and archives, including client-runtime updater libraries;
3. scan the staged tree and archive members for forbidden files, private keys, config dumps, and synthetic canaries;
4. hash final bytes and bind hashes to the source, Engine revision, toolchain, config, and package identity;
5. publish immutable artifacts only after install/deploy and updater acceptance pass.

Do not print a real value to search for it. Prefer forbidden-path and file-type rules, secret-scanning tools, entropy checks with reviewed allowlists, and synthetic canaries created solely for the test lane.

## CI trust boundaries

The reusable Engine validation workflow uses top-level `contents: read` permission and does not perform release signing or deployment. Its only current GitHub secret reference is the coverage upload token. That repository state is evidence for Engine validation, not a template proving that an embedding project's release workflow is safe.

A project release lane should enforce all of the following:

- untrusted pull-request code never receives release, deployment, database, or signing credentials;
- signing and deployment run only from reviewed, protected revisions and protected environments;
- workflow and third-party action revisions are pinned under project policy;
- self-hosted runners are treated as persistent privileged hosts, cleaned between jobs, and isolated from untrusted builds;
- secret-bearing steps disable shell tracing and never echo environment, command lines, generated config, or provider responses;
- caches never contain keystores, credentials, signed-session state, private config, or production database material;
- artifacts have explicit retention and access policy, and upload paths cannot include the entire workspace by accident;
- release credentials are scoped to one environment and least privilege; development, staging, and production do not share values;
- approval, signing, publication, and rollback events leave a sanitized audit trail.

Repository-secret masking is not a content scanner. A transformed, split, encoded, short, or tool-emitted value may escape masking, and a malicious build can exfiltrate a value without printing it.

## Provision, rotate, and revoke

Maintain a project-owned inventory with a non-secret identifier, purpose, owner, storage location, consumers, environments, privilege, creation date, rotation rule, and revocation procedure. Do not put the value itself in the inventory.

For routine rotation:

1. create the replacement through the owning provider;
2. provision it to the narrow target environment;
3. deploy consumers that can use the replacement, allowing a reviewed overlap only when the protocol requires it;
4. verify normal operation and negative behavior for the old value;
5. revoke the old value;
6. remove stale copies from runners, hosts, caches, backups where policy allows, and operator machines;
7. record sanitized evidence and the next rotation trigger.

For suspected exposure, stop affected publication/deployment, revoke first when service safety permits, rotate every credential derived from or co-located with the exposed material, and preserve sanitized forensic evidence. Remove exposed logs and artifacts from normal access, but assume every downloaded copy persists. Rewriting git history or deleting a CI log does not make the old value trustworthy again. Rebuild and re-sign from a known-good revision and runner before resuming rollout.

## Verification workflow

Run the focused reusable checks after changing substitution, signing, or security guidance:

```bash
python BuildTools/tests/test_package_security.py
python BuildTools/tests/test_docs_security_and_secrets.py
python BuildTools/tests/test_docs_package.py
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_validate.py
```

Then qualify the affected project lane with non-production credentials:

1. inspect the authored `.fomain` and selected sub-config for literals;
2. bake and verify that runtime secrets remain `$TARGET_*` directives in `Baking/Configs`, and that package credentials are absent;
3. package a development artifact and inspect the generated tree, raw package, archives, logs, and manifest;
4. verify the Windows signing hook obtains credentials from its own protected environment, or verify the project-owned Android signing stage without placing secrets in Engine config;
5. install or deploy, start the packaged application, and exercise updater/network behavior;
6. revoke or delete the synthetic credentials and purge the isolated evidence according to test policy.

Passing Engine tests proves runtime substitution and the current packager boundaries, including the Android host-only handoff gap. It does not prove a provider account, runner, secret manager, application log, database, distribution store, or incident process is secure.

Use [Release Operations](operations.md) for target-host preflight, staged rollout, evidence preservation, and rollback. Use [Backup and Recovery](backup-and-recovery.md) for encrypted recovery sets, restore-role access, key identifiers, and secret-free sidecar manifests. Keep credentials and sensitive incident material out of both operational records.

## Failure routing

| Symptom | Inspect first |
|---|---|
| Concrete credential appears in `Baking/Configs` | replace `$ENV`/`$FILE` with a target form and rebake from a clean output |
| Command-line log shows a credential | setting name and `Common.SecretSettingTokens`; rotate the value because other argument exposures remain |
| Packaging reports a missing path/file | baked target config or root project config, selected sub-config, and file permissions; do not put a secret there |
| Android release uses the debug key | complete four-field signing tuple and selected package config |
| Generated Gradle file contains a password | packaging implementation regression; stop and rotate before publication |
| Windows hook was skipped | literal `Packaging.CodeSigningHook` path in project config and hook file visibility |
| Signature is absent after a successful hook | hook verification/failure contract and post-sign mutation order |
| Secret appears in logs, artifacts, cache, or history | revoke/rotate first, restrict access, preserve sanitized evidence, then remove copies |
| Untrusted CI job can reach protected material | workflow event, permissions, environment approval, runner isolation, and cache/artifact scope |

## Source paths inspected

- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tools/ConfigBaker.cpp`
- `Source/Tests/Test_Settings.cpp`
- `BuildTools/foconfig.py`
- `BuildTools/package.py`
- `BuildTools/android-project/app/build.gradle`
- `BuildTools/tests/test_package_security.py`
- `.github/workflows/validate.yml`
