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

* string WorkDir
* string CommandLine
* vector<string> CommandLineArgs
* uint MinNameLength
* uint MaxNameLength
* uint Breaktime
* string Language
* bool Quit
* uint TalkDistance
* bool RunOnCombat
* bool RunOnTransfer
* uint TimeoutTransfer
* uint TimeoutBattle
* uint GlobalMapWidth
* uint GlobalMapHeight
* uint GlobalMapZoneLength
* int LookChecks
* vector<uint> LookDir
* vector<uint> LookSneakDir
* uint MinimumOfflineTime
* uint RegistrationTimeout
* uint NpcMaxTalkers
* uint DlgTalkMinTime
* uint DlgBarterMinTime
* uint WhisperDist
* uint ShoutDist
* bool NoAnswerShuffle
* uint SneakDivider
* uint LookMinimum
* int DeadHitPoints
* uint Port
* bool SecuredWebSockets
* bool DisableTcpNagle
* bool DisableZlibCompression
* uint FloodSize
* string WssPrivateKey
* string WssCertificate
* string Host
* uint ProxyType
* string ProxyHost
* uint ProxyPort
* string ProxyUser
* string ProxyPass
* uint PingPeriod
* bool WaitPing
* uint Ping
* bool DebugNet
* bool DisableAudio
* int SoundVolume
* int MusicVolume
* int ScreenWidth
* int ScreenHeight
* float SpritesZoom
* int ScrOx
* int ScrOy
* bool ShowCorners
* bool ShowSpriteCuts
* bool ShowDrawOrder
* bool ShowSpriteBorders
* vector<float> EffectValues
* bool MapHexagonal
* int MapDirCount
* int MapHexWidth
* int MapHexHeight
* int MapHexLineHeight
* int MapTileOffsX
* int MapTileOffsY
* int MapTileStep
* int MapRoofOffsX
* int MapRoofOffsY
* int MapRoofSkipSize
* float MapCameraAngle
* bool MapSmoothPath
* string MapDataPrefix
* string WindowName
* bool WindowCentered
* bool ForceOpenGL
* bool ForceDirect3D
* bool ForceMetal
* bool ForceGnm
* bool RenderDebug
* uint Animation3dSmoothTime
* uint Animation3dFPS
* bool Enable3dRendering
* int MultiSampling
* bool VSync
* bool AlwaysOnTop
* vector<float> EffectValues
* bool FullScreen
* int Brightness
* uint FPS
* int FixedFPS
* int StartYear
* string ASServer
* string ASClient
* string ASMapper
* string ResourcesOutput
* vector<string> ContentEntry
* vector<string> ResourcesEntry
* vector<bool> CritterSlotEnabled
* vector<bool> CritterSlotSendData
* uint CritterFidgetTime
* uint Anim2CombatBegin
* uint Anim2CombatIdle
* uint Anim2CombatEnd
* string PlayerOffAppendix
* bool ShowPlayerNames
* bool ShowNpcNames
* bool ShowCritId
* bool ShowGroups
* float SpritesZoomMax
* float SpritesZoomMin
* uint ScrollDelay
* int ScrollStep
* uint RainTick
* int16 RainSpeedX
* int16 RainSpeedY
* uint ChosenLightColor
* uint8 ChosenLightDistance
* int ChosenLightIntensity
* uint8 ChosenLightFlags
* bool MouseScroll
* bool ScrollCheck
* bool ScrollKeybLeft
* bool ScrollKeybRight
* bool ScrollKeybUp
* bool ScrollKeybDown
* bool ScrollMouseLeft
* bool ScrollMouseRight
* bool ScrollMouseUp
* bool ScrollMouseDown
* uint8 RoofAlpha
* bool ShowTile
* bool ShowRoof
* bool ShowItem
* bool ShowScen
* bool ShowWall
* bool ShowCrit
* bool ShowFast
* bool HideCursor
* bool ShowMoveCursor
* bool WebBuild
* bool WindowsBuild
* bool LinuxBuild
* bool MacOsBuild
* bool AndroidBuild
* bool IOsBuild
* bool DesktopBuild
* bool TabletBuild
* uint DoubleClickTime
* uint ConsoleHistorySize
* bool DisableMouseEvents
* bool DisableKeyboardEvents
* int MouseX
* int MouseY
* string AutoLogin
* uint TextDelay
* bool WinNotify
* bool SoundNotify
* bool HelpInfo
* string ServerDir
* string StartMap
* int StartHexX
* int StartHexY
* bool SplitTilesCollection
* uint AdminPanelPort
* bool AssimpLogging
* string DbStorage
* string DbHistory
* bool NoStart
* int GameSleep

## Enums

### MessageBoxTextType


### MouseButton

* Left = 0
* Right = 1
* Middle = 2
* Ext0 = 5
* Ext1 = 6
* Ext2 = 7
* Ext3 = 8
* Ext4 = 9

### KeyCode

* DIK_NONE = 0
* DIK_ESCAPE = 0x01
* DIK_1 = 0x02
* DIK_2 = 0x03
* DIK_3 = 0x04
* DIK_4 = 0x05
* DIK_5 = 0x06
* DIK_6 = 0x07
* DIK_7 = 0x08
* DIK_8 = 0x09
* DIK_9 = 0x0A
* DIK_0 = 0x0B
* DIK_MINUS = 0x0C
* DIK_EQUALS = 0x0D
* DIK_BACK = 0x0E
* DIK_TAB = 0x0F
* DIK_Q = 0x10
* DIK_W = 0x11
* DIK_E = 0x12
* DIK_R = 0x13
* DIK_T = 0x14
* DIK_Y = 0x15
* DIK_U = 0x16
* DIK_I = 0x17
* DIK_O = 0x18
* DIK_P = 0x19
* DIK_LBRACKET = 0x1A
* DIK_RBRACKET = 0x1B
* DIK_RETURN = 0x1C
* DIK_LCONTROL = 0x1D
* DIK_A = 0x1E
* DIK_S = 0x1F
* DIK_D = 0x20
* DIK_F = 0x21
* DIK_G = 0x22
* DIK_H = 0x23
* DIK_J = 0x24
* DIK_K = 0x25
* DIK_L = 0x26
* DIK_SEMICOLON = 0x27
* DIK_APOSTROPHE = 0x28
* DIK_GRAVE = 0x29
* DIK_LSHIFT = 0x2A
* DIK_BACKSLASH = 0x2B
* DIK_Z = 0x2C
* DIK_X = 0x2D
* DIK_C = 0x2E
* DIK_V = 0x2F
* DIK_B = 0x30
* DIK_N = 0x31
* DIK_M = 0x32
* DIK_COMMA = 0x33
* DIK_PERIOD = 0x34
* DIK_SLASH = 0x35
* DIK_RSHIFT = 0x36
* DIK_MULTIPLY = 0x37
* DIK_LMENU = 0x38
* DIK_SPACE = 0x39
* DIK_CAPITAL = 0x3A
* DIK_F1 = 0x3B
* DIK_F2 = 0x3C
* DIK_F3 = 0x3D
* DIK_F4 = 0x3E
* DIK_F5 = 0x3F
* DIK_F6 = 0x40
* DIK_F7 = 0x41
* DIK_F8 = 0x42
* DIK_F9 = 0x43
* DIK_F10 = 0x44
* DIK_NUMLOCK = 0x45
* DIK_SCROLL = 0x46
* DIK_NUMPAD7 = 0x47
* DIK_NUMPAD8 = 0x48
* DIK_NUMPAD9 = 0x49
* DIK_SUBTRACT = 0x4A
* DIK_NUMPAD4 = 0x4B
* DIK_NUMPAD5 = 0x4C
* DIK_NUMPAD6 = 0x4D
* DIK_ADD = 0x4E
* DIK_NUMPAD1 = 0x4F
* DIK_NUMPAD2 = 0x50
* DIK_NUMPAD3 = 0x51
* DIK_NUMPAD0 = 0x52
* DIK_DECIMAL = 0x53
* DIK_F11 = 0x57
* DIK_F12 = 0x58
* DIK_NUMPADENTER = 0x9C
* DIK_RCONTROL = 0x9D
* DIK_DIVIDE = 0xB5
* DIK_SYSRQ = 0xB7
* DIK_RMENU = 0xB8
* DIK_PAUSE = 0xC5
* DIK_HOME = 0xC7
* DIK_UP = 0xC8
* DIK_PRIOR = 0xC9
* DIK_LEFT = 0xCB
* DIK_RIGHT = 0xCD
* DIK_END = 0xCF
* DIK_DOWN = 0xD0
* DIK_NEXT = 0xD1
* DIK_INSERT = 0xD2
* DIK_DELETE = 0xD3
* DIK_LWIN = 0xDB
* DIK_RWIN = 0xDC
* DIK_TEXT = 0xFE
* DIK_CLIPBOARD_PASTE = 0xFF

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


