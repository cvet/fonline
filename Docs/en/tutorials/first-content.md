---
layout: default
title: First Content Change
locale: en
document_id: first-content-tutorial
permalink: /Docs/en/tutorials/first-content.html
---

# First Content Change

Change localized prototype text and prove the baked result in the
[Minimal Multiplayer](../../../Examples/MinimalMultiplayer/README.md) example.

## Locate the owning sources

The lesson deliberately separates authored data from behavior:

| Source | Responsibility |
|---|---|
| `Content/StarterContent.fopro` | player, NPC, item, and location prototypes plus English/Russian text |
| `Maps/TutorialMap.fomap` | map identity, size, and work hex |
| `Scripts/Tutorial.fos` | world creation, interaction, and client presentation |
| `FOnlineMinimalMultiplayer.fomain` | generated baker inputs, language order, and runtime settings |

Do not edit `Build/*`, `Baking/*`, generated metadata, or a packaged resource
to author content. Those are outputs. Project-owned configuration choices live
in `generate_config.py`; its generated `.fomain` must remain reproducible.

## Rename the tutorial item

Open
[`Content/StarterContent.fopro`](../../../Examples/MinimalMultiplayer/Content/StarterContent.fopro)
and replace both `$Text` values on `TutorialSupply`. Keep the stable prototype
name unchanged:

```ini
[ProtoItem]
$Name = TutorialSupply
$Text engl = "Emergency cache"
$Text russ = "Аварийный контейнер"
Stackable = True
```

`Baking.BakeLanguages = engl russ` makes English the normalization base for
this example. Both locale values are authored together so the content contract
does not silently rely on fallback.

The checked smoke and packaged-runtime scenarios intentionally treat the
visible English name as a public expectation. Update the client marker in both
`tutorial-smoke.json` and `package-smoke.json` in the same change:

```json
"tutorial_client_content=Emergency cache"
```

Keeping the assertion with the content revision distinguishes an intentional
rename from missing or stale baked text.

## Bake and verify

Run the complete check from a standalone example checkout:

```powershell
python validate.py
```

Expected semantic evidence includes:

```text
[tutorial-smoke] remote calls and replicated persistent property verified
[gameplay-test] scenario content-test: passed
tutorial_client_content=Emergency cache
tutorial_server_supply_collected=1
[gameplay-test] summary: suite=minimal-multiplayer-tutorial status=passed scenarios=2 passed=2 failed=0
```

Process-output lines may have runner labels around the markers. The markers and
final passed summary are the contract.

For an Engine checkout, run `python validate.py` from
`Examples/MinimalMultiplayer`; the script selects the supported host preset.

To inspect Russian text visibly, set the `Client.Language` override to `russ`
in `generate_config.py`, regenerate the checked `.fomain`, rebuild the check
preset, and launch the normal desktop client. Restore or deliberately commit
the selected default language as project policy; language selection does not
belong in Engine source.

## Add content safely

For a new prototype or map:

1. Add the authored section under a directory consumed by the appropriate
   `ResourcePack`.
2. Give it a stable `$Name`; changing persisted identities later is a migration,
   not a cosmetic rename.
3. Author every locale in `Baking.BakeLanguages`.
4. Reference it through script metadata or a validated `hstring`, according to
   the owning API.
5. Extend the content test, then bake and run the smallest consuming route.

Use [Prototype Format](../how-to/content/prototype-format.md), [Map Format](../how-to/content/map-format.md),
and [Text and Localization](../how-to/content/text-and-localization.md) for the complete
contracts.

## Recovery

- `Unknown prototype`: confirm the extension is in
  `Baking.ProtoFileExtensions` and the containing directory is mounted by the
  `Protos` resource pack.
- The old name remains: do not patch generated or packaged outputs; rerun the
  checked bake and inspect `Baking/Baking.report.json` for the owning pack.
- The content test passes but the client marker fails: inspect the `Texts`
  resource pack and runtime `Game.GetText()` lookup, not only prototype
  existence.

Continue with [First Automated Test](first-test.md).
