#ifdef BIND_DUMMY_DATA
# include "DummyData.h"
# pragma push_macro( "SCRIPT_FUNC" )
# pragma push_macro( "SCRIPT_FUNC_THIS" )
# pragma push_macro( "SCRIPT_METHOD" )
# pragma push_macro( "SCRIPT_FUNC_CONV" )
# pragma push_macro( "SCRIPT_FUNC_THIS_CONV" )
# pragma push_macro( "SCRIPT_METHOD_CONV" )
# undef SCRIPT_FUNC
# undef SCRIPT_FUNC_THIS
# undef SCRIPT_METHOD
# undef SCRIPT_FUNC_CONV
# undef SCRIPT_FUNC_THIS_CONV
# undef SCRIPT_METHOD_CONV
# define SCRIPT_FUNC( ... )         asFUNCTION( DummyFunc )
# define SCRIPT_FUNC_THIS( ... )    asFUNCTION( DummyFunc )
# define SCRIPT_METHOD( ... )       asFUNCTION( DummyFunc )
# define SCRIPT_FUNC_CONV         asCALL_GENERIC
# define SCRIPT_FUNC_THIS_CONV    asCALL_GENERIC
# define SCRIPT_METHOD_CONV       asCALL_GENERIC
static void DummyFunc( asIScriptGeneric* gen ) {}
#endif

#include "ScriptFunctions.h"

#ifndef BIND_CLASS_EXT
# define BIND_CLASS_EXT           BIND_CLASS
#endif

static int Bind( asIScriptEngine* engine, PropertyRegistrator** registrators )
{
    int errors = 0;
    BIND_ASSERT( engine->RegisterTypedef( "hash", "uint" ) );
    BIND_ASSERT( engine->RegisterTypedef( "resource", "uint" ) );
    // Todo: register new type with automating string - number convertation, exclude GetStrHash
    // BIND_ASSERT( engine->RegisterObjectType( "hash", 0, asOBJ_VALUE | asOBJ_POD ) );

    // Entity
    #define REGISTER_ENTITY( class_name, real_class )                                                                                                         \
        BIND_ASSERT( engine->RegisterObjectType( class_name, 0, asOBJ_REF ) );                                                                                \
        BIND_ASSERT( engine->RegisterObjectBehaviour( class_name, asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD( real_class, AddRef ), SCRIPT_METHOD_CONV ) );   \
        BIND_ASSERT( engine->RegisterObjectBehaviour( class_name, asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD( real_class, Release ), SCRIPT_METHOD_CONV ) ); \
        BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const uint Id", OFFSETOF( real_class, Id ) ) );                                             \
        BIND_ASSERT( engine->RegisterObjectMethod( class_name, "hash get_ProtoId() const", SCRIPT_METHOD( real_class, GetProtoId ), SCRIPT_METHOD_CONV ) );   \
        BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const bool IsDestroyed", OFFSETOF( real_class, IsDestroyed ) ) );                           \
        BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const bool IsDestroying", OFFSETOF( real_class, IsDestroying ) ) );                         \
        BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const int RefCounter", OFFSETOF( real_class, RefCounter ) ) );
    /*BIND_ASSERT( engine->RegisterObjectProperty( class_name, "Entity@ Parent", OFFSETOF( real_class, Parent ) ) );*/
    /*BIND_ASSERT( engine->RegisterObjectProperty( class_name, "const array<Entity@> Children", OFFSETOF( real_class, Children ) ) );*/
    #define REGISTER_ENTITY_CAST( class_name, real_class )                                                                                                                         \
        BIND_ASSERT( engine->RegisterObjectMethod( "Entity", class_name "@ opCast()", SCRIPT_FUNC_THIS( ( EntityUpCast< real_class >) ), SCRIPT_FUNC_THIS_CONV ) );                \
        BIND_ASSERT( engine->RegisterObjectMethod( "Entity", "const " class_name "@ opCast() const", SCRIPT_FUNC_THIS( ( EntityUpCast< real_class >) ), SCRIPT_FUNC_THIS_CONV ) ); \
        BIND_ASSERT( engine->RegisterObjectMethod( class_name, "Entity@ opImplCast()", SCRIPT_FUNC_THIS( ( EntityDownCast< real_class >) ), SCRIPT_FUNC_THIS_CONV ) );             \
        BIND_ASSERT( engine->RegisterObjectMethod( class_name, "const Entity@ opImplCast() const", SCRIPT_FUNC_THIS( ( EntityDownCast< real_class >) ), SCRIPT_FUNC_THIS_CONV ) );
    REGISTER_ENTITY( "Entity", Entity );

    // Global vars
    BIND_ASSERT( engine->RegisterObjectType( "GlobalVars", 0, asOBJ_REF | asOBJ_NOCOUNT ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "GlobalVars@ Globals", &Globals ) );

    // Map and location for client and mapper
    #if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
    REGISTER_ENTITY( "Map", Map );
    REGISTER_ENTITY_CAST( "Map", Map );
    REGISTER_ENTITY( "Location", Location );
    REGISTER_ENTITY_CAST( "Location", Location );
    #endif
    #if defined ( BIND_CLIENT )
    BIND_ASSERT( engine->RegisterGlobalProperty( "Map@ CurMap", &BIND_CLASS ClientCurMap ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "Location@ CurLocation", &BIND_CLASS ClientCurLocation ) );
    #endif

    #ifdef BIND_SERVER
    /************************************************************************/
    /* Types                                                                */
    /************************************************************************/
    REGISTER_ENTITY( "Item", Item );
    REGISTER_ENTITY_CAST( "Item", Item );
    REGISTER_ENTITY( "Critter", Critter );
    REGISTER_ENTITY_CAST( "Critter", Critter );
    REGISTER_ENTITY( "Map", Map );
    REGISTER_ENTITY_CAST( "Map", Map );
    REGISTER_ENTITY( "Location", Location );
    REGISTER_ENTITY_CAST( "Location", Location );

    /************************************************************************/
    /* Item                                                                 */
    /************************************************************************/
    BIND_ASSERT( engine->RegisterFuncdef( "bool ItemPredicate(const Item@+)" ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void ItemInitFunc(Item@+, bool)" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool SetScript(ItemInitFunc@+ func)", SCRIPT_FUNC_THIS( BIND_CLASS Item_SetScript ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ AddItem(hash protoId, uint count, uint stackId)", SCRIPT_FUNC_THIS( BIND_CLASS Item_AddItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<Item@>@ GetItems(uint stackId)", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<const Item@>@ GetItems(uint stackId) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Map@+ GetMapPosition(uint16& hexX, uint16& hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetMapPosition ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool ChangeProto(hash protoId)", SCRIPT_FUNC_THIS( BIND_CLASS Item_ChangeProto ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void Animate(uint fromFrame, uint toFrame)", SCRIPT_FUNC_THIS( BIND_CLASS Item_Animate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool CallStaticItemFunction(Critter@+ cr, Item@+ item, int param) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_CallStaticItemFunction ), SCRIPT_FUNC_THIS_CONV ) );

    /************************************************************************/
    /* Critter                                                              */
    /************************************************************************/
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsPlayer() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsPlayer ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNpc() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsNpc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetAccess() const", SCRIPT_FUNC_THIS( BIND_CLASS Cl_GetAccess ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetAccess(int access)", SCRIPT_FUNC_THIS( BIND_CLASS Cl_SetAccess ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Map@+ GetMap()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetMap ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Map@+ GetMap() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetMap ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool MoveToDir(uint8 dir)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_MoveToDir ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void TransitToHex(uint16 hexX, uint16 hexY, uint8 dir)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_TransitToHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void TransitToMap(Map@+ map, uint16 hexX, uint16 hexY, uint8 dir)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_TransitToMapHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void TransitToGlobal()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_TransitToGlobal ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void TransitToGlobal(array<Critter@>@+ group)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_TransitToGlobalWithGroup ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void TransitToGlobalGroup(Critter@+ leader)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_TransitToGlobalGroup ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsLife() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsLife ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnockout() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsKnockout ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsDead() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsDead ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsFree() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsFree ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsBusy() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsBusy ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Wait(uint ms)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_Wait ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void RefreshVisible()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_RefreshVisible ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ViewMap(Map@+ map, uint look, uint16 hx, uint16 hy, uint8 dir)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_ViewMap ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint CountItem(hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_CountItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool DeleteItem(hash protoId, uint count)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_DeleteItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ AddItem(hash protoId, uint count)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_AddItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(uint itemId)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItem(uint itemId) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemByPid(hash protoId)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemByPid(hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemBySlot(uint8 slot)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemBySlot(uint8 slot) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItem(ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItems()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItems() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItemsBySlot(uint8 slot)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItemsBySlot(uint8 slot) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItems(ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItems(ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ChangeItemSlot(uint itemId, uint8 slot)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_ChangeItemSlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetCond(int cond)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetCond ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void CloseDialog()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_CloseDialog ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Critter@>@ GetCritters(bool lookOnMe, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetCritters ), SCRIPT_FUNC_THIS_CONV ) ); // Todo: const
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Critter@>@ GetTalkedPlayers() const", SCRIPT_FUNC_THIS( BIND_CLASS Npc_GetTalkedPlayers ), SCRIPT_FUNC_THIS_CONV ) );                   // Todo: const
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSee(Critter@+ cr) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsSeeCr ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSeenBy(Critter@+ cr) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsSeenByCr ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSee(Item@+ item) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsSeeItem ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Say(uint8 howSay, string text)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_Say ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SayMsg(uint8 howSay, uint16 textMsg, uint strNum)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SayMsg ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SayMsg(uint8 howSay, uint16 textMsg, uint strNum, string lexems)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SayMsgLex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetDir(uint8 dir)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetDir ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Action(int action, int actionExt, const Item@+ item)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_Action ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2, const Item@+ item, bool clearSequence, bool delayPlay)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_Animate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetAnims(int cond, uint anim1, uint anim2)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetAnims ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void PlaySound(string soundName, bool sendSelf)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_PlaySound ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendCombatResult(array<uint>@+ combatResult)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SendCombatResult ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnownLoc(bool byId, uint locNum) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsKnownLoc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetKnownLoc(bool byId, uint locNum)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetKnownLoc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool UnsetKnownLoc(bool byId, uint locNum)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_UnsetKnownLoc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetFog(uint16 zoneX, uint16 zoneY, int fog)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetFog ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetFog(uint16 zoneX, uint16 zoneY) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetFog ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendItems(const array<const Item@>@+ items, int param = 0)", SCRIPT_FUNC_THIS( BIND_CLASS Cl_SendItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendItems(const array<Item@>@+ items, int param = 0)", SCRIPT_FUNC_THIS( BIND_CLASS Cl_SendItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Disconnect()", SCRIPT_FUNC_THIS( BIND_CLASS Cl_Disconnect ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsOnline()", SCRIPT_FUNC_THIS( BIND_CLASS Cl_IsOnline ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterFuncdef( "void CritterInitFunc(Critter@+, bool)" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetScript(CritterInitFunc@+ func)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetScript ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterFuncdef( "uint TimeEventFunc(Critter@+,int,uint&)" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddTimeEvent(TimeEventFunc@+ func, uint duration, int identifier)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_AddTimeEvent ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddTimeEvent(TimeEventFunc@+ func, uint duration, int identifier, uint rate)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_AddTimeEventRate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTimeEvents(int identifier, array<uint>@+ indexes, array<uint>@+ durations, array<uint>@+ rates) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetTimeEvents ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTimeEvents(array<int>@+ findIdentifiers, array<int>@+ identifiers, array<uint>@+ indexes, array<uint>@+ durations, array<uint>@+ rates) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetTimeEventsArr ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ChangeTimeEvent(uint index, uint newDuration, uint newRate)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_ChangeTimeEvent ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EraseTimeEvent(uint index)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_EraseTimeEvent ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint EraseTimeEvents(int identifier)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_EraseTimeEvents ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint EraseTimeEvents(array<int>@+ identifiers)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_EraseTimeEventsArr ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void MoveToCritter(Critter@+ cr, uint cut, bool isRun)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_MoveToCritter ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void MoveToHex(uint16 hexX, uint16 hexY, uint cut, bool isRun)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_MoveToHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterEnum( "MovingState" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "MovingState GetMovingState() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetMovingState ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ResetMovingState(uint& gagId)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_ResetMovingState ), SCRIPT_FUNC_THIS_CONV ) );

    // Parameters
    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const string Name", OFFSETOF( Critter, Name ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const bool IsRunning", OFFSETOF( Critter, IsRunning ) ) );

    /************************************************************************/
    /* Map                                                                  */
    /************************************************************************/
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Location@+ GetLocation()", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetLocation ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Location@+ GetLocation() const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetLocation ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void MapInitFunc(Map@+, bool)" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool SetScript(MapInitFunc@+ func)", SCRIPT_FUNC_THIS( BIND_CLASS Map_SetScript ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetItem(uint itemId)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetItem(uint itemId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetItem(uint16 hexX, uint16 hexY, hash protoId)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetItem(uint16 hexX, uint16 hexY, hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems()", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems() const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(uint16 hexX, uint16 hexY)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(uint16 hexX, uint16 hexY, uint radius, hash protoId)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHexEx ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(uint16 hexX, uint16 hexY, uint radius, hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHexEx ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItemsByPid(hash protoId)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItemsByPid(hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(uint16 hexX, uint16 hexY, ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHexPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(uint16 hexX, uint16 hexY, ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHexPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Item@>@ GetItems(uint16 hexX, uint16 hexY, uint radius, ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHexRadiusPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetItems(uint16 hexX, uint16 hexY, uint radius, ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetItemsHexRadiusPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Item@+ GetStaticItem(uint16 hexX, uint16 hexY, hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetStaticItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetStaticItems(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetStaticItemsHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetStaticItems(uint16 hexX, uint16 hexY, uint radius, hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetStaticItemsHexEx ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetStaticItems(hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetStaticItemsByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetStaticItems(ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetStaticItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Item@>@ GetStaticItems() const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetStaticItemsAll ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetCritter(uint16 hexX, uint16 hexY)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCritterHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Critter@+ GetCritter(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCritterHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetCritter(uint critterId)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCritterById ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Critter@+ GetCritter(uint critterId) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCritterById ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCritters ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCritters ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCritters(hash pid, int findType)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersByPids ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCritters(hash pid, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersByPids ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersInPath ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersInPath ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersInPathBlock ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersInPathBlock ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersWhoViewPath ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersWhoViewPath ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersSeeing(array<Critter@>@+ critters, bool lookOnThem, int findType)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersSeeing ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersSeeing(array<const Critter@>@+ critters, bool lookOnThem, int findType)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersSeeing ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<Critter@>@ GetCrittersSeeing(array<Critter@>@+ critters, bool lookOnThem, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersSeeing ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "array<const Critter@>@ GetCrittersSeeing(array<const Critter@>@+ critters, bool lookOnThem, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetCrittersSeeing ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetHexInPath ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetHexCoordWall(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetHexInPathWall ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetPathLengthHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetPathLength(Critter@+ cr, uint16 toHx, uint16 toHy, uint cut) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetPathLengthCr ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void VerifyTrigger(Critter@+ cr, uint16 hexX, uint16 hexY, uint8 dir)", SCRIPT_FUNC_THIS( BIND_CLASS Map_VerifyTrigger ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetNpcCount(int npcRole, int findType) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetNpcCount ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetNpc(int npcRole, int findType, uint skipCount)", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetNpc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "const Critter@+ GetNpc(int npcRole, int findType, uint skipCount) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_GetNpc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexPassed(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_IsHexPassed ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexesPassed(uint16 hexX, uint16 hexY, uint radius) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_IsHexesPassed ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexRaked(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_IsHexRaked ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetText(uint16 hexX, uint16 hexY, uint color, string text)", SCRIPT_FUNC_THIS( BIND_CLASS Map_SetText ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum)", SCRIPT_FUNC_THIS( BIND_CLASS Map_SetTextMsg ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum, string lexems)", SCRIPT_FUNC_THIS( BIND_CLASS Map_SetTextMsgLex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void RunEffect(hash effectPid, uint16 hexX, uint16 hexY, uint16 radius)", SCRIPT_FUNC_THIS( BIND_CLASS Map_RunEffect ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void RunFlyEffect(hash effectPid, Critter@+ fromCr, Critter@+ toCr, uint16 fromX, uint16 fromY, uint16 toX, uint16 toY)", SCRIPT_FUNC_THIS( BIND_CLASS Map_RunFlyEffect ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool CheckPlaceForItem(uint16 hexX, uint16 hexY, hash pid) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_CheckPlaceForItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void BlockHex(uint16 hexX, uint16 hexY, bool full)", SCRIPT_FUNC_THIS( BIND_CLASS Map_BlockHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void UnblockHex(uint16 hexX, uint16 hexY)", SCRIPT_FUNC_THIS( BIND_CLASS Map_UnblockHex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void PlaySound(string soundName)", SCRIPT_FUNC_THIS( BIND_CLASS Map_PlaySound ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void PlaySound(string soundName, uint16 hexX, uint16 hexY, uint radius)", SCRIPT_FUNC_THIS( BIND_CLASS Map_PlaySoundRadius ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool Reload()", SCRIPT_FUNC_THIS( BIND_CLASS Map_Reload ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps) const", SCRIPT_FUNC_THIS( BIND_CLASS Map_MoveHexByDir ), SCRIPT_FUNC_THIS_CONV ) );

    /************************************************************************/
    /* Location                                                             */
    /************************************************************************/
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetMapCount() const", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMapCount ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "Map@+ GetMap(hash mapPid)", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMap ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "const Map@+ GetMap(hash mapPid) const", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMap ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "Map@+ GetMapByIndex(uint index)", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMapByIndex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "const Map@+ GetMapByIndex(uint index) const", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMapByIndex ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "array<Map@>@ GetMaps()", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMaps ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "array<const Map@>@ GetMaps() const", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetMaps ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool GetEntrance(uint entrance, uint& mapIndex, hash& entire) const", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetEntrance ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetEntrances(array<uint>@+ mapsIndex, array<hash>@+ entires) const", SCRIPT_FUNC_THIS( BIND_CLASS Location_GetEntrances ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool Reload()", SCRIPT_FUNC_THIS( BIND_CLASS Location_Reload ), SCRIPT_FUNC_THIS_CONV ) );

    /************************************************************************/
    /* Global                                                               */
    /************************************************************************/
    BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItem(uint itemId)", SCRIPT_FUNC( BIND_CLASS Global_GetItem ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item@+ item, uint count, Critter@+ toCr, bool skipChecks = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveItemCr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item@+ item, uint count, Item@+ toCont, uint stackId, bool skipChecks = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveItemCont ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item@+ item, uint count, Map@+ toMap, uint16 toHx, uint16 toHy, bool skipChecks = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveItemMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(const array<Item@>@+ items, Critter@+ toCr, bool skipChecks = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveItemsCr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(const array<Item@>@+ items, Item@+ toCont, uint stackId, bool skipChecks = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveItemsCont ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(const array<Item@>@+ items, Map@+ toMap, uint16 toHx, uint16 toHy, bool skipChecks = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveItemsMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItem(Item@+ item)", SCRIPT_FUNC( BIND_CLASS Global_DeleteItem ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItem(uint itemId)", SCRIPT_FUNC( BIND_CLASS Global_DeleteItemById ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItems(array<Item@>@+ items)", SCRIPT_FUNC( BIND_CLASS Global_DeleteItems ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItems(array<uint>@+ itemIds)", SCRIPT_FUNC( BIND_CLASS Global_DeleteItemsById ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteNpc(Critter@+ npc)", SCRIPT_FUNC( BIND_CLASS Global_DeleteNpc ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteNpc(uint npcId)", SCRIPT_FUNC( BIND_CLASS Global_DeleteNpcById ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersDistantion(const Critter@+ cr1, const Critter@+ cr2)", SCRIPT_FUNC( BIND_CLASS Global_GetCrittersDistantion ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessage(uint16 channel, string text)", SCRIPT_FUNC( BIND_CLASS Global_RadioMessage ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_RadioMessageMsg ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum, string lexems)", SCRIPT_FUNC( BIND_CLASS Global_RadioMessageMsgLex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ CreateLocation(hash locPid, uint16 worldX, uint16 worldY, array<Critter@>@+ critters = null)", SCRIPT_FUNC( BIND_CLASS Global_CreateLocation ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteLocation(Location@+ loc)", SCRIPT_FUNC( BIND_CLASS Global_DeleteLocation ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteLocation(uint locId)", SCRIPT_FUNC( BIND_CLASS Global_DeleteLocationById ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritter(uint critterId)", SCRIPT_FUNC( BIND_CLASS Global_GetCritter ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "const Critter@ GetPlayer(string name)", SCRIPT_FUNC( BIND_CLASS Global_GetPlayer ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetGlobalMapCritters(uint16 worldX, uint16 worldY, uint radius, int findType)", SCRIPT_FUNC( BIND_CLASS Global_GetGlobalMapCritters ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ GetMap(uint mapId)", SCRIPT_FUNC( BIND_CLASS Global_GetMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ GetMapByPid(hash mapPid, uint skipCount)", SCRIPT_FUNC( BIND_CLASS Global_GetMapByPid ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ GetLocation(uint locId)", SCRIPT_FUNC( BIND_CLASS Global_GetLocation ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ GetLocationByPid(hash locPid, uint skipCount)", SCRIPT_FUNC( BIND_CLASS Global_GetLocationByPid ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Location@>@ GetLocations(uint16 worldX, uint16 worldY, uint radius)", SCRIPT_FUNC( BIND_CLASS Global_GetLocations ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Location@>@ GetVisibleLocations(uint16 worldX, uint16 worldY, uint radius, Critter@+ visibleBy)", SCRIPT_FUNC( BIND_CLASS Global_GetVisibleLocations ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint>@ GetZoneLocationIds(uint16 zoneX, uint16 zoneY, uint zoneRadius)", SCRIPT_FUNC( BIND_CLASS Global_GetZoneLocationIds ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter@+ player, Critter@+ npc, bool ignoreDistance)", SCRIPT_FUNC( BIND_CLASS Global_RunDialogNpc ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter@+ player, Critter@+ npc, uint dialogPack, bool ignoreDistance)", SCRIPT_FUNC( BIND_CLASS Global_RunDialogNpcDlgPack ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter@+ player, uint dialogPack, uint16 hexX, uint16 hexY, bool ignoreDistance)", SCRIPT_FUNC( BIND_CLASS Global_RunDialogHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int64 WorldItemCount(hash protoId)", SCRIPT_FUNC( BIND_CLASS Global_WorldItemCount ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void TextListenerFunc(Critter@+, string)" ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool AddTextListener(int sayType, string firstStr, uint parameter, TextListenerFunc@+ func)", SCRIPT_FUNC( BIND_CLASS Global_AddTextListener ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseTextListener(int sayType, string firstStr, uint parameter)", SCRIPT_FUNC( BIND_CLASS Global_EraseTextListener ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool SwapCritters(Critter@+ cr1, Critter@+ cr2, bool withInventory)", SCRIPT_FUNC( BIND_CLASS Global_SwapCritters ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetAllItems(hash pid)", SCRIPT_FUNC( BIND_CLASS Global_GetAllItems ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetOnlinePlayers()", SCRIPT_FUNC( BIND_CLASS Global_GetOnlinePlayers ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint>@ GetRegisteredPlayerIds()", SCRIPT_FUNC( BIND_CLASS Global_GetRegisteredPlayerIds ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetAllNpc(hash pid)", SCRIPT_FUNC( BIND_CLASS Global_GetAllNpc ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Map@>@ GetAllMaps(hash pid)", SCRIPT_FUNC( BIND_CLASS Global_GetAllMaps ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Location@>@ GetAllLocations(hash pid)", SCRIPT_FUNC( BIND_CLASS Global_GetAllLocations ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadImage(uint index, string imageName, uint imageDepth)", SCRIPT_FUNC( BIND_CLASS Global_LoadImage ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetImageColor(uint index, uint x, uint y)", SCRIPT_FUNC( BIND_CLASS Global_GetImageColor ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetTime(uint16 multiplier, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)", SCRIPT_FUNC( BIND_CLASS Global_SetTime ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void YieldWebRequest(string url, const dict<string, string>@+ post, bool& success, string& result)", SCRIPT_FUNC( BIND_CLASS Global_YieldWebRequest ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void YieldWebRequest(string url, const array<string>@+ headers, string post, bool& success, string& result)", SCRIPT_FUNC( BIND_CLASS Global_YieldWebRequestExt ), SCRIPT_FUNC_CONV ) );
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
    REGISTER_ENTITY( "Critter", CritterCl );
    REGISTER_ENTITY_CAST( "Critter", CritterCl );
    REGISTER_ENTITY( "Item", Item );
    REGISTER_ENTITY_CAST( "Item", Item );
    #endif

    #ifdef BIND_CLIENT
    BIND_ASSERT( engine->RegisterFuncdef( "bool ItemPredicate(const Item@+)" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsChosen() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsChosen ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsPlayer() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsPlayer ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNpc() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsNpc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsOffline() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsOffline ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsLife() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsLife ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnockout() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsKnockout ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsDead() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsDead ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsFree() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsFree ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsBusy() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsBusy ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnim3d() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsAnim3d ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnimAviable(uint anim1, uint anim2) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsAnimAviable ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnimPlaying() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_IsAnimPlaying ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetAnim1() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetAnim1 ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_Animate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2, const Item@+ item)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_AnimateEx ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ClearAnim()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_ClearAnim ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Wait(uint ms)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_Wait ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint CountItem(hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_CountItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(uint itemId)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItem(uint itemId) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItem ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItem(ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemBySlot(uint8 slot)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemBySlot(uint8 slot) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemByPid(hash protoId)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "const Item@+ GetItemByPid(hash protoId) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemByPid ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItems()", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItems() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItemsBySlot(uint8 slot)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItemsBySlot(uint8 slot) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsBySlot ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetItems(ItemPredicate@+ predicate)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetItems(ItemPredicate@+ predicate) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetItemsPredicate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetVisible(bool visible)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_SetVisible ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool GetVisible() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetVisible ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void set_ContourColor(uint value)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_set_ContourColor ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint get_ContourColor() const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_get_ContourColor ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void GetNameTextInfo( bool& nameVisible, int& x, int& y, int& w, int& h, int& lines ) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetNameTextInfo ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void AnimationCallbackFunc(Critter@+ cr)" ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void AddAnimationCallback(uint anim1, uint anim2, float normalizedTime, AnimationCallbackFunc@+ animationCallback) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_AddAnimationCallback ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool GetBonePosition(hash boneName, int& boneX, int& boneY) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetBonePosition ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "string Name", OFFSETOF( CritterCl, Name ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "string NameOnHead", OFFSETOF( CritterCl, NameOnHead ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "string Avatar", OFFSETOF( CritterCl, Avatar ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "uint NameColor", OFFSETOF( CritterCl, NameColor ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "bool IsRunning", OFFSETOF( CritterCl, IsRunning ) ) );

    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@ Clone(uint newCount = 0) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_Clone ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool  GetMapPosition(uint16& hexX, uint16& hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetMapPosition ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void  Animate(uint fromFrame, uint toFrame)", SCRIPT_FUNC_THIS( BIND_CLASS Item_Animate ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<Item@>@  GetItems(uint stackId)", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetItems ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<const Item@>@  GetItems(uint stackId) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetItems ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "string CustomCall(string command, string separator = \" \")", SCRIPT_FUNC( BIND_CLASS Global_CustomCall ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetChosen()", SCRIPT_FUNC( BIND_CLASS Global_GetChosen ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItem(uint itemId)", SCRIPT_FUNC( BIND_CLASS Global_GetItem ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetMapAllItems()", SCRIPT_FUNC( BIND_CLASS Global_GetMapAllItems ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetMapHexItems(uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_GetMapHexItems ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersDistantion(const Critter@+ cr1, const Critter@+ cr2)", SCRIPT_FUNC( BIND_CLASS Global_GetCrittersDistantion ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritter(uint critterId)", SCRIPT_FUNC( BIND_CLASS Global_GetCritter ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType)", SCRIPT_FUNC( BIND_CLASS Global_GetCritters ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCritters(hash pid, int findType)", SCRIPT_FUNC( BIND_CLASS Global_GetCrittersByPids ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType)", SCRIPT_FUNC( BIND_CLASS Global_GetCrittersInPath ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy)", SCRIPT_FUNC( BIND_CLASS Global_GetCrittersInPathBlock ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)", SCRIPT_FUNC( BIND_CLASS Global_GetHexInPath ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint8>@ GetPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", SCRIPT_FUNC( BIND_CLASS Global_GetPathHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint8>@ GetPath(Critter@+ cr, uint16 toHx, uint16 toHy, uint cut)", SCRIPT_FUNC( BIND_CLASS Global_GetPathCr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", SCRIPT_FUNC( BIND_CLASS Global_GetPathLengthHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(Critter@+ cr, uint16 toHx, uint16 toHy, uint cut)", SCRIPT_FUNC( BIND_CLASS Global_GetPathLengthCr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void FlushScreen(uint fromColor, uint toColor, uint timeMs)", SCRIPT_FUNC( BIND_CLASS Global_FlushScreen ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void QuakeScreen(uint noise, uint timeMs)", SCRIPT_FUNC( BIND_CLASS Global_QuakeScreen ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlaySound(string soundName)", SCRIPT_FUNC( BIND_CLASS Global_PlaySound ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlayMusic(string musicName, uint repeatTime = 0)", SCRIPT_FUNC( BIND_CLASS Global_PlayMusic ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void PlayVideo(string videoName, bool canStop)", SCRIPT_FUNC( BIND_CLASS Global_PlayVideo ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string text)", SCRIPT_FUNC( BIND_CLASS Global_Message ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string text, int type)", SCRIPT_FUNC( BIND_CLASS Global_MessageType ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_MessageMsg ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum, int type)", SCRIPT_FUNC( BIND_CLASS Global_MessageMsgType ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MapMessage(string text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)", SCRIPT_FUNC( BIND_CLASS Global_MapMessage ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum, uint skipCount)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrSkip ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumUpper(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrNumUpper ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumLower(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrNumLower ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrCount(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrCount ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMsgStr(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_IsMsgStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, string str)", SCRIPT_FUNC( BIND_CLASS Global_ReplaceTextStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, int i)", SCRIPT_FUNC( BIND_CLASS Global_ReplaceTextInt ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string FormatTags(string text, string lexems)", SCRIPT_FUNC( BIND_CLASS Global_FormatTags ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreenToHex(uint16 hexX, uint16 hexY, uint speed, bool canStop = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveScreenToHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreenOffset(int ox, int oy, uint speed, bool canStop = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveScreenOffset ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void LockScreenScroll(Critter@+ cr, bool softLock, bool unlockIfSame = false)", SCRIPT_FUNC( BIND_CLASS Global_LockScreenScroll ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int GetFog(uint16 zoneX, uint16 zoneY)", SCRIPT_FUNC( BIND_CLASS Global_GetFog ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDayTime(uint dayPart)", SCRIPT_FUNC( BIND_CLASS Global_GetDayTime ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b)", SCRIPT_FUNC( BIND_CLASS Global_GetDayColor ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ShowScreen(int screen, dictionary@+ params = null)", SCRIPT_FUNC( BIND_CLASS Global_ShowScreen ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void HideScreen(int screen = 0)", SCRIPT_FUNC( BIND_CLASS Global_HideScreen ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetHexPos(uint16 hexX, uint16 hexY, int& x, int& y)", SCRIPT_FUNC( BIND_CLASS Global_GetHexPos ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetMonitorHex(int x, int y, uint16& hexX, uint16& hexY)", SCRIPT_FUNC( BIND_CLASS Global_GetMonitorHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetMonitorItem(int x, int y)", SCRIPT_FUNC( BIND_CLASS Global_GetMonitorItem ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetMonitorCritter(int x, int y)", SCRIPT_FUNC( BIND_CLASS Global_GetMonitorCritter ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Entity@+ GetMonitorEntity(int x, int y)", SCRIPT_FUNC( BIND_CLASS Global_GetMonitorEntity ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint16 GetMapWidth()", SCRIPT_FUNC( BIND_CLASS Global_GetMapWidth ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint16 GetMapHeight()", SCRIPT_FUNC( BIND_CLASS Global_GetMapHeight ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMapHexPassed(uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_IsMapHexPassed ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMapHexRaked(uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_IsMapHexRaked ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)", SCRIPT_FUNC( BIND_CLASS Global_MoveHexByDir ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetTileName(uint16 hexX, uint16 hexY, bool roof, int layer)", SCRIPT_FUNC( BIND_CLASS Global_GetTileName ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Preload3dFiles(array<string>@+ fileNames)", SCRIPT_FUNC( BIND_CLASS Global_Preload3dFiles ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void WaitPing()", SCRIPT_FUNC( BIND_CLASS Global_WaitPing ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadFont(int font, string fontFileName)", SCRIPT_FUNC( BIND_CLASS Global_LoadFont ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetDefaultFont(int font, uint color)", SCRIPT_FUNC( BIND_CLASS Global_SetDefaultFont ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetEffect(int effectType, int effectSubtype, string effectName, string effectDefines = \"\")", SCRIPT_FUNC( BIND_CLASS Global_SetEffect ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void RefreshMap(bool onlyTiles, bool onlyRoof, bool onlyLight)", SCRIPT_FUNC( BIND_CLASS Global_RefreshMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MouseClick(int x, int y, int button)", SCRIPT_FUNC( BIND_CLASS Global_MouseClick ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void KeyboardPress(uint8 key1, uint8 key2, string key1Text = \"\", string key2Text = \"\")", SCRIPT_FUNC( BIND_CLASS Global_KeyboardPress ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRainAnimation(string fallAnimName, string dropAnimName)", SCRIPT_FUNC( BIND_CLASS Global_SetRainAnimation ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeZoom(float targetZoom)", SCRIPT_FUNC( BIND_CLASS Global_ChangeZoom ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveScreenshot(string filePath)", SCRIPT_FUNC( BIND_CLASS Global_SaveScreenshot ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveText(string filePath, string text)", SCRIPT_FUNC( BIND_CLASS Global_SaveText ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheData(string name, const array<uint8>@+ data)", SCRIPT_FUNC( BIND_CLASS Global_SetCacheData ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheData(string name, const array<uint8>@+ data, uint dataSize)", SCRIPT_FUNC( BIND_CLASS Global_SetCacheDataSize ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<uint8>@ GetCacheData(string name)", SCRIPT_FUNC( BIND_CLASS Global_GetCacheData ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheDataStr(string name, string data)", SCRIPT_FUNC( BIND_CLASS Global_SetCacheDataStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetCacheDataStr(string name)", SCRIPT_FUNC( BIND_CLASS Global_GetCacheDataStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCacheData(string name)", SCRIPT_FUNC( BIND_CLASS Global_IsCacheData ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseCacheData(string name)", SCRIPT_FUNC( BIND_CLASS Global_EraseCacheData ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetUserConfig(array<string>@+ keyValues)", SCRIPT_FUNC( BIND_CLASS Global_SetUserConfig ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ActivateOffscreenSurface(bool forceClear = false)", SCRIPT_FUNC( BIND_CLASS Global_ActivateOffscreenSurface ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void PresentOffscreenSurface(int effectSubtype)", SCRIPT_FUNC( BIND_CLASS Global_PresentOffscreenSurface ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void PresentOffscreenSurface(int effectSubtype, int x, int y, int w, int h)", SCRIPT_FUNC( BIND_CLASS Global_PresentOffscreenSurfaceExt ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void PresentOffscreenSurface(int effectSubtype, int fromX, int fromY, int fromW, int fromH, int toX, int toY, int toW, int toH)", SCRIPT_FUNC( BIND_CLASS Global_PresentOffscreenSurfaceExt2 ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __IsConnected", &BIND_CLASS_EXT IsConnected ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __IsConnecting", &BIND_CLASS_EXT IsConnecting ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __IsUpdating", &BIND_CLASS_EXT UpdateFilesInProgress ) );
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetFullSecond(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)", SCRIPT_FUNC( BIND_CLASS Global_GetFullSecond ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void GetTime(uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)", SCRIPT_FUNC( BIND_CLASS Global_GetTime ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void GetGameTime(uint fullSecond, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)", SCRIPT_FUNC( BIND_CLASS Global_GetGameTime ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetPropertyGetCallback(int propertyValue, ?&in func)", asFUNCTION( BIND_CLASS Global_SetPropertyGetCallback ), asCALL_GENERIC ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool AddPropertySetCallback(int propertyValue, ?&in func, bool deferred)", asFUNCTION( BIND_CLASS Global_AddPropertySetCallback ), asCALL_GENERIC ) );

    BIND_ASSERT( engine->RegisterGlobalProperty( "const uint __FullSecond", &GameOpt.FullSecond ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableTcpNagle", &GameOpt.DisableTcpNagle ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableZlibCompression", &GameOpt.DisableZlibCompression ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __FloodSize", &GameOpt.FloodSize ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __NoAnswerShuffle", &GameOpt.NoAnswerShuffle ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DialogDemandRecheck", &GameOpt.DialogDemandRecheck ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __SneakDivider", &GameOpt.SneakDivider ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookMinimum", &GameOpt.LookMinimum ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "int __DeadHitPoints", &GameOpt.DeadHitPoints ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Breaktime", &GameOpt.Breaktime ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutTransfer", &GameOpt.TimeoutTransfer ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutBattle", &GameOpt.TimeoutBattle ) );
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
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ForceRebuildResources", &GameOpt.ForceRebuildResources ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "string __CommandLine", &GameOpt.CommandLine ) );
    #endif

    #ifdef BIND_MAPPER
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ AddChild(hash pid)", SCRIPT_FUNC_THIS( BIND_CLASS Item_AddChild ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ AddChild(hash pid) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_AddChild ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<Item@>@ GetChildren(uint16 hexX, uint16 hexY)", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetChildren ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Item", "array<const Item@>@ GetChildren(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Item_GetChildren ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<Item@>@ GetChildren(uint16 hexX, uint16 hexY)", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetChildren ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "array<const Item@>@ GetChildren(uint16 hexX, uint16 hexY) const", SCRIPT_FUNC_THIS( BIND_CLASS Crit_GetChildren ), SCRIPT_FUNC_THIS_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ AddItem(hash protoId, uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_AddItem ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ AddCritter(hash protoId, uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_AddCritter ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItemByHex(uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_GetItemByHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Item@>@ GetItemsByHex(uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_GetItemsByHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritterByHex(uint16 hexX, uint16 hexY, int findType)", SCRIPT_FUNC( BIND_CLASS Global_GetCritterByHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Critter@>@ GetCrittersByHex(uint16 hexX, uint16 hexY, int findType)", SCRIPT_FUNC( BIND_CLASS Global_GetCrittersByHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveEntity(Entity@+ entity, uint16 hexX, uint16 hexY)", SCRIPT_FUNC( BIND_CLASS Global_MoveEntity ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteEntity(Entity@+ entity)", SCRIPT_FUNC( BIND_CLASS Global_DeleteEntity ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteEntities(array<Entity@>@+ entities)", SCRIPT_FUNC( BIND_CLASS Global_DeleteEntities ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SelectEntity(Entity@+ entity, bool set)", SCRIPT_FUNC( BIND_CLASS Global_SelectEntity ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SelectEntities(array<Entity@>@+ entities, bool set)", SCRIPT_FUNC( BIND_CLASS Global_SelectEntities ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "Entity@+ GetSelectedEntity()", SCRIPT_FUNC( BIND_CLASS Global_GetSelectedEntity ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Entity@>@ GetSelectedEntities()", SCRIPT_FUNC( BIND_CLASS Global_GetSelectedEntities ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTilesCount(uint16 hexX, uint16 hexY, bool roof)", SCRIPT_FUNC( BIND_CLASS Global_GetTilesCount ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteTile(uint16 hexX, uint16 hexY, bool roof, int layer)", SCRIPT_FUNC( BIND_CLASS Global_DeleteTile ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetTile(uint16 hexX, uint16 hexY, bool roof, int layer)", SCRIPT_FUNC( BIND_CLASS Global_GetTileHash ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void AddTile(uint16 hexX, uint16 hexY, int offsX, int offsY, int layer, bool roof, hash picHash)", SCRIPT_FUNC( BIND_CLASS Global_AddTileHash ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetTileName(uint16 hexX, uint16 hexY, bool roof, int layer)", SCRIPT_FUNC( BIND_CLASS Global_GetTileName ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void AddTileName(uint16 hexX, uint16 hexY, int offsX, int offsY, int layer, bool roof, string picName)", SCRIPT_FUNC( BIND_CLASS Global_AddTileName ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ LoadMap(string fileName)", SCRIPT_FUNC( BIND_CLASS Global_LoadMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void UnloadMap(Map@+ map)", SCRIPT_FUNC( BIND_CLASS Global_UnloadMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveMap(Map@+ map, string customName = \"\")", SCRIPT_FUNC( BIND_CLASS Global_SaveMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool ShowMap(Map@+ map)", SCRIPT_FUNC( BIND_CLASS Global_ShowMap ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<Map@>@ GetLoadedMaps(int& index)", SCRIPT_FUNC( BIND_CLASS Global_GetLoadedMaps ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<string>@ GetMapFileNames(string dir)", SCRIPT_FUNC( BIND_CLASS Global_GetMapFileNames ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ResizeMap(uint16 width, uint16 height)", SCRIPT_FUNC( BIND_CLASS Global_ResizeMap ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetTileDirs(int tab, array<string>@+ dirNames, array<bool>@+ includeSubdirs)", SCRIPT_FUNC( BIND_CLASS Global_TabGetTileDirs ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetItemPids(int tab, string subTab, array<hash>@+ itemPids)", SCRIPT_FUNC( BIND_CLASS Global_TabGetItemPids ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetCritterPids(int tab, string subTab, array<hash>@+ critterPids)", SCRIPT_FUNC( BIND_CLASS Global_TabGetCritterPids ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetTileDirs(int tab, array<string>@+ dirNames, array<bool>@+ includeSubdirs)", SCRIPT_FUNC( BIND_CLASS Global_TabSetTileDirs ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetItemPids(int tab, string subTab, array<hash>@+ itemPids)", SCRIPT_FUNC( BIND_CLASS Global_TabSetItemPids ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetCritterPids(int tab, string subTab, array<hash>@+ critterPids)", SCRIPT_FUNC( BIND_CLASS Global_TabSetCritterPids ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void TabDelete(int tab)", SCRIPT_FUNC( BIND_CLASS Global_TabDelete ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSelect(int tab, string subTab, bool show = false)", SCRIPT_FUNC( BIND_CLASS Global_TabSelect ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetName(int tab, string tabName)", SCRIPT_FUNC( BIND_CLASS Global_TabSetName ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)", SCRIPT_FUNC( BIND_CLASS Global_GetHexInPath ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", SCRIPT_FUNC( BIND_CLASS Global_GetPathLengthHex ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string text)", SCRIPT_FUNC( BIND_CLASS Global_Message ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_MessageMsg ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "void MapMessage(string text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)", SCRIPT_FUNC( BIND_CLASS Global_MapMessage ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetMsgStr(int textMsg, uint strNum, uint skipCount)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrSkip ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumUpper(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrNumUpper ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumLower(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrNumLower ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrCount(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_GetMsgStrCount ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMsgStr(int textMsg, uint strNum)", SCRIPT_FUNC( BIND_CLASS Global_IsMsgStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, string str)", SCRIPT_FUNC( BIND_CLASS Global_ReplaceTextStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string ReplaceText(string text, string replace, int i)", SCRIPT_FUNC( BIND_CLASS Global_ReplaceTextInt ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreenToHex(uint16 hexX, uint16 hexY, uint speed, bool canStop = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveScreenToHex ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreenOffset(int ox, int oy, uint speed, bool canStop = false)", SCRIPT_FUNC( BIND_CLASS Global_MoveScreenOffset ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y)", SCRIPT_FUNC( BIND_CLASS Global_GetHexPos ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy, bool ignoreInterface = false)", SCRIPT_FUNC( BIND_CLASS Global_GetMonitorHex ), SCRIPT_FUNC_CONV ) );
    // BIND_ASSERT( engine->RegisterGlobalFunction( "MapperObject@ GetMonitorObject(int x, int y, bool ignoreInterface = false)", SCRIPT_FUNC( BIND_CLASS Global_GetMonitorObject ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)", SCRIPT_FUNC( BIND_CLASS Global_MoveHexByDir ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetIfaceIniStr(string key)", SCRIPT_FUNC( BIND_CLASS Global_GetIfaceIniStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadFont(int font, string fontFileName)", SCRIPT_FUNC( BIND_CLASS Global_LoadFont ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetDefaultFont(int font, uint color)", SCRIPT_FUNC( BIND_CLASS Global_SetDefaultFont ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void MouseClick(int x, int y, int button)", SCRIPT_FUNC( BIND_CLASS Global_MouseClick ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void KeyboardPress(uint8 key1, uint8 key2, string key1Text = \"\", string key2Text = \"\")", SCRIPT_FUNC( BIND_CLASS Global_KeyboardPress ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRainAnimation(string fallAnimName, string dropAnimName)", SCRIPT_FUNC( BIND_CLASS Global_SetRainAnimation ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeZoom(float targetZoom)", SCRIPT_FUNC( BIND_CLASS Global_ChangeZoom ), SCRIPT_FUNC_CONV ) );

    BIND_ASSERT( engine->RegisterGlobalProperty( "string __ServerDir", &GameOpt.ServerDir ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowCorners", &GameOpt.ShowCorners ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowSpriteCuts", &GameOpt.ShowSpriteCuts ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowDrawOrder", &GameOpt.ShowDrawOrder ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __SplitTilesCollection", &GameOpt.SplitTilesCollection ) );
    #endif

    #if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
    BIND_ASSERT( engine->RegisterObjectType( "MapSprite", sizeof( MapSprite ), asOBJ_REF ) );
    BIND_ASSERT( engine->RegisterObjectBehaviour( "MapSprite", asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD( MapSprite, AddRef ), SCRIPT_METHOD_CONV ) );
    BIND_ASSERT( engine->RegisterObjectBehaviour( "MapSprite", asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD( MapSprite, Release ), SCRIPT_METHOD_CONV ) );
    BIND_ASSERT( engine->RegisterObjectBehaviour( "MapSprite", asBEHAVE_FACTORY, "MapSprite@ f()", SCRIPT_FUNC( MapSprite::Factory ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "bool Valid", OFFSETOF( MapSprite, Valid ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "uint SprId", OFFSETOF( MapSprite, SprId ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "uint16 HexX", OFFSETOF( MapSprite, HexX ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "uint16 HexY", OFFSETOF( MapSprite, HexY ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "hash ProtoId", OFFSETOF( MapSprite, ProtoId ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int FrameIndex", OFFSETOF( MapSprite, FrameIndex ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int OffsX", OFFSETOF( MapSprite, OffsX ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int OffsY", OFFSETOF( MapSprite, OffsY ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "bool IsFlat", OFFSETOF( MapSprite, IsFlat ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "bool NoLight", OFFSETOF( MapSprite, NoLight ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int DrawOrder", OFFSETOF( MapSprite, DrawOrder ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int DrawOrderHyOffset", OFFSETOF( MapSprite, DrawOrderHyOffset ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int Corner", OFFSETOF( MapSprite, Corner ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "bool DisableEgg", OFFSETOF( MapSprite, DisableEgg ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "uint Color", OFFSETOF( MapSprite, Color ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "uint ContourColor", OFFSETOF( MapSprite, ContourColor ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "bool IsTweakOffs", OFFSETOF( MapSprite, IsTweakOffs ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int16 TweakOffsX", OFFSETOF( MapSprite, TweakOffsX ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "int16 TweakOffsY", OFFSETOF( MapSprite, TweakOffsY ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "bool IsTweakAlpha", OFFSETOF( MapSprite, IsTweakAlpha ) ) );
    BIND_ASSERT( engine->RegisterObjectProperty( "MapSprite", "uint8 TweakAlpha", OFFSETOF( MapSprite, TweakAlpha ) ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "uint LoadSprite(string name)", SCRIPT_FUNC( BIND_CLASS Global_LoadSprite ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint LoadSprite(hash name)", SCRIPT_FUNC( BIND_CLASS Global_LoadSpriteHash ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSpriteWidth(uint sprId, int frameIndex)", SCRIPT_FUNC( BIND_CLASS Global_GetSpriteWidth ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSpriteHeight(uint sprId, int frameIndex)", SCRIPT_FUNC( BIND_CLASS Global_GetSpriteHeight ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSpriteCount(uint sprId)", SCRIPT_FUNC( BIND_CLASS Global_GetSpriteCount ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSpriteTicks(uint sprId)", SCRIPT_FUNC( BIND_CLASS Global_GetSpriteTicks ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPixelColor(uint sprId, int frameIndex, int x, int y)", SCRIPT_FUNC( BIND_CLASS Global_GetPixelColor ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void GetTextInfo(string text, int w, int h, int font, int flags, int& tw, int& th, int& lines)", SCRIPT_FUNC( BIND_CLASS Global_GetTextInfo ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSprite(uint sprId, int frameIndex, int x, int y, uint color = 0, bool applyOffsets = false)", SCRIPT_FUNC( BIND_CLASS Global_DrawSprite ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSprite(uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom = false, uint color = 0, bool applyOffsets = false)", SCRIPT_FUNC( BIND_CLASS Global_DrawSpriteSize ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSpritePattern(uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color = 0)", SCRIPT_FUNC( BIND_CLASS Global_DrawSpritePattern ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawText(string text, int x, int y, int w, int h, uint color, int font, int flags)", SCRIPT_FUNC( BIND_CLASS Global_DrawText ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawPrimitive(int primitiveType, array<int>@+ data)", SCRIPT_FUNC( BIND_CLASS Global_DrawPrimitive ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawMapSprite(MapSprite@+ mapSprite)", SCRIPT_FUNC( BIND_CLASS Global_DrawMapSprite ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawCritter2d(hash modelName, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)", SCRIPT_FUNC( BIND_CLASS Global_DrawCritter2d ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawCritter3d(uint instance, hash modelName, uint anim1, uint anim2, const array<int>@+ layers, const array<float>@+ position, uint color)", SCRIPT_FUNC( BIND_CLASS Global_DrawCritter3d ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void PushDrawScissor(int x, int y, int w, int h)", SCRIPT_FUNC( BIND_CLASS Global_PushDrawScissor ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void PopDrawScissor()", SCRIPT_FUNC( BIND_CLASS Global_PopDrawScissor ), SCRIPT_FUNC_CONV ) );

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
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __FullScreen", &GameOpt.FullScreen ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __VSync", &GameOpt.VSync ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "int __Light", &GameOpt.Light ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ScrollDelay", &GameOpt.ScrollDelay ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "int __ScrollStep", &GameOpt.ScrollStep ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MouseScroll", &GameOpt.MouseScroll ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ScrollCheck", &GameOpt.ScrollCheck ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "string __Host", &GameOpt.Host ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Port", &GameOpt.Port ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ProxyType", &GameOpt.ProxyType ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "string __ProxyHost", &GameOpt.ProxyHost ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ProxyPort", &GameOpt.ProxyPort ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "string __ProxyUser", &GameOpt.ProxyUser ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "string __ProxyPass", &GameOpt.ProxyPass ) );
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
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __CritterFidgetTime", &GameOpt.CritterFidgetTime ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Anim2CombatBegin", &GameOpt.Anim2CombatBegin ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Anim2CombatIdle", &GameOpt.Anim2CombatIdle ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Anim2CombatEnd", &GameOpt.Anim2CombatEnd ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ConsoleHistorySize", &GameOpt.ConsoleHistorySize ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "int __SoundVolume", &GameOpt.SoundVolume ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "int __MusicVolume", &GameOpt.MusicVolume ) );
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
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __DesktopBuild", &GameOpt.DesktopBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __TabletBuild", &GameOpt.TabletBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __WebBuild", &GameOpt.WebBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __WindowsBuild", &GameOpt.WindowsBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __LinuxBuild", &GameOpt.LinuxBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __MacOsBuild", &GameOpt.MacOsBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __AndroidBuild", &GameOpt.AndroidBuild ) );
    BIND_ASSERT( engine->RegisterGlobalProperty( "const bool __IOsBuild", &GameOpt.IOsBuild ) );

    BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadDataFile(string dataFileName)", SCRIPT_FUNC( BIND_CLASS Global_LoadDataFile ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void AllowSlot(uint8 index, bool enableSend)", SCRIPT_FUNC( BIND_CLASS Global_AllowSlot ), SCRIPT_FUNC_CONV ) );

    // Invoker
    BIND_ASSERT( engine->RegisterFuncdef( "void CallFunc()" ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithIValue(int value)" ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithUValue(uint value)" ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithIValues(array<int>@+ values)" ) );
    BIND_ASSERT( engine->RegisterFuncdef( "void CallFuncWithUValues(array<uint>@+ values)" ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFunc@+ func)", SCRIPT_FUNC( ScriptInvoker::Global_DeferredCall ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithIValue@+ func, int value)", SCRIPT_FUNC( ScriptInvoker::Global_DeferredCallWithValue ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithUValue@+ func, uint value)", SCRIPT_FUNC( ScriptInvoker::Global_DeferredCallWithValue ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithIValues@+ func, const array<int>@+ values)", SCRIPT_FUNC( ScriptInvoker::Global_DeferredCallWithValues ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint DeferredCall(uint delay, CallFuncWithUValues@+ func, const array<uint>@+ values)", SCRIPT_FUNC( ScriptInvoker::Global_DeferredCallWithValues ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsDeferredCallPending(uint id)", SCRIPT_FUNC( ScriptInvoker::Global_IsDeferredCallPending ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool CancelDeferredCall(uint id)", SCRIPT_FUNC( ScriptInvoker::Global_CancelDeferredCall ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetDeferredCallData(uint id, uint& delay, array<int>@+ values)", SCRIPT_FUNC( ScriptInvoker::Global_GetDeferredCallData ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetDeferredCallData(uint id, uint& delay, array<uint>@+ values)", SCRIPT_FUNC( ScriptInvoker::Global_GetDeferredCallData ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDeferredCallsList(array<uint>@+ ids)", SCRIPT_FUNC( ScriptInvoker::Global_GetDeferredCallsList ), SCRIPT_FUNC_CONV ) );
    #ifdef BIND_SERVER
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFunc@+ func)", SCRIPT_FUNC( ScriptInvoker::Global_SavedDeferredCall ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithIValue@+ func, int value)", SCRIPT_FUNC( ScriptInvoker::Global_SavedDeferredCallWithValue ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithUValue@+ func, uint value)", SCRIPT_FUNC( ScriptInvoker::Global_SavedDeferredCallWithValue ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithIValues@+ func, const array<int>@+ values)", SCRIPT_FUNC( ScriptInvoker::Global_SavedDeferredCallWithValues ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint SavedDeferredCall(uint delay, CallFuncWithUValues@+ func, const array<uint>@+ values)", SCRIPT_FUNC( ScriptInvoker::Global_SavedDeferredCallWithValues ), SCRIPT_FUNC_CONV ) );
    #endif

    #define BIND_ASSERT_EXT( expr )    BIND_ASSERT( ( expr ) ? 0 : -1 )
    BIND_ASSERT_EXT( registrators[ 0 ]->Init() );
    BIND_ASSERT_EXT( registrators[ 1 ]->Init() );
    BIND_ASSERT_EXT( registrators[ 2 ]->Init() );
    BIND_ASSERT_EXT( registrators[ 3 ]->Init() );
    BIND_ASSERT_EXT( registrators[ 4 ]->Init() );

    #if defined ( BIND_SERVER )
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ AddNpc(hash protoId, uint16 hexX, uint16 hexY, uint8 dir, dict<CritterProperty, int>@+ props = null)", SCRIPT_FUNC_THIS( BIND_CLASS Map_AddNpc ), SCRIPT_FUNC_THIS_CONV ) );
    BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ AddItem(uint16 hexX, uint16 hexY, hash protoId, uint count, dict<ItemProperty, int>@+ props = null)", SCRIPT_FUNC_THIS( BIND_CLASS Map_AddItem ), SCRIPT_FUNC_THIS_CONV ) );
    #endif

    // ScriptFunctions.h
    #ifdef FONLINE_SCRIPT_COMPILER
    # undef SCRIPT_FUNC
    # undef SCRIPT_FUNC_CONV
    # define SCRIPT_FUNC         asFUNCTION
    # define SCRIPT_FUNC_CONV    asCALL_CDECL
    #endif
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Assert(bool condition, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_Assert ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message)", SCRIPT_FUNC( Global_ThrowException ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in)", SCRIPT_FUNC( Global_ThrowException ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in)", SCRIPT_FUNC( Global_ThrowException ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_ThrowException ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_ThrowException ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void ThrowException(string message, const ?&in, const ?&in, const ?&in, const ?&in, const ?&in)", SCRIPT_FUNC( Global_ThrowException ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int Random(int min, int max)", SCRIPT_FUNC( Global_Random ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Log(string text)", SCRIPT_FUNC( Global_Log ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool StrToInt(string text, int& result)", SCRIPT_FUNC( Global_StrToInt ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool StrToFloat(string text, float& result)", SCRIPT_FUNC( Global_StrToFloat ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDistantion(uint16 hexX1, uint16 hexY1, uint16 hexX2, uint16 hexY2)", SCRIPT_FUNC( Global_GetDistantion ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 GetDirection(uint16 fromHexX, uint16 fromHexY, uint16 toHexX, uint16 toHexY)", SCRIPT_FUNC( Global_GetDirection ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 GetOffsetDir(uint16 fromHexX, uint16 fromHexY, uint16 toHexX, uint16 toHexY, float offset)", SCRIPT_FUNC( Global_GetOffsetDir ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTick()", SCRIPT_FUNC( Global_GetTick ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAngelScriptProperty(int property)", SCRIPT_FUNC( Global_GetAngelScriptProperty ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void SetAngelScriptProperty(int property, uint value)", SCRIPT_FUNC( Global_SetAngelScriptProperty ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetStrHash(string str)", SCRIPT_FUNC( Global_GetStrHash ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string GetHashStr(hash h)", SCRIPT_FUNC( Global_GetHashStr ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint DecodeUTF8(string text, uint& length)", SCRIPT_FUNC( Global_DecodeUTF8 ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string EncodeUTF8(uint ucs)", SCRIPT_FUNC( Global_EncodeUTF8 ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "array<string>@ GetFolderFileNames(string path, string extension, bool includeSubdirs)", SCRIPT_FUNC( Global_GetFolderFileNames ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "bool DeleteFile(string fileName)", SCRIPT_FUNC( Global_DeleteFile ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void CreateDirectoryTree(string path)", SCRIPT_FUNC( Global_CreateDirectoryTree ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void Yield(uint time)", SCRIPT_FUNC( Global_Yield ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string SHA1(string text)", SCRIPT_FUNC( Global_SHA1 ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "string SHA2(string text)", SCRIPT_FUNC( Global_SHA2 ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int SystemCall(string command)", SCRIPT_FUNC( Global_SystemCall ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "int SystemCall(string command, string& output)", SCRIPT_FUNC( Global_SystemCallExt ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "void OpenLink(string link)", SCRIPT_FUNC( Global_OpenLink ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "const Item@ GetProtoItem(hash protoId, dict<ItemProperty, int>@+ props = null)", SCRIPT_FUNC( Global_GetProtoItem ), SCRIPT_FUNC_CONV ) );
    BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetUnixTime()", SCRIPT_FUNC( Global_GetUnixTime ), SCRIPT_FUNC_CONV ) );

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    return errors;
}

#ifdef BIND_DUMMY_DATA
# pragma pop_macro( "SCRIPT_FUNC" )
# pragma pop_macro( "SCRIPT_FUNC_THIS" )
# pragma pop_macro( "SCRIPT_METHOD" )
# pragma pop_macro( "SCRIPT_FUNC_CONV" )
# pragma pop_macro( "SCRIPT_FUNC_THIS_CONV" )
# pragma pop_macro( "SCRIPT_METHOD_CONV" )
#endif
