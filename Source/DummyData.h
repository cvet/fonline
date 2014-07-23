// Dummy data

struct SyncObject
{
    int* SyncMngr;

    void Lock() {}
};

struct AIDataPlane
{
    int Type;
    int Priority;
    int Identifier;
    int IdentifierExt;

    struct
    {
        int WaitSecond;
        int ScriptBindId;
    } Misc;

    struct
    {
        int TargId;
        int MinHp;
        int IsGag;
        int GagHexX, GagHexY;
        int LastHexX, LastHexY;
        int IsRun;
    } Attack;

    struct
    {
        int HexX;
        int HexY;
        int Dir;
        int IsRun;
        int Cut;
    } Walk;

    struct
    {
        int HexX;
        int HexY;
        int Pid;
        int UseItemId;
        int ToOpen;
        int IsRun;
    } Pick;

    int  RefCounter;

    void AddRef()  {}
    void Release() {}
};

struct GameVar
{
    GameVar& operator+=( const int )      { return *this; }
    GameVar& operator-=( const int )      { return *this; }
    GameVar& operator*=( const int )      { return *this; }
    GameVar& operator/=( const int )      { return *this; }
    GameVar& operator=( const int )       { return *this; }
    GameVar& operator+=( const GameVar& ) { return *this; }
    GameVar& operator-=( const GameVar& ) { return *this; }
    GameVar& operator*=( const GameVar& ) { return *this; }
    GameVar& operator/=( const GameVar& ) { return *this; }
    GameVar& operator=( const GameVar& )  { return *this; }

    void GetValue()    {}
    void GetMin()      {}
    void GetMax()      {}
    void IsQuest()     {}
    void GetQuestStr() {}

    void AddRef()  {}
    void Release() {}
};

void GameVarAddInt()       {}
void GameVarSubInt()       {}
void GameVarMulInt()       {}
void GameVarDivInt()       {}
void GameVarAddGameVar()   {}
void GameVarSubGameVar()   {}
void GameVarMulGameVar()   {}
void GameVarDivGameVar()   {}
void GameVarEqualInt()     {}
void GameVarCmpInt()       {}
void GameVarEqualGameVar() {}
void GameVarCmpGameVar()   {}

struct ProtoItem
{
    int  ProtoId;
    int  PicMap;
    int  PicInv;
    int  Type;
    int  Stackable;
    int  Deteriorable;
    int  GroundLevel;
    int  Slot;
    int  Flags;
    int  Corner;
    int  DisableEgg;
    int  Dir;
    int  Weight;
    int  Volume;
    int  Cost;
    int  StartCount;
    int  SoundId;
    int  Material;
    int  LightFlags;
    int  LightDistance;
    int  LightIntensity;
    int  LightColor;
    int  AnimWaitBase;
    int  AnimWaitRndMin;
    int  AnimWaitRndMax;
    int  AnimStay[ 2 ];
    int  AnimShow[ 2 ];
    int  AnimHide[ 2 ];
    int  OffsetX;
    int  OffsetY;
    int  SpriteCut;
    int  DrawOrderOffsetHexY;
    int  RadioChannel;
    int  RadioFlags;
    int  RadioBroadcastSend;
    int  RadioBroadcastRecv;
    int  IndicatorStart;
    int  IndicatorMax;
    int  HolodiskNum;
    int  StartValue[ 10 ];
    int  BlockLines[ 10 ];
    int  ChildPid[ 10 ];
    int  ChildLines[ 10 ][ 10 ];
    int  UserData[ 100 ];
    int  Weapon_IsUnarmed;
    int  Weapon_UnarmedTree;
    int  Weapon_UnarmedPriority;
    int  Weapon_UnarmedMinAgility;
    int  Weapon_UnarmedMinUnarmed;
    int  Weapon_UnarmedMinLevel;
    int  Weapon_Anim1;
    int  Weapon_MaxAmmoCount;
    int  Weapon_Caliber;
    int  Weapon_DefaultAmmoPid;
    int  Weapon_MinStrength;
    int  Weapon_Perk;
    int  Weapon_ActiveUses;
    int  Weapon_Skill[ 3 ];
    int  Weapon_PicUse[ 3 ];
    int  Weapon_MaxDist[ 3 ];
    int  Weapon_Round[ 3 ];
    int  Weapon_ApCost[ 3 ];
    int  Weapon_Aim[ 3 ];
    int  Weapon_SoundId[ 3 ];
    int  Ammo_Caliber;
    int  Container_Volume;
    int  Container_CannotPickUp;
    int  Container_MagicHandsGrnd;
    int  Container_Changeble;
    int  Door_NoBlockMove;
    int  Door_NoBlockShoot;
    int  Door_NoBlockLight;
    int  Locker_Condition;
    int  Grid_Type;
    int  Car_Speed;
    int  Car_Passability;
    int  Car_DeteriorationRate;
    int  Car_CrittersCapacity;
    int  Car_TankVolume;
    int  Car_MaxDeterioration;
    int  Car_FuelConsumption;
    int  Car_Entrance;
    int  Car_MovementType;

    void AddRef()  {}
    void Release() {}
};

struct Item
{
    int Id;
    int Proto;
    int ViewByCritter;
    int ViewPlaceOnMap;
    int Accessory;
    int Reserved00;
    int ChildObjects;
    int Reserved11;
    int Lexems;
    int IsNotValid;

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

    struct ItemData
    {
        int SortValue;
        int Info;
        int Indicator;
        int PicMapHash;
        int PicInvHash;
        int AnimWaitBase;
        int AnimStay[ 2 ];
        int AnimShow[ 2 ];
        int AnimHide[ 2 ];
        int Flags;
        int Mode;
        int LightIntensity;
        int LightDistance;
        int LightFlags;
        int LightColor;
        int ScriptId;
        int TrapValue;
        int Count;
        int Cost;
        int ScriptValues[ 10 ];
        int BrokenFlags;
        int BrokenCount;
        int Deterioration;
        int AmmoPid;
        int AmmoCount;
        int LockerId;
        int LockerCondition;
        int LockerComplexity;
        int HolodiskNumber;
        int RadioChannel;
        int RadioFlags;
        int RadioBroadcastSend;
        int RadioBroadcastRecv;
        int Charge;
        int OffsetX;
        int OffsetY;
        int Dir;
        int Reserved[ 10 ];
    } Data;

    int  RefCounter;

    void AddRef()  {}
    void Release() {}
};

struct MapObject
{
    int MapObjType;
    int ProtoId;
    int MapX;
    int MapY;

    int UID;
    int ContainerUID;
    int ParentUID;
    int ParentChildIndex;

    int LightColor;
    int LightDay;
    int LightDirOff;
    int LightDistance;
    int LightIntensity;

    int UserData[ 10 ];

    struct
    {
        int Dir;
        int Cond;
        int Anim1;
        int Anim2;
        int Params[ 1000 ];
    } MCritter;

    struct
    {
        int OffsetX;
        int OffsetY;
        int AnimStayBegin;
        int AnimStayEnd;
        int AnimWait;
        int PicMapDeprecated;
        int PicInvDeprecated;
        int InfoOffset;
        int Reserved[ 3 ];
        int PicMapHash;
        int PicInvHash;

        int Count;

        int BrokenFlags;
        int BrokenCount;
        int Deterioration;

        int InContainer;
        int ItemSlot;

        int AmmoPid;
        int AmmoCount;

        int LockerDoorId;
        int LockerCondition;
        int LockerComplexity;

        int TrapValue;

        int Val[ 10 ];
    } MItem;

    struct
    {
        int OffsetX;
        int OffsetY;
        int AnimStayBegin;
        int AnimStayEnd;
        int AnimWait;
        int PicMapDeprecated;
        int PicInvDeprecated;
        int InfoOffset;
        int Reserved[ 3 ];
        int PicMapHash;
        int PicInvHash;

        int CanUse;
        int CanTalk;
        int TriggerNum;

        int ParamsCount;
        int Param[ 5 ];

        int ToMapPid;
        int ToEntire;
        int ToMapX;
        int ToMapY;
        int ToDir;

        int SpriteCut;
    } MScenery;

    void AddRef()  {}
    void Release() {}
};

struct CritData
{
    int Id;
    int HexX;
    int HexY;
    int WorldX;
    int WorldY;
    int BaseType;
    int Dir;
    int Cond;
    int Anim1Life;
    int Anim1Knockout;
    int Anim1Dead;
    int Anim2Life;
    int Anim2Knockout;
    int Anim2Dead;
    int ShowCritterDist1;
    int ShowCritterDist2;
    int ShowCritterDist3;
};

struct Critter
{
    #ifdef BIND_CLIENT
    int  Pid;
    int  BaseType;
    int  BaseTypeAlias;
    int  HexX, HexY;
    int  CrDir;
    int  Cond;
    int  Anim1Life;
    int  Anim1Knockout;
    int  Anim1Dead;
    int  Anim2Life;
    int  Anim2Knockout;
    int  Anim2Dead;
    int  Name;
    int  NameOnHead;
    int  Lexems;
    int  Avatar;
    int  DialogId;
    int  NameColor;
    int  ContourColor;
    int  Layers3d;
    #endif

    int  Data;
    int  Id;
    int  Flags;
    int  NameStr;
    int  IsRuning;
    int  IsNotValid;
    int  ThisPtr[ 1 ];
    int  RefCounter;

    void AddRef()  {}
    void Release() {}
};
typedef Critter CritterCl;

struct Map
{
    struct MapData
    {
        int MapId;
        int MapPid;
        int MapRain;
        int IsTurnBasedAviable;
        int MapTime;
        int ScriptId;
        int MapDayTime[ 4 ];
        int MapDayColor[ 12 ];
    } Data;

    int  TurnBasedRound;
    int  TurnBasedTurn;
    int  TurnBasedWholeTurn;

    int  IsNotValid;
    int  RefCounter;

    void AddRef()  {}
    void Release() {}
};

struct Location
{
    struct LocData
    {
        int LocId;
        int LocPid;
        int WX;
        int WY;
        int Radius;
        int Visible;
        int GeckVisible;
        int AutoGarbage;
        int Reserved0;
        int Color;
        int Reserved3[ 10 ];
        int MapIds[ 1 ];
    } Data;

    int  GeckCount;
    int  IsNotValid;
    int  RefCounter;

    void AddRef()  {}
    void Release() {}
};

struct ProtoMap
{
    struct
    {
        int MaxHexX;
        int MaxHexY;
        int WorkHexX;
        int WorkHexY;
        int Time;
        int NoLogOut;
    } Header;

    void AddRef()  {}
    void Release() {}
};

struct CraftItem
{
    int  Num;
    int  Name;
    int  Info;
    int  Experience;
    int  Script;

    void AddRef()  {}
    void Release() {}
};

struct BindClass
{
    #ifdef BIND_SERVER
    static void ProtoItem_GetScriptName() {}

    static void Synchronizer_Constructor( void* ) {}
    static void Synchronizer_Destructor( void* )  {}

    static void NpcPlane_GetCopy()        {}
    static void NpcPlane_SetChild()       {}
    static void NpcPlane_GetChild()       {}
    static void NpcPlane_Misc_SetScript() {}

    static void Container_AddItem()  {}
    static void Container_GetItems() {}
    static void Container_GetItem()  {}

    static void Item_IsStackable()    {}
    static void Item_IsDeteriorable() {}
    static void Item_SetScript()      {}
    static void Item_GetScriptId()    {}
    static void Item_SetEvent()       {}
    static void Item_GetType()        {}
    static void Item_GetProtoId()     {}
    static void Item_GetCount()       {}
    static void Item_SetCount()       {}
    static void Item_GetCost()        {}
    static void Item_GetMapPosition() {}
    static void Item_ChangeProto()    {}
    static void Item_Update()         {}
    static void Item_Animate()        {}
    static void Item_SetLexems()      {}
    static void Item_GetChild()       {}
    static void Item_LockerOpen()     {}
    static void Item_LockerClose()    {}

    static void Item_EventFinish()  {}
    static void Item_EventAttack()  {}
    static void Item_EventUse()     {}
    static void Item_EventUseOnMe() {}
    static void Item_EventSkill()   {}
    static void Item_EventDrop()    {}
    static void Item_EventMove()    {}
    static void Item_EventWalk()    {}

    static void Item_set_Flags()     {}
    static void Item_get_Flags()     {}
    static void Item_set_TrapValue() {}
    static void Item_get_TrapValue() {}

    static void CraftItem_GetShowParams() {}
    static void CraftItem_GetNeedParams() {}
    static void CraftItem_GetNeedTools()  {}
    static void CraftItem_GetNeedItems()  {}
    static void CraftItem_GetOutItems()   {}

    static void Scen_CallSceneryFunction() {}

    static void Crit_IsPlayer()                 {}
    static void Crit_IsNpc()                    {}
    static void Crit_IsCanWalk()                {}
    static void Crit_IsCanRun()                 {}
    static void Crit_IsCanRotate()              {}
    static void Crit_IsCanAim()                 {}
    static void Crit_IsAnim1()                  {}
    static void Crit_IsAnim3d()                 {}
    static void Cl_GetAccess()                  {}
    static void Cl_SetAccess()                  {}
    static void Crit_SetLexems()                {}
    static void Crit_SetEvent()                 {}
    static void Crit_GetMap()                   {}
    static void Crit_GetMapId()                 {}
    static void Crit_GetMapProtoId()            {}
    static void Crit_SetHomePos()               {}
    static void Crit_GetHomePos()               {}
    static void Crit_ChangeCrType()             {}
    static void Cl_DropTimers()                 {}
    static void Crit_MoveRandom()               {}
    static void Crit_MoveToDir()                {}
    static void Crit_TransitToHex()             {}
    static void Crit_TransitToMapHex()          {}
    static void Crit_TransitToMapEntire()       {}
    static void Crit_TransitToGlobal()          {}
    static void Crit_TransitToGlobalWithGroup() {}
    static void Crit_TransitToGlobalGroup()     {}
    static void Crit_IsLife()                   {}
    static void Crit_IsKnockout()               {}
    static void Crit_IsDead()                   {}
    static void Crit_IsFree()                   {}
    static void Crit_IsBusy()                   {}
    static void Crit_Wait()                     {}
    static void Crit_ToDead()                   {}
    static void Crit_ToLife()                   {}
    static void Crit_ToKnockout()               {}
    static void Crit_RefreshVisible()           {}
    static void Crit_ViewMap()                  {}
    static void Crit_AddScore()                 {}
    static void Crit_GetScore()                 {}
    static void Crit_AddHolodiskInfo()          {}
    static void Crit_EraseHolodiskInfo()        {}
    static void Crit_IsHolodiskInfo()           {}
    static void Crit_Say()                      {}
    static void Crit_SayMsg()                   {}
    static void Crit_SayMsgLex()                {}
    static void Crit_SetDir()                   {}
    static void Crit_PickItem()                 {}
    static void Crit_SetFavoriteItem()          {}
    static void Crit_GetFavoriteItem()          {}
    static void Crit_GetCritters()              {}
    static void Crit_GetFollowGroup()           {}
    static void Crit_GetFollowLeader()          {}
    static void Crit_GetGlobalGroup()           {}
    static void Crit_IsGlobalGroupLeader()      {}
    static void Crit_LeaveGlobalGroup()         {}
    static void Crit_GiveGlobalGroupLead()      {}
    static void Npc_GetTalkedPlayers()          {}
    static void Crit_IsSeeCr()                  {}
    static void Crit_IsSeenByCr()               {}
    static void Crit_IsSeeItem()                {}
    static void Crit_AddItem()                  {}
    static void Crit_DeleteItem()               {}
    static void Crit_ItemsCount()               {}
    static void Crit_ItemsWeight()              {}
    static void Crit_ItemsVolume()              {}
    static void Crit_CountItem()                {}
    static void Crit_GetItem()                  {}
    static void Crit_GetItemById()              {}
    static void Crit_GetItems()                 {}
    static void Crit_GetItemsByType()           {}
    static void Crit_GetSlotProto()             {}
    static void Crit_MoveItem()                 {}

    static void Npc_ErasePlane()           {}
    static void Npc_ErasePlaneIndex()      {}
    static void Npc_DropPlanes()           {}
    static void Npc_IsNoPlanes()           {}
    static void Npc_IsCurPlane()           {}
    static void Npc_GetCurPlane()          {}
    static void Npc_GetPlanes()            {}
    static void Npc_GetPlanesIdentifier()  {}
    static void Npc_GetPlanesIdentifier2() {}
    static void Npc_AddPlane()             {}

    static void Crit_SendMessage()      {}
    static void Crit_SendCombatResult() {}
    static void Crit_Action()           {}
    static void Crit_Animate()          {}
    static void Crit_SetAnims()         {}
    static void Crit_PlaySound()        {}
    static void Crit_PlaySoundType()    {}

    static void Cl_IsKnownLoc()      {}
    static void Cl_SetKnownLoc()     {}
    static void Cl_UnsetKnownLoc()   {}
    static void Cl_SetFog()          {}
    static void Cl_GetFog()          {}
    static void Crit_SetTimeout()    {}
    static void Crit_GetParam()      {}
    static void Cl_ShowContainer()   {}
    static void Cl_ShowScreen()      {}
    static void Cl_RunClientScript() {}
    static void Cl_Disconnect()      {}

    static void Crit_SetScript()         {}
    static void Crit_GetScriptId()       {}
    static void Crit_SetBagRefreshTime() {}
    static void Crit_GetBagRefreshTime() {}
    static void Crit_SetInternalBag()    {}
    static void Crit_GetInternalBag()    {}
    static void Crit_GetProtoId()        {}
    static void Crit_GetMultihex()       {}
    static void Crit_SetMultihex()       {}

    static void Crit_AddEnemyInStack()      {}
    static void Crit_CheckEnemyInStack()    {}
    static void Crit_EraseEnemyFromStack()  {}
    static void Crit_ChangeEnemyStackSize() {}
    static void Crit_GetEnemyStack()        {}
    static void Crit_ClearEnemyStack()      {}
    static void Crit_ClearEnemyStackNpc()   {}

    static void Crit_AddTimeEvent()       {}
    static void Crit_AddTimeEventRate()   {}
    static void Crit_GetTimeEvents()      {}
    static void Crit_GetTimeEventsArr()   {}
    static void Crit_ChangeTimeEvent()    {}
    static void Crit_EraseTimeEvent()     {}
    static void Crit_EraseTimeEvents()    {}
    static void Crit_EraseTimeEventsArr() {}

    static void Crit_EventIdle()                 {}
    static void Crit_EventFinish()               {}
    static void Crit_EventDead()                 {}
    static void Crit_EventRespawn()              {}
    static void Crit_EventShowCritter()          {}
    static void Crit_EventShowCritter1()         {}
    static void Crit_EventShowCritter2()         {}
    static void Crit_EventShowCritter3()         {}
    static void Crit_EventHideCritter()          {}
    static void Crit_EventHideCritter1()         {}
    static void Crit_EventHideCritter2()         {}
    static void Crit_EventHideCritter3()         {}
    static void Crit_EventShowItemOnMap()        {}
    static void Crit_EventChangeItemOnMap()      {}
    static void Crit_EventHideItemOnMap()        {}
    static void Crit_EventAttack()               {}
    static void Crit_EventAttacked()             {}
    static void Crit_EventStealing()             {}
    static void Crit_EventMessage()              {}
    static void Crit_EventUseItem()              {}
    static void Crit_EventUseItemOnMe()          {}
    static void Crit_EventUseSkill()             {}
    static void Crit_EventUseSkillOnMe()         {}
    static void Crit_EventDropItem()             {}
    static void Crit_EventMoveItem()             {}
    static void Crit_EventKnockout()             {}
    static void Crit_EventSmthDead()             {}
    static void Crit_EventSmthStealing()         {}
    static void Crit_EventSmthAttack()           {}
    static void Crit_EventSmthAttacked()         {}
    static void Crit_EventSmthUseItem()          {}
    static void Crit_EventSmthUseSkill()         {}
    static void Crit_EventSmthDropItem()         {}
    static void Crit_EventSmthMoveItem()         {}
    static void Crit_EventSmthKnockout()         {}
    static void Crit_EventPlaneBegin()           {}
    static void Crit_EventPlaneEnd()             {}
    static void Crit_EventPlaneRun()             {}
    static void Crit_EventBarter()               {}
    static void Crit_EventTalk()                 {}
    static void Crit_EventGlobalProcess()        {}
    static void Crit_EventGlobalInvite()         {}
    static void Crit_EventTurnBasedProcess()     {}
    static void Crit_EventSmthTurnBasedProcess() {}

    static void Global_GetGlobalVar() {}
    static void Global_GetLocalVar()  {}
    static void Global_GetUnicumVar() {}

    static void Map_GetProtoId()               {}
    static void Map_GetLocation()              {}
    static void Map_SetScript()                {}
    static void Map_GetScriptId()              {}
    static void Map_SetEvent()                 {}
    static void Map_SetLoopTime()              {}
    static void Map_GetRain()                  {}
    static void Map_SetRain()                  {}
    static void Map_GetTime()                  {}
    static void Map_SetTime()                  {}
    static void Map_GetDayTime()               {}
    static void Map_SetDayTime()               {}
    static void Map_GetDayColor()              {}
    static void Map_SetDayColor()              {}
    static void Map_SetTurnBasedAvailability() {}
    static void Map_IsTurnBasedAvailability()  {}
    static void Map_BeginTurnBased()           {}
    static void Map_IsTurnBased()              {}
    static void Map_EndTurnBased()             {}
    static void Map_GetTurnBasedSequence()     {}
    static void Map_SetData()                  {}
    static void Map_GetData()                  {}
    static void Map_AddItem()                  {}
    static void Map_GetItemsHex()              {}
    static void Map_GetItemsHexEx()            {}
    static void Map_GetItemsByPid()            {}
    static void Map_GetItemsByType()           {}
    static void Map_GetItem()                  {}
    static void Map_GetItemHex()               {}
    static void Map_GetDoor()                  {}
    static void Map_GetCar()                   {}
    static void Map_GetSceneryHex()            {}
    static void Map_GetSceneriesHex()          {}
    static void Map_GetSceneriesHexEx()        {}
    static void Map_GetSceneriesByPid()        {}
    static void Map_GetCritterHex()            {}
    static void Map_GetCritterById()           {}
    static void Map_GetCritters()              {}
    static void Map_GetCrittersByPids()        {}
    static void Map_GetCrittersInPath()        {}
    static void Map_GetCrittersInPathBlock()   {}
    static void Map_GetCrittersWhoViewPath()   {}
    static void Map_GetCrittersSeeing()        {}
    static void Map_GetHexInPath()             {}
    static void Map_GetHexInPathWall()         {}
    static void Map_GetPathLengthHex()         {}
    static void Map_GetPathLengthCr()          {}
    static void Map_AddNpc()                   {}
    static void Map_GetNpcCount()              {}
    static void Map_GetNpc()                   {}
    static void Map_CountEntire()              {}
    static void Map_GetEntires()               {}
    static void Map_GetEntireCoords()          {}
    static void Map_GetEntireCoordsDir()       {}
    static void Map_GetNearEntireCoords()      {}
    static void Map_GetNearEntireCoordsDir()   {}
    static void Map_IsHexPassed()              {}
    static void Map_IsHexRaked()               {}
    static void Map_SetText()                  {}
    static void Map_SetTextMsg()               {}
    static void Map_SetTextMsgLex()            {}
    static void Map_RunEffect()                {}
    static void Map_RunFlyEffect()             {}
    static void Map_CheckPlaceForItem()        {}
    static void Map_BlockHex()                 {}
    static void Map_UnblockHex()               {}
    static void Map_PlaySound()                {}
    static void Map_PlaySoundRadius()          {}
    static void Map_Reload()                   {}
    static void Map_GetWidth()                 {}
    static void Map_GetHeight()                {}
    static void Map_MoveHexByDir()             {}
    static void Map_VerifyTrigger()            {}

    static void Map_EventFinish()           {}
    static void Map_EventLoop0()            {}
    static void Map_EventLoop1()            {}
    static void Map_EventLoop2()            {}
    static void Map_EventLoop3()            {}
    static void Map_EventLoop4()            {}
    static void Map_EventInCritter()        {}
    static void Map_EventOutCritter()       {}
    static void Map_EventCritterDead()      {}
    static void Map_EventTurnBasedBegin()   {}
    static void Map_EventTurnBasedEnd()     {}
    static void Map_EventTurnBasedProcess() {}

    static void Location_GetProtoId()    {}
    static void Location_SetEvent()      {}
    static void Location_GetMapCount()   {}
    static void Location_GetMap()        {}
    static void Location_GetMapByIndex() {}
    static void Location_GetMaps()       {}
    static void Location_Reload()        {}
    static void Location_Update()        {}

    static void Location_EventFinish() {}
    static void Location_EventEnter()  {}

    static void Global_GetItem()                {}
    static void Global_GetCrittersDistantion()  {}
    static void Global_MoveItemCr()             {}
    static void Global_MoveItemMap()            {}
    static void Global_MoveItemCont()           {}
    static void Global_MoveItemsCr()            {}
    static void Global_MoveItemsMap()           {}
    static void Global_MoveItemsCont()          {}
    static void Global_DeleteItem()             {}
    static void Global_DeleteItems()            {}
    static void Global_DeleteNpc()              {}
    static void Global_RadioMessage()           {}
    static void Global_RadioMessageMsg()        {}
    static void Global_RadioMessageMsgLex()     {}
    static void Global_GetFullSecond()          {}
    static void Global_GetGameTime()            {}
    static void Global_CreateLocation()         {}
    static void Global_DeleteLocation()         {}
    static void Global_GetProtoCritter()        {}
    static void Global_GetCritter()             {}
    static void Global_GetPlayer()              {}
    static void Global_GetPlayerId()            {}
    static void Global_GetPlayerName()          {}
    static void Global_GetGlobalMapCritters()   {}
    static void Global_CreateTimeEventEmpty()   {}
    static void Global_CreateTimeEventValue()   {}
    static void Global_CreateTimeEventValues()  {}
    static void Global_EraseTimeEvent()         {}
    static void Global_GetTimeEvent()           {}
    static void Global_SetTimeEvent()           {}
    static void Global_SetAnyData()             {}
    static void Global_SetAnyDataSize()         {}
    static void Global_GetAnyData()             {}
    static void Global_IsAnyData()              {}
    static void Global_EraseAnyData()           {}
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
    static void Global_SetBestScore()           {}
    static void Global_AddTextListener()        {}
    static void Global_EraseTextListener()      {}
    static void Global_CreatePlane()            {}
    static void Global_GetBagItems()            {}
    static void Global_SetChosenSendParameter() {}
    static void Global_SetSendParameter()       {}
    static void Global_SetSendParameterFunc()   {}
    static void Global_SwapCritters()           {}
    static void Global_GetAllItems()            {}
    static void Global_GetAllPlayers()          {}
    static void Global_GetRegisteredPlayers()   {}
    static void Global_GetAllNpc()              {}
    static void Global_GetAllMaps()             {}
    static void Global_GetAllLocations()        {}
    static void Global_GetScriptId()            {}
    static void Global_GetScriptName()          {}
    static void Global_GetItemDataMask()        {}
    static void Global_SetItemDataMask()        {}
    static void Global_LoadImage()              {}
    static void Global_GetImageColor()          {}
    static void Global_Synchronize()            {}
    static void Global_Resynchronize()          {}
    static void Global_SetTime()                {}
    #endif

    #ifdef BIND_CLIENT
    static void Crit_IsChosen()         {}
    static void Crit_IsPlayer()         {}
    static void Crit_IsNpc()            {}
    static void Crit_IsLife()           {}
    static void Crit_IsKnockout()       {}
    static void Crit_IsDead()           {}
    static void Crit_IsFree()           {}
    static void Crit_IsBusy()           {}
    static void Crit_IsAnim3d()         {}
    static void Crit_IsAnimAviable()    {}
    static void Crit_IsAnimPlaying()    {}
    static void Crit_GetAnim1()         {}
    static void Crit_Animate()          {}
    static void Crit_AnimateEx()        {}
    static void Crit_ClearAnim()        {}
    static void Crit_Wait()             {}
    static void Crit_ItemsCount()       {}
    static void Crit_ItemsWeight()      {}
    static void Crit_ItemsVolume()      {}
    static void Crit_CountItem()        {}
    static void Crit_GetItem()          {}
    static void Crit_GetItems()         {}
    static void Crit_GetItemsByType()   {}
    static void Crit_GetSlotProto()     {}
    static void Crit_SetVisible()       {}
    static void Crit_GetVisible()       {}
    static void Crit_set_ContourColor() {}
    static void Crit_get_ContourColor() {}
    static void Crit_GetMultihex()      {}
    static void Crit_IsTurnBasedTurn()  {}
    static void Crit_GetNameTextInfo()  {}

    static void Item_IsStackable()    {}
    static void Item_IsDeteriorable() {}
    static void Item_GetScriptId()    {}
    static void Item_GetType()        {}
    static void Item_GetProtoId()     {}
    static void Item_GetCount()       {}
    static void Item_GetMapPosition() {}
    static void Item_Animate()        {}
    static void Item_GetChild()       {}

    static void Global_CustomCall()                 {}
    static void Global_GetChosen()                  {}
    static void Global_GetChosenActions()           {}
    static void Global_SetChosenActions()           {}
    static void Global_GetItem()                    {}
    static void Global_GetCrittersDistantion()      {}
    static void Global_GetCritter()                 {}
    static void Global_GetCritters()                {}
    static void Global_GetCrittersByPids()          {}
    static void Global_GetCrittersInPath()          {}
    static void Global_GetCrittersInPathBlock()     {}
    static void Global_FlushScreen()                {}
    static void Global_QuakeScreen()                {}
    static void Global_PlaySound()                  {}
    static void Global_PlaySoundType()              {}
    static void Global_PlayMusic()                  {}
    static void Global_PlayVideo()                  {}
    static void Global_IsTurnBased()                {}
    static void Global_GetTurnBasedTime()           {}
    static void Global_GetCurrentMapPid()           {}
    static void Global_GetMessageFilters()          {}
    static void Global_SetMessageFilters()          {}
    static void Global_MessageType()                {}
    static void Global_MessageMsgType()             {}
    static void Global_FormatTags()                 {}
    static void Global_GetSomeValue()               {}
    static void Global_LockScreenScroll()           {}
    static void Global_GetFog()                     {}
    static void Global_RefreshItemsCollection()     {}
    static void Global_GetScroll()                  {}
    static void Global_SetScroll()                  {}
    static void Global_GetDayTime()                 {}
    static void Global_GetDayColor()                {}
    static void Global_GetFullSecond()              {}
    static void Global_GetGameTime()                {}
    static void Global_Preload3dFiles()             {}
    static void Global_RunServerScript()            {}
    static void Global_RunServerScriptUnsafe()      {}
    static void Global_ShowScreen()                 {}
    static void Global_HideScreen()                 {}
    static void Global_GetHardcodedScreenPos()      {}
    static void Global_DrawHardcodedScreen()        {}
    static void Global_HandleHardcodedScreenMouse() {}
    static void Global_HandleHardcodedScreenKey()   {}
    static void Global_GetMonitorItem()             {}
    static void Global_GetMonitorCritter()          {}
    static void Global_GetMapWidth()                {}
    static void Global_GetMapHeight()               {}
    static void Global_GetCurrentCursor()           {}
    static void Global_GetLastCursor()              {}
    static void Global_ChangeCursor()               {}
    static void Global_WaitPing()                   {}
    static void Global_SetEffect()                  {}
    static void Global_RefreshMap()                 {}
    static void Global_SaveScreenshot()             {}
    static void Global_SaveLogFile()                {}
    static void Global_SetCacheData()               {}
    static void Global_SetCacheDataSize()           {}
    static void Global_GetCacheData()               {}
    static void Global_SetCacheDataStr()            {}
    static void Global_GetCacheDataStr()            {}
    static void Global_IsCacheData()                {}
    static void Global_EraseCacheData()             {}

    static int  ConsoleActive;
    static int  GmapActive, GmapWait;
    static int  GmapZoom;
    static int  GmapOffsetX, GmapOffsetY;
    static int  GmapGroupCurX, GmapGroupCurY, GmapGroupToX, GmapGroupToY;
    static int  GmapGroupSpeed;
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
    static void DataRef_Index() {}
    static void DataVal_Index() {}

    static void Global_GetTime()                        {}
    static void Global_SetParameterGetBehaviour()       {}
    static void Global_SetParameterChangeBehaviour()    {}
    static void Global_SetParameterDialogGetBehaviour() {}
    static void Global_SetRegistrationParam()           {}
    static void Global_IsCritterCanWalk()               {}
    static void Global_IsCritterCanRun()                {}
    static void Global_IsCritterCanRotate()             {}
    static void Global_IsCritterCanAim()                {}
    static void Global_IsCritterCanArmor()              {}
    static void Global_IsCritterAnim1()                 {}
    static void Global_GetCritterAnimType()             {}
    static void Global_GetCritterAlias()                {}
    static void Global_GetCritterTypeName()             {}
    static void Global_GetCritterSoundName()            {}
    static void Global_GetGlobalMapRelief()             {}
    static void Global_GetCraftItem()                   {}
    #endif

    #ifdef BIND_MAPPER
    static void MapperObject_CritterParam_Index() {}
    static void MapperObject_get_ScriptName()     {}
    static void MapperObject_set_ScriptName()     {}
    static void MapperObject_get_FuncName()       {}
    static void MapperObject_set_FuncName()       {}
    static void MapperObject_get_Critter_Cond()   {}
    static void MapperObject_set_Critter_Cond()   {}
    static void MapperObject_get_PicMap()         {}
    static void MapperObject_set_PicMap()         {}
    static void MapperObject_get_PicInv()         {}
    static void MapperObject_set_PicInv()         {}
    static void MapperObject_Update()             {}
    static void MapperObject_AddChild()           {}
    static void MapperObject_GetChilds()          {}
    static void MapperObject_MoveToHex()          {}
    static void MapperObject_MoveToHexOffset()    {}
    static void MapperObject_MoveToDir()          {}

    static void MapperMap_get_Name()         {}
    static void MapperMap_AddObject()        {}
    static void MapperMap_GetObject()        {}
    static void MapperMap_GetObjects()       {}
    static void MapperMap_UpdateObjects()    {}
    static void MapperMap_Resize()           {}
    static void MapperMap_GetTilesCount()    {}
    static void MapperMap_DeleteTile()       {}
    static void MapperMap_GetTileHash()      {}
    static void MapperMap_AddTileHash()      {}
    static void MapperMap_GetTileName()      {}
    static void MapperMap_AddTileName()      {}
    static void MapperMap_GetTerrainName()   {}
    static void MapperMap_SetTerrainName()   {}
    static void MapperMap_GetDayTime()       {}
    static void MapperMap_SetDayTime()       {}
    static void MapperMap_GetDayColor()      {}
    static void MapperMap_SetDayColor()      {}
    static void MapperMap_get_ScriptModule() {}
    static void MapperMap_set_ScriptModule() {}
    static void MapperMap_get_ScriptFunc()   {}
    static void MapperMap_set_ScriptFunc()   {}

    static void Global_ShowCritterParam()   {}
    static void Global_GetFastPrototypes()  {}
    static void Global_SetFastPrototypes()  {}
    static void Global_LoadMap()            {}
    static void Global_UnloadMap()          {}
    static void Global_SaveMap()            {}
    static void Global_ShowMap()            {}
    static void Global_GetLoadedMaps()      {}
    static void Global_GetMapFileNames()    {}
    static void Global_DeleteObject()       {}
    static void Global_DeleteObjects()      {}
    static void Global_SelectObject()       {}
    static void Global_SelectObjects()      {}
    static void Global_GetSelectedObject()  {}
    static void Global_GetSelectedObjects() {}

    static void Global_TabGetTileDirs()    {}
    static void Global_TabGetItemPids()    {}
    static void Global_TabGetCritterPids() {}
    static void Global_TabSetTileDirs()    {}
    static void Global_TabSetItemPids()    {}
    static void Global_TabSetCritterPids() {}
    static void Global_TabDelete()         {}
    static void Global_TabSelect()         {}
    static void Global_TabSetName()        {}

    static void Global_IsCritterCanWalk()    {}
    static void Global_IsCritterCanRun()     {}
    static void Global_IsCritterCanRotate()  {}
    static void Global_IsCritterCanAim()     {}
    static void Global_IsCritterCanArmor()   {}
    static void Global_IsCritterAnim1()      {}
    static void Global_GetCritterAnimType()  {}
    static void Global_GetCritterAlias()     {}
    static void Global_GetCritterTypeName()  {}
    static void Global_GetCritterSoundName() {}

    static void Global_GetMonitorObject() {}
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
    static void Global_GetHexInPath()       {}
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
    static void Global_MoveScreen()         {}
    static void Global_MoveHexByDir()       {}
    static void Global_AppendIfaceIni()     {}
    static void Global_GetIfaceIniStr()     {}
    static void Global_LoadSprite()         {}
    static void Global_LoadSpriteHash()     {}
    static void Global_GetSpriteWidth()     {}
    static void Global_GetSpriteHeight()    {}
    static void Global_GetSpriteCount()     {}
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
    static void Global_GetKeybLang()        {}
    static void Global_GetHexPos()          {}
    static void Global_GetMonitorHex()      {}
    static void Global_LoadFont()           {}
    static void Global_SetDefaultFont()     {}
    static void Global_MouseClick()         {}
    static void Global_KeyboardPress()      {}
    static void Global_SetRainAnimation()   {}
    static void Global_GetFPS()             {}
    #endif

    static void Global_GetLastError()           {}
    static void Global_Log()                    {}
    static void Global_GetTick()                {}
    static void Global_GetProtoItem()           {}
    static void Global_StrToInt()               {}
    static void Global_StrToFloat()             {}
    static void Global_GetDistantion()          {}
    static void Global_GetDirection()           {}
    static void Global_GetOffsetDir()           {}
    static void Global_GetAngelScriptProperty() {}
    static void Global_SetAngelScriptProperty() {}
    static void Global_GetStrHash()             {}
    static void Global_LoadDataFile()           {}
    static void Global_GetConstantValue()       {}
    static void Global_GetConstantName()        {}
    static void Global_AddConstant()            {}
    static void Global_LoadConstants()          {}
    static void Global_AllowSlot()              {}
    static void Global_DecodeUTF8()             {}
    static void Global_EncodeUTF8()             {}
};

#ifdef BIND_CLIENT
int BindClass::ConsoleActive;
int BindClass::GmapActive;
int BindClass::GmapWait;
int BindClass::GmapZoom;
int BindClass::GmapOffsetX;
int BindClass::GmapOffsetY;
int BindClass::GmapGroupCurX;
int BindClass::GmapGroupCurY;
int BindClass::GmapGroupToX;
int BindClass::GmapGroupToY;
int BindClass::GmapGroupSpeed;
#endif

void Random() {}

struct GameOptions
{
    int YearStart;
    int YearStartFTLo;
    int YearStartFTHi;
    int Year;
    int Month;
    int Day;
    int Hour;
    int Minute;
    int Second;
    int FullSecondStart;
    int FullSecond;
    int TimeMultiplier;
    int GameTimeTick;

    int Singleplayer;
    int DisableTcpNagle;
    int DisableZlibCompression;
    int FloodSize;
    int NoAnswerShuffle;
    int DialogDemandRecheck;
    int FixBoyDefaultExperience;
    int SneakDivider;
    int LevelCap;
    int LevelCapAddExperience;
    int LookNormal;
    int LookMinimum;
    int GlobalMapMaxGroupCount;
    int CritterIdleTick;
    int TurnBasedTick;
    int DeadHitPoints;
    int Breaktime;
    int TimeoutTransfer;
    int TimeoutBattle;
    int ApRegeneration;
    int RtApCostCritterWalk;
    int RtApCostCritterRun;
    int RtApCostMoveItemContainer;
    int RtApCostMoveItemInventory;
    int RtApCostPickItem;
    int RtApCostDropItem;
    int RtApCostReloadWeapon;
    int RtApCostPickCritter;
    int RtApCostUseItem;
    int RtApCostUseSkill;
    int RtAlwaysRun;
    int TbApCostCritterMove;
    int TbApCostMoveItemContainer;
    int TbApCostMoveItemInventory;
    int TbApCostPickItem;
    int TbApCostDropItem;
    int TbApCostReloadWeapon;
    int TbApCostPickCritter;
    int TbApCostUseItem;
    int TbApCostUseSkill;
    int TbAlwaysRun;
    int ApCostAimEyes;
    int ApCostAimHead;
    int ApCostAimGroin;
    int ApCostAimTorso;
    int ApCostAimArms;
    int ApCostAimLegs;
    int RunOnCombat;
    int RunOnTransfer;
    int GlobalMapWidth;
    int GlobalMapHeight;
    int GlobalMapZoneLength;
    int GlobalMapMoveTime;
    int EncounterTime;
    int BagRefreshTime;
    int AttackAnimationsMinDist;
    int WhisperDist;
    int ShoutDist;
    int LookChecks;
    int LookDir[ 4 ];
    int LookSneakDir[ 4 ];
    int LookWeight;
    int CustomItemCost;
    int RegistrationTimeout;
    int AccountPlayTime;
    int LoggingVars;
    int SkipScriptBinaries;
    int ScriptRunSuspendTimeout;
    int ScriptRunMessageTimeout;
    int TalkDistance;
    int NpcMaxTalkers;
    int MinNameLength;
    int MaxNameLength;
    int DlgTalkMinTime;
    int DlgBarterMinTime;
    int MinimumOfflineTime;
    int GameServer;
    int UpdateServer;
    int GenerateWorldDisabled;

    int StartSpecialPoints;
    int StartTagSkillPoints;
    int SkillMaxValue;
    int SkillModAdd2;
    int SkillModAdd3;
    int SkillModAdd4;
    int SkillModAdd5;
    int SkillModAdd6;

    int AbsoluteOffsets;
    int SkillBegin;
    int SkillEnd;
    int TimeoutBegin;
    int TimeoutEnd;
    int KillBegin;
    int KillEnd;
    int PerkBegin;
    int PerkEnd;
    int AddictionBegin;
    int AddictionEnd;
    int KarmaBegin;
    int KarmaEnd;
    int DamageBegin;
    int DamageEnd;
    int TraitBegin;
    int TraitEnd;
    int ReputationBegin;
    int ReputationEnd;

    int ReputationLoved;
    int ReputationLiked;
    int ReputationAccepted;
    int ReputationNeutral;
    int ReputationAntipathy;
    int ReputationHated;

    int MapHexagonal;
    int MapHexWidth;
    int MapHexHeight;
    int MapHexLineHeight;
    int MapTileOffsX;
    int MapTileOffsY;
    int MapTileStep;
    int MapRoofOffsX;
    int MapRoofOffsY;
    int MapRoofSkipSize;
    int MapCameraAngle;
    int MapSmoothPath;
    int MapDataPrefix;

    // Client
    int Quit;
    int OpenGLRendering;
    int OpenGLDebug;
    int AssimpLogging;
    int MouseX;
    int MouseY;
    int ScrOx;
    int ScrOy;
    int ShowTile;
    int ShowRoof;
    int ShowItem;
    int ShowScen;
    int ShowWall;
    int ShowCrit;
    int ShowFast;
    int ShowPlayerNames;
    int ShowNpcNames;
    int ShowCritId;
    int ScrollKeybLeft;
    int ScrollKeybRight;
    int ScrollKeybUp;
    int ScrollKeybDown;
    int ScrollMouseLeft;
    int ScrollMouseRight;
    int ScrollMouseUp;
    int ScrollMouseDown;
    int ShowGroups;
    int HelpInfo;
    int DebugInfo;
    int DebugNet;
    int Enable3dRendering;
    int FullScreen;
    int VSync;
    int Light;
    int Host;
    int Port;
    int UpdateServerHost;
    int UpdateServerPort;
    int ProxyType;
    int ProxyHost;
    int ProxyPort;
    int ProxyUser;
    int ProxyPass;
    int Name;
    int ScrollDelay;
    int ScrollStep;
    int ScrollCheck;
    int FixedFPS;
    int FPS;
    int PingPeriod;
    int Ping;
    int MsgboxInvert;
    int DefaultCombatMode;
    int MessNotify;
    int SoundNotify;
    int AlwaysOnTop;
    int TextDelay;
    int DamageHitDelay;
    int ScreenWidth;
    int ScreenHeight;
    int MultiSampling;
    int MouseScroll;
    int IndicatorType;
    int DoubleClickTime;
    int RoofAlpha;
    int HideCursor;
    int DisableLMenu;
    int DisableMouseEvents;
    int DisableKeyboardEvents;
    int HidePassword;
    int PlayerOffAppendix;
    int CombatMessagesType;
    int Animation3dSmoothTime;
    int Animation3dFPS;
    int RunModMul;
    int RunModDiv;
    int RunModAdd;
    int MapZooming;
    int SpritesZoom;
    int SpritesZoomMax;
    int SpritesZoomMin;
    int EffectValues[ 10 ];
    int AlwaysRun;
    int AlwaysRunMoveDist;
    int AlwaysRunUseDist;
    int CritterFidgetTime;
    int Anim2CombatBegin;
    int Anim2CombatIdle;
    int Anim2CombatEnd;
    int ConsoleHistorySize;
    int SoundVolume;
    int MusicVolume;
    int RegParams;
    int RegName;
    int RegPassword;

    // Mapper
    int ClientPath;
    int ServerPath;
    int ShowCorners;
    int ShowSpriteCuts;
    int ShowDrawOrder;
    int SplitTilesCollection;
} GameOpt;
