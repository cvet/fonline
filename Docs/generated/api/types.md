---
title: Script Types
document_id: generated-api-types
locale: en
generated: true
---

# Script Types

> Generated reference. Do not edit this page directly. Update engine metadata, regenerate `Docs/generated/api.json`, then run `python BuildTools/docs_reference.py --write`.

A dash in the description column means that the owning source metadata has no documentation comment. Every contract cell identifies an explicit source classification or the default `internal` policy; script reachability alone does not make a symbol public or stable.

[Reference index](index.md) | [Canonical JSON model](../api.json) | [Generation contract](../../GeneratedApiAndMetadata.md)

This page contains **966** type and member symbols.

## Entities

| Entity | Symbol ID | Capabilities | Runtime | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>entity Critter</code> | <a id="symbol-script-entity-critter-ce4dbf1c01"></a><code>script.entity.Critter</code> | prototypes, time-events | server, client, mapper | <code>internal</code> (default) | [Source/Common/Entity.h:48](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L48) | - |
| <code>entity Game</code> | <a id="symbol-script-entity-game-d3adadeb38"></a><code>script.entity.Game</code> | global | server, client, mapper | <code>internal</code> (default) | [Source/Common/Entity.h:44](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L44) | - |
| <code>entity ImGui</code> | <a id="symbol-script-entity-imgui-ecf8634513"></a><code>script.entity.ImGui</code> | global | server, client, mapper | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:53](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L53) | - |
| <code>entity Item</code> | <a id="symbol-script-entity-item-51c323d48b"></a><code>script.entity.Item</code> | prototypes, statics, abstract, time-events | server, client, mapper | <code>internal</code> (default) | [Source/Common/Entity.h:49](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L49) | - |
| <code>entity Location</code> | <a id="symbol-script-entity-location-fd326245c6"></a><code>script.entity.Location</code> | prototypes, time-events | server, client, mapper | <code>internal</code> (default) | [Source/Common/Entity.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L46) | - |
| <code>entity Map</code> | <a id="symbol-script-entity-map-7134b606ea"></a><code>script.entity.Map</code> | prototypes, time-events | server, client, mapper | <code>internal</code> (default) | [Source/Common/Entity.h:47](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L47) | - |
| <code>entity Player</code> | <a id="symbol-script-entity-player-c6a98645be"></a><code>script.entity.Player</code> | time-events | server, client, mapper | <code>internal</code> (default) | [Source/Common/Entity.h:45](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L45) | - |

## Enums

<a id="symbol-script-enum-cornertype-f3531f8416"></a>
### <code>CornerType</code>

<code>enum CornerType : uint8</code>  
Symbol ID: <code>script.enum.CornerType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>East</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-cornertype-east-885d0fb054"></a><code>script.enum-value.CornerType.East</code> | <code>internal</code> (default) | [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998) | - |
| <code>EastWest</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-cornertype-eastwest-4ad68b6877"></a><code>script.enum-value.CornerType.EastWest</code> | <code>internal</code> (default) | [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998) | - |
| <code>North</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-cornertype-north-091b2bd934"></a><code>script.enum-value.CornerType.North</code> | <code>internal</code> (default) | [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998) | - |
| <code>NorthSouth</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-cornertype-northsouth-d33aa02173"></a><code>script.enum-value.CornerType.NorthSouth</code> | <code>internal</code> (default) | [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998) | - |
| <code>South</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-cornertype-south-4c4d0f1040"></a><code>script.enum-value.CornerType.South</code> | <code>internal</code> (default) | [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998) | - |
| <code>West</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-cornertype-west-671337b680"></a><code>script.enum-value.CornerType.West</code> | <code>internal</code> (default) | [Source/Common/Common.h:998](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L998) | - |

<a id="symbol-script-enum-critteraction-3750345c21"></a>
### <code>CritterAction</code>

<code>enum CritterAction : uint16</code>  
Symbol ID: <code>script.enum.CritterAction</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918)

Critter actions<br>Flags for chosen:<br>l - hardcoded local call<br>s - hardcoded server call<br>for all others critters actions call only server<br>flags actionExt item

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Connect</code> | <code>20</code> | 20 | <a id="symbol-script-enum-value-critteraction-connect-91d9599a8f"></a><code>script.enum-value.CritterAction.Connect</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>Dead</code> | <code>19</code> | 19 | <a id="symbol-script-enum-value-critteraction-dead-909a9304bb"></a><code>script.enum-value.CritterAction.Dead</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>Disconnect</code> | <code>21</code> | 21 | <a id="symbol-script-enum-value-critteraction-disconnect-b1cff448ec"></a><code>script.enum-value.CritterAction.Disconnect</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>DropItem</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-critteraction-dropitem-0b5aabf11c"></a><code>script.enum-value.CritterAction.DropItem</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>Knockout</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-critteraction-knockout-e94bb8c977"></a><code>script.enum-value.CritterAction.Knockout</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>MoveItem</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-critteraction-moveitem-961b3ff13d"></a><code>script.enum-value.CritterAction.MoveItem</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critteraction-none-dbe33f7e48"></a><code>script.enum-value.CritterAction.None</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>Refresh</code> | <code>23</code> | 23 | <a id="symbol-script-enum-value-critteraction-refresh-a920e75eba"></a><code>script.enum-value.CritterAction.Refresh</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>Respawn</code> | <code>22</code> | 22 | <a id="symbol-script-enum-value-critteraction-respawn-f3d97e6b1b"></a><code>script.enum-value.CritterAction.Respawn</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>StandUp</code> | <code>17</code> | 17 | <a id="symbol-script-enum-value-critteraction-standup-de42a8038f"></a><code>script.enum-value.CritterAction.StandUp</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |
| <code>SwapItems</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-critteraction-swapitems-9097b30148"></a><code>script.enum-value.CritterAction.SwapItems</code> | <code>internal</code> (default) | [Source/Common/Common.h:918](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L918) | - |

<a id="symbol-script-enum-critteractionanim-d1b9326b0f"></a>
### <code>CritterActionAnim</code>

<code>enum CritterActionAnim : uint16</code>  
Symbol ID: <code>script.enum.CritterActionAnim</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>DeadFront</code> | <code>102</code> | 102 | <a id="symbol-script-enum-value-critteractionanim-deadfront-3ddeb54952"></a><code>script.enum-value.CritterActionAnim.DeadFront</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>Idle</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-critteractionanim-idle-e3b819837c"></a><code>script.enum-value.CritterActionAnim.Idle</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>IdleProneFront</code> | <code>86</code> | 86 | <a id="symbol-script-enum-value-critteractionanim-idlepronefront-3856fdc6a8"></a><code>script.enum-value.CritterActionAnim.IdleProneFront</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>Limp</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-critteractionanim-limp-8bc32f32d6"></a><code>script.enum-value.CritterActionAnim.Limp</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critteractionanim-none-2f257cb433"></a><code>script.enum-value.CritterActionAnim.None</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>PanicRun</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-critteractionanim-panicrun-b987d91a35"></a><code>script.enum-value.CritterActionAnim.PanicRun</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>Run</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-critteractionanim-run-2c98d7d8ec"></a><code>script.enum-value.CritterActionAnim.Run</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>RunBack</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-critteractionanim-runback-d524253ff7"></a><code>script.enum-value.CritterActionAnim.RunBack</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>SneakRun</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-critteractionanim-sneakrun-9e0694f7bd"></a><code>script.enum-value.CritterActionAnim.SneakRun</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>SneakWalk</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-critteractionanim-sneakwalk-0bd6c2732a"></a><code>script.enum-value.CritterActionAnim.SneakWalk</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>TurnLeft</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-critteractionanim-turnleft-015180e701"></a><code>script.enum-value.CritterActionAnim.TurnLeft</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>TurnRight</code> | <code>17</code> | 17 | <a id="symbol-script-enum-value-critteractionanim-turnright-2c7d42713e"></a><code>script.enum-value.CritterActionAnim.TurnRight</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>Walk</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-critteractionanim-walk-a17b0fad6a"></a><code>script.enum-value.CritterActionAnim.Walk</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |
| <code>WalkBack</code> | <code>15</code> | 15 | <a id="symbol-script-enum-value-critteractionanim-walkback-ed52893994"></a><code>script.enum-value.CritterActionAnim.WalkBack</code> | <code>internal</code> (default) | [Source/Common/Common.h:941](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L941) | - |

<a id="symbol-script-enum-crittercondition-f3dff81aea"></a>
### <code>CritterCondition</code>

<code>enum CritterCondition : uint8</code>  
Symbol ID: <code>script.enum.CritterCondition</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:904](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L904)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Alive</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-crittercondition-alive-57871d8f60"></a><code>script.enum-value.CritterCondition.Alive</code> | <code>internal</code> (default) | [Source/Common/Common.h:904](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L904) | - |
| <code>Dead</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-crittercondition-dead-9dc3cf59e8"></a><code>script.enum-value.CritterCondition.Dead</code> | <code>internal</code> (default) | [Source/Common/Common.h:904](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L904) | - |
| <code>Knockout</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-crittercondition-knockout-7e736780bf"></a><code>script.enum-value.CritterCondition.Knockout</code> | <code>internal</code> (default) | [Source/Common/Common.h:904](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L904) | - |

<a id="symbol-script-enum-critterfindtype-f173bdcbf4"></a>
### <code>CritterFindType</code>

<code>enum CritterFindType : uint8</code>  
Symbol ID: <code>script.enum.CritterFindType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Any</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critterfindtype-any-758b9aeb5e"></a><code>script.enum-value.CritterFindType.Any</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>Dead</code> | <code>0x02</code> | 2 | <a id="symbol-script-enum-value-critterfindtype-dead-806a096c2c"></a><code>script.enum-value.CritterFindType.Dead</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>DeadNpc</code> | <code>0x22</code> | 34 | <a id="symbol-script-enum-value-critterfindtype-deadnpc-6c59ca1d9f"></a><code>script.enum-value.CritterFindType.DeadNpc</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>DeadPlayers</code> | <code>0x12</code> | 18 | <a id="symbol-script-enum-value-critterfindtype-deadplayers-3a99704def"></a><code>script.enum-value.CritterFindType.DeadPlayers</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>NonDead</code> | <code>0x01</code> | 1 | <a id="symbol-script-enum-value-critterfindtype-nondead-ddf4ff5f8a"></a><code>script.enum-value.CritterFindType.NonDead</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>NonDeadNpc</code> | <code>0x21</code> | 33 | <a id="symbol-script-enum-value-critterfindtype-nondeadnpc-ee3d30eb67"></a><code>script.enum-value.CritterFindType.NonDeadNpc</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>NonDeadPlayers</code> | <code>0x11</code> | 17 | <a id="symbol-script-enum-value-critterfindtype-nondeadplayers-11b2d40ccc"></a><code>script.enum-value.CritterFindType.NonDeadPlayers</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>Npc</code> | <code>0x20</code> | 32 | <a id="symbol-script-enum-value-critterfindtype-npc-42e615cf6b"></a><code>script.enum-value.CritterFindType.Npc</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |
| <code>Players</code> | <code>0x10</code> | 16 | <a id="symbol-script-enum-value-critterfindtype-players-44460b3235"></a><code>script.enum-value.CritterFindType.Players</code> | <code>internal</code> (default) | [Source/Common/Common.h:975](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L975) | - |

<a id="symbol-script-enum-critteritemslot-048a7d85c7"></a>
### <code>CritterItemSlot</code>

<code>enum CritterItemSlot : uint8</code>  
Symbol ID: <code>script.enum.CritterItemSlot</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:896](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L896)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Inventory</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critteritemslot-inventory-4408d91752"></a><code>script.enum-value.CritterItemSlot.Inventory</code> | <code>internal</code> (default) | [Source/Common/Common.h:896](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L896) | - |
| <code>Main</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-critteritemslot-main-03f296c567"></a><code>script.enum-value.CritterItemSlot.Main</code> | <code>internal</code> (default) | [Source/Common/Common.h:896](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L896) | - |
| <code>Outside</code> | <code>255</code> | 255 | <a id="symbol-script-enum-value-critteritemslot-outside-298d4a4413"></a><code>script.enum-value.CritterItemSlot.Outside</code> | <code>internal</code> (default) | [Source/Common/Common.h:896](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L896) | - |

<a id="symbol-script-enum-critterproperty-e40cb3adb6"></a>
### <code>CritterProperty</code>

<code>enum CritterProperty : uint16</code>  
Symbol ID: <code>script.enum.CritterProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AttachMaster</code> | <code>23</code> | 23 | <a id="symbol-script-enum-value-critterproperty-attachmaster-6f8020da2b"></a><code>script.enum-value.CritterProperty.AttachMaster</code> | <code>internal</code> (default) | - | - |
| <code>Condition</code> | <code>27</code> | 27 | <a id="symbol-script-enum-value-critterproperty-condition-51775a8d44"></a><code>script.enum-value.CritterProperty.Condition</code> | <code>internal</code> (default) | - | - |
| <code>ControlledByPlayer</code> | <code>19</code> | 19 | <a id="symbol-script-enum-value-critterproperty-controlledbyplayer-37a46f7a22"></a><code>script.enum-value.CritterProperty.ControlledByPlayer</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-critterproperty-customholderentry-e32fef9228"></a><code>script.enum-value.CritterProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-critterproperty-customholderid-4fda9b55b8"></a><code>script.enum-value.CritterProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>DeadDrawNoFlatten</code> | <code>30</code> | 30 | <a id="symbol-script-enum-value-critterproperty-deaddrawnoflatten-4215434298"></a><code>script.enum-value.CritterProperty.DeadDrawNoFlatten</code> | <code>internal</code> (default) | - | - |
| <code>Dir</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-critterproperty-dir-5ec6529b62"></a><code>script.enum-value.CritterProperty.Dir</code> | <code>internal</code> (default) | - | - |
| <code>Elevation</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-critterproperty-elevation-8b69921e39"></a><code>script.enum-value.CritterProperty.Elevation</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-critterproperty-explicitlypersistent-f8e07450eb"></a><code>script.enum-value.CritterProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>GlobalMapTripId</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-critterproperty-globalmaptripid-d0a375c6b4"></a><code>script.enum-value.CritterProperty.GlobalMapTripId</code> | <code>internal</code> (default) | - | - |
| <code>Hex</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-critterproperty-hex-17e7de1aa8"></a><code>script.enum-value.CritterProperty.Hex</code> | <code>internal</code> (default) | - | - |
| <code>HexOffset</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-critterproperty-hexoffset-fb6bd3bc1d"></a><code>script.enum-value.CritterProperty.HexOffset</code> | <code>internal</code> (default) | - | - |
| <code>HideSprite</code> | <code>24</code> | 24 | <a id="symbol-script-enum-value-critterproperty-hidesprite-8bcb45e9b0"></a><code>script.enum-value.CritterProperty.HideSprite</code> | <code>internal</code> (default) | - | - |
| <code>InitScript</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-critterproperty-initscript-f9ba540c1b"></a><code>script.enum-value.CritterProperty.InitScript</code> | <code>internal</code> (default) | - | - |
| <code>IsAttached</code> | <code>22</code> | 22 | <a id="symbol-script-enum-value-critterproperty-isattached-307fc0779d"></a><code>script.enum-value.CritterProperty.IsAttached</code> | <code>internal</code> (default) | - | - |
| <code>IsChosen</code> | <code>20</code> | 20 | <a id="symbol-script-enum-value-critterproperty-ischosen-7fcc9beba3"></a><code>script.enum-value.CritterProperty.IsChosen</code> | <code>internal</code> (default) | - | - |
| <code>IsPlayerOffline</code> | <code>21</code> | 21 | <a id="symbol-script-enum-value-critterproperty-isplayeroffline-5cac0380e6"></a><code>script.enum-value.CritterProperty.IsPlayerOffline</code> | <code>internal</code> (default) | - | - |
| <code>ItemIds</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-critterproperty-itemids-809bf6daa1"></a><code>script.enum-value.CritterProperty.ItemIds</code> | <code>internal</code> (default) | - | - |
| <code>LightColor</code> | <code>35</code> | 35 | <a id="symbol-script-enum-value-critterproperty-lightcolor-68b869d696"></a><code>script.enum-value.CritterProperty.LightColor</code> | <code>internal</code> (default) | - | - |
| <code>LightDistance</code> | <code>33</code> | 33 | <a id="symbol-script-enum-value-critterproperty-lightdistance-e6163a2f88"></a><code>script.enum-value.CritterProperty.LightDistance</code> | <code>internal</code> (default) | - | - |
| <code>LightFlags</code> | <code>34</code> | 34 | <a id="symbol-script-enum-value-critterproperty-lightflags-155e6b4c96"></a><code>script.enum-value.CritterProperty.LightFlags</code> | <code>internal</code> (default) | - | - |
| <code>LightIntensity</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-critterproperty-lightintensity-09fa18f54a"></a><code>script.enum-value.CritterProperty.LightIntensity</code> | <code>internal</code> (default) | - | - |
| <code>LightSource</code> | <code>31</code> | 31 | <a id="symbol-script-enum-value-critterproperty-lightsource-ea4d6f5d75"></a><code>script.enum-value.CritterProperty.LightSource</code> | <code>internal</code> (default) | - | - |
| <code>LookDistance</code> | <code>29</code> | 29 | <a id="symbol-script-enum-value-critterproperty-lookdistance-ef101f8e7b"></a><code>script.enum-value.CritterProperty.LookDistance</code> | <code>internal</code> (default) | - | - |
| <code>MapId</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-critterproperty-mapid-b847ec3015"></a><code>script.enum-value.CritterProperty.MapId</code> | <code>internal</code> (default) | - | - |
| <code>ModelLayers</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-critterproperty-modellayers-ca45dee3aa"></a><code>script.enum-value.CritterProperty.ModelLayers</code> | <code>internal</code> (default) | - | - |
| <code>ModelName</code> | <code>12</code> | 12 | <a id="symbol-script-enum-value-critterproperty-modelname-abe312515f"></a><code>script.enum-value.CritterProperty.ModelName</code> | <code>internal</code> (default) | - | - |
| <code>MovingSpeed</code> | <code>25</code> | 25 | <a id="symbol-script-enum-value-critterproperty-movingspeed-44dc8d39cb"></a><code>script.enum-value.CritterProperty.MovingSpeed</code> | <code>internal</code> (default) | - | - |
| <code>Multihex</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-critterproperty-multihex-351af1112c"></a><code>script.enum-value.CritterProperty.Multihex</code> | <code>internal</code> (default) | - | - |
| <code>NameOffset</code> | <code>28</code> | 28 | <a id="symbol-script-enum-value-critterproperty-nameoffset-c4385905d4"></a><code>script.enum-value.CritterProperty.NameOffset</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critterproperty-none-6c1bb2dd08"></a><code>script.enum-value.CritterProperty.None</code> | <code>internal</code> (default) | - | - |
| <code>ScaleFactor</code> | <code>14</code> | 14 | <a id="symbol-script-enum-value-critterproperty-scalefactor-b13568fd54"></a><code>script.enum-value.CritterProperty.ScaleFactor</code> | <code>internal</code> (default) | - | - |
| <code>ShowCritterDist1</code> | <code>15</code> | 15 | <a id="symbol-script-enum-value-critterproperty-showcritterdist1-940a8bdd6e"></a><code>script.enum-value.CritterProperty.ShowCritterDist1</code> | <code>internal</code> (default) | - | - |
| <code>ShowCritterDist2</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-critterproperty-showcritterdist2-5888c4c88a"></a><code>script.enum-value.CritterProperty.ShowCritterDist2</code> | <code>internal</code> (default) | - | - |
| <code>ShowCritterDist3</code> | <code>17</code> | 17 | <a id="symbol-script-enum-value-critterproperty-showcritterdist3-b2e0634556"></a><code>script.enum-value.CritterProperty.ShowCritterDist3</code> | <code>internal</code> (default) | - | - |
| <code>VisibilityMode</code> | <code>26</code> | 26 | <a id="symbol-script-enum-value-critterproperty-visibilitymode-f92e88b4c6"></a><code>script.enum-value.CritterProperty.VisibilityMode</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-critterseetype-f8dc891511"></a>
### <code>CritterSeeType</code>

<code>enum CritterSeeType : uint8</code>  
Symbol ID: <code>script.enum.CritterSeeType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:960](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L960)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Any</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critterseetype-any-fc4646a4c5"></a><code>script.enum-value.CritterSeeType.Any</code> | <code>internal</code> (default) | [Source/Common/Common.h:960](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L960) | - |
| <code>WhoISee</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-critterseetype-whoisee-14d3d5b178"></a><code>script.enum-value.CritterSeeType.WhoISee</code> | <code>internal</code> (default) | [Source/Common/Common.h:960](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L960) | - |
| <code>WhoSeeMe</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-critterseetype-whoseeme-143f0263bb"></a><code>script.enum-value.CritterSeeType.WhoSeeMe</code> | <code>internal</code> (default) | [Source/Common/Common.h:960](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L960) | - |

<a id="symbol-script-enum-critterstateanim-36ce6b6f9c"></a>
### <code>CritterStateAnim</code>

<code>enum CritterStateAnim : uint16</code>  
Symbol ID: <code>script.enum.CritterStateAnim</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:934](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L934)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-critterstateanim-none-19de5ffb41"></a><code>script.enum-value.CritterStateAnim.None</code> | <code>internal</code> (default) | [Source/Common/Common.h:934](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L934) | - |
| <code>Unarmed</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-critterstateanim-unarmed-3edc820536"></a><code>script.enum-value.CritterStateAnim.Unarmed</code> | <code>internal</code> (default) | [Source/Common/Common.h:934](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L934) | - |

<a id="symbol-script-enum-crittervisibilitymode-9fd00f2322"></a>
### <code>CritterVisibilityMode</code>

<code>enum CritterVisibilityMode : uint8</code>  
Symbol ID: <code>script.enum.CritterVisibilityMode</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:968](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L968)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Full</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-crittervisibilitymode-full-659bf2f786"></a><code>script.enum-value.CritterVisibilityMode.Full</code> | <code>internal</code> (default) | [Source/Common/Common.h:968](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L968) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-crittervisibilitymode-none-354691628c"></a><code>script.enum-value.CritterVisibilityMode.None</code> | <code>internal</code> (default) | [Source/Common/Common.h:968](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L968) | - |

<a id="symbol-script-enum-drawordertype-be7793e78b"></a>
### <code>DrawOrderType</code>

<code>enum DrawOrderType : uint8</code>  
Symbol ID: <code>script.enum.DrawOrderType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AfterLight</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-drawordertype-afterlight-9171346ff8"></a><code>script.enum-value.DrawOrderType.AfterLight</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Critter</code> | <code>25</code> | 25 | <a id="symbol-script-enum-value-drawordertype-critter-8e93d8426e"></a><code>script.enum-value.DrawOrderType.Critter</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>DeadCritter</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-drawordertype-deadcritter-15cc5f8cea"></a><code>script.enum-value.DrawOrderType.DeadCritter</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>FlatEnd</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-drawordertype-flatend-3818c1eef3"></a><code>script.enum-value.DrawOrderType.FlatEnd</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>FlatItemAfterLight</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-drawordertype-flatitemafterlight-a7a1bdfa84"></a><code>script.enum-value.DrawOrderType.FlatItemAfterLight</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>FlatItemPreLight</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-drawordertype-flatitemprelight-a7a417c418"></a><code>script.enum-value.DrawOrderType.FlatItemPreLight</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>HexGrid</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-drawordertype-hexgrid-f9701cea71"></a><code>script.enum-value.DrawOrderType.HexGrid</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Item</code> | <code>22</code> | 22 | <a id="symbol-script-enum-value-drawordertype-item-abfed1f149"></a><code>script.enum-value.DrawOrderType.Item</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Last</code> | <code>39</code> | 39 | <a id="symbol-script-enum-value-drawordertype-last-8dd3defc2c"></a><code>script.enum-value.DrawOrderType.Last</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Light</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-drawordertype-light-087ec83709"></a><code>script.enum-value.DrawOrderType.Light</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>NormalBegin</code> | <code>19</code> | 19 | <a id="symbol-script-enum-value-drawordertype-normalbegin-f7021edd8c"></a><code>script.enum-value.DrawOrderType.NormalBegin</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>NormalEnd</code> | <code>31</code> | 31 | <a id="symbol-script-enum-value-drawordertype-normalend-a566d04f33"></a><code>script.enum-value.DrawOrderType.NormalEnd</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Particles</code> | <code>28</code> | 28 | <a id="symbol-script-enum-value-drawordertype-particles-a0ea426acf"></a><code>script.enum-value.DrawOrderType.Particles</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>PreLight</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-drawordertype-prelight-7c25a5e24a"></a><code>script.enum-value.DrawOrderType.PreLight</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Roof</code> | <code>33</code> | 33 | <a id="symbol-script-enum-value-drawordertype-roof-ce93906e98"></a><code>script.enum-value.DrawOrderType.Roof</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Roof1</code> | <code>34</code> | 34 | <a id="symbol-script-enum-value-drawordertype-roof1-a13b7140e1"></a><code>script.enum-value.DrawOrderType.Roof1</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Roof2</code> | <code>35</code> | 35 | <a id="symbol-script-enum-value-drawordertype-roof2-a10b9d7760"></a><code>script.enum-value.DrawOrderType.Roof2</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Roof3</code> | <code>36</code> | 36 | <a id="symbol-script-enum-value-drawordertype-roof3-e4e5a97b3c"></a><code>script.enum-value.DrawOrderType.Roof3</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Roof4</code> | <code>37</code> | 37 | <a id="symbol-script-enum-value-drawordertype-roof4-b6f5316d89"></a><code>script.enum-value.DrawOrderType.Roof4</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>RoofParticles</code> | <code>38</code> | 38 | <a id="symbol-script-enum-value-drawordertype-roofparticles-147dcd8d14"></a><code>script.enum-value.DrawOrderType.RoofParticles</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Tile</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-drawordertype-tile-d27cf61595"></a><code>script.enum-value.DrawOrderType.Tile</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Tile1</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-drawordertype-tile1-2674db2367"></a><code>script.enum-value.DrawOrderType.Tile1</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Tile2</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-drawordertype-tile2-3370e1cbbd"></a><code>script.enum-value.DrawOrderType.Tile2</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Tile3</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-drawordertype-tile3-3a8e775479"></a><code>script.enum-value.DrawOrderType.Tile3</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |
| <code>Tile4</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-drawordertype-tile4-e18aafb41c"></a><code>script.enum-value.DrawOrderType.Tile4</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:48](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L48) | - |

<a id="symbol-script-enum-effecttype-1fd00760a6"></a>
### <code>EffectType</code>

<code>enum EffectType : uint32</code>  
Symbol ID: <code>script.enum.EffectType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CritterSprite</code> | <code>0x00000002</code> | 2 | <a id="symbol-script-enum-value-effecttype-crittersprite-42301d9fb1"></a><code>script.enum-value.EffectType.CritterSprite</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>FlushFog</code> | <code>0x20000000</code> | 536870912 | <a id="symbol-script-enum-value-effecttype-flushfog-0e4ceb6b80"></a><code>script.enum-value.EffectType.FlushFog</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>FlushLight</code> | <code>0x10000000</code> | 268435456 | <a id="symbol-script-enum-value-effecttype-flushlight-643fc54691"></a><code>script.enum-value.EffectType.FlushLight</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>FlushMap</code> | <code>0x08000000</code> | 134217728 | <a id="symbol-script-enum-value-effecttype-flushmap-641a660820"></a><code>script.enum-value.EffectType.FlushMap</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>FlushPrimitive</code> | <code>0x04000000</code> | 67108864 | <a id="symbol-script-enum-value-effecttype-flushprimitive-c43bb4c4f4"></a><code>script.enum-value.EffectType.FlushPrimitive</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>FlushRenderTarget</code> | <code>0x01000000</code> | 16777216 | <a id="symbol-script-enum-value-effecttype-flushrendertarget-d350da9412"></a><code>script.enum-value.EffectType.FlushRenderTarget</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>Fog</code> | <code>0x00400000</code> | 4194304 | <a id="symbol-script-enum-value-effecttype-fog-f2291da8d0"></a><code>script.enum-value.EffectType.Fog</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>Font</code> | <code>0x00010000</code> | 65536 | <a id="symbol-script-enum-value-effecttype-font-7ae8501af6"></a><code>script.enum-value.EffectType.Font</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>GenericSprite</code> | <code>0x00000001</code> | 1 | <a id="symbol-script-enum-value-effecttype-genericsprite-928964d5e8"></a><code>script.enum-value.EffectType.GenericSprite</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>Interface</code> | <code>0x00001000</code> | 4096 | <a id="symbol-script-enum-value-effecttype-interface-7e5f6faa9a"></a><code>script.enum-value.EffectType.Interface</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>Light</code> | <code>0x00200000</code> | 2097152 | <a id="symbol-script-enum-value-effecttype-light-54be6d5179"></a><code>script.enum-value.EffectType.Light</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-effecttype-none-3ca115d1b0"></a><code>script.enum-value.EffectType.None</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>Offscreen</code> | <code>0x40000000</code> | 1073741824 | <a id="symbol-script-enum-value-effecttype-offscreen-824708c6ae"></a><code>script.enum-value.EffectType.Offscreen</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>Primitive</code> | <code>0x00100000</code> | 1048576 | <a id="symbol-script-enum-value-effecttype-primitive-36d6f3c8d6"></a><code>script.enum-value.EffectType.Primitive</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>RainSprite</code> | <code>0x00000010</code> | 16 | <a id="symbol-script-enum-value-effecttype-rainsprite-fb160a2175"></a><code>script.enum-value.EffectType.RainSprite</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>RoofSprite</code> | <code>0x00000008</code> | 8 | <a id="symbol-script-enum-value-effecttype-roofsprite-560a72f7b0"></a><code>script.enum-value.EffectType.RoofSprite</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>SkinnedMesh</code> | <code>0x00000400</code> | 1024 | <a id="symbol-script-enum-value-effecttype-skinnedmesh-b620bcacd0"></a><code>script.enum-value.EffectType.SkinnedMesh</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |
| <code>TileSprite</code> | <code>0x00000004</code> | 4 | <a id="symbol-script-enum-value-effecttype-tilesprite-30e1eff247"></a><code>script.enum-value.EffectType.TileSprite</code> | <code>internal</code> (default) | [Source/Client/EffectManager.h:49](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h#L49) | - |

<a id="symbol-script-enum-eggappearencetype-ebf1416838"></a>
### <code>EggAppearenceType</code>

<code>enum EggAppearenceType : uint8</code>  
Symbol ID: <code>script.enum.EggAppearenceType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Always</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-eggappearencetype-always-63cdca5540"></a><code>script.enum-value.EggAppearenceType.Always</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84) | - |
| <code>ByX</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-eggappearencetype-byx-7f2e483597"></a><code>script.enum-value.EggAppearenceType.ByX</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84) | - |
| <code>ByXAndY</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-eggappearencetype-byxandy-b7fa559ebe"></a><code>script.enum-value.EggAppearenceType.ByXAndY</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84) | - |
| <code>ByXOrY</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-eggappearencetype-byxory-a55e7dd0db"></a><code>script.enum-value.EggAppearenceType.ByXOrY</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84) | - |
| <code>ByY</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-eggappearencetype-byy-5d69dae820"></a><code>script.enum-value.EggAppearenceType.ByY</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-eggappearencetype-none-3373355d69"></a><code>script.enum-value.EggAppearenceType.None</code> | <code>internal</code> (default) | [Source/Client/MapSprite.h:84](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L84) | - |

<a id="symbol-script-enum-engineinfomessage-eb0aa0fd80"></a>
### <code>EngineInfoMessage</code>

<code>enum EngineInfoMessage : uint16</code>  
Symbol ID: <code>script.enum.EngineInfoMessage</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>KickedFromGame</code> | <code>5000</code> | 5000 | <a id="symbol-script-enum-value-engineinfomessage-kickedfromgame-097acf4b41"></a><code>script.enum-value.EngineInfoMessage.KickedFromGame</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetBan</code> | <code>1046</code> | 1046 | <a id="symbol-script-enum-value-engineinfomessage-netban-d4540fc4b1"></a><code>script.enum-value.EngineInfoMessage.NetBan</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetBanReason</code> | <code>1047</code> | 1047 | <a id="symbol-script-enum-value-engineinfomessage-netbanreason-bb6fe5b413"></a><code>script.enum-value.EngineInfoMessage.NetBanReason</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetBanned</code> | <code>1034</code> | 1034 | <a id="symbol-script-enum-value-engineinfomessage-netbanned-32ed789f19"></a><code>script.enum-value.EngineInfoMessage.NetBanned</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetBannedIp</code> | <code>1043</code> | 1043 | <a id="symbol-script-enum-value-engineinfomessage-netbannedip-7af6c55008"></a><code>script.enum-value.EngineInfoMessage.NetBannedIp</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetBdError</code> | <code>1023</code> | 1023 | <a id="symbol-script-enum-value-engineinfomessage-netbderror-5064befe7d"></a><code>script.enum-value.EngineInfoMessage.NetBdError</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetBeginEndSpaces</code> | <code>1032</code> | 1032 | <a id="symbol-script-enum-value-engineinfomessage-netbeginendspaces-e2f5ccf473"></a><code>script.enum-value.EngineInfoMessage.NetBeginEndSpaces</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetConnError</code> | <code>1008</code> | 1008 | <a id="symbol-script-enum-value-engineinfomessage-netconnerror-2356e96db0"></a><code>script.enum-value.EngineInfoMessage.NetConnError</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetConnFail</code> | <code>1018</code> | 1018 | <a id="symbol-script-enum-value-engineinfomessage-netconnfail-0d95e499a0"></a><code>script.enum-value.EngineInfoMessage.NetConnFail</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetConnSuccess</code> | <code>1010</code> | 1010 | <a id="symbol-script-enum-value-engineinfomessage-netconnsuccess-aa67573ca6"></a><code>script.enum-value.EngineInfoMessage.NetConnSuccess</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetConnection</code> | <code>1007</code> | 1007 | <a id="symbol-script-enum-value-engineinfomessage-netconnection-e2ad51dc5d"></a><code>script.enum-value.EngineInfoMessage.NetConnection</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetDataTransErr</code> | <code>1025</code> | 1025 | <a id="symbol-script-enum-value-engineinfomessage-netdatatranserr-cdd6fd26d1"></a><code>script.enum-value.EngineInfoMessage.NetDataTransErr</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetDifferentLang</code> | <code>1030</code> | 1030 | <a id="symbol-script-enum-value-engineinfomessage-netdifferentlang-52d7db7f45"></a><code>script.enum-value.EngineInfoMessage.NetDifferentLang</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetDisconnByDemand</code> | <code>1013</code> | 1013 | <a id="symbol-script-enum-value-engineinfomessage-netdisconnbydemand-8790c6f93a"></a><code>script.enum-value.EngineInfoMessage.NetDisconnByDemand</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetFailRunStartScript</code> | <code>1038</code> | 1038 | <a id="symbol-script-enum-value-engineinfomessage-netfailrunstartscript-6a18533bba"></a><code>script.enum-value.EngineInfoMessage.NetFailRunStartScript</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetFailToLoadIface</code> | <code>1037</code> | 1037 | <a id="symbol-script-enum-value-engineinfomessage-netfailtoloadiface-3c6d0e49f2"></a><code>script.enum-value.EngineInfoMessage.NetFailToLoadIface</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetHexesBusy</code> | <code>1012</code> | 1012 | <a id="symbol-script-enum-value-engineinfomessage-nethexesbusy-58c9ad500c"></a><code>script.enum-value.EngineInfoMessage.NetHexesBusy</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetKnockKnock</code> | <code>1041</code> | 1041 | <a id="symbol-script-enum-value-engineinfomessage-netknockknock-39d7b473fb"></a><code>script.enum-value.EngineInfoMessage.NetKnockKnock</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetLanguageNotSupported</code> | <code>1039</code> | 1039 | <a id="symbol-script-enum-value-engineinfomessage-netlanguagenotsupported-a15ebfd2ba"></a><code>script.enum-value.EngineInfoMessage.NetLanguageNotSupported</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetLoginOk</code> | <code>1028</code> | 1028 | <a id="symbol-script-enum-value-engineinfomessage-netloginok-8f66788736"></a><code>script.enum-value.EngineInfoMessage.NetLoginOk</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetLoginScriptFail</code> | <code>1048</code> | 1048 | <a id="symbol-script-enum-value-engineinfomessage-netloginscriptfail-b4f3577626"></a><code>script.enum-value.EngineInfoMessage.NetLoginScriptFail</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetManySymbols</code> | <code>1031</code> | 1031 | <a id="symbol-script-enum-value-engineinfomessage-netmanysymbols-f0896dd106"></a><code>script.enum-value.EngineInfoMessage.NetManySymbols</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetNameWrongChars</code> | <code>1035</code> | 1035 | <a id="symbol-script-enum-value-engineinfomessage-netnamewrongchars-aecf8e19c4"></a><code>script.enum-value.EngineInfoMessage.NetNameWrongChars</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetNetMsgErr</code> | <code>1026</code> | 1026 | <a id="symbol-script-enum-value-engineinfomessage-netnetmsgerr-744fd68628"></a><code>script.enum-value.EngineInfoMessage.NetNetMsgErr</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetPassWrongChars</code> | <code>1036</code> | 1036 | <a id="symbol-script-enum-value-engineinfomessage-netpasswrongchars-6f8a83a8f3"></a><code>script.enum-value.EngineInfoMessage.NetPassWrongChars</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetPermanentDeath</code> | <code>1049</code> | 1049 | <a id="symbol-script-enum-value-engineinfomessage-netpermanentdeath-7e783217fd"></a><code>script.enum-value.EngineInfoMessage.NetPermanentDeath</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetPlayerAlready</code> | <code>1003</code> | 1003 | <a id="symbol-script-enum-value-engineinfomessage-netplayeralready-cece50913b"></a><code>script.enum-value.EngineInfoMessage.NetPlayerAlready</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetPlayerInGame</code> | <code>1004</code> | 1004 | <a id="symbol-script-enum-value-engineinfomessage-netplayeringame-aaaf630586"></a><code>script.enum-value.EngineInfoMessage.NetPlayerInGame</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetSetProtoErr</code> | <code>1027</code> | 1027 | <a id="symbol-script-enum-value-engineinfomessage-netsetprotoerr-2d97805d8a"></a><code>script.enum-value.EngineInfoMessage.NetSetProtoErr</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetStartCoordFail</code> | <code>1022</code> | 1022 | <a id="symbol-script-enum-value-engineinfomessage-netstartcoordfail-baa9ad86f2"></a><code>script.enum-value.EngineInfoMessage.NetStartCoordFail</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetStartLocFail</code> | <code>1020</code> | 1020 | <a id="symbol-script-enum-value-engineinfomessage-netstartlocfail-8c02af9fb8"></a><code>script.enum-value.EngineInfoMessage.NetStartLocFail</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetStartMapFail</code> | <code>1021</code> | 1021 | <a id="symbol-script-enum-value-engineinfomessage-netstartmapfail-6cfc227443"></a><code>script.enum-value.EngineInfoMessage.NetStartMapFail</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetTimeLeft</code> | <code>1045</code> | 1045 | <a id="symbol-script-enum-value-engineinfomessage-nettimeleft-7885fbf05a"></a><code>script.enum-value.EngineInfoMessage.NetTimeLeft</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetTwoSpace</code> | <code>1033</code> | 1033 | <a id="symbol-script-enum-value-engineinfomessage-nettwospace-d447304a66"></a><code>script.enum-value.EngineInfoMessage.NetTwoSpace</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetWrongLogin</code> | <code>1001</code> | 1001 | <a id="symbol-script-enum-value-engineinfomessage-netwronglogin-2e412e22b9"></a><code>script.enum-value.EngineInfoMessage.NetWrongLogin</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetWrongNetProto</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-engineinfomessage-netwrongnetproto-1278e9d4b3"></a><code>script.enum-value.EngineInfoMessage.NetWrongNetProto</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetWrongPass</code> | <code>1002</code> | 1002 | <a id="symbol-script-enum-value-engineinfomessage-netwrongpass-71a3a7cba8"></a><code>script.enum-value.EngineInfoMessage.NetWrongPass</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>NetWrongTagSkill</code> | <code>1029</code> | 1029 | <a id="symbol-script-enum-value-engineinfomessage-netwrongtagskill-fe6472051b"></a><code>script.enum-value.EngineInfoMessage.NetWrongTagSkill</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-engineinfomessage-none-e3a03105e2"></a><code>script.enum-value.EngineInfoMessage.None</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |
| <code>ServerLog</code> | <code>5001</code> | 5001 | <a id="symbol-script-enum-value-engineinfomessage-serverlog-bb9788eff7"></a><code>script.enum-value.EngineInfoMessage.ServerLog</code> | <code>internal</code> (default) | [Source/Common/Common.h:386](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L386) | - |

<a id="symbol-script-enum-eventpriority-bf3411f780"></a>
### <code>EventPriority</code>

<code>enum EventPriority : int32</code>  
Symbol ID: <code>script.enum.EventPriority</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Entity.h:150](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L150)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>High</code> | <code>3000000</code> | 3000000 | <a id="symbol-script-enum-value-eventpriority-high-8fab900717"></a><code>script.enum-value.EventPriority.High</code> | <code>internal</code> (default) | [Source/Common/Entity.h:150](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L150) | - |
| <code>Highest</code> | <code>4000000</code> | 4000000 | <a id="symbol-script-enum-value-eventpriority-highest-27ce6d091e"></a><code>script.enum-value.EventPriority.Highest</code> | <code>internal</code> (default) | [Source/Common/Entity.h:150](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L150) | - |
| <code>Low</code> | <code>1000000</code> | 1000000 | <a id="symbol-script-enum-value-eventpriority-low-d6541c53d4"></a><code>script.enum-value.EventPriority.Low</code> | <code>internal</code> (default) | [Source/Common/Entity.h:150](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L150) | - |
| <code>Lowest</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-eventpriority-lowest-332c21403c"></a><code>script.enum-value.EventPriority.Lowest</code> | <code>internal</code> (default) | [Source/Common/Entity.h:150](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L150) | - |
| <code>Normal</code> | <code>2000000</code> | 2000000 | <a id="symbol-script-enum-value-eventpriority-normal-7dedb4329b"></a><code>script.enum-value.EventPriority.Normal</code> | <code>internal</code> (default) | [Source/Common/Entity.h:150](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L150) | - |

<a id="symbol-script-enum-eventresult-399e7f00ad"></a>
### <code>EventResult</code>

<code>enum EventResult : int32</code>  
Symbol ID: <code>script.enum.EventResult</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Entity.h:141](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L141)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>ContinueChain</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-eventresult-continuechain-888de62f91"></a><code>script.enum-value.EventResult.ContinueChain</code> | <code>internal</code> (default) | [Source/Common/Entity.h:141](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L141) | - |
| <code>StopChain</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-eventresult-stopchain-1c1b213146"></a><code>script.enum-value.EventResult.StopChain</code> | <code>internal</code> (default) | [Source/Common/Entity.h:141](https://github.com/cvet/fonline/blob/master/Source/Common/Entity.h#L141) | - |

<a id="symbol-script-enum-fontflag-a5a7fdebc9"></a>
### <code>FontFlag</code>

<code>enum FontFlag : uint32</code>  
Symbol ID: <code>script.enum.FontFlag</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64)

Font flags

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AlignBottom</code> | <code>0x0020</code> | 32 | <a id="symbol-script-enum-value-fontflag-alignbottom-c85522ac3b"></a><code>script.enum-value.FontFlag.AlignBottom</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>AlignRight</code> | <code>0x0010</code> | 16 | <a id="symbol-script-enum-value-fontflag-alignright-2f62cdbd9d"></a><code>script.enum-value.FontFlag.AlignRight</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>Bordered</code> | <code>0x0200</code> | 512 | <a id="symbol-script-enum-value-fontflag-bordered-feaecfbdb1"></a><code>script.enum-value.FontFlag.Bordered</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>CenterX</code> | <code>0x0004</code> | 4 | <a id="symbol-script-enum-value-fontflag-centerx-c09eb136d9"></a><code>script.enum-value.FontFlag.CenterX</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>CenterY</code> | <code>0x0008</code> | 8 | <a id="symbol-script-enum-value-fontflag-centery-0d77b06ad7"></a><code>script.enum-value.FontFlag.CenterY</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>Justify</code> | <code>0x0100</code> | 256 | <a id="symbol-script-enum-value-fontflag-justify-4108749baa"></a><code>script.enum-value.FontFlag.Justify</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>KeepTail</code> | <code>0x0040</code> | 64 | <a id="symbol-script-enum-value-fontflag-keeptail-c7d0dd6d29"></a><code>script.enum-value.FontFlag.KeepTail</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>NoColorize</code> | <code>0x0080</code> | 128 | <a id="symbol-script-enum-value-fontflag-nocolorize-329097bc05"></a><code>script.enum-value.FontFlag.NoColorize</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>NoWrap</code> | <code>0x0001</code> | 1 | <a id="symbol-script-enum-value-fontflag-nowrap-6d4f2c759c"></a><code>script.enum-value.FontFlag.NoWrap</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-fontflag-none-d1a8845209"></a><code>script.enum-value.FontFlag.None</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |
| <code>TruncateLine</code> | <code>0x0002</code> | 2 | <a id="symbol-script-enum-value-fontflag-truncateline-9880234238"></a><code>script.enum-value.FontFlag.TruncateLine</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:64](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L64) | - |

<a id="symbol-script-enum-fonttype-7d77843f47"></a>
### <code>FontType</code>

<code>enum FontType : int32</code>  
Symbol ID: <code>script.enum.FontType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Client/FontManager.h:57](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L57)

Font slot index. Engine ships a single named slot (Default = 0); scripts may add more entries via the codegen Enum annotation.

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Default</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-fonttype-default-550c374418"></a><code>script.enum-value.FontType.Default</code> | <code>internal</code> (default) | [Source/Client/FontManager.h:57](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L57) | - |

<a id="symbol-script-enum-gameproperty-353beafc54"></a>
### <code>GameProperty</code>

<code>enum GameProperty : uint16</code>  
Symbol ID: <code>script.enum.GameProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-gameproperty-customholderentry-efa0d5f011"></a><code>script.enum-value.GameProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-gameproperty-customholderid-4981c06906"></a><code>script.enum-value.GameProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-gameproperty-explicitlypersistent-5d4ecc1d37"></a><code>script.enum-value.GameProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>FrameDeltaTime</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-gameproperty-framedeltatime-a5eaefcf21"></a><code>script.enum-value.GameProperty.FrameDeltaTime</code> | <code>internal</code> (default) | - | - |
| <code>FrameTime</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-gameproperty-frametime-18fbcc754a"></a><code>script.enum-value.GameProperty.FrameTime</code> | <code>internal</code> (default) | - | - |
| <code>FramesPerSecond</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-gameproperty-framespersecond-f4ebb7f904"></a><code>script.enum-value.GameProperty.FramesPerSecond</code> | <code>internal</code> (default) | - | - |
| <code>GlobalDayTime</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-gameproperty-globaldaytime-a15e90c5c0"></a><code>script.enum-value.GameProperty.GlobalDayTime</code> | <code>internal</code> (default) | - | - |
| <code>HistoryRecordsId</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-gameproperty-historyrecordsid-55976a5ca7"></a><code>script.enum-value.GameProperty.HistoryRecordsId</code> | <code>internal</code> (default) | - | - |
| <code>LastEntityId</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-gameproperty-lastentityid-ea6bcc0f02"></a><code>script.enum-value.GameProperty.LastEntityId</code> | <code>internal</code> (default) | - | - |
| <code>LastGlobalMapTripId</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-gameproperty-lastglobalmaptripid-993eaf78c1"></a><code>script.enum-value.GameProperty.LastGlobalMapTripId</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-gameproperty-none-586efde487"></a><code>script.enum-value.GameProperty.None</code> | <code>internal</code> (default) | - | - |
| <code>SynchronizedTime</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-gameproperty-synchronizedtime-06761ddf3e"></a><code>script.enum-value.GameProperty.SynchronizedTime</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-imguiproperty-63cdcb5d96"></a>
### <code>ImGuiProperty</code>

<code>enum ImGuiProperty : uint16</code>  
Symbol ID: <code>script.enum.ImGuiProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imguiproperty-customholderentry-9a4c7b83cf"></a><code>script.enum-value.ImGuiProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imguiproperty-customholderid-9e5ae47a26"></a><code>script.enum-value.ImGuiProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-imguiproperty-explicitlypersistent-23ae4875f6"></a><code>script.enum-value.ImGuiProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imguiproperty-none-10a7205599"></a><code>script.enum-value.ImGuiProperty.None</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-imgui-buttonflags-c6abda3943"></a>
### <code>ImGui_ButtonFlags</code>

<code>enum ImGui_ButtonFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_ButtonFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:382](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L382)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>EnableNav</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-buttonflags-enablenav-e8fc6143d7"></a><code>script.enum-value.ImGui_ButtonFlags.EnableNav</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:382](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L382) | - |
| <code>MouseButtonLeft</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-buttonflags-mousebuttonleft-fc135b9fcd"></a><code>script.enum-value.ImGui_ButtonFlags.MouseButtonLeft</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:382](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L382) | - |
| <code>MouseButtonMiddle</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-buttonflags-mousebuttonmiddle-b839abacfa"></a><code>script.enum-value.ImGui_ButtonFlags.MouseButtonMiddle</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:382](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L382) | - |
| <code>MouseButtonRight</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-buttonflags-mousebuttonright-aff4285c56"></a><code>script.enum-value.ImGui_ButtonFlags.MouseButtonRight</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:382](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L382) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-buttonflags-none-dd86ff3c93"></a><code>script.enum-value.ImGui_ButtonFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:382](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L382) | - |

<a id="symbol-script-enum-imgui-childflags-68e5f17e89"></a>
### <code>ImGui_ChildFlags</code>

<code>enum ImGui_ChildFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_ChildFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AlwaysAutoResize</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-childflags-alwaysautoresize-4683501d4a"></a><code>script.enum-value.ImGui_ChildFlags.AlwaysAutoResize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>AlwaysUseWindowPadding</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-childflags-alwaysusewindowpadding-6b23fa2beb"></a><code>script.enum-value.ImGui_ChildFlags.AlwaysUseWindowPadding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>AutoResizeX</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-childflags-autoresizex-921838976b"></a><code>script.enum-value.ImGui_ChildFlags.AutoResizeX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>AutoResizeY</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-childflags-autoresizey-7b634da36c"></a><code>script.enum-value.ImGui_ChildFlags.AutoResizeY</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>Border</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-childflags-border-cc0cb12879"></a><code>script.enum-value.ImGui_ChildFlags.Border</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>FrameStyle</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-childflags-framestyle-3043d7af46"></a><code>script.enum-value.ImGui_ChildFlags.FrameStyle</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>NavFlattened</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-childflags-navflattened-ee1d458471"></a><code>script.enum-value.ImGui_ChildFlags.NavFlattened</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-childflags-none-0a6b0d8c91"></a><code>script.enum-value.ImGui_ChildFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>ResizeX</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-childflags-resizex-02017f6c83"></a><code>script.enum-value.ImGui_ChildFlags.ResizeX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |
| <code>ResizeY</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-childflags-resizey-aedac8f11d"></a><code>script.enum-value.ImGui_ChildFlags.ResizeY</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:100](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L100) | - |

<a id="symbol-script-enum-imgui-col-25a0242522"></a>
### <code>ImGui_Col</code>

<code>enum ImGui_Col : int32</code>  
Symbol ID: <code>script.enum.ImGui_Col</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Border</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-imgui-col-border-0480104e2f"></a><code>script.enum-value.ImGui_Col.Border</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>Button</code> | <code>22</code> | 22 | <a id="symbol-script-enum-value-imgui-col-button-3f85cd606a"></a><code>script.enum-value.ImGui_Col.Button</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ButtonActive</code> | <code>24</code> | 24 | <a id="symbol-script-enum-value-imgui-col-buttonactive-7536e4817d"></a><code>script.enum-value.ImGui_Col.ButtonActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ButtonHovered</code> | <code>23</code> | 23 | <a id="symbol-script-enum-value-imgui-col-buttonhovered-fdda0c4bba"></a><code>script.enum-value.ImGui_Col.ButtonHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>CheckMark</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-imgui-col-checkmark-8d1e5e7a84"></a><code>script.enum-value.ImGui_Col.CheckMark</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ChildBg</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-imgui-col-childbg-99bbea4fe0"></a><code>script.enum-value.ImGui_Col.ChildBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>FrameBg</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-imgui-col-framebg-a836b9cbeb"></a><code>script.enum-value.ImGui_Col.FrameBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>FrameBgActive</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-imgui-col-framebgactive-effcb09f9c"></a><code>script.enum-value.ImGui_Col.FrameBgActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>FrameBgHovered</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-col-framebghovered-11402ed9fc"></a><code>script.enum-value.ImGui_Col.FrameBgHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>Header</code> | <code>25</code> | 25 | <a id="symbol-script-enum-value-imgui-col-header-dd42491a70"></a><code>script.enum-value.ImGui_Col.Header</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>HeaderActive</code> | <code>27</code> | 27 | <a id="symbol-script-enum-value-imgui-col-headeractive-c00f9361f4"></a><code>script.enum-value.ImGui_Col.HeaderActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>HeaderHovered</code> | <code>26</code> | 26 | <a id="symbol-script-enum-value-imgui-col-headerhovered-2cec684567"></a><code>script.enum-value.ImGui_Col.HeaderHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>MenuBarBg</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-imgui-col-menubarbg-67abdf6b83"></a><code>script.enum-value.ImGui_Col.MenuBarBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>PopupBg</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-col-popupbg-e1db6d8cdf"></a><code>script.enum-value.ImGui_Col.PopupBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ResizeGrip</code> | <code>31</code> | 31 | <a id="symbol-script-enum-value-imgui-col-resizegrip-8adc6c6521"></a><code>script.enum-value.ImGui_Col.ResizeGrip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ResizeGripActive</code> | <code>33</code> | 33 | <a id="symbol-script-enum-value-imgui-col-resizegripactive-b8ec92eedf"></a><code>script.enum-value.ImGui_Col.ResizeGripActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ResizeGripHovered</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-col-resizegriphovered-539ff8964c"></a><code>script.enum-value.ImGui_Col.ResizeGripHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ScrollbarBg</code> | <code>14</code> | 14 | <a id="symbol-script-enum-value-imgui-col-scrollbarbg-a16f652eaa"></a><code>script.enum-value.ImGui_Col.ScrollbarBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>ScrollbarGrab</code> | <code>15</code> | 15 | <a id="symbol-script-enum-value-imgui-col-scrollbargrab-8dd45b5631"></a><code>script.enum-value.ImGui_Col.ScrollbarGrab</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>Separator</code> | <code>28</code> | 28 | <a id="symbol-script-enum-value-imgui-col-separator-a1c1076f4f"></a><code>script.enum-value.ImGui_Col.Separator</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>SeparatorActive</code> | <code>30</code> | 30 | <a id="symbol-script-enum-value-imgui-col-separatoractive-29e43769e0"></a><code>script.enum-value.ImGui_Col.SeparatorActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>SeparatorHovered</code> | <code>29</code> | 29 | <a id="symbol-script-enum-value-imgui-col-separatorhovered-67ff3f1440"></a><code>script.enum-value.ImGui_Col.SeparatorHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>SliderGrab</code> | <code>20</code> | 20 | <a id="symbol-script-enum-value-imgui-col-slidergrab-ab7f6361fa"></a><code>script.enum-value.ImGui_Col.SliderGrab</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>SliderGrabActive</code> | <code>21</code> | 21 | <a id="symbol-script-enum-value-imgui-col-slidergrabactive-a4629394cc"></a><code>script.enum-value.ImGui_Col.SliderGrabActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>Tab</code> | <code>36</code> | 36 | <a id="symbol-script-enum-value-imgui-col-tab-c9eeed4fd3"></a><code>script.enum-value.ImGui_Col.Tab</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TabDimmed</code> | <code>39</code> | 39 | <a id="symbol-script-enum-value-imgui-col-tabdimmed-7bbb937408"></a><code>script.enum-value.ImGui_Col.TabDimmed</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TabDimmedSelected</code> | <code>40</code> | 40 | <a id="symbol-script-enum-value-imgui-col-tabdimmedselected-d125222bc6"></a><code>script.enum-value.ImGui_Col.TabDimmedSelected</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TabHovered</code> | <code>35</code> | 35 | <a id="symbol-script-enum-value-imgui-col-tabhovered-c0045167b2"></a><code>script.enum-value.ImGui_Col.TabHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TabSelected</code> | <code>37</code> | 37 | <a id="symbol-script-enum-value-imgui-col-tabselected-c57e2f38d9"></a><code>script.enum-value.ImGui_Col.TabSelected</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TableHeaderBg</code> | <code>46</code> | 46 | <a id="symbol-script-enum-value-imgui-col-tableheaderbg-e65e805b53"></a><code>script.enum-value.ImGui_Col.TableHeaderBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TableRowBg</code> | <code>49</code> | 49 | <a id="symbol-script-enum-value-imgui-col-tablerowbg-f25aaf1344"></a><code>script.enum-value.ImGui_Col.TableRowBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TableRowBgAlt</code> | <code>50</code> | 50 | <a id="symbol-script-enum-value-imgui-col-tablerowbgalt-b99bb82eeb"></a><code>script.enum-value.ImGui_Col.TableRowBgAlt</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>Text</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-col-text-081665f042"></a><code>script.enum-value.ImGui_Col.Text</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TextDisabled</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-col-textdisabled-bd843b6cf9"></a><code>script.enum-value.ImGui_Col.TextDisabled</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TitleBg</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-imgui-col-titlebg-06b0b3b882"></a><code>script.enum-value.ImGui_Col.TitleBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>TitleBgActive</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-imgui-col-titlebgactive-52d07a3bce"></a><code>script.enum-value.ImGui_Col.TitleBgActive</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |
| <code>WindowBg</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-col-windowbg-95ff369998"></a><code>script.enum-value.ImGui_Col.WindowBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:423](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L423) | - |

<a id="symbol-script-enum-imgui-coloreditflags-497a70d0a5"></a>
### <code>ImGui_ColorEditFlags</code>

<code>enum ImGui_ColorEditFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_ColorEditFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AlphaBar</code> | <code>262144</code> | 262144 | <a id="symbol-script-enum-value-imgui-coloreditflags-alphabar-7bceaddfc3"></a><code>script.enum-value.ImGui_ColorEditFlags.AlphaBar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>AlphaNoBg</code> | <code>8192</code> | 8192 | <a id="symbol-script-enum-value-imgui-coloreditflags-alphanobg-68d98c9ccd"></a><code>script.enum-value.ImGui_ColorEditFlags.AlphaNoBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>AlphaOpaque</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-coloreditflags-alphaopaque-919134fcb3"></a><code>script.enum-value.ImGui_ColorEditFlags.AlphaOpaque</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>AlphaPreviewHalf</code> | <code>16384</code> | 16384 | <a id="symbol-script-enum-value-imgui-coloreditflags-alphapreviewhalf-f96fcbb85a"></a><code>script.enum-value.ImGui_ColorEditFlags.AlphaPreviewHalf</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>DefaultOptions</code> | <code>177209344</code> | 177209344 | <a id="symbol-script-enum-value-imgui-coloreditflags-defaultoptions-345795b2c2"></a><code>script.enum-value.ImGui_ColorEditFlags.DefaultOptions</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>DisplayHSV</code> | <code>2097152</code> | 2097152 | <a id="symbol-script-enum-value-imgui-coloreditflags-displayhsv-76049be830"></a><code>script.enum-value.ImGui_ColorEditFlags.DisplayHSV</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>DisplayHex</code> | <code>4194304</code> | 4194304 | <a id="symbol-script-enum-value-imgui-coloreditflags-displayhex-687ef8e5c4"></a><code>script.enum-value.ImGui_ColorEditFlags.DisplayHex</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>DisplayRGB</code> | <code>1048576</code> | 1048576 | <a id="symbol-script-enum-value-imgui-coloreditflags-displayrgb-f1b5a25fca"></a><code>script.enum-value.ImGui_ColorEditFlags.DisplayRGB</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>Float</code> | <code>16777216</code> | 16777216 | <a id="symbol-script-enum-value-imgui-coloreditflags-float-57dc94a6b5"></a><code>script.enum-value.ImGui_ColorEditFlags.Float</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>HDR</code> | <code>524288</code> | 524288 | <a id="symbol-script-enum-value-imgui-coloreditflags-hdr-c86bf0cd0c"></a><code>script.enum-value.ImGui_ColorEditFlags.HDR</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>InputHSV</code> | <code>268435456</code> | 268435456 | <a id="symbol-script-enum-value-imgui-coloreditflags-inputhsv-5693533d7d"></a><code>script.enum-value.ImGui_ColorEditFlags.InputHSV</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>InputRGB</code> | <code>134217728</code> | 134217728 | <a id="symbol-script-enum-value-imgui-coloreditflags-inputrgb-298ae9fa83"></a><code>script.enum-value.ImGui_ColorEditFlags.InputRGB</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoAlpha</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-coloreditflags-noalpha-8a2d8b9660"></a><code>script.enum-value.ImGui_ColorEditFlags.NoAlpha</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoBorder</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-coloreditflags-noborder-849a869ae0"></a><code>script.enum-value.ImGui_ColorEditFlags.NoBorder</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoDragDrop</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-coloreditflags-nodragdrop-dcacab60a3"></a><code>script.enum-value.ImGui_ColorEditFlags.NoDragDrop</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoInputs</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-coloreditflags-noinputs-8fa7d6098a"></a><code>script.enum-value.ImGui_ColorEditFlags.NoInputs</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoLabel</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-coloreditflags-nolabel-e3f2c40c1b"></a><code>script.enum-value.ImGui_ColorEditFlags.NoLabel</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoOptions</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-coloreditflags-nooptions-a9cadbf410"></a><code>script.enum-value.ImGui_ColorEditFlags.NoOptions</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoPicker</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-coloreditflags-nopicker-e6200c47f3"></a><code>script.enum-value.ImGui_ColorEditFlags.NoPicker</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoSidePreview</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-coloreditflags-nosidepreview-5147ea28b9"></a><code>script.enum-value.ImGui_ColorEditFlags.NoSidePreview</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoSmallPreview</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-coloreditflags-nosmallpreview-65c02cebe5"></a><code>script.enum-value.ImGui_ColorEditFlags.NoSmallPreview</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>NoTooltip</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-coloreditflags-notooltip-4a799b29ca"></a><code>script.enum-value.ImGui_ColorEditFlags.NoTooltip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-coloreditflags-none-8f6e3cf3f4"></a><code>script.enum-value.ImGui_ColorEditFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>PickerHueBar</code> | <code>33554432</code> | 33554432 | <a id="symbol-script-enum-value-imgui-coloreditflags-pickerhuebar-ba86114611"></a><code>script.enum-value.ImGui_ColorEditFlags.PickerHueBar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>PickerHueWheel</code> | <code>67108864</code> | 67108864 | <a id="symbol-script-enum-value-imgui-coloreditflags-pickerhuewheel-47ec0cf22d"></a><code>script.enum-value.ImGui_ColorEditFlags.PickerHueWheel</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |
| <code>Uint8</code> | <code>8388608</code> | 8388608 | <a id="symbol-script-enum-value-imgui-coloreditflags-uint8-3bc3dd4b25"></a><code>script.enum-value.ImGui_ColorEditFlags.Uint8</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:392](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L392) | - |

<a id="symbol-script-enum-imgui-comboflags-b867b29182"></a>
### <code>ImGui_ComboFlags</code>

<code>enum ImGui_ComboFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_ComboFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>HeightLarge</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-comboflags-heightlarge-e0b141f243"></a><code>script.enum-value.ImGui_ComboFlags.HeightLarge</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>HeightLargest</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-comboflags-heightlargest-d617a89f8a"></a><code>script.enum-value.ImGui_ComboFlags.HeightLargest</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>HeightRegular</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-comboflags-heightregular-2a680d6081"></a><code>script.enum-value.ImGui_ComboFlags.HeightRegular</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>HeightSmall</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-comboflags-heightsmall-fde17c6c42"></a><code>script.enum-value.ImGui_ComboFlags.HeightSmall</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>NoArrowButton</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-comboflags-noarrowbutton-b0be93da8f"></a><code>script.enum-value.ImGui_ComboFlags.NoArrowButton</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>NoPreview</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-comboflags-nopreview-75af79cbe7"></a><code>script.enum-value.ImGui_ComboFlags.NoPreview</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-comboflags-none-d67a49c01f"></a><code>script.enum-value.ImGui_ComboFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>PopupAlignLeft</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-comboflags-popupalignleft-ed9f864b07"></a><code>script.enum-value.ImGui_ComboFlags.PopupAlignLeft</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |
| <code>WidthFitPreview</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-comboflags-widthfitpreview-83126ffa22"></a><code>script.enum-value.ImGui_ComboFlags.WidthFitPreview</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:305](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L305) | - |

<a id="symbol-script-enum-imgui-cond-d82c79fb28"></a>
### <code>ImGui_Cond</code>

<code>enum ImGui_Cond : uint32</code>  
Symbol ID: <code>script.enum.ImGui_Cond</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:115](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L115)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Always</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-cond-always-91db7d1f4e"></a><code>script.enum-value.ImGui_Cond.Always</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:115](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L115) | - |
| <code>Appearing</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-cond-appearing-a79ee37bcc"></a><code>script.enum-value.ImGui_Cond.Appearing</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:115](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L115) | - |
| <code>FirstUseEver</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-cond-firstuseever-2647abacd0"></a><code>script.enum-value.ImGui_Cond.FirstUseEver</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:115](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L115) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-cond-none-aafa7ccd1b"></a><code>script.enum-value.ImGui_Cond.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:115](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L115) | - |
| <code>Once</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-cond-once-95bb040899"></a><code>script.enum-value.ImGui_Cond.Once</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:115](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L115) | - |

<a id="symbol-script-enum-imgui-dir-2582aa4248"></a>
### <code>ImGui_Dir</code>

<code>enum ImGui_Dir : int32</code>  
Symbol ID: <code>script.enum.ImGui_Dir</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:358](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L358)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Down</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-imgui-dir-down-4ef7d45147"></a><code>script.enum-value.ImGui_Dir.Down</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:358](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L358) | - |
| <code>Left</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-dir-left-26b03f1689"></a><code>script.enum-value.ImGui_Dir.Left</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:358](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L358) | - |
| <code>None</code> | <code>-1</code> | -1 | <a id="symbol-script-enum-value-imgui-dir-none-140c511c1e"></a><code>script.enum-value.ImGui_Dir.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:358](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L358) | - |
| <code>Right</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-dir-right-32ba59ee15"></a><code>script.enum-value.ImGui_Dir.Right</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:358](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L358) | - |
| <code>Up</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-dir-up-96ce78c9ca"></a><code>script.enum-value.ImGui_Dir.Up</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:358](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L358) | - |

<a id="symbol-script-enum-imgui-focusedflags-0753748cba"></a>
### <code>ImGui_FocusedFlags</code>

<code>enum ImGui_FocusedFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_FocusedFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AnyWindow</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-focusedflags-anywindow-3fe13ba8d8"></a><code>script.enum-value.ImGui_FocusedFlags.AnyWindow</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158) | - |
| <code>ChildWindows</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-focusedflags-childwindows-7b72e3d942"></a><code>script.enum-value.ImGui_FocusedFlags.ChildWindows</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158) | - |
| <code>NoPopupHierarchy</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-focusedflags-nopopuphierarchy-71aba2db61"></a><code>script.enum-value.ImGui_FocusedFlags.NoPopupHierarchy</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-focusedflags-none-380fd8bccd"></a><code>script.enum-value.ImGui_FocusedFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158) | - |
| <code>RootAndChildWindows</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-imgui-focusedflags-rootandchildwindows-8ca433a240"></a><code>script.enum-value.ImGui_FocusedFlags.RootAndChildWindows</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158) | - |
| <code>RootWindow</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-focusedflags-rootwindow-a5a343af72"></a><code>script.enum-value.ImGui_FocusedFlags.RootWindow</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:158](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L158) | - |

<a id="symbol-script-enum-imgui-hoveredflags-e0d3770939"></a>
### <code>ImGui_HoveredFlags</code>

<code>enum ImGui_HoveredFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_HoveredFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AllowWhenBlockedByActiveItem</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-hoveredflags-allowwhenblockedbyactiveitem-849ae7b900"></a><code>script.enum-value.ImGui_HoveredFlags.AllowWhenBlockedByActiveItem</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>AllowWhenBlockedByPopup</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-hoveredflags-allowwhenblockedbypopup-27722f483c"></a><code>script.enum-value.ImGui_HoveredFlags.AllowWhenBlockedByPopup</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>AllowWhenDisabled</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-hoveredflags-allowwhendisabled-48fdbb8f8f"></a><code>script.enum-value.ImGui_HoveredFlags.AllowWhenDisabled</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>AllowWhenOverlapped</code> | <code>768</code> | 768 | <a id="symbol-script-enum-value-imgui-hoveredflags-allowwhenoverlapped-9344977380"></a><code>script.enum-value.ImGui_HoveredFlags.AllowWhenOverlapped</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>AllowWhenOverlappedByItem</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-hoveredflags-allowwhenoverlappedbyitem-1c978630bf"></a><code>script.enum-value.ImGui_HoveredFlags.AllowWhenOverlappedByItem</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>AllowWhenOverlappedByWindow</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-hoveredflags-allowwhenoverlappedbywindow-e90fe28400"></a><code>script.enum-value.ImGui_HoveredFlags.AllowWhenOverlappedByWindow</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>AnyWindow</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-hoveredflags-anywindow-032e72489f"></a><code>script.enum-value.ImGui_HoveredFlags.AnyWindow</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>ChildWindows</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-hoveredflags-childwindows-85273777e1"></a><code>script.enum-value.ImGui_HoveredFlags.ChildWindows</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>ForTooltip</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-hoveredflags-fortooltip-0f32002745"></a><code>script.enum-value.ImGui_HoveredFlags.ForTooltip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>NoNavOverride</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-hoveredflags-nonavoverride-478aa836ad"></a><code>script.enum-value.ImGui_HoveredFlags.NoNavOverride</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>NoPopupHierarchy</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-hoveredflags-nopopuphierarchy-d5392308a8"></a><code>script.enum-value.ImGui_HoveredFlags.NoPopupHierarchy</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-hoveredflags-none-35b91016ca"></a><code>script.enum-value.ImGui_HoveredFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>RectOnly</code> | <code>928</code> | 928 | <a id="symbol-script-enum-value-imgui-hoveredflags-rectonly-3f78933364"></a><code>script.enum-value.ImGui_HoveredFlags.RectOnly</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |
| <code>RootWindow</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-hoveredflags-rootwindow-79b5019ee7"></a><code>script.enum-value.ImGui_HoveredFlags.RootWindow</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:169](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L169) | - |

<a id="symbol-script-enum-imgui-inputtextflags-1cccacf73f"></a>
### <code>ImGui_InputTextFlags</code>

<code>enum ImGui_InputTextFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_InputTextFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AutoSelectAll</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-inputtextflags-autoselectall-c321f282d6"></a><code>script.enum-value.ImGui_InputTextFlags.AutoSelectAll</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>CharsDecimal</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-inputtextflags-charsdecimal-431141b129"></a><code>script.enum-value.ImGui_InputTextFlags.CharsDecimal</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>CharsHexadecimal</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-inputtextflags-charshexadecimal-13bca30a14"></a><code>script.enum-value.ImGui_InputTextFlags.CharsHexadecimal</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>CharsNoBlank</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-inputtextflags-charsnoblank-0d24b0af0f"></a><code>script.enum-value.ImGui_InputTextFlags.CharsNoBlank</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>CharsScientific</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-inputtextflags-charsscientific-2f12018da7"></a><code>script.enum-value.ImGui_InputTextFlags.CharsScientific</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>CharsUppercase</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-inputtextflags-charsuppercase-4c16748bc0"></a><code>script.enum-value.ImGui_InputTextFlags.CharsUppercase</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>DisplayEmptyRefVal</code> | <code>16384</code> | 16384 | <a id="symbol-script-enum-value-imgui-inputtextflags-displayemptyrefval-05ad536b37"></a><code>script.enum-value.ImGui_InputTextFlags.DisplayEmptyRefVal</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>EnterReturnsTrue</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-inputtextflags-enterreturnstrue-ba1974565a"></a><code>script.enum-value.ImGui_InputTextFlags.EnterReturnsTrue</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-inputtextflags-none-06ead58189"></a><code>script.enum-value.ImGui_InputTextFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>ParseEmptyRefVal</code> | <code>8192</code> | 8192 | <a id="symbol-script-enum-value-imgui-inputtextflags-parseemptyrefval-f1215acf71"></a><code>script.enum-value.ImGui_InputTextFlags.ParseEmptyRefVal</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |
| <code>ReadOnly</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-inputtextflags-readonly-571617b475"></a><code>script.enum-value.ImGui_InputTextFlags.ReadOnly</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:319](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L319) | - |

<a id="symbol-script-enum-imgui-mousebutton-e208e24134"></a>
### <code>ImGui_MouseButton</code>

<code>enum ImGui_MouseButton : int32</code>  
Symbol ID: <code>script.enum.ImGui_MouseButton</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:350](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L350)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Left</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-mousebutton-left-94ac44d280"></a><code>script.enum-value.ImGui_MouseButton.Left</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:350](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L350) | - |
| <code>Middle</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-mousebutton-middle-874680cd1a"></a><code>script.enum-value.ImGui_MouseButton.Middle</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:350](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L350) | - |
| <code>Right</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-mousebutton-right-853ad189e5"></a><code>script.enum-value.ImGui_MouseButton.Right</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:350](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L350) | - |

<a id="symbol-script-enum-imgui-popupflags-e906c25c5a"></a>
### <code>ImGui_PopupFlags</code>

<code>enum ImGui_PopupFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_PopupFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AnyPopup</code> | <code>3072</code> | 3072 | <a id="symbol-script-enum-value-imgui-popupflags-anypopup-b47fcfe2b3"></a><code>script.enum-value.ImGui_PopupFlags.AnyPopup</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>AnyPopupId</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-popupflags-anypopupid-a7dd1f3f94"></a><code>script.enum-value.ImGui_PopupFlags.AnyPopupId</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>AnyPopupLevel</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-popupflags-anypopuplevel-1b5333e470"></a><code>script.enum-value.ImGui_PopupFlags.AnyPopupLevel</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>MouseButtonLeft</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-popupflags-mousebuttonleft-936cf75f6a"></a><code>script.enum-value.ImGui_PopupFlags.MouseButtonLeft</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>MouseButtonMiddle</code> | <code>12</code> | 12 | <a id="symbol-script-enum-value-imgui-popupflags-mousebuttonmiddle-97a810ac97"></a><code>script.enum-value.ImGui_PopupFlags.MouseButtonMiddle</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>MouseButtonRight</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-popupflags-mousebuttonright-afaafc7bf2"></a><code>script.enum-value.ImGui_PopupFlags.MouseButtonRight</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>NoOpenOverExistingPopup</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-popupflags-noopenoverexistingpopup-fd43af0c3f"></a><code>script.enum-value.ImGui_PopupFlags.NoOpenOverExistingPopup</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>NoOpenOverItems</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-popupflags-noopenoveritems-b66a4c05a7"></a><code>script.enum-value.ImGui_PopupFlags.NoOpenOverItems</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>NoReopen</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-popupflags-noreopen-f4f7a1e339"></a><code>script.enum-value.ImGui_PopupFlags.NoReopen</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-popupflags-none-520844598f"></a><code>script.enum-value.ImGui_PopupFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:335](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L335) | - |

<a id="symbol-script-enum-imgui-selectableflags-1ecd4f14ce"></a>
### <code>ImGui_SelectableFlags</code>

<code>enum ImGui_SelectableFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_SelectableFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AllowDoubleClick</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-selectableflags-allowdoubleclick-ecabf3b3c8"></a><code>script.enum-value.ImGui_SelectableFlags.AllowDoubleClick</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125) | - |
| <code>AllowOverlap</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-selectableflags-allowoverlap-a3bf017319"></a><code>script.enum-value.ImGui_SelectableFlags.AllowOverlap</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125) | - |
| <code>Disabled</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-selectableflags-disabled-298c5123be"></a><code>script.enum-value.ImGui_SelectableFlags.Disabled</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125) | - |
| <code>NoAutoClosePopups</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-selectableflags-noautoclosepopups-97b906dbb5"></a><code>script.enum-value.ImGui_SelectableFlags.NoAutoClosePopups</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-selectableflags-none-2815b1d948"></a><code>script.enum-value.ImGui_SelectableFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125) | - |
| <code>SpanAllColumns</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-selectableflags-spanallcolumns-1bcd42b9d5"></a><code>script.enum-value.ImGui_SelectableFlags.SpanAllColumns</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:125](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L125) | - |

<a id="symbol-script-enum-imgui-sliderflags-9660cee445"></a>
### <code>ImGui_SliderFlags</code>

<code>enum ImGui_SliderFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_SliderFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AlwaysClamp</code> | <code>1536</code> | 1536 | <a id="symbol-script-enum-value-imgui-sliderflags-alwaysclamp-6a4f081ec1"></a><code>script.enum-value.ImGui_SliderFlags.AlwaysClamp</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>ClampOnInput</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-sliderflags-clamponinput-50e13e8b7d"></a><code>script.enum-value.ImGui_SliderFlags.ClampOnInput</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>ClampZeroRange</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-sliderflags-clampzerorange-ff2fb4dcef"></a><code>script.enum-value.ImGui_SliderFlags.ClampZeroRange</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>Logarithmic</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-sliderflags-logarithmic-08fec326cc"></a><code>script.enum-value.ImGui_SliderFlags.Logarithmic</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>NoInput</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-sliderflags-noinput-febdda203c"></a><code>script.enum-value.ImGui_SliderFlags.NoInput</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>NoRoundToFormat</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-sliderflags-noroundtoformat-277ab8fd0c"></a><code>script.enum-value.ImGui_SliderFlags.NoRoundToFormat</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>NoSpeedTweaks</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-sliderflags-nospeedtweaks-81da7896d4"></a><code>script.enum-value.ImGui_SliderFlags.NoSpeedTweaks</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-sliderflags-none-f6b0ca273a"></a><code>script.enum-value.ImGui_SliderFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |
| <code>WrapAround</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-sliderflags-wraparound-ded5d04a1d"></a><code>script.enum-value.ImGui_SliderFlags.WrapAround</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:368](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L368) | - |

<a id="symbol-script-enum-imgui-stylevar-40b127918a"></a>
### <code>ImGui_StyleVar</code>

<code>enum ImGui_StyleVar : int32</code>  
Symbol ID: <code>script.enum.ImGui_StyleVar</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Alpha</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-stylevar-alpha-18c9fd685e"></a><code>script.enum-value.ImGui_StyleVar.Alpha</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>ButtonTextAlign</code> | <code>36</code> | 36 | <a id="symbol-script-enum-value-imgui-stylevar-buttontextalign-7da9647c89"></a><code>script.enum-value.ImGui_StyleVar.ButtonTextAlign</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>CellPadding</code> | <code>17</code> | 17 | <a id="symbol-script-enum-value-imgui-stylevar-cellpadding-bdb51c337f"></a><code>script.enum-value.ImGui_StyleVar.CellPadding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>DisabledAlpha</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-stylevar-disabledalpha-e8b4d3acca"></a><code>script.enum-value.ImGui_StyleVar.DisabledAlpha</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>FrameBorderSize</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-imgui-stylevar-framebordersize-679e6ccdfa"></a><code>script.enum-value.ImGui_StyleVar.FrameBorderSize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>FramePadding</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-imgui-stylevar-framepadding-fa5ed3fd13"></a><code>script.enum-value.ImGui_StyleVar.FramePadding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>FrameRounding</code> | <code>12</code> | 12 | <a id="symbol-script-enum-value-imgui-stylevar-framerounding-9bddffcfdd"></a><code>script.enum-value.ImGui_StyleVar.FrameRounding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>GrabMinSize</code> | <code>21</code> | 21 | <a id="symbol-script-enum-value-imgui-stylevar-grabminsize-595d16ccd1"></a><code>script.enum-value.ImGui_StyleVar.GrabMinSize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>GrabRounding</code> | <code>22</code> | 22 | <a id="symbol-script-enum-value-imgui-stylevar-grabrounding-90390fa569"></a><code>script.enum-value.ImGui_StyleVar.GrabRounding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>IndentSpacing</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-stylevar-indentspacing-9f93daf9fa"></a><code>script.enum-value.ImGui_StyleVar.IndentSpacing</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>ItemInnerSpacing</code> | <code>15</code> | 15 | <a id="symbol-script-enum-value-imgui-stylevar-iteminnerspacing-48d26b14d1"></a><code>script.enum-value.ImGui_StyleVar.ItemInnerSpacing</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>ItemSpacing</code> | <code>14</code> | 14 | <a id="symbol-script-enum-value-imgui-stylevar-itemspacing-17db48766a"></a><code>script.enum-value.ImGui_StyleVar.ItemSpacing</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>ScrollbarRounding</code> | <code>19</code> | 19 | <a id="symbol-script-enum-value-imgui-stylevar-scrollbarrounding-645c1fe6a9"></a><code>script.enum-value.ImGui_StyleVar.ScrollbarRounding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>ScrollbarSize</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-imgui-stylevar-scrollbarsize-1993320e07"></a><code>script.enum-value.ImGui_StyleVar.ScrollbarSize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>SelectableTextAlign</code> | <code>37</code> | 37 | <a id="symbol-script-enum-value-imgui-stylevar-selectabletextalign-213a3bc099"></a><code>script.enum-value.ImGui_StyleVar.SelectableTextAlign</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>TabRounding</code> | <code>25</code> | 25 | <a id="symbol-script-enum-value-imgui-stylevar-tabrounding-c27aace69b"></a><code>script.enum-value.ImGui_StyleVar.TabRounding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>WindowBorderSize</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-stylevar-windowbordersize-e092954718"></a><code>script.enum-value.ImGui_StyleVar.WindowBorderSize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>WindowPadding</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-stylevar-windowpadding-72df6b32cc"></a><code>script.enum-value.ImGui_StyleVar.WindowPadding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |
| <code>WindowRounding</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-imgui-stylevar-windowrounding-073a98c667"></a><code>script.enum-value.ImGui_StyleVar.WindowRounding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:465](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L465) | - |

<a id="symbol-script-enum-imgui-tabbarflags-9cb3a82fbf"></a>
### <code>ImGui_TabBarFlags</code>

<code>enum ImGui_TabBarFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TabBarFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AutoSelectNewTabs</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-tabbarflags-autoselectnewtabs-d24968abb9"></a><code>script.enum-value.ImGui_TabBarFlags.AutoSelectNewTabs</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>DrawSelectedOverline</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-tabbarflags-drawselectedoverline-9979181f93"></a><code>script.enum-value.ImGui_TabBarFlags.DrawSelectedOverline</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>FittingPolicyMixed</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-tabbarflags-fittingpolicymixed-6ed09f0dcc"></a><code>script.enum-value.ImGui_TabBarFlags.FittingPolicyMixed</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>FittingPolicyScroll</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-tabbarflags-fittingpolicyscroll-898362cc1f"></a><code>script.enum-value.ImGui_TabBarFlags.FittingPolicyScroll</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>FittingPolicyShrink</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-tabbarflags-fittingpolicyshrink-6b94b35d56"></a><code>script.enum-value.ImGui_TabBarFlags.FittingPolicyShrink</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>NoCloseWithMiddleMouseButton</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-tabbarflags-noclosewithmiddlemousebutton-46482fc418"></a><code>script.enum-value.ImGui_TabBarFlags.NoCloseWithMiddleMouseButton</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>NoTabListScrollingButtons</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-tabbarflags-notablistscrollingbuttons-12169d9cdb"></a><code>script.enum-value.ImGui_TabBarFlags.NoTabListScrollingButtons</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>NoTooltip</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-tabbarflags-notooltip-70fba05377"></a><code>script.enum-value.ImGui_TabBarFlags.NoTooltip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-tabbarflags-none-1ca1ffdcec"></a><code>script.enum-value.ImGui_TabBarFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>Reorderable</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-tabbarflags-reorderable-ea6fdc1a1c"></a><code>script.enum-value.ImGui_TabBarFlags.Reorderable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |
| <code>TabListPopupButton</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-tabbarflags-tablistpopupbutton-d6ada75b4b"></a><code>script.enum-value.ImGui_TabBarFlags.TabListPopupButton</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:274](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L274) | - |

<a id="symbol-script-enum-imgui-tabitemflags-fa4577c84b"></a>
### <code>ImGui_TabItemFlags</code>

<code>enum ImGui_TabItemFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TabItemFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Leading</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-tabitemflags-leading-524fdd4daa"></a><code>script.enum-value.ImGui_TabItemFlags.Leading</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>NoAssumedClosure</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-tabitemflags-noassumedclosure-1716ce0e7f"></a><code>script.enum-value.ImGui_TabItemFlags.NoAssumedClosure</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>NoCloseWithMiddleMouseButton</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-tabitemflags-noclosewithmiddlemousebutton-a02e71f77e"></a><code>script.enum-value.ImGui_TabItemFlags.NoCloseWithMiddleMouseButton</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>NoPushId</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-tabitemflags-nopushid-7e57945c08"></a><code>script.enum-value.ImGui_TabItemFlags.NoPushId</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>NoReorder</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-tabitemflags-noreorder-d272727dfd"></a><code>script.enum-value.ImGui_TabItemFlags.NoReorder</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>NoTooltip</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-tabitemflags-notooltip-51bd462e01"></a><code>script.enum-value.ImGui_TabItemFlags.NoTooltip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-tabitemflags-none-f2404fd796"></a><code>script.enum-value.ImGui_TabItemFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>SetSelected</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-tabitemflags-setselected-075cfa0885"></a><code>script.enum-value.ImGui_TabItemFlags.SetSelected</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>Trailing</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-tabitemflags-trailing-0956852ced"></a><code>script.enum-value.ImGui_TabItemFlags.Trailing</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |
| <code>UnsavedDocument</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-tabitemflags-unsaveddocument-8b2366f5c5"></a><code>script.enum-value.ImGui_TabItemFlags.UnsavedDocument</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:290](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L290) | - |

<a id="symbol-script-enum-imgui-tablebgtarget-2010434d87"></a>
### <code>ImGui_TableBgTarget</code>

<code>enum ImGui_TableBgTarget : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TableBgTarget</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:265](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L265)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CellBg</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-imgui-tablebgtarget-cellbg-2a7569a6fa"></a><code>script.enum-value.ImGui_TableBgTarget.CellBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:265](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L265) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-tablebgtarget-none-3fad62dd2a"></a><code>script.enum-value.ImGui_TableBgTarget.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:265](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L265) | - |
| <code>RowBg0</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-tablebgtarget-rowbg0-526623f078"></a><code>script.enum-value.ImGui_TableBgTarget.RowBg0</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:265](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L265) | - |
| <code>RowBg1</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-tablebgtarget-rowbg1-737fdd1bdd"></a><code>script.enum-value.ImGui_TableBgTarget.RowBg1</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:265](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L265) | - |

<a id="symbol-script-enum-imgui-tablecolumnflags-cb4b5db509"></a>
### <code>ImGui_TableColumnFlags</code>

<code>enum ImGui_TableColumnFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TableColumnFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AngledHeader</code> | <code>262144</code> | 262144 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-angledheader-d99db27e15"></a><code>script.enum-value.ImGui_TableColumnFlags.AngledHeader</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>DefaultHide</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-defaulthide-02ecd80a9f"></a><code>script.enum-value.ImGui_TableColumnFlags.DefaultHide</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>DefaultSort</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-defaultsort-54db19fb82"></a><code>script.enum-value.ImGui_TableColumnFlags.DefaultSort</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>Disabled</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-disabled-e3862c88ec"></a><code>script.enum-value.ImGui_TableColumnFlags.Disabled</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>IndentDisable</code> | <code>131072</code> | 131072 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-indentdisable-578826ca84"></a><code>script.enum-value.ImGui_TableColumnFlags.IndentDisable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>IndentEnable</code> | <code>65536</code> | 65536 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-indentenable-8bd0750bc2"></a><code>script.enum-value.ImGui_TableColumnFlags.IndentEnable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>IsEnabled</code> | <code>16777216</code> | 16777216 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-isenabled-e31575b224"></a><code>script.enum-value.ImGui_TableColumnFlags.IsEnabled</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>IsHovered</code> | <code>134217728</code> | 134217728 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-ishovered-a4551a01d6"></a><code>script.enum-value.ImGui_TableColumnFlags.IsHovered</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>IsSorted</code> | <code>67108864</code> | 67108864 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-issorted-4c68f18f59"></a><code>script.enum-value.ImGui_TableColumnFlags.IsSorted</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>IsVisible</code> | <code>33554432</code> | 33554432 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-isvisible-395cf13969"></a><code>script.enum-value.ImGui_TableColumnFlags.IsVisible</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoClip</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-noclip-d932bb680c"></a><code>script.enum-value.ImGui_TableColumnFlags.NoClip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoHeaderLabel</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-noheaderlabel-ed98bcefff"></a><code>script.enum-value.ImGui_TableColumnFlags.NoHeaderLabel</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoHeaderWidth</code> | <code>8192</code> | 8192 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-noheaderwidth-4b651d3476"></a><code>script.enum-value.ImGui_TableColumnFlags.NoHeaderWidth</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoHide</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-nohide-540140ca2f"></a><code>script.enum-value.ImGui_TableColumnFlags.NoHide</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoReorder</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-noreorder-b7bd8ed7bd"></a><code>script.enum-value.ImGui_TableColumnFlags.NoReorder</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoResize</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-noresize-89b5b9bed4"></a><code>script.enum-value.ImGui_TableColumnFlags.NoResize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoSort</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-nosort-e568dc2f27"></a><code>script.enum-value.ImGui_TableColumnFlags.NoSort</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoSortAscending</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-nosortascending-0d4e19c1fd"></a><code>script.enum-value.ImGui_TableColumnFlags.NoSortAscending</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>NoSortDescending</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-nosortdescending-8e4359f9cd"></a><code>script.enum-value.ImGui_TableColumnFlags.NoSortDescending</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-none-15877a24a2"></a><code>script.enum-value.ImGui_TableColumnFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>PreferSortAscending</code> | <code>16384</code> | 16384 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-prefersortascending-c2a0ed7f7a"></a><code>script.enum-value.ImGui_TableColumnFlags.PreferSortAscending</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>PreferSortDescending</code> | <code>32768</code> | 32768 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-prefersortdescending-47ef0a80ac"></a><code>script.enum-value.ImGui_TableColumnFlags.PreferSortDescending</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>WidthFixed</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-widthfixed-a8ce2e0d9a"></a><code>script.enum-value.ImGui_TableColumnFlags.WidthFixed</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |
| <code>WidthStretch</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-tablecolumnflags-widthstretch-805f20e192"></a><code>script.enum-value.ImGui_TableColumnFlags.WidthStretch</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:229](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L229) | - |

<a id="symbol-script-enum-imgui-tableflags-eb05e79e59"></a>
### <code>ImGui_TableFlags</code>

<code>enum ImGui_TableFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TableFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Borders</code> | <code>1920</code> | 1920 | <a id="symbol-script-enum-value-imgui-tableflags-borders-338ccb8654"></a><code>script.enum-value.ImGui_TableFlags.Borders</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersH</code> | <code>384</code> | 384 | <a id="symbol-script-enum-value-imgui-tableflags-bordersh-844da56103"></a><code>script.enum-value.ImGui_TableFlags.BordersH</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersInner</code> | <code>640</code> | 640 | <a id="symbol-script-enum-value-imgui-tableflags-bordersinner-26f71edf6b"></a><code>script.enum-value.ImGui_TableFlags.BordersInner</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersInnerH</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-tableflags-bordersinnerh-cbccdad2fc"></a><code>script.enum-value.ImGui_TableFlags.BordersInnerH</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersInnerV</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-tableflags-bordersinnerv-308bb9ba68"></a><code>script.enum-value.ImGui_TableFlags.BordersInnerV</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersOuter</code> | <code>1280</code> | 1280 | <a id="symbol-script-enum-value-imgui-tableflags-bordersouter-c6d5ff7e20"></a><code>script.enum-value.ImGui_TableFlags.BordersOuter</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersOuterH</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-tableflags-bordersouterh-6ca5ce3901"></a><code>script.enum-value.ImGui_TableFlags.BordersOuterH</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersOuterV</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-tableflags-bordersouterv-6fac9d4bdc"></a><code>script.enum-value.ImGui_TableFlags.BordersOuterV</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>BordersV</code> | <code>1536</code> | 1536 | <a id="symbol-script-enum-value-imgui-tableflags-bordersv-af84f9d3ad"></a><code>script.enum-value.ImGui_TableFlags.BordersV</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>ContextMenuInBody</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-tableflags-contextmenuinbody-64b6c93b9f"></a><code>script.enum-value.ImGui_TableFlags.ContextMenuInBody</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>Hideable</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-tableflags-hideable-bf9b6ff310"></a><code>script.enum-value.ImGui_TableFlags.Hideable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>HighlightHoveredColumn</code> | <code>268435456</code> | 268435456 | <a id="symbol-script-enum-value-imgui-tableflags-highlighthoveredcolumn-51ae02393c"></a><code>script.enum-value.ImGui_TableFlags.HighlightHoveredColumn</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoBordersInBody</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-tableflags-nobordersinbody-a9de8d56cc"></a><code>script.enum-value.ImGui_TableFlags.NoBordersInBody</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoBordersInBodyUntilResize</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-tableflags-nobordersinbodyuntilresize-7aa3af1669"></a><code>script.enum-value.ImGui_TableFlags.NoBordersInBodyUntilResize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoClip</code> | <code>1048576</code> | 1048576 | <a id="symbol-script-enum-value-imgui-tableflags-noclip-e536740c73"></a><code>script.enum-value.ImGui_TableFlags.NoClip</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoHostExtendX</code> | <code>65536</code> | 65536 | <a id="symbol-script-enum-value-imgui-tableflags-nohostextendx-587cdb459e"></a><code>script.enum-value.ImGui_TableFlags.NoHostExtendX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoHostExtendY</code> | <code>131072</code> | 131072 | <a id="symbol-script-enum-value-imgui-tableflags-nohostextendy-6ab372d6d1"></a><code>script.enum-value.ImGui_TableFlags.NoHostExtendY</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoKeepColumnsVisible</code> | <code>262144</code> | 262144 | <a id="symbol-script-enum-value-imgui-tableflags-nokeepcolumnsvisible-adaeccd4e8"></a><code>script.enum-value.ImGui_TableFlags.NoKeepColumnsVisible</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoPadInnerX</code> | <code>8388608</code> | 8388608 | <a id="symbol-script-enum-value-imgui-tableflags-nopadinnerx-9ed949a878"></a><code>script.enum-value.ImGui_TableFlags.NoPadInnerX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoPadOuterX</code> | <code>4194304</code> | 4194304 | <a id="symbol-script-enum-value-imgui-tableflags-nopadouterx-663791d178"></a><code>script.enum-value.ImGui_TableFlags.NoPadOuterX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>NoSavedSettings</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-tableflags-nosavedsettings-3ba0913960"></a><code>script.enum-value.ImGui_TableFlags.NoSavedSettings</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-tableflags-none-d9564adf25"></a><code>script.enum-value.ImGui_TableFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>PadOuterX</code> | <code>2097152</code> | 2097152 | <a id="symbol-script-enum-value-imgui-tableflags-padouterx-8eb6f66c87"></a><code>script.enum-value.ImGui_TableFlags.PadOuterX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>PreciseWidths</code> | <code>524288</code> | 524288 | <a id="symbol-script-enum-value-imgui-tableflags-precisewidths-05aaee4bd7"></a><code>script.enum-value.ImGui_TableFlags.PreciseWidths</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>Reorderable</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-tableflags-reorderable-91350c4439"></a><code>script.enum-value.ImGui_TableFlags.Reorderable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>Resizable</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-tableflags-resizable-2d0f6e8f05"></a><code>script.enum-value.ImGui_TableFlags.Resizable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>RowBg</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-tableflags-rowbg-27eacd9276"></a><code>script.enum-value.ImGui_TableFlags.RowBg</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>ScrollX</code> | <code>16777216</code> | 16777216 | <a id="symbol-script-enum-value-imgui-tableflags-scrollx-041b7d5d1c"></a><code>script.enum-value.ImGui_TableFlags.ScrollX</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>ScrollY</code> | <code>33554432</code> | 33554432 | <a id="symbol-script-enum-value-imgui-tableflags-scrolly-ff58049448"></a><code>script.enum-value.ImGui_TableFlags.ScrollY</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>SizingFixedFit</code> | <code>8192</code> | 8192 | <a id="symbol-script-enum-value-imgui-tableflags-sizingfixedfit-ffaa195f4d"></a><code>script.enum-value.ImGui_TableFlags.SizingFixedFit</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>SizingFixedSame</code> | <code>16384</code> | 16384 | <a id="symbol-script-enum-value-imgui-tableflags-sizingfixedsame-69c1222e0d"></a><code>script.enum-value.ImGui_TableFlags.SizingFixedSame</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>SizingStretchProp</code> | <code>24576</code> | 24576 | <a id="symbol-script-enum-value-imgui-tableflags-sizingstretchprop-188f1aef8b"></a><code>script.enum-value.ImGui_TableFlags.SizingStretchProp</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>SizingStretchSame</code> | <code>32768</code> | 32768 | <a id="symbol-script-enum-value-imgui-tableflags-sizingstretchsame-8578d6197c"></a><code>script.enum-value.ImGui_TableFlags.SizingStretchSame</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>SortMulti</code> | <code>67108864</code> | 67108864 | <a id="symbol-script-enum-value-imgui-tableflags-sortmulti-266f9f8af2"></a><code>script.enum-value.ImGui_TableFlags.SortMulti</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>SortTristate</code> | <code>134217728</code> | 134217728 | <a id="symbol-script-enum-value-imgui-tableflags-sorttristate-3541e2dd1d"></a><code>script.enum-value.ImGui_TableFlags.SortTristate</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |
| <code>Sortable</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-tableflags-sortable-bb4a351017"></a><code>script.enum-value.ImGui_TableFlags.Sortable</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:188](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L188) | - |

<a id="symbol-script-enum-imgui-tablerowflags-2ed82034a7"></a>
### <code>ImGui_TableRowFlags</code>

<code>enum ImGui_TableRowFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TableRowFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:258](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L258)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Headers</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-tablerowflags-headers-098a0be84b"></a><code>script.enum-value.ImGui_TableRowFlags.Headers</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:258](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L258) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-tablerowflags-none-f9eaf918bc"></a><code>script.enum-value.ImGui_TableRowFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:258](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L258) | - |

<a id="symbol-script-enum-imgui-treenodeflags-be8eefa38b"></a>
### <code>ImGui_TreeNodeFlags</code>

<code>enum ImGui_TreeNodeFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_TreeNodeFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AllowOverlap</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-treenodeflags-allowoverlap-5d8f3b1420"></a><code>script.enum-value.ImGui_TreeNodeFlags.AllowOverlap</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>Bullet</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-treenodeflags-bullet-7b2b9c4614"></a><code>script.enum-value.ImGui_TreeNodeFlags.Bullet</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>CollapsingHeader</code> | <code>26</code> | 26 | <a id="symbol-script-enum-value-imgui-treenodeflags-collapsingheader-87aaaf8816"></a><code>script.enum-value.ImGui_TreeNodeFlags.CollapsingHeader</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>DefaultOpen</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-treenodeflags-defaultopen-a8ee86343e"></a><code>script.enum-value.ImGui_TreeNodeFlags.DefaultOpen</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>FramePadding</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-treenodeflags-framepadding-24e18ff816"></a><code>script.enum-value.ImGui_TreeNodeFlags.FramePadding</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>Framed</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-treenodeflags-framed-3e0e8ea92d"></a><code>script.enum-value.ImGui_TreeNodeFlags.Framed</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>Leaf</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-treenodeflags-leaf-219f42cc25"></a><code>script.enum-value.ImGui_TreeNodeFlags.Leaf</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>NoAutoOpenOnLog</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-treenodeflags-noautoopenonlog-ccf5fc118d"></a><code>script.enum-value.ImGui_TreeNodeFlags.NoAutoOpenOnLog</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>NoTreePushOnOpen</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-treenodeflags-notreepushonopen-dede1dfa34"></a><code>script.enum-value.ImGui_TreeNodeFlags.NoTreePushOnOpen</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-treenodeflags-none-0067b84f3c"></a><code>script.enum-value.ImGui_TreeNodeFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>OpenOnArrow</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-treenodeflags-openonarrow-d44996e48b"></a><code>script.enum-value.ImGui_TreeNodeFlags.OpenOnArrow</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>OpenOnDoubleClick</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-treenodeflags-openondoubleclick-86dab34a3b"></a><code>script.enum-value.ImGui_TreeNodeFlags.OpenOnDoubleClick</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>Selected</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-treenodeflags-selected-ce665915c1"></a><code>script.enum-value.ImGui_TreeNodeFlags.Selected</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>SpanAllColumns</code> | <code>16384</code> | 16384 | <a id="symbol-script-enum-value-imgui-treenodeflags-spanallcolumns-8870f74b43"></a><code>script.enum-value.ImGui_TreeNodeFlags.SpanAllColumns</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>SpanAvailWidth</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-treenodeflags-spanavailwidth-df6d7adeff"></a><code>script.enum-value.ImGui_TreeNodeFlags.SpanAvailWidth</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>SpanFullWidth</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-treenodeflags-spanfullwidth-c6fb7c1ce1"></a><code>script.enum-value.ImGui_TreeNodeFlags.SpanFullWidth</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |
| <code>SpanLabelWidth</code> | <code>8192</code> | 8192 | <a id="symbol-script-enum-value-imgui-treenodeflags-spanlabelwidth-35e603a77c"></a><code>script.enum-value.ImGui_TreeNodeFlags.SpanLabelWidth</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:136](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L136) | - |

<a id="symbol-script-enum-imgui-windowflags-2ba78bf735"></a>
### <code>ImGui_WindowFlags</code>

<code>enum ImGui_WindowFlags : uint32</code>  
Symbol ID: <code>script.enum.ImGui_WindowFlags</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AlwaysAutoResize</code> | <code>64</code> | 64 | <a id="symbol-script-enum-value-imgui-windowflags-alwaysautoresize-bfd4435fc5"></a><code>script.enum-value.ImGui_WindowFlags.AlwaysAutoResize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>AlwaysHorizontalScrollbar</code> | <code>32768</code> | 32768 | <a id="symbol-script-enum-value-imgui-windowflags-alwayshorizontalscrollbar-12966fd034"></a><code>script.enum-value.ImGui_WindowFlags.AlwaysHorizontalScrollbar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>AlwaysVerticalScrollbar</code> | <code>16384</code> | 16384 | <a id="symbol-script-enum-value-imgui-windowflags-alwaysverticalscrollbar-9f07584c9b"></a><code>script.enum-value.ImGui_WindowFlags.AlwaysVerticalScrollbar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>HorizontalScrollbar</code> | <code>2048</code> | 2048 | <a id="symbol-script-enum-value-imgui-windowflags-horizontalscrollbar-be3efbc38a"></a><code>script.enum-value.ImGui_WindowFlags.HorizontalScrollbar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>MenuBar</code> | <code>1024</code> | 1024 | <a id="symbol-script-enum-value-imgui-windowflags-menubar-dd3e7ee294"></a><code>script.enum-value.ImGui_WindowFlags.MenuBar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoBackground</code> | <code>128</code> | 128 | <a id="symbol-script-enum-value-imgui-windowflags-nobackground-7840355f7c"></a><code>script.enum-value.ImGui_WindowFlags.NoBackground</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoBringToFrontOnFocus</code> | <code>8192</code> | 8192 | <a id="symbol-script-enum-value-imgui-windowflags-nobringtofrontonfocus-cb169f434f"></a><code>script.enum-value.ImGui_WindowFlags.NoBringToFrontOnFocus</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoCollapse</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-imgui-windowflags-nocollapse-fb79688a37"></a><code>script.enum-value.ImGui_WindowFlags.NoCollapse</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoDecoration</code> | <code>43</code> | 43 | <a id="symbol-script-enum-value-imgui-windowflags-nodecoration-224d4cc215"></a><code>script.enum-value.ImGui_WindowFlags.NoDecoration</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoFocusOnAppearing</code> | <code>4096</code> | 4096 | <a id="symbol-script-enum-value-imgui-windowflags-nofocusonappearing-19ef425f26"></a><code>script.enum-value.ImGui_WindowFlags.NoFocusOnAppearing</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoInputs</code> | <code>197120</code> | 197120 | <a id="symbol-script-enum-value-imgui-windowflags-noinputs-e60f84efd3"></a><code>script.enum-value.ImGui_WindowFlags.NoInputs</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoMouseInputs</code> | <code>512</code> | 512 | <a id="symbol-script-enum-value-imgui-windowflags-nomouseinputs-0e14b57477"></a><code>script.enum-value.ImGui_WindowFlags.NoMouseInputs</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoMove</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-imgui-windowflags-nomove-6d2725e614"></a><code>script.enum-value.ImGui_WindowFlags.NoMove</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoNav</code> | <code>196608</code> | 196608 | <a id="symbol-script-enum-value-imgui-windowflags-nonav-c0c324e37d"></a><code>script.enum-value.ImGui_WindowFlags.NoNav</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoNavFocus</code> | <code>131072</code> | 131072 | <a id="symbol-script-enum-value-imgui-windowflags-nonavfocus-0f61efff50"></a><code>script.enum-value.ImGui_WindowFlags.NoNavFocus</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoNavInputs</code> | <code>65536</code> | 65536 | <a id="symbol-script-enum-value-imgui-windowflags-nonavinputs-e270846519"></a><code>script.enum-value.ImGui_WindowFlags.NoNavInputs</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoResize</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-imgui-windowflags-noresize-6f5742034f"></a><code>script.enum-value.ImGui_WindowFlags.NoResize</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoSavedSettings</code> | <code>256</code> | 256 | <a id="symbol-script-enum-value-imgui-windowflags-nosavedsettings-b111861271"></a><code>script.enum-value.ImGui_WindowFlags.NoSavedSettings</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoScrollWithMouse</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-imgui-windowflags-noscrollwithmouse-4ec9e72540"></a><code>script.enum-value.ImGui_WindowFlags.NoScrollWithMouse</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoScrollbar</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-imgui-windowflags-noscrollbar-cbf7174029"></a><code>script.enum-value.ImGui_WindowFlags.NoScrollbar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>NoTitleBar</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-imgui-windowflags-notitlebar-f2db0031f7"></a><code>script.enum-value.ImGui_WindowFlags.NoTitleBar</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-imgui-windowflags-none-ecb312c8ab"></a><code>script.enum-value.ImGui_WindowFlags.None</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |
| <code>UnsavedDocument</code> | <code>262144</code> | 262144 | <a id="symbol-script-enum-value-imgui-windowflags-unsaveddocument-d23a1302de"></a><code>script.enum-value.ImGui_WindowFlags.UnsavedDocument</code> | <code>internal</code> (default) | [Source/Common/ImGuiExt/ImGuiStuff.h:72](https://github.com/cvet/fonline/blob/master/Source/Common/ImGuiExt/ImGuiStuff.h#L72) | - |

<a id="symbol-script-enum-itemownership-aa2d799c7d"></a>
### <code>ItemOwnership</code>

<code>enum ItemOwnership : uint8</code>  
Symbol ID: <code>script.enum.ItemOwnership</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:989](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L989)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CritterInventory</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-itemownership-critterinventory-dc45fb90e3"></a><code>script.enum-value.ItemOwnership.CritterInventory</code> | <code>internal</code> (default) | [Source/Common/Common.h:989](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L989) | - |
| <code>ItemContainer</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-itemownership-itemcontainer-86d46b33fb"></a><code>script.enum-value.ItemOwnership.ItemContainer</code> | <code>internal</code> (default) | [Source/Common/Common.h:989](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L989) | - |
| <code>MapHex</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-itemownership-maphex-1515f04bc3"></a><code>script.enum-value.ItemOwnership.MapHex</code> | <code>internal</code> (default) | [Source/Common/Common.h:989](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L989) | - |
| <code>Nowhere</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-itemownership-nowhere-209b7aa41e"></a><code>script.enum-value.ItemOwnership.Nowhere</code> | <code>internal</code> (default) | [Source/Common/Common.h:989](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L989) | - |

<a id="symbol-script-enum-itemproperty-387cd84346"></a>
### <code>ItemProperty</code>

<code>enum ItemProperty : uint16</code>  
Symbol ID: <code>script.enum.ItemProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AlwaysHideSprite</code> | <code>28</code> | 28 | <a id="symbol-script-enum-value-itemproperty-alwayshidesprite-6320d8f1b1"></a><code>script.enum-value.ItemProperty.AlwaysHideSprite</code> | <code>internal</code> (default) | - | - |
| <code>Colorize</code> | <code>52</code> | 52 | <a id="symbol-script-enum-value-itemproperty-colorize-1e727ce2b7"></a><code>script.enum-value.ItemProperty.Colorize</code> | <code>internal</code> (default) | - | - |
| <code>ColorizeColor</code> | <code>37</code> | 37 | <a id="symbol-script-enum-value-itemproperty-colorizecolor-fbb347ac26"></a><code>script.enum-value.ItemProperty.ColorizeColor</code> | <code>internal</code> (default) | - | - |
| <code>ContainerId</code> | <code>12</code> | 12 | <a id="symbol-script-enum-value-itemproperty-containerid-11b56fbc35"></a><code>script.enum-value.ItemProperty.ContainerId</code> | <code>internal</code> (default) | - | - |
| <code>ContainerStack</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-itemproperty-containerstack-37655089ae"></a><code>script.enum-value.ItemProperty.ContainerStack</code> | <code>internal</code> (default) | - | - |
| <code>Corner</code> | <code>19</code> | 19 | <a id="symbol-script-enum-value-itemproperty-corner-f0304f37db"></a><code>script.enum-value.ItemProperty.Corner</code> | <code>internal</code> (default) | - | - |
| <code>Count</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-itemproperty-count-f6b9bd025e"></a><code>script.enum-value.ItemProperty.Count</code> | <code>internal</code> (default) | - | - |
| <code>CritterId</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-itemproperty-critterid-bd77ad2c7d"></a><code>script.enum-value.ItemProperty.CritterId</code> | <code>internal</code> (default) | - | - |
| <code>CritterSlot</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-itemproperty-critterslot-5955631177"></a><code>script.enum-value.ItemProperty.CritterSlot</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-itemproperty-customholderentry-52f3518a3e"></a><code>script.enum-value.ItemProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-itemproperty-customholderid-5164c3a0d6"></a><code>script.enum-value.ItemProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>DisableEgg</code> | <code>20</code> | 20 | <a id="symbol-script-enum-value-itemproperty-disableegg-c1f9dcea1a"></a><code>script.enum-value.ItemProperty.DisableEgg</code> | <code>internal</code> (default) | - | - |
| <code>DrawFlatten</code> | <code>47</code> | 47 | <a id="symbol-script-enum-value-itemproperty-drawflatten-a526d50983"></a><code>script.enum-value.ItemProperty.DrawFlatten</code> | <code>internal</code> (default) | - | - |
| <code>DrawMultihexLines</code> | <code>24</code> | 24 | <a id="symbol-script-enum-value-itemproperty-drawmultihexlines-039827bc60"></a><code>script.enum-value.ItemProperty.DrawMultihexLines</code> | <code>internal</code> (default) | - | - |
| <code>DrawMultihexMesh</code> | <code>25</code> | 25 | <a id="symbol-script-enum-value-itemproperty-drawmultihexmesh-fd658ef9cc"></a><code>script.enum-value.ItemProperty.DrawMultihexMesh</code> | <code>internal</code> (default) | - | - |
| <code>DrawOrderOffsetHexY</code> | <code>48</code> | 48 | <a id="symbol-script-enum-value-itemproperty-draworderoffsethexy-603ca62ccb"></a><code>script.enum-value.ItemProperty.DrawOrderOffsetHexY</code> | <code>internal</code> (default) | - | - |
| <code>Elevation</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-itemproperty-elevation-fa2ee215d5"></a><code>script.enum-value.ItemProperty.Elevation</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-itemproperty-explicitlypersistent-64d1acaa32"></a><code>script.enum-value.ItemProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>Hex</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-itemproperty-hex-c0cd34af91"></a><code>script.enum-value.ItemProperty.Hex</code> | <code>internal</code> (default) | - | - |
| <code>Hidden</code> | <code>26</code> | 26 | <a id="symbol-script-enum-value-itemproperty-hidden-4cc42a84cd"></a><code>script.enum-value.ItemProperty.Hidden</code> | <code>internal</code> (default) | - | - |
| <code>HideSprite</code> | <code>27</code> | 27 | <a id="symbol-script-enum-value-itemproperty-hidesprite-5e3dca4e87"></a><code>script.enum-value.ItemProperty.HideSprite</code> | <code>internal</code> (default) | - | - |
| <code>InitScript</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-itemproperty-initscript-7b8c0eb409"></a><code>script.enum-value.ItemProperty.InitScript</code> | <code>internal</code> (default) | - | - |
| <code>InnerItemIds</code> | <code>14</code> | 14 | <a id="symbol-script-enum-value-itemproperty-inneritemids-400238b54b"></a><code>script.enum-value.ItemProperty.InnerItemIds</code> | <code>internal</code> (default) | - | - |
| <code>IsGag</code> | <code>51</code> | 51 | <a id="symbol-script-enum-value-itemproperty-isgag-f1000234c8"></a><code>script.enum-value.ItemProperty.IsGag</code> | <code>internal</code> (default) | - | - |
| <code>IsRoofTile</code> | <code>45</code> | 45 | <a id="symbol-script-enum-value-itemproperty-isrooftile-e11d1107a2"></a><code>script.enum-value.ItemProperty.IsRoofTile</code> | <code>internal</code> (default) | - | - |
| <code>IsScenery</code> | <code>42</code> | 42 | <a id="symbol-script-enum-value-itemproperty-isscenery-f80ae7f1d1"></a><code>script.enum-value.ItemProperty.IsScenery</code> | <code>internal</code> (default) | - | - |
| <code>IsTile</code> | <code>44</code> | 44 | <a id="symbol-script-enum-value-itemproperty-istile-0e7ea7e77f"></a><code>script.enum-value.ItemProperty.IsTile</code> | <code>internal</code> (default) | - | - |
| <code>IsTrigger</code> | <code>40</code> | 40 | <a id="symbol-script-enum-value-itemproperty-istrigger-22dd62b5d2"></a><code>script.enum-value.ItemProperty.IsTrigger</code> | <code>internal</code> (default) | - | - |
| <code>IsWall</code> | <code>43</code> | 43 | <a id="symbol-script-enum-value-itemproperty-iswall-84af2c36a7"></a><code>script.enum-value.ItemProperty.IsWall</code> | <code>internal</code> (default) | - | - |
| <code>LightColor</code> | <code>36</code> | 36 | <a id="symbol-script-enum-value-itemproperty-lightcolor-6025a00a6a"></a><code>script.enum-value.ItemProperty.LightColor</code> | <code>internal</code> (default) | - | - |
| <code>LightDistance</code> | <code>34</code> | 34 | <a id="symbol-script-enum-value-itemproperty-lightdistance-093712e957"></a><code>script.enum-value.ItemProperty.LightDistance</code> | <code>internal</code> (default) | - | - |
| <code>LightFlags</code> | <code>35</code> | 35 | <a id="symbol-script-enum-value-itemproperty-lightflags-2d9cc8b979"></a><code>script.enum-value.ItemProperty.LightFlags</code> | <code>internal</code> (default) | - | - |
| <code>LightIntensity</code> | <code>33</code> | 33 | <a id="symbol-script-enum-value-itemproperty-lightintensity-b7ef28c129"></a><code>script.enum-value.ItemProperty.LightIntensity</code> | <code>internal</code> (default) | - | - |
| <code>LightSource</code> | <code>32</code> | 32 | <a id="symbol-script-enum-value-itemproperty-lightsource-7a417b0c2d"></a><code>script.enum-value.ItemProperty.LightSource</code> | <code>internal</code> (default) | - | - |
| <code>LightThru</code> | <code>31</code> | 31 | <a id="symbol-script-enum-value-itemproperty-lightthru-be15a7d705"></a><code>script.enum-value.ItemProperty.LightThru</code> | <code>internal</code> (default) | - | - |
| <code>MapId</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-itemproperty-mapid-2f68ee60ec"></a><code>script.enum-value.ItemProperty.MapId</code> | <code>internal</code> (default) | - | - |
| <code>MultihexGeneration</code> | <code>23</code> | 23 | <a id="symbol-script-enum-value-itemproperty-multihexgeneration-27323b5694"></a><code>script.enum-value.ItemProperty.MultihexGeneration</code> | <code>internal</code> (default) | - | - |
| <code>MultihexLines</code> | <code>21</code> | 21 | <a id="symbol-script-enum-value-itemproperty-multihexlines-4df837d8fc"></a><code>script.enum-value.ItemProperty.MultihexLines</code> | <code>internal</code> (default) | - | - |
| <code>MultihexMesh</code> | <code>22</code> | 22 | <a id="symbol-script-enum-value-itemproperty-multihexmesh-cbca82b3e4"></a><code>script.enum-value.ItemProperty.MultihexMesh</code> | <code>internal</code> (default) | - | - |
| <code>NoBlock</code> | <code>29</code> | 29 | <a id="symbol-script-enum-value-itemproperty-noblock-09536c7fc1"></a><code>script.enum-value.ItemProperty.NoBlock</code> | <code>internal</code> (default) | - | - |
| <code>NoHighlight</code> | <code>49</code> | 49 | <a id="symbol-script-enum-value-itemproperty-nohighlight-b772953ec7"></a><code>script.enum-value.ItemProperty.NoHighlight</code> | <code>internal</code> (default) | - | - |
| <code>NoLightInfluence</code> | <code>50</code> | 50 | <a id="symbol-script-enum-value-itemproperty-nolightinfluence-d179e4c896"></a><code>script.enum-value.ItemProperty.NoLightInfluence</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-itemproperty-none-6638d24533"></a><code>script.enum-value.ItemProperty.None</code> | <code>internal</code> (default) | - | - |
| <code>Offset</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-itemproperty-offset-a82c15ee61"></a><code>script.enum-value.ItemProperty.Offset</code> | <code>internal</code> (default) | - | - |
| <code>Ownership</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-itemproperty-ownership-3225786d21"></a><code>script.enum-value.ItemProperty.Ownership</code> | <code>internal</code> (default) | - | - |
| <code>PicInv</code> | <code>41</code> | 41 | <a id="symbol-script-enum-value-itemproperty-picinv-e854e1012b"></a><code>script.enum-value.ItemProperty.PicInv</code> | <code>internal</code> (default) | - | - |
| <code>PicMap</code> | <code>17</code> | 17 | <a id="symbol-script-enum-value-itemproperty-picmap-24a2edeacd"></a><code>script.enum-value.ItemProperty.PicMap</code> | <code>internal</code> (default) | - | - |
| <code>ShootThru</code> | <code>30</code> | 30 | <a id="symbol-script-enum-value-itemproperty-shootthru-bc731b7dca"></a><code>script.enum-value.ItemProperty.ShootThru</code> | <code>internal</code> (default) | - | - |
| <code>Stackable</code> | <code>15</code> | 15 | <a id="symbol-script-enum-value-itemproperty-stackable-59c0ba3260"></a><code>script.enum-value.ItemProperty.Stackable</code> | <code>internal</code> (default) | - | - |
| <code>Static</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-itemproperty-static-8e7c73113d"></a><code>script.enum-value.ItemProperty.Static</code> | <code>internal</code> (default) | - | - |
| <code>StaticScript</code> | <code>38</code> | 38 | <a id="symbol-script-enum-value-itemproperty-staticscript-1fcfa22310"></a><code>script.enum-value.ItemProperty.StaticScript</code> | <code>internal</code> (default) | - | - |
| <code>TileLayer</code> | <code>46</code> | 46 | <a id="symbol-script-enum-value-itemproperty-tilelayer-0255036e37"></a><code>script.enum-value.ItemProperty.TileLayer</code> | <code>internal</code> (default) | - | - |
| <code>TriggerScript</code> | <code>39</code> | 39 | <a id="symbol-script-enum-value-itemproperty-triggerscript-66589ee8c9"></a><code>script.enum-value.ItemProperty.TriggerScript</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-keycode-5908ee7ee0"></a>
### <code>KeyCode</code>

<code>enum KeyCode : uint8</code>  
Symbol ID: <code>script.enum.KeyCode</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>A</code> | <code>0x1E</code> | 30 | <a id="symbol-script-enum-value-keycode-a-117796d422"></a><code>script.enum-value.KeyCode.A</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Add</code> | <code>0x4E</code> | 78 | <a id="symbol-script-enum-value-keycode-add-785b7a58be"></a><code>script.enum-value.KeyCode.Add</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Apostrophe</code> | <code>0x28</code> | 40 | <a id="symbol-script-enum-value-keycode-apostrophe-4fc33edc46"></a><code>script.enum-value.KeyCode.Apostrophe</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>B</code> | <code>0x30</code> | 48 | <a id="symbol-script-enum-value-keycode-b-4de86d01e5"></a><code>script.enum-value.KeyCode.B</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Back</code> | <code>0x0E</code> | 14 | <a id="symbol-script-enum-value-keycode-back-594a25ef5e"></a><code>script.enum-value.KeyCode.Back</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Backslash</code> | <code>0x2B</code> | 43 | <a id="symbol-script-enum-value-keycode-backslash-a2ec152112"></a><code>script.enum-value.KeyCode.Backslash</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C</code> | <code>0x2E</code> | 46 | <a id="symbol-script-enum-value-keycode-c-ddca079b79"></a><code>script.enum-value.KeyCode.C</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C0</code> | <code>0x0B</code> | 11 | <a id="symbol-script-enum-value-keycode-c0-1a40f906b0"></a><code>script.enum-value.KeyCode.C0</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C1</code> | <code>0x02</code> | 2 | <a id="symbol-script-enum-value-keycode-c1-23ad1e331e"></a><code>script.enum-value.KeyCode.C1</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C2</code> | <code>0x03</code> | 3 | <a id="symbol-script-enum-value-keycode-c2-ad63746c88"></a><code>script.enum-value.KeyCode.C2</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C3</code> | <code>0x04</code> | 4 | <a id="symbol-script-enum-value-keycode-c3-24dec1783d"></a><code>script.enum-value.KeyCode.C3</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C4</code> | <code>0x05</code> | 5 | <a id="symbol-script-enum-value-keycode-c4-7118dcbecd"></a><code>script.enum-value.KeyCode.C4</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C5</code> | <code>0x06</code> | 6 | <a id="symbol-script-enum-value-keycode-c5-628bf2ae28"></a><code>script.enum-value.KeyCode.C5</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C6</code> | <code>0x07</code> | 7 | <a id="symbol-script-enum-value-keycode-c6-405b8343ea"></a><code>script.enum-value.KeyCode.C6</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C7</code> | <code>0x08</code> | 8 | <a id="symbol-script-enum-value-keycode-c7-75ae40d623"></a><code>script.enum-value.KeyCode.C7</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C8</code> | <code>0x09</code> | 9 | <a id="symbol-script-enum-value-keycode-c8-03e8fe3ed8"></a><code>script.enum-value.KeyCode.C8</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>C9</code> | <code>0x0A</code> | 10 | <a id="symbol-script-enum-value-keycode-c9-e4b4392caf"></a><code>script.enum-value.KeyCode.C9</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Capital</code> | <code>0x3A</code> | 58 | <a id="symbol-script-enum-value-keycode-capital-1c7148e1db"></a><code>script.enum-value.KeyCode.Capital</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Comma</code> | <code>0x33</code> | 51 | <a id="symbol-script-enum-value-keycode-comma-cd18c9d69a"></a><code>script.enum-value.KeyCode.Comma</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>D</code> | <code>0x20</code> | 32 | <a id="symbol-script-enum-value-keycode-d-807a315e7c"></a><code>script.enum-value.KeyCode.D</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Decimal</code> | <code>0x53</code> | 83 | <a id="symbol-script-enum-value-keycode-decimal-00be1d5887"></a><code>script.enum-value.KeyCode.Decimal</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Delete</code> | <code>0xD3</code> | 211 | <a id="symbol-script-enum-value-keycode-delete-9d62d81cb1"></a><code>script.enum-value.KeyCode.Delete</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Divide</code> | <code>0xB5</code> | 181 | <a id="symbol-script-enum-value-keycode-divide-546b5f98fb"></a><code>script.enum-value.KeyCode.Divide</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Down</code> | <code>0xD0</code> | 208 | <a id="symbol-script-enum-value-keycode-down-d0498747e6"></a><code>script.enum-value.KeyCode.Down</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>E</code> | <code>0x12</code> | 18 | <a id="symbol-script-enum-value-keycode-e-964e4db0a5"></a><code>script.enum-value.KeyCode.E</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>End</code> | <code>0xCF</code> | 207 | <a id="symbol-script-enum-value-keycode-end-0e1a3c328f"></a><code>script.enum-value.KeyCode.End</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Equals</code> | <code>0x0D</code> | 13 | <a id="symbol-script-enum-value-keycode-equals-5121844e1c"></a><code>script.enum-value.KeyCode.Equals</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Escape</code> | <code>0x01</code> | 1 | <a id="symbol-script-enum-value-keycode-escape-06d8bec55e"></a><code>script.enum-value.KeyCode.Escape</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F</code> | <code>0x21</code> | 33 | <a id="symbol-script-enum-value-keycode-f-cce1bbd462"></a><code>script.enum-value.KeyCode.F</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F1</code> | <code>0x3B</code> | 59 | <a id="symbol-script-enum-value-keycode-f1-a9ce8717ff"></a><code>script.enum-value.KeyCode.F1</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F10</code> | <code>0x44</code> | 68 | <a id="symbol-script-enum-value-keycode-f10-d203f11997"></a><code>script.enum-value.KeyCode.F10</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F11</code> | <code>0x57</code> | 87 | <a id="symbol-script-enum-value-keycode-f11-24702b1817"></a><code>script.enum-value.KeyCode.F11</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F12</code> | <code>0x58</code> | 88 | <a id="symbol-script-enum-value-keycode-f12-6646329060"></a><code>script.enum-value.KeyCode.F12</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F2</code> | <code>0x3C</code> | 60 | <a id="symbol-script-enum-value-keycode-f2-2b5169b2d2"></a><code>script.enum-value.KeyCode.F2</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F3</code> | <code>0x3D</code> | 61 | <a id="symbol-script-enum-value-keycode-f3-5e51ea1083"></a><code>script.enum-value.KeyCode.F3</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F4</code> | <code>0x3E</code> | 62 | <a id="symbol-script-enum-value-keycode-f4-be125b533e"></a><code>script.enum-value.KeyCode.F4</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F5</code> | <code>0x3F</code> | 63 | <a id="symbol-script-enum-value-keycode-f5-e538944b9f"></a><code>script.enum-value.KeyCode.F5</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F6</code> | <code>0x40</code> | 64 | <a id="symbol-script-enum-value-keycode-f6-c175e61f4c"></a><code>script.enum-value.KeyCode.F6</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F7</code> | <code>0x41</code> | 65 | <a id="symbol-script-enum-value-keycode-f7-14ce60c668"></a><code>script.enum-value.KeyCode.F7</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F8</code> | <code>0x42</code> | 66 | <a id="symbol-script-enum-value-keycode-f8-fee2ad6bba"></a><code>script.enum-value.KeyCode.F8</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>F9</code> | <code>0x43</code> | 67 | <a id="symbol-script-enum-value-keycode-f9-9bcd850f7f"></a><code>script.enum-value.KeyCode.F9</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>G</code> | <code>0x22</code> | 34 | <a id="symbol-script-enum-value-keycode-g-2ba7e52cac"></a><code>script.enum-value.KeyCode.G</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Grave</code> | <code>0x29</code> | 41 | <a id="symbol-script-enum-value-keycode-grave-1fe9d5b2f3"></a><code>script.enum-value.KeyCode.Grave</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>H</code> | <code>0x23</code> | 35 | <a id="symbol-script-enum-value-keycode-h-649389192c"></a><code>script.enum-value.KeyCode.H</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Home</code> | <code>0xC7</code> | 199 | <a id="symbol-script-enum-value-keycode-home-27b2508b97"></a><code>script.enum-value.KeyCode.Home</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>I</code> | <code>0x17</code> | 23 | <a id="symbol-script-enum-value-keycode-i-10dc53fd98"></a><code>script.enum-value.KeyCode.I</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Insert</code> | <code>0xD2</code> | 210 | <a id="symbol-script-enum-value-keycode-insert-ec13ca6f2c"></a><code>script.enum-value.KeyCode.Insert</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>J</code> | <code>0x24</code> | 36 | <a id="symbol-script-enum-value-keycode-j-b72248d74d"></a><code>script.enum-value.KeyCode.J</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>K</code> | <code>0x25</code> | 37 | <a id="symbol-script-enum-value-keycode-k-5d363d0ce5"></a><code>script.enum-value.KeyCode.K</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>L</code> | <code>0x26</code> | 38 | <a id="symbol-script-enum-value-keycode-l-0fe9528def"></a><code>script.enum-value.KeyCode.L</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Lbracket</code> | <code>0x1A</code> | 26 | <a id="symbol-script-enum-value-keycode-lbracket-45ed017b60"></a><code>script.enum-value.KeyCode.Lbracket</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Lcontrol</code> | <code>0x1D</code> | 29 | <a id="symbol-script-enum-value-keycode-lcontrol-afcc1dc78d"></a><code>script.enum-value.KeyCode.Lcontrol</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Left</code> | <code>0xCB</code> | 203 | <a id="symbol-script-enum-value-keycode-left-7338d280d0"></a><code>script.enum-value.KeyCode.Left</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Lmenu</code> | <code>0x38</code> | 56 | <a id="symbol-script-enum-value-keycode-lmenu-9d2fda9713"></a><code>script.enum-value.KeyCode.Lmenu</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Lshift</code> | <code>0x2A</code> | 42 | <a id="symbol-script-enum-value-keycode-lshift-f5f8904ee9"></a><code>script.enum-value.KeyCode.Lshift</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Lwin</code> | <code>0xDB</code> | 219 | <a id="symbol-script-enum-value-keycode-lwin-447f2aa70e"></a><code>script.enum-value.KeyCode.Lwin</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>M</code> | <code>0x32</code> | 50 | <a id="symbol-script-enum-value-keycode-m-0deda84cb2"></a><code>script.enum-value.KeyCode.M</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Minus</code> | <code>0x0C</code> | 12 | <a id="symbol-script-enum-value-keycode-minus-95b58c9faa"></a><code>script.enum-value.KeyCode.Minus</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Multiply</code> | <code>0x37</code> | 55 | <a id="symbol-script-enum-value-keycode-multiply-301b0f6db0"></a><code>script.enum-value.KeyCode.Multiply</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>N</code> | <code>0x31</code> | 49 | <a id="symbol-script-enum-value-keycode-n-6b69a4d2f5"></a><code>script.enum-value.KeyCode.N</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Next</code> | <code>0xD1</code> | 209 | <a id="symbol-script-enum-value-keycode-next-345da4e39d"></a><code>script.enum-value.KeyCode.Next</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>None</code> | <code>0x00</code> | 0 | <a id="symbol-script-enum-value-keycode-none-671ddb4eaf"></a><code>script.enum-value.KeyCode.None</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numlock</code> | <code>0x45</code> | 69 | <a id="symbol-script-enum-value-keycode-numlock-15b02fe74a"></a><code>script.enum-value.KeyCode.Numlock</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad0</code> | <code>0x52</code> | 82 | <a id="symbol-script-enum-value-keycode-numpad0-aae643cf61"></a><code>script.enum-value.KeyCode.Numpad0</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad1</code> | <code>0x4F</code> | 79 | <a id="symbol-script-enum-value-keycode-numpad1-52f822c0a6"></a><code>script.enum-value.KeyCode.Numpad1</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad2</code> | <code>0x50</code> | 80 | <a id="symbol-script-enum-value-keycode-numpad2-981e94021a"></a><code>script.enum-value.KeyCode.Numpad2</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad3</code> | <code>0x51</code> | 81 | <a id="symbol-script-enum-value-keycode-numpad3-4ad4c371cd"></a><code>script.enum-value.KeyCode.Numpad3</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad4</code> | <code>0x4B</code> | 75 | <a id="symbol-script-enum-value-keycode-numpad4-241790fb3c"></a><code>script.enum-value.KeyCode.Numpad4</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad5</code> | <code>0x4C</code> | 76 | <a id="symbol-script-enum-value-keycode-numpad5-de5272b4bc"></a><code>script.enum-value.KeyCode.Numpad5</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad6</code> | <code>0x4D</code> | 77 | <a id="symbol-script-enum-value-keycode-numpad6-98e489e23a"></a><code>script.enum-value.KeyCode.Numpad6</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad7</code> | <code>0x47</code> | 71 | <a id="symbol-script-enum-value-keycode-numpad7-a45430417d"></a><code>script.enum-value.KeyCode.Numpad7</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad8</code> | <code>0x48</code> | 72 | <a id="symbol-script-enum-value-keycode-numpad8-7c1f9a4d1b"></a><code>script.enum-value.KeyCode.Numpad8</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpad9</code> | <code>0x49</code> | 73 | <a id="symbol-script-enum-value-keycode-numpad9-79471d1662"></a><code>script.enum-value.KeyCode.Numpad9</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Numpadenter</code> | <code>0x9C</code> | 156 | <a id="symbol-script-enum-value-keycode-numpadenter-daa1c6ea57"></a><code>script.enum-value.KeyCode.Numpadenter</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>O</code> | <code>0x18</code> | 24 | <a id="symbol-script-enum-value-keycode-o-cbdcf83afd"></a><code>script.enum-value.KeyCode.O</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>P</code> | <code>0x19</code> | 25 | <a id="symbol-script-enum-value-keycode-p-ffe434e4eb"></a><code>script.enum-value.KeyCode.P</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Pause</code> | <code>0xC5</code> | 197 | <a id="symbol-script-enum-value-keycode-pause-1933a2f868"></a><code>script.enum-value.KeyCode.Pause</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Period</code> | <code>0x34</code> | 52 | <a id="symbol-script-enum-value-keycode-period-fd7b56c2ac"></a><code>script.enum-value.KeyCode.Period</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Prior</code> | <code>0xC9</code> | 201 | <a id="symbol-script-enum-value-keycode-prior-c16f771430"></a><code>script.enum-value.KeyCode.Prior</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Q</code> | <code>0x10</code> | 16 | <a id="symbol-script-enum-value-keycode-q-b28ccdb2f5"></a><code>script.enum-value.KeyCode.Q</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>R</code> | <code>0x13</code> | 19 | <a id="symbol-script-enum-value-keycode-r-52eee03408"></a><code>script.enum-value.KeyCode.R</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Rbracket</code> | <code>0x1B</code> | 27 | <a id="symbol-script-enum-value-keycode-rbracket-42591b8392"></a><code>script.enum-value.KeyCode.Rbracket</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Rcontrol</code> | <code>0x9D</code> | 157 | <a id="symbol-script-enum-value-keycode-rcontrol-00571e9de1"></a><code>script.enum-value.KeyCode.Rcontrol</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Return</code> | <code>0x1C</code> | 28 | <a id="symbol-script-enum-value-keycode-return-a2ca2c283f"></a><code>script.enum-value.KeyCode.Return</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Right</code> | <code>0xCD</code> | 205 | <a id="symbol-script-enum-value-keycode-right-46d3fde046"></a><code>script.enum-value.KeyCode.Right</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Rmenu</code> | <code>0xB8</code> | 184 | <a id="symbol-script-enum-value-keycode-rmenu-1098912dea"></a><code>script.enum-value.KeyCode.Rmenu</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Rshift</code> | <code>0x36</code> | 54 | <a id="symbol-script-enum-value-keycode-rshift-34bf7c6d77"></a><code>script.enum-value.KeyCode.Rshift</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Rwin</code> | <code>0xDC</code> | 220 | <a id="symbol-script-enum-value-keycode-rwin-5c1ffe43a8"></a><code>script.enum-value.KeyCode.Rwin</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>S</code> | <code>0x1F</code> | 31 | <a id="symbol-script-enum-value-keycode-s-297c7cc7a1"></a><code>script.enum-value.KeyCode.S</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Scroll</code> | <code>0x46</code> | 70 | <a id="symbol-script-enum-value-keycode-scroll-a2baa7862b"></a><code>script.enum-value.KeyCode.Scroll</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Semicolon</code> | <code>0x27</code> | 39 | <a id="symbol-script-enum-value-keycode-semicolon-5c520b78c7"></a><code>script.enum-value.KeyCode.Semicolon</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Slash</code> | <code>0x35</code> | 53 | <a id="symbol-script-enum-value-keycode-slash-82bde41430"></a><code>script.enum-value.KeyCode.Slash</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Space</code> | <code>0x39</code> | 57 | <a id="symbol-script-enum-value-keycode-space-d6b9c50c23"></a><code>script.enum-value.KeyCode.Space</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Subtract</code> | <code>0x4A</code> | 74 | <a id="symbol-script-enum-value-keycode-subtract-a45d17d66a"></a><code>script.enum-value.KeyCode.Subtract</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Sysrq</code> | <code>0xB7</code> | 183 | <a id="symbol-script-enum-value-keycode-sysrq-27df47a8e9"></a><code>script.enum-value.KeyCode.Sysrq</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>T</code> | <code>0x14</code> | 20 | <a id="symbol-script-enum-value-keycode-t-5bb02a6cff"></a><code>script.enum-value.KeyCode.T</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Tab</code> | <code>0x0F</code> | 15 | <a id="symbol-script-enum-value-keycode-tab-f3ade6c075"></a><code>script.enum-value.KeyCode.Tab</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Text</code> | <code>0xFF</code> | 255 | <a id="symbol-script-enum-value-keycode-text-bc4120ade2"></a><code>script.enum-value.KeyCode.Text</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>U</code> | <code>0x16</code> | 22 | <a id="symbol-script-enum-value-keycode-u-65c4e3ade0"></a><code>script.enum-value.KeyCode.U</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Up</code> | <code>0xC8</code> | 200 | <a id="symbol-script-enum-value-keycode-up-42890b8397"></a><code>script.enum-value.KeyCode.Up</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>V</code> | <code>0x2F</code> | 47 | <a id="symbol-script-enum-value-keycode-v-eb90d68a67"></a><code>script.enum-value.KeyCode.V</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>W</code> | <code>0x11</code> | 17 | <a id="symbol-script-enum-value-keycode-w-47fb56f76a"></a><code>script.enum-value.KeyCode.W</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>X</code> | <code>0x2D</code> | 45 | <a id="symbol-script-enum-value-keycode-x-31d7c01594"></a><code>script.enum-value.KeyCode.X</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Y</code> | <code>0x15</code> | 21 | <a id="symbol-script-enum-value-keycode-y-00d170a194"></a><code>script.enum-value.KeyCode.Y</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |
| <code>Z</code> | <code>0x2C</code> | 44 | <a id="symbol-script-enum-value-keycode-z-a9cdf61297"></a><code>script.enum-value.KeyCode.Z</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:49](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L49) | - |

<a id="symbol-script-enum-locationproperty-5a3cf941d0"></a>
### <code>LocationProperty</code>

<code>enum LocationProperty : uint16</code>  
Symbol ID: <code>script.enum.LocationProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-locationproperty-customholderentry-a241eb41fa"></a><code>script.enum-value.LocationProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-locationproperty-customholderid-617552fda2"></a><code>script.enum-value.LocationProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-locationproperty-explicitlypersistent-e611ea5e17"></a><code>script.enum-value.LocationProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>InitScript</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-locationproperty-initscript-d69db0aa75"></a><code>script.enum-value.LocationProperty.InitScript</code> | <code>internal</code> (default) | - | - |
| <code>MapIds</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-locationproperty-mapids-d0db77c341"></a><code>script.enum-value.LocationProperty.MapIds</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-locationproperty-none-83f012bd1c"></a><code>script.enum-value.LocationProperty.None</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-mapproperty-cbf22f8d2d"></a>
### <code>MapProperty</code>

<code>enum MapProperty : uint16</code>  
Symbol ID: <code>script.enum.MapProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>CritterIds</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-mapproperty-critterids-e03343e34e"></a><code>script.enum-value.MapProperty.CritterIds</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-mapproperty-customholderentry-bc38720e5e"></a><code>script.enum-value.MapProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-mapproperty-customholderid-cd03a81354"></a><code>script.enum-value.MapProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>DayColor</code> | <code>17</code> | 17 | <a id="symbol-script-enum-value-mapproperty-daycolor-e6d869ec84"></a><code>script.enum-value.MapProperty.DayColor</code> | <code>internal</code> (default) | - | - |
| <code>DayColorTime</code> | <code>16</code> | 16 | <a id="symbol-script-enum-value-mapproperty-daycolortime-a646a284ea"></a><code>script.enum-value.MapProperty.DayColorTime</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-mapproperty-explicitlypersistent-562516f64b"></a><code>script.enum-value.MapProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>FixedDayTime</code> | <code>15</code> | 15 | <a id="symbol-script-enum-value-mapproperty-fixeddaytime-c8724a88f3"></a><code>script.enum-value.MapProperty.FixedDayTime</code> | <code>internal</code> (default) | - | - |
| <code>GlobalDayColor</code> | <code>19</code> | 19 | <a id="symbol-script-enum-value-mapproperty-globaldaycolor-39aa9726e5"></a><code>script.enum-value.MapProperty.GlobalDayColor</code> | <code>internal</code> (default) | - | - |
| <code>GlobalDayLightCapacity</code> | <code>21</code> | 21 | <a id="symbol-script-enum-value-mapproperty-globaldaylightcapacity-a6ed70ff17"></a><code>script.enum-value.MapProperty.GlobalDayLightCapacity</code> | <code>internal</code> (default) | - | - |
| <code>InitScript</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-mapproperty-initscript-b876da1053"></a><code>script.enum-value.MapProperty.InitScript</code> | <code>internal</code> (default) | - | - |
| <code>ItemIds</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-mapproperty-itemids-e2cdd30553"></a><code>script.enum-value.MapProperty.ItemIds</code> | <code>internal</code> (default) | - | - |
| <code>LocId</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-mapproperty-locid-a93d344230"></a><code>script.enum-value.MapProperty.LocId</code> | <code>internal</code> (default) | - | - |
| <code>LocMapIndex</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-mapproperty-locmapindex-0dba1de3b8"></a><code>script.enum-value.MapProperty.LocMapIndex</code> | <code>internal</code> (default) | - | - |
| <code>MapDayColor</code> | <code>18</code> | 18 | <a id="symbol-script-enum-value-mapproperty-mapdaycolor-ce998ef7b8"></a><code>script.enum-value.MapProperty.MapDayColor</code> | <code>internal</code> (default) | - | - |
| <code>MapDayLightCapacity</code> | <code>20</code> | 20 | <a id="symbol-script-enum-value-mapproperty-mapdaylightcapacity-d9a77fba3b"></a><code>script.enum-value.MapProperty.MapDayLightCapacity</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-mapproperty-none-23bf9e84fc"></a><code>script.enum-value.MapProperty.None</code> | <code>internal</code> (default) | - | - |
| <code>ScrollAxialArea</code> | <code>12</code> | 12 | <a id="symbol-script-enum-value-mapproperty-scrollaxialarea-79e9b2730e"></a><code>script.enum-value.MapProperty.ScrollAxialArea</code> | <code>internal</code> (default) | - | - |
| <code>ScrollOffset</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-mapproperty-scrolloffset-1c50ee7560"></a><code>script.enum-value.MapProperty.ScrollOffset</code> | <code>internal</code> (default) | - | - |
| <code>Size</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-mapproperty-size-599b665a36"></a><code>script.enum-value.MapProperty.Size</code> | <code>internal</code> (default) | - | - |
| <code>SpritesZoom</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-mapproperty-spriteszoom-1b1dd831d3"></a><code>script.enum-value.MapProperty.SpritesZoom</code> | <code>internal</code> (default) | - | - |
| <code>SpritesZoomTarget</code> | <code>14</code> | 14 | <a id="symbol-script-enum-value-mapproperty-spriteszoomtarget-9a4dbb07fd"></a><code>script.enum-value.MapProperty.SpritesZoomTarget</code> | <code>internal</code> (default) | - | - |
| <code>WorkHex</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-mapproperty-workhex-b6fe6839d2"></a><code>script.enum-value.MapProperty.WorkHex</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-mousebutton-bacc57ac02"></a>
### <code>MouseButton</code>

<code>enum MouseButton : uint8</code>  
Symbol ID: <code>script.enum.MouseButton</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Ext0</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-mousebutton-ext0-cd463d7efd"></a><code>script.enum-value.MouseButton.Ext0</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Ext1</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-mousebutton-ext1-e4b625ed71"></a><code>script.enum-value.MouseButton.Ext1</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Ext2</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-mousebutton-ext2-f7f1e7cdf9"></a><code>script.enum-value.MouseButton.Ext2</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Ext3</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-mousebutton-ext3-80ddaa2c3c"></a><code>script.enum-value.MouseButton.Ext3</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Ext4</code> | <code>9</code> | 9 | <a id="symbol-script-enum-value-mousebutton-ext4-d32d4b842f"></a><code>script.enum-value.MouseButton.Ext4</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Left</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-mousebutton-left-ebc6512011"></a><code>script.enum-value.MouseButton.Left</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Middle</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-mousebutton-middle-962ed22259"></a><code>script.enum-value.MouseButton.Middle</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>Right</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-mousebutton-right-3bbb49267e"></a><code>script.enum-value.MouseButton.Right</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>WheelDown</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-mousebutton-wheeldown-cecdb674de"></a><code>script.enum-value.MouseButton.WheelDown</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |
| <code>WheelUp</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-mousebutton-wheelup-532fdbcdc1"></a><code>script.enum-value.MouseButton.WheelUp</code> | <code>internal</code> (default) | [Source/Frontend/Application.h:159](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L159) | - |

<a id="symbol-script-enum-movingstate-eb721ca711"></a>
### <code>MovingState</code>

<code>enum MovingState : uint8</code>  
Symbol ID: <code>script.enum.MovingState</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Attached</code> | <code>13</code> | 13 | <a id="symbol-script-enum-value-movingstate-attached-0f6435a408"></a><code>script.enum-value.MovingState.Attached</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>CantMove</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-movingstate-cantmove-c0fdae51f0"></a><code>script.enum-value.MovingState.CantMove</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>Deadlock</code> | <code>10</code> | 10 | <a id="symbol-script-enum-value-movingstate-deadlock-6dd9526c6d"></a><code>script.enum-value.MovingState.Deadlock</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>GagCritter</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-movingstate-gagcritter-6f80e8a86f"></a><code>script.enum-value.MovingState.GagCritter</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>GagItem</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-movingstate-gagitem-3e2bf95a77"></a><code>script.enum-value.MovingState.GagItem</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>GenericError</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-movingstate-genericerror-8086021ebd"></a><code>script.enum-value.MovingState.GenericError</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>HexBusy</code> | <code>8</code> | 8 | <a id="symbol-script-enum-value-movingstate-hexbusy-040aa1e20a"></a><code>script.enum-value.MovingState.HexBusy</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>HexTooFar</code> | <code>7</code> | 7 | <a id="symbol-script-enum-value-movingstate-hextoofar-81a11bb4d6"></a><code>script.enum-value.MovingState.HexTooFar</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>InProgress</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-movingstate-inprogress-6b411de6ba"></a><code>script.enum-value.MovingState.InProgress</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>NotAlive</code> | <code>12</code> | 12 | <a id="symbol-script-enum-value-movingstate-notalive-50a0500bf2"></a><code>script.enum-value.MovingState.NotAlive</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>Stopped</code> | <code>14</code> | 14 | <a id="symbol-script-enum-value-movingstate-stopped-508dcff1b3"></a><code>script.enum-value.MovingState.Stopped</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>Success</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-movingstate-success-12fe0ef8d8"></a><code>script.enum-value.MovingState.Success</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>TargetNotFound</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-movingstate-targetnotfound-4123839e40"></a><code>script.enum-value.MovingState.TargetNotFound</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |
| <code>TraceFailed</code> | <code>11</code> | 11 | <a id="symbol-script-enum-value-movingstate-tracefailed-45fdb61396"></a><code>script.enum-value.MovingState.TraceFailed</code> | <code>internal</code> (default) | [Source/Common/Movement.h:42](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L42) | - |

<a id="symbol-script-enum-multihexgenerationtype-6a63829cf3"></a>
### <code>MultihexGenerationType</code>

<code>enum MultihexGenerationType : uint8</code>  
Symbol ID: <code>script.enum.MultihexGenerationType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Common/Common.h:1009](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1009)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>AnyUnique</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-multihexgenerationtype-anyunique-ac7b3447c3"></a><code>script.enum-value.MultihexGenerationType.AnyUnique</code> | <code>internal</code> (default) | [Source/Common/Common.h:1009](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1009) | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-multihexgenerationtype-none-e010cd8cd3"></a><code>script.enum-value.MultihexGenerationType.None</code> | <code>internal</code> (default) | [Source/Common/Common.h:1009](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1009) | - |
| <code>SameSibling</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-multihexgenerationtype-samesibling-9bf9686f19"></a><code>script.enum-value.MultihexGenerationType.SameSibling</code> | <code>internal</code> (default) | [Source/Common/Common.h:1009](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1009) | - |

<a id="symbol-script-enum-playerproperty-96d5a536b2"></a>
### <code>PlayerProperty</code>

<code>enum PlayerProperty : uint16</code>  
Symbol ID: <code>script.enum.PlayerProperty</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: -

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>ControlledCritterId</code> | <code>5</code> | 5 | <a id="symbol-script-enum-value-playerproperty-controlledcritterid-e3ec2794f5"></a><code>script.enum-value.PlayerProperty.ControlledCritterId</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderEntry</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-playerproperty-customholderentry-5892e920e3"></a><code>script.enum-value.PlayerProperty.CustomHolderEntry</code> | <code>internal</code> (default) | - | - |
| <code>CustomHolderId</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-playerproperty-customholderid-4904069b38"></a><code>script.enum-value.PlayerProperty.CustomHolderId</code> | <code>internal</code> (default) | - | - |
| <code>ExplicitlyPersistent</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-playerproperty-explicitlypersistent-adaa853ee2"></a><code>script.enum-value.PlayerProperty.ExplicitlyPersistent</code> | <code>internal</code> (default) | - | - |
| <code>LastControlledCritterId</code> | <code>6</code> | 6 | <a id="symbol-script-enum-value-playerproperty-lastcontrolledcritterid-58dcfaa8b1"></a><code>script.enum-value.PlayerProperty.LastControlledCritterId</code> | <code>internal</code> (default) | - | - |
| <code>Logined</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-playerproperty-logined-8ed61a6787"></a><code>script.enum-value.PlayerProperty.Logined</code> | <code>internal</code> (default) | - | - |
| <code>None</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-playerproperty-none-973f5ea126"></a><code>script.enum-value.PlayerProperty.None</code> | <code>internal</code> (default) | - | - |

<a id="symbol-script-enum-renderprimitivetype-4ccb2772ca"></a>
### <code>RenderPrimitiveType</code>

<code>enum RenderPrimitiveType : uint8</code>  
Symbol ID: <code>script.enum.RenderPrimitiveType</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Frontend/Rendering.h:97](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h#L97)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>LineList</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-renderprimitivetype-linelist-e599e71381"></a><code>script.enum-value.RenderPrimitiveType.LineList</code> | <code>internal</code> (default) | [Source/Frontend/Rendering.h:97](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h#L97) | - |
| <code>LineStrip</code> | <code>2</code> | 2 | <a id="symbol-script-enum-value-renderprimitivetype-linestrip-432b685ab6"></a><code>script.enum-value.RenderPrimitiveType.LineStrip</code> | <code>internal</code> (default) | [Source/Frontend/Rendering.h:97](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h#L97) | - |
| <code>PointList</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-renderprimitivetype-pointlist-204d7329b7"></a><code>script.enum-value.RenderPrimitiveType.PointList</code> | <code>internal</code> (default) | [Source/Frontend/Rendering.h:97](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h#L97) | - |
| <code>TriangleList</code> | <code>3</code> | 3 | <a id="symbol-script-enum-value-renderprimitivetype-trianglelist-bead3e525f"></a><code>script.enum-value.RenderPrimitiveType.TriangleList</code> | <code>internal</code> (default) | [Source/Frontend/Rendering.h:97](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h#L97) | - |
| <code>TriangleStrip</code> | <code>4</code> | 4 | <a id="symbol-script-enum-value-renderprimitivetype-trianglestrip-a4efe1c6ce"></a><code>script.enum-value.RenderPrimitiveType.TriangleStrip</code> | <code>internal</code> (default) | [Source/Frontend/Rendering.h:97](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h#L97) | - |

<a id="symbol-script-enum-transparenteggslot-7c45432993"></a>
### <code>TransparentEggSlot</code>

<code>enum TransparentEggSlot : uint8</code>  
Symbol ID: <code>script.enum.TransparentEggSlot</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: -  
Source: [Source/Client/SpriteManager.h:61](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L61)

-

| Value | Declared | Numeric | Symbol ID | Contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <code>Primary</code> | <code>0</code> | 0 | <a id="symbol-script-enum-value-transparenteggslot-primary-7b4e6a047a"></a><code>script.enum-value.TransparentEggSlot.Primary</code> | <code>internal</code> (default) | [Source/Client/SpriteManager.h:61](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L61) | - |
| <code>Secondary</code> | <code>1</code> | 1 | <a id="symbol-script-enum-value-transparenteggslot-secondary-2be87e7211"></a><code>script.enum-value.TransparentEggSlot.Secondary</code> | <code>internal</code> (default) | [Source/Client/SpriteManager.h:61](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L61) | - |

## Value Types

<a id="symbol-script-value-type-gamepadstate-912c2e3e03"></a>
### <code>GamepadState</code>

<code>value type GamepadState</code>  
Symbol ID: <code>script.value-type.GamepadState</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>float32</code>, <code>-</code>, <code>LeftStickX</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>LeftStickY</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>RightStickX</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>RightStickY</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>LeftTrigger</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>RightTrigger</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>Available</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>South</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>East</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>West</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>North</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>Back</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>Start</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>LeftStickButton</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>RightStickButton</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>LeftShoulder</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>RightShoulder</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>DpadUp</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>DpadDown</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>DpadLeft</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>DpadRight</code>, <code>+</code>, <code>bool</code>, <code>-</code>, <code>Reserved</code>  
Source: [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>bool GamepadState.Available</code> | <a id="symbol-script-value-field-gamepadstate-available-ef6732fd73"></a><code>script.value-field.GamepadState.Available</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.Back</code> | <a id="symbol-script-value-field-gamepadstate-back-2e621c1bb5"></a><code>script.value-field.GamepadState.Back</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.DpadDown</code> | <a id="symbol-script-value-field-gamepadstate-dpaddown-78b9f2375c"></a><code>script.value-field.GamepadState.DpadDown</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.DpadLeft</code> | <a id="symbol-script-value-field-gamepadstate-dpadleft-28dd3f9618"></a><code>script.value-field.GamepadState.DpadLeft</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.DpadRight</code> | <a id="symbol-script-value-field-gamepadstate-dpadright-3845212f8f"></a><code>script.value-field.GamepadState.DpadRight</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.DpadUp</code> | <a id="symbol-script-value-field-gamepadstate-dpadup-6e5f052ef9"></a><code>script.value-field.GamepadState.DpadUp</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.East</code> | <a id="symbol-script-value-field-gamepadstate-east-e58b12dd8a"></a><code>script.value-field.GamepadState.East</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.LeftShoulder</code> | <a id="symbol-script-value-field-gamepadstate-leftshoulder-ad7044f08e"></a><code>script.value-field.GamepadState.LeftShoulder</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.LeftStickButton</code> | <a id="symbol-script-value-field-gamepadstate-leftstickbutton-f4451cf0c5"></a><code>script.value-field.GamepadState.LeftStickButton</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>float32 GamepadState.LeftStickX</code> | <a id="symbol-script-value-field-gamepadstate-leftstickx-c5f8bb5984"></a><code>script.value-field.GamepadState.LeftStickX</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>float32 GamepadState.LeftStickY</code> | <a id="symbol-script-value-field-gamepadstate-leftsticky-83e0e1f413"></a><code>script.value-field.GamepadState.LeftStickY</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>float32 GamepadState.LeftTrigger</code> | <a id="symbol-script-value-field-gamepadstate-lefttrigger-b39d8ebdd3"></a><code>script.value-field.GamepadState.LeftTrigger</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.North</code> | <a id="symbol-script-value-field-gamepadstate-north-5b247c8074"></a><code>script.value-field.GamepadState.North</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.Reserved</code> | <a id="symbol-script-value-field-gamepadstate-reserved-3fddb0725f"></a><code>script.value-field.GamepadState.Reserved</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.RightShoulder</code> | <a id="symbol-script-value-field-gamepadstate-rightshoulder-ada1e418b6"></a><code>script.value-field.GamepadState.RightShoulder</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.RightStickButton</code> | <a id="symbol-script-value-field-gamepadstate-rightstickbutton-ac0d8462a0"></a><code>script.value-field.GamepadState.RightStickButton</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>float32 GamepadState.RightStickX</code> | <a id="symbol-script-value-field-gamepadstate-rightstickx-3c48e9f2a4"></a><code>script.value-field.GamepadState.RightStickX</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>float32 GamepadState.RightStickY</code> | <a id="symbol-script-value-field-gamepadstate-rightsticky-a3b26c156a"></a><code>script.value-field.GamepadState.RightStickY</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>float32 GamepadState.RightTrigger</code> | <a id="symbol-script-value-field-gamepadstate-righttrigger-e290c8ef98"></a><code>script.value-field.GamepadState.RightTrigger</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.South</code> | <a id="symbol-script-value-field-gamepadstate-south-04c21dd00b"></a><code>script.value-field.GamepadState.South</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.Start</code> | <a id="symbol-script-value-field-gamepadstate-start-ab1132cdc9"></a><code>script.value-field.GamepadState.Start</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |
| <code>bool GamepadState.West</code> | <a id="symbol-script-value-field-gamepadstate-west-495d96d983"></a><code>script.value-field.GamepadState.West</code> | value | <code>internal</code> (default) | [Source/Frontend/Application.h:174](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.h#L174) | - |

<a id="symbol-script-value-type-languagename-65ead35f2b"></a>
### <code>LanguageName</code>

<code>value type LanguageName</code>  
Symbol ID: <code>script.value-type.LanguageName</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>hstring</code>, <code>-</code>, <code>Name</code>  
Source: [Source/Common/TextPack.h:47](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L47)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>hstring LanguageName.Name</code> | <a id="symbol-script-value-field-languagename-name-6a57de92a1"></a><code>script.value-field.LanguageName.Name</code> | value | <code>internal</code> (default) | [Source/Common/TextPack.h:47](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L47) | - |

<a id="symbol-script-value-type-textformat-fdc38b051d"></a>
### <code>TextFormat</code>

<code>value type TextFormat</code>  
Symbol ID: <code>script.value-type.TextFormat</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>FontType</code>, <code>-</code>, <code>Font</code>, <code>+</code>, <code>FontFlag</code>, <code>-</code>, <code>Flags</code>, <code>+</code>, <code>int32</code>, <code>-</code>, <code>SkipLines</code>  
Source: [Source/Client/FontManager.h:82](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L82)

Bundled text formatting parameters: font slot, FontFlag bitmask, and skip-lines counter.<br>`SkipLines` is &quot;skip from top&quot; by default; with FontFlag::AlignBottom set it becomes &quot;skip from bottom&quot; (trailing lines).

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>FontFlag TextFormat.Flags</code> | <a id="symbol-script-value-field-textformat-flags-954ab22bf2"></a><code>script.value-field.TextFormat.Flags</code> | value | <code>internal</code> (default) | [Source/Client/FontManager.h:82](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L82) | - |
| <code>FontType TextFormat.Font</code> | <a id="symbol-script-value-field-textformat-font-54adc00ad4"></a><code>script.value-field.TextFormat.Font</code> | value | <code>internal</code> (default) | [Source/Client/FontManager.h:82](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L82) | - |
| <code>int32 TextFormat.SkipLines</code> | <a id="symbol-script-value-field-textformat-skiplines-289c6d57d9"></a><code>script.value-field.TextFormat.SkipLines</code> | value | <code>internal</code> (default) | [Source/Client/FontManager.h:82](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h#L82) | - |

<a id="symbol-script-value-type-textpackkey-fc2a0c075a"></a>
### <code>TextPackKey</code>

<code>value type TextPackKey</code>  
Symbol ID: <code>script.value-type.TextPackKey</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>TextPackName</code>, <code>-</code>, <code>Collection</code>, <code>+</code>, <code>hstring</code>, <code>-</code>, <code>Key1</code>, <code>+</code>, <code>hstring</code>, <code>-</code>, <code>Key2</code>, <code>+</code>, <code>hstring</code>, <code>-</code>, <code>Key3</code>  
Source: [Source/Common/TextPack.h:50](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L50)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>TextPackName TextPackKey.Collection</code> | <a id="symbol-script-value-field-textpackkey-collection-27cebad592"></a><code>script.value-field.TextPackKey.Collection</code> | value | <code>internal</code> (default) | [Source/Common/TextPack.h:50](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L50) | - |
| <code>hstring TextPackKey.Key1</code> | <a id="symbol-script-value-field-textpackkey-key1-3895b9dca9"></a><code>script.value-field.TextPackKey.Key1</code> | value | <code>internal</code> (default) | [Source/Common/TextPack.h:50](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L50) | - |
| <code>hstring TextPackKey.Key2</code> | <a id="symbol-script-value-field-textpackkey-key2-c17315d9a1"></a><code>script.value-field.TextPackKey.Key2</code> | value | <code>internal</code> (default) | [Source/Common/TextPack.h:50](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L50) | - |
| <code>hstring TextPackKey.Key3</code> | <a id="symbol-script-value-field-textpackkey-key3-3c6dd0de52"></a><code>script.value-field.TextPackKey.Key3</code> | value | <code>internal</code> (default) | [Source/Common/TextPack.h:50](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L50) | - |

<a id="symbol-script-value-type-textpackname-1ef38e6f7d"></a>
### <code>TextPackName</code>

<code>value type TextPackName</code>  
Symbol ID: <code>script.value-type.TextPackName</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>hstring</code>, <code>-</code>, <code>Name</code>  
Source: [Source/Common/TextPack.h:44](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L44)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>hstring TextPackName.Name</code> | <a id="symbol-script-value-field-textpackname-name-863a288988"></a><code>script.value-field.TextPackName.Name</code> | value | <code>internal</code> (default) | [Source/Common/TextPack.h:44](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h#L44) | - |

<a id="symbol-script-value-type-fpos-2cfdca1ade"></a>
### <code>fpos</code>

<code>value type fpos</code>  
Symbol ID: <code>script.value-type.fpos</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>float32</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>y</code>  
Source: [Source/Essentials/ExtendedTypes.h:642](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L642)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>float32 fpos.x</code> | <a id="symbol-script-value-field-fpos-x-b9719527a3"></a><code>script.value-field.fpos.x</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:642](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L642) | - |
| <code>float32 fpos.y</code> | <a id="symbol-script-value-field-fpos-y-85cf9b11bb"></a><code>script.value-field.fpos.y</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:642](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L642) | - |

<a id="symbol-script-value-type-frect-ca1327a5a5"></a>
### <code>frect</code>

<code>value type frect</code>  
Symbol ID: <code>script.value-type.frect</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>float32</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>y</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>width</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>height</code>  
Source: [Source/Essentials/ExtendedTypes.h:654](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L654)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>float32 frect.height</code> | <a id="symbol-script-value-field-frect-height-6fd60f81c3"></a><code>script.value-field.frect.height</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:654](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L654) | - |
| <code>float32 frect.width</code> | <a id="symbol-script-value-field-frect-width-57519673f5"></a><code>script.value-field.frect.width</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:654](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L654) | - |
| <code>float32 frect.x</code> | <a id="symbol-script-value-field-frect-x-f8ffdbbc41"></a><code>script.value-field.frect.x</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:654](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L654) | - |
| <code>float32 frect.y</code> | <a id="symbol-script-value-field-frect-y-4ce03e890a"></a><code>script.value-field.frect.y</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:654](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L654) | - |

<a id="symbol-script-value-type-fsize-2a9bf5108c"></a>
### <code>fsize</code>

<code>value type fsize</code>  
Symbol ID: <code>script.value-type.fsize</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>float32</code>, <code>-</code>, <code>width</code>, <code>+</code>, <code>float32</code>, <code>-</code>, <code>height</code>  
Source: [Source/Essentials/ExtendedTypes.h:648](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L648)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>float32 fsize.height</code> | <a id="symbol-script-value-field-fsize-height-c9d778b634"></a><code>script.value-field.fsize.height</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:648](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L648) | - |
| <code>float32 fsize.width</code> | <a id="symbol-script-value-field-fsize-width-93eba00341"></a><code>script.value-field.fsize.width</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:648](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L648) | - |

<a id="symbol-script-value-type-hdir-f937e5a509"></a>
### <code>hdir</code>

<code>value type hdir</code>  
Symbol ID: <code>script.value-type.hdir</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int8</code>, <code>-</code>, <code>value</code>  
Source: [Source/Common/Geometry.h:105](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L105)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int8 hdir.value</code> | <a id="symbol-script-value-field-hdir-value-b0f2e80104"></a><code>script.value-field.hdir.value</code> | value | <code>internal</code> (default) | [Source/Common/Geometry.h:105](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L105) | - |

<a id="symbol-script-value-type-ident-a6c4623341"></a>
### <code>ident</code>

<code>value type ident</code>  
Symbol ID: <code>script.value-type.ident</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int64</code>, <code>-</code>, <code>value</code>  
Source: [Source/Common/Common.h:96](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L96)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int64 ident.value</code> | <a id="symbol-script-value-field-ident-value-d818ed0ec5"></a><code>script.value-field.ident.value</code> | value | <code>internal</code> (default) | [Source/Common/Common.h:96](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L96) | - |

<a id="symbol-script-value-type-ipos-ebbc2642f2"></a>
### <code>ipos</code>

<code>value type ipos</code>  
Symbol ID: <code>script.value-type.ipos</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int32</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>int32</code>, <code>-</code>, <code>y</code>  
Source: [Source/Essentials/ExtendedTypes.h:621](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L621)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int32 ipos.x</code> | <a id="symbol-script-value-field-ipos-x-48d3a91319"></a><code>script.value-field.ipos.x</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:621](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L621) | - |
| <code>int32 ipos.y</code> | <a id="symbol-script-value-field-ipos-y-76b7d3c644"></a><code>script.value-field.ipos.y</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:621](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L621) | - |

<a id="symbol-script-value-type-ipos16-b12a2e0aa1"></a>
### <code>ipos16</code>

<code>value type ipos16</code>  
Symbol ID: <code>script.value-type.ipos16</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int16</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>int16</code>, <code>-</code>, <code>y</code>  
Source: [Source/Essentials/ExtendedTypes.h:615](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L615)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int16 ipos16.x</code> | <a id="symbol-script-value-field-ipos16-x-21b67af083"></a><code>script.value-field.ipos16.x</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:615](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L615) | - |
| <code>int16 ipos16.y</code> | <a id="symbol-script-value-field-ipos16-y-52397bd551"></a><code>script.value-field.ipos16.y</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:615](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L615) | - |

<a id="symbol-script-value-type-ipos8-14d2dd6ff2"></a>
### <code>ipos8</code>

<code>value type ipos8</code>  
Symbol ID: <code>script.value-type.ipos8</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int8</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>int8</code>, <code>-</code>, <code>y</code>  
Source: [Source/Essentials/ExtendedTypes.h:609](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L609)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int8 ipos8.x</code> | <a id="symbol-script-value-field-ipos8-x-86abe7ac1d"></a><code>script.value-field.ipos8.x</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:609](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L609) | - |
| <code>int8 ipos8.y</code> | <a id="symbol-script-value-field-ipos8-y-460aed53e0"></a><code>script.value-field.ipos8.y</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:609](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L609) | - |

<a id="symbol-script-value-type-irect-8a196ec2be"></a>
### <code>irect</code>

<code>value type irect</code>  
Symbol ID: <code>script.value-type.irect</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int32</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>int32</code>, <code>-</code>, <code>y</code>, <code>+</code>, <code>int32</code>, <code>-</code>, <code>width</code>, <code>+</code>, <code>int32</code>, <code>-</code>, <code>height</code>  
Source: [Source/Essentials/ExtendedTypes.h:635](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L635)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int32 irect.height</code> | <a id="symbol-script-value-field-irect-height-b8f4d9c46f"></a><code>script.value-field.irect.height</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:635](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L635) | - |
| <code>int32 irect.width</code> | <a id="symbol-script-value-field-irect-width-a5e33a0b05"></a><code>script.value-field.irect.width</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:635](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L635) | - |
| <code>int32 irect.x</code> | <a id="symbol-script-value-field-irect-x-6c199f0dc8"></a><code>script.value-field.irect.x</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:635](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L635) | - |
| <code>int32 irect.y</code> | <a id="symbol-script-value-field-irect-y-082cd73288"></a><code>script.value-field.irect.y</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:635](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L635) | - |

<a id="symbol-script-value-type-isize-49ec1327be"></a>
### <code>isize</code>

<code>value type isize</code>  
Symbol ID: <code>script.value-type.isize</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int32</code>, <code>-</code>, <code>width</code>, <code>+</code>, <code>int32</code>, <code>-</code>, <code>height</code>  
Source: [Source/Essentials/ExtendedTypes.h:628](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L628)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int32 isize.height</code> | <a id="symbol-script-value-field-isize-height-9bb6c7f07e"></a><code>script.value-field.isize.height</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:628](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L628) | - |
| <code>int32 isize.width</code> | <a id="symbol-script-value-field-isize-width-63ff66599e"></a><code>script.value-field.isize.width</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:628](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L628) | - |

<a id="symbol-script-value-type-mdir-a1ff0aef65"></a>
### <code>mdir</code>

<code>value type mdir</code>  
Symbol ID: <code>script.value-type.mdir</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int16</code>, <code>-</code>, <code>angle</code>  
Source: [Source/Common/Geometry.h:156](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L156)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int16 mdir.angle</code> | <a id="symbol-script-value-field-mdir-angle-2e9ac28366"></a><code>script.value-field.mdir.angle</code> | value | <code>internal</code> (default) | [Source/Common/Geometry.h:156](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L156) | - |

<a id="symbol-script-value-type-mpos-77e467844b"></a>
### <code>mpos</code>

<code>value type mpos</code>  
Symbol ID: <code>script.value-type.mpos</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int16</code>, <code>-</code>, <code>x</code>, <code>+</code>, <code>int16</code>, <code>-</code>, <code>y</code>  
Source: [Source/Common/Geometry.h:40](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L40)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int16 mpos.x</code> | <a id="symbol-script-value-field-mpos-x-f534c77757"></a><code>script.value-field.mpos.x</code> | value | <code>internal</code> (default) | [Source/Common/Geometry.h:40](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L40) | - |
| <code>int16 mpos.y</code> | <a id="symbol-script-value-field-mpos-y-69975d44de"></a><code>script.value-field.mpos.y</code> | value | <code>internal</code> (default) | [Source/Common/Geometry.h:40](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L40) | - |

<a id="symbol-script-value-type-msize-51ed53f341"></a>
### <code>msize</code>

<code>value type msize</code>  
Symbol ID: <code>script.value-type.msize</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int16</code>, <code>-</code>, <code>width</code>, <code>+</code>, <code>int16</code>, <code>-</code>, <code>height</code>  
Source: [Source/Common/Geometry.h:54](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L54)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int16 msize.height</code> | <a id="symbol-script-value-field-msize-height-2a676e6a19"></a><code>script.value-field.msize.height</code> | value | <code>internal</code> (default) | [Source/Common/Geometry.h:54](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L54) | - |
| <code>int16 msize.width</code> | <a id="symbol-script-value-field-msize-width-8de54fa17a"></a><code>script.value-field.msize.width</code> | value | <code>internal</code> (default) | [Source/Common/Geometry.h:54](https://github.com/cvet/fonline/blob/master/Source/Common/Geometry.h#L54) | - |

<a id="symbol-script-value-type-nanotime-b905d77db5"></a>
### <code>nanotime</code>

<code>value type nanotime</code>  
Symbol ID: <code>script.value-type.nanotime</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int64</code>, <code>-</code>, <code>value</code>  
Source: [Source/Essentials/TimeRelated.h:135](https://github.com/cvet/fonline/blob/master/Source/Essentials/TimeRelated.h#L135)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int64 nanotime.value</code> | <a id="symbol-script-value-field-nanotime-value-480df27610"></a><code>script.value-field.nanotime.value</code> | value | <code>internal</code> (default) | [Source/Essentials/TimeRelated.h:135](https://github.com/cvet/fonline/blob/master/Source/Essentials/TimeRelated.h#L135) | - |

<a id="symbol-script-value-type-synctime-1c27bdefae"></a>
### <code>synctime</code>

<code>value type synctime</code>  
Symbol ID: <code>script.value-type.synctime</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int64</code>, <code>-</code>, <code>value</code>  
Source: [Source/Essentials/TimeRelated.h:205](https://github.com/cvet/fonline/blob/master/Source/Essentials/TimeRelated.h#L205)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int64 synctime.value</code> | <a id="symbol-script-value-field-synctime-value-2c3d9455cd"></a><code>script.value-field.synctime.value</code> | value | <code>internal</code> (default) | [Source/Essentials/TimeRelated.h:205](https://github.com/cvet/fonline/blob/master/Source/Essentials/TimeRelated.h#L205) | - |

<a id="symbol-script-value-type-timespan-08db5edc8b"></a>
### <code>timespan</code>

<code>value type timespan</code>  
Symbol ID: <code>script.value-type.timespan</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>int64</code>, <code>-</code>, <code>value</code>  
Source: [Source/Essentials/TimeRelated.h:47](https://github.com/cvet/fonline/blob/master/Source/Essentials/TimeRelated.h#L47)

-

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>int64 timespan.value</code> | <a id="symbol-script-value-field-timespan-value-005d52b5ab"></a><code>script.value-field.timespan.value</code> | value | <code>internal</code> (default) | [Source/Essentials/TimeRelated.h:47](https://github.com/cvet/fonline/blob/master/Source/Essentials/TimeRelated.h#L47) | - |

<a id="symbol-script-value-type-ucolor-92d4b0314f"></a>
### <code>ucolor</code>

<code>value type ucolor</code>  
Symbol ID: <code>script.value-type.ucolor</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>Layout</code>, <code>=</code>, <code>uint32</code>, <code>-</code>, <code>value</code>  
Source: [Source/Essentials/ExtendedTypes.h:67](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L67)

Color type

| Field | Symbol ID | Value contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <code>uint32 ucolor.value</code> | <a id="symbol-script-value-field-ucolor-value-a2f3af8a49"></a><code>script.value-field.ucolor.value</code> | value | <code>internal</code> (default) | [Source/Essentials/ExtendedTypes.h:67](https://github.com/cvet/fonline/blob/master/Source/Essentials/ExtendedTypes.h#L67) | - |

## Reference Types

<a id="symbol-script-ref-type-client-foglayer-ca9e4bd2e0"></a>
### <code>FogLayer</code>

<code>ref type FogLayer</code>  
Symbol ID: <code>script.ref-type.client.FogLayer</code>  
Runtime: client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>RefCounted</code>, <code>Export</code>, <code>=</code>, <code>Enabled</code>, <code>,</code>, <code>Distance</code>, <code>,</code>, <code>Radius</code>, <code>,</code>, <code>ExtraLength</code>, <code>,</code>, <code>TransitionDuration</code>, <code>,</code>, <code>OvalRoundness</code>, <code>,</code>, <code>EdgeNoise</code>, <code>,</code>, <code>Depth</code>, <code>,</code>, <code>ClearRadius</code>, <code>,</code>, <code>TintColor</code>, <code>,</code>, <code>OverlayColor</code>, <code>,</code>, <code>CenterColor</code>, <code>,</code>, <code>Traced</code>, <code>,</code>, <code>CheckShootBlocks</code>, <code>,</code>, <code>OriginHex</code>, <code>,</code>, <code>Disposed</code>, <code>,</code>, <code>Dispose</code>  
Source: [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90)

-

| Kind | Member | Symbol ID | Member contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| ref-field | <code>ucolor FogLayer.CenterColor</code> | <a id="symbol-script-ref-field-client-foglayer-centercolor-66aec91629"></a><code>script.ref-field.client.FogLayer.CenterColor</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>bool FogLayer.CheckShootBlocks</code> | <a id="symbol-script-ref-field-client-foglayer-checkshootblocks-e012c92996"></a><code>script.ref-field.client.FogLayer.CheckShootBlocks</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>float32 FogLayer.ClearRadius</code> | <a id="symbol-script-ref-field-client-foglayer-clearradius-0f48f732b9"></a><code>script.ref-field.client.FogLayer.ClearRadius</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>float32 FogLayer.Depth</code> | <a id="symbol-script-ref-field-client-foglayer-depth-a0585e97f9"></a><code>script.ref-field.client.FogLayer.Depth</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-method | <code>void FogLayer.Dispose()</code> | <a id="symbol-script-ref-method-client-foglayer-dispose-8c3f918bb0"></a><code>script.ref-method.client.FogLayer.Dispose</code> | callable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>bool FogLayer.Disposed</code> | <a id="symbol-script-ref-field-client-foglayer-disposed-b2c82deb26"></a><code>script.ref-field.client.FogLayer.Disposed</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>int32 FogLayer.Distance</code> | <a id="symbol-script-ref-field-client-foglayer-distance-9fee9d1665"></a><code>script.ref-field.client.FogLayer.Distance</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>float32 FogLayer.EdgeNoise</code> | <a id="symbol-script-ref-field-client-foglayer-edgenoise-fbc772aafe"></a><code>script.ref-field.client.FogLayer.EdgeNoise</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>bool FogLayer.Enabled</code> | <a id="symbol-script-ref-field-client-foglayer-enabled-2e4e62bb01"></a><code>script.ref-field.client.FogLayer.Enabled</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>int32 FogLayer.ExtraLength</code> | <a id="symbol-script-ref-field-client-foglayer-extralength-3803cc526c"></a><code>script.ref-field.client.FogLayer.ExtraLength</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>mpos FogLayer.OriginHex</code> | <a id="symbol-script-ref-field-client-foglayer-originhex-6513d5fdfc"></a><code>script.ref-field.client.FogLayer.OriginHex</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>float32 FogLayer.OvalRoundness</code> | <a id="symbol-script-ref-field-client-foglayer-ovalroundness-dc3f725a09"></a><code>script.ref-field.client.FogLayer.OvalRoundness</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>ucolor FogLayer.OverlayColor</code> | <a id="symbol-script-ref-field-client-foglayer-overlaycolor-0fdfc03c3a"></a><code>script.ref-field.client.FogLayer.OverlayColor</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>int32 FogLayer.Radius</code> | <a id="symbol-script-ref-field-client-foglayer-radius-eb42a86ed9"></a><code>script.ref-field.client.FogLayer.Radius</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>ucolor FogLayer.TintColor</code> | <a id="symbol-script-ref-field-client-foglayer-tintcolor-c70cc3b886"></a><code>script.ref-field.client.FogLayer.TintColor</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>bool FogLayer.Traced</code> | <a id="symbol-script-ref-field-client-foglayer-traced-326294b00d"></a><code>script.ref-field.client.FogLayer.Traced</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |
| ref-field | <code>int32 FogLayer.TransitionDuration</code> | <a id="symbol-script-ref-field-client-foglayer-transitionduration-15bf608354"></a><code>script.ref-field.client.FogLayer.TransitionDuration</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:90](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L90) | - |

<a id="symbol-script-ref-type-client-mapspriteholder-413b842993"></a>
### <code>MapSpriteHolder</code>

<code>ref type MapSpriteHolder</code>  
Symbol ID: <code>script.ref-type.client.MapSpriteHolder</code>  
Runtime: client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>RefCounted</code>, <code>HasFactory</code>, <code>Export</code>, <code>=</code>, <code>Valid</code>, <code>,</code>, <code>SprId</code>, <code>,</code>, <code>Hex</code>, <code>,</code>, <code>ProtoId</code>, <code>,</code>, <code>Offset</code>, <code>,</code>, <code>IsFlat</code>, <code>,</code>, <code>NoLight</code>, <code>,</code>, <code>DrawOrder</code>, <code>,</code>, <code>DrawOrderHyOffset</code>, <code>,</code>, <code>Corner</code>, <code>,</code>, <code>DisableEgg</code>, <code>,</code>, <code>Color</code>, <code>,</code>, <code>IsTweakOffs</code>, <code>,</code>, <code>TweakOffset</code>, <code>,</code>, <code>IsTweakAlpha</code>, <code>,</code>, <code>TweakAlpha</code>, <code>,</code>, <code>Angle</code>, <code>,</code>, <code>MapProjected</code>, <code>,</code>, <code>StopDraw</code>  
Source: [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216)

-

| Kind | Member | Symbol ID | Member contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| ref-field | <code>int16 MapSpriteHolder.Angle</code> | <a id="symbol-script-ref-field-client-mapspriteholder-angle-80d7024854"></a><code>script.ref-field.client.MapSpriteHolder.Angle</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>ucolor MapSpriteHolder.Color</code> | <a id="symbol-script-ref-field-client-mapspriteholder-color-66dfe0c9ee"></a><code>script.ref-field.client.MapSpriteHolder.Color</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>CornerType MapSpriteHolder.Corner</code> | <a id="symbol-script-ref-field-client-mapspriteholder-corner-0c782b7813"></a><code>script.ref-field.client.MapSpriteHolder.Corner</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.DisableEgg</code> | <a id="symbol-script-ref-field-client-mapspriteholder-disableegg-ecff4521f9"></a><code>script.ref-field.client.MapSpriteHolder.DisableEgg</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>DrawOrderType MapSpriteHolder.DrawOrder</code> | <a id="symbol-script-ref-field-client-mapspriteholder-draworder-199f5cc044"></a><code>script.ref-field.client.MapSpriteHolder.DrawOrder</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>int32 MapSpriteHolder.DrawOrderHyOffset</code> | <a id="symbol-script-ref-field-client-mapspriteholder-draworderhyoffset-e1bf12c154"></a><code>script.ref-field.client.MapSpriteHolder.DrawOrderHyOffset</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>mpos MapSpriteHolder.Hex</code> | <a id="symbol-script-ref-field-client-mapspriteholder-hex-e458bb5b43"></a><code>script.ref-field.client.MapSpriteHolder.Hex</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.IsFlat</code> | <a id="symbol-script-ref-field-client-mapspriteholder-isflat-9e5b2d1ea2"></a><code>script.ref-field.client.MapSpriteHolder.IsFlat</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.IsTweakAlpha</code> | <a id="symbol-script-ref-field-client-mapspriteholder-istweakalpha-929ddf69a0"></a><code>script.ref-field.client.MapSpriteHolder.IsTweakAlpha</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.IsTweakOffs</code> | <a id="symbol-script-ref-field-client-mapspriteholder-istweakoffs-c8dd54d25a"></a><code>script.ref-field.client.MapSpriteHolder.IsTweakOffs</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.MapProjected</code> | <a id="symbol-script-ref-field-client-mapspriteholder-mapprojected-a9e4b2aa27"></a><code>script.ref-field.client.MapSpriteHolder.MapProjected</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.NoLight</code> | <a id="symbol-script-ref-field-client-mapspriteholder-nolight-779a8d9ff0"></a><code>script.ref-field.client.MapSpriteHolder.NoLight</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>ipos MapSpriteHolder.Offset</code> | <a id="symbol-script-ref-field-client-mapspriteholder-offset-8737961cee"></a><code>script.ref-field.client.MapSpriteHolder.Offset</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>hstring MapSpriteHolder.ProtoId</code> | <a id="symbol-script-ref-field-client-mapspriteholder-protoid-af6c03f2b5"></a><code>script.ref-field.client.MapSpriteHolder.ProtoId</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>uint32 MapSpriteHolder.SprId</code> | <a id="symbol-script-ref-field-client-mapspriteholder-sprid-e82be29c42"></a><code>script.ref-field.client.MapSpriteHolder.SprId</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-method | <code>void MapSpriteHolder.StopDraw()</code> | <a id="symbol-script-ref-method-client-mapspriteholder-stopdraw-96babdfb93"></a><code>script.ref-method.client.MapSpriteHolder.StopDraw</code> | callable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>uint8 MapSpriteHolder.TweakAlpha</code> | <a id="symbol-script-ref-field-client-mapspriteholder-tweakalpha-2b1224cd98"></a><code>script.ref-field.client.MapSpriteHolder.TweakAlpha</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>ipos MapSpriteHolder.TweakOffset</code> | <a id="symbol-script-ref-field-client-mapspriteholder-tweakoffset-1bad3c756a"></a><code>script.ref-field.client.MapSpriteHolder.TweakOffset</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |
| ref-field | <code>bool MapSpriteHolder.Valid</code> | <a id="symbol-script-ref-field-client-mapspriteholder-valid-5bf8f82dde"></a><code>script.ref-field.client.MapSpriteHolder.Valid</code> | mutable | <code>internal</code> (default) | [Source/Client/MapSprite.h:216](https://github.com/cvet/fonline/blob/master/Source/Client/MapSprite.h#L216) | - |

<a id="symbol-script-ref-type-common-movingcontext-263cd8f02b"></a>
### <code>MovingContext</code>

<code>ref type MovingContext</code>  
Symbol ID: <code>script.ref-type.common.MovingContext</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>RefCounted</code>, <code>Export</code>, <code>=</code>, <code>GetSpeed</code>, <code>,</code>, <code>GetStartHex</code>, <code>,</code>, <code>GetEndHex</code>, <code>,</code>, <code>GetStartHexOffset</code>, <code>,</code>, <code>GetEndHexOffset</code>, <code>,</code>, <code>GetPreBlockHex</code>, <code>,</code>, <code>GetBlockHex</code>, <code>,</code>, <code>GetWholeTime</code>, <code>,</code>, <code>GetWholeDist</code>, <code>,</code>, <code>GetElapsedTime</code>, <code>,</code>, <code>IsCompleted</code>, <code>,</code>, <code>GetCompleteReason</code>, <code>,</code>, <code>EvaluateProjectedHex</code>, <code>,</code>, <code>EvaluateNearestPathHex</code>, <code>,</code>, <code>EvaluatePathHexes</code>  
Source: [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86)

-

| Kind | Member | Symbol ID | Member contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| ref-method | <code>mpos MovingContext.EvaluateNearestPathHex(mpos current_hex, mpos from_hex, mpos fallback_hex)</code> | <a id="symbol-script-ref-method-common-movingcontext-evaluatenearestpathhex-83abbdc13b"></a><code>script.ref-method.common.MovingContext.EvaluateNearestPathHex</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>mpos[] MovingContext.EvaluatePathHexes(mpos current_hex)</code> | <a id="symbol-script-ref-method-common-movingcontext-evaluatepathhexes-ed3aae2d0a"></a><code>script.ref-method.common.MovingContext.EvaluatePathHexes</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>mpos MovingContext.EvaluateProjectedHex(float32 look_ahead_ms)</code> | <a id="symbol-script-ref-method-common-movingcontext-evaluateprojectedhex-f39b3e1d91"></a><code>script.ref-method.common.MovingContext.EvaluateProjectedHex</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>mpos MovingContext.GetBlockHex()</code> | <a id="symbol-script-ref-method-common-movingcontext-getblockhex-82b7db854f"></a><code>script.ref-method.common.MovingContext.GetBlockHex</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>MovingState MovingContext.GetCompleteReason()</code> | <a id="symbol-script-ref-method-common-movingcontext-getcompletereason-bd290bf07d"></a><code>script.ref-method.common.MovingContext.GetCompleteReason</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>float32 MovingContext.GetElapsedTime()</code> | <a id="symbol-script-ref-method-common-movingcontext-getelapsedtime-fc308a8194"></a><code>script.ref-method.common.MovingContext.GetElapsedTime</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>mpos MovingContext.GetEndHex()</code> | <a id="symbol-script-ref-method-common-movingcontext-getendhex-c868368d80"></a><code>script.ref-method.common.MovingContext.GetEndHex</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>ipos16 MovingContext.GetEndHexOffset()</code> | <a id="symbol-script-ref-method-common-movingcontext-getendhexoffset-34bd706a1a"></a><code>script.ref-method.common.MovingContext.GetEndHexOffset</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>mpos MovingContext.GetPreBlockHex()</code> | <a id="symbol-script-ref-method-common-movingcontext-getpreblockhex-32b85c82ed"></a><code>script.ref-method.common.MovingContext.GetPreBlockHex</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>uint16 MovingContext.GetSpeed()</code> | <a id="symbol-script-ref-method-common-movingcontext-getspeed-b5f0795d99"></a><code>script.ref-method.common.MovingContext.GetSpeed</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>mpos MovingContext.GetStartHex()</code> | <a id="symbol-script-ref-method-common-movingcontext-getstarthex-856450ffaa"></a><code>script.ref-method.common.MovingContext.GetStartHex</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>ipos16 MovingContext.GetStartHexOffset()</code> | <a id="symbol-script-ref-method-common-movingcontext-getstarthexoffset-608e0f29ea"></a><code>script.ref-method.common.MovingContext.GetStartHexOffset</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>float32 MovingContext.GetWholeDist()</code> | <a id="symbol-script-ref-method-common-movingcontext-getwholedist-87c5022fb4"></a><code>script.ref-method.common.MovingContext.GetWholeDist</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>float32 MovingContext.GetWholeTime()</code> | <a id="symbol-script-ref-method-common-movingcontext-getwholetime-85ed9aede4"></a><code>script.ref-method.common.MovingContext.GetWholeTime</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |
| ref-method | <code>bool MovingContext.IsCompleted()</code> | <a id="symbol-script-ref-method-common-movingcontext-iscompleted-cfbdf78e77"></a><code>script.ref-method.common.MovingContext.IsCompleted</code> | callable | <code>internal</code> (default) | [Source/Common/Movement.h:86](https://github.com/cvet/fonline/blob/master/Source/Common/Movement.h#L86) | - |

<a id="symbol-script-ref-type-client-spritepattern-338f66b808"></a>
### <code>SpritePattern</code>

<code>ref type SpritePattern</code>  
Symbol ID: <code>script.ref-type.client.SpritePattern</code>  
Runtime: client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>RefCounted</code>, <code>Export</code>, <code>=</code>, <code>Finished</code>, <code>,</code>, <code>EveryHex</code>, <code>,</code>, <code>InteractWithRoof</code>, <code>,</code>, <code>CheckTileProperty</code>, <code>,</code>, <code>TileProperty</code>, <code>,</code>, <code>ExpectedTilePropertyValue</code>, <code>,</code>, <code>Finish</code>  
Source: [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75)

-

| Kind | Member | Symbol ID | Member contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| ref-field | <code>bool SpritePattern.CheckTileProperty</code> | <a id="symbol-script-ref-field-client-spritepattern-checktileproperty-8d1164a505"></a><code>script.ref-field.client.SpritePattern.CheckTileProperty</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |
| ref-field | <code>ipos SpritePattern.EveryHex</code> | <a id="symbol-script-ref-field-client-spritepattern-everyhex-f07074e9c8"></a><code>script.ref-field.client.SpritePattern.EveryHex</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |
| ref-field | <code>int32 SpritePattern.ExpectedTilePropertyValue</code> | <a id="symbol-script-ref-field-client-spritepattern-expectedtilepropertyvalue-ebb444a6d9"></a><code>script.ref-field.client.SpritePattern.ExpectedTilePropertyValue</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |
| ref-method | <code>void SpritePattern.Finish()</code> | <a id="symbol-script-ref-method-client-spritepattern-finish-7ab7254fd1"></a><code>script.ref-method.client.SpritePattern.Finish</code> | callable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |
| ref-field | <code>bool SpritePattern.Finished</code> | <a id="symbol-script-ref-field-client-spritepattern-finished-2c8e196fef"></a><code>script.ref-field.client.SpritePattern.Finished</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |
| ref-field | <code>bool SpritePattern.InteractWithRoof</code> | <a id="symbol-script-ref-field-client-spritepattern-interactwithroof-7b8d8d5f3e"></a><code>script.ref-field.client.SpritePattern.InteractWithRoof</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |
| ref-field | <code>ItemProperty SpritePattern.TileProperty</code> | <a id="symbol-script-ref-field-client-spritepattern-tileproperty-3b60dd55dd"></a><code>script.ref-field.client.SpritePattern.TileProperty</code> | mutable | <code>internal</code> (default) | [Source/Client/MapView.h:75](https://github.com/cvet/fonline/blob/master/Source/Client/MapView.h#L75) | - |

<a id="symbol-script-ref-type-common-timeeventcontext-96e913190c"></a>
### <code>TimeEventContext</code>

<code>ref type TimeEventContext</code>  
Symbol ID: <code>script.ref-type.common.TimeEventContext</code>  
Runtime: server, client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>RefCounted</code>, <code>Export</code>, <code>=</code>, <code>GetId</code>, <code>,</code>, <code>GetData</code>, <code>,</code>, <code>GetDataArray</code>, <code>,</code>, <code>HasData</code>, <code>,</code>, <code>IsStopped</code>, <code>,</code>, <code>GetRepeat</code>, <code>,</code>, <code>Stop</code>, <code>,</code>, <code>Repeat</code>, <code>,</code>, <code>SetData</code>, <code>,</code>, <code>SetDataArray</code>  
Source: [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46)

-

| Kind | Member | Symbol ID | Member contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| ref-method | <code>any TimeEventContext.GetData()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-getdata-c1c76c4d19"></a><code>script.ref-method.common.TimeEventContext.GetData</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>any[] TimeEventContext.GetDataArray()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-getdataarray-e96b45a02f"></a><code>script.ref-method.common.TimeEventContext.GetDataArray</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>uint32 TimeEventContext.GetId()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-getid-553f36df6d"></a><code>script.ref-method.common.TimeEventContext.GetId</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>timespan TimeEventContext.GetRepeat()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-getrepeat-00ae66d751"></a><code>script.ref-method.common.TimeEventContext.GetRepeat</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>bool TimeEventContext.HasData()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-hasdata-1e4b00b774"></a><code>script.ref-method.common.TimeEventContext.HasData</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>bool TimeEventContext.IsStopped()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-isstopped-809e3af8ff"></a><code>script.ref-method.common.TimeEventContext.IsStopped</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>void TimeEventContext.Repeat(timespan repeat)</code> | <a id="symbol-script-ref-method-common-timeeventcontext-repeat-2a86a70e3a"></a><code>script.ref-method.common.TimeEventContext.Repeat</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>void TimeEventContext.SetData(any data)</code> | <a id="symbol-script-ref-method-common-timeeventcontext-setdata-9b2ba3482f"></a><code>script.ref-method.common.TimeEventContext.SetData</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>void TimeEventContext.SetDataArray(any[] data)</code> | <a id="symbol-script-ref-method-common-timeeventcontext-setdataarray-503546a84d"></a><code>script.ref-method.common.TimeEventContext.SetDataArray</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |
| ref-method | <code>void TimeEventContext.Stop()</code> | <a id="symbol-script-ref-method-common-timeeventcontext-stop-fda24a47de"></a><code>script.ref-method.common.TimeEventContext.Stop</code> | callable | <code>internal</code> (default) | [Source/Common/TimeEvents.h:46](https://github.com/cvet/fonline/blob/master/Source/Common/TimeEvents.h#L46) | - |

<a id="symbol-script-ref-type-client-videoplayback-7102c807e3"></a>
### <code>VideoPlayback</code>

<code>ref type VideoPlayback</code>  
Symbol ID: <code>script.ref-type.client.VideoPlayback</code>  
Runtime: client, mapper  
Contract: <code>internal</code> (default)  
Flags: <code>RefCounted</code>, <code>Export</code>, <code>=</code>, <code>Stopped</code>  
Source: [Source/Client/Client.h:76](https://github.com/cvet/fonline/blob/master/Source/Client/Client.h#L76)

-

| Kind | Member | Symbol ID | Member contract | API contract | Source | Description |
| --- | --- | --- | --- | --- | --- | --- |
| ref-field | <code>bool VideoPlayback.Stopped</code> | <a id="symbol-script-ref-field-client-videoplayback-stopped-941befb4f8"></a><code>script.ref-field.client.VideoPlayback.Stopped</code> | mutable | <code>internal</code> (default) | [Source/Client/Client.h:76](https://github.com/cvet/fonline/blob/master/Source/Client/Client.h#L76) | - |
