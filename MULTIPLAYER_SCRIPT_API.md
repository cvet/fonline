# FOnline Engine Multiplayer Script API

> Document under development, do not rely on this API before the global refactoring complete.  
> Estimated finishing date is middle of this year.

## Table of Content

- [General information](#general-information)
- [Common global methods](#common-global-methods)
- [Server global methods](#server-global-methods)
- [Client global methods](#client-global-methods)
- [Global properties](#global-properties)
- [Entities](#entities)
  * [Item properties](#item-properties)
  * [Item server methods](#item-server-methods)
  * [Item client methods](#item-client-methods)
  * [Critter properties](#critter-properties)
  * [Critter server methods](#critter-server-methods)
  * [Critter client methods](#critter-client-methods)
  * [Map properties](#map-properties)
  * [Map server methods](#map-server-methods)
  * [Map client methods](#map-client-methods)
  * [Location properties](#location-properties)
  * [Location server methods](#location-server-methods)
  * [Location client methods](#location-client-methods)
- [Events](#events)
  * [Server events](#server-events)
  * [Client events](#client-events)
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

## Common global methods

* void Assert(bool condition) *(AngelScript only)*
* void ThrowException(string message) *(AngelScript only)*
* int Random(int minValue, int maxValue)
* void Log(string text)
* bool StrToInt(string text, ref int result) *(AngelScript only)*
* bool StrToFloat(string text, ref float result) *(AngelScript only)*
* int GetHexDistance(uint16 hx1, uint16 hy1, uint16 hx2, uint16 hy2)
* uint8 GetHexDir(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy)
* uint8 GetHexDirWithOffset(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float offset)
* uint GetTick()
* hash GetStrHash(string text)
* string GetHashStr(hash h)
* uint DecodeUTF8(string text, ref uint length)
* string EncodeUTF8(uint ucs)
* void Yield(uint time) *(AngelScript only)*
* string SHA1(string text)
* string SHA2(string text)
* int SystemCall(string command) *(AngelScript only)*
* int SystemCallExt(string command, ref string output) *(AngelScript only)*
* void OpenLink(string link)
* Entity GetProtoItem(hash pid, int->int props)
* uint GetUnixTime()

## Server global methods

* int GetCritterDistance(Critter cr1, Critter cr2)
* Item GetItem(uint itemId)
* void MoveItemToCritter(Item item, uint count, Critter toCr, bool skipChecks)
* void MoveItemToMap(Item item, uint count, Map toMap, uint16 toHx, uint16 toHy, bool skipChecks)
* void MoveItemToContainer(Item item, uint count, Item toCont, uint stackId, bool skipChecks)
* void MoveItemsToCritter(Item[] items, Critter toCr, bool skipChecks)
* void MoveItemsToMap(Item[] items, Map toMap, uint16 toHx, uint16 toHy, bool skipChecks)
* void MoveItemsToContainer(Item[] items, Item toCont, uint stackId, bool skipChecks)
* void DeleteItem(Item item)
* void DeleteItemById(uint itemId)
* void DeleteItems(Item[] items)
* void DeleteItemsById(uint[] itemIds)
* void DeleteNpc(Critter npc)
* void DeleteNpcById(uint npcId)
* void RadioMessage(uint16 channel, string text)
* void RadioMessageMsg(uint16 channel, uint16 textMsg, uint numStr)
* void RadioMessageMsgLex(uint16 channel, uint16 textMsg, uint numStr, string lexems)
* uint GetFullSecond(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
* void GetGameTime(uint fullSecond, ref uint16 year, ref uint16 month, ref uint16 day, ref uint16 dayOfWeek, ref uint16 hour, ref uint16 minute, ref uint16 second)
* Location CreateLocation(hash locPid, uint16 wx, uint16 wy, Critter[] critters)
* void DeleteLocation(Location loc)
* void DeleteLocationById(uint locId)
* Critter GetCritter(uint crid)
* Critter GetPlayer(string name)
* Critter[] GetGlobalMapCritters(uint16 wx, uint16 wy, uint radius, int findType)
* Map GetMap(uint mapId)
* Map GetMapByPid(hash mapPid, uint skipCount)
* Location GetLocation(uint locId)
* Location GetLocationByPid(hash locPid, uint skipCount)
* Location[] GetLocationsAroundPos(uint16 wx, uint16 wy, uint radius)
* Location[] GetVisibleLocationsAroundPos(uint16 wx, uint16 wy, uint radius, Critter cr)
* uint[] GetZoneLocationIds(uint16 zx, uint16 zy, uint zoneRadius)
* bool RunNpcDialog(Critter player, Critter npc, bool ignoreDistance)
* bool RunCustomNpcDialog(Critter player, Critter npc, uint dlgPack, bool ignoreDistance)
* bool RunCustomDialogOnHex(Critter player, uint dlgPack, uint16 hx, uint16 hy, bool ignoreDistance)
* int64 GetWorldItemCount(hash pid)
* void AddTextListener(int sayType, string firstStr, uint parameter, callback-Entity func)
* void EraseTextListener(int sayType, string firstStr, uint parameter)
* void SwapCritters(Critter cr1, Critter cr2, bool withInventory)
* Item[] GetAllItems(hash pid)
* Critter[] GetOnlinePlayers()
* uint[] GetRegisteredPlayerIds()
* Critter[] GetAllNpc(hash pid)
* Map[] GetAllMaps(hash pid)
* Location[] GetAllLocations(hash pid)
* void GetTime(ref uint16 year, ref uint16 month, ref uint16 day, ref uint16 dayOfWeek, ref uint16 hour, ref uint16 minute, ref uint16 second, ref uint16 milliseconds)
* void SetTime(uint16 multiplier, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
* void AllowSlot(uint8 index, bool enableSend)
* void AddDataSource(string datName)

## Client global methods

* string CustomCall(string command, string separator)
* Critter GetChosen()
* Item GetItem(uint itemId)
* Item[] GetVisibleItems()
* Item[] GetVisibleItemsOnHex(uint16 hx, uint16 hy)
* int GetCritterDistance(Critter cr1, Critter cr2)
* Critter GetCritter(uint critterId)
* Critter[] GetCrittersAroundHex(uint16 hx, uint16 hy, uint radius, int findType)
* Critter[] GetCrittersByPids(hash pid, int findType)
* Critter[] GetCrittersInPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType)
* Critter[] GetCrittersWithBlockInPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, ref uint16 preBlockHx, ref uint16 preBlockHy, ref uint16 blockHx, ref uint16 blockHy)
* void GetHexInPath(uint16 fromHx, uint16 fromHy, ref uint16 toHx, ref uint16 toHy, float angle, uint dist)
* uint8[] GetPathToHex(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)
* uint8[] GetPathToCritter(Critter cr, uint16 toHx, uint16 toHy, uint cut)
* uint GetPathLengthToHex(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)
* uint GetPathLengthToCritter(Critter cr, uint16 toHx, uint16 toHy, uint cut)
* void FlushScreen(uint fromColor, uint toColor, uint ms)
* void QuakeScreen(uint noise, uint ms)
* void PlaySound(string soundName)
* void PlayMusic(string musicName, uint repeatTime)
* void PlayVideo(string videoName, bool canStop)
* hash GetCurMapPid()
* void Message(string msg)
* void MessageExt(string msg, int type)
* void MessageMsg(int textMsg, uint strNum)
* void MessageMsgExt(int textMsg, uint strNum, int type)
* void MapMessage(string text, uint16 hx, uint16 hy, uint ms, uint color, bool fade, int ox, int oy)
* string GetMsgStr(int textMsg, uint strNum)
* string GetMsgStrExt(int textMsg, uint strNum, uint skipCount)
* uint GetMsgStrNumUpper(int textMsg, uint strNum)
* uint GetMsgStrNumLower(int textMsg, uint strNum)
* uint GetMsgStrCount(int textMsg, uint strNum)
* bool IsMsgStr(int textMsg, uint strNum)
* string ReplaceTextStr(string text, string replace, string str) *(AngelScript only)*
* string ReplaceTextInt(string text, string replace, int i) *(AngelScript only)*
* string FormatTags(string text, string lexems)
* void MoveScreenToHex(uint16 hx, uint16 hy, uint speed, bool canStop)
* void MoveScreenByOffset(int ox, int oy, uint speed, bool canStop)
* void LockScreenScroll(Critter cr, bool softLock, bool unlockIfSame)
* int GetFog(uint16 zoneX, uint16 zoneY)
* uint GetDayTime(uint dayPart)
* void GetDayColor(uint dayPart, ref uint8 r, ref uint8 g, ref uint8 b)
* uint GetFullSecond(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
* void GetGameTime(uint fullSecond, ref uint16 year, ref uint16 month, ref uint16 day, ref uint16 dayOfWeek, ref uint16 hour, ref uint16 minute, ref uint16 second)
* void MoveHexByDir(ref uint16 hx, ref uint16 hy, uint8 dir, uint steps)
* hash GetTileName(uint16 hx, uint16 hy, bool roof, int layer)
* void Preload3dFiles(string[] fnames)
* void WaitPing()
* void LoadFont(int fontIndex, string fontFname)
* void SetDefaultFont(int font, uint color)
* void SetEffect(int effectType, int effectSubtype, string effectName, string effectDefines)
* void RedrawMap(bool onlyTiles, bool onlyRoof, bool onlyLight)
* void SimulateMouseClick(int x, int y, int button)
* void SimulateKeyboardPress(uint8 key1, uint8 key2, string key1Text, string key2Text)
* void SetRainAnim(string fallAnimName, string dropAnimName)
* void ChangeZoom(float targetZoom)
* void GetTime(ref uint16 year, ref uint16 month, ref uint16 day, ref uint16 dayOfWeek, ref uint16 hour, ref uint16 minute, ref uint16 second, ref uint16 milliseconds)
* void AllowSlot(uint8 index, bool enableSend)
* void AddDataSource(string datName)
* uint LoadSprite(string sprName)
* uint LoadSpriteByHash(hash nameHash)
* int GetSpriteWidth(uint sprId, int frameIndex)
* int GetSpriteHeight(uint sprId, int frameIndex)
* uint GetSpriteCount(uint sprId)
* uint GetSpriteTicks(uint sprId)
* uint GetSpritePixelColor(uint sprId, int frameIndex, int x, int y)
* void GetTextInfo(string text, int w, int h, int font, int flags, ref int tw, ref int th, ref int lines)
* void DrawSprite(uint sprId, int frameIndex, int x, int y, uint color, bool offs)
* void DrawSpriteSized(uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom, uint color, bool offs)
* void DrawSpritePattern(uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color)
* void DrawText(string text, int x, int y, int w, int h, uint color, int font, int flags)
* void DrawPrimitive(int primitiveType, int[] data)
* void DrawMapSprite(MapSprite mapSpr)
* void DrawCritter2d(hash modelName, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)
* void DrawCritter3d(uint instance, hash modelName, uint anim1, uint anim2, int[] layers, float[] position, uint color)
* void PushDrawScissor(int x, int y, int w, int h)
* void PopDrawScissor()
* void ActivateOffscreenSurface(bool forceClear)
* void PresentOffscreenSurface(int effectSubtype)
* void PresentOffscreenSurfaceExt(int effectSubtype, int x, int y, int w, int h)
* void PresentOffscreenSurfaceExt2(int effectSubtype, int fromX, int fromY, int fromW, int fromH, int toX, int toY, int toW, int toH)
* void ShowScreen(int screen, string->int data)
* void HideScreen(int screen)
* bool GetHexMonitorPos(uint16 hx, uint16 hy, ref int x, ref int y)
* bool GetHexByMonitorPos(int x, int y, ref uint16 hx, ref uint16 hy)
* Item GetItemByMonitorPos(int x, int y)
* Critter GetCritterByMonitorPos(int x, int y)
* Entity GetEntityByMonitorPos(int x, int y)
* uint16 GetMapWidth()
* uint16 GetMapHeight()
* bool IsMapHexPassed(uint16 hx, uint16 hy)
* bool IsMapHexRaked(uint16 hx, uint16 hy)
* void SaveScreenshot(string filePath)
* void SaveText(string filePath, string text)
* void SetCacheData(string name, uint8[] data)
* void SetCacheDataExt(string name, uint8[] data, uint dataSize)
* uint8[] GetCacheData(string name)
* void SetCacheText(string name, string str)
* string GetCacheText(string name)
* bool IsCacheEntry(string name)
* void RemoveCacheEntry(string name)
* void SetUserConfig(string->string keyValues)

## Global properties

* PrivateCommon uint16 YearStart
* PrivateCommon uint16 Year
* PrivateCommon uint16 Month
* PrivateCommon uint16 Day
* PrivateCommon uint16 Hour
* PrivateCommon uint16 Minute
* PrivateCommon uint16 Second
* PrivateCommon uint16 TimeMultiplier
* PrivateServer uint LastEntityId
* PrivateCommon uint LastDeferredCallId
* PrivateCommon uint HistoryRecordsId

## Entities

### Item properties

* Public ItemOwnership Accessory
* Public uint MapId
* Public uint16 HexX
* Public uint16 HexY
* Public uint CritId
* Public uint8 CritSlot
* Public uint ContainerId
* Public uint ContainerStack
* Public float FlyEffectSpeed
* Public hash PicMap
* Public hash PicInv
* Public int16 OffsetX
* Public int16 OffsetY
* Public bool Stackable
* Public bool GroundLevel
* Public bool Opened
* Public CornerType Corner
* Public uint8 Slot
* Public uint Weight
* Public uint Volume
* Public bool DisableEgg
* Public uint16 AnimWaitBase
* Public uint16 AnimWaitRndMin
* Public uint16 AnimWaitRndMax
* Public uint8 AnimStay0
* Public uint8 AnimStay1
* Public uint8 AnimShow0
* Public uint8 AnimShow1
* Public uint8 AnimHide0
* Public uint8 AnimHide1
* Public uint8 SpriteCut
* Public int8 DrawOrderOffsetHexY
* Public uint8[] BlockLines
* Public bool IsStatic
* Public bool IsScenery
* Public bool IsWall
* Public bool IsCanOpen
* Public bool IsScrollBlock
* Public bool IsHidden
* Public bool IsHiddenPicture
* Public bool IsHiddenInStatic
* Public bool IsFlat
* Public bool IsNoBlock
* Public bool IsShootThru
* Public bool IsLightThru
* Public bool IsAlwaysView
* Public bool IsBadItem
* Public bool IsNoHighlight
* Public bool IsShowAnim
* Public bool IsShowAnimExt
* Public bool IsLight
* Public bool IsGeck
* Public bool IsTrap
* Public bool IsTrigger
* Public bool IsNoLightInfluence
* Public bool IsGag
* Public bool IsColorize
* Public bool IsColorizeInv
* Public bool IsCanTalk
* Public bool IsRadio
* Public string Lexems
* PublicModifiable int16 SortValue
* Public uint8 Info
* PublicModifiable uint8 Mode
* Public int8 LightIntensity
* Public uint8 LightDistance
* Public uint8 LightFlags
* Public uint LightColor
* PrivateServer hash ScriptId
* Public uint Count
* Protected int16 TrapValue
* Protected uint16 RadioChannel
* Protected uint16 RadioFlags
* Protected uint8 RadioBroadcastSend
* Protected uint8 RadioBroadcastRecv

### Item server methods

* Item AddItem(hash pid, uint count, uint stackId)
* Item[] GetItems(uint stackId)
* bool SetScript(callback-Item func)
* Map GetMapPos(ref uint16 hx, ref uint16 hy)
* void ChangeProto(hash pid)
* void Animate(uint8 fromFrm, uint8 toFrm)
* bool CallStaticItemFunction(Critter cr, Item item, int param)

### Item client methods

* bool GetMapPos(ref uint16 hx, ref uint16 hy)
* void Animate(uint fromFrame, uint toFrame)
* Item[] GetItems(uint stackId)

### Critter properties

* Public hash ModelName
* Protected uint WalkTime
* Protected uint RunTime
* Protected uint Multihex
* PrivateServer uint MapId
* PrivateServer uint RefMapId
* PrivateServer hash RefMapPid
* PrivateServer uint RefLocationId
* PrivateServer hash RefLocationPid
* PrivateCommon uint16 HexX
* PrivateCommon uint16 HexY
* PrivateCommon uint8 Dir
* PrivateServer string Password
* PrivateCommon CritterCondition Cond
* PrivateServer bool ClientToDelete
* Protected uint16 WorldX
* Protected uint16 WorldY
* Protected uint GlobalMapLeaderId
* PrivateServer uint GlobalMapTripId
* PrivateServer uint RefGlobalMapTripId
* PrivateServer uint RefGlobalMapLeaderId
* PrivateServer uint16 LastMapHexX
* PrivateServer uint16 LastMapHexY
* PrivateCommon uint Anim1Life
* PrivateCommon uint Anim1Knockout
* PrivateCommon uint Anim1Dead
* PrivateCommon uint Anim2Life
* PrivateCommon uint Anim2Knockout
* PrivateCommon uint Anim2Dead
* PrivateServer uint8[] GlobalMapFog
* PrivateServer hash[] TE_FuncNum
* PrivateServer uint[] TE_Rate
* PrivateServer uint[] TE_NextTime
* PrivateServer int[] TE_Identifier
* VirtualPrivateServer int SneakCoefficient
* VirtualProtected uint LookDistance
* Public int8 Gender
* Protected hash NpcRole
* Protected int ReplicationTime
* Public uint TalkDistance
* Public int ScaleFactor
* Public int CurrentHp
* PrivateServer uint MaxTalkers
* Public hash DialogId
* Public string Lexems
* PrivateServer uint HomeMapId
* PrivateServer uint16 HomeHexX
* PrivateServer uint16 HomeHexY
* PrivateServer uint8 HomeDir
* PrivateServer uint[] KnownLocations
* PrivateServer uint[] ConnectionIp
* PrivateServer uint16[] ConnectionPort
* PrivateServer uint ShowCritterDist1
* PrivateServer uint ShowCritterDist2
* PrivateServer uint ShowCritterDist3
* PrivateServer hash ScriptId
* Protected hash[] KnownLocProtoId
* PrivateClient int[] Anim3dLayer
* Protected bool IsHide
* Protected bool IsNoHome
* Protected bool IsGeck
* Protected bool IsNoUnarmed
* VirtualProtected bool IsNoWalk
* VirtualProtected bool IsNoRun
* Protected bool IsNoRotate
* Public bool IsNoTalk
* Public bool IsNoFlatten
* Public uint TimeoutBattle
* Protected uint TimeoutTransfer
* Protected uint TimeoutRemoveFromGame

### Critter server methods

* bool IsPlayer()
* bool IsNpc()
* int GetAccess()
* bool SetAccess(int access)
* Map GetMap()
* bool MoveToDir(uint8 direction)
* void TransitToHex(uint16 hx, uint16 hy, uint8 dir)
* void TransitToMapHex(Map map, uint16 hx, uint16 hy, uint8 dir)
* void TransitToGlobal()
* void TransitToGlobalWithGroup(Critter[] group)
* void TransitToGlobalGroup(Critter leader)
* bool IsAlive()
* bool IsKnockout()
* bool IsDead()
* bool IsFree()
* bool IsBusy()
* void Wait(uint ms)
* void RefreshView()
* void ViewMap(Map map, uint look, uint16 hx, uint16 hy, uint8 dir)
* void Say(uint8 howSay, string text)
* void SayMsg(uint8 howSay, uint16 textMsg, uint numStr)
* void SayMsgLex(uint8 howSay, uint16 textMsg, uint numStr, string lexems)
* void SetDir(uint8 dir)
* Critter[] GetCritters(bool lookOnMe, int findType)
* Critter[] GetTalkedPlayers()
* bool IsSeeCr(Critter cr)
* bool IsSeenByCr(Critter cr)
* bool IsSeeItem(Item item)
* uint CountItem(hash protoId)
* bool DeleteItem(hash pid, uint count)
* Item AddItem(hash pid, uint count)
* Item GetItem(uint itemId)
* Item GetItemByPredicate(predicate-Item predicate)
* Item GetItemBySlot(int slot)
* Item GetItemByPid(hash protoId)
* Item[] GetItems()
* Item[] GetItemsBySlot(int slot)
* Item[] GetItemsByPredicate(predicate-Item predicate)
* void ChangeItemSlot(uint itemId, int slot)
* void SetCondition(int cond)
* void CloseDialog()
* void Action(int action, int actionExt, Item item)
* void Animate(uint anim1, uint anim2, Item item, bool clearSequence, bool delayPlay)
* void SetConditionAnims(int cond, uint anim1, uint anim2)
* void PlaySound(string soundName, bool sendSelf)
* bool IsKnownLocation(bool byId, uint locNum)
* void SetKnownLocation(bool byId, uint locNum)
* void UnsetKnownLocation(bool byId, uint locNum)
* void SetFog(uint16 zoneX, uint16 zoneY, int fog)
* int GetFog(uint16 zoneX, uint16 zoneY)
* void ShowItems(Item[] items, int param)
* void Disconnect()
* bool IsOnline()
* void SetScript(string funcName)
* void AddTimeEvent(callback-Critter func, uint duration, int identifier)
* void AddTimeEventWithRate(callback-Critter func, uint duration, int identifier, uint rate)
* uint GetTimeEvents(int identifier, ref int[] indexes, ref int[] durations, ref int[] rates)
* uint GetTimeEventsExt(int[] findIdentifiers, int[] identifiers, ref int[] indexes, ref int[] durations, ref int[] rates)
* void ChangeTimeEvent(uint index, uint newDuration, uint newRate)
* void EraseTimeEvent(uint index)
* uint EraseTimeEvents(int identifier)
* uint EraseTimeEventsExt(int[] identifiers)
* void MoveToCritter(Critter target, uint cut, bool isRun)
* void MoveToHex(uint16 hx, uint16 hy, uint cut, bool isRun)
* int GetMovingState()
* void ResetMovingState(ref uint gagId)

### Critter client methods

* bool IsChosen()
* bool IsPlayer()
* bool IsNpc()
* bool IsOffline()
* bool IsAlive()
* bool IsKnockout()
* bool IsDead()
* bool IsFree()
* bool IsBusy()
* bool IsAnim3d()
* bool IsAnimAvailable(uint anim1, uint anim2)
* bool IsAnimPlaying()
* uint GetAnim1()
* void Animate(uint anim1, uint anim2)
* void AnimateExt(uint anim1, uint anim2, Item item)
* void StopAnim()
* void Wait(uint ms)
* uint CountItem(hash protoId)
* Item GetItem(uint itemId)
* Item GetItemByPredicate(predicate-Item predicate)
* Item GetItemBySlot(int slot)
* Item GetItemByPid(hash protoId)
* Item[] GetItems()
* Item[] GetItemsBySlot(int slot)
* Item[] GetItemsByPredicate(predicate-Item predicate)
* void SetVisibility(bool visible)
* bool GetVisibility()
* void SetContourColor(uint value)
* uint GetContourColor()
* void GetNameTextInfo(ref bool nameVisible, ref int x, ref int y, ref int w, ref int h, ref int lines)
* void AddAnimCallback(uint anim1, uint anim2, float normalizedTime, callback-Critter animCallback)
* bool GetBonePos(hash boneName, ref int boneX, ref int boneY)

### Map properties

* PrivateServer uint LoopTime1
* PrivateServer uint LoopTime2
* PrivateServer uint LoopTime3
* PrivateServer uint LoopTime4
* PrivateServer uint LoopTime5
* PrivateServer string FileDir
* PrivateServer uint16 Width
* PrivateServer uint16 Height
* PrivateServer uint16 WorkHexX
* PrivateServer uint16 WorkHexY
* PrivateServer uint LocId
* PrivateServer uint LocMapIndex
* PrivateServer uint8 RainCapacity
* PrivateServer int CurDayTime
* PrivateServer hash ScriptId
* PrivateServer int[] DayTime
* PrivateServer uint8[] DayColor
* PrivateServer bool IsNoLogOut

### Map server methods

* Location GetLocation()
* void SetScript(string funcName)
* Item AddItem(uint16 hx, uint16 hy, hash protoId, uint count, int->int props)
* Item[] GetItems()
* Item[] GetItemsOnHex(uint16 hx, uint16 hy)
* Item[] GetItemsAroundHex(uint16 hx, uint16 hy, uint radius, hash pid)
* Item[] GetItemsByPid(hash pid)
* Item[] GetItemsByPredicate(predicate-Item predicate)
* Item[] GetItemsOnHexByPredicate(uint16 hx, uint16 hy, predicate-Item predicate)
* Item[] GetItemsAroundHexByPredicate(uint16 hx, uint16 hy, uint radius, predicate-Item predicate)
* Item GetItem(uint itemId)
* Item GetItemOnHex(uint16 hx, uint16 hy, hash pid)
* Critter GetCritterOnHex(uint16 hx, uint16 hy)
* Item GetStaticItemOnHex(uint16 hx, uint16 hy, hash pid)
* Item[] GetStaticItemsInHex(uint16 hx, uint16 hy)
* Item[] GetStaticItemsAroundHex(uint16 hx, uint16 hy, uint radius, hash pid)
* Item[] GetStaticItemsByPid(hash pid)
* Item[] GetStaticItemsByPredicate(predicate-Item predicate)
* Item[] GetStaticItems()
* Critter GetCritter(uint crid)
* Critter[] GetCrittersAroundHex(uint16 hx, uint16 hy, uint radius, int findType)
* Critter[] GetCrittersByPids(hash pid, int findType)
* Critter[] GetCrittersInPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType)
* Critter[] GetCrittersWithBlockInPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, ref uint16 preBlockHx, ref uint16 preBlockHy, ref uint16 blockHx, ref uint16 blockHy)
* Critter[] GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType)
* Critter[] GetCrittersSeeing(Critter[] critters, bool lookOnThem, int findType)
* void GetHexInPath(uint16 fromHx, uint16 fromHy, ref uint16 toHx, ref uint16 toHy, float angle, uint dist)
* void GetWallHexInPath(uint16 fromHx, uint16 fromHy, ref uint16 toHx, ref uint16 toHy, float angle, uint dist)
* uint GetPathLengthToHex(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)
* uint GetPathLengthToCritter(Critter cr, uint16 toHx, uint16 toHy, uint cut)
* Critter AddNpc(hash protoId, uint16 hx, uint16 hy, uint8 dir, int->int props)
* uint GetNpcCount(int npcRole, int findType)
* Critter GetNpc(int npcRole, int findType, uint skipCount)
* bool IsHexPassed(uint16 hexX, uint16 hexY)
* bool IsHexesPassed(uint16 hexX, uint16 hexY, uint radius)
* bool IsHexRaked(uint16 hexX, uint16 hexY)
* void SetText(uint16 hexX, uint16 hexY, uint color, string text)
* void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum)
* void SetTextMsgLex(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum, string lexems)
* void RunEffect(hash effPid, uint16 hx, uint16 hy, uint radius)
* void RunFlyEffect(hash effPid, Critter fromCr, Critter toCr, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy)
* bool CheckPlaceForItem(uint16 hx, uint16 hy, hash pid)
* void BlockHex(uint16 hx, uint16 hy, bool full)
* void UnblockHex(uint16 hx, uint16 hy)
* void PlaySound(string soundName)
* void PlaySoundInRadius(string soundName, uint16 hx, uint16 hy, uint radius)
* void Regenerate()
* void MoveHexByDir(ref uint16 hx, ref uint16 hy, uint8 dir, uint steps)
* void VerifyTrigger(Critter cr, uint16 hx, uint16 hy, uint8 dir)

### Map client methods


### Location properties

* PrivateServer hash[] MapProtos
* PrivateServer hash[] MapEntrances
* PrivateServer hash[] Automaps
* PrivateServer uint MaxPlayers
* PrivateServer bool AutoGarbage
* PrivateServer bool GeckVisible
* PrivateServer hash EntranceScript
* PrivateServer uint16 WorldX
* PrivateServer uint16 WorldY
* PrivateServer uint16 Radius
* PrivateServer bool Hidden
* PrivateServer bool ToGarbage
* PrivateServer uint Color
* PrivateServer bool IsEncounter

### Location server methods

* uint GetMapCount()
* Map GetMap(hash mapPid)
* Map GetMapByIndex(uint index)
* Map[] GetMaps()
* void GetEntrance(uint entrance, ref uint mapIndex, ref hash entire)
* uint GetEntrances(ref int[] mapsIndex, ref hash[] entires)
* void Regenerate()

### Location client methods


## Events

### Server events

* Init()
* GenerateWorld()
* Start()
* Finish()
* Loop()
* GlobalMapCritterIn(Critter critter)
* GlobalMapCritterOut(Critter critter)
* LocationInit(Location location, bool firstTime)
* LocationFinish(Location location)
* MapInit(Map map, bool firstTime)
* MapFinish(Map map)
* MapLoop(Map map, uint loopIndex)
* MapCritterIn(Map map, Critter critter)
* MapCritterOut(Map map, Critter critter)
* MapCheckLook(Map map, Critter critter, Critter target)
* MapCheckTrapLook(Map map, Critter critter, Item item)
* CritterInit(Critter critter, bool firstTime)
* CritterFinish(Critter critter)
* CritterIdle(Critter critter)
* CritterGlobalMapIdle(Critter critter)
* CritterCheckMoveItem(Critter critter, Item item, uint8 toSlot)
* CritterMoveItem(Critter critter, Item item, uint8 fromSlot)
* CritterShow(Critter critter, Critter showCritter)
* CritterShowDist1(Critter critter, Critter showCritter)
* CritterShowDist2(Critter critter, Critter showCritter)
* CritterShowDist3(Critter critter, Critter showCritter)
* CritterHide(Critter critter, Critter hideCritter)
* CritterHideDist1(Critter critter, Critter hideCritter)
* CritterHideDist2(Critter critter, Critter hideCritter)
* CritterHideDist3(Critter critter, Critter hideCritter)
* CritterShowItemOnMap(Critter critter, Item item, bool added, Critter fromCritter)
* CritterHideItemOnMap(Critter critter, Item item, bool removed, Critter toCritter)
* CritterChangeItemOnMap(Critter critter, Item item)
* CritterMessage(Critter critter, Critter receiver, int num, int value)
* CritterTalk(Critter critter, Critter who, bool begin, uint talkers)
* CritterBarter(Critter cr, Critter trader, bool begin, uint barterCount)
* CritterGetAttackDistantion(Critter critter, Item item, uint8 itemMode, ref uint dist)
* PlayerRegistration(uint ip, string name, ref uint disallowMsgNum, ref uint disallowStrNum, ref string disallowLex)
* PlayerLogin(uint ip, string name, uint id, ref uint disallowMsgNum, ref uint disallowStrNum, ref string disallowLex)
* PlayerGetAccess(Critter player, int arg1, ref string arg2)
* PlayerAllowCommand(Critter player, string arg1, uint8 arg2)
* PlayerLogout(Critter player)
* ItemInit(Item item, bool firstTime)
* ItemFinish(Item item)
* ItemWalk(Item item, Critter critter, bool isIn, uint8 dir)
* ItemCheckMove(Item item, uint count, Entity from, Entity to)
* StaticItemWalk(Item item, Critter critter, bool isIn, uint8 dir)

### Client events

* Start()
* Finish()
* AutoLogin(string login, string password)
* Loop()
* GetActiveScreens(ref int[] screens)
* ScreenChange(bool show, int screen, string->int data)
* ScreenScroll(int offsetX, int offsetY)
* RenderIface()
* RenderMap()
* MouseDown(MouseButton button)
* MouseUp(MouseButton button)
* MouseMove(int offsetX, int offsetY)
* KeyDown(KeyCode key, string text)
* KeyUp(KeyCode key)
* InputLost()
* CritterIn(Critter critter)
* CritterOut(Critter critter)
* ItemMapIn(Item item)
* ItemMapChanged(Item item, Item oldItem)
* ItemMapOut(Item item)
* ItemInvAllIn()
* ItemInvIn(Item item)
* ItemInvChanged(Item item, Item oldItem)
* ItemInvOut(Item item)
* MapLoad()
* MapUnload()
* ReceiveItems(Item[] items, int param)
* MapMessage(ref string text, ref uint16 hexX, ref uint16 hexY, ref uint color, ref uint delay)
* InMessage(string text, ref int sayType, ref uint critterId, ref uint delay)
* OutMessage(ref string text, ref int sayType)
* MessageBox(string text, MessageBoxTextType type, bool scriptCall)
* CombatResult(uint[] result)
* ItemCheckMove(Item item, uint count, Entity from, Entity to)
* CritterAction(bool localCall, Critter critter, int action, int actionExt, Item actionItem)
* Animation2dProcess(bool arg1, Critter arg2, uint arg3, uint arg4, Item arg5)
* Animation3dProcess(bool arg1, Critter arg2, uint arg3, uint arg4, Item arg5)
* CritterAnimation(hash arg1, uint arg2, uint arg3, ref uint arg4, ref uint arg5, ref int arg6, ref int arg7, ref string arg8)
* CritterAnimationSubstitute(hash arg1, uint arg2, uint arg3, ref hash arg4, ref uint arg5, ref uint arg6)
* CritterAnimationFallout(hash arg1, ref uint arg2, ref uint arg3, ref uint arg4, ref uint arg5, ref uint arg6)
* CritterCheckMoveItem(Critter critter, Item item, uint8 toSlot)
* CritterGetAttackDistantion(Critter critter, Item item, uint8 itemMode, ref uint dist)

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


