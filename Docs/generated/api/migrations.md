---
title: Migration Rules
document_id: generated-api-migrations
locale: en
generated: true
---

# Migration Rules

> Generated reference. Do not edit this page directly. Update engine metadata, regenerate `Docs/generated/api.json`, then run `python BuildTools/docs_reference.py --write`.

A dash in the description column means that the owning source metadata has no documentation comment. Every contract cell identifies an explicit source classification or the default `internal` policy; script reachability alone does not make a symbol public or stable.

[Reference index](index.md) | [Canonical JSON model](../api.json) | [Generation contract](../../GeneratedApiAndMetadata.md)

This page contains **28** native metadata migration rules.

<a id="group-migration-property-6e343be3f7"></a>
## <code>Property</code>

| Scope | Previous name | Replacement | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Item</code> | <code>Accessory</code> | <code>Ownership</code> | <a id="symbol-migration-property-item-accessory-6fb6f43e73"></a><code>migration.Property.Item.Accessory</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:105](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L105) | - |
| <code>Item</code> | <code>BlockLines</code> | <code>MultihexLines</code> | <a id="symbol-migration-property-item-blocklines-30e02aef8c"></a><code>migration.Property.Item.BlockLines</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:140](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L140) | - |
| <code>Critter</code> | <code>Cond</code> | <code>Condition</code> | <a id="symbol-migration-property-critter-cond-0d6aaf7b9e"></a><code>migration.Property.Critter.Cond</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:276](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L276) | - |
| <code>Item</code> | <code>CritId</code> | <code>CritterId</code> | <a id="symbol-migration-property-item-critid-945e69cf64"></a><code>migration.Property.Item.CritId</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:114](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L114) | - |
| <code>Item</code> | <code>CritSlot</code> | <code>CritterSlot</code> | <a id="symbol-migration-property-item-critslot-310bece15f"></a><code>migration.Property.Item.CritSlot</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:117](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L117) | - |
| <code>Map</code> | <code>CurDayTime</code> | <code>FixedDayTime</code> | <a id="symbol-migration-property-map-curdaytime-d2a86d2a26"></a><code>migration.Property.Map.CurDayTime</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:331](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L331) | - |
| <code>Map</code> | <code>DayTime</code> | <code>DayColorTime</code> | <a id="symbol-migration-property-map-daytime-dfb063d2cd"></a><code>migration.Property.Map.DayTime</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:334](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L334) | - |
| <code>Item</code> | <code>IsColorize</code> | <code>Colorize</code> | <a id="symbol-migration-property-item-iscolorize-5edb6124c0"></a><code>migration.Property.Item.IsColorize</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:213](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L213) | - |
| <code>Critter</code> | <code>IsControlledByPlayer</code> | <code>ControlledByPlayer</code> | <a id="symbol-migration-property-critter-iscontrolledbyplayer-8301e73564"></a><code>migration.Property.Critter.IsControlledByPlayer</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:259](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L259) | - |
| <code>Item</code> | <code>IsFlat</code> | <code>DrawFlatten</code> | <a id="symbol-migration-property-item-isflat-5d3ec910e8"></a><code>migration.Property.Item.IsFlat</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:200](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L200) | - |
| <code>Item</code> | <code>IsHidden</code> | <code>Hidden</code> | <a id="symbol-migration-property-item-ishidden-cfc0c2ec10"></a><code>migration.Property.Item.IsHidden</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:151](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L151) | - |
| <code>Item</code> | <code>IsHiddenPicture</code> | <code>AlwaysHideSprite</code> | <a id="symbol-migration-property-item-ishiddenpicture-d83492ed07"></a><code>migration.Property.Item.IsHiddenPicture</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:156](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L156) | - |
| <code>Item</code> | <code>IsLight</code> | <code>LightSource</code> | <a id="symbol-migration-property-item-islight-13b661d5f2"></a><code>migration.Property.Item.IsLight</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:168](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L168) | - |
| <code>Item</code> | <code>IsLightThru</code> | <code>LightThru</code> | <a id="symbol-migration-property-item-islightthru-5b37629a02"></a><code>migration.Property.Item.IsLightThru</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:165](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L165) | - |
| <code>Item</code> | <code>IsNoBlock</code> | <code>NoBlock</code> | <a id="symbol-migration-property-item-isnoblock-bec75eef3d"></a><code>migration.Property.Item.IsNoBlock</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:159](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L159) | - |
| <code>Critter</code> | <code>IsNoFlatten</code> | <code>DeadDrawNoFlatten</code> | <a id="symbol-migration-property-critter-isnoflatten-d3ee4b7a53"></a><code>migration.Property.Critter.IsNoFlatten</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:283](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L283) | - |
| <code>Item</code> | <code>IsNoHighlight</code> | <code>NoHighlight</code> | <a id="symbol-migration-property-item-isnohighlight-e86515f912"></a><code>migration.Property.Item.IsNoHighlight</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:205](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L205) | - |
| <code>Item</code> | <code>IsNoLightInfluence</code> | <code>NoLightInfluence</code> | <a id="symbol-migration-property-item-isnolightinfluence-ec0ee923d6"></a><code>migration.Property.Item.IsNoLightInfluence</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:208](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L208) | - |
| <code>Item</code> | <code>IsShootThru</code> | <code>ShootThru</code> | <a id="symbol-migration-property-item-isshootthru-f424aefe52"></a><code>migration.Property.Item.IsShootThru</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:162](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L162) | - |
| <code>Item</code> | <code>IsStatic</code> | <code>Static</code> | <a id="symbol-migration-property-item-isstatic-5e837e559e"></a><code>migration.Property.Item.IsStatic</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:102](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L102) | - |
| <code>Item</code> | <code>SceneryScript</code> | <code>StaticScript</code> | <a id="symbol-migration-property-item-sceneryscript-b11ef9150f"></a><code>migration.Property.Item.SceneryScript</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:181](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L181) | - |
| <code>Critter</code> | <code>ScriptId</code> | <code>InitScript</code> | <a id="symbol-migration-property-critter-scriptid-dd3bdf607f"></a><code>migration.Property.Critter.ScriptId</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:228](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L228) | - |
| <code>Item</code> | <code>ScriptId</code> | <code>InitScript</code> | <a id="symbol-migration-property-item-scriptid-db0591aae9"></a><code>migration.Property.Item.ScriptId</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:99](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L99) | - |
| <code>Location</code> | <code>ScriptId</code> | <code>InitScript</code> | <a id="symbol-migration-property-location-scriptid-2f0bc18a7b"></a><code>migration.Property.Location.ScriptId</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:359](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L359) | - |
| <code>Map</code> | <code>ScriptId</code> | <code>InitScript</code> | <a id="symbol-migration-property-map-scriptid-78be913261"></a><code>migration.Property.Map.ScriptId</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:308](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L308) | - |
| <code>Item</code> | <code>Slot</code> | <code>CritterSlot</code> | <a id="symbol-migration-property-item-slot-c6f49a1b48"></a><code>migration.Property.Item.Slot</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:118](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L118) | - |
| <code>Item</code> | <code>SubItemIds</code> | <code>InnerItemIds</code> | <a id="symbol-migration-property-item-subitemids-34ff1917dc"></a><code>migration.Property.Item.SubItemIds</code> | <code>internal</code> (default) | [Source/Common/EntityProperties.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/EntityProperties.h#L125) | - |

<a id="group-migration-version-012227aec8"></a>
## <code>Version</code>

| Scope | Previous name | Replacement | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>0</code> | <code>0</code> | <code>32</code> | <a id="symbol-migration-version-0-0-44f81ee08d"></a><code>migration.Version.0.0</code> | <code>internal</code> (default) | [Source/Common/Common.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L46) | Force change of compatability version |
