# FOnline Engine Mapper Script API

> Document under development, do not rely on this API before the global refactoring complete.  
> Estimated finishing date is middle of this year.

## Table of Content

- [General information](#general-information)
- [Global methods](#global-methods)
- [Global properties](#global-properties)
- [Entities](#entities)
  * [Item properties](#item-properties)
  * [Critter properties](#critter-properties)
  * [Map properties](#map-properties)
  * [Location properties](#location-properties)
- [Events](#events)
- [Settings](#settings)
- [Enums](#enums)
  * [MessageBoxTextType](#messageboxtexttype)
  * [MouseButton](#mousebutton)
  * [KeyCode](#keycode)
  * [CornerType](#cornertype)
  * [MovingState](#movingstate)
  * [CritterCondition](#crittercondition)
  * [ItemOwnership](#itemownership)
  * [Anim1](#anim1)
  * [CursorType](#cursortype)
- [Content](#content)
  * [Item pids](#item-pids)
  * [Critter pids](#critter-pids)
  * [Map pids](#map-pids)
  * [Location pids](#location-pids)

## General infomation

This document automatically generated from engine provided script API so any change in API will reflect to this document and all scripting layers (C++, C#, AngelScript).  
You can easily contribute to this API using provided by engine functionality.  
...write about FO_API* macro usage...

## Global methods

* void Assert(bool condition)
* void ThrowException(string message)
* int Random(int minValue, int maxValue)
* void Log(string text)
* bool StrToInt(string text, ref int result)
* bool StrToFloat(string text, ref float result)
* int GetHexDistance(uint16 hx1, uint16 hy1, uint16 hx2, uint16 hy2)
* uint8 GetHexDir(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy)
* uint8 GetHexDirWithOffset(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float offset)
* uint GetTick()
* hash GetStrHash(string text)
* string GetHashStr(hash h)
* uint DecodeUTF8(string text, ref uint length)
* string EncodeUTF8(uint ucs)
* void Yield(uint time)
* string SHA1(string text)
* string SHA2(string text)
* int SystemCall(string command)
* int SystemCallExt(string command, ref string output)
* void OpenLink(string link)
* Entity GetProtoItem(hash pid, int->int props)
* uint GetUnixTime()
* Item AddItem(hash pid, uint16 hx, uint16 hy)
* Critter AddCritter(hash pid, uint16 hx, uint16 hy)
* Item GetItemByHex(uint16 hx, uint16 hy)
* Item[] GetItemsByHex(uint16 hx, uint16 hy)
* Critter GetCritterByHex(uint16 hx, uint16 hy, int findType)
* Critter[] GetCrittersByHex(uint16 hx, uint16 hy, int findType)
* void MoveEntity(Entity entity, uint16 hx, uint16 hy)
* void DeleteEntity(Entity entity)
* void DeleteEntities(Entity[] entities)
* void SelectEntity(Entity entity, bool set)
* void SelectEntities(Entity[] entities, bool set)
* Entity GetSelectedEntity()
* Entity[] GetSelectedEntities()
* uint GetTilesCount(uint16 hx, uint16 hy, bool roof)
* void DeleteTile(uint16 hx, uint16 hy, bool roof, int layer)
* hash GetTileHash(uint16 hx, uint16 hy, bool roof, int layer)
* void AddTileHash(uint16 hx, uint16 hy, int ox, int oy, int layer, bool roof, hash picHash)
* string GetTileName(uint16 hx, uint16 hy, bool roof, int layer)
* void AddTileName(uint16 hx, uint16 hy, int ox, int oy, int layer, bool roof, string picName)
* Map LoadMap(string fileName)
* void UnloadMap(Map map)
* bool SaveMap(Map map, string customName)
* bool ShowMap(Map map)
* Map[] GetLoadedMaps(ref int index)
* string[] GetMapFileNames(string dir)
* void ResizeMap(uint16 width, uint16 height)
* string[] TabGetTileDirs(int tab)
* hash[] TabGetItemPids(int tab, string subTab)
* hash[] TabGetCritterPids(int tab, string subTab)
* void TabSetTileDirs(int tab, string[] dirNames, bool[] includeSubdirs)
* void TabSetItemPids(int tab, string subTab, hash[] itemPids)
* void TabSetCritterPids(int tab, string subTab, hash[] critterPids)
* void TabDelete(int tab)
* void TabSelect(int tab, string subTab, bool show)
* void TabSetName(int tab, string tabName)
* void MoveScreenToHex(uint16 hx, uint16 hy, uint speed, bool canStop)
* void MoveScreenOffset(int ox, int oy, uint speed, bool canStop)
* void MoveHexByDir(ref uint16 hx, ref uint16 hy, uint8 dir, uint steps)
* string GetIfaceIniStr(string key)
* void Message(string msg)
* void MessageMsg(int textMsg, uint strNum)
* void MapMessage(string text, uint16 hx, uint16 hy, uint ms, uint color, bool fade, int ox, int oy)
* string GetMsgStr(int textMsg, uint strNum)
* string GetMsgStrSkip(int textMsg, uint strNum, uint skipCount)
* uint GetMsgStrNumUpper(int textMsg, uint strNum)
* uint GetMsgStrNumLower(int textMsg, uint strNum)
* uint GetMsgStrCount(int textMsg, uint strNum)
* bool IsMsgStr(int textMsg, uint strNum)
* string ReplaceTextStr(string text, string replace, string str)
* string ReplaceTextInt(string text, string replace, int i)
* void GetHexInPath(uint16 fromHx, uint16 fromHy, ref uint16 toHx, ref uint16 toHy, float angle, uint dist)
* uint GetPathLengthHex(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)
* bool GetHexPos(uint16 hx, uint16 hy, ref int x, ref int y)
* bool GetMonitorHex(int x, int y, ref uint16 hx, ref uint16 hy, bool ignoreInterface)
* Entity GetMonitorObject(int x, int y, bool ignoreInterface)
* void AddDataSource(string datName)
* bool LoadFont(int fontIndex, string fontFname)
* void SetDefaultFont(int font, uint color)
* void MouseClick(int x, int y, int button)
* void KeyboardPress(uint8 key1, uint8 key2, string key1Text, string key2Text)
* void SetRainAnimation(string fallAnimName, string dropAnimName)
* void ChangeZoom(float targetZoom)
* uint LoadSprite(string sprName)
* uint LoadSpriteHash(uint nameHash)
* int GetSpriteWidth(uint sprId, int sprIndex)
* int GetSpriteHeight(uint sprId, int sprIndex)
* uint GetSpriteCount(uint sprId)
* uint GetSpriteTicks(uint sprId)
* uint GetPixelColor(uint sprId, int frameIndex, int x, int y)
* void GetTextInfo(string text, int w, int h, int font, int flags, ref int tw, ref int th, ref int lines)
* void DrawSprite(uint sprId, int frameIndex, int x, int y, uint color, bool offs)
* void DrawSpriteSize(uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom, uint color, bool offs)
* void DrawSpritePattern(uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color)
* void DrawText(string text, int x, int y, int w, int h, uint color, int font, int flags)
* void DrawPrimitive(int primitiveType, int[] data)
* void DrawMapSprite(MapSprite mapSpr)
* void DrawCritter2d(hash modelName, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)
* void DrawCritter3d(uint instance, hash modelName, uint anim1, uint anim2, int[] layers, float[] position, uint color)
* void PushDrawScissor(int x, int y, int w, int h)
* void PopDrawScissor()

## Global properties

* const uint16 YearStart
* const uint16 Year
* const uint16 Month
* const uint16 Day
* const uint16 Hour
* const uint16 Minute
* const uint16 Second
* const uint16 TimeMultiplier
* const uint LastEntityId
* const uint LastDeferredCallId
* const uint HistoryRecordsId

## Entities

### Item properties

* const ItemOwnership Accessory
* const uint MapId
* const uint16 HexX
* const uint16 HexY
* const uint CritId
* const uint8 CritSlot
* const uint ContainerId
* const uint ContainerStack
* float FlyEffectSpeed
* hash PicMap
* hash PicInv
* int16 OffsetX
* int16 OffsetY
* const bool Stackable
* bool GroundLevel
* bool Opened
* const CornerType Corner
* const uint8 Slot
* uint Weight
* uint Volume
* const bool DisableEgg
* const uint16 AnimWaitBase
* const uint16 AnimWaitRndMin
* const uint16 AnimWaitRndMax
* const uint8 AnimStay0
* const uint8 AnimStay1
* const uint8 AnimShow0
* const uint8 AnimShow1
* const uint8 AnimHide0
* const uint8 AnimHide1
* const uint8 SpriteCut
* const int8 DrawOrderOffsetHexY
* const uint8[] BlockLines
* const bool IsStatic
* bool IsScenery
* bool IsWall
* bool IsCanOpen
* bool IsScrollBlock
* bool IsHidden
* bool IsHiddenPicture
* bool IsHiddenInStatic
* bool IsFlat
* bool IsNoBlock
* bool IsShootThru
* bool IsLightThru
* bool IsAlwaysView
* bool IsBadItem
* bool IsNoHighlight
* bool IsShowAnim
* bool IsShowAnimExt
* bool IsLight
* bool IsGeck
* bool IsTrap
* bool IsTrigger
* bool IsNoLightInfluence
* bool IsGag
* bool IsColorize
* bool IsColorizeInv
* bool IsCanTalk
* bool IsRadio
* string Lexems
* int16 SortValue
* uint8 Info
* uint8 Mode
* int8 LightIntensity
* uint8 LightDistance
* uint8 LightFlags
* uint LightColor
* hash ScriptId
* uint Count
* int16 TrapValue
* uint16 RadioChannel
* uint16 RadioFlags
* uint8 RadioBroadcastSend
* uint8 RadioBroadcastRecv

### Critter properties

* hash ModelName
* uint WalkTime
* uint RunTime
* const uint Multihex
* const uint MapId
* const uint RefMapId
* const hash RefMapPid
* const uint RefLocationId
* const hash RefLocationPid
* const uint16 HexX
* const uint16 HexY
* const uint8 Dir
* const string Password
* const CritterCondition Cond
* const bool ClientToDelete
* uint16 WorldX
* uint16 WorldY
* const uint GlobalMapLeaderId
* const uint GlobalMapTripId
* const uint RefGlobalMapTripId
* const uint RefGlobalMapLeaderId
* const uint16 LastMapHexX
* const uint16 LastMapHexY
* const uint Anim1Life
* const uint Anim1Knockout
* const uint Anim1Dead
* const uint Anim2Life
* const uint Anim2Knockout
* const uint Anim2Dead
* const uint8[] GlobalMapFog
* const hash[] TE_FuncNum
* const uint[] TE_Rate
* const uint[] TE_NextTime
* const int[] TE_Identifier
* int8 Gender
* hash NpcRole
* int ReplicationTime
* uint TalkDistance
* int ScaleFactor
* int CurrentHp
* uint MaxTalkers
* hash DialogId
* string Lexems
* uint HomeMapId
* uint16 HomeHexX
* uint16 HomeHexY
* uint8 HomeDir
* uint[] KnownLocations
* uint[] ConnectionIp
* uint16[] ConnectionPort
* uint ShowCritterDist1
* uint ShowCritterDist2
* uint ShowCritterDist3
* hash ScriptId
* hash[] KnownLocProtoId
* int[] Anim3dLayer
* bool IsHide
* bool IsNoHome
* bool IsGeck
* bool IsNoUnarmed
* bool IsNoRotate
* bool IsNoTalk
* bool IsNoFlatten
* uint TimeoutBattle
* uint TimeoutTransfer
* uint TimeoutRemoveFromGame

### Map properties

* uint LoopTime1
* uint LoopTime2
* uint LoopTime3
* uint LoopTime4
* uint LoopTime5
* const string FileDir
* const uint16 Width
* const uint16 Height
* const uint16 WorkHexX
* const uint16 WorkHexY
* const uint LocId
* const uint LocMapIndex
* uint8 RainCapacity
* int CurDayTime
* hash ScriptId
* int[] DayTime
* uint8[] DayColor
* bool IsNoLogOut

### Location properties

* const hash[] MapProtos
* const hash[] MapEntrances
* const hash[] Automaps
* uint MaxPlayers
* bool AutoGarbage
* bool GeckVisible
* hash EntranceScript
* uint16 WorldX
* uint16 WorldY
* uint16 Radius
* bool Hidden
* bool ToGarbage
* uint Color
* bool IsEncounter

## Events

* ConsoleMessage(ref string text)
* MapLoad(Map map)
* MapSave(Map map)
* InspectorProperties(Entity entity, ref int[] properties)

## Settings

## Enums

### MessageBoxTextType


### MouseButton


### KeyCode


### CornerType

* NorthSouth = 0
* West = 1
* East = 2
* South = 3
* North = 4
* EastWest = 5

### MovingState

* InProgress = 0
* Success = 1
* TargetNotFound = 2
* CantMove = 3
* GagCritter = 4
* GagItem = 5
* InternalError = 6
* HexTooFar = 7
* HexBusy = 8
* HexBusyRing = 9
* Deadlock = 10
* TraceFail = 11

### CritterCondition

* Unknown = 0
* Alive = 1
* Knockout = 2
* Dead = 3

### ItemOwnership

* Nowhere = 0
* CritterInventory = 1
* MapHex = 2
* ItemContainer = 3

### Anim1

* None = 0

### CursorType

* Default = 0

## Content

### Item pids


### Critter pids


### Map pids


### Location pids


