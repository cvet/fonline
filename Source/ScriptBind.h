BIND_ASSERT( engine->RegisterTypedef( "hash", "uint" ) );
BIND_ASSERT( engine->RegisterTypedef( "resource", "uint" ) );
// Todo: register new type with automating string - number convertation, exclude GetStrHash
// BIND_ASSERT( engine->RegisterObjectType( "hash", 0, asOBJ_VALUE | asOBJ_POD ) );

// Entity
#define REGISTER_ENTITY( class_name )                                                                                                         \
    BIND_ASSERT( engine->RegisterObjectType( class_name, 0, asOBJ_REF ) );                                                                    \
    BIND_ASSERT( engine->RegisterObjectBehaviour( class_name, asBEHAVE_ADDREF, "void f()", asMETHOD( Entity, AddRef ), asCALL_THISCALL ) );   \
    BIND_ASSERT( engine->RegisterObjectBehaviour( class_name, asBEHAVE_RELEASE, "void f()", asMETHOD( Entity, Release ), asCALL_THISCALL ) ); \
    BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const uint Id", OFFSETOF( Entity, Id ) ) );                                     \
    BIND_ASSERT( engine->RegisterObjectMethod( class_name, "hash get_ProtoId() const", asMETHOD( Entity, GetProtoId ), asCALL_THISCALL ) );   \
    BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const bool IsDestroyed", OFFSETOF( Entity, IsDestroyed ) ) );                   \
    BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const bool IsDestroying", OFFSETOF( Entity, IsDestroying ) ) );                 \
    BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const int RefCounter", OFFSETOF( Entity, RefCounter ) ) );
/*BIND_ASSERT( engine->RegisterObjectProperty( class_name, "Entity@ Parent", OFFSETOF( Entity, Parent ) ) );*/
/*BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const array<Entity@> Children", OFFSETOF( Entity, Children ) ) );*/
#define REGISTER_ENTITY_CAST( class_name, real_class )                                                                                                                   \
    BIND_ASSERT( engine->RegisterObjectMethod( "Entity", class_name "@ opCast()", asFUNCTION( ( EntityUpCast< real_class >) ), asCALL_CDECL_OBJFIRST ) );                \
    BIND_ASSERT( engine->RegisterObjectMethod( "Entity", "const " class_name "@ opCast() const", asFUNCTION( ( EntityUpCast< real_class >) ), asCALL_CDECL_OBJFIRST ) ); \
    BIND_ASSERT( engine->RegisterObjectMethod( class_name, "Entity@ opImplCast()", asFUNCTION( ( EntityDownCast< real_class >) ), asCALL_CDECL_OBJFIRST ) );             \
    BIND_ASSERT( engine->RegisterObjectMethod( class_name, "const Entity@ opImplCast() const", asFUNCTION( ( EntityDownCast< real_class >) ), asCALL_CDECL_OBJFIRST ) );
REGISTER_ENTITY( "Entity" );

// Global vars
BIND_ASSERT( engine->RegisterObjectType( "GlobalVars", 0, asOBJ_REF | asOBJ_NOCOUNT ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "GlobalVars@ Globals", &Globals ) );

// Map and location for client and mapper
#if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
REGISTER_ENTITY( "Map" );
REGISTER_ENTITY_CAST( "Map", Map );
REGISTER_ENTITY( "Location" );
REGISTER_ENTITY_CAST( "Location", Location );
BIND_ASSERT( engine->RegisterGlobalProperty( "Map@ CurMap", &BIND_CLASS ClientCurMap ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "Location@ CurLocation", &BIND_CLASS ClientCurLocation ) );
#endif

#ifdef BIND_SERVER
/************************************************************************/
/* Types                                                                */
/************************************************************************/
REGISTER_ENTITY( "Item" );
REGISTER_ENTITY_CAST( "Item", Item );
REGISTER_ENTITY( "Critter" );
REGISTER_ENTITY_CAST( "Critter", Critter );
REGISTER_ENTITY( "Map" );
REGISTER_ENTITY_CAST( "Map", Map );
REGISTER_ENTITY( "Location" );
REGISTER_ENTITY_CAST( "Location", Location );

BIND_ASSERT( engine->RegisterObjectType( "NpcPlane", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "NpcPlane", asBEHAVE_ADDREF, "void f()", asMETHOD( AIDataPlane, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "NpcPlane", asBEHAVE_RELEASE, "void f()", asMETHOD( AIDataPlane, Release ), asCALL_THISCALL ) );

/************************************************************************/
/* NpcPlane                                                             */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "int Type", OFFSETOF( AIDataPlane, Type ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint Priority", OFFSETOF( AIDataPlane, Priority ) ) );

// BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "NpcPlane@ Child", OFFSETOF( AIDataPlane, Type ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "int Identifier", OFFSETOF( AIDataPlane, Identifier ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint IdentifierExt", OFFSETOF( AIDataPlane, IdentifierExt ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "bool Run", OFFSETOF( AIDataPlane, Pick.IsRun ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint Misc_WaitSecond", OFFSETOF( AIDataPlane, Misc.WaitSecond ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint Misc_ScriptId", OFFSETOF( AIDataPlane, Misc.ScriptBindId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint Attack_TargId", OFFSETOF( AIDataPlane, Attack.TargId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "int Attack_MinHp", OFFSETOF( AIDataPlane, Attack.MinHp ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "bool Attack_IsGag", OFFSETOF( AIDataPlane, Attack.IsGag ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Attack_GagHexX", OFFSETOF( AIDataPlane, Attack.GagHexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Attack_GagHexY", OFFSETOF( AIDataPlane, Attack.GagHexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Attack_LastHexX", OFFSETOF( AIDataPlane, Attack.LastHexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Attack_LastHexY", OFFSETOF( AIDataPlane, Attack.LastHexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Walk_HexX", OFFSETOF( AIDataPlane, Walk.HexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Walk_HexY", OFFSETOF( AIDataPlane, Walk.HexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint8 Walk_Dir", OFFSETOF( AIDataPlane, Walk.Dir ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint Walk_Cut", OFFSETOF( AIDataPlane, Walk.Cut ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Pick_HexX", OFFSETOF( AIDataPlane, Pick.HexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint16 Pick_HexY", OFFSETOF( AIDataPlane, Pick.HexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "hash Pick_Pid", OFFSETOF( AIDataPlane, Pick.Pid ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "uint Pick_UseItemId", OFFSETOF( AIDataPlane, Pick.UseItemId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "bool Pick_ToOpen", OFFSETOF( AIDataPlane, Pick.ToOpen ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "bool Pick_IsRun", OFFSETOF( AIDataPlane, Pick.IsRun ) ) );

BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "NpcPlane@ GetCopy() const", asFUNCTION( BIND_CLASS NpcPlane_GetCopy ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "NpcPlane@+ SetChild(NpcPlane@+ child)", asFUNCTION( BIND_CLASS NpcPlane_SetChild ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "NpcPlane@+ GetChild(uint index) const", asFUNCTION( BIND_CLASS NpcPlane_GetChild ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterFuncdef( "void NpcPlaneMiscFunc(Critter@+)" ) );
BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "bool Misc_SetScript(NpcPlaneMiscFunc@+ func)", asFUNCTION( BIND_CLASS NpcPlane_Misc_SetScript ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Item                                                                 */
/************************************************************************/

BIND_ASSERT( engine->RegisterFuncdef( "void ItemInitFunc(Item@+, bool)" ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool SetScript(ItemInitFunc@+ func)", asFUNCTION( BIND_CLASS Item_SetScript ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ AddItem(hash protoId, uint count, uint stackId)", asFUNCTION( BIND_CLASS Item_AddItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<Item@>@ GetItems(uint stackId)", asFUNCTION( BIND_CLASS Item_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<const Item@>@ GetItems(uint stackId) const", asFUNCTION( BIND_CLASS Item_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Map@+ GetMapPosition(uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Item_GetMapPosition ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool ChangeProto(hash protoId)", asFUNCTION( BIND_CLASS Item_ChangeProto ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void Animate(uint fromFrame, uint toFrame)", asFUNCTION( BIND_CLASS Item_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool CallSceneryFunction(Critter@+ cr, Item@+ item, int param) const", asFUNCTION( BIND_CLASS Item_CallSceneryFunction ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Critter                                                              */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsPlayer() const", asFUNCTION( BIND_CLASS Crit_IsPlayer ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNpc() const", asFUNCTION( BIND_CLASS Crit_IsNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetAccess() const", asFUNCTION( BIND_CLASS Cl_GetAccess ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetAccess(int access)", asFUNCTION( BIND_CLASS Cl_SetAccess ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Map@+ GetMap()", asFUNCTION( BIND_CLASS Crit_GetMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Map@+ GetMap() const", asFUNCTION( BIND_CLASS Crit_GetMap ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool MoveRandom()", asFUNCTION( BIND_CLASS Crit_MoveRandom ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool MoveToDir(uint8 dir)", asFUNCTION( BIND_CLASS Crit_MoveToDir ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToHex(uint16 hexX, uint16 hexY, uint8 dir)", asFUNCTION( BIND_CLASS Crit_TransitToHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToMap(uint mapId, uint16 hexX, uint16 hexY, uint8 dir)", asFUNCTION( BIND_CLASS Crit_TransitToMapHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToMap(uint mapId, int entireNum)", asFUNCTION( BIND_CLASS Crit_TransitToMapEntire ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToGlobal()", asFUNCTION( BIND_CLASS Crit_TransitToGlobal ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToGlobal(array<Critter@>@+ group)", asFUNCTION( BIND_CLASS Crit_TransitToGlobalWithGroup ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToGlobalGroup(uint critterId)", asFUNCTION( BIND_CLASS Crit_TransitToGlobalGroup ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsLife() const", asFUNCTION( BIND_CLASS Crit_IsLife ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnockout() const", asFUNCTION( BIND_CLASS Crit_IsKnockout ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsDead() const", asFUNCTION( BIND_CLASS Crit_IsDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsFree() const", asFUNCTION( BIND_CLASS Crit_IsFree ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsBusy() const", asFUNCTION( BIND_CLASS Crit_IsBusy ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Wait(uint ms)", asFUNCTION( BIND_CLASS Crit_Wait ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void RefreshVisible()", asFUNCTION( BIND_CLASS Crit_RefreshVisible ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ViewMap(Map@+ map, uint look, uint16 hx, uint16 hy, uint8 dir)", asFUNCTION( BIND_CLASS Crit_ViewMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint CountItem(hash protoId) const", asFUNCTION( BIND_CLASS Crit_CountItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool DeleteItem(hash protoId, uint count)", asFUNCTION( BIND_CLASS Crit_DeleteItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ AddItem(hash protoId, uint count)", asFUNCTION( BIND_CLASS Crit_AddItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Crit_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItem(uint itemId) const", asFUNCTION( BIND_CLASS Crit_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemByPid(hash protoId)", asFUNCTION( BIND_CLASS Crit_GetItemByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemByPid(hash protoId) const", asFUNCTION( BIND_CLASS Crit_GetItemByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemBySlot(uint8 slot)", asFUNCTION( BIND_CLASS Crit_GetItemBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemBySlot(uint8 slot) const", asFUNCTION( BIND_CLASS Crit_GetItemBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItems()", asFUNCTION( BIND_CLASS Crit_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItems() const", asFUNCTION( BIND_CLASS Crit_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItemsBySlot(uint8 slot)", asFUNCTION( BIND_CLASS Crit_GetItemsBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItemsBySlot(uint8 slot) const", asFUNCTION( BIND_CLASS Crit_GetItemsBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItemsByType(int type)", asFUNCTION( BIND_CLASS Crit_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItemsByType(int type) const", asFUNCTION( BIND_CLASS Crit_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ChangeItemSlot(uint itemId, uint8 slot)", asFUNCTION( BIND_CLASS Crit_ChangeItemSlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetCond(int cond)", asFUNCTION( BIND_CLASS Crit_SetCond ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void CloseDialog()", asFUNCTION( BIND_CLASS Crit_CloseDialog ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Critter@>@ GetCritters(bool lookOnMe, int findType) const", asFUNCTION( BIND_CLASS Crit_GetCritters ), asCALL_CDECL_OBJFIRST ) ); // Todo: const
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Critter@>@ GetTalkedPlayers() const", asFUNCTION( BIND_CLASS Npc_GetTalkedPlayers ), asCALL_CDECL_OBJFIRST ) );                   // Todo: const
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSee(Critter@+ cr) const", asFUNCTION( BIND_CLASS Crit_IsSeeCr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSeenBy(Critter@+ cr) const", asFUNCTION( BIND_CLASS Crit_IsSeenByCr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSee(Item@+ item) const", asFUNCTION( BIND_CLASS Crit_IsSeeItem ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Say(uint8 howSay, string text)", asFUNCTION( BIND_CLASS Crit_Say ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SayMsg(uint8 howSay, uint16 textMsg, uint strNum)", asFUNCTION( BIND_CLASS Crit_SayMsg ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SayMsg(uint8 howSay, uint16 textMsg, uint strNum, string lexems)", asFUNCTION( BIND_CLASS Crit_SayMsgLex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetDir(uint8 dir)", asFUNCTION( BIND_CLASS Crit_SetDir ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint ErasePlane(int planeType, bool all)", asFUNCTION( BIND_CLASS Npc_ErasePlane ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool ErasePlane(uint index)", asFUNCTION( BIND_CLASS Npc_ErasePlaneIndex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void DropPlanes()", asFUNCTION( BIND_CLASS Npc_DropPlanes ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNoPlanes() const", asFUNCTION( BIND_CLASS Npc_IsNoPlanes ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsCurPlane(int planeType) const", asFUNCTION( BIND_CLASS Npc_IsCurPlane ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "NpcPlane@+ GetCurPlane() const", asFUNCTION( BIND_CLASS Npc_GetCurPlane ), asCALL_CDECL_OBJFIRST ) );                                                 // Todo: const
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<NpcPlane@>@ GetPlanes() const", asFUNCTION( BIND_CLASS Npc_GetPlanes ), asCALL_CDECL_OBJFIRST ) );                                              // Todo: const
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<NpcPlane@>@ GetPlanes(int identifier) const", asFUNCTION( BIND_CLASS Npc_GetPlanesIdentifier ), asCALL_CDECL_OBJFIRST ) );                      // Todo: const
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<NpcPlane@>@ GetPlanes(int identifier, uint identifierExt) const", asFUNCTION( BIND_CLASS Npc_GetPlanesIdentifier2 ), asCALL_CDECL_OBJFIRST ) ); // Todo: const
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddPlane(NpcPlane@+ plane)", asFUNCTION( BIND_CLASS Npc_AddPlane ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void NextPlane(int reason)", asFUNCTION( BIND_CLASS Npc_NextPlane ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendMessage(int num, int val, int to)", asFUNCTION( BIND_CLASS Crit_SendMessage ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Action(int action, int actionExt, const Item@+ item)", asFUNCTION( BIND_CLASS Crit_Action ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2, const Item@+ item, bool clearSequence, bool delayPlay)", asFUNCTION( BIND_CLASS Crit_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetAnims(int cond, uint anim1, uint anim2)", asFUNCTION( BIND_CLASS Crit_SetAnims ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void PlaySound(string soundName, bool sendSelf)", asFUNCTION( BIND_CLASS Crit_PlaySound ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendCombatResult(array<uint>@+ combatResult)", asFUNCTION( BIND_CLASS Crit_SendCombatResult ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnownLoc(bool byId, uint locNum) const", asFUNCTION( BIND_CLASS Crit_IsKnownLoc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetKnownLoc(bool byId, uint locNum)", asFUNCTION( BIND_CLASS Crit_SetKnownLoc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool UnsetKnownLoc(bool byId, uint locNum)", asFUNCTION( BIND_CLASS Crit_UnsetKnownLoc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetFog(uint16 zoneX, uint16 zoneY, int fog)", asFUNCTION( BIND_CLASS Crit_SetFog ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetFog(uint16 zoneX, uint16 zoneY) const", asFUNCTION( BIND_CLASS Crit_GetFog ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendItems(const array<const Item@>@+ items, int param = 0)", asFUNCTION( BIND_CLASS Cl_SendItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendItems(const array<Item@>@+ items, int param = 0)", asFUNCTION( BIND_CLASS Cl_SendItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Disconnect()", asFUNCTION( BIND_CLASS Cl_Disconnect ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterFuncdef( "void CritterInitFunc(Critter@+, bool)" ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetScript(CritterInitFunc@+ func)", asFUNCTION( BIND_CLASS Crit_SetScript ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void AddEnemyToStack(uint critterId)", asFUNCTION( BIND_CLASS Crit_AddEnemyToStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool CheckEnemyInStack(uint critterId) const", asFUNCTION( BIND_CLASS Crit_CheckEnemyInStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EraseEnemyFromStack(uint critterId)", asFUNCTION( BIND_CLASS Crit_EraseEnemyFromStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ClearEnemyStack()", asFUNCTION( BIND_CLASS Crit_ClearEnemyStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ClearEnemyStackNpc()", asFUNCTION( BIND_CLASS Crit_ClearEnemyStackNpc ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterFuncdef( "uint TimeEventFunc(Critter@+,int,uint&)" ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddTimeEvent(TimeEventFunc@+ func, uint duration, int identifier)", asFUNCTION( BIND_CLASS Crit_AddTimeEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddTimeEvent(TimeEventFunc@+ func, uint duration, int identifier, uint rate)", asFUNCTION( BIND_CLASS Crit_AddTimeEventRate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTimeEvents(int identifier, array<uint>@+ indexes, array<uint>@+ durations, array<uint>@+ rates) const", asFUNCTION( BIND_CLASS Crit_GetTimeEvents ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTimeEvents(array<int>@+ findIdentifiers, array<int>@+ identifiers, array<uint>@+ indexes, array<uint>@+ durations, array<uint>@+ rates) const", asFUNCTION( BIND_CLASS Crit_GetTimeEventsArr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ChangeTimeEvent(uint index, uint newDuration, uint newRate)", asFUNCTION( BIND_CLASS Crit_ChangeTimeEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EraseTimeEvent(uint index)", asFUNCTION( BIND_CLASS Crit_EraseTimeEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint EraseTimeEvents(int identifier)", asFUNCTION( BIND_CLASS Crit_EraseTimeEvents ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint EraseTimeEvents(array<int>@+ identifiers)", asFUNCTION( BIND_CLASS Crit_EraseTimeEventsArr ), asCALL_CDECL_OBJFIRST ) );

// Parameters
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const string Name", OFFSETOF( Critter, Name ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const bool IsRunning", OFFSETOF( Critter, IsRunning ) ) );

/************************************************************************/
/* Map                                                                  */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Location@+ GetLocation()", asFUNCTION( BIND_CLASS Map_GetLocation ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Location@+ GetLocation() const", asFUNCTION( BIND_CLASS Map_GetLocation ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterFuncdef( "void MapInitFunc(Map@+, bool)" ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool SetScript(MapInitFunc@+ func)", asFUNCTION( BIND_CLASS Map_SetScript ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Map_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetItem(uint itemId) const", asFUNCTION( BIND_CLASS Map_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetItem(uint16 hexX, uint16 hexY, hash protoId)", asFUNCTION( BIND_CLASS Map_GetItemHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetItem(uint16 hexX, uint16 hexY, hash protoId) const", asFUNCTION( BIND_CLASS Map_GetItemHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems()", asFUNCTION( BIND_CLASS Map_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems() const", asFUNCTION( BIND_CLASS Map_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Map_GetItemsHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetItemsHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(uint16 hexX, uint16 hexY, uint radius, hash protoId)", asFUNCTION( BIND_CLASS Map_GetItemsHexEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(uint16 hexX, uint16 hexY, uint radius, hash protoId) const", asFUNCTION( BIND_CLASS Map_GetItemsHexEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItemsByPid(hash protoId)", asFUNCTION( BIND_CLASS Map_GetItemsByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItemsByPid(hash protoId) const", asFUNCTION( BIND_CLASS Map_GetItemsByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItemsByType(int type)", asFUNCTION( BIND_CLASS Map_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItemsByType(int type) const", asFUNCTION( BIND_CLASS Map_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetDoor(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Map_GetDoor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetDoor(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetDoor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetCar(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Map_GetCar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetCar(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetCar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetScenery(uint16 hexX, uint16 hexY, hash protoId) const", asFUNCTION( BIND_CLASS Map_GetSceneryHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetSceneries(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetSceneriesHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetSceneries(uint16 hexX, uint16 hexY, uint radius, hash protoId) const", asFUNCTION( BIND_CLASS Map_GetSceneriesHexEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetSceneries(hash protoId) const", asFUNCTION( BIND_CLASS Map_GetSceneriesByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetCritter(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Map_GetCritterHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Critter@+ GetCritter(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetCritterHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetCritter(uint critterId)", asFUNCTION( BIND_CLASS Map_GetCritterById ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Critter@+ GetCritter(uint critterId) const", asFUNCTION( BIND_CLASS Map_GetCritterById ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType)", asFUNCTION( BIND_CLASS Map_GetCritters ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType) const", asFUNCTION( BIND_CLASS Map_GetCritters ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCritters(hash pid, int findType)", asFUNCTION( BIND_CLASS Map_GetCrittersByPids ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCritters(hash pid, int findType) const", asFUNCTION( BIND_CLASS Map_GetCrittersByPids ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType)", asFUNCTION( BIND_CLASS Map_GetCrittersInPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType) const", asFUNCTION( BIND_CLASS Map_GetCrittersInPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy)", asFUNCTION( BIND_CLASS Map_GetCrittersInPathBlock ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy) const", asFUNCTION( BIND_CLASS Map_GetCrittersInPathBlock ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType)", asFUNCTION( BIND_CLASS Map_GetCrittersWhoViewPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType) const", asFUNCTION( BIND_CLASS Map_GetCrittersWhoViewPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersSeeing(array<Critter@>@+ critters, bool lookOnThem, int findType)", asFUNCTION( BIND_CLASS Map_GetCrittersSeeing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersSeeing(array<const Critter@>@+ critters, bool lookOnThem, int findType)", asFUNCTION( BIND_CLASS Map_GetCrittersSeeing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersSeeing(array<Critter@>@+ critters, bool lookOnThem, int findType) const", asFUNCTION( BIND_CLASS Map_GetCrittersSeeing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersSeeing(array<const Critter@>@+ critters, bool lookOnThem, int findType) const", asFUNCTION( BIND_CLASS Map_GetCrittersSeeing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const", asFUNCTION( BIND_CLASS Map_GetHexInPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetHexCoordWall(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const", asFUNCTION( BIND_CLASS Map_GetHexInPathWall ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut) const", asFUNCTION( BIND_CLASS Map_GetPathLengthHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetPathLength(Critter@+ cr, uint16 toHx, uint16 toHy, uint cut) const", asFUNCTION( BIND_CLASS Map_GetPathLengthCr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void VerifyTrigger(Critter@+ cr, uint16 hexX, uint16 hexY, uint8 dir)", asFUNCTION( BIND_CLASS Map_VerifyTrigger ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetNpcCount(int npcRole, int findType) const", asFUNCTION( BIND_CLASS Map_GetNpcCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetNpc(int npcRole, int findType, uint skipCount)", asFUNCTION( BIND_CLASS Map_GetNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Critter@+ GetNpc(int npcRole, int findType, uint skipCount) const", asFUNCTION( BIND_CLASS Map_GetNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint CountEntire(int entire) const", asFUNCTION( BIND_CLASS Map_CountEntire ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetEntires(int entire, array<int>@+ entires, array<uint16>@+ hexX, array<uint16>@+ hexY) const", asFUNCTION( BIND_CLASS Map_GetEntires ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetEntireCoords(int entire, uint skip, uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Map_GetEntireCoords ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetEntireCoords(int entire, uint skip, uint16& hexX, uint16& hexY, uint8& dir) const", asFUNCTION( BIND_CLASS Map_GetEntireCoordsDir ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetNearEntireCoords(int& entire, uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Map_GetNearEntireCoords ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetNearEntireCoords(int& entire, uint16& hexX, uint16& hexY, uint8& dir) const", asFUNCTION( BIND_CLASS Map_GetNearEntireCoordsDir ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexPassed(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_IsHexPassed ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexesPassed(uint16 hexX, uint16 hexY, uint radius) const", asFUNCTION( BIND_CLASS Map_IsHexesPassed ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexRaked(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_IsHexRaked ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetText(uint16 hexX, uint16 hexY, uint color, string text)", asFUNCTION( BIND_CLASS Map_SetText ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum)", asFUNCTION( BIND_CLASS Map_SetTextMsg ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum, string lexems)", asFUNCTION( BIND_CLASS Map_SetTextMsgLex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void RunEffect(hash effectPid, uint16 hexX, uint16 hexY, uint16 radius)", asFUNCTION( BIND_CLASS Map_RunEffect ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void RunFlyEffect(hash effectPid, Critter@+ fromCr, Critter@+ toCr, uint16 fromX, uint16 fromY, uint16 toX, uint16 toY)", asFUNCTION( BIND_CLASS Map_RunFlyEffect ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool CheckPlaceForItem(uint16 hexX, uint16 hexY, hash pid) const", asFUNCTION( BIND_CLASS Map_CheckPlaceForItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void BlockHex(uint16 hexX, uint16 hexY, bool full)", asFUNCTION( BIND_CLASS Map_BlockHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void UnblockHex(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Map_UnblockHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void PlaySound(string soundName)", asFUNCTION( BIND_CLASS Map_PlaySound ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void PlaySound(string soundName, uint16 hexX, uint16 hexY, uint radius)", asFUNCTION( BIND_CLASS Map_PlaySoundRadius ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool Reload()", asFUNCTION( BIND_CLASS Map_Reload ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps) const", asFUNCTION( BIND_CLASS Map_MoveHexByDir ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Location                                                             */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetMapCount() const", asFUNCTION( BIND_CLASS Location_GetMapCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "Map@+ GetMap(hash mapPid)", asFUNCTION( BIND_CLASS Location_GetMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "const Map@+ GetMap(hash mapPid) const", asFUNCTION( BIND_CLASS Location_GetMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "Map@+ GetMapByIndex(uint index)", asFUNCTION( BIND_CLASS Location_GetMapByIndex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "const Map@+ GetMapByIndex(uint index) const", asFUNCTION( BIND_CLASS Location_GetMapByIndex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "array<Map@>@ GetMaps()", asFUNCTION( BIND_CLASS Location_GetMaps ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "array<const Map@>@ GetMaps() const", asFUNCTION( BIND_CLASS Location_GetMaps ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool GetEntrance(uint entrance, uint& mapIndex, hash& entire) const", asFUNCTION( BIND_CLASS Location_GetEntrance ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetEntrances(array<uint>@+ mapsIndex, array<hash>@+ entires) const", asFUNCTION( BIND_CLASS Location_GetEntrances ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool Reload()", asFUNCTION( BIND_CLASS Location_Reload ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Global                                                               */
/************************************************************************/
BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Global_GetItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item@+ item, uint count, Critter@+ toCr, bool skipChecks = false)", asFUNCTION( BIND_CLASS Global_MoveItemCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item@+ item, uint count, Item@+ toCont, uint stackId, bool skipChecks = false)", asFUNCTION( BIND_CLASS Global_MoveItemCont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item@+ item, uint count, Map@+ toMap, uint16 toHx, uint16 toHy, bool skipChecks = false)", asFUNCTION( BIND_CLASS Global_MoveItemMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(const array<Item@>@+ items, Critter@+ toCr, bool skipChecks = false)", asFUNCTION( BIND_CLASS Global_MoveItemsCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(const array<Item@>@+ items, Item@+ toCont, uint stackId, bool skipChecks = false)", asFUNCTION( BIND_CLASS Global_MoveItemsCont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(const array<Item@>@+ items, Map@+ toMap, uint16 toHx, uint16 toHy, bool skipChecks = false)", asFUNCTION( BIND_CLASS Global_MoveItemsMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItem(Item@+ item)", asFUNCTION( BIND_CLASS Global_DeleteItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItem(uint itemId)", asFUNCTION( BIND_CLASS Global_DeleteItemById ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItems(array<Item@>@+ items)", asFUNCTION( BIND_CLASS Global_DeleteItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItems(array<uint>@+ itemIds)", asFUNCTION( BIND_CLASS Global_DeleteItemsById ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteNpc(Critter@+ npc)", asFUNCTION( BIND_CLASS Global_DeleteNpc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteNpc(uint npcId)", asFUNCTION( BIND_CLASS Global_DeleteNpcById ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersDistantion(const Critter@+ cr1, const Critter@+ cr2)", asFUNCTION( BIND_CLASS Global_GetCrittersDistantion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessage(uint16 channel, string text)", asFUNCTION( BIND_CLASS Global_RadioMessage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_RadioMessageMsg ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum, string lexems)", asFUNCTION( BIND_CLASS Global_RadioMessageMsgLex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateLocation(hash locPid, uint16 worldX, uint16 worldY, array<Critter@>@+ critters)", asFUNCTION( BIND_CLASS Global_CreateLocation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteLocation(Location@+ loc)", asFUNCTION( BIND_CLASS Global_DeleteLocation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteLocation(uint locId)", asFUNCTION( BIND_CLASS Global_DeleteLocationById ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritter(uint critterId)", asFUNCTION( BIND_CLASS Global_GetCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetPlayer(string name)", asFUNCTION( BIND_CLASS Global_GetPlayer ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPlayerId(string name)", asFUNCTION( BIND_CLASS Global_GetPlayerId ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetPlayerName(uint playerId)", asFUNCTION( BIND_CLASS Global_GetPlayerName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetGlobalMapCritters(uint16 worldX, uint16 worldY, uint radius, int findType)", asFUNCTION( BIND_CLASS Global_GetGlobalMapCritters ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ GetMap(uint mapId)", asFUNCTION( BIND_CLASS Global_GetMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ GetMapByPid(hash mapPid, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetMapByPid ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ GetLocation(uint locId)", asFUNCTION( BIND_CLASS Global_GetLocation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ GetLocationByPid(hash locPid, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetLocationByPid ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Location@>@ GetLocations(uint16 worldX, uint16 worldY, uint radius)", asFUNCTION( BIND_CLASS Global_GetLocations ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Location@>@ GetVisibleLocations(uint16 worldX, uint16 worldY, uint radius, Critter@+ visibleBy)", asFUNCTION( BIND_CLASS Global_GetVisibleLocations ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint>@ GetZoneLocationIds(uint16 zoneX, uint16 zoneY, uint zoneRadius)", asFUNCTION( BIND_CLASS Global_GetZoneLocationIds ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter@+ player, Critter@+ npc, bool ignoreDistance)", asFUNCTION( BIND_CLASS Global_RunDialogNpc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter@+ player, Critter@+ npc, uint dialogPack, bool ignoreDistance)", asFUNCTION( BIND_CLASS Global_RunDialogNpcDlgPack ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter@+ player, uint dialogPack, uint16 hexX, uint16 hexY, bool ignoreDistance)", asFUNCTION( BIND_CLASS Global_RunDialogHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int64 WorldItemCount(hash protoId)", asFUNCTION( BIND_CLASS Global_WorldItemCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterFuncdef( "void TextListenerFunc(Critter@+, string)" ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool AddTextListener(int sayType, string firstStr, uint parameter, TextListenerFunc@+ func)", asFUNCTION( BIND_CLASS Global_AddTextListener ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseTextListener(int sayType, string firstStr, uint parameter)", asFUNCTION( BIND_CLASS Global_EraseTextListener ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "NpcPlane@ CreatePlane()", asFUNCTION( BIND_CLASS Global_CreatePlane ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SwapCritters(Critter@+ cr1, Critter@+ cr2, bool withInventory)", asFUNCTION( BIND_CLASS Global_SwapCritters ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetAllItems(hash pid)", asFUNCTION( BIND_CLASS Global_GetAllItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetAllPlayers()", asFUNCTION( BIND_CLASS Global_GetAllPlayers ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetRegisteredPlayers(array<uint>@+ ids, array<string>@+ names)", asFUNCTION( BIND_CLASS Global_GetRegisteredPlayers ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetAllNpc(hash pid)", asFUNCTION( BIND_CLASS Global_GetAllNpc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Map@>@ GetAllMaps(hash pid)", asFUNCTION( BIND_CLASS Global_GetAllMaps ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Location@>@ GetAllLocations(hash pid)", asFUNCTION( BIND_CLASS Global_GetAllLocations ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadImage(uint index, string imageName, uint imageDepth)", asFUNCTION( BIND_CLASS Global_LoadImage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetImageColor(uint index, uint x, uint y)", asFUNCTION( BIND_CLASS Global_GetImageColor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetTime(uint16 multiplier, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)", asFUNCTION( BIND_CLASS Global_SetTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void YieldWebRequest(string url, const dict<string, string>@+ post, bool& success, string& result)", asFUNCTION( BIND_CLASS Global_YieldWebRequest ), asCALL_CDECL ) );
#endif

#if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
REGISTER_ENTITY( "Critter" );
REGISTER_ENTITY_CAST( "Critter", CritterCl );
REGISTER_ENTITY( "Item" );
REGISTER_ENTITY_CAST( "Item", Item );
#endif

#ifdef BIND_CLIENT
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsChosen() const", asFUNCTION( BIND_CLASS Crit_IsChosen ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsPlayer() const", asFUNCTION( BIND_CLASS Crit_IsPlayer ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNpc() const", asFUNCTION( BIND_CLASS Crit_IsNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsOffline() const", asFUNCTION( BIND_CLASS Crit_IsOffline ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsLife() const", asFUNCTION( BIND_CLASS Crit_IsLife ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnockout() const", asFUNCTION( BIND_CLASS Crit_IsKnockout ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsDead() const", asFUNCTION( BIND_CLASS Crit_IsDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsFree() const", asFUNCTION( BIND_CLASS Crit_IsFree ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsBusy() const", asFUNCTION( BIND_CLASS Crit_IsBusy ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnim3d() const", asFUNCTION( BIND_CLASS Crit_IsAnim3d ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnimAviable(uint anim1, uint anim2) const", asFUNCTION( BIND_CLASS Crit_IsAnimAviable ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnimPlaying() const", asFUNCTION( BIND_CLASS Crit_IsAnimPlaying ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetAnim1() const", asFUNCTION( BIND_CLASS Crit_GetAnim1 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2)", asFUNCTION( BIND_CLASS Crit_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2, const Item@+ item)", asFUNCTION( BIND_CLASS Crit_AnimateEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ClearAnim()", asFUNCTION( BIND_CLASS Crit_ClearAnim ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Wait(uint ms)", asFUNCTION( BIND_CLASS Crit_Wait ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint CountItem(hash protoId) const", asFUNCTION( BIND_CLASS Crit_CountItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Crit_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItem(uint itemId) const", asFUNCTION( BIND_CLASS Crit_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemBySlot(uint8 slot)", asFUNCTION( BIND_CLASS Crit_GetItemBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemBySlot(uint8 slot) const", asFUNCTION( BIND_CLASS Crit_GetItemBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemByPid(hash protoId)", asFUNCTION( BIND_CLASS Crit_GetItemByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemByPid(hash protoId) const", asFUNCTION( BIND_CLASS Crit_GetItemByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItems()", asFUNCTION( BIND_CLASS Crit_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItems() const", asFUNCTION( BIND_CLASS Crit_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItemsBySlot(uint8 slot)", asFUNCTION( BIND_CLASS Crit_GetItemsBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItemsBySlot(uint8 slot) const", asFUNCTION( BIND_CLASS Crit_GetItemsBySlot ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItemsByType(int type)", asFUNCTION( BIND_CLASS Crit_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItemsByType(int type) const", asFUNCTION( BIND_CLASS Crit_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetVisible(bool visible)", asFUNCTION( BIND_CLASS Crit_SetVisible ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool GetVisible() const", asFUNCTION( BIND_CLASS Crit_GetVisible ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void set_ContourColor(uint value)", asFUNCTION( BIND_CLASS Crit_set_ContourColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint get_ContourColor() const", asFUNCTION( BIND_CLASS Crit_get_ContourColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void GetNameTextInfo( bool& nameVisible, int& x, int& y, int& w, int& h, int& lines ) const", asFUNCTION( BIND_CLASS Crit_GetNameTextInfo ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "string Name", OFFSETOF( CritterCl, Name ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "string NameOnHead", OFFSETOF( CritterCl, NameOnHead ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "string Avatar", OFFSETOF( CritterCl, Avatar ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "uint NameColor", OFFSETOF( CritterCl, NameColor ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "bool IsRunning", OFFSETOF( CritterCl, IsRunning ) ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@ Clone(uint newCount = 0) const", asFUNCTION( BIND_CLASS Item_Clone ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool  GetMapPosition(uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Item_GetMapPosition ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void  Animate(uint fromFrame, uint toFrame)", asFUNCTION( BIND_CLASS Item_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<Item@>@  GetItems(uint stackId)", asFUNCTION( BIND_CLASS Item_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<const Item@>@  GetItems(uint stackId) const", asFUNCTION( BIND_CLASS Item_GetItems ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "string CustomCall(string command, string separator = \" \")", asFUNCTION( BIND_CLASS Global_CustomCall ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetChosen()", asFUNCTION( BIND_CLASS Global_GetChosen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Global_GetItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetMapAllItems()", asFUNCTION( BIND_CLASS Global_GetMapAllItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetMapHexItems(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_GetMapHexItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersDistantion(const Critter@+ cr1, const Critter@+ cr2)", asFUNCTION( BIND_CLASS Global_GetCrittersDistantion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritter(uint critterId)", asFUNCTION( BIND_CLASS Global_GetCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType)", asFUNCTION( BIND_CLASS Global_GetCritters ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCritters(hash pid, int findType)", asFUNCTION( BIND_CLASS Global_GetCrittersByPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType)", asFUNCTION( BIND_CLASS Global_GetCrittersInPath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy)", asFUNCTION( BIND_CLASS Global_GetCrittersInPathBlock ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)", asFUNCTION( BIND_CLASS Global_GetHexInPath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint8>@ GetPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint8>@ GetPath(Critter@+ cr, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathLengthHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(Critter@+ cr, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathLengthCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void FlushScreen(uint fromColor, uint toColor, uint timeMs)", asFUNCTION( BIND_CLASS Global_FlushScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void QuakeScreen(uint noise, uint timeMs)", asFUNCTION( BIND_CLASS Global_QuakeScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlaySound(string soundName)", asFUNCTION( BIND_CLASS Global_PlaySound ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlayMusic(string musicName, uint pos, uint repeat)", asFUNCTION( BIND_CLASS Global_PlayMusic ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void PlayVideo(string videoName, bool canStop)", asFUNCTION( BIND_CLASS Global_PlayVideo ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string text)", asFUNCTION( BIND_CLASS Global_Message ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string text, int type)", asFUNCTION( BIND_CLASS Global_MessageType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_MessageMsg ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum, int type)", asFUNCTION( BIND_CLASS Global_MessageMsgType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MapMessage(string text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)", asFUNCTION( BIND_CLASS Global_MapMessage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetMsgStrSkip ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumUpper(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumUpper ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumLower(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumLower ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrCount(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_IsMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, string str)", asFUNCTION( BIND_CLASS Global_ReplaceTextStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, int i)", asFUNCTION( BIND_CLASS Global_ReplaceTextInt ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string FormatTags(string text, string lexems)", asFUNCTION( BIND_CLASS Global_FormatTags ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreen(uint16 hexX, uint16 hexY, uint speed, bool canStop = false)", asFUNCTION( BIND_CLASS Global_MoveScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void LockScreenScroll(Critter@+ cr, bool unlockIfSame = false)", asFUNCTION( BIND_CLASS Global_LockScreenScroll ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetFog(uint16 zoneX, uint16 zoneY)", asFUNCTION( BIND_CLASS Global_GetFog ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDayTime(uint dayPart)", asFUNCTION( BIND_CLASS Global_GetDayTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b)", asFUNCTION( BIND_CLASS Global_GetDayColor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ShowScreen(int screen, dictionary@+ params = null)", asFUNCTION( BIND_CLASS Global_ShowScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void HideScreen(int screen = 0)", asFUNCTION( BIND_CLASS Global_HideScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y)", asFUNCTION( BIND_CLASS Global_GetHexPos ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy)", asFUNCTION( BIND_CLASS Global_GetMonitorHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetMonitorItem(int x, int y)", asFUNCTION( BIND_CLASS Global_GetMonitorItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetMonitorCritter(int x, int y)", asFUNCTION( BIND_CLASS Global_GetMonitorCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Entity@+ GetMonitorEntity(int x, int y)", asFUNCTION( BIND_CLASS Global_GetMonitorEntity ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint16 GetMapWidth()", asFUNCTION( BIND_CLASS Global_GetMapWidth ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint16 GetMapHeight()", asFUNCTION( BIND_CLASS Global_GetMapHeight ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMapHexPassed(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_IsMapHexPassed ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMapHexRaked(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_IsMapHexRaked ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)", asFUNCTION( BIND_CLASS Global_MoveHexByDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Preload3dFiles(array<string>@+ fileNames)", asFUNCTION( BIND_CLASS Global_Preload3dFiles ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void WaitPing()", asFUNCTION( BIND_CLASS Global_WaitPing ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadFont(int font, string fontFileName)", asFUNCTION( BIND_CLASS Global_LoadFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetDefaultFont(int font, uint color)", asFUNCTION( BIND_CLASS Global_SetDefaultFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetEffect(int effectType, int effectSubtype, string effectName, string effectDefines = \"\")", asFUNCTION( BIND_CLASS Global_SetEffect ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RefreshMap(bool onlyTiles, bool onlyRoof, bool onlyLight)", asFUNCTION( BIND_CLASS Global_RefreshMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MouseClick(int x, int y, int button)", asFUNCTION( BIND_CLASS Global_MouseClick ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void KeyboardPress(uint8 key1, uint8 key2, string key1Text = \"\", string key2Text = \"\")", asFUNCTION( BIND_CLASS Global_KeyboardPress ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRainAnimation(string fallAnimName, string dropAnimName)", asFUNCTION( BIND_CLASS Global_SetRainAnimation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeZoom(float targetZoom)", asFUNCTION( BIND_CLASS Global_ChangeZoom ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveScreenshot(string filePath)", asFUNCTION( BIND_CLASS Global_SaveScreenshot ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveText(string filePath, string text)", asFUNCTION( BIND_CLASS Global_SaveText ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheData(string name, const array<uint8>@+ data)", asFUNCTION( BIND_CLASS Global_SetCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheData(string name, const array<uint8>@+ data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetCacheDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint8>@ GetCacheData(string name)", asFUNCTION( BIND_CLASS Global_GetCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheDataStr(string name, string data)", asFUNCTION( BIND_CLASS Global_SetCacheDataStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetCacheDataStr(string name)", asFUNCTION( BIND_CLASS Global_GetCacheDataStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCacheData(string name)", asFUNCTION( BIND_CLASS Global_IsCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseCacheData(string name)", asFUNCTION( BIND_CLASS Global_EraseCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetUserConfig(array<string>@+ keyValues)", asFUNCTION( BIND_CLASS Global_SetUserConfig ), asCALL_CDECL ) );
#endif

#if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetFullSecond(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)", asFUNCTION( BIND_CLASS Global_GetFullSecond ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetTime(uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)", asFUNCTION( BIND_CLASS Global_GetTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetGameTime(uint fullSecond, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)", asFUNCTION( BIND_CLASS Global_GetGameTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetPropertyGetCallback(int propertyValue, ?&in func)", asFUNCTION( BIND_CLASS Global_SetPropertyGetCallback ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool AddPropertySetCallback(int propertyValue, ?&in func, bool deferred)", asFUNCTION( BIND_CLASS Global_AddPropertySetCallback ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __Year", &GameOpt.Year ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __Month", &GameOpt.Month ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __Day", &GameOpt.Day ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __Hour", &GameOpt.Hour ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __Minute", &GameOpt.Minute ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __Second", &GameOpt.Second ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint16 __TimeMultiplier", &GameOpt.TimeMultiplier ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const uint __FullSecond", &GameOpt.FullSecond ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "bool __Singleplayer", &GameOpt.Singleplayer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableTcpNagle", &GameOpt.DisableTcpNagle ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableZlibCompression", &GameOpt.DisableZlibCompression ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __FloodSize", &GameOpt.FloodSize ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __BruteForceTick", &GameOpt.BruteForceTick ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __NoAnswerShuffle", &GameOpt.NoAnswerShuffle ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DialogDemandRecheck", &GameOpt.DialogDemandRecheck ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __SneakDivider", &GameOpt.SneakDivider ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookMinimum", &GameOpt.LookMinimum ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __CritterIdleTick", &GameOpt.CritterIdleTick ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __DeadHitPoints", &GameOpt.DeadHitPoints ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Breaktime", &GameOpt.Breaktime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutTransfer", &GameOpt.TimeoutTransfer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutBattle", &GameOpt.TimeoutBattle ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RtAlwaysRun", &GameOpt.RtAlwaysRun ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RunOnCombat", &GameOpt.RunOnCombat ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RunOnTransfer", &GameOpt.RunOnTransfer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapWidth", &GameOpt.GlobalMapWidth ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapHeight", &GameOpt.GlobalMapHeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapZoneLength", &GameOpt.GlobalMapZoneLength ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __BagRefreshTime", &GameOpt.BagRefreshTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __WisperDist", &GameOpt.WhisperDist ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ShoutDist", &GameOpt.ShoutDist ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __LookChecks", &GameOpt.LookChecks ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookDir0", &GameOpt.LookDir[ 0 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookDir1", &GameOpt.LookDir[ 1 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookDir2", &GameOpt.LookDir[ 2 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookDir3", &GameOpt.LookDir[ 3 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookDir4", &GameOpt.LookDir[ 4 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookSneakDir0", &GameOpt.LookSneakDir[ 0 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookSneakDir1", &GameOpt.LookSneakDir[ 1 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookSneakDir2", &GameOpt.LookSneakDir[ 2 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookSneakDir3", &GameOpt.LookSneakDir[ 3 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookSneakDir4", &GameOpt.LookSneakDir[ 4 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RegistrationTimeout", &GameOpt.RegistrationTimeout ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AccountPlayTime", &GameOpt.AccountPlayTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ScriptRunSuspendTimeout", &GameOpt.ScriptRunSuspendTimeout ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ScriptRunMessageTimeout", &GameOpt.ScriptRunMessageTimeout ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TalkDistance", &GameOpt.TalkDistance ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __NpcMaxTalkers", &GameOpt.NpcMaxTalkers ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __MinNameLength", &GameOpt.MinNameLength ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __MaxNameLength", &GameOpt.MaxNameLength ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DlgTalkMinTime", &GameOpt.DlgTalkMinTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DlgBarterMinTime", &GameOpt.DlgBarterMinTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __MinimumOfflineTime", &GameOpt.MinimumOfflineTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __GameServer", &GameOpt.GameServer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __UpdateServer", &GameOpt.UpdateServer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __CommandLine", &GameOpt.CommandLine ) );
#endif

#ifdef BIND_MAPPER
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ AddChild(hash pid)", asFUNCTION( BIND_CLASS Item_AddChild ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ AddChild(hash pid) const", asFUNCTION( BIND_CLASS Crit_AddChild ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<Item@>@ GetChildren(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Item_GetChildren ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<const Item@>@ GetChildren(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Item_GetChildren ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetChildren(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Crit_GetChildren ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetChildren(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Crit_GetChildren ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ AddItem(hash protoId, uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_AddItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ AddCritter(hash protoId, uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_AddCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItemByHex(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_GetItemByHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetItemsByHex(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_GetItemsByHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritterByHex(uint16 hexX, uint16 hexY, int findType)", asFUNCTION( BIND_CLASS Global_GetCritterByHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersByHex(uint16 hexX, uint16 hexY, int findType)", asFUNCTION( BIND_CLASS Global_GetCrittersByHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveEntity(Entity@+ entity, uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Global_MoveEntity ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteEntity(Entity@+ entity)", asFUNCTION( BIND_CLASS Global_DeleteEntity ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteEntities(array<Entity@>@+ entities)", asFUNCTION( BIND_CLASS Global_DeleteEntities ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SelectEntity(Entity@+ entity, bool set)", asFUNCTION( BIND_CLASS Global_SelectEntity ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SelectEntities(array<Entity@>@+ entities, bool set)", asFUNCTION( BIND_CLASS Global_SelectEntities ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Entity@+ GetSelectedEntity()", asFUNCTION( BIND_CLASS Global_GetSelectedEntity ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<Entity@>@ GetSelectedEntities()", asFUNCTION( BIND_CLASS Global_GetSelectedEntities ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTilesCount(uint16 hexX, uint16 hexY, bool roof)", asFUNCTION( BIND_CLASS Global_GetTilesCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteTile(uint16 hexX, uint16 hexY, bool roof, int layer)", asFUNCTION( BIND_CLASS Global_DeleteTile ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetTile(uint16 hexX, uint16 hexY, bool roof, int layer)", asFUNCTION( BIND_CLASS Global_GetTileHash ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void AddTile(uint16 hexX, uint16 hexY, int offsX, int offsY, int layer, bool roof, hash picHash)", asFUNCTION( BIND_CLASS Global_AddTileHash ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetTileName(uint16 hexX, uint16 hexY, bool roof, int layer)", asFUNCTION( BIND_CLASS Global_GetTileName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void AddTileName(uint16 hexX, uint16 hexY, int offsX, int offsY, int layer, bool roof, string picName)", asFUNCTION( BIND_CLASS Global_AddTileName ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterObjectType( "MapperMap", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "MapperMap", asBEHAVE_ADDREF, "void f()", asMETHOD( ProtoMap, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "MapperMap", asBEHAVE_RELEASE, "void f()", asMETHOD( ProtoMap, Release ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "MapperMap@+ LoadMap(string fileName)", asFUNCTION( BIND_CLASS Global_LoadMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void UnloadMap(MapperMap@+ map)", asFUNCTION( BIND_CLASS Global_UnloadMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveMap(MapperMap@+ map, string customName = \"\")", asFUNCTION( BIND_CLASS Global_SaveMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool ShowMap(MapperMap@+ map)", asFUNCTION( BIND_CLASS Global_ShowMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<MapperMap@>@ GetLoadedMaps(int& index)", asFUNCTION( BIND_CLASS Global_GetLoadedMaps ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<string>@ GetMapFileNames(string dir)", asFUNCTION( BIND_CLASS Global_GetMapFileNames ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ResizeMap(uint16 width, uint16 height)", asFUNCTION( BIND_CLASS Global_ResizeMap ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetTileDirs(int tab, array<string>@+ dirNames, array<bool>@+ includeSubdirs)", asFUNCTION( BIND_CLASS Global_TabGetTileDirs ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetItemPids(int tab, string subTab, array<hash>@+ itemPids)", asFUNCTION( BIND_CLASS Global_TabGetItemPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetCritterPids(int tab, string subTab, array<hash>@+ critterPids)", asFUNCTION( BIND_CLASS Global_TabGetCritterPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetTileDirs(int tab, array<string>@+ dirNames, array<bool>@+ includeSubdirs)", asFUNCTION( BIND_CLASS Global_TabSetTileDirs ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetItemPids(int tab, string subTab, array<hash>@+ itemPids)", asFUNCTION( BIND_CLASS Global_TabSetItemPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetCritterPids(int tab, string subTab, array<hash>@+ critterPids)", asFUNCTION( BIND_CLASS Global_TabSetCritterPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabDelete(int tab)", asFUNCTION( BIND_CLASS Global_TabDelete ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSelect(int tab, string subTab, bool show = false)", asFUNCTION( BIND_CLASS Global_TabSelect ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetName(int tab, string tabName)", asFUNCTION( BIND_CLASS Global_TabSetName ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)", asFUNCTION( BIND_CLASS Global_GetHexInPath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathLengthHex ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string text)", asFUNCTION( BIND_CLASS Global_Message ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_MessageMsg ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void MapMessage(string text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)", asFUNCTION( BIND_CLASS Global_MapMessage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetMsgStrSkip ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumUpper(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumUpper ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumLower(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumLower ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrCount(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_IsMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, string str)", asFUNCTION( BIND_CLASS Global_ReplaceTextStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, int i)", asFUNCTION( BIND_CLASS Global_ReplaceTextInt ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreen(uint16 hexX, uint16 hexY, uint speed, bool canStop = false)", asFUNCTION( BIND_CLASS Global_MoveScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y)", asFUNCTION( BIND_CLASS Global_GetHexPos ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorHex ), asCALL_CDECL ) );
// BIND_ASSERT( engine->RegisterGlobalFunction( "MapperObject@ GetMonitorObject(int x, int y, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorObject ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)", asFUNCTION( BIND_CLASS Global_MoveHexByDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetIfaceIniStr(string key)", asFUNCTION( BIND_CLASS Global_GetIfaceIniStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadFont(int font, string fontFileName)", asFUNCTION( BIND_CLASS Global_LoadFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetDefaultFont(int font, uint color)", asFUNCTION( BIND_CLASS Global_SetDefaultFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MouseClick(int x, int y, int button)", asFUNCTION( BIND_CLASS Global_MouseClick ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void KeyboardPress(uint8 key1, uint8 key2, string key1Text = \"\", string key2Text = \"\")", asFUNCTION( BIND_CLASS Global_KeyboardPress ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRainAnimation(string fallAnimName, string dropAnimName)", asFUNCTION( BIND_CLASS Global_SetRainAnimation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeZoom(float targetZoom)", asFUNCTION( BIND_CLASS Global_ChangeZoom ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "string __ServerDir", &GameOpt.ServerDir ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowCorners", &GameOpt.ShowCorners ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowSpriteCuts", &GameOpt.ShowSpriteCuts ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowDrawOrder", &GameOpt.ShowDrawOrder ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __SplitTilesCollection", &GameOpt.SplitTilesCollection ) );
#endif

#if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
BIND_ASSERT( engine->RegisterGlobalFunction( "uint LoadSprite(string name)", asFUNCTION( BIND_CLASS Global_LoadSprite ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint LoadSprite(hash name)", asFUNCTION( BIND_CLASS Global_LoadSpriteHash ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSpriteWidth(uint sprId, int frameIndex)", asFUNCTION( BIND_CLASS Global_GetSpriteWidth ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSpriteHeight(uint sprId, int frameIndex)", asFUNCTION( BIND_CLASS Global_GetSpriteHeight ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSpriteCount(uint sprId)", asFUNCTION( BIND_CLASS Global_GetSpriteCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSpriteTicks(uint sprId)", asFUNCTION( BIND_CLASS Global_GetSpriteTicks ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPixelColor(uint sprId, int frameIndex, int x, int y)", asFUNCTION( BIND_CLASS Global_GetPixelColor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetTextInfo(string text, int w, int h, int font, int flags, int& tw, int& th, int& lines)", asFUNCTION( BIND_CLASS Global_GetTextInfo ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSprite(uint sprId, int frameIndex, int x, int y, uint color = 0, bool applyOffsets = false)", asFUNCTION( BIND_CLASS Global_DrawSprite ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSprite(uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom = false, uint color = 0, bool applyOffsets = false)", asFUNCTION( BIND_CLASS Global_DrawSpriteSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSpritePattern(uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color = 0)", asFUNCTION( BIND_CLASS Global_DrawSpritePattern ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawText(string text, int x, int y, int w, int h, uint color, int font, int flags)", asFUNCTION( BIND_CLASS Global_DrawText ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawPrimitive(int primitiveType, array<int>@+ data)", asFUNCTION( BIND_CLASS Global_DrawPrimitive ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawMapSpriteProto(uint16 hx, uint16 hy, uint sprId, int frameIndex, int offsX, int offsY, hash protoId)", asFUNCTION( BIND_CLASS Global_DrawMapSpriteProto ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawMapSpriteExt(uint16 hx, uint16 hy, uint sprId, int frameIndex, int offsX, int offsY, bool isFlat, bool noLight, int drawOrder, int drawOrderHyOffset, int corner, bool disableEgg, uint color, uint contourColor)", asFUNCTION( BIND_CLASS Global_DrawMapSpriteExt ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawCritter2d(hash modelName, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)", asFUNCTION( BIND_CLASS Global_DrawCritter2d ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawCritter3d(uint instance, hash modelName, uint anim1, uint anim2, const array<int>@+ layers, const array<float>@+ position, uint color)", asFUNCTION( BIND_CLASS Global_DrawCritter3d ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void PushDrawScissor(int x, int y, int w, int h)", asFUNCTION( BIND_CLASS Global_PushDrawScissor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void PopDrawScissor()", asFUNCTION( BIND_CLASS Global_PopDrawScissor ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "bool __Quit", &GameOpt.Quit ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __WaitPing", &GameOpt.WaitPing ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __OpenGLRendering", &GameOpt.OpenGLRendering ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __OpenGLDebug", &GameOpt.OpenGLDebug ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __AssimpLogging", &GameOpt.AssimpLogging ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MouseX", &GameOpt.MouseX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MouseY", &GameOpt.MouseY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint8 __RoofAlpha", &GameOpt.RoofAlpha ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __HideCursor", &GameOpt.HideCursor ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowMoveCursor", &GameOpt.ShowMoveCursor ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const int __ScreenWidth", &GameOpt.ScreenWidth ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const int __ScreenHeight", &GameOpt.ScreenHeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MultiSampling", &GameOpt.MultiSampling ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableMouseEvents", &GameOpt.DisableMouseEvents ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableKeyboardEvents", &GameOpt.DisableKeyboardEvents ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __HidePassword", &GameOpt.HidePassword ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __PlayerOffAppendix", &GameOpt.PlayerOffAppendix ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DamageHitDelay", &GameOpt.DamageHitDelay ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowTile", &GameOpt.ShowTile ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowRoof", &GameOpt.ShowRoof ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowItem", &GameOpt.ShowItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowScen", &GameOpt.ShowScen ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowWall", &GameOpt.ShowWall ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowCrit", &GameOpt.ShowCrit ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowFast", &GameOpt.ShowFast ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowPlayerNames", &GameOpt.ShowPlayerNames ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowNpcNames", &GameOpt.ShowNpcNames ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowCritId", &GameOpt.ShowCritId ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollKeybLeft", &GameOpt.ScrollKeybLeft ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollKeybRight", &GameOpt.ScrollKeybRight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollKeybUp", &GameOpt.ScrollKeybUp ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollKeybDown", &GameOpt.ScrollKeybDown ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollMouseLeft", &GameOpt.ScrollMouseLeft ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollMouseRight", &GameOpt.ScrollMouseRight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollMouseUp", &GameOpt.ScrollMouseUp ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollMouseDown", &GameOpt.ScrollMouseDown ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowGroups", &GameOpt.ShowGroups ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __HelpInfo", &GameOpt.HelpInfo ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DebugInfo", &GameOpt.DebugInfo ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __Enable3dRendering", &GameOpt.Enable3dRendering ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __FullScr", &GameOpt.FullScreen ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __VSync", &GameOpt.VSync ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __Light", &GameOpt.Light ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ScrollDelay", &GameOpt.ScrollDelay ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ScrollStep", &GameOpt.ScrollStep ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MouseScroll", &GameOpt.MouseScroll ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollCheck", &GameOpt.ScrollCheck ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __Host", &GameOpt.Host ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Port", &GameOpt.Port ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __UpdateServerHost", &GameOpt.UpdateServerHost ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __UpdateServerPort", &GameOpt.UpdateServerPort ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ProxyType", &GameOpt.ProxyType ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __ProxyHost", &GameOpt.ProxyHost ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ProxyPort", &GameOpt.ProxyPort ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __ProxyUser", &GameOpt.ProxyUser ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __ProxyPass", &GameOpt.ProxyPass ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __Name", &GameOpt.Name ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TextDelay", &GameOpt.TextDelay ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __AlwaysOnTop", &GameOpt.AlwaysOnTop ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __FixedFPS", &GameOpt.FixedFPS ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __FPS", &GameOpt.FPS ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __PingPeriod", &GameOpt.PingPeriod ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Ping", &GameOpt.Ping ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MsgboxInvert", &GameOpt.MsgboxInvert ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MessNotify", &GameOpt.MessNotify ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __SoundNotify", &GameOpt.SoundNotify ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DoubleClickTime", &GameOpt.DoubleClickTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __RunModMul", &GameOpt.RunModMul ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __RunModDiv", &GameOpt.RunModDiv ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __RunModAdd", &GameOpt.RunModAdd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Animation3dSmoothTime", &GameOpt.Animation3dSmoothTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Animation3dFPS", &GameOpt.Animation3dFPS ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MapZooming", &GameOpt.MapZooming ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __SpritesZoom", &GameOpt.SpritesZoom ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __SpritesZoomMin", &GameOpt.SpritesZoomMin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __SpritesZoomMax", &GameOpt.SpritesZoomMax ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue0", &GameOpt.EffectValues[ 0 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue1", &GameOpt.EffectValues[ 1 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue2", &GameOpt.EffectValues[ 2 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue3", &GameOpt.EffectValues[ 3 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue4", &GameOpt.EffectValues[ 4 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue5", &GameOpt.EffectValues[ 5 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue6", &GameOpt.EffectValues[ 6 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue7", &GameOpt.EffectValues[ 7 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue8", &GameOpt.EffectValues[ 8 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __EffectValue9", &GameOpt.EffectValues[ 9 ] ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __AlwaysRun", &GameOpt.AlwaysRun ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AlwaysRunMoveDist", &GameOpt.AlwaysRunMoveDist ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AlwaysRunUseDist", &GameOpt.AlwaysRunUseDist ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __CritterFidgetTime", &GameOpt.CritterFidgetTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Anim2CombatBegin", &GameOpt.Anim2CombatBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Anim2CombatIdle", &GameOpt.Anim2CombatIdle ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Anim2CombatEnd", &GameOpt.Anim2CombatEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ConsoleHistorySize", &GameOpt.ConsoleHistorySize ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SoundVolume", &GameOpt.SoundVolume ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MusicVolume", &GameOpt.MusicVolume ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __RegName", &GameOpt.RegName ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __RegPassword", &GameOpt.RegPassword ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ChosenLightColor", &GameOpt.ChosenLightColor ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint8 __ChosenLightDistance", &GameOpt.ChosenLightDistance ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ChosenLightIntensity", &GameOpt.ChosenLightIntensity ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint8 __ChosenLightFlags", &GameOpt.ChosenLightFlags ) );
#endif

BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MapHexagonal", &GameOpt.MapHexagonal ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapHexWidth", &GameOpt.MapHexWidth ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapHexHeight", &GameOpt.MapHexHeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapHexLineHeight", &GameOpt.MapHexLineHeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapTileOffsX", &GameOpt.MapTileOffsX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapTileOffsY", &GameOpt.MapTileOffsY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapTileStep", &GameOpt.MapTileStep ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapRoofOffsX", &GameOpt.MapRoofOffsX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapRoofOffsY", &GameOpt.MapRoofOffsY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MapRoofSkipSize", &GameOpt.MapRoofSkipSize ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __MapCameraAngle", &GameOpt.MapCameraAngle ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MapSmoothPath", &GameOpt.MapSmoothPath ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string __MapDataPrefix", &GameOpt.MapDataPrefix ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadDataFile(string dataFileName)", asFUNCTION( BIND_CLASS Global_LoadDataFile ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void AllowSlot(uint8 index, bool enableSend)", asFUNCTION( BIND_CLASS Global_AllowSlot ), asCALL_CDECL ) );

// ScriptFunctions.h
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_Assert ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message)", asFUNCTION( Global_ThrowException ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in)", asFUNCTION( Global_ThrowException ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in)", asFUNCTION( Global_ThrowException ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_ThrowException ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_ThrowException ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in)", asFUNCTION( Global_ThrowException ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int Random(int min, int max)", asFUNCTION( Global_Random ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Log(string text)", asFUNCTION( Global_Log ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool StrToInt(string text, int& result)", asFUNCTION( Global_StrToInt ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool StrToFloat(string text, float& result)", asFUNCTION( Global_StrToFloat ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDistantion(uint16 hexX1, uint16 hexY1, uint16 hexX2, uint16 hexY2)", asFUNCTION( Global_GetDistantion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 GetDirection(uint16 fromHexX, uint16 fromHexY, uint16 toHexX, uint16 toHexY)", asFUNCTION( Global_GetDirection ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 GetOffsetDir(uint16 fromHexX, uint16 fromHexY, uint16 toHexX, uint16 toHexY, float offset)", asFUNCTION( Global_GetOffsetDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTick()", asFUNCTION( Global_GetTick ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAngelScriptProperty(int property)", asFUNCTION( Global_GetAngelScriptProperty ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetAngelScriptProperty(int property, uint value)", asFUNCTION( Global_SetAngelScriptProperty ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetStrHash(string str)", asFUNCTION( Global_GetStrHash ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string GetHashStr(hash h)", asFUNCTION( Global_GetHashStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DecodeUTF8(string text, uint& length)", asFUNCTION( Global_DecodeUTF8 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string EncodeUTF8(uint ucs)", asFUNCTION( Global_EncodeUTF8 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "array<string>@ GetFolderFileNames(string path, string extension, bool includeSubdirs)", asFUNCTION( Global_GetFolderFileNames ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool DeleteFile(string fileName)", asFUNCTION( Global_DeleteFile ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void CreateDirectoryTree(string path)", asFUNCTION( Global_CreateDirectoryTree ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Yield(uint time)", asFUNCTION( Global_Yield ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string SHA1(string text)", asFUNCTION( Global_SHA1 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string SHA2(string text)", asFUNCTION( Global_SHA2 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void OpenLink(string link)", asFUNCTION( Global_OpenLink ), asCALL_CDECL ) );

// Invoker
BIND_ASSERT( engine->RegisterFuncdef( "void CallFunc()" ) );
BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithIValue(int value)" ) );
BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithUValue(uint value)" ) );
BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithIValues(array<int>@+ values)" ) );
BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithUValues(array<uint>@+ values)" ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFunc@+ func)", asFUNCTION( ScriptInvoker::Global_DeferredCall ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithIValue@+ func, int value)", asFUNCTION( ScriptInvoker::Global_DeferredCallWithValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithUValue@+ func, uint value)", asFUNCTION( ScriptInvoker::Global_DeferredCallWithValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithIValues@+ func, const array<int>@+ values)", asFUNCTION( ScriptInvoker::Global_DeferredCallWithValues ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithUValues@+ func, const array<uint>@+ values)", asFUNCTION( ScriptInvoker::Global_DeferredCallWithValues ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsDeferredCallPending(uint id)", asFUNCTION( ScriptInvoker::Global_IsDeferredCallPending ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool CancelDeferredCall(uint id)", asFUNCTION( ScriptInvoker::Global_CancelDeferredCall ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetDeferredCallData(uint id, uint& delay, array<int>@+ values)", asFUNCTION( ScriptInvoker::Global_GetDeferredCallData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetDeferredCallData(uint id, uint& delay, array<uint>@+ values)", asFUNCTION( ScriptInvoker::Global_GetDeferredCallData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDeferredCallsList(array<uint>@+ ids)", asFUNCTION( ScriptInvoker::Global_GetDeferredCallsList ), asCALL_CDECL ) );
#ifdef BIND_SERVER
BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFunc@+ func)", asFUNCTION( ScriptInvoker::Global_SavedDeferredCall ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithIValue@+ func, int value)", asFUNCTION( ScriptInvoker::Global_SavedDeferredCallWithValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithUValue@+ func, uint value)", asFUNCTION( ScriptInvoker::Global_SavedDeferredCallWithValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithIValues@+ func, const array<int>@+ values)", asFUNCTION( ScriptInvoker::Global_SavedDeferredCallWithValues ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithUValues@+ func, const array<uint>@+ values)", asFUNCTION( ScriptInvoker::Global_SavedDeferredCallWithValues ), asCALL_CDECL ) );
#endif

#define BIND_ASSERT_EXT( expr )    BIND_ASSERT( ( expr ) ? 0 : -1 )
BIND_ASSERT_EXT( registrators[ 0 ]->Init() );
BIND_ASSERT_EXT( registrators[ 1 ]->Init() );
BIND_ASSERT_EXT( registrators[ 2 ]->Init() );
BIND_ASSERT_EXT( registrators[ 3 ]->Init() );
BIND_ASSERT_EXT( registrators[ 4 ]->Init() );

#if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
BIND_ASSERT( engine->RegisterGlobalFunction( "void AddRegistrationProperty(CritterProperty prop)", asFUNCTION( BIND_CLASS Global_AddRegistrationProperty ), asCALL_CDECL ) );

void* reg_props = engine->CreateScriptObject( engine->GetTypeInfoByDecl( "array<CritterProperty>" ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "array<CritterProperty>@ CritterPropertyRegProperties", new void*( reg_props ) ) ); // Todo: Leak
#endif
#if defined ( BIND_SERVER )
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ AddNpc(hash protoId, uint16 hexX, uint16 hexY, uint8 dir, dict<CritterProperty, int>@+ props = null)", asFUNCTION( BIND_CLASS Map_AddNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ AddItem(uint16 hexX, uint16 hexY, hash protoId, uint count, dict<ItemProperty, int>@+ props = null)", asFUNCTION( BIND_CLASS Map_AddItem ), asCALL_CDECL_OBJFIRST ) );
#endif

BIND_ASSERT( engine->RegisterGlobalFunction( "const Item@ GetProtoItem(hash protoId, dict<ItemProperty, int>@+ props = null)", asFUNCTION( Global_GetProtoItem ), asCALL_CDECL ) );

/************************************************************************/
/*                                                                      */
/************************************************************************/
