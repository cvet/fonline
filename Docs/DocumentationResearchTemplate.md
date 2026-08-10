# Documentation Research Template

Use this template before writing or expanding any `Engine/Docs/*.md` page. Copy the sections into working notes or into the new doc while drafting, then remove transient notes before finalizing if they do not help readers.

## Topic

- **Doc path:** `Docs/<Topic>.md`
- **Status:** `researching`
- **Audience:** engine user / game integrator / engine maintainer / AI maintainer
- **Scope:** what this doc owns
- **Out of scope:** what belongs to another engine doc or to the embedding project's docs

## Source paths inspected

List exact paths, not vague areas:

- `Source/...`
- `BuildTools/...`
- `Resources/...`
- `ThirdParty/...` if directly relevant

## Tests inspected

List tests that encode behavior:

- `Source/Tests/Test_*.cpp`

If no direct test exists, say so explicitly and prefer conservative wording.

## Build or preset references

- `BuildTools/cmake/stages/...`
- `BuildTools/cmake/helpers/...`
- Embedding project `CMakePresets.json` only when the doc intentionally uses that project as an example.

## Current behavior summary

Write a short source-grounded summary:

- What the system does.
- What owns configuration.
- What is generated vs hand-authored.
- Which runtime side owns it: common, client, server, mapper, tool, platform frontend.

## Reader tasks to support

List concrete tasks this doc should help with:

- Find the entry point for a subsystem.
- Change behavior safely.
- Validate the change.
- Route game-specific work back to the embedding project.

## Caveats and boundaries

- Mention incomplete or platform-specific behavior.
- Avoid promising stable public API unless the code/docs actually establish it.
- Do not copy game-specific content rules into engine docs.

## Links to update

- [Docs/README.md](README.md)
- [DocumentationBacklog.md](DocumentationBacklog.md)
- [../README.md](../README.md), if the topic is a major entry point
- Source-tree README, if the topic is source navigation
- Related focused docs

## Verification checklist

- [ ] Every linked local Markdown file exists.
- [ ] Every cited source path exists.
- [ ] `git diff --check` passes.
- [ ] Markdown link check passes over `README.md`, `AGENTS.md`, `Docs/**/*.md`, `Source/README.md`, `Source/Tests/README.md`, and `BuildTools/README.md`.
- [ ] `git diff --cached --name-only` is empty unless staging was requested.
- [ ] The doc distinguishes engine ownership from embedding-project ownership.
