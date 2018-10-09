// Dummy data

struct SyncObject
{
    int* SyncMngr;

    void Lock() {}
};

struct Entity
{
    int  Props;
    int  Type;
    int  Id;
    int  ProtoId;
    int  RefCounter;
    int  IsDestroyed;
    int  IsDestroying;
    int  Parent;
    int  Children;

    void AddRef()     {}
    void Release()    {}
    void GetProtoId() {}
};

struct Item: public Entity
{
    Item( uint id, void* proto ) {}

    int Proto;
    int ViewByCritter;
    int ViewPlaceOnMap;
    int Accessory;
    int ChildObjects;

    struct
    {
        int MapId;
        int HexX;
        int HexY;
    } AccHex;

    struct
    {
        int Id;
        int Slot;
    } AccCritter;

    struct
    {
        int ContainerId;
        int StackId;
    } AccContainer;
};

struct Critter: public Entity
{
    #ifdef BIND_CLIENT
    int Pid;
    int CrType;
    int CrTypeAlias;
    int HexX, HexY;
    int CrDir;
    int Cond;
    int Anim1Life;
    int Anim1Knockout;
    int Anim1Dead;
    int Anim2Life;
    int Anim2Knockout;
    int Anim2Dead;
    int NameOnHead;
    int Avatar;
    int DialogId;
    int NameColor;
    int ContourColor;
    int Layers3d;
    #endif

    int Data;
    int Flags;
    int Name;
    int IsRunning;
};
typedef Critter CritterCl;

struct Map: public Entity
{
    // Empty
};

struct Location: public Entity
{
    int GeckCount;
};

struct ProtoMap
{
    void AddRef()  {}
    void Release() {}
};

struct BindClass
{
    #ifdef BIND_SERVER
    static void NpcPlane_GetCopy()        {}
    static void NpcPlane_SetChild()       {}
    static void NpcPlane_GetChild()       {}
    static void NpcPlane_Misc_SetScript() {}

    static void Item_AddItem()                {}
    static void Item_GetItems()               {}
    static void Item_SetScript()              {}
    static void Item_SetEvent()               {}
    static void Item_GetMapPosition()         {}
    static void Item_ChangeProto()            {}
    static void Item_Animate()                {}
    static void Item_CallStaticItemFunction() {}

    static void Crit_IsPlayer()                 {}
    static void Crit_IsNpc()                    {}
    static void Crit_IsCanWalk()                {}
    static void Crit_IsCanRun()                 {}
    static void Crit_IsCanRotate()              {}
    static void Crit_IsAnim1()                  {}
    static void Crit_IsAnim3d()                 {}
    static void Cl_GetAccess()                  {}
    static void Cl_SetAccess()                  {}
    static void Crit_SetLexems()                {}
    static void Crit_GetMap()                   {}
    static void Crit_MoveToDir()                {}
    static void Crit_TransitToHex()             {}
    static void Crit_TransitToMapHex()          {}
    static void Crit_TransitToGlobal()          {}
    static void Crit_TransitToGlobalWithGroup() {}
    static void Crit_TransitToGlobalGroup()     {}
    static void Crit_IsLife()                   {}
    static void Crit_IsKnockout()               {}
    static void Crit_IsDead()                   {}
    static void Crit_IsFree()                   {}
    static void Crit_IsBusy()                   {}
    static void Crit_Wait()                     {}
    static void Crit_RefreshVisible()           {}
    static void Crit_ViewMap()                  {}
    static void Crit_Say()                      {}
    static void Crit_SayMsg()                   {}
    static void Crit_SayMsgLex()                {}
    static void Crit_SetDir()                   {}
    static void Crit_GetCritters()              {}
    static void Npc_GetTalkedPlayers()          {}
    static void Crit_IsSeeCr()                  {}
    static void Crit_IsSeenByCr()               {}
    static void Crit_IsSeeItem()                {}
    static void Crit_CountItem()                {}
    static void Crit_DeleteItem()               {}
    static void Crit_AddItem()                  {}
    static void Crit_GetItem()                  {}
    static void Crit_GetItemPredicate()         {}
    static void Crit_GetItemByPid()             {}
    static void Crit_GetItemBySlot()            {}
    static void Crit_GetItems()                 {}
    static void Crit_GetItemsPredicate()        {}
    static void Crit_ChangeItemSlot()           {}
    static void Crit_DropItem()                 {}
    static void Crit_SetCond()                  {}
    static void Crit_CloseDialog()              {}

    static void Crit_SendCombatResult() {}
    static void Crit_Action()           {}
    static void Crit_Animate()          {}
    static void Crit_SetAnims()         {}
    static void Crit_PlaySound()        {}

    static void Crit_IsKnownLoc()    {}
    static void Crit_SetKnownLoc()   {}
    static void Crit_UnsetKnownLoc() {}
    static void Crit_SetFog()        {}
    static void Crit_GetFog()        {}

    static void Cl_SendItems()  {}
    static void Cl_Disconnect() {}

    static void Crit_SetScript() {}

    static void Crit_AddTimeEvent()       {}
    static void Crit_AddTimeEventRate()   {}
    static void Crit_GetTimeEvents()      {}
    static void Crit_GetTimeEventsArr()   {}
    static void Crit_ChangeTimeEvent()    {}
    static void Crit_EraseTimeEvent()     {}
    static void Crit_EraseTimeEvents()    {}
    static void Crit_EraseTimeEventsArr() {}

    static void Crit_MoveToCritter()    {}
    static void Crit_MoveToHex()        {}
    static void Crit_GetMovingState()   {}
    static void Crit_ResetMovingState() {}

    static void Map_GetLocation()                {}
    static void Map_SetScript()                  {}
    static void Map_AddItem()                    {}
    static void Map_GetItems()                   {}
    static void Map_GetItemsHex()                {}
    static void Map_GetItemsHexEx()              {}
    static void Map_GetItemsByPid()              {}
    static void Map_GetItemsPredicate()          {}
    static void Map_GetItemsHexPredicate()       {}
    static void Map_GetItemsHexRadiusPredicate() {}
    static void Map_GetItem()                    {}
    static void Map_GetItemHex()                 {}
    static void Map_GetStaticItem()              {}
    static void Map_GetStaticItemsHex()          {}
    static void Map_GetStaticItemsHexEx()        {}
    static void Map_GetStaticItemsByPid()        {}
    static void Map_GetStaticItemsPredicate()    {}
    static void Map_GetStaticItemsAll()          {}
    static void Map_GetCritterHex()              {}
    static void Map_GetCritterById()             {}
    static void Map_GetCritters()                {}
    static void Map_GetCrittersByPids()          {}
    static void Map_GetCrittersInPath()          {}
    static void Map_GetCrittersInPathBlock()     {}
    static void Map_GetCrittersWhoViewPath()     {}
    static void Map_GetCrittersSeeing()          {}
    static void Map_GetHexInPath()               {}
    static void Map_GetHexInPathWall()           {}
    static void Map_GetPathLengthHex()           {}
    static void Map_GetPathLengthCr()            {}
    static void Map_AddNpc()                     {}
    static void Map_GetNpcCount()                {}
    static void Map_GetNpc()                     {}
    static void Map_IsHexPassed()                {}
    static void Map_IsHexesPassed()              {}
    static void Map_IsHexRaked()                 {}
    static void Map_SetText()                    {}
    static void Map_SetTextMsg()                 {}
    static void Map_SetTextMsgLex()              {}
    static void Map_RunEffect()                  {}
    static void Map_RunFlyEffect()               {}
    static void Map_CheckPlaceForItem()          {}
    static void Map_BlockHex()                   {}
    static void Map_UnblockHex()                 {}
    static void Map_PlaySound()                  {}
    static void Map_PlaySoundRadius()            {}
    static void Map_Reload()                     {}
    static void Map_MoveHexByDir()               {}
    static void Map_VerifyTrigger()              {}

    static void Location_GetMapCount()   {}
    static void Location_GetMap()        {}
    static void Location_GetMapByIndex() {}
    static void Location_GetMaps()       {}
    static void Location_GetEntrance()   {}
    static void Location_GetEntrances()  {}
    static void Location_Reload()        {}

    static void Global_GetItem()                {}
    static void Global_GetCrittersDistantion()  {}
    static void Global_MoveItemCr()             {}
    static void Global_MoveItemMap()            {}
    static void Global_MoveItemCont()           {}
    static void Global_MoveItemsCr()            {}
    static void Global_MoveItemsMap()           {}
    static void Global_MoveItemsCont()          {}
    static void Global_DeleteItem()             {}
    static void Global_DeleteItemById()         {}
    static void Global_DeleteItems()            {}
    static void Global_DeleteItemsById()        {}
    static void Global_DeleteNpc()              {}
    static void Global_DeleteNpcById()          {}
    static void Global_RadioMessage()           {}
    static void Global_RadioMessageMsg()        {}
    static void Global_RadioMessageMsgLex()     {}
    static void Global_GetFullSecond()          {}
    static void Global_GetGameTime()            {}
    static void Global_CreateLocation()         {}
    static void Global_DeleteLocation()         {}
    static void Global_DeleteLocationById()     {}
    static void Global_GetCritter()             {}
    static void Global_GetPlayer()              {}
    static void Global_GetPlayerId()            {}
    static void Global_GetPlayerName()          {}
    static void Global_GetGlobalMapCritters()   {}
    static void Global_SetStartLocation()       {}
    static void Global_GetMap()                 {}
    static void Global_GetMapByPid()            {}
    static void Global_GetLocation()            {}
    static void Global_GetLocationByPid()       {}
    static void Global_GetLocations()           {}
    static void Global_GetVisibleLocations()    {}
    static void Global_GetZoneLocationIds()     {}
    static void Global_RunDialogNpc()           {}
    static void Global_RunDialogNpcDlgPack()    {}
    static void Global_RunDialogHex()           {}
    static void Global_WorldItemCount()         {}
    static void Global_AddTextListener()        {}
    static void Global_EraseTextListener()      {}
    static void Global_CreatePlane()            {}
    static void Global_SetChosenSendParameter() {}
    static void Global_SetSendParameter()       {}
    static void Global_SetSendParameterFunc()   {}
    static void Global_SwapCritters()           {}
    static void Global_GetAllItems()            {}
    static void Global_GetOnlinePlayers()       {}
    static void Global_GetRegisteredPlayerIds() {}
    static void Global_GetAllNpc()              {}
    static void Global_GetAllMaps()             {}
    static void Global_GetAllLocations()        {}
    static void Global_LoadImage()              {}
    static void Global_GetImageColor()          {}
    static void Global_SetTime()                {}
    static void Global_YieldWebRequest()        {}
    #endif

    #ifdef BIND_CLIENT
    static void Crit_IsChosen()             {}
    static void Crit_IsPlayer()             {}
    static void Crit_IsNpc()                {}
    static void Crit_IsOffline()            {}
    static void Crit_IsLife()               {}
    static void Crit_IsKnockout()           {}
    static void Crit_IsDead()               {}
    static void Crit_IsFree()               {}
    static void Crit_IsBusy()               {}
    static void Crit_IsAnim3d()             {}
    static void Crit_IsAnimAviable()        {}
    static void Crit_IsAnimPlaying()        {}
    static void Crit_GetAnim1()             {}
    static void Crit_Animate()              {}
    static void Crit_AnimateEx()            {}
    static void Crit_ClearAnim()            {}
    static void Crit_Wait()                 {}
    static void Crit_CountItem()            {}
    static void Crit_GetItem()              {}
    static void Crit_GetItemByPid()         {}
    static void Crit_GetItemBySlot()        {}
    static void Crit_GetItems()             {}
    static void Crit_SetVisible()           {}
    static void Crit_GetVisible()           {}
    static void Crit_set_ContourColor()     {}
    static void Crit_get_ContourColor()     {}
    static void Crit_GetNameTextInfo()      {}
    static void Crit_AddAnimationCallback() {}
    static void Crit_GetBonePosition()      {}

    static void Item_Clone()          {}
    static void Item_GetItems()       {}
    static void Item_GetMapPosition() {}
    static void Item_Animate()        {}

    static void Global_CustomCall()             {}
    static void Global_GetChosen()              {}
    static void Global_GetItem()                {}
    static void Global_GetMapAllItems()         {}
    static void Global_GetMapHexItems()         {}
    static void Global_GetCrittersDistantion()  {}
    static void Global_GetCritter()             {}
    static void Global_GetCritters()            {}
    static void Global_GetCrittersByPids()      {}
    static void Global_GetCrittersInPath()      {}
    static void Global_GetCrittersInPathBlock() {}
    static void Global_FlushScreen()            {}
    static void Global_QuakeScreen()            {}
    static void Global_PlaySound()              {}
    static void Global_PlayMusic()              {}
    static void Global_PlayVideo()              {}
    static void Global_GetCurrentMapPid()       {}
    static void Global_MessageType()            {}
    static void Global_MessageMsgType()         {}
    static void Global_FormatTags()             {}
    static void Global_LockScreenScroll()       {}
    static void Global_GetFog()                 {}
    static void Global_GetDayTime()             {}
    static void Global_GetDayColor()            {}
    static void Global_GetFullSecond()          {}
    static void Global_GetGameTime()            {}
    static void Global_Preload3dFiles()         {}
    static void Global_GetTileName()            {}

    static void Global_ShowScreen()        {}
    static void Global_HideScreen()        {}
    static void Global_GetMonitorItem()    {}
    static void Global_GetMonitorCritter() {}
    static void Global_GetMonitorEntity()  {}
    static void Global_GetMapWidth()       {}
    static void Global_GetMapHeight()      {}
    static void Global_IsMapHexPassed()    {}
    static void Global_IsMapHexRaked()     {}
    static void Global_WaitPing()          {}
    static void Global_SetEffect()         {}
    static void Global_RefreshMap()        {}
    static void Global_SaveScreenshot()    {}
    static void Global_SaveText()          {}
    static void Global_SetCacheData()      {}
    static void Global_SetCacheDataSize()  {}
    static void Global_GetCacheData()      {}
    static void Global_SetCacheDataStr()   {}
    static void Global_GetCacheDataStr()   {}
    static void Global_IsCacheData()       {}
    static void Global_EraseCacheData()    {}
    static void Global_SetUserConfig()     {}

    static void Global_ActivateOffscreenSurface()    {}
    static void Global_PresentOffscreenSurface()     {}
    static void Global_PresentOffscreenSurfaceExt()  {}
    static void Global_PresentOffscreenSurfaceExt2() {}

    static int  ClientCurMap;
    static int  ClientCurLocation;
    static int  IsConnected;
    static int  IsConnecting;
    static int  UpdateFilesInProgress;
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
    static void DataRef_Index() {}
    static void DataVal_Index() {}

    static void Global_GetTime()                                       {}
    static void Global_SetPropertyGetCallback( asIScriptGeneric* gen ) {}
    static void Global_AddPropertySetCallback( asIScriptGeneric* gen ) {}
    static void Global_GetGlobalMapRelief()                            {}
    #endif

    #ifdef BIND_MAPPER
    static void Item_AddChild()    {}
    static void Crit_AddChild()    {}
    static void Item_GetChildren() {}
    static void Crit_GetChildren() {}

    static void Global_AddItem()             {}
    static void Global_AddCritter()          {}
    static void Global_GetItemByHex()        {}
    static void Global_GetItemsByHex()       {}
    static void Global_GetCritterByHex()     {}
    static void Global_GetCrittersByHex()    {}
    static void Global_MoveEntity()          {}
    static void Global_DeleteEntity()        {}
    static void Global_DeleteEntities()      {}
    static void Global_SelectEntity()        {}
    static void Global_SelectEntities()      {}
    static void Global_GetSelectedEntity()   {}
    static void Global_GetSelectedEntities() {}

    static void Global_GetTilesCount() {}
    static void Global_DeleteTile()    {}
    static void Global_GetTileHash()   {}
    static void Global_AddTileHash()   {}
    static void Global_GetTileName()   {}
    static void Global_AddTileName()   {}

    static void Global_GetFastPrototypes() {}
    static void Global_SetFastPrototypes() {}
    static void Global_LoadMap()           {}
    static void Global_UnloadMap()         {}
    static void Global_SaveMap()           {}
    static void Global_ShowMap()           {}
    static void Global_GetLoadedMaps()     {}
    static void Global_GetMapFileNames()   {}
    static void Global_ResizeMap()         {}

    static void Global_TabGetTileDirs()    {}
    static void Global_TabGetItemPids()    {}
    static void Global_TabGetCritterPids() {}
    static void Global_TabSetTileDirs()    {}
    static void Global_TabSetItemPids()    {}
    static void Global_TabSetCritterPids() {}
    static void Global_TabDelete()         {}
    static void Global_TabSelect()         {}
    static void Global_TabSetName()        {}

    static void Global_GetIfaceIniStr()   {}
    static void Global_GetMonitorObject() {}

    static int  ClientCurMap;
    static int  ClientCurLocation;
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
    static void Global_GetHexInPath()       {}
    static void Global_GetPathHex()         {}
    static void Global_GetPathCr()          {}
    static void Global_GetPathLengthHex()   {}
    static void Global_GetPathLengthCr()    {}
    static void Global_Message()            {}
    static void Global_MessageMsg()         {}
    static void Global_MapMessage()         {}
    static void Global_GetMsgStr()          {}
    static void Global_GetMsgStrSkip()      {}
    static void Global_GetMsgStrNumUpper()  {}
    static void Global_GetMsgStrNumLower()  {}
    static void Global_GetMsgStrCount()     {}
    static void Global_IsMsgStr()           {}
    static void Global_ReplaceTextStr()     {}
    static void Global_ReplaceTextInt()     {}
    static void Global_MoveScreenToHex()    {}
    static void Global_MoveScreenOffset()   {}
    static void Global_MoveHexByDir()       {}
    static void Global_LoadSprite()         {}
    static void Global_LoadSpriteHash()     {}
    static void Global_GetSpriteWidth()     {}
    static void Global_GetSpriteHeight()    {}
    static void Global_GetSpriteCount()     {}
    static void Global_GetSpriteTicks()     {}
    static void Global_GetPixelColor()      {}
    static void Global_GetTextInfo()        {}
    static void Global_DrawSprite()         {}
    static void Global_DrawSpritePattern()  {}
    static void Global_DrawSpriteOffs()     {}
    static void Global_DrawSpriteSize()     {}
    static void Global_DrawSpriteSizeOffs() {}
    static void Global_DrawText()           {}
    static void Global_DrawPrimitive()      {}
    static void Global_DrawMapSprite()      {}
    static void Global_DrawCritter2d()      {}
    static void Global_DrawCritter3d()      {}
    static void Global_PushDrawScissor()    {}
    static void Global_PopDrawScissor()     {}
    static void Global_GetKeybLang()        {}
    static void Global_GetHexPos()          {}
    static void Global_GetMonitorHex()      {}
    static void Global_LoadFont()           {}
    static void Global_SetDefaultFont()     {}
    static void Global_MouseClick()         {}
    static void Global_KeyboardPress()      {}
    static void Global_SetRainAnimation()   {}
    static void Global_ChangeZoom()         {}
    #endif

    static void Global_LoadDataFile()  {}
    static void Global_AllowSlot()     {}
    static void Global_GetProtoItem()  {}
    static void Global_GetDistantion() {}
};

#ifdef BIND_CLIENT
int BindClass::ClientCurMap;
int BindClass::ClientCurLocation;
int BindClass::IsConnected;
int BindClass::IsConnecting;
int BindClass::UpdateFilesInProgress;
#endif

#ifdef BIND_MAPPER
int BindClass::ClientCurMap;
int BindClass::ClientCurLocation;
#endif
